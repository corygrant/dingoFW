#include "digital_output.h"

void Digital_Output::Update()
{
    if(!pConfig->bEnabled)
    {
        fVal = 0;
        palWriteLine(m_line, 0);
        return;
    }

    bool bOn = (*pInput != 0);
    palWriteLine(m_line, bOn ? PAL_HIGH : PAL_LOW);
    fVal = bOn;
}