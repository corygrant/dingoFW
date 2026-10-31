#pragma once

#include "hal.h"
#include "enums.h"

#define DEVICE_TYPE 4

#define PROCESS_STACK 0x1000

#define HAS_SE_LEDS TRUE
#define HAS_USB TRUE
#define HAS_I2C1 TRUE
#define HAS_I2C2 TRUE
#define HAS_EXT_TEMP_SENSOR FALSE
#define HAS_EXT_MEMORY FALSE
#define EXT_MEMORY_I2C I2CD2
#define HAS_BATT_VOLT_SENSE TRUE
#define CAN_SLEEP TRUE

#define NUM_OUTPUTS 16
#define NUM_DIG_OUTPUTS 0
#define NUM_DIG_INPUTS 6
#define NUM_ANALOG_INPUTS 4
#define NUM_VIRT_INPUTS 16
#define NUM_CAN_INPUTS 32
#define NUM_CAN_OUTPUTS 32
#define NUM_CAN_BUSES 2
#define NUM_FLASHERS 4
#define NUM_WIPER_INTER_DELAYS 6
#define NUM_WIPER_SPEED_MAP 8
#define NUM_COUNTERS 4
#define NUM_CONDITIONS 32
#define NUM_MATH 8
#define NUM_TIMERS 8
#define NUM_KEYPADS 2
#define HAS_WIPERS TRUE
#define HAS_STARTER_DISABLE TRUE

#define KEYPAD_MAX_BUTTONS 20
#define KEYPAD_MAX_ANALOG_INPUTS 4
#define KEYPAD_MAX_DIALS 2

#define HAS_LUA FALSE // Needs FRAM for the script
#define NUM_LUA_OUTPUTS 0

#define HAS_NEOPIXELS TRUE
#define NUM_INT_NEOPIXELS 5
#define MAX_NEOPIXELS 16
// TIM1 clock = 170MHz, CCR counts for 0.4µs / 0.8µs high time
#define NEO_T0H 68
#define NEO_T1H 136
// TIM1 Update → any free DMA channel, routed by DMAMUX
#define NEO_DMA_STREAM_ID STM32_DMA_STREAM_ID_ANY
#define NEO_DMA_REQUEST STM32_DMAMUX1_TIM1_UP

//Always false
//Always true
//State
//Temperature sensor
//Battery voltage
#define VAR_MAP_SYS_VARS 4

#define VAR_MAP_WIPER_VARS 6

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

#define ADC1_NUM_CHANNELS 7
#define ADC1_BUF_DEPTH 1

#define ADC2_NUM_CHANNELS 11
#define ADC2_BUF_DEPTH 1

#define ADC3_NUM_CHANNELS 2
#define ADC3_BUF_DEPTH 1

#define ADC4_NUM_CHANNELS 1
#define ADC4_BUF_DEPTH 1

#define ADC5_NUM_CHANNELS 2
#define ADC5_BUF_DEPTH 1

#define BTS7002_1EPP_KILIS 22950

#define SLEEP_TIMEOUT 30000

// Milliseconds from the 64-bit timestamp, truncated to 32 bits so it wraps cleanly
// every ~49.7 days. Always compare as (SYS_TIME - start), never as absolute times.
#define SYS_TIME ((uint32_t)(chVTGetTimeStamp() / (CH_CFG_ST_FREQUENCY / 1000)))

static const float ALWAYS_FALSE = 0.0f;
static const float ALWAYS_TRUE = 1.0f;
 
enum class AnalogChannel
{
    IS1 = 0,
    IS2,
    IS3,
    IS4,
    IS5,
    IS6,
    IS7,
    IS8,
    IS9,
    IS10,
    IS11,
    IS12,
    IS13,
    IS14,
    IS15,
    IS16,
    AIN1,
    AIN2,
    AIN3,
    AIN4,
    BattVolt,
    TempSensor,
    VRefInt
};

enum class LedType
{
    Status,
    Error
};

const CANConfig &GetCanConfig(CanBitrate bitrate);

/*
 I2C kernel clock = PCLK1 = 170MHz, 400kHz fast mode, analog filter on
 PRESC = 1 -> 11.76ns tick, SCLL = 155 (1.84us), SCLH = 45 (0.54us + sync), SCLDEL = 8, SDADEL = 0
*/
const I2CConfig i2cConfig = {
    STM32_TIMINGR_PRESC(1U) |
    STM32_TIMINGR_SCLDEL(8U) | STM32_TIMINGR_SDADEL(0U) |
    STM32_TIMINGR_SCLH(45U) | STM32_TIMINGR_SCLL(155U),
    0,
    0
};

msg_t InitAdc();
void DeInitAdc();
uint16_t GetAdcRaw(AnalogChannel channel);
float GetAdcVolts(AnalogChannel channel);
float GetBattVolt();
float GetTemperature();
float GetVDDA();

#if HAS_NEOPIXELS
void UpdateNeopixels();
#endif