#pragma once

#include "hal.h"
#include "enums.h"
#include "port.h"
#include "lin_frame.h"

struct Config_LinOutput{
    bool bEnabled;
    uint16_t nInput;  // Var map index
    uint8_t nFrame;   // Index into the LIN frame table
    uint8_t nStartBit;
    uint8_t nBitLength;
    float fFactor;
    float fOffset;
    ByteOrder eByteOrder;
    bool bSigned;
};

class LinOutput
{
public:
    bool CheckTxTime()
    {
        if (SYS_TIME - nLastTxTime >= nInterval)
        {
            nLastTxTime = SYS_TIME;
            return true;
        }

        return false;
    }

    stLinFrame stFrame;
    uint16_t nInterval; //ms
    uint32_t nLastTxTime;
    uint8_t nBus;
};