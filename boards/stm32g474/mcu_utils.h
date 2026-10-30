#pragma once

// G4 factory calibration is taken at VDDA = 3.0V, at 30C and 130C (RM0440)
#define STM32_TS_CAL1 *((uint16_t *)0x1FFF75A8)
#define STM32_TS_CAL2 *((uint16_t *)0x1FFF75CA)
#define STM32_TS_CAL1_TEMP 30.0
#define STM32_TS_CAL2_TEMP 130.0
#define STM32_CAL_VDDA 3.0

#define STM32_VREF_INT_CAL *((uint16_t *)0x1FFF75AA)

// G4 splits the EXTI pending register into PR1 (lines 0-31) and PR2
#define EXTI_PENDING EXTI->PR1

void EnterStopMode();
void RequestBootloader();
void CheckBootloaderRequest(void);
