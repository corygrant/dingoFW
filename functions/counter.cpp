#include "counter.h"
#include "dbc.h"
#include "edge.h"

void Counter::Update()
{
    if (!pConfig->bEnabled)
    {
        fVal = 0;
        return;
    }

    // Detect edges and store last states before any early return,
    // otherwise an edge is seen again every cycle (e.g. falling-edge reset stuck while low)
    bool bReset = Edge::Check(pConfig->eResetEdge, bLastReset, *pResetInput);
    bool bInc = Edge::Check(pConfig->eIncEdge, bLastInc, *pIncInput);
    bool bDec = Edge::Check(pConfig->eDecEdge, bLastDec, *pDecInput);

    bLastInc = *pIncInput;
    bLastDec = *pDecInput;
    bLastReset = *pResetInput;

    // Keep within range - covers startup (0) and min/max config changes
    if (fVal < pConfig->nMinCount)
        fVal = pConfig->nMinCount;
    if (fVal > pConfig->nMaxCount)
        fVal = pConfig->nMaxCount;

    // Reset
    if (bReset)
    {
        fVal = pConfig->nMinCount;
        return;
    }

    // Hold to reset
    if (pConfig->bHoldToReset)
    {
        if (*pIncInput && (SYS_TIME - nLastIncTime >= pConfig->nResetTime))
        {
            fVal = pConfig->nMinCount;
            return;
        }

        if (*pDecInput && (SYS_TIME - nLastDecTime >= pConfig->nResetTime))
        {
            fVal = pConfig->nMinCount;
            return;
        }
    }

    // Increment
    if (bInc)
    {
        if (fVal >= pConfig->nMaxCount)
            fVal = pConfig->bWrapAround ? pConfig->nMinCount : pConfig->nMaxCount;
        else
            fVal++;

        nLastIncTime = SYS_TIME;
    }

    // Decrement
    if (bDec)
    {
        if (fVal <= pConfig->nMinCount)
            fVal = pConfig->bWrapAround ? pConfig->nMaxCount : pConfig->nMinCount;
        else
            fVal--;

        nLastDecTime = SYS_TIME;
    }
}