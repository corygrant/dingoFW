#include "timer.h"

void Timer::Update()
{
    if (!pConfig->bEnabled)
    {
        bInit = false;
        bRunning = false;
        bOut = false;
        fVal = 0.0f;
        return;
    }

    const bool bIn = *pInput > 0.5f;
    const bool bReset = *pResetInput > 0.5f;
    const uint32_t nNow = SYS_TIME;

    // First pass after enable only samples the input. Without this a timer
    // whose input is already high at power up sees a rising edge that never
    // happened and fires a pulse on its own.
    if (!bInit)
    {
        bInit = true;
        bLastIn = bIn;
        bRunning = false;
        bOut = false;
        fVal = 0.0f;
        return;
    }

    // Reset is level sensitive, not edge: while it is held the timer stays
    // cleared, so it can be used to cancel a running delay and keep it cancelled
    if (bReset)
    {
        bRunning = false;
        bOut = false;
        bLastIn = bIn;
        fVal = 0.0f;
        return;
    }

    const bool bRising = bIn && !bLastIn;
    const bool bFalling = !bIn && bLastIn;
    const bool bElapsed = bRunning && ((nNow - nStartTime) >= pConfig->nTime);

    switch (pConfig->eMode)
    {
    case TimerMode::OnDelay:
        if (bRising)
        {
            bRunning = true;
            nStartTime = nNow;
        }

        if (!bIn)
        {
            bRunning = false;
            bOut = false;
        }
        else if (bElapsed)
        {
            bRunning = false;
            bOut = true;
        }
        break;

    case TimerMode::OffDelay:
        if (bIn)
        {
            bRunning = false;
            bOut = true;
        }
        else
        {
            if (bFalling)
            {
                bRunning = true;
                nStartTime = nNow;
            }

            if (bElapsed)
            {
                bRunning = false;
                bOut = false;
            }
        }
        break;

    case TimerMode::PulseRetrig:
        if (bRising)
        {
            bRunning = true;
            bOut = true;
            nStartTime = nNow;
        }
        else if (bElapsed)
        {
            bRunning = false;
            bOut = false;
        }
        break;

    case TimerMode::PulseOneShot:
        if (bRising && !bRunning)
        {
            bRunning = true;
            bOut = true;
            nStartTime = nNow;
        }
        else if (bElapsed)
        {
            bRunning = false;
            bOut = false;
        }
        break;
    }

    bLastIn = bIn;
    fVal = bOut ? 1.0f : 0.0f;
}
