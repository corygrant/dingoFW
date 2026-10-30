#include "can.h"
#include "hal.h"
#include "port.h"
#include "device_config.h"
#include "mailbox.h"
#include "msg.h"
#include "param_protocol.h"
#include "can_frame.h"

#include <iterator>

static volatile uint32_t nLastCanRxTime;
// While set only replies to dingoConfig are sent, see SetCanTxQuiet()
static volatile bool bTxQuiet;

extern DeviceConfig stConfig;

// Bus 0 (CAN1) carries dingoConfig, USB passthrough and the cyclic status msgs
// Bus 1 (CAN2) carries CAN inputs/outputs only
static CANDriver *const canDrivers[NUM_CAN_BUSES] = {
    &CAND1,
    #if NUM_CAN_BUSES > 1
    &CAND2,
    #endif
};

static THD_WORKING_AREA(waCanCyclicTxThread, CAN_THREAD_STACK);
void CanCyclicTxThread(void *)
{
    chRegSetThreadName("CAN Cyclic Tx");

    CANTxMsg msg;

    while (1)
    {

        if (!IsParamOpInProgress() && !bTxQuiet && !*pVarMap[stConfig.stDevice.nMuteCanTxInput])
        {
            for (uint8_t i = 0; i < NUM_TX_MSGS; i++)
            {
                msg = TxMsgs[i]();
                if (!msg.bSend)
                    continue;
                PostTxFrame(&msg.frame);
            }
        }

        if (chThdShouldTerminateX())
            chThdExit(MSG_OK);

        chThdSleepMilliseconds(CAN_TX_CYCLIC_MSG_DELAY);
    }
}

static THD_WORKING_AREA(waCanTxThread[NUM_CAN_BUSES], CAN_THREAD_STACK);
void CanTxThread(void *arg)
{
    const uint8_t nBus = (uint8_t)(uintptr_t)arg;

    chRegSetThreadName("CAN Tx");

    CANTxFrame msg;

    while (1)
    {
        #if !defined(CAN_IDE_STD)
        // FDCAN has no automatic bus-off recovery (bxCAN ABOM), it sits in init until cleared
        if (canDrivers[nBus]->fdcan->CCCR & FDCAN_CCCR_INIT)
            canDrivers[nBus]->fdcan->CCCR &= ~FDCAN_CCCR_INIT;
        #endif

        // Blocks until a frame is queued, no polling while idle
        // Timeout only so the thread can check for termination
        if (FetchTxFrame(&msg, nBus, TIME_MS2I(100)) == MSG_OK)
        {
            // ID is set by each sender (CAN outputs can be extended), the other header bits aren't always initialized
            CanClearTxFlags(msg);

            const bool bConfigReply = (nBus == 0) && !CanIsExt(msg) &&
                                      (CanId(msg) == (uint32_t)(stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET));

            if (!bTxQuiet || bConfigReply)
                canTransmitTimeout(canDrivers[nBus], CAN_ANY_MAILBOX, &msg, TIME_MS2I(10));

            // Pacing between frames, set CAN_TX_MSG_SPLIT to 0 to send at bus rate
            #if CAN_TX_MSG_SPLIT > 0
            chThdSleepMicroseconds(CAN_TX_MSG_SPLIT);
            #endif
        }

        if (chThdShouldTerminateX())
            chThdExit(MSG_OK);
    }
}

static THD_WORKING_AREA(waCanRxThread[NUM_CAN_BUSES], CAN_THREAD_STACK);
void CanRxThread(void *arg)
{
    const uint8_t nBus = (uint8_t)(uintptr_t)arg;

    CANRxFrame msg;

    CANTxFrame usbTx;

    chRegSetThreadName("CAN Rx");

    while (true)
    {

        msg_t res = canReceiveTimeout(canDrivers[nBus], CAN_ANY_MAILBOX, &msg, TIME_MS2I(100));
        if (res == MSG_OK)
        {
            nLastCanRxTime = SYS_TIME;

            // Classic CAN DLC 9-15 is legal on the wire but still means 8 data bytes
            if (msg.DLC > 8)
                msg.DLC = 8;

            // Config frames go straight to the param thread
            if ((nBus != 0) || !RouteParamFrame(&msg))
                res = PostRxFrame(&msg, nBus);

            if((nBus == 0) && stConfig.stDevice.bConnectUsbToCan)
            {
                //Copy data to USB for data pass through
                //Don't send if it's a settings msg for this device
                if((CanId(msg) != (uint32_t)(stConfig.stDevice.nBaseId + CONFIG_RX_OFFSET)) && 
                   (CanId(msg) != (uint32_t)(stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET))) 
                {
                    //If USB not connected, mailbox will fill up and messages will be dropped
                    CanSetId(usbTx, CanId(msg), CanIsExt(msg));
                    usbTx.DLC = msg.DLC;
                    for(size_t i = 0; i < msg.DLC; i++)
                        usbTx.data8[i] = msg.data8[i];
                    res = PostTxUsbFrame(&usbTx);
                }
            }
        }

        if (chThdShouldTerminateX())
            chThdExit(MSG_OK);
    }
}

static thread_t *canCyclicTxThreadRef;
static thread_t *canTxThreadRef[NUM_CAN_BUSES];
static thread_t *canRxThreadRef[NUM_CAN_BUSES];

msg_t InitCan(Config_Device *conf)
{
    if (canCyclicTxThreadRef)
    {
        StopCan();
    }

    for (uint8_t nBus = 0; nBus < NUM_CAN_BUSES; nBus++)
    {
        msg_t ret = canStart(canDrivers[nBus], &GetCanConfig(conf->eCanSpeed[nBus]));
        if (ret != HAL_RET_SUCCESS)
            return ret;
    }

    UpdateCanFilters();

    canCyclicTxThreadRef = chThdCreateStatic(waCanCyclicTxThread, sizeof(waCanCyclicTxThread), NORMALPRIO + 1, CanCyclicTxThread, nullptr);
    for (uint8_t nBus = 0; nBus < NUM_CAN_BUSES; nBus++)
    {
        canTxThreadRef[nBus] = chThdCreateStatic(waCanTxThread[nBus], sizeof(waCanTxThread[nBus]), NORMALPRIO + 1, CanTxThread, (void *)(uintptr_t)nBus);
        canRxThreadRef[nBus] = chThdCreateStatic(waCanRxThread[nBus], sizeof(waCanRxThread[nBus]), NORMALPRIO + 1, CanRxThread, (void *)(uintptr_t)nBus);
    }

    return HAL_RET_SUCCESS;
}

void StopCan()
{
    // Signal threads to terminate
    chThdTerminate(canCyclicTxThreadRef);
    for (uint8_t nBus = 0; nBus < NUM_CAN_BUSES; nBus++)
    {
        chThdTerminate(canTxThreadRef[nBus]);
        chThdTerminate(canRxThreadRef[nBus]);
    }

    // Wait for threads to exit
    chThdWait(canCyclicTxThreadRef);
    canCyclicTxThreadRef = NULL;
    for (uint8_t nBus = 0; nBus < NUM_CAN_BUSES; nBus++)
    {
        chThdWait(canTxThreadRef[nBus]);
        chThdWait(canRxThreadRef[nBus]);
        canTxThreadRef[nBus] = NULL;
        canRxThreadRef[nBus] = NULL;

        // Stop CAN driver
        canStop(canDrivers[nBus]);
    }
}

//=============================================================================
// Hardware filters
// Rebuilt from the live config and written straight to the filter hardware,
// works while running.
//=============================================================================

#define FILTER_EXT 0x80000000u // Marks an extended ID in the collected list

#if defined(CAN_IDE_STD) // bxCAN
// The HAL's canSTM32SetFilters() needs the driver stopped (and turns the CAN
// clock off), registers are written directly instead: reception only pauses
// for the few us spent in filter init mode.

#define MAX_FILTER_IDS (STM32_CAN_MAX_FILTERS * 2) // 32-bit list mode, 2 IDs per bank

static uint32_t FilterReg(uint32_t nId)
{
    // FR layout: STID[31:21] or EXID[31:3], IDE bit 2
    return (nId & FILTER_EXT) ? (((nId & ~FILTER_EXT) << 3) | 0x04) : (nId << 21);
}

static void WriteFilters(uint8_t, const uint32_t *pIds, uint8_t nNumIds, bool bAcceptAll)
{
    CAN_TypeDef *can = CAND1.can;
    uint8_t nBanks = (nNumIds + 1) / 2;

    can->FMR |= CAN_FMR_FINIT;
    #if STM32_HAS_CAN2
    // HAL splits the banks 50/50 with CAN2, CAN2 is unused so give CAN1 all of them
    can->FMR = (can->FMR & ~CAN_FMR_CAN2SB) | (STM32_CAN_MAX_FILTERS << CAN_FMR_CAN2SB_Pos);
    #endif
    can->FA1R = 0;
    can->FFA1R = 0; // All banks to FIFO 0

    if (bAcceptAll)
    {
        // Bank 0, 32-bit mask mode, mask 0 = accept everything
        can->FM1R = 0;
        can->FS1R = 1;
        can->sFilterRegister[0].FR1 = 0;
        can->sFilterRegister[0].FR2 = 0;
        nBanks = 1;
    }
    else
    {
        // 32-bit list mode, odd count repeats the last ID in the spare slot
        for (uint8_t b = 0; b < nBanks; b++)
        {
            can->sFilterRegister[b].FR1 = FilterReg(pIds[b * 2]);
            can->sFilterRegister[b].FR2 = FilterReg((b * 2 + 1 < nNumIds) ? pIds[b * 2 + 1] : pIds[b * 2]);
        }
        can->FM1R = (1UL << nBanks) - 1;
        can->FS1R = (1UL << nBanks) - 1;
    }

    can->FA1R = (1UL << nBanks) - 1;
    can->FMR &= ~CAN_FMR_FINIT;
}

#else // FDCAN
// Filter elements live in message RAM, writable while running. Non-matching
// frames are rejected by RXGFC (see GetCanConfig), so accepting everything is
// a mask-0 filter, same as bxCAN.

#define FDCAN_STD_FILTERS 28 // STM32_FDCAN_FLS_NBR in the LLD
#define FDCAN_EXT_FILTERS 8  // STM32_FDCAN_FLE_NBR in the LLD
#define MAX_FILTER_IDS ((FDCAN_STD_FILTERS + FDCAN_EXT_FILTERS) * 2) // Dual ID mode, 2 IDs per element

static void WriteFilters(uint8_t nBus, const uint32_t *pIds, uint8_t nNumIds, bool bAcceptAll)
{
    // Every element is written so stale ones from a previous config are cleared
    // filter_cfg 0 (SFEC/EFEC = 0) disables the element
    CANFilter filters[FDCAN_STD_FILTERS + FDCAN_EXT_FILTERS] = {};
    CANFilter *pStd = &filters[0];
    CANFilter *pExt = &filters[FDCAN_STD_FILTERS];
    for (uint8_t i = 0; i < FDCAN_EXT_FILTERS; i++)
        pExt[i].filter_type = CAN_FILTER_TYPE_EXT;

    uint8_t nStd = 0;
    uint8_t nExt = 0;
    for (uint8_t i = 0; i < nNumIds && !bAcceptAll; i++)
    {
        const bool bExt = pIds[i] & FILTER_EXT;
        uint8_t &n = bExt ? nExt : nStd;
        if (n >= (bExt ? FDCAN_EXT_FILTERS : FDCAN_STD_FILTERS) * 2)
        {
            bAcceptAll = true; // More IDs of this type than elements
            break;
        }

        CANFilter &f = (bExt ? pExt : pStd)[n / 2];
        f.filter_mode = CAN_FILTER_MODE_DUAL;
        f.filter_cfg = CAN_FILTER_CFG_FIFO_0;
        if (n % 2 == 0)
            f.identifier1 = f.identifier2 = pIds[i] & ~FILTER_EXT; // Odd count repeats the ID in the spare slot
        else
            f.identifier2 = pIds[i] & ~FILTER_EXT;
        n++;
    }

    if (bAcceptAll)
    {
        for (CANFilter &f : filters)
            f.filter_cfg = static_cast<filter_cfg_t>(0);

        // Classic mode, mask 0 = accept everything
        pStd[0].filter_mode = pExt[0].filter_mode = CAN_FILTER_MODE_CLASSIC;
        pStd[0].filter_cfg = pExt[0].filter_cfg = CAN_FILTER_CFG_FIFO_0;
        pStd[0].identifier1 = pStd[0].identifier2 = 0;
        pExt[0].identifier1 = pExt[0].identifier2 = 0;
    }

    canSTM32SetFilters(canDrivers[nBus], FDCAN_STD_FILTERS + FDCAN_EXT_FILTERS, filters);
}

#endif

void UpdateCanFilters()
{
    for (uint8_t nBus = 0; nBus < NUM_CAN_BUSES; nBus++)
    {
        // Registers are only clocked while the driver is running, InitCan() calls this after canStart()
        if (canDrivers[nBus]->state != CAN_READY)
            continue;

        uint32_t nIds[MAX_FILTER_IDS];
        uint8_t nNumIds = 0;
        bool bOverflow = false;

        auto Add = [&](uint32_t nId, bool bExtended) {
            if (nNumIds < MAX_FILTER_IDS)
                nIds[nNumIds++] = bExtended ? (nId | FILTER_EXT) : nId;
            else
                bOverflow = true;
        };

        if (stConfig.stDevice.bCanFilterEnabled)
        {
            if (nBus == 0)
            {
                // Config/request frames from dingoConfig
                Add(stConfig.stDevice.nBaseId + CONFIG_RX_OFFSET, false);
            }

            for (uint8_t i = 0; i < NUM_CAN_INPUTS; i++)
            {
                if (stConfig.stCanInput[i].bEnabled && (CanInputBus(stConfig.stCanInput[i]) == nBus))
                    Add(stConfig.stCanInput[i].nID, stConfig.stCanInput[i].nIDE == 1);
            }

            #if NUM_KEYPADS > 0
            for (uint8_t i = 0; i < NUM_KEYPADS; i++)
            {
                const Config_Keypad &kp = stConfig.stKeypad[i];
                if (!kp.bEnabled || KeypadBus(kp) != nBus)
                    continue;

                // Both brands send button state on the CANopen TPDO1 ID
                Add(kp.nNodeId + static_cast<uint16_t>(BlinkMarineMessageId::ButtonState), false);

                if (kp.eModel < KeypadModel::Grayhill6Key)
                {
                    Add(kp.nNodeId + static_cast<uint16_t>(BlinkMarineMessageId::DialState1), false);
                    Add(kp.nNodeId + static_cast<uint16_t>(BlinkMarineMessageId::DialState2), false);
                    Add(kp.nNodeId + static_cast<uint16_t>(BlinkMarineMessageId::AnalogInput), false);
                }
            }
            #endif
        }

        // Filtering disabled, or more IDs than filter slots - accept everything rather than drop frames
        WriteFilters(nBus, nIds, nNumIds, !stConfig.stDevice.bCanFilterEnabled || bOverflow);
    }
}

// Before sleep every device on the bus has to stop transmitting, a frame from
// one that is still awake wakes the others straight back up. Covers the cyclic
// status, CAN outputs, CAN messages and keypad frames alike, as they all go
// through CanTxThread. Frames queued while quiet are dropped, not delayed.
void SetCanTxQuiet(bool bQuiet)
{
    bTxQuiet = bQuiet;
}

uint32_t GetLastCanRxTime()
{
    return nLastCanRxTime;
}