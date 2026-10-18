#include "can_message.h"
#include "mailbox.h"

//=============================================================================
// A whole CAN frame with a fixed, user defined payload, sent while an input is
// true. CAN outputs write one variable into a bit field and cannot express an
// arbitrary payload, which is what a command frame to another device usually
// needs - for example the frame that tells a dashboard to shut down.
//
// nInterval is the gap between repeats while the input stays true. Receivers
// that debounce a command over several seconds need the frame to keep arriving,
// so repeating is the default. Setting nInterval to 0 sends a single frame on
// each rising edge instead.
//=============================================================================

void CanMessage::Update()
{
    if (!pConfig->bEnabled)
    {
        bInit = false;
        fVal = 0.0f;
        return;
    }

    const bool bIn = *pInput > 0.5f;
    const uint32_t nNow = SYS_TIME;

    // First pass only samples the input. Without this an input that is already
    // true at power up looks like a rising edge and sends a frame nobody asked for.
    if (!bInit)
    {
        bInit = true;
        bLast = bIn;
        fVal = 0.0f;
        return;
    }

    const bool bRising = bIn && !bLast;
    bLast = bIn;

    if (!bIn)
    {
        fVal = 0.0f;
        return;
    }

    bool bSend;
    if (pConfig->nInterval == 0)
        bSend = bRising;
    else
        bSend = bRising || ((nNow - nLastTxTime) >= pConfig->nInterval);

    if (bSend)
    {
        CANTxFrame frame;
        frame.IDE = (pConfig->nIDE == 1) ? CAN_IDE_EXT : CAN_IDE_STD;
        frame.RTR = CAN_RTR_DATA;

        if (pConfig->nIDE == 1)
            frame.EID = pConfig->nID;
        else
            frame.SID = pConfig->nID;

        frame.DLC = (pConfig->nDLC > 8) ? 8 : pConfig->nDLC;

        for (uint8_t i = 0; i < 8; i++)
            frame.data8[i] = pConfig->nData[i];

        PostTxFrame(&frame);

        nLastTxTime = nNow;
    }

    fVal = 1.0f;
}
