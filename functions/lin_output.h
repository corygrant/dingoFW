#pragma once

#include "hal.h"
#include "port.h"

struct Config_LinOutput{
    bool bEnabled;
    uint16_t nInput;
    uint8_t nId;
    uint8_t nStartBit;
    uint8_t nBitLength;
    float fFactor;
    float fOffset;
    ByteOrder eByteOrder;
    bool bSigned;
    uint16_t nInterval; //ms
    uint8_t nBus; //Only needed with >1 bus
};

inline uint8_t LinOutputBus(const Config_LinOutput &conf)
{
  #if NUM_LIN_BUSES > 1
  return conf.nBus;
  #else
  (void)conf;
  return 0;
  #endif
}

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