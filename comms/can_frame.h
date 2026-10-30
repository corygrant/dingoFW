#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include "hal.h"

// bxCAN (F3/F4) and FDCAN (G4) frames name the ID fields differently, these hide it.
// Templates so the same helper works on CANRxFrame and CANTxFrame.

#if defined(CAN_IDE_STD) // bxCAN

template <typename F> inline bool CanIsExt(const F &f) { return f.IDE == CAN_IDE_EXT; }
template <typename F> inline uint32_t CanId(const F &f) { return CanIsExt(f) ? f.EID : f.SID; }

template <typename F> inline void CanSetId(F &f, uint32_t nId, bool bExt = false)
{
    f.IDE = bExt ? CAN_IDE_EXT : CAN_IDE_STD;
    if (bExt)
        f.EID = nId;
    else
        f.SID = nId; // SID and EID are a union, only set one
}

// Header bits senders don't set, cleared before transmit
inline void CanClearTxFlags(CANTxFrame &f) { f.RTR = CAN_RTR_DATA; }

#else // FDCAN

template <typename F> inline bool CanIsExt(const F &f) { return f.common.XTD; }
template <typename F> inline uint32_t CanId(const F &f) { return CanIsExt(f) ? f.ext.EID : f.std.SID; }

template <typename F> inline void CanSetId(F &f, uint32_t nId, bool bExt = false)
{
    f.header32[0] = 0; // Also clears RTR and ESI
    if (bExt)
        f.ext.EID = nId;
    else
        f.std.SID = nId;
    f.common.XTD = bExt;
}

// Header bits senders don't set, cleared before transmit
// Left uninitialized FDF/BPS would send an FD frame, classic CAN only
inline void CanClearTxFlags(CANTxFrame &f)
{
    f.common.RTR = 0;
    f.common.ESI = 0;
    uint8_t nDlc = f.DLC;
    f.header32[1] = 0;
    f.DLC = nDlc;
}

#endif

// FDCAN frames have no data64
template <typename F> inline uint64_t CanData64(const F &f)
{
    uint64_t n;
    memcpy(&n, f.data8, sizeof(n));
    return n;
}

template <typename F> inline void CanSetData64(F &f, uint64_t n) { memcpy(f.data8, &n, sizeof(n)); }

// Buffered frame storage, classic CAN only: the frame's own header plus 8 data bytes, byte for byte.
// FDCAN frames reserve 64 data bytes (72 total), a slot is 16. bxCAN data is already the last 8 bytes, no change.
// Bytes past data8[7] are left as-is on load, the driver only reads DLC bytes.
template <typename F> struct CanSlot
{
    uint8_t raw[offsetof(F, data8) + 8];
};

template <typename F> inline void CanStore(CanSlot<F> &s, const F &f) { memcpy(s.raw, &f, sizeof(s.raw)); }
template <typename F> inline void CanLoad(F &f, const CanSlot<F> &s) { memcpy(&f, s.raw, sizeof(s.raw)); }
