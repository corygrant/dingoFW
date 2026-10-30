#include "mailbox.h"
#include "ch.hpp"
#include "port.h"
#include "can_frame.h"

static chibios_rt::Mailbox<CanSlot<CANRxFrame>*, MAILBOX_SIZE> rxMb;
static chibios_rt::Mailbox<CanSlot<CANTxFrame>*, MAILBOX_SIZE> txMb[NUM_CAN_BUSES]; // One per bus, a stalled bus can't hold up the other
static chibios_rt::Mailbox<CanSlot<CANTxFrame>*, MAILBOX_SIZE> txUsbMb;

//Mailbox buffer of CAN frames
//Not managed by mailbox
//Slots hold header + 8 data bytes, see CanSlot
CanSlot<CANRxFrame> rxFrames[MAILBOX_SIZE];
CanSlot<CANTxFrame> txFrames[NUM_CAN_BUSES][MAILBOX_SIZE];
CanSlot<CANTxFrame> txUsbFrames[MAILBOX_SIZE];
uint8_t rxBus[MAILBOX_SIZE]; // Bus each rx frame arrived on

//Used to manage the memory used by the mailbox
bool rxMsgUsed[MAILBOX_SIZE];
bool txMsgUsed[NUM_CAN_BUSES][MAILBOX_SIZE];
bool txUsbMsgUsed[MAILBOX_SIZE];

// Mutexes protecting each *MsgUsed array from concurrent access between
// Post* (CanRxThread/main thread) and Fetch* (CanTxThread/main thread).
// Priority inheritance ensures no priority inversion across thread priorities.
static chibios_rt::Mutex rxMutex;
static chibios_rt::Mutex txMutex;
static chibios_rt::Mutex txUsbMutex;

msg_t PostTxFrame(CANTxFrame *frame, uint8_t nBus)
{
    if (nBus >= NUM_CAN_BUSES)
        return MSG_RESET;

    txMutex.lock();
    for (int i = 0; i < MAILBOX_SIZE; i++) {
        if (!txMsgUsed[nBus][i]) {
            CanStore(txFrames[nBus][i], *frame);
            txMsgUsed[nBus][i] = true;

            msg_t result = txMb[nBus].post(&txFrames[nBus][i], TIME_IMMEDIATE);
            if (result != MSG_OK) {
                txMsgUsed[nBus][i] = false;
                txMutex.unlock();
                return result;
            }
            txMutex.unlock();
            if (nBus == 0)
                PostTxUsbFrame(frame);  // Only post to USB after successful CAN TX post, USB mirrors bus 0
            return result;
        }
    }

    txMutex.unlock();
    return MSG_TIMEOUT;  // No free slots
}

msg_t FetchTxFrame(CANTxFrame *frame, uint8_t nBus, sysinterval_t timeout)
{
    CanSlot<CANTxFrame> *txFrame;
    msg_t result = txMb[nBus].fetch(&txFrame, timeout);
    if (result == MSG_OK) {
        CanLoad(*frame, *txFrame); // Copy before freeing the slot so a producer can't overwrite it
        txMutex.lock();
        txMsgUsed[nBus][txFrame - txFrames[nBus]] = false;
        txMutex.unlock();
    }
    return result;
}

msg_t PostTxUsbFrame(CANTxFrame *frame)
{
    txUsbMutex.lock();
    for (int i = 0; i < MAILBOX_SIZE; i++) {
        if (!txUsbMsgUsed[i]) {
            CanStore(txUsbFrames[i], *frame);
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
    CanSlot<CANTxFrame> *txFrame;
    msg_t result = txUsbMb.fetch(&txFrame, timeout);
    if (result == MSG_OK) {
        CanLoad(*frame, *txFrame); // Copy before freeing the slot so a producer can't overwrite it
        txUsbMutex.lock();
        txUsbMsgUsed[txFrame - txUsbFrames] = false;
        txUsbMutex.unlock();
    }
    return result;
}

msg_t PostRxFrame(CANRxFrame *frame, uint8_t nBus)
{
    rxMutex.lock();
    for (int i = 0; i < MAILBOX_SIZE; i++) {
        if (!rxMsgUsed[i]) {
            CanStore(rxFrames[i], *frame);
            rxBus[i] = nBus;
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

msg_t FetchRxFrame(CANRxFrame *frame, uint8_t *pBus)
{
    CanSlot<CANRxFrame> *rxFrame;
    msg_t result = rxMb.fetch(&rxFrame, TIME_IMMEDIATE);
    if (result == MSG_OK) {
        CanLoad(*frame, *rxFrame); // Copy before freeing the slot so a producer can't overwrite it
        *pBus = rxBus[rxFrame - rxFrames];
        rxMutex.lock();
        rxMsgUsed[rxFrame - rxFrames] = false;
        rxMutex.unlock();
    }
    return result;
}
