#include "lin_outputs.h"
#include "mailbox.h"
#include "dbc.h"
#include "lin_frame.h"

Config_LinOutput* LinOutputs::pConfigs[NUM_LIN_OUTPUTS];
float* LinOutputs::pInput[NUM_LIN_OUTPUTS];
LinOutput LinOutputs::linOut[LIN_OUT_FRAMES];
int8_t LinOutputs::nAssignedOut[NUM_LIN_OUTPUTS];

void LinOutputs::ClearFrames()
{
    for (int i = 0; i < LIN_OUT_FRAMES; ++i)
    {
        LinSetData64(linOut[i].stFrame, 0);
        linOut[i].stFrame.nLength = 0;
        linOut[i].stFrame.nId = 0;
        linOut[i].nBus = 0;
    }

    for (int i = 0; i < NUM_LIN_OUTPUTS; ++i)
    {
        nAssignedOut[i] = -1;
    }
}

void LinOutputs::InitAllFrames()
{
    ClearFrames();

    for (int i = 0; i < NUM_LIN_OUTPUTS; ++i)
    {
        if (!pConfigs[i]->bEnabled) continue;

        uint8_t  nNewId       = pConfigs[i]->nId;
        uint8_t  nNewStartBit = pConfigs[i]->nStartBit;
        uint8_t  nNewBitLen   = pConfigs[i]->nBitLength;
        uint16_t nNewInterval = pConfigs[i]->nInterval;
        uint8_t  nNewBus      = LinOutputBus(*pConfigs[i]);

        if(nNewBitLen == 0 || nNewBitLen > 64) continue; // Invalid config, skip
        if(nNewInterval == 0) nNewInterval = 100; // Default to 100ms

        // Look for an existing frame with a matching ID
        for (int j = 0; j < LIN_OUT_FRAMES; ++j)
        {
            if (linOut[j].stFrame.nLength == 0) continue; // Unused frame slot

            bool bMatch = (linOut[j].stFrame.nId == nNewId) &&
                          (linOut[j].nBus == nNewBus);
            if (bMatch)
            {
                nAssignedOut[i] = j;

                //Expand length to fit longest output assigned to the frame
                uint8_t newLength = CalcLength(nNewStartBit, nNewBitLen);
                if (linOut[j].stFrame.nLength < newLength)
                    linOut[j].stFrame.nLength = newLength;

                //Use shortest interval of any output assigned to the frame
                if (linOut[j].nInterval > nNewInterval)
                    linOut[j].nInterval = nNewInterval;

                break;
            }
        }

        if (nAssignedOut[i] != -1) continue;

        // No existing frame found, assign to first available slot
        for (int j = 0; j < LIN_OUT_FRAMES; ++j)
        {
            if (linOut[j].stFrame.nLength == 0)
            {
                nAssignedOut[i] = j;
                linOut[j].stFrame.nId = nNewId;
                linOut[j].nBus = nNewBus;

                linOut[j].stFrame.nLength = CalcLength(nNewStartBit, nNewBitLen);
                linOut[j].nInterval = nNewInterval;
                break;
            }
        }
    }
}

void LinOutputs::Update()
{
    for (int i = 0; i < NUM_LIN_OUTPUTS; ++i)
    {
        if (nAssignedOut[i] == -1) continue;

        if (linOut[i].stFrame.nLength == 0) continue; // Unused frame slot

        if (linOut[i].CheckTxTime())
        {
            // Update the frame data with the output value
            Dbc::EncodeFloat(pInput[i], pConfigs[i]->nStartBit, pConfigs[i]->nBitLength,
                             pConfigs[i]->fFactor, pConfigs[i]->fOffset,
                             pConfigs[i]->eByteOrder, pConfigs[i]->bSigned,
                             linOut[i].stFrame.nData);

            // Transmit the frame on the appropriate bus
            PostLinTxFrame(linOut[i].stFrame, linOut[i].nBus);
        }
    }
}

uint8_t LinOutputs::CalcLength(uint8_t nStartBit, uint8_t nBitLength)
{
    // Calculate the length in bytes needed to accommodate the bits
    uint8_t endBit = nStartBit + nBitLength;
    return (endBit + 7) / 8; // Round up to the nearest byte
}