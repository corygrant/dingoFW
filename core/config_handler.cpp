#include "config_handler.h"
#include "msg.h"
#include "device_config.h"
#include "can.h"
#include "can_input.h"
#include "can_outputs.h"
#include "counter.h"
#include "condition.h"
#include "timer.h"
#include "can_message.h"
#include "flasher.h"
#include "virtual_input.h"
#if NUM_OUTPUTS > 0
#include "profet.h"
#endif
#if HAS_WIPERS > 0
#include "wiper/wiper.h"
#endif
#if HAS_STARTER_DISABLE > 0
#include "starter.h"
#endif
#if HAS_IGNITION > 0
#include "ignition.h"
#endif
#if NUM_KEYPADS > 0
#include "keypad/keypad.h"
#endif
#if NUM_DIG_INPUTS > 0
#include "digital_input.h"
#endif
#if NUM_DIG_OUTPUTS > 0
#include "digital_output.h"
#endif
#if NUM_ANALOG_INPUTS > 0
#include "analog_input.h"
#endif

extern DeviceConfig stConfig;
extern CanInput canIn[NUM_CAN_INPUTS];
extern CanOutputs canOutputs;
extern VirtualInput virtIn[NUM_VIRT_INPUTS];
extern Flasher flasher[NUM_FLASHERS];
extern Counter counter[NUM_COUNTERS];
extern Condition condition[NUM_CONDITIONS];
#if NUM_TIMERS > 0
extern Timer timer[NUM_TIMERS];
#endif
#if NUM_CAN_MESSAGES > 0
extern CanMessage canMsg[NUM_CAN_MESSAGES];
#endif
#if NUM_OUTPUTS > 0
extern Profet pf[NUM_OUTPUTS];
#endif
#if HAS_WIPERS
extern Wiper wiper;
#endif
#if HAS_STARTER_DISABLE
extern Starter starter;
#endif
#if HAS_IGNITION
extern Ignition ignition;
#endif
#if NUM_KEYPADS > 0
extern Keypad keypad[NUM_KEYPADS];
#endif
#if NUM_DIG_INPUTS > 0
extern Digital_Input digIn[NUM_DIG_INPUTS];
#endif
#if NUM_DIG_OUTPUTS > 0
extern Digital_Output digOut[NUM_DIG_OUTPUTS];
#endif
#if NUM_ANALOG_INPUTS > 0
extern Analog_Input analogIn[NUM_ANALOG_INPUTS];
#endif

static MUTEX_DECL(configMutex);

void LockConfig()
{
    chMtxLock(&configMutex);
}

void UnlockConfig()
{
    chMtxUnlock(&configMutex);
}

void ApplyAllConfig()
{
    ApplyConfig(CanInput::nBaseIndex);
    ApplyConfig(CanOutputs::nBaseIndex);
    ApplyConfig(VirtualInput::nBaseIndex);
    ApplyConfig(Flasher::nBaseIndex);
    ApplyConfig(Counter::nBaseIndex);
    ApplyConfig(Condition::nBaseIndex);
    #if NUM_TIMERS > 0
    ApplyConfig(Timer::nBaseIndex);
    #endif
    #if NUM_CAN_MESSAGES > 0
    ApplyConfig(CanMessage::nBaseIndex);
    #endif
    #if NUM_OUTPUTS > 0
    ApplyConfig(Profet::nBaseIndex);
    #endif
    #if HAS_WIPERS
    ApplyConfig(Wiper::nBaseIndex);
    #endif
    #if HAS_STARTER_DISABLE
    ApplyConfig(Starter::nBaseIndex);
    #endif
    #if HAS_IGNITION
    ApplyConfig(Ignition::nBaseIndex);
    #endif
    #if NUM_KEYPADS > 0
    ApplyConfig(Keypad::nBaseIndex);
    #endif
    #if NUM_DIG_INPUTS > 0
    ApplyConfig(Digital_Input::nBaseIndex);
    #endif
    #if NUM_DIG_OUTPUTS > 0
    ApplyConfig(Digital_Output::nBaseIndex);
    #endif
    #if NUM_ANALOG_INPUTS > 0
    ApplyConfig(Analog_Input::nBaseIndex);
    #endif
}

void ApplyConfig(uint16_t nIndex)
{
    uint16_t nBaseIndex = nIndex & 0xFF00;

    // Device config (0x0000) - filter enable and base ID (config frame ID) live here
    // CAN bitrate is not applied live - burn then send MsgCmd::Restart
    if (nBaseIndex == 0x0000)
    {
        UpdateCanFilters();
    }

    if (nBaseIndex == CanInput::nBaseIndex)
    {
        for (uint8_t i = 0; i < NUM_CAN_INPUTS; i++)
            canIn[i].SetConfig(&stConfig.stCanInput[i]);

        UpdateCanFilters();
    }

    if (nBaseIndex == CanOutputs::nBaseIndex)
    {
        canOutputs.SetConfig(stConfig.stCanOutput);

        CanOutputs::InitAllFrames();
    }

    if (nBaseIndex == VirtualInput::nBaseIndex)
    {
        for (uint8_t i = 0; i < NUM_VIRT_INPUTS; i++)
            virtIn[i].SetConfig(&stConfig.stVirtualInput[i]);
    }

    if (nBaseIndex == Flasher::nBaseIndex)
    {
        for (uint8_t i = 0; i < NUM_FLASHERS; i++)
            flasher[i].SetConfig(&stConfig.stFlasher[i]);
    }

    if (nBaseIndex == Counter::nBaseIndex)
    {
        for (uint8_t i = 0; i < NUM_COUNTERS; i++)
            counter[i].SetConfig(&stConfig.stCounter[i]);
    }

    if (nBaseIndex == Condition::nBaseIndex)
    {
        for (uint8_t i = 0; i < NUM_CONDITIONS; i++)
            condition[i].SetConfig(&stConfig.stCondition[i]);
    }

    #if NUM_TIMERS > 0
    if (nBaseIndex == Timer::nBaseIndex)
    {
        for (uint8_t i = 0; i < NUM_TIMERS; i++)
            timer[i].SetConfig(&stConfig.stTimer[i]);
    }
    #endif

    #if NUM_CAN_MESSAGES > 0
    if (nBaseIndex == CanMessage::nBaseIndex)
    {
        for (uint8_t i = 0; i < NUM_CAN_MESSAGES; i++)
            canMsg[i].SetConfig(&stConfig.stCanMessage[i]);
    }
    #endif

    #if NUM_OUTPUTS > 0
    if (nBaseIndex == Profet::nBaseIndex)
    {
        for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
            pf[i].SetConfig(&stConfig.stOutput[i]);

        // Clear all pairing pointers before linking
        for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
        {
            pf[i].pPrimary  = nullptr;
            pf[i].pFollower = nullptr;
        }

        // Link follower -> primary pairs with validation
        for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
        {
            int8_t pri = stConfig.stOutput[i].nPrimaryOutput;

            if (pri == -1)                                           continue; // unpaired
            if (pri == i)                                            continue; // self-pair
            if (pri >= NUM_OUTPUTS)                                  continue; // out of range
            if (stConfig.stOutput[pri].nPrimaryOutput != -1)         continue; // primary is itself a follower (no chains)
            if (!stConfig.stOutput[pri].bEnabled)                    continue; // primary not enabled

            pf[i].pPrimary      = &pf[pri];
            pf[pri].pFollower   = &pf[i];
        }
    }
    #endif

    #if HAS_WIPERS
    if (nBaseIndex == Wiper::nBaseIndex)
    {
        wiper.SetConfig(&stConfig.stWiper);
    }
    #endif

    #if HAS_STARTER_DISABLE
    if (nBaseIndex == Starter::nBaseIndex)
    {
        starter.SetConfig(&stConfig.stStarter);
    }
    #endif

    #if HAS_IGNITION
    if (nBaseIndex == Ignition::nBaseIndex)
    {
        ignition.SetConfig(&stConfig.stIgnition);
    }
    #endif

    #if NUM_KEYPADS > 0
    // Keypad (0x30xx), buttons (0x31xx) and dials (0x32xx) are all applied through Keypad::SetConfig
    if ((nBaseIndex & 0xF000) == Keypad::nBaseIndex)
    {
        for (uint8_t i = 0; i < NUM_KEYPADS; i++)
            keypad[i].SetConfig(&stConfig.stKeypad[i]);

        UpdateCanFilters(); // Node ID / model / enable change the keypad Rx IDs
    }
    #endif

    #if NUM_DIG_INPUTS > 0
    if (nBaseIndex == Digital_Input::nBaseIndex)
    {
        for (uint8_t i = 0; i < NUM_DIG_INPUTS; i++)
            digIn[i].SetConfig(&stConfig.stDigInput[i]);
    }
    #endif

    #if NUM_DIG_OUTPUTS > 0
    if (nBaseIndex == Digital_Output::nBaseIndex)
    {
        for (uint8_t i = 0; i < NUM_DIG_OUTPUTS; i++)
            digOut[i].SetConfig(&stConfig.stDigOutput[i]);
    }
    #endif

    #if NUM_ANALOG_INPUTS > 0
    if (nBaseIndex == Analog_Input::nBaseIndex)
    {
        for (uint8_t i = 0; i < NUM_ANALOG_INPUTS; i++)
            analogIn[i].SetConfig(&stConfig.stAnalogInput[i]);
    }
    #endif
}