#include "grayhill_button.h"
#include "keypad_button.h"

void UpdateButtonLedGrayhill(KeypadButton* btn)
{
    if (!btn->pConfig->bEnabled)
        return;

    uint8_t nColor = 0; // All LEDs off unless a var is active

    for (uint8_t i = 0; i < 4; i++)
    {
        if (static_cast<uint8_t>(*btn->pLedVars[i]) == 1)
            nColor = btn->pConfig->nColors[i];
    }

    // Fault LEDs takes precedence over value LEDs
    if (*btn->pFaultLedVar == 1)
        nColor = btn->pConfig->nFaultColor;

    btn->bLed[0] = (nColor & 0x01) > 0; // Bit 0
    btn->bLed[1] = (nColor & 0x02) > 0; // Bit 1
    btn->bLed[2] = (nColor & 0x04) > 0; // Bit 2
}
