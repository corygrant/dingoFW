#pragma once

#include <cstdint>
#include "port.h"
#include "enums.h"

extern float *pVarMap[VAR_MAP_SIZE];

struct Config_Ignition{
  bool bEnabled;
  IgnitionMode eMode;
  uint16_t nIgnInput;       //Key ON position (KeySwitch) or start button (StartButton)
  uint16_t nStartInput;     //Key START position (KeySwitch) or start condition (StartButton)
  uint16_t nEngineRunInput; //Engine is running, normally a CAN input via a condition
  uint16_t nStopInput;      //Optional, forces everything off while held
  uint32_t nMaxCrankTime;   //ms, hard limit on how long the starter may be engaged
};

class Ignition
{
public:
    Ignition() {
    };

    static const uint16_t nBaseIndex = 0x1B00;

    void SetConfig(Config_Ignition* config)
    {
        pConfig = config;
        pIgnInput = pVarMap[config->nIgnInput];
        pStartInput = pVarMap[config->nStartInput];
        pEngineRunInput = pVarMap[config->nEngineRunInput];
        pStopInput = pVarMap[config->nStopInput];
    }

    void Update();

    float fIgnition; //Ignition power is on, in every state except Off
    float fStarter;  //Starter motor should be engaged
    float fState;    //IgnitionState, for the var map and diagnostics

private:
    Config_Ignition* pConfig;

    float *pIgnInput;
    float *pStartInput;
    float *pEngineRunInput;
    float *pStopInput;

    IgnitionState eState;

    bool bInit;
    bool bLastIgnIn;
    bool bCrankLockout;
    uint32_t nCrankStartTime;
};
