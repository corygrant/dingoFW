#pragma once

#include "hal.h"

// Portable accessors hiding the difference between the bxCAN frame layout
// (flat .SID/.EID/.IDE/.RTR fields, implemented in comms/can_bxcan.cpp) and
// the FDCAN frame layout (.std.SID/.ext.EID/.common.{XTD,RTR,ESI} bitfields
// nested in unions, implemented in comms/can_fdcan.cpp).
//
// CanFrameGetStdId/CanFrameGetExtId read "the ID interpreted as standard/
// extended" regardless of the frame's own type flag, matching how the
// original bxCAN code read .SID/.EID directly (both fields alias the same
// bits, so either is always readable) - callers decide which interpretation
// they want based on their own expected ID type, not the frame's.
uint32_t CanFrameGetStdId(const CANRxFrame &frame);
uint32_t CanFrameGetExtId(const CANRxFrame &frame);
uint32_t CanFrameGetStdId(const CANTxFrame &frame);
uint32_t CanFrameGetExtId(const CANTxFrame &frame);

bool CanFrameIsExtended(const CANTxFrame &frame);

// Sets the frame's ID and its extended/standard flag together.
void CanFrameSetId(CANTxFrame &frame, uint32_t nId, bool bExtended);

// Zeroes the ID and clears the extended flag (matches the original
// "IDE = 0; EID = 0; // clears SID as well, union" bxCAN idiom).
void CanFrameClearId(CANTxFrame &frame);

// Sets up a classic (non-FD), data (non-remote) frame with defaulted/unused
// header bits cleared. Equivalent to the bxCAN
// "frame.IDE = CAN_IDE_STD; frame.RTR = CAN_RTR_DATA;" idiom.
void CanFrameSetStandardDefaults(CANTxFrame &frame);
