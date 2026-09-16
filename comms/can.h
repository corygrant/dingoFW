#pragma once

#include <cstdint>
#include "enums.h"
#include "config.h"
#include "port.h"

msg_t InitCan(Config_Device *conf);
void StopCan();
void ClearCanFilters(uint8_t nBus = 0);
void SetCanFilterId(uint8_t nFilterNum, uint32_t nId, bool bExtended, uint8_t nBus = 0);
void SetCanFilterEnabled(bool bEnabled, uint8_t nBus = 0);
uint32_t GetLastCanRxTime();
