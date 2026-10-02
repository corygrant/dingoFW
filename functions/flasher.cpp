#include "flasher.h"
#include "dbc.h"

void Flasher::Update(uint32_t nTimeNow)
{
    if (!pConfig->bEnabled)
    {
        fVal = 0;
        return;
    }

    if (!*pInput)
    {
        fVal = 0;
        bCycleDone = false; // Re-arm single cycle for the next time the input turns on
        return;
    }

    // Single cycle - one flash per activation of the input
    if (pConfig->bSingleCycle && bCycleDone)
    {
        fVal = 0;
        return;
    }

    if ((fVal == 0) && ((nTimeNow - nTimeOff) > pConfig->nFlashOffTime))
    {
        fVal = 1;
        nTimeOn = nTimeNow;
    }
    if ((fVal == 1) && ((nTimeNow - nTimeOn) > pConfig->nFlashOnTime))
    {
        fVal = 0;
        nTimeOff = nTimeNow;
        bCycleDone = true;
    }
}