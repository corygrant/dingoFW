#include "ignition.h"
#include "mailbox.h"

//=============================================================================
// Ignition and starter control
//
// KeySwitch:   the key's ON position is a maintained input and powers the
//              ignition directly. Its START position is a separate momentary
//              input, because the key springs back out of it.
//
// StartButton: one press of the button toggles the ignition. Pressing it while
//              the start condition input is held also cranks; pressing it on
//              its own only powers the ignition.
//
// The starter is released as soon as the engine-running input goes true, and in
// any case after nMaxCrankTime. After that limit the start request has to be
// released before the starter can be engaged again, so a stuck key or a held
// button cannot crank continuously.
//
// The key ON position or the start button is either a variable or a bit in a
// CAN frame, read here directly so a button module needs no CAN input.
//
// Outputs are handed to the ignition by role instead of by input, and the
// ignition runs the dash power down on its own:
//
//   ignition off -> Ignition and Accessory outputs off
//                -> grace time, in case the ignition comes straight back on
//                -> dash shutdown frame, repeated until the dash power is cut
//
// A door switch powers the dash for a while with the ignition off.
//=============================================================================

static constexpr uint32_t RESTART_OFF_TIME = 2000; // ms a halted dash stays unpowered before it may come back on

void Ignition::CheckMsg(const CANRxFrame &rx)
{
    if ((pConfig == nullptr) || !pConfig->bEnabled)
        return;

    if (pConfig->eButtonSource == IgnitionSource::CanFrame)
    {
        const bool bMatch = (pConfig->nButtonIDE == 1)
                                ? ((rx.IDE == CAN_IDE_EXT) && (rx.EID == pConfig->nButtonId))
                                : ((rx.IDE == CAN_IDE_STD) && (rx.SID == pConfig->nButtonId));

        if (bMatch && (rx.DLC > pConfig->nButtonByte))
        {
            bButtonFrame = (rx.data8[pConfig->nButtonByte] & pConfig->nButtonMask) != 0;
            nButtonRxTime = SYS_TIME;
        }
    }
}

void Ignition::Update()
{
    if (!pConfig->bEnabled)
    {
        bInit = false;
        bDoorInit = false;
        bButtonFrame = false;
        eState = IgnitionState::Off;
        eDash = DashState::Off;
        fIgnition = 0.0f;
        fStarter = 0.0f;
        fAccessory = 0.0f;
        fDash = 0.0f;
        fState = static_cast<float>(IgnitionState::Off);
        return;
    }

    const uint32_t nNow = SYS_TIME;

    // First pass only samples the inputs. A button already held at power up
    // would otherwise look like a press, and the crank lockout makes sure a key
    // left in the START position has to be released before it can crank. A
    // start button needs a fresh press to crank anyway, and its start condition
    // is often what woke the device (the clutch pressed on getting in), so it
    // starts unlocked.
    if (!bInit)
    {
        bInit = true;
        bLastIgnIn = ButtonIn(nNow);
        bCrankLockout = (pConfig->eMode == IgnitionMode::KeySwitch);
        eState = IgnitionState::Off;
        eDash = DashState::Off;
    }

    UpdateLocal(nNow);
    UpdateDoor(nNow);

    const bool bWantOn = (eState != IgnitionState::Off) || bDoorActive;

    UpdateDash(nNow, bWantOn);

    fIgnition = (eState != IgnitionState::Off) ? 1.0f : 0.0f;
    fStarter = (eState == IgnitionState::Cranking) ? 1.0f : 0.0f;
    fAccessory = ((eState == IgnitionState::Ignition) || (eState == IgnitionState::Running)) ? 1.0f : 0.0f;
    fDash = ((eDash == DashState::On) || (eDash == DashState::Grace) || (eDash == DashState::Halting)) ? 1.0f : 0.0f;
    fState = static_cast<float>(eState);
}

bool Ignition::ButtonIn(uint32_t nNow)
{
    if (pConfig->eButtonSource == IgnitionSource::CanFrame)
    {
        if (!bButtonFrame)
            return false;

        // A sender that stops transmitting must not leave the button pressed
        return (pConfig->nButtonTimeout == 0) || ((nNow - nButtonRxTime) < pConfig->nButtonTimeout);
    }

    return *pIgnInput > 0.5f;
}

void Ignition::UpdateLocal(uint32_t nNow)
{
    const bool bIgnIn = ButtonIn(nNow);
    const bool bStartIn = *pStartInput > 0.5f;
    const bool bEngineRun = *pEngineRunInput > 0.5f;
    const bool bStopIn = *pStopInput > 0.5f;

    const bool bIgnRising = bIgnIn && !bLastIgnIn;
    const bool bKey = pConfig->eMode == IgnitionMode::KeySwitch;
    const bool bCrankAllowed = !bEngineRun && !bCrankLockout;

    if (bStopIn)
    {
        eState = IgnitionState::Off;
    }
    else
    {
        switch (eState)
        {
        case IgnitionState::Off:
            if (bKey)
            {
                if (bIgnIn)
                    eState = IgnitionState::Ignition;
            }
            else if (bIgnRising)
            {
                if (bStartIn && bCrankAllowed)
                {
                    eState = IgnitionState::Cranking;
                    nCrankStartTime = nNow;
                }
                else
                {
                    eState = IgnitionState::Ignition;
                }
            }
            break;

        case IgnitionState::Ignition:
            if (bKey)
            {
                if (!bIgnIn)
                    eState = IgnitionState::Off;
                else if (bStartIn && bCrankAllowed)
                {
                    eState = IgnitionState::Cranking;
                    nCrankStartTime = nNow;
                }
                else if (bEngineRun)
                    eState = IgnitionState::Running;
            }
            else
            {
                if (bIgnRising)
                {
                    if (bStartIn && bCrankAllowed)
                    {
                        eState = IgnitionState::Cranking;
                        nCrankStartTime = nNow;
                    }
                    else
                    {
                        eState = IgnitionState::Off;
                    }
                }
                else if (bEngineRun)
                    eState = IgnitionState::Running;
            }
            break;

        case IgnitionState::Cranking:
            if (bEngineRun)
            {
                eState = IgnitionState::Running;
            }
            else if ((nNow - nCrankStartTime) >= pConfig->nMaxCrankTime)
            {
                // Out of time. Fall back to ignition only. A key held in START
                // must be released before it can crank again; a start button
                // already needs a new press.
                eState = IgnitionState::Ignition;
                bCrankLockout = bKey;
            }
            else if (bKey)
            {
                if (!bIgnIn)
                    eState = IgnitionState::Off;
                else if (!bStartIn)
                    eState = IgnitionState::Ignition;
            }
            else if (bIgnRising)
            {
                // Pressing the button again during cranking aborts it
                eState = IgnitionState::Off;
            }
            break;

        case IgnitionState::Running:
            if (bKey)
            {
                if (!bIgnIn)
                    eState = IgnitionState::Off;
            }
            else if (bIgnRising)
            {
                eState = IgnitionState::Off;
            }

            // Engine stopped on its own, drop back so it can be cranked again
            if ((eState == IgnitionState::Running) && !bEngineRun)
                eState = IgnitionState::Ignition;
            break;
        }
    }

    // The lockout is what makes a held start request a single crank attempt.
    // Releasing the request arms the next one.
    if (!bStartIn)
        bCrankLockout = false;

    bLastIgnIn = bIgnIn;
}

void Ignition::UpdateDoor(uint32_t nNow)
{
    if (pConfig->nDoorInput == 0)
    {
        bDoorActive = false;
        return;
    }

    const bool bDoor = *pDoorInput > 0.5f;

    // A door already open on the first pass counts: opening it is usually
    // what woke the device up
    const bool bRising = bDoor && (!bLastDoor || !bDoorInit);
    bDoorInit = true;
    bLastDoor = bDoor;

    if (bRising)
    {
        bDoorActive = true;
        nDoorTime = nNow;
    }

    // Timed rather than following the door, a door left open must not keep
    // the dash running
    if (bDoorActive && ((nNow - nDoorTime) >= pConfig->nDoorOnTime))
        bDoorActive = false;
}

void Ignition::UpdateDash(uint32_t nNow, bool bWantOn)
{
    switch (eDash)
    {
    case DashState::Off:
        if (bWantOn)
            eDash = DashState::On;
        break;

    case DashState::On:
        if (!bWantOn)
        {
            eDash = DashState::Grace;
            nDashTime = nNow;
        }
        break;

    case DashState::Grace:
        if (bWantOn)
        {
            eDash = DashState::On;
        }
        else if ((nNow - nDashTime) >= pConfig->nGraceTime)
        {
            eDash = DashState::Halting;
            nDashTime = nNow;

            if (pConfig->bShutdownEnabled)
            {
                SendShutdown();
                nShutdownTxTime = nNow;
            }
        }
        break;

    case DashState::Halting:
        if (pConfig->bShutdownEnabled)
        {
            // Once told to shut down the dash is left to finish, even if the
            // ignition comes back on. Cutting its power part way through is
            // exactly the corruption this sequence is here to avoid. It is
            // powered up again from Restart.
            if ((pConfig->nShutdownInterval > 0) &&
                ((nNow - nShutdownTxTime) >= pConfig->nShutdownInterval))
            {
                SendShutdown();
                nShutdownTxTime = nNow;
            }

            if ((nNow - nDashTime) >= pConfig->nDashOffDelay)
            {
                eDash = DashState::Restart;
                nDashTime = nNow;
            }
        }
        else
        {
            // Nothing was sent, so this is a plain off delay
            if (bWantOn)
                eDash = DashState::On;
            else if ((nNow - nDashTime) >= pConfig->nDashOffDelay)
                eDash = DashState::Off;
        }
        break;

    case DashState::Restart:
        // A halted dash only boots again after a real power cycle
        if ((nNow - nDashTime) >= RESTART_OFF_TIME)
            eDash = DashState::Off;
        break;
    }
}

void Ignition::SendShutdown()
{
    CANTxFrame frame;
    frame.IDE = (pConfig->nShutdownIDE == 1) ? CAN_IDE_EXT : CAN_IDE_STD;
    frame.RTR = CAN_RTR_DATA;

    if (pConfig->nShutdownIDE == 1)
        frame.EID = pConfig->nShutdownId;
    else
        frame.SID = pConfig->nShutdownId;

    frame.DLC = (pConfig->nShutdownDLC > 8) ? 8 : pConfig->nShutdownDLC;

    for (uint8_t i = 0; i < 8; i++)
        frame.data8[i] = pConfig->nShutdownData[i];

    PostTxFrame(&frame);
}

float *Ignition::GetRoleVar(IgnitionOutputRole eRole)
{
    switch (eRole)
    {
    case IgnitionOutputRole::Ignition:
        return &fIgnition;
    case IgnitionOutputRole::Accessory:
        return &fAccessory;
    case IgnitionOutputRole::Dash:
        return &fDash;
    case IgnitionOutputRole::Starter:
        return &fStarter;
    default:
        return nullptr;
    }
}
