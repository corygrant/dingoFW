#include "param_protocol.h"
#include "device_config.h"
#include "config.h"
#include "mailbox.h"
#include "config_handler.h"
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

// Retry a TX post against a momentarily-full mailbox instead of dropping it.
// Used for every param-protocol response so a busy bus doesn't silently
// swallow a reply and force the host to wait out its full timeout.
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

// Set while a ReadAll/WriteAll bulk transfer is in progress so cyclic TX can pause.
volatile bool g_bParamOpInProgress = false;
static volatile uint32_t nParamOpStartTime = 0;
#define PARAM_OP_MAX_DURATION_MS 4000 // safety valve: never pause cyclic TX longer than this

static void SetParamOpInProgress(bool bInProgress) {
    g_bParamOpInProgress = bInProgress;
    if (bInProgress)
        nParamOpStartTime = SYS_TIME;
}

bool IsCyclicTxPaused() {
    if (!g_bParamOpInProgress)
        return false;

    //if (SYS_TIME - nParamOpStartTime > PARAM_OP_MAX_DURATION_MS) {
    //    g_bParamOpInProgress = false; // stale op (e.g. host abandoned a WriteAll mid-stream)
    //    return false;
    //}

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

// True if the WriteAll currently in flight was WriteAllModified rather than a full WriteAll.
// Only full WriteAll supports the missing-param report/patch flow below: WriteAllModified
// sends a host-chosen subset, so an unset bit in writeReceivedMask can't be told apart from
// "never supposed to be sent" versus "dropped".
static bool bLastWriteWasModified = false;

#define MAX_WRITE_ALL_MISSING_REPORTED 16

// Reports exactly which params are missing after a failed full-WriteAll WriteAllComplete, so
// the host can patch just those instead of re-streaming everything. If there are too many to
// enumerate usefully, or the mismatch isn't attributable to any missing param (bForceOverflow),
// skip the list and send the 0xFFFF sentinel so the host falls back to a full resend.
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

MsgCmd ProcessParamMsg(CANRxFrame *rx, uint16_t *nIndex) {
    CANTxFrame tx;
    ParamMsg msg;

    if (rx->SID != stConfig.stDevice.nBaseId + CONFIG_RX_OFFSET)
        return MsgCmd::Invalid;

    if (rx->DLC != 8)
        return MsgCmd::Invalid;

    DecodeParamCmd(rx, &msg);

    *nIndex = msg.nIndex;

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
            if (param && WriteParam(param, msg.nValue)) {
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
                // Only apply if every param was received AND the value stream wasn't corrupted
                // (count alone can match even if a frame's bytes were mangled in transit).
                // WriteAllModified sends a host-chosen subset, so there's no missing-param
                // report/patch here — a mismatch just fails, as before.
                nWriteCrc = ~nWriteCrc; // Finalize CRC of what we actually received
                bApplied = (nNumWriteParams == nExpectedParams && nWriteCrc == nExpectedCrc) ? 1 : 0;
                if (bApplied) {
                    ApplyTempParams();
                }
                nReportedCount = nNumWriteParams;
                nReportedCrc = nWriteCrc;
            } else {
                // Full WriteAll: verify via the received-param bitset and a canonical
                // table-order CRC over temp values, both of which are independent of the
                // order frames actually arrived in — so this checks the same way whether
                // it's the first attempt or after a missing-param patch round.
                uint16_t nReceived = CountReceivedParams();
                uint32_t nCrc = (nReceived == nExpectedParams) ? CalcParamsCrc(true) : 0;
                bApplied = (nReceived == nExpectedParams && nCrc == nExpectedCrc) ? 1 : 0;
                if (bApplied) {
                    ApplyTempParams();
                } else {
                    bNeedsMissingList = true;
                    bCrcMismatchOnFullCount = (nReceived == nExpectedParams); // nothing to enumerate
                }
                nReportedCount = nReceived;
                nReportedCrc = nCrc;
            }

            EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::WriteAllComplete), nReportedCount, bApplied, nReportedCrc); // count, applied flag, and our CRC for comparison
            PostTxFrameWithRetry(&tx);

            // Sent after WriteAllComplete, never before: software only starts collecting
            // WriteAllMissing frames once it has seen the failing WriteAllComplete.
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

    return msg.eCmd;
}