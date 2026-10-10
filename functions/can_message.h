#pragma once

#include <cstdint>
#include "port.h"
#include "hal.h"
#include "enums.h"

extern float *pVarMap[VAR_MAP_SIZE];

struct Config_CanMessage{
  bool bEnabled;
  uint16_t nInput;    //Frame is sent while this is true
  uint8_t nIDE;       //0=STD, 1=EXT
  uint32_t nID;
  uint8_t nDLC;       //0-8
  uint8_t nData[8];   //Fixed payload
  uint16_t nInterval; //ms between repeats while the input is true, 0 = once per rising edge
};

class CanMessage
{
public:
    CanMessage() {
    };

    static const uint16_t nBaseIndex = 0x1C00;

    void SetConfig(Config_CanMessage* config)
    {
        pConfig = config;
        pInput = pVarMap[config->nInput];
    }

    void Update();

    float fVal; //1 while the frame is being sent

private:
    Config_CanMessage* pConfig;

    float *pInput;

    bool bInit;
    bool bLast;
    uint32_t nLastTxTime;
};
