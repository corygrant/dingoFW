#include "can_outputs.h"
#include "mailbox.h"
#include "dbc.h"
#include "can_frame.h"

Config_CanOutput* CanOutputs::pConfigs[NUM_CAN_OUTPUTS];
float* CanOutputs::pInput[NUM_CAN_OUTPUTS];
CanOutput CanOutputs::canOut[CAN_OUT_FRAMES];
int8_t CanOutputs::nAssignedOut[NUM_CAN_OUTPUTS];

void CanOutputs::ClearFrames()
{
    for (int i = 0; i < CAN_OUT_FRAMES; ++i)
    {
        canOut[i].stFrame.data32[0] = 0;
        canOut[i].stFrame.data32[1] = 0;
        canOut[i].stFrame.DLC = 0;
        CanFrameClearId(canOut[i].stFrame);
        canOut[i].nBus = 0;
    }

    for (int i = 0; i < NUM_CAN_OUTPUTS; ++i)
    {
        nAssignedOut[i] = -1;
    }
}

void CanOutputs::InitAllFrames()
{
    ClearFrames();

    for (int i = 0; i < NUM_CAN_OUTPUTS; ++i)
    {
        if (!pConfigs[i]->bEnabled) continue;

        uint8_t  nNewIDE      = pConfigs[i]->nIDE;
        uint16_t nNewID       = pConfigs[i]->nID;
        uint8_t  nNewStartBit = pConfigs[i]->nStartBit;
        uint8_t  nNewBitLen   = pConfigs[i]->nBitLength;
        uint16_t nNewInterval = pConfigs[i]->nInterval;

        if(nNewBitLen == 0 || nNewBitLen > 64) continue; // Invalid config, skip
        if(nNewInterval == 0) nNewInterval = 100; // Default to 100ms

        // Look for an existing frame with a matching ID
        for (int j = 0; j < CAN_OUT_FRAMES; ++j)
        {
            if (canOut[j].stFrame.DLC == 0) continue; // Unused frame slot

            bool bMatch = (nNewIDE == 0) ? (!CanFrameIsExtended(canOut[j].stFrame) && CanFrameGetStdId(canOut[j].stFrame) == nNewID)
                                      : (CanFrameIsExtended(canOut[j].stFrame) && CanFrameGetExtId(canOut[j].stFrame) == nNewID);
            bMatch = bMatch && (canOut[j].nBus == pConfigs[i]->nBus);
            if (bMatch)
            {
                nAssignedOut[i] = j;

                //Expand DLC to fit longest output assigned to the frame
                uint8_t newDlc = CalcDlc(nNewStartBit, nNewBitLen);
                if (canOut[j].stFrame.DLC < newDlc)
                    canOut[j].stFrame.DLC = newDlc;

                //Use shortest interval of any output assigned to the frame
                if (canOut[j].nInterval > nNewInterval)
                    canOut[j].nInterval = nNewInterval;

                break;
            }
        }

        if (nAssignedOut[i] != -1) continue;

        // No existing frame found, assign to first available slot
        for (int j = 0; j < CAN_OUT_FRAMES; ++j)
        {
            if (canOut[j].stFrame.DLC == 0)
            {
                nAssignedOut[i] = j;
                CanFrameSetId(canOut[j].stFrame, nNewID, nNewIDE != 0);
                canOut[j].nBus = pConfigs[i]->nBus;

                canOut[j].stFrame.DLC = CalcDlc(nNewStartBit, nNewBitLen);
                canOut[j].nInterval = nNewInterval;
                break;
            }
        }
    }
}

void CanOutputs::Update()
{
    for (int i = 0; i < CAN_OUT_FRAMES; ++i)
    {
        if (canOut[i].stFrame.DLC == 0) continue; // Skip unused frames

        if(canOut[i].CheckTxTime())
        {
            // Update data from all assigned outputs before sending
            for (int j = 0; j < NUM_CAN_OUTPUTS; ++j)
            {
                if (nAssignedOut[j] == i)
                {
                    Dbc::EncodeFloat( canOut[i].stFrame.data8, static_cast<float>(*pInput[j]), pConfigs[j]->nStartBit, pConfigs[j]->nBitLength,
                                    pConfigs[j]->fFactor, pConfigs[j]->fOffset, pConfigs[j]->eByteOrder);
                }
            }

            PostTxFrame(&canOut[i].stFrame, canOut[i].nBus);
        }
    }
}

uint8_t CanOutputs::CalcDlc(uint8_t nStartBit, uint8_t nBitLength)
{
    uint8_t nLastBit  = nStartBit + nBitLength - 1;
    uint8_t nEndByte  = nLastBit / 8;

    return (nEndByte + 1);
}
