#pragma once

// F4 factory calibration is taken at VDDA = 3.3V, at 30C and 110C (RM0390)
#define STM32_TS_CAL1 *((uint16_t *)0x1FFF7A2C)
#define STM32_TS_CAL2 *((uint16_t *)0x1FFF7A2E)
#define STM32_TS_CAL1_TEMP 30.0
#define STM32_TS_CAL2_TEMP 110.0
#define STM32_CAL_VDDA 3.3

#define STM32_VREF_INT_CAL *((uint16_t *)0x1FFF7A2A)

// F4 has a single EXTI pending register
#define EXTI_PENDING EXTI->PR

void EnterStopMode();
void RequestBootloader();
void CheckBootloaderRequest(void);