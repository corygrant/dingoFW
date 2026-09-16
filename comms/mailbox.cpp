#include "mailbox.h"
#include "ch.hpp"
#include "port.h"

// One RX/TX mailbox set per physical CAN bus. The USB mailbox stays a single
// shared instance - USB<->CAN passthrough is bus-0-only (see
// comms/can_bxcan.cpp / comms/can_fdcan.cpp).
struct CanBusMailbox
{
    chibios_rt::Mailbox<CANRxFrame*, MAILBOX_SIZE> rxMb;
    chibios_rt::Mailbox<CANTxFrame*, MAILBOX_SIZE> txMb;

    CANRxFrame rxFrames[MAILBOX_SIZE];
    CANTxFrame txFrames[MAILBOX_SIZE];

    bool rxMsgUsed[MAILBOX_SIZE] = {};
    bool txMsgUsed[MAILBOX_SIZE] = {};

    chibios_rt::Mutex rxMutex;
    chibios_rt::Mutex txMutex;
};
static CanBusMailbox canBus[NUM_CAN_BUSES];

static chibios_rt::Mailbox<CANTxFrame*, MAILBOX_SIZE> txUsbMb;
CANTxFrame txUsbFrames[MAILBOX_SIZE];
bool txUsbMsgUsed[MAILBOX_SIZE];
static chibios_rt::Mutex txUsbMutex;

msg_t PostTxFrame(CANTxFrame *frame, uint8_t nBus)
{
    CanBusMailbox &bus = canBus[nBus];

    bus.txMutex.lock();
    for (int i = 0; i < MAILBOX_SIZE; i++) {
        if (!bus.txMsgUsed[i]) {
            bus.txFrames[i] = *frame;
            bus.txMsgUsed[i] = true;

            msg_t result = bus.txMb.post(&bus.txFrames[i], TIME_IMMEDIATE);
            if (result != MSG_OK) {
                bus.txMsgUsed[i] = false;
                bus.txMutex.unlock();
                return result;
            }
            bus.txMutex.unlock();
            if (nBus == 0)
                PostTxUsbFrame(frame);  // Only mirror bus 0's TX to USB
            return result;
        }
    }

    bus.txMutex.unlock();
    return MSG_TIMEOUT;  // No free slots
}

msg_t FetchTxFrame(CANTxFrame *frame, uint8_t nBus)
{
    CanBusMailbox &bus = canBus[nBus];

    CANTxFrame *txFrame;
    msg_t result = bus.txMb.fetch(&txFrame, TIME_IMMEDIATE);
    if (result == MSG_OK) {
        bus.txMutex.lock();
        for (int i = 0; i < MAILBOX_SIZE; i++) {
            if (txFrame == &bus.txFrames[i]) {
                bus.txMsgUsed[i] = false;
                break;
            }
        }
        bus.txMutex.unlock();
        *frame = *txFrame;
    }
    return result;
}

msg_t PostTxUsbFrame(CANTxFrame *frame)
{
    txUsbMutex.lock();
    for (int i = 0; i < MAILBOX_SIZE; i++) {
        if (!txUsbMsgUsed[i]) {
            txUsbFrames[i] = *frame;
            txUsbMsgUsed[i] = true;

            msg_t result = txUsbMb.post(&txUsbFrames[i], TIME_IMMEDIATE);
            if (result != MSG_OK)
                txUsbMsgUsed[i] = false;
            txUsbMutex.unlock();
            return result;
        }
    }

    txUsbMutex.unlock();
    return MSG_TIMEOUT;  // No free slots
}

msg_t FetchTxUsbFrame(CANTxFrame *frame)
{
    CANTxFrame *txFrame;
    msg_t result = txUsbMb.fetch(&txFrame, TIME_IMMEDIATE);
    if (result == MSG_OK) {
        txUsbMutex.lock();
        for (int i = 0; i < MAILBOX_SIZE; i++) {
            if (txFrame == &txUsbFrames[i]) {
                txUsbMsgUsed[i] = false;
                break;
            }
        }
        txUsbMutex.unlock();
        *frame = *txFrame;
    }
    return result;
}

msg_t PostRxFrame(CANRxFrame *frame, uint8_t nBus)
{
    CanBusMailbox &bus = canBus[nBus];

    bus.rxMutex.lock();
    for (int i = 0; i < MAILBOX_SIZE; i++) {
        if (!bus.rxMsgUsed[i]) {
            bus.rxFrames[i] = *frame;
            bus.rxMsgUsed[i] = true;

            msg_t result = bus.rxMb.post(&bus.rxFrames[i], TIME_IMMEDIATE);
            if (result != MSG_OK)
                bus.rxMsgUsed[i] = false;
            bus.rxMutex.unlock();
            return result;
        }
    }

    bus.rxMutex.unlock();
    return MSG_TIMEOUT;  // No free slots
}

msg_t FetchRxFrame(CANRxFrame *frame, uint8_t nBus)
{
    CanBusMailbox &bus = canBus[nBus];

    CANRxFrame *rxFrame;
    msg_t result = bus.rxMb.fetch(&rxFrame, TIME_IMMEDIATE);
    if (result == MSG_OK) {
        bus.rxMutex.lock();
        for (int i = 0; i < MAILBOX_SIZE; i++) {
            if (rxFrame == &bus.rxFrames[i]) {
                bus.rxMsgUsed[i] = false;
                break;
            }
        }
        bus.rxMutex.unlock();
        *frame = *rxFrame;
    }
    return result;
}

bool RxFramesEmpty(uint8_t nBus)
{
    return (canBus[nBus].rxMb.getUsedCountI() == 0);
}
