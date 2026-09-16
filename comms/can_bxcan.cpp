// bxCAN backend (STM32F1/F3/F4 CAN peripheral). Used by every board with
// NUM_CAN_BUSES == 1. Only bus 0 exists here; the nBus parameters accepted
// by the public API (comms/can.h) are accepted for signature compatibility
// with comms/can_fdcan.cpp but otherwise unused - this backend only ever
// drives CAND1.
#include "can.h"
#include "hal.h"
#include "port.h"
#include "device_config.h"
#include "mailbox.h"
#include "msg.h"
#include "param_protocol.h"
#include "can_frame.h"

#include <iterator>

static CANFilter canfilters[STM32_CAN_MAX_FILTERS];
static uint32_t nFilterIds[STM32_CAN_MAX_FILTERS * 2];
static bool bFilterExtended[STM32_CAN_MAX_FILTERS * 2];

static uint32_t nLastCanRxTime;
static bool bCanFilterEnabled = true;

void ConfigureCanFilters();

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
                PostTxFrame(&msg.frame);
            }
        }

        if (chThdShouldTerminateX())
            chThdExit(MSG_OK);

        chThdSleepMilliseconds(CAN_TX_CYCLIC_MSG_DELAY);
    }
}

static THD_WORKING_AREA(waCanTxThread, 256);
void CanTxThread(void *)
{
    chRegSetThreadName("CAN Tx");

    CANTxFrame msg;

    while (1)
    {
        // Send all messages in the TX queue
        msg_t res;
        do
        {
            res = FetchTxFrame(&msg);
            if (res == MSG_OK)
            {
                canTransmitTimeout(&CAND1, CAN_ANY_MAILBOX, &msg, TIME_MS2I(10));
            }
            chThdSleepMicroseconds(CAN_TX_MSG_SPLIT);
        } while (res == MSG_OK);

        if (chThdShouldTerminateX())
            chThdExit(MSG_OK);

        chThdSleepMicroseconds(30);
    }
}

static THD_WORKING_AREA(waCanRxThread, 128);
void CanRxThread(void *)
{
    CANRxFrame msg;

    CANTxFrame usbTx;

    chRegSetThreadName("CAN Rx");

    while (true)
    {

        msg_t res = canReceiveTimeout(&CAND1, CAN_ANY_MAILBOX, &msg, TIME_IMMEDIATE);
        if (res == MSG_OK)
        {
            nLastCanRxTime = SYS_TIME;

            res = PostRxFrame(&msg);

            if(stConfig.stDevice.bConnectUsbToCan)
            {
                //Copy data to USB for data pass through
                //Don't send if it's a settings msg for this device
                uint32_t nId = CanFrameGetStdId(msg);
                if((nId != (uint32_t)(stConfig.stDevice.nBaseId + CONFIG_RX_OFFSET)) &&
                   (nId != (uint32_t)(stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET)))
                {
                    //If USB not connected, mailbox will fill up and messages will be dropped
                    CanFrameSetId(usbTx, nId, false);
                    usbTx.DLC = msg.DLC;
                    for(size_t i = 0; i < msg.DLC; i++)
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
static thread_t *canTxThreadRef;
static thread_t *canRxThreadRef;

msg_t InitCan(Config_Device *conf)
{
    if (canCyclicTxThreadRef || canTxThreadRef || canRxThreadRef)
    {
        StopCan();
    }

    SetCanFilterEnabled(conf->bCanFilterEnabled);

    ConfigureCanFilters();

    msg_t ret = canStart(&CAND1, &GetCanConfig(conf->eCanSpeed));
    if (ret != HAL_RET_SUCCESS)
        return ret;
    canCyclicTxThreadRef = chThdCreateStatic(waCanCyclicTxThread, sizeof(waCanCyclicTxThread), NORMALPRIO + 1, CanCyclicTxThread, nullptr);
    canTxThreadRef = chThdCreateStatic(waCanTxThread, sizeof(waCanTxThread), NORMALPRIO + 1, CanTxThread, nullptr);
    canRxThreadRef = chThdCreateStatic(waCanRxThread, sizeof(waCanRxThread), NORMALPRIO + 1, CanRxThread, nullptr);

    return HAL_RET_SUCCESS;
}

void StopCan()
{
    // Signal threads to terminate
    chThdTerminate(canCyclicTxThreadRef);
    chThdTerminate(canTxThreadRef);
    chThdTerminate(canRxThreadRef);

    // Wait for threads to exit
    chThdWait(canCyclicTxThreadRef);
    chThdWait(canTxThreadRef);
    chThdWait(canRxThreadRef);

    // Stop CAN driver
    canStop(&CAND1);

    // Reset thread references
    canCyclicTxThreadRef = NULL;
    canTxThreadRef = NULL;
    canRxThreadRef = NULL;
}

void ClearCanFilters(uint8_t /*nBus*/)
{
    // Clear all filters
    for (uint8_t i = 0; i < STM32_CAN_MAX_FILTERS; i++)
    {
        nFilterIds[i] = 0;
        bFilterExtended[i] = false;

        canfilters[i].register1 = 0;
        canfilters[i].register2 = 0;
        canfilters[i].filter = 0;
        canfilters[i].assignment = 0;
        canfilters[i].mode = 0;
        canfilters[i].scale = 0;
    }
}

void SetCanFilterId(uint8_t nFilterNum, uint32_t nId, bool bExtended, uint8_t /*nBus*/)
{
    if (nFilterNum >= (STM32_CAN_MAX_FILTERS * 2))
        return;

    bFilterExtended[nFilterNum] = bExtended;

    if (bExtended)
    {
        nFilterIds[nFilterNum] = (nId << 3) | 0x04; // Set IDE bit for extended ID
    }
    else
    {
        nFilterIds[nFilterNum] = nId << 21;
    }
}

void ConfigureCanFilters()
{

    if(!bCanFilterEnabled)
    {
        // Default HAL config = filter 0 enabled to allow all messages
        return;
    }

    uint8_t nCurrentFilter = 0;

    // Go through nFilterIds and set filter register1 and register2 for each filter if ID is set
    // CANNOT SET ALL FILTERS, MUST USE ONLY THE NUMBER OF REQUIRED FILTERS
    for (uint8_t i = 0; i < (STM32_CAN_MAX_FILTERS * 2); i += 2)
    {
        if (nFilterIds[i] != 0 || nFilterIds[i + 1] != 0)
        {
            canfilters[nCurrentFilter].filter = nCurrentFilter; // Filter bank number
            canfilters[nCurrentFilter].assignment = 0;          // Assign to FIFO 0
            canfilters[nCurrentFilter].mode = 1;                // List mode
            canfilters[nCurrentFilter].scale = 1;               // 32-bit scale

            // First ID (register1)
            if (nFilterIds[i] != 0)
            {
                canfilters[nCurrentFilter].register1 = nFilterIds[i];
            }

            // Second ID (register2)
            if (nFilterIds[i + 1] != 0)
            {
                canfilters[nCurrentFilter].register2 = nFilterIds[i + 1];
            }

            nCurrentFilter++;
        }
    }

    // Apply all filter configurations
    // If no CAN inputs are enabled, filter[0].register1 is still set to settings request message ID (BaseId-1)
    canSTM32SetFilters(&CAND1, STM32_CAN_MAX_FILTERS, nCurrentFilter, canfilters);
}

uint32_t GetLastCanRxTime()
{
    return nLastCanRxTime;
}

void SetCanFilterEnabled(bool bEnabled, uint8_t /*nBus*/)
{
    bCanFilterEnabled = bEnabled;

    // TODO: Reconfigure filters if enabled/disabled
}

// --- comms/can_frame.h portable frame accessors (bxCAN layout) ---

uint32_t CanFrameGetStdId(const CANRxFrame &frame) { return frame.SID; }
uint32_t CanFrameGetExtId(const CANRxFrame &frame) { return frame.EID; }
uint32_t CanFrameGetStdId(const CANTxFrame &frame) { return frame.SID; }
uint32_t CanFrameGetExtId(const CANTxFrame &frame) { return frame.EID; }

bool CanFrameIsExtended(const CANTxFrame &frame) { return frame.IDE != 0; }

void CanFrameSetId(CANTxFrame &frame, uint32_t nId, bool bExtended)
{
    frame.IDE = bExtended ? CAN_IDE_EXT : CAN_IDE_STD;
    if (bExtended)
        frame.EID = nId;
    else
        frame.SID = nId;
}

void CanFrameClearId(CANTxFrame &frame)
{
    frame.IDE = 0;
    frame.EID = 0; // Clears SID as well, union
}

void CanFrameSetStandardDefaults(CANTxFrame &frame)
{
    frame.IDE = CAN_IDE_STD;
    frame.RTR = CAN_RTR_DATA;
}
