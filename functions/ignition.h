#pragma once

#include <cstdint>
#include "hal.h"
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

  //Several devices sharing one ignition
  IgnitionRole eRole;
  uint16_t nSyncId;         //Standard ID the master broadcasts its state on

  //Where the key ON / start button comes from
  IgnitionSource eButtonSource;
  uint8_t nButtonIDE;
  uint32_t nButtonId;
  uint8_t nButtonByte;
  uint8_t nButtonMask;      //Button reads pressed when any of these bits is set
  uint16_t nButtonTimeout;  //ms without the frame before it reads released, 0 = never

  //Dash shutdown, applied to the outputs with the Dash role
  bool bShutdownEnabled;    //Send the shutdown frame, otherwise only the power off delay applies
  uint8_t nShutdownIDE;
  uint32_t nShutdownId;
  uint8_t nShutdownDLC;
  uint8_t nShutdownData[8];
  uint16_t nShutdownInterval; //ms between repeats of the shutdown frame
  uint32_t nGraceTime;        //ms after ignition off before the shutdown starts
  uint32_t nDashOffDelay;     //ms from the shutdown frame to cutting the dash power
  uint16_t nDoorInput;        //Optional, powers the dash without the ignition
  uint32_t nDoorOnTime;       //ms the dash stays on after the door input

  uint32_t nSleepDelay;       //ms from ignition off to sleep, 0 = ignition does not manage sleep

  IgnitionOutputRole eOutputRole[NUM_OUTPUTS];
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
        pDoorInput = pVarMap[config->nDoorInput];
    }

    // Button frame and, on a follower, the master's state broadcast
    void CheckMsg(const CANRxFrame &rx);
    void Update();

    // The variable an output with this role is switched from
    float *GetRoleVar(IgnitionOutputRole eRole);

    // All CAN transmit has to stop before sleep, see UpdateSleep()
    bool TxQuiet() const { return (ePhase == SleepPhase::Quiet) || (ePhase == SleepPhase::Sleep); }
    bool SleepRequest() const { return ePhase == SleepPhase::Sleep; }

    float fIgnition; //Ignition power is on, in every state except Off
    float fStarter;  //Starter motor should be engaged
    float fState;    //IgnitionState, for the var map and diagnostics
    float fAccessory;
    float fDash;

private:
    enum class SleepPhase : uint8_t
    {
        Awake,
        Announce, // Master only, tells the followers before going quiet
        Quiet,
        Sleep
    };

    void UpdateLocal(uint32_t nNow);
    void UpdateFollower(uint32_t nNow);
    void UpdateDoor(uint32_t nNow);
    void UpdateDash(uint32_t nNow, bool bWantOn);
    void UpdateSleep(uint32_t nNow, bool bWantOn);
    void SendSync(uint32_t nNow);
    void SendShutdown();
    bool ButtonIn(uint32_t nNow);
    bool MasterLinkOk(uint32_t nNow) const;

    Config_Ignition* pConfig = nullptr;

    float *pIgnInput;
    float *pStartInput;
    float *pEngineRunInput;
    float *pStopInput;
    float *pDoorInput;

    IgnitionState eState;

    bool bInit;
    bool bLastIgnIn;
    bool bCrankLockout;
    uint32_t nCrankStartTime;

    // Button frame
    bool bButtonFrame;
    uint32_t nButtonRxTime;

    // Follower
    bool bSyncSeen;
    uint32_t nSyncRxTime;
    IgnitionState eMasterState;
    bool bMasterSleep;

    // Master
    uint32_t nSyncTxTime;
    IgnitionState eLastSyncState;
    bool bLastSyncSleep;

    // Door
    bool bDoorInit;
    bool bLastDoor;
    bool bDoorActive;
    uint32_t nDoorTime;

    // Dash
    DashState eDash;
    uint32_t nDashTime;
    uint32_t nShutdownTxTime;

    // Sleep
    SleepPhase ePhase;
    IgnitionSleepStatus eSleepStatus;
    uint32_t nIdleSince;
    uint32_t nPhaseTime;
};
