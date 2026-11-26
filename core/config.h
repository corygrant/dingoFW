#pragma once

#include <cstdint>
#include "port.h"
#include "enums.h"
#include "dbc.h"
#include "virtual_input.h"
#include "flasher.h"
#include "can_input.h"
#include "can_output.h"
#include "counter.h"
#include "condition.h"
#include "timer.h"
#include "math_block.h"
#if NUM_OUTPUTS > 0
#include "profet.h"
#endif
#if HAS_WIPERS > 0
#include "wiper.h"
#endif
#if HAS_STARTER_DISABLE > 0
#include "starter.h"
#endif
#if NUM_KEYPADS > 0
#include "keypad.h"
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
#if NUM_LIN_FRAMES > 0
#include "lin_frame.h"
#include "lin_input.h"
#include "lin_output.h"
#endif  

#define CONFIG_VERSION 0x000A //Increment when config structure changes

struct Config_Device{
  uint16_t nConfigVersion;
  uint16_t nBaseId;
  CanBitrate eCanSpeed[2]; // per bus, [1] unused on single bus boards
  bool bCanFilterEnabled;

  //Not used by all devices
  bool bSleepEnabled;
  bool bDisableDigInWake;
  bool bDisableCanWake;
  bool bConnectUsbToCan;
  uint16_t nMuteCanTxInput;
  uint16_t nForceSleepInput;
};

struct DeviceConfig{
  Config_Device stDevice;
  Config_VirtualInput stVirtualInput[NUM_VIRT_INPUTS];
  Config_Flasher stFlasher[NUM_FLASHERS];
  Config_CanInput stCanInput[NUM_CAN_INPUTS];
  Config_CanOutput stCanOutput[NUM_CAN_OUTPUTS];
  Config_Counter stCounter[NUM_COUNTERS];
  Config_Condition stCondition[NUM_CONDITIONS];
  #if NUM_TIMERS > 0
  Config_Timer stTimer[NUM_TIMERS];
  #endif
  #if NUM_MATH > 0
  Config_Math stMath[NUM_MATH];
  #endif

  #if NUM_DIG_INPUTS > 0
  Config_DigInput stDigInput[NUM_DIG_INPUTS];
  #endif
  #if NUM_OUTPUTS > 0
  Config_Output stOutput[NUM_OUTPUTS];
  #endif
  #if HAS_WIPERS 
  Config_Wiper stWiper;
  #endif
  #if HAS_STARTER_DISABLE
  Config_Starter stStarter;
  #endif
  #if NUM_KEYPADS > 0
  Config_Keypad stKeypad[NUM_KEYPADS];
  #endif
  #if NUM_DIG_OUTPUTS > 0
  Config_DigOutput stDigOutput[NUM_DIG_OUTPUTS];
  #endif
  #if NUM_ANALOG_INPUTS > 0
  Config_AnalogInput stAnalogInput[NUM_ANALOG_INPUTS];
  #endif
  #if NUM_LIN_FRAMES > 0
  Config_LinFrame stLinFrame[NUM_LIN_FRAMES];
  Config_LinInput stLinInput[NUM_LIN_INPUTS];
  Config_LinOutput stLinOutput[NUM_LIN_OUTPUTS];
  #endif
};

extern DeviceConfig stConfig;
extern DeviceConfig stConfigTemp; // Used for staging new config before applying

void InitConfig();
bool WriteConfig();