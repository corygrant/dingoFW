#pragma once

#include "port.h"
#include "param_registry.h"

struct ParamMsg
{
    MsgCmd eCmd;
    uint16_t nIndex;
    uint8_t nSubIndex;
    uint32_t nValue;
};

MsgCmd ProcessParamMsg(CANRxFrame *rx, uint16_t *nIndex);
void SetAllDefaultParams(bool temp = false);

// True while a ReadAll/WriteAll bulk transfer is in progress; cyclic TX should
// pause during this window to avoid contending with the transfer on a busy bus.
// Self-clears after PARAM_OP_MAX_DURATION_MS so an abandoned transfer can't
// permanently starve cyclic messages.
bool IsCyclicTxPaused();