#include "mcu_utils.h"
#include "hal.h"

// With PRIMASK set, WFI still returns at once on any enabled pending IRQ (a CAN RX or timer IRQ that fired just
// before the mask), so the module reset instead of sleeping. Leave only the EXTI wake lines enabled; an edge
// already latched in EXTI->PR1 re-pends its IRQ and wakes at once, as it should.
static void QuietNvic()
{
    SysTick->CTRL = 0;
    for (uint32_t i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    static const IRQn_Type wake[] = {EXTI0_IRQn, EXTI1_IRQn, EXTI2_IRQn, EXTI3_IRQn, EXTI4_IRQn, EXTI9_5_IRQn, EXTI15_10_IRQn};
    for (IRQn_Type n : wake)
        NVIC_EnableIRQ(n);
}

void EnterStopMode()
{
    PWR->CR1 = (PWR->CR1 & ~PWR_CR1_LPMS) | PWR_CR1_LPMS_STOP1; // Stop 1 (regulator in low power, flash off automatically)
    PWR->SCR = PWR_SCR_CWUF | PWR_SCR_CSBF;                 // clear wakeup flags, clear standby flag
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;                      // enable deep sleep mode

    __disable_irq();
    QuietNvic();

    __WFI();

    // Resume here after wakeup
    NVIC_SystemReset();
}

 void RequestBootloader()
{
    // Set the magic code
    *((volatile unsigned long *)BL_HANDOFF_ADDR) = 0xDEADBEEF;

    // Reset the microcontroller to start the bootloader on next boot
    // The OpenBLT bootloader (bootloader/common/hooks.c) jumps to the
    // ST ROM USB DFU bootloader if the magic code is set
    NVIC_SystemReset();
    
    // No further code will execute after this point
}