#include "mcu_utils.h"
#include "hal.h"

// With PRIMASK set, WFI still returns at once on any enabled pending IRQ (a CAN RX or timer IRQ that fired just
// before the mask), so the module reset instead of sleeping. Leave only the EXTI wake lines enabled; an edge
// already latched in EXTI->PR re-pends its IRQ and wakes at once, as it should.
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
    PWR->CR &= ~PWR_CR_PDDS;	            // cleared PDDS means stop mode (not standby) 
	PWR->CR |= PWR_CR_CWUF | PWR_CR_CSBF;	// clear wakeup flag, clear standby flag
    PWR->CR |= PWR_CR_FPDS | PWR_CR_LPDS;	// turn off flash, regulator in low power mode
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;      // enable deep sleep mode

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