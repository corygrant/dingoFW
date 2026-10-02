#pragma once

#include <cstdint>
#include "config.h"

void ApplyAllConfig();
void ApplyConfig(uint16_t nIndex);

// Held by DeviceThread for each cycle and by the param thread while changing live config
void LockConfig();
void UnlockConfig();