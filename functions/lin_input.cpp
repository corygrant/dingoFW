#include "lin_input.h"
#include "dbc.h"

bool LinInput::CheckMsg(const stLinFrame &frame, uint8_t nBus)
{
    if (!pConfig->bEnabled)
        return false;
    if (LinInputBus(*pConfig) != nBus)
        return false;
    if (pConfig->nId != frame.nId)
        return false;
    if (pConfig->nBitLength == 0)
        return false;

    nLastRxTime = SYS_TIME;

    fVal = Dbc::DecodeFloat(frame.nData, pConfig->nStartBit, pConfig->nBitLength,
                            pConfig->fFactor, pConfig->fOffset,
                            pConfig->eByteOrder, pConfig->bSigned);

    // Use Input to enable momentary/latching
    switch (pConfig->eOperator)
    {
    case Operator::Equal:
        fOutput = input.Check(pConfig->eMode, false, fVal == pConfig->fOperand);
        break;

    case Operator::NotEqual:
        fOutput = input.Check(pConfig->eMode, false, fVal != pConfig->fOperand);
        break;

    case Operator::GreaterThan:
        fOutput = input.Check(pConfig->eMode, false, fVal > pConfig->fOperand);
        break;

    case Operator::LessThan:
        fOutput = input.Check(pConfig->eMode, false, fVal < pConfig->fOperand);
        break;

    case Operator::GreaterThanOrEqual:
        fOutput = input.Check(pConfig->eMode, false, fVal >= pConfig->fOperand);
        break;

    case Operator::LessThanOrEqual:
        fOutput = input.Check(pConfig->eMode, false, fVal <= pConfig->fOperand);
        break;

    case Operator::BitwiseAnd:
        fOutput = input.Check(pConfig->eMode, false, ((uint32_t)fVal & (uint32_t)pConfig->fOperand) > 0);
        break;

    case Operator::BitwiseNand:
        fOutput = input.Check(pConfig->eMode, false, !(((uint32_t)fVal & (uint32_t)pConfig->fOperand) > 0));
        break;
    }

    return true;
}

void LinInput::CheckTimeout()
{
    if (!pConfig->bTimeoutEnabled)
        return;

    if ((SYS_TIME - nLastRxTime) > pConfig->nTimeout)
    {
        fVal = 0;
        fOutput = 0;
    }
}