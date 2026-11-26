#pragma once

// LIN protocol helpers with no HAL dependency, checked on the host by tests/lin_check.cpp

#include <cstdint>
#include "enums.h"

#define LIN_BAUD 19200U
#define LIN_DIAG_ID_MASTER 0x3C
#define LIN_DIAG_ID_SLAVE 0x3D

// 6-bit frame ID -> protected ID (parity in bits 6 and 7)
inline uint8_t LinPid(uint8_t nId)
{
    nId &= 0x3F;
    uint8_t p0 = (nId >> 0) ^ (nId >> 1) ^ (nId >> 2) ^ (nId >> 4);
    uint8_t p1 = ~((nId >> 1) ^ (nId >> 3) ^ (nId >> 4) ^ (nId >> 5));
    return nId | ((p0 & 1) << 6) | ((p1 & 1) << 7);
}

// Inverted carry-folded 8-bit sum. Enhanced includes the PID; diagnostic frames are always classic.
inline uint8_t LinCalcChecksum(uint8_t nPid, const uint8_t *pData, uint8_t nLen, LinChecksum eType)
{
    bool bDiag = (nPid & 0x3F) >= LIN_DIAG_ID_MASTER; // 0x3E/0x3F are reserved, treat the same
    uint16_t nSum = (eType == LinChecksum::Enhanced && !bDiag) ? nPid : 0;

    for (uint8_t i = 0; i < nLen; i++)
    {
        nSum += pData[i];
        if (nSum > 0xFF)
            nSum -= 0xFF; // Fold the carry back in
    }

    return ~nSum & 0xFF;
}

// TFrame_Max = 1.4 x (34 header bits + 10 x (N data + 1 checksum) bits), in microseconds, rounded up
inline uint32_t LinFrameMaxUs(uint8_t nLen)
{
    uint32_t nBitsX10 = 14U * (34U + 10U * (nLen + 1U));
    return (nBitsX10 * 1000000U + 10U * LIN_BAUD - 1U) / (10U * LIN_BAUD);
}

// Shortest slot time a frame of this length fits in, whole ms
inline uint16_t LinMinSlotMs(uint8_t nLen)
{
    return (LinFrameMaxUs(nLen) + 999U) / 1000U;
}
