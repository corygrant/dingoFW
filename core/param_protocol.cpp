#include "param_protocol.h"
#include "device_config.h"
#include "config.h"
#include "mailbox.h"
#include "config_handler.h"
#include "request_msg.h"
#include "crc.h"
#include <cstring>

extern DeviceConfig stConfig;

uint16_t nNumWriteParams = 0;
uint16_t nNumReadParams = 0;

uint32_t nReadCrc = 0xFFFFFFFF;
uint32_t nWriteCrc = 0xFFFFFFFF;
uint32_t nCheckCrc = 0xFFFFFFFF;

void DecodeParamCmd(CANRxFrame *rx, ParamMsg *out)
{
    out->eCmd = static_cast<MsgCmd>(rx->data8[0]);
    out->nIndex = rx->data8[1] | (rx->data8[2] << 8);
    out->nSubIndex = rx->data8[3];
    out->nValue =  rx->data8[4] |
                   rx->data8[5] << 8 |
                   rx->data8[6] << 16 |
                  (rx->data8[7]) << 24;
}

void EncodeParamRsp(CANTxFrame *tx, uint8_t cmd, uint16_t index, uint8_t subindex, uint32_t value)
{
    tx->IDE = 0;
    tx->RTR = 0;
    tx->DLC = 8;

    tx->SID =  stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;

    tx->data8[0] = cmd;
    tx->data8[1] = index & 0xFF;
    tx->data8[2] = (index >> 8) & 0xFF;
    tx->data8[3] = subindex;
    tx->data8[4] = value & 0xFF;
    tx->data8[5] = (value >> 8) & 0xFF;
    tx->data8[6] = (value >> 16) & 0xFF;
    tx->data8[7] = (value >> 24) & 0xFF;
}

#define TX_MAX_RETRIES 50   // 50 × 200µs = 10ms max stall per frame before aborting

// Retry a TX post when mailbox is full
msg_t PostTxFrameWithRetry(CANTxFrame *tx) {
    msg_t ret;
    uint8_t txRetries = 0;
    do {
        ret = PostTxFrame(tx);
        if (ret != MSG_OK) {
            chThdSleepMicroseconds(200);
            txRetries++;
        }
    } while (ret != MSG_OK && txRetries < TX_MAX_RETRIES);
    return ret;
}

// Set while a ReadAll/WriteAll transfer is in progress so cyclic TX can pause.
volatile bool g_bParamOpInProgress = false;
static volatile uint32_t nParamOpStartTime = 0;
#define PARAM_OP_MAX_DURATION_MS 4000 // never pause cyclic TX longer than this

static void SetParamOpInProgress(bool bInProgress) {
    g_bParamOpInProgress = bInProgress;
    if (bInProgress)
        nParamOpStartTime = SYS_TIME;
}

bool IsParamOpInProgress() {
    if (!g_bParamOpInProgress)
        return false;

    if (SYS_TIME - nParamOpStartTime > PARAM_OP_MAX_DURATION_MS) {
        g_bParamOpInProgress = false; // stale
        return false;
    }

    return true;
}

void SendAllParams(bool modifiedOnly) {
    CANTxFrame tx;
    uint8_t nBatchCount = 0;

    for (int i = 0; i < NUM_PARAMS; i++) {
        if (modifiedOnly && IsDefaultValue(&stParams[i])) {
            continue; // Skip default params if only sending modified
        }

        if(nBatchCount > 50) { // Send in batches of 50 to avoid overflowing CAN buffers
            chThdSleepMilliseconds(10); // Short delay between batches
            nBatchCount = 0;
        }
        nBatchCount++;

        uint32_t value = ReadParam(&stParams[i]);
        EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::ReadAllRsp),
                        stParams[i].nIndex, stParams[i].nSubIndex, value);

        if (PostTxFrameWithRetry(&tx) != MSG_OK)
            break; // TX stalled — abort; ReadAllComplete sent below with wrong CRC so host retries

        nReadCrc = CalculateCRC32Partial(&tx.data8[4], 4, nReadCrc);

        nNumReadParams++;
    }

    chThdSleepMilliseconds(1);
    nReadCrc = ~nReadCrc; // Finalize CRC after all params sent
    EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::ReadAllComplete), nNumReadParams, 0, nReadCrc); // End of params marker, return number of params sent and CRC
    PostTxFrameWithRetry(&tx);
}

void CheckCrc() {
    CANTxFrame tx;

    nCheckCrc = CalcParamsCrc(false); // Canonical table-order CRC over live values

    EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::CheckCrcRsp), 0, 0, nCheckCrc); // End of params marker, return number of params sent and CRC
    PostTxFrameWithRetry(&tx);

}

void SetAllDefaultParams(bool temp) {
    for (int i = 0; i < NUM_PARAMS; i++) {
        WriteParam(&stParams[i], stParams[i].nDefaultVal, temp);
    }
}

void ApplyTempParams() {
    for (int i = 0; i < NUM_PARAMS; i++) {
        uint32_t tempVal = ReadParam(&stParams[i], true);
        WriteParam(&stParams[i], tempVal, false);
    }
}

static void ApplyLiveConfig() {
    LockConfig();
    ApplyTempParams();
    ApplyAllConfig();
    UnlockConfig();
}

static bool bLastWriteWasModified = false;

#define MAX_WRITE_ALL_MISSING_REPORTED 16

static void SendWriteAllMissingList(bool bForceOverflow) {
    CANTxFrame tx;
    bool bOverflow = bForceOverflow;

    if (!bOverflow) {
        uint16_t nMissing = 0;
        for (uint16_t i = 0; i < NUM_PARAMS && nMissing <= MAX_WRITE_ALL_MISSING_REPORTED; i++) {
            if (!IsParamReceived(i)) nMissing++;
        }
        bOverflow = (nMissing > MAX_WRITE_ALL_MISSING_REPORTED);
    }

    uint16_t nSent = 0;
    if (!bOverflow) {
        for (uint16_t i = 0; i < NUM_PARAMS && nSent < MAX_WRITE_ALL_MISSING_REPORTED; i++) {
            if (!IsParamReceived(i)) {
                EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::WriteAllMissing),
                                stParams[i].nIndex, stParams[i].nSubIndex, 0);
                PostTxFrameWithRetry(&tx);
                nSent++;
            }
        }
    }

    uint16_t nDoneCount = bOverflow ? 0xFFFF : nSent;
    EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::WriteAllMissingDone), nDoneCount, 0, 0);
    PostTxFrameWithRetry(&tx);
}

static void ProcessParamMsg(CANRxFrame *rx) {
    CANTxFrame tx;
    ParamMsg msg;

    if (rx->SID != stConfig.stDevice.nBaseId + CONFIG_RX_OFFSET)
        return;

    if (rx->DLC != 8)
        return;

    DecodeParamCmd(rx, &msg);

    switch(msg.eCmd) {
        case MsgCmd::Read: {
            const ParamInfo* param = FindParam(msg.nIndex, msg.nSubIndex);
            if (param) {
                uint32_t value = ReadParam(param);
                EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::Read), msg.nIndex, msg.nSubIndex, value);
                PostTxFrameWithRetry(&tx);
                break;
            }
            EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::ReadParamNotFound), msg.nIndex, msg.nSubIndex, 0);
            PostTxFrameWithRetry(&tx);
            break;
        }

        case MsgCmd::Write: {
            const ParamInfo* param = FindParam(msg.nIndex, msg.nSubIndex);
            if (!param)
                break;

            LockConfig();
            bool bWritten = WriteParam(param, msg.nValue);
            if (bWritten)
                ApplyConfig(msg.nIndex);
            UnlockConfig();

            if (bWritten) {
                uint32_t value = ReadParam(param);
                EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::Write), msg.nIndex, msg.nSubIndex, value);
                PostTxFrameWithRetry(&tx);
            }
            break;
        }

        case MsgCmd::ReadAll:
        case MsgCmd::ReadAllModified:
            SetParamOpInProgress(true);
            nNumReadParams = 0;
            nReadCrc = 0xFFFFFFFF; // Reset CRC for new batch
            EncodeParamRsp(&tx, static_cast<uint8_t>(msg.eCmd), 0, 0, 0); // Start of params marker
            PostTxFrameWithRetry(&tx);
            chThdSleepMilliseconds(1);
            SendAllParams(msg.eCmd == MsgCmd::ReadAllModified);
            SetParamOpInProgress(false); // ReadAllComplete already sent by SendAllParams
            break;

        case MsgCmd::WriteAll:
        case MsgCmd::WriteAllModified:
            SetParamOpInProgress(true); // cleared on WriteAllComplete below
            bLastWriteWasModified = (msg.eCmd == MsgCmd::WriteAllModified);
            nNumWriteParams = 0;
            nWriteCrc = 0xFFFFFFFF; // Reset CRC for new batch
            ResetWriteReceivedMask();
            SetAllDefaultParams(true); // Clear temp values
            EncodeParamRsp(&tx, static_cast<uint8_t>(msg.eCmd), 0, 0, 0); // Start of params marker
            PostTxFrameWithRetry(&tx);
            break;

        case MsgCmd::WriteAllVal: {
            const ParamInfo* param = FindParam(msg.nIndex, msg.nSubIndex);
            //Param not found or invalid value, respond with error
            if (!param){
                EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::WriteAllParamNotFound), msg.nIndex, msg.nSubIndex, 0);
                PostTxFrameWithRetry(&tx);
                break;
            }
            //Param out of range, respond with error
            if (!WriteParam(param, msg.nValue, true)) {
                EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::WriteAllOutOfRange), msg.nIndex, msg.nSubIndex, msg.nValue);
                PostTxFrameWithRetry(&tx);
                break;
            }
            nWriteCrc = CalculateCRC32Partial(&rx->data8[4], 4, nWriteCrc);
            nNumWriteParams++;
            MarkParamReceived(param);
            break;
        }

        case MsgCmd::WriteAllComplete: {
            uint16_t nExpectedParams = rx->data8[1] | (rx->data8[2] << 8);
            uint32_t nExpectedCrc = msg.nValue; // host's CRC of what it sent, bytes 4-7

            uint16_t nReportedCount;
            uint32_t nReportedCrc;
            uint8_t bApplied;
            bool bNeedsMissingList = false;
            bool bCrcMismatchOnFullCount = false;

            if (bLastWriteWasModified) {
                // Only apply if every param was received
                nWriteCrc = ~nWriteCrc; // Finalize CRC of what we actually received
                bApplied = (nNumWriteParams == nExpectedParams && nWriteCrc == nExpectedCrc) ? 1 : 0;
                if (bApplied) {
                    ApplyLiveConfig();
                }
                nReportedCount = nNumWriteParams;
                nReportedCrc = nWriteCrc;
            } else {
                // Full WriteAll, count params instead of relying on receive order
                uint16_t nReceived = CountReceivedParams();
                uint32_t nCrc = (nReceived == nExpectedParams) ? CalcParamsCrc(true) : 0;
                bApplied = (nReceived == nExpectedParams && nCrc == nExpectedCrc) ? 1 : 0;
                if (bApplied) {
                    ApplyLiveConfig();
                } else {
                    bNeedsMissingList = true;
                    bCrcMismatchOnFullCount = (nReceived == nExpectedParams); // nothing to enumerate
                }
                nReportedCount = nReceived;
                nReportedCrc = nCrc;
            }

            EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::WriteAllComplete), nReportedCount, bApplied, nReportedCrc);
            PostTxFrameWithRetry(&tx);

            if (bNeedsMissingList) {
                SendWriteAllMissingList(bCrcMismatchOnFullCount);
            }

            SetParamOpInProgress(false);
            break;
        }

        case MsgCmd::CheckCrc:
            CheckCrc();
            break;

        default:
            break;
    }
}

static CANRxFrame paramFrames[MAILBOX_SIZE];
static msg_t paramMsgs[MAILBOX_SIZE];
static objects_fifo_t paramFifo;

bool RouteParamFrame(CANRxFrame *frame)
{
    if ((frame->IDE != CAN_IDE_STD) ||
        (frame->SID != stConfig.stDevice.nBaseId + CONFIG_RX_OFFSET))
        return false;

    CANRxFrame *pFrame = static_cast<CANRxFrame*>(chFifoTakeObjectTimeout(&paramFifo, TIME_IMMEDIATE));
    if (pFrame == nullptr)
        return true; // Queue full, drop - host retries on CRC/count mismatch

    *pFrame = *frame;
    chFifoSendObject(&paramFifo, pFrame);
    return true;
}

static THD_WORKING_AREA(waParamThread, DEVICE_THREAD_STACK);
static void ParamThread(void *)
{
    chRegSetThreadName("Param");

    while (true)
    {
        CANRxFrame *pFrame;
        if (chFifoReceiveObjectTimeout(&paramFifo, reinterpret_cast<void**>(&pFrame), TIME_INFINITE) != MSG_OK)
            continue;

        CANRxFrame frame = *pFrame;
        chFifoReturnObject(&paramFifo, pFrame);

        CheckRequestMsgs(&frame);
        ProcessParamMsg(&frame);
    }
}

void InitParamThread()
{
    chFifoObjectInit(&paramFifo, sizeof(CANRxFrame), MAILBOX_SIZE, paramFrames, paramMsgs);

    // Priority below DeviceThread
    chThdCreateStatic(waParamThread, sizeof(waParamThread), NORMALPRIO - 1, ParamThread, nullptr);
}