#pragma once

#include "port.h"
#include "hal.h"
#include "lin_frame.h"

struct Config_LinInput{
    bool bEnabled;
    bool bTimeoutEnabled;
    uint16_t nTimeout; //ms
    uint8_t nId;
    uint8_t nLength;
    uint8_t nStartBit;   
    uint8_t nBitLength;  
    float fFactor;       
    float fOffset;       
    ByteOrder eByteOrder;
    bool bSigned;        
    Operator eOperator;
    float fOperand;       
    InputMode eMode;
    uint8_t nBus;
};

inline uint8_t LinInputBus(const Config_LinInput &conf)
{
  #if NUM_LIN_BUSES > 1
  return conf.nBus;
  #else
  (void)conf;
  return 0;
  #endif
}

class LinInput
{
public:
    LinInput() {
    };

    static const uint16_t nBaseIndex = 0x1400;

    bool CheckMsg(const stLinFrame &frame, uint8_t nBus);

    void SetConfig(Config_LinInput* config) { pConfig = config; }
    void CheckTimeout();

    float fOutput;
    float fVal;

private:
    Config_LinInput* pConfig;
    Input input;
    uint32_t nLastRxTime;
}