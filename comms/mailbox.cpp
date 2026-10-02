#include "mailbox.h"
#include "ch.hpp"
#include "port.h"

static chibios_rt::Mailbox<CANRxFrame*, MAILBOX_SIZE> rxMb;
static chibios_rt::Mailbox<CANTxFrame*, MAILBOX_SIZE> txMb;
static chibios_rt::Mailbox<CANTxFrame*, MAILBOX_SIZE> txUsbMb;

//Mailbox buffer of CAN frames
//Not managed by mailbox
CANRxFrame rxFrames[MAILBOX_SIZE];
CANTxFrame txFrames[MAILBOX_SIZE];
CANTxFrame txUsbFrames[MAILBOX_SIZE];

//Used to manage the memory used by the mailbox
bool rxMsgUsed[MAILBOX_SIZE];
bool txMsgUsed[MAILBOX_SIZE];
bool txUsbMsgUsed[MAILBOX_SIZE];

// Mutexes protecting each *MsgUsed array from concurrent access between
// Post* (CanRxThread/main thread) and Fetch* (CanTxThread/main thread).
// Priority inheritance ensures no priority inversion across thread priorities.
static chibios_rt::Mutex rxMutex;
static chibios_rt::Mutex txMutex;
static chibios_rt::Mutex txUsbMutex;

msg_t PostTxFrame(CANTxFrame *frame)
{
    txMutex.lock();
    for (int i = 0; i < MAILBOX_SIZE; i++) {
        if (!txMsgUsed[i]) {
            txFrames[i] = *frame;
            txMsgUsed[i] = true;

            msg_t result = txMb.post(&txFrames[i], TIME_IMMEDIATE);
            if (result != MSG_OK) {
                txMsgUsed[i] = false;
                txMutex.unlock();
                return result;
            }
            txMutex.unlock();
            PostTxUsbFrame(frame);  // Only post to USB after successful CAN TX post
            return result;
        }
    }

    txMutex.unlock();
    return MSG_TIMEOUT;  // No free slots
}

msg_t FetchTxFrame(CANTxFrame *frame, sysinterval_t timeout)
{
    CANTxFrame *txFrame;
    msg_t result = txMb.fetch(&txFrame, timeout);
    if (result == MSG_OK) {
        *frame = *txFrame; // Copy before freeing the slot so a producer can't overwrite it
        txMutex.lock();
        txMsgUsed[txFrame - txFrames] = false;
        txMutex.unlock();
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

msg_t FetchTxUsbFrame(CANTxFrame *frame, sysinterval_t timeout)
{
    CANTxFrame *txFrame;
    msg_t result = txUsbMb.fetch(&txFrame, timeout);
    if (result == MSG_OK) {
        *frame = *txFrame; // Copy before freeing the slot so a producer can't overwrite it
        txUsbMutex.lock();
        txUsbMsgUsed[txFrame - txUsbFrames] = false;
        txUsbMutex.unlock();
    }
    return result;
}

msg_t PostRxFrame(CANRxFrame *frame)
{
    rxMutex.lock();
    for (int i = 0; i < MAILBOX_SIZE; i++) {
        if (!rxMsgUsed[i]) {
            rxFrames[i] = *frame;
            rxMsgUsed[i] = true;

            msg_t result = rxMb.post(&rxFrames[i], TIME_IMMEDIATE);
            if (result != MSG_OK)
                rxMsgUsed[i] = false;
            rxMutex.unlock();
            return result;
        }
    }

    rxMutex.unlock();
    return MSG_TIMEOUT;  // No free slots
}   

msg_t FetchRxFrame(CANRxFrame *frame)
{
    CANRxFrame *rxFrame;
    msg_t result = rxMb.fetch(&rxFrame, TIME_IMMEDIATE);
    if (result == MSG_OK) {
        *frame = *rxFrame; // Copy before freeing the slot so a producer can't overwrite it
        rxMutex.lock();
        rxMsgUsed[rxFrame - rxFrames] = false;
        rxMutex.unlock();
    }
    return result;
}

bool RxFramesEmpty()
{
    return (rxMb.getUsedCountI() == 0);
}
