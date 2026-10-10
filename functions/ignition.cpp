#include "ignition.h"

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
// Outputs are handed to the ignition by role instead of by input: Ignition
// outputs are on whenever the ignition is, Accessory outputs drop out while
// cranking, Starter outputs are on only while cranking and Dash outputs follow
// the ignition.
//=============================================================================

void Ignition::Update()
{
    if (!pConfig->bEnabled)
    {
        bInit = false;
        eState = IgnitionState::Off;
        fIgnition = 0.0f;
        fStarter = 0.0f;
        fAccessory = 0.0f;
        fDash = 0.0f;
        fState = static_cast<float>(IgnitionState::Off);
        return;
    }

    const bool bIgnIn = *pIgnInput > 0.5f;
    const bool bStartIn = *pStartInput > 0.5f;
    const bool bEngineRun = *pEngineRunInput > 0.5f;
    const bool bStopIn = *pStopInput > 0.5f;
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
        bLastIgnIn = bIgnIn;
        bCrankLockout = (pConfig->eMode == IgnitionMode::KeySwitch);
        eState = IgnitionState::Off;
        fIgnition = 0.0f;
        fStarter = 0.0f;
        fAccessory = 0.0f;
        fDash = 0.0f;
        fState = static_cast<float>(eState);
        return;
    }

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

    fIgnition = (eState != IgnitionState::Off) ? 1.0f : 0.0f;
    fStarter = (eState == IgnitionState::Cranking) ? 1.0f : 0.0f;
    fAccessory = ((eState == IgnitionState::Ignition) || (eState == IgnitionState::Running)) ? 1.0f : 0.0f;
    fDash = fIgnition;
    fState = static_cast<float>(eState);

    bLastIgnIn = bIgnIn;
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
