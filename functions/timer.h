#pragma once

#include <cstdint>
#include "port.h"
#include "enums.h"

extern float *pVarMap[VAR_MAP_SIZE];

struct Config_Timer{
  bool bEnabled;
  uint16_t nInput;
  uint16_t nResetInput;
  TimerMode eMode;
  uint32_t nTime; //ms
};

class Timer
{
public:
    Timer() {
    };

    static const uint16_t nBaseIndex = 0x1A00;

    void SetConfig(Config_Timer* config)
    {
        pConfig = config;
        pInput = pVarMap[config->nInput];
        pResetInput = pVarMap[config->nResetInput];
    }

    void Update();

    float fVal;

private:
    Config_Timer* pConfig;

    float *pInput;
    float *pResetInput;

    bool bInit;
    bool bLastIn;
    bool bRunning;
    bool bOut;
    uint32_t nStartTime;
};
