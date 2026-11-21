#pragma once

#include "hal.h"
#include "enums.h"

#define DEVICE_TYPE 5

#define PROCESS_STACK 0x1000

#define HAS_SE_LEDS TRUE
#define HAS_ADC FALSE
#define HAS_USB TRUE
#define HAS_I2C1 TRUE
#define HAS_I2C2 FALSE
#define HAS_EXT_TEMP_SENSOR FALSE
#define HAS_EXT_MEMORY TRUE
#define EXT_MEMORY_I2C I2CD1
#define HAS_BATT_VOLT_SENSE FALSE
#define CAN_SLEEP TRUE
#define HAS_LIN TRUE
#define LIN_UART UARTD2

#define NUM_OUTPUTS 0
#define NUM_DIG_OUTPUTS 0
#define NUM_DIG_INPUTS 0
#define NUM_ANALOG_INPUTS 0
#define NUM_VIRT_INPUTS 16
#define NUM_CAN_INPUTS 32
#define NUM_CAN_OUTPUTS 32
#define NUM_CAN_BUSES 1
#define NUM_FLASHERS 4
#define NUM_WIPER_INTER_DELAYS 0
#define NUM_WIPER_SPEED_MAP 0
#define NUM_COUNTERS 4
#define NUM_CONDITIONS 32
#define NUM_MATH 8
#define NUM_TIMERS 8
#define NUM_KEYPADS 0
#define HAS_WIPERS FALSE
#define HAS_STARTER_DISABLE FALSE

#define KEYPAD_MAX_BUTTONS 0
#define KEYPAD_MAX_ANALOG_INPUTS 0
#define KEYPAD_MAX_DIALS 0

// Lua keeps its script in FRAM (core/lua_script.h)
#define HAS_LUA TRUE
#define NUM_LUA_OUTPUTS 32
#define LUA_HEAP_SIZE (40 * 1024)
#define EXT_MEMORY_SIZE 0x8000 // MB85RC256V

#define HAS_NEOPIXELS FALSE

//Always false
//Always true
//State
#define VAR_MAP_SYS_VARS 3
#define VAR_MAP_WIPER_VARS 0

#define VAR_MAP_SIZE ( \
    VAR_MAP_SYS_VARS + \
    (NUM_DIG_INPUTS * 3) + \
    (NUM_ANALOG_INPUTS * 4) + \
    (NUM_DIG_OUTPUTS * 1) + \
    (NUM_CAN_INPUTS * 2) + \
    (NUM_VIRT_INPUTS * 1) + \
    (NUM_OUTPUTS * 4) + \
    (NUM_FLASHERS * 1) + \
    (NUM_CONDITIONS * 1) + \
    (NUM_COUNTERS * 1) + \
    (NUM_TIMERS * 2) + \
    (NUM_MATH * 1) + \
    (NUM_LUA_OUTPUTS * 1) + \
    VAR_MAP_WIPER_VARS + \
    (NUM_KEYPADS * (KEYPAD_MAX_BUTTONS + KEYPAD_MAX_DIALS + KEYPAD_MAX_ANALOG_INPUTS)) \
)

// Last 4 x 2KB pages of flash (pages 252-255, 0x0807E000), reserved in dingopdm_v8.ld
// Assumes factory default dual bank mode (DBANK = 1, 2KB pages), which the ChibiOS G4 EFL driver requires
#define CONFIG_SECTOR       252U
#define CONFIG_NUM_SECTORS  4U
#define CONFIG_FLASH_OFFSET (CONFIG_SECTOR * 2048U)
#define CONFIG_FLASH        getBaseFlash(&EFLD1)

#define MAILBOX_SIZE 128
#define DEVICE_THREAD_STACK 2048
#define CAN_THREAD_STACK 512

#define NUM_TX_MSGS 37
#define DEFAULT_BASE_ID 0x0DE

#define SLEEP_TIMEOUT 30000

// Milliseconds from the 64-bit timestamp, truncated to 32 bits so it wraps cleanly
// every ~49.7 days. Always compare as (SYS_TIME - start), never as absolute times.
#define SYS_TIME ((uint32_t)(chVTGetTimeStamp() / (CH_CFG_ST_FREQUENCY / 1000)))

static const float ALWAYS_FALSE = 0.0f;
static const float ALWAYS_TRUE = 1.0f;

enum class LedType
{
    Status,
    Error
};

const CANConfig &GetCanConfig(CanBitrate bitrate);

const I2CConfig i2cConfig = {
    .timingr          = STM32_TIMINGR_PRESC(15U) | STM32_TIMINGR_SCLDEL(4U) | 
                        STM32_TIMINGR_SDADEL(2U) | STM32_TIMINGR_SCLH(15U) | 
                        STM32_TIMINGR_SCLL(21U),
    .cr1              = 0,
    .cr2              = 0
};

//msg_t InitAdc();
//void DeInitAdc();
//uint16_t GetAdcRaw(AnalogChannel channel);
//float GetAdcVolts(AnalogChannel channel);
//float GetBattVolt();
//float GetTemperature();
//float GetVDDA();