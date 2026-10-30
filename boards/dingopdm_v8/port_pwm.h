#pragma once

#include "hal.h"

//===============================================
// Output PWM
// 4 outputs share each timer (one channel each), so each group of 4
// shares a frequency, set by the Ch1 output of the group.
// Pins are driven in software from the callbacks, so any timer can drive any pin.
//   TIM3 = PF1-4, TIM4 = PF5-8, TIM5 = PF9-12, TIM8 = PF13-16
//===============================================

// Period callback: raise every enabled channel with a non-zero duty
static inline void pwmSetLines(PWMDriver *pwmp, ioline_t ch1, ioline_t ch2, ioline_t ch3, ioline_t ch4)
{
    const ioline_t lines[4] = {ch1, ch2, ch3, ch4};
    for (uint8_t i = 0; i < 4; i++)
    {
        if ((pwmp->enabled & (1 << i)) && (pwmp->tim->CCR[i] > 0))
            palSetLine(lines[i]);
    }
}

//===============================================
// Output PWM period callbacks
//===============================================
static void pwmTim3pcb(PWMDriver *pwmp)
{
    pwmSetLines(pwmp, LINE_PF1_IN, LINE_PF2_IN, LINE_PF3_IN, LINE_PF4_IN);
}

static void pwmTim4pcb(PWMDriver *pwmp)
{
    pwmSetLines(pwmp, LINE_PF5_IN, LINE_PF6_IN, LINE_PF7_IN, LINE_PF8_IN);
}

static void pwmTim5pcb(PWMDriver *pwmp)
{
    pwmSetLines(pwmp, LINE_PF9_IN, LINE_PF10_IN, LINE_PF11_IN, LINE_PF12_IN);
}

static void pwmTim8pcb(PWMDriver *pwmp)
{
    pwmSetLines(pwmp, LINE_PF13_IN, LINE_PF14_IN, LINE_PF15_IN, LINE_PF16_IN);
}

//===============================================
// Output PWM duty cycle callbacks
//===============================================
#define PWM_DUTY_CB(name, ch, line)              \
    static void name(PWMDriver *pwmp)            \
    {                                            \
        if (pwmp->enabled & (1 << (ch)))         \
            palClearLine(line);                  \
    }

PWM_DUTY_CB(pwmOut1cb, 0, LINE_PF1_IN)
PWM_DUTY_CB(pwmOut2cb, 1, LINE_PF2_IN)
PWM_DUTY_CB(pwmOut3cb, 2, LINE_PF3_IN)
PWM_DUTY_CB(pwmOut4cb, 3, LINE_PF4_IN)
PWM_DUTY_CB(pwmOut5cb, 0, LINE_PF5_IN)
PWM_DUTY_CB(pwmOut6cb, 1, LINE_PF6_IN)
PWM_DUTY_CB(pwmOut7cb, 2, LINE_PF7_IN)
PWM_DUTY_CB(pwmOut8cb, 3, LINE_PF8_IN)
PWM_DUTY_CB(pwmOut9cb, 0, LINE_PF9_IN)
PWM_DUTY_CB(pwmOut10cb, 1, LINE_PF10_IN)
PWM_DUTY_CB(pwmOut11cb, 2, LINE_PF11_IN)
PWM_DUTY_CB(pwmOut12cb, 3, LINE_PF12_IN)
PWM_DUTY_CB(pwmOut13cb, 0, LINE_PF13_IN)
PWM_DUTY_CB(pwmOut14cb, 1, LINE_PF14_IN)
PWM_DUTY_CB(pwmOut15cb, 2, LINE_PF15_IN)
PWM_DUTY_CB(pwmOut16cb, 3, LINE_PF16_IN)

//===============================================
// Output PWM configurations
//===============================================
static const PWMConfig pwm3Cfg = {
    .frequency = 1000000,
    .period = 10000,
    .callback = pwmTim3pcb,
    .channels = {
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut1cb},
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut2cb},
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut3cb},
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut4cb}
    },
    .cr2 = 0,
    .bdtr = 0,
    .dier = 0
};

static const PWMConfig pwm4Cfg = {
    .frequency = 1000000,
    .period = 10000,
    .callback = pwmTim4pcb,
    .channels = {
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut5cb},
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut6cb},
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut7cb},
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut8cb}
    },
    .cr2 = 0,
    .bdtr = 0,
    .dier = 0
};

static const PWMConfig pwm5Cfg = {
    .frequency = 1000000,
    .period = 10000,
    .callback = pwmTim5pcb,
    .channels = {
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut9cb},
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut10cb},
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut11cb},
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut12cb}
    },
    .cr2 = 0,
    .bdtr = 0,
    .dier = 0
};

static const PWMConfig pwm8Cfg = {
    .frequency = 1000000,
    .period = 10000,
    .callback = pwmTim8pcb,
    .channels = {
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut13cb},
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut14cb},
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut15cb},
        {PWM_OUTPUT_ACTIVE_HIGH, pwmOut16cb}
    },
    .cr2 = 0,
    .bdtr = 0,
    .dier = 0
};

//===============================================
// NeoPixel PWM configuration
// DMA-driven: TIM1 Update event → DMAMUX → DMA, which streams CCR values
// directly — no CPU callbacks needed.
// TIM1 clock = 170MHz (APB2 = 170MHz, /1)
// Period = 212 counts → 802kHz bit rate (1.25µs per bit)
//===============================================
static const PWMConfig pwmNeoCfg = {
    .frequency = 170000000,
    .period    = 212,
    .callback  = NULL,
    .channels  = {
        {PWM_OUTPUT_ACTIVE_HIGH, NULL},  // PE9 = TIM1 CH1
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL}
    },
    .cr2  = 0,
    .bdtr = 0,
    .dier = STM32_TIM_DIER_UDE  // Update DMA Request enable
};
