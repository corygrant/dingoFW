#pragma once

#include <cstdint>
#include "enums.h"

typedef struct {
    uint8_t nId;
    uint8_t nData[8];
    uint8_t nLength;
    uint8_t nChecksum;
} stLinFrame;

// One entry in the LIN schedule. Signals (LIN inputs/outputs) reference a frame by index.
struct Config_LinFrame {
    bool bEnabled;
    uint8_t nId;             // 6-bit frame ID, 0-63 (not the PID)
    LinDirection eDirection;
    uint8_t nLength;         // 1-8 bytes
    LinChecksum eChecksum;
    uint16_t nSlotTime;      // ms allotted to this frame in the schedule
    uint8_t nBus;            // Only used when NUM_LIN_BUSES > 1
};
