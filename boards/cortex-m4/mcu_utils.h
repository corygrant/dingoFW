#pragma once

#include <cstdint>

#define STM32_TEMP_3V3_30C *((uint16_t *)0x1FFF7A2C)
#define STM32_TEMP_3V3_110C *((uint16_t *)0x1FFF7A2E)

#define STM32_VREF_INT_CAL *((uint16_t *)0x1FFF7A2A)

// What brought the MCU out of stop mode. Kept in RAM across the reset that
// follows the wake up, read once at boot with TakeWakeRecord().
struct WakeRecord
{
    uint32_t nExtiPending;   // EXTI->PR, bit n = EXTI line n
    uint32_t nIrqPending[3]; // NVIC->ISPR[0..2]
};

void EnterStopMode();
bool TakeWakeRecord(WakeRecord *pRecord);
void RequestBootloader();
void CheckBootloaderRequest(void);