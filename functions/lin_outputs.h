#pragma once

#include "port.h"
#include "config.h"
#include "hal.h"
#include "lin_output.h"

// Same value, used to show difference in intent between number of frames vs number of outputs
#define LIN_OUT_FRAMES NUM_LIN_OUTPUTS // Max possible frames is when all outputs have unique IDs

extern float *pVarMap[VAR_MAP_SIZE];

class LinOutputs
{
public:
    static const uint16_t nBaseIndex = 0x3000;

    static void SetConfig(Config_LinOutput config[]) {
        for(uint8_t i = 0; i < NUM_LIN_OUTPUTS; i++) {
            pConfigs[i] = &config[i];
            pInput[i] = pVarMap[config[i].nInput];
        }
    }

    static void InitAllFrames();
    static void Update();

private:
    static Config_LinOutput* pConfigs[NUM_LIN_OUTPUTS];

    static float *pInput[NUM_LIN_OUTPUTS];

    static LinOutput linOut[LIN_OUT_FRAMES];

    // Frame indexes are independent of LIN output indexes, multiple outputs may be packed into the same frame based on ID
    static int8_t nAssignedOut[NUM_LIN_OUTPUTS]; // Maps LIN output index to frame index, -1 if not assigned    

    static void ClearFrames();
    static uint8_t CalcLength(uint8_t nStartBit, uint8_t nBitLength);
};

// LIN Outputs are packed into frames based on ID, multiple outputs may share a frame if they have the same ID. 
// Each output configures which bits of the frame it uses for its value. 
// When an output is updated, it updates the relevant bits in its assigned frame, and the frame is transmitted at the configured interval.