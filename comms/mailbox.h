#pragma once

#include <cstdint>
#include "hal.h"

msg_t PostTxFrame(CANTxFrame *frame, uint8_t nBus = 0);
msg_t PostTxUsbFrame(CANTxFrame *frame);
msg_t FetchTxFrame(CANTxFrame *frame, uint8_t nBus = 0);
msg_t FetchTxUsbFrame(CANTxFrame *frame);
msg_t PostRxFrame(CANRxFrame *frame, uint8_t nBus = 0);
msg_t FetchRxFrame(CANRxFrame *frame, uint8_t nBus = 0);
bool RxFramesEmpty(uint8_t nBus = 0);
