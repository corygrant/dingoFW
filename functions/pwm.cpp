#include "pwm.h"
#include "dbc.h"

#if NUM_OUTPUTS > 0
void Pwm::Update()
{
    bChannelEnabled = (bool)(m_pwm->enabled & (1 << 0));

    if (!pConfig->bEnabled || !bChannelEnabled) {
        bLastChannelEnabled = bChannelEnabled;
        bSoftStartComplete = false;
        nDutyCycle = 0;
    }

    if (!pConfig->bSoftStart) {
        nDutyCycle = GetTargetDutyCycle();
    }
    else
    {
        // Initialize soft start
        if (bChannelEnabled != bLastChannelEnabled) {
            InitSoftStart();
        }
        
        // Update soft start
        if (!bSoftStartComplete) {
            UpdateSoftStart();
        }
        // After soft start completes, continue updating duty cycle if variable
        else if (pConfig->bVariableDutyCycle) {
            nDutyCycle = GetTargetDutyCycle();
        }
    }

    UpdateFrequency();
    
    bLastChannelEnabled = bChannelEnabled;
}

uint8_t Pwm::GetTargetDutyCycle() {
    if (pConfig->bVariableDutyCycle && pConfig->nDutyCycleInputDenom > 0) {
        // Clamp duty cycle to 100% and minimum duty cycle
        float fDc = (*pInput) / pConfig->nDutyCycleInputDenom;
        if (fDc > 100.0f)
            fDc = 100.0f;
        if (!(fDc >= pConfig->nMinDutyCycle))
            fDc = pConfig->nMinDutyCycle;
        return (uint8_t)fDc;
    }
    return pConfig->nFixedDutyCycle;
}

void Pwm::InitSoftStart() {
    nSoftStartTime = SYS_TIME;

    // No ramp time configured - go straight to target instead of dividing by zero
    if (pConfig->nSoftStartRampTime == 0) {
        nDutyCycle = GetTargetDutyCycle();
        bSoftStartComplete = true;
        return;
    }

    fSoftStartStep = GetTargetDutyCycle() / (float)pConfig->nSoftStartRampTime;
    bSoftStartComplete = false;
}

void Pwm::UpdateSoftStart() {
    uint8_t targetDuty = GetTargetDutyCycle();
    // Compare in float - the ramp can overshoot 255 in one step on short ramps
    float fDuty = (fSoftStartStep * (SYS_TIME - nSoftStartTime)) + pConfig->nMinDutyCycle;

    if (fDuty >= targetDuty) {
        nDutyCycle = targetDuty;
        bSoftStartComplete = true;
    }
    else {
        nDutyCycle = (uint8_t)fDuty;
    }
}

void Pwm::UpdateFrequency()
{
    // Frequency within range and
    // (Frequency setting has changed or
    // PWM driver frequency != Frequency setting)
    if (((pConfig->nFreq <= 400) && (pConfig->nFreq > 0)) &&
        ((pConfig->nFreq != nLastFreq) ||
        (m_pwm->period != (m_pwmCfg->frequency / pConfig->nFreq))))
    {
        pwmChangePeriod(m_pwm, m_pwmCfg->frequency / pConfig->nFreq);
    }

    nLastFreq = pConfig->nFreq;
}

msg_t Pwm::Init()
{
    return pwmStart(m_pwm, m_pwmCfg);
}

void Pwm::EnsureStarted()
{
    if (m_pwm->state != PWM_READY)
        Init();
}

void Pwm::OverrideFrequency(uint16_t nFreq)
{
    if (nFreq > 0 && nFreq <= 400 &&
        m_pwm->period != (m_pwmCfg->frequency / nFreq))
    {
        pwmChangePeriod(m_pwm, m_pwmCfg->frequency / nFreq);
    }
}

void Pwm::On()
{
    // PWM duty cycle is 0-10000
    //  100% = 10000
    //  50% = 5000
    //  0% = 0
    pwmEnableChannel(m_pwm, static_cast<uint8_t>(m_pwmCh), PWM_PERCENTAGE_TO_WIDTH(m_pwm, nDutyCycle * 100));
    pwmEnablePeriodicNotification(m_pwm);
    pwmEnableChannelNotification(m_pwm, static_cast<uint8_t>(m_pwmCh));
}

void Pwm::Off()
{
    pwmDisablePeriodicNotification(m_pwm);
    pwmDisableChannel(m_pwm, static_cast<uint8_t>(m_pwmCh));
}
#endif