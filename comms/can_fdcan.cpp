// FDCAN backend (STM32G4/H7-family FDCAN peripheral). Used by boards with
// NUM_CAN_BUSES > 1. Unlike bxCAN, FDCAN requires the driver to already be
// in CAN_READY state (i.e. canStart() already called) before
// canSTM32SetFilters() may be used - the opposite order from
// comms/can_bxcan.cpp, which sets filters while the driver is CAN_STOP.
// FDCAN also partitions filters into separate standard/extended pools
// (STM32_FDCAN_FLS_NBR/STM32_FDCAN_FLE_NBR) instead of bxCAN's single mixed
// pool, and its CANRxFrame/CANTxFrame use .std.SID/.ext.EID/.common.{XTD,RTR}
// bitfields instead of bxCAN's flat .SID/.EID/.IDE/.RTR - see can_frame.h.
#include "can.h"
#include "hal.h"
#include "port.h"
#include "device_config.h"
#include "mailbox.h"
#include "msg.h"
#include "can_frame.h"
#include "param_protocol.h"

static CANDriver * const canDrivers[NUM_CAN_BUSES] = {
    &CAND1,
    &CAND2,
};

// FDCAN partitions filters into separate standard/extended pools, each
// usable in "dual ID" mode (2 IDs per filter element) - see
// STM32_FDCAN_FLS_NBR/STM32_FDCAN_FLE_NBR in the FDCANv1 LLD. Bookkeeping is
// per-bus and kept separate from bxCAN's flat/mixed nFilterIds arrays since
// the hardware itself doesn't allow borrowing between std/ext pools.
#define FDCAN_MAX_STD_FILTERS 28
#define FDCAN_MAX_EXT_FILTERS 8

static uint32_t nStdFilterIds[NUM_CAN_BUSES][FDCAN_MAX_STD_FILTERS * 2];
static uint32_t nExtFilterIds[NUM_CAN_BUSES][FDCAN_MAX_EXT_FILTERS * 2];
static CANFilter canFilters[NUM_CAN_BUSES][FDCAN_MAX_STD_FILTERS + FDCAN_MAX_EXT_FILTERS];

static uint32_t nLastCanRxTime;
static bool bCanFilterEnabled[NUM_CAN_BUSES];

static void ConfigureCanFilters(uint8_t nBus);

static THD_WORKING_AREA(waCanCyclicTxThread, 128);
void CanCyclicTxThread(void *)
{
    chRegSetThreadName("CAN Cyclic Tx");

    CANTxMsg msg;

    while (1)
    {
        if (!IsCyclicTxPaused())
        {
            for (uint8_t i = 0; i < NUM_TX_MSGS; i++)
            {
                msg = TxMsgs[i]();
                if (!msg.bSend)
                    continue;
                CanFrameSetStandardDefaults(msg.frame);
                PostTxFrame(&msg.frame, msg.nBus);
            }
        }

        if (chThdShouldTerminateX())
            chThdExit(MSG_OK);

        chThdSleepMilliseconds(CAN_TX_CYCLIC_MSG_DELAY);
    }
}

static THD_WORKING_AREA(waCanTxThread0, 256);
static THD_WORKING_AREA(waCanTxThread1, 256);
void CanTxThread(void *arg)
{
    uint8_t nBus = (uint8_t)(uintptr_t)arg;
    CANDriver *canp = canDrivers[nBus];

    chRegSetThreadName("CAN Tx");

    CANTxFrame msg;

    while (1)
    {
        // Send all messages in the TX queue
        msg_t res;
        do
        {
            res = FetchTxFrame(&msg, nBus);
            if (res == MSG_OK)
            {
                canTransmitTimeout(canp, CAN_ANY_MAILBOX, &msg, TIME_MS2I(10));
            }
            chThdSleepMicroseconds(CAN_TX_MSG_SPLIT);
        } while (res == MSG_OK);

        if (chThdShouldTerminateX())
            chThdExit(MSG_OK);

        chThdSleepMicroseconds(30);
    }
}

static THD_WORKING_AREA(waCanRxThread0, 128);
static THD_WORKING_AREA(waCanRxThread1, 128);
void CanRxThread(void *arg)
{
    uint8_t nBus = (uint8_t)(uintptr_t)arg;
    CANDriver *canp = canDrivers[nBus];

    CANRxFrame msg;
    CANTxFrame usbTx;

    chRegSetThreadName("CAN Rx");

    while (true)
    {
        msg_t res = canReceiveTimeout(canp, CAN_ANY_MAILBOX, &msg, TIME_IMMEDIATE);
        if (res == MSG_OK)
        {
            nLastCanRxTime = SYS_TIME;

            res = PostRxFrame(&msg, nBus);

            // USB<->CAN passthrough is bus-0-only, same as the settings
            // protocol (core/device.cpp CyclicUpdate gates both on nBus==0).
            // The settings protocol always uses standard IDs, so - matching
            // the original bxCAN backend's comparison against msg.SID - the
            // standard-ID interpretation is used here regardless of the
            // frame's actual type.
            if (nBus == 0 && stConfig.stDevice.bConnectUsbToCan)
            {
                uint32_t nId = CanFrameGetStdId(msg);
                if ((nId != (uint32_t)(stConfig.stDevice.nBaseId + CONFIG_RX_OFFSET)) &&
                    (nId != (uint32_t)(stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET)))
                {
                    CanFrameSetId(usbTx, nId, false);
                    usbTx.DLC = msg.DLC;
                    for (size_t i = 0; i < msg.DLC; i++)
                        usbTx.data8[i] = msg.data8[i];
                    res = PostTxUsbFrame(&usbTx);
                }
            }
        }

        if (chThdShouldTerminateX())
            chThdExit(MSG_OK);

        chThdSleepMicroseconds(30);
    }
}

static thread_t *canCyclicTxThreadRef;
static thread_t *canTxThreadRef[NUM_CAN_BUSES];
static thread_t *canRxThreadRef[NUM_CAN_BUSES];

msg_t InitCan(Config_Device *conf)
{
    if (canCyclicTxThreadRef || canTxThreadRef[0] || canRxThreadRef[0])
    {
        StopCan();
    }

    for (uint8_t nBus = 0; nBus < NUM_CAN_BUSES; nBus++)
        SetCanFilterEnabled(conf->bCanFilterEnabled, nBus);

    // FDCAN requires CAN_READY (i.e. canStart() already called) before
    // canSTM32SetFilters() may be used - the opposite order from bxCAN.
    for (uint8_t nBus = 0; nBus < NUM_CAN_BUSES; nBus++)
    {
        msg_t ret = canStart(canDrivers[nBus], &GetCanConfig(nBus, conf->eCanSpeed));
        if (ret != HAL_RET_SUCCESS)
            return ret;
        ConfigureCanFilters(nBus);
    }

    canCyclicTxThreadRef = chThdCreateStatic(waCanCyclicTxThread, sizeof(waCanCyclicTxThread), NORMALPRIO + 1, CanCyclicTxThread, nullptr);

    canTxThreadRef[0] = chThdCreateStatic(waCanTxThread0, sizeof(waCanTxThread0), NORMALPRIO + 1, CanTxThread, (void *)(uintptr_t)0);
    canRxThreadRef[0] = chThdCreateStatic(waCanRxThread0, sizeof(waCanRxThread0), NORMALPRIO + 1, CanRxThread, (void *)(uintptr_t)0);
#if NUM_CAN_BUSES > 1
    canTxThreadRef[1] = chThdCreateStatic(waCanTxThread1, sizeof(waCanTxThread1), NORMALPRIO + 1, CanTxThread, (void *)(uintptr_t)1);
    canRxThreadRef[1] = chThdCreateStatic(waCanRxThread1, sizeof(waCanRxThread1), NORMALPRIO + 1, CanRxThread, (void *)(uintptr_t)1);
#endif

    return HAL_RET_SUCCESS;
}

void StopCan()
{
    chThdTerminate(canCyclicTxThreadRef);
    for (uint8_t nBus = 0; nBus < NUM_CAN_BUSES; nBus++)
    {
        chThdTerminate(canTxThreadRef[nBus]);
        chThdTerminate(canRxThreadRef[nBus]);
    }

    chThdWait(canCyclicTxThreadRef);
    for (uint8_t nBus = 0; nBus < NUM_CAN_BUSES; nBus++)
    {
        chThdWait(canTxThreadRef[nBus]);
        chThdWait(canRxThreadRef[nBus]);
        canStop(canDrivers[nBus]);
        canTxThreadRef[nBus] = NULL;
        canRxThreadRef[nBus] = NULL;
    }
    canCyclicTxThreadRef = NULL;
}

void ClearCanFilters(uint8_t nBus)
{
    for (uint8_t i = 0; i < FDCAN_MAX_STD_FILTERS * 2; i++)
        nStdFilterIds[nBus][i] = 0;
    for (uint8_t i = 0; i < FDCAN_MAX_EXT_FILTERS * 2; i++)
        nExtFilterIds[nBus][i] = 0;
}

// nFilterNum indexes a combined std+ext ID space: [0, FDCAN_MAX_STD_FILTERS*2)
// are standard IDs, the range above that (up to FDCAN_MAX_EXT_FILTERS*2 more)
// are extended IDs. Callers (core/config_handler.cpp) pick the right range
// based on the input's own configured ID type.
void SetCanFilterId(uint8_t nFilterNum, uint32_t nId, bool bExtended, uint8_t nBus)
{
    if (bExtended)
    {
        if (nFilterNum < FDCAN_MAX_EXT_FILTERS * 2)
            nExtFilterIds[nBus][nFilterNum] = nId;
    }
    else
    {
        if (nFilterNum < FDCAN_MAX_STD_FILTERS * 2)
            nStdFilterIds[nBus][nFilterNum] = nId;
    }
}

static void ConfigureCanFilters(uint8_t nBus)
{
    if (!bCanFilterEnabled[nBus])
        return; // Default HAL config accepts all messages

    uint8_t nCurrentFilter = 0;

    // Pack standard IDs 2-per-filter using dual-ID mode, same packing
    // strategy as the bxCAN backend's list-mode banks.
    for (uint8_t i = 0; i < (FDCAN_MAX_STD_FILTERS * 2); i += 2)
    {
        if (nStdFilterIds[nBus][i] == 0 && nStdFilterIds[nBus][i + 1] == 0)
            continue;

        canFilters[nBus][nCurrentFilter].filter_type = CAN_FILTER_TYPE_STD;
        canFilters[nBus][nCurrentFilter].filter_mode = CAN_FILTER_MODE_DUAL;
        canFilters[nBus][nCurrentFilter].filter_cfg = CAN_FILTER_CFG_FIFO_0;
        canFilters[nBus][nCurrentFilter].identifier1 = nStdFilterIds[nBus][i];
        canFilters[nBus][nCurrentFilter].identifier2 = nStdFilterIds[nBus][i + 1];
        nCurrentFilter++;
    }

    for (uint8_t i = 0; i < (FDCAN_MAX_EXT_FILTERS * 2); i += 2)
    {
        if (nExtFilterIds[nBus][i] == 0 && nExtFilterIds[nBus][i + 1] == 0)
            continue;

        canFilters[nBus][nCurrentFilter].filter_type = CAN_FILTER_TYPE_EXT;
        canFilters[nBus][nCurrentFilter].filter_mode = CAN_FILTER_MODE_DUAL;
        canFilters[nBus][nCurrentFilter].filter_cfg = CAN_FILTER_CFG_FIFO_0;
        canFilters[nBus][nCurrentFilter].identifier1 = nExtFilterIds[nBus][i];
        canFilters[nBus][nCurrentFilter].identifier2 = nExtFilterIds[nBus][i + 1];
        nCurrentFilter++;
    }

    canSTM32SetFilters(canDrivers[nBus], nCurrentFilter, canFilters[nBus]);
}

uint32_t GetLastCanRxTime()
{
    return nLastCanRxTime;
}

void SetCanFilterEnabled(bool bEnabled, uint8_t nBus)
{
    bCanFilterEnabled[nBus] = bEnabled;
}

// --- comms/can_frame.h portable frame accessors (FDCAN layout) ---

uint32_t CanFrameGetStdId(const CANRxFrame &frame) { return frame.std.SID; }
uint32_t CanFrameGetExtId(const CANRxFrame &frame) { return frame.ext.EID; }
uint32_t CanFrameGetStdId(const CANTxFrame &frame) { return frame.std.SID; }
uint32_t CanFrameGetExtId(const CANTxFrame &frame) { return frame.ext.EID; }

bool CanFrameIsExtended(const CANTxFrame &frame) { return frame.common.XTD != 0; }

void CanFrameSetId(CANTxFrame &frame, uint32_t nId, bool bExtended)
{
    frame.common.XTD = bExtended ? 1 : 0;
    if (bExtended)
        frame.ext.EID = nId;
    else
        frame.std.SID = nId;
}

void CanFrameClearId(CANTxFrame &frame)
{
    frame.common.XTD = 0;
    frame.ext.EID = 0; // Overlaps std.SID's bits, same union
}

void CanFrameSetStandardDefaults(CANTxFrame &frame)
{
    frame.common.RTR = 0; // Data frame
    frame.FDF = 0;        // Classic CAN, not CAN-FD
}
