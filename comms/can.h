#pragma once

#include <cstdint>
#include "enums.h"
#include "config.h"

msg_t InitCan(Config_Device *conf);
void StopCan();
// Rebuild hardware filters from live config (config ID, CAN inputs, keypads), safe while running
void UpdateCanFilters();
uint32_t GetLastCanRxTime();
// Stop all transmit except replies to dingoConfig, used before sleep
void SetCanTxQuiet(bool bQuiet);