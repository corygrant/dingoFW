#include "sleep.h"
#include "device.h"
#include "port.h"
#include "can.h"
#include "usb.h"
#include "status.h"
#include "mcu_utils.h"
#include "device_config.h"

#if CAN_SLEEP
// Static variables that were in pdm.cpp
static uint8_t nNumOutputsOn;
static uint8_t nLastNumOutputsOn;
static uint32_t nAllOutputsOffTime;
static bool bLastUsbConnected;
static uint32_t nUsbDisconnectedTime;

// External variables from pdm.cpp that we need access to
extern DeviceConfig stConfig;
bool bSleepRequest;
extern Profet pf[NUM_OUTPUTS];
#if HAS_IGNITION
extern Ignition ignition;
#endif

static uint8_t nLastWakeSource;

bool CheckEnterSleep()
{
    bool bEnterSleep = false;

    // Count number of outputs on
    nNumOutputsOn = 0;
    for (int i = 0; i < NUM_OUTPUTS; i++)
    {
        if (GetOutputState(i) != ProfetState::Off)
            nNumOutputsOn++;
    }

    // All outputs just turned off, save time - wait SLEEP_TIMEOUT before sleep
    if ((nNumOutputsOn == 0) && (nLastNumOutputsOn > 0))
    {
        nAllOutputsOffTime = SYS_TIME;
    }
    nLastNumOutputsOn = nNumOutputsOn;

    //USB disconnected, save time - wait SLEEP_TIMEOUT before sleep
    if (!GetUsbConnected() && bLastUsbConnected)
        nUsbDisconnectedTime = SYS_TIME;

    bLastUsbConnected = GetUsbConnected();

    // CAN Rx thread can stamp a time slightly after SYS_TIME was read here,
    // so treat a negative (signed) difference as 0. Wrap-safe.
    int32_t nCanRxDiff = static_cast<int32_t>(SYS_TIME - GetLastCanRxTime());
    uint32_t nCanRxIdleTime = (nCanRxDiff > 0) ? nCanRxDiff : 0;

    // No outputs on, no CAN msgs received and no USB connected
    // Go to sleep after timeout
    bEnterSleep = stConfig.stDevice.bSleepEnabled &&
                  (nNumOutputsOn == 0) &&
                  (nLastNumOutputsOn == 0) &&
                  !GetUsbConnected() &&
                  ((SYS_TIME - nUsbDisconnectedTime) > SLEEP_TIMEOUT) &&
                  ((SYS_TIME - nAllOutputsOffTime) > SLEEP_TIMEOUT) &&
                  (nCanRxIdleTime > SLEEP_TIMEOUT);

    #if HAS_IGNITION
    if (ignition.SleepRequest())
        return true;
    #endif

    return bEnterSleep || bSleepRequest || *pVarMap[stConfig.stDevice.nForceSleepInput];
}

void EnableLineEventWithPull(ioline_t line, InputPull pull) 
{
    uint32_t eventMode = PAL_EVENT_MODE_BOTH_EDGES;
    
    switch(pull) {
        case InputPull::Up:
            eventMode |= PAL_STM32_PUPDR_PULLUP;
            break;
        case InputPull::Down:
            eventMode |= PAL_STM32_PUPDR_PULLDOWN;
            break;
        default:
            eventMode |= PAL_STM32_PUPDR_FLOATING;
            break;
    }
    
    palEnableLineEvent(line, eventMode);
}

void EnterSleep()
{
    // Clear CAN sleep request so we don't immediately re-enter sleep after waking up
    bSleepRequest = false;

    // Forced sleep can arrive with outputs on - shut them off, no current protection while stopped
    #if NUM_OUTPUTS > 0
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
        pf[i].ForceOff();
    #endif

    // Stop transmitting before the transceiver goes to standby. A frame still
    // going out while another device on the bus falls asleep wakes it up again.
    // Frames already in the CAN mailboxes finish during the wait below.
    SetCanTxQuiet(true);
    chThdSleepMilliseconds(100);

    // Stop the CAN controller before its pins change. Left running, it sees the
    // RX pin taken away from it and answers with an error frame, and the error
    // flag the other nodes send back arrives just after the transceiver went to
    // standby - which wakes this device straight back up whenever anything else
    // is on the bus.
    StopCan();
    palSetLineMode(LINE_CAN_TX, PAL_MODE_OUTPUT_PUSHPULL);
    palSetLine(LINE_CAN_TX); // Recessive

    palSetLine(LINE_CAN_STANDBY); // CAN disabled

    // Let the transceiver settle in standby before any wake source is armed
    chThdSleepMilliseconds(20);

    // Set wakeup sources

    // Digital inputs change detection, with configured pullup or pulldown
    // Can be disabled in config
    if(!stConfig.stDevice.bDisableDigInWake)
    {
        for(uint8_t i = 0; i < NUM_DIG_INPUTS; i++)
        {
            EnableLineEventWithPull(digIn[i].GetLine(), stConfig.stDigInput[i].ePull);
        }
    }

    // CAN receive detection - can be disabled in config
    if(!stConfig.stDevice.bDisableCanWake)
    {
        palSetLineMode(LINE_CAN_RX, PAL_MODE_INPUT);
        palEnableLineEvent(LINE_CAN_RX, PAL_EVENT_MODE_BOTH_EDGES | PAL_STM32_PUPDR_FLOATING);
    }

    // USB detection - always active
    palSetLineMode(LINE_USB_DP, PAL_MODE_INPUT);
    palEnableLineEvent(LINE_USB_DP, PAL_EVENT_MODE_BOTH_EDGES | PAL_STM32_PUPDR_FLOATING);
    palSetLineMode(LINE_USB_DM, PAL_MODE_INPUT);
    palEnableLineEvent(LINE_USB_DM, PAL_EVENT_MODE_BOTH_EDGES | PAL_STM32_PUPDR_FLOATING);

    // Arming a line can latch an edge from the pin changing mode. Clear those so
    // only a real wake source ends the stop. One arriving from here on is still
    // pending at the WFI and wakes the MCU as it should.
    EXTI->PR = 0x0000FFFF;
    NVIC_ClearPendingIRQ(EXTI0_IRQn);
    NVIC_ClearPendingIRQ(EXTI1_IRQn);
    NVIC_ClearPendingIRQ(EXTI2_IRQn);
    NVIC_ClearPendingIRQ(EXTI3_IRQn);
    NVIC_ClearPendingIRQ(EXTI4_IRQn);
    NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);

    EnterStopMode();
}

static uint32_t PadMask(ioline_t line)
{
    return 1UL << PAL_PAD(line);
}

void CaptureWakeSource()
{
    WakeRecord rec;
    if (!TakeWakeRecord(&rec))
    {
        nLastWakeSource = 0;
        return;
    }

    // EXTI line n serves pin n of whichever port is mapped to it
    uint32_t nCan = PadMask(LINE_CAN_RX);
    uint32_t nUsb = PadMask(LINE_USB_DP) | PadMask(LINE_USB_DM);
    uint32_t nDigIn = 0;
    for (uint8_t i = 0; i < NUM_DIG_INPUTS; i++)
        nDigIn |= PadMask(digIn[i].GetLine());

    const uint32_t nExti = rec.nExtiPending & 0xFFFF;
    uint8_t nSource = WAKE_SRC_FROM_SLEEP;
    if (nExti & nCan)
        nSource |= WAKE_SRC_CAN;
    if (nExti & nDigIn)
        nSource |= WAKE_SRC_DIG_IN;
    if (nExti & nUsb)
        nSource |= WAKE_SRC_USB;
    if (nExti & ~(nCan | nUsb | nDigIn))
        nSource |= WAKE_SRC_OTHER_LINE;

    // Any interrupt pending other than the EXTI ones also ends a stop. Only
    // worth reporting when no wake line fired: once awake the clocks restart
    // and the OS timer is pending again before the record is written.
    if (nSource != WAKE_SRC_FROM_SLEEP)
    {
        nLastWakeSource = nSource;
        return;
    }

    uint32_t nIrq[3] = {rec.nIrqPending[0], rec.nIrqPending[1], rec.nIrqPending[2]};
    const IRQn_Type eExtiIrqs[] = {EXTI0_IRQn, EXTI1_IRQn, EXTI2_IRQn, EXTI3_IRQn,
                                   EXTI4_IRQn, EXTI9_5_IRQn, EXTI15_10_IRQn};
    for (IRQn_Type irq : eExtiIrqs)
        nIrq[irq / 32] &= ~(1UL << (irq % 32));
    if (nIrq[0] | nIrq[1] | nIrq[2])
        nSource |= WAKE_SRC_INTERRUPT;

    nLastWakeSource = nSource;
}

uint8_t GetLastWakeSource()
{
    return nLastWakeSource;
}

#endif