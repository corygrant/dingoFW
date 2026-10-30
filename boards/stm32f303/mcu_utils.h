#pragma once

// F3 factory calibration is taken at VDDA = 3.3V, at 30C and 110C (RM0316)
#define STM32_TS_CAL1 *((uint16_t *)0x1FFFF7B8)
#define STM32_TS_CAL2 *((uint16_t *)0x1FFFF7C2)
#define STM32_TS_CAL1_TEMP 30.0
#define STM32_TS_CAL2_TEMP 110.0
#define STM32_CAL_VDDA 3.3

#define STM32_VREF_INT_CAL *((uint16_t *)0x1FFFF7BA)

// F3 pending register for lines 0-31 (PR2 holds the rest)
#define EXTI_PENDING EXTI->PR
