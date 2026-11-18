#pragma once

#include <cstdint>
#include "port.h"
#include "enums.h"
#include "mailbox.h"
#include "device_config.h"    

struct CANTxMsg
{
    CANTxFrame frame;
    bool bSend;
};

CANTxMsg TxMsg0();
CANTxMsg TxMsg1();
CANTxMsg TxMsg2();
CANTxMsg TxMsg3();
CANTxMsg TxMsg4();
CANTxMsg TxMsg5();
CANTxMsg TxMsg6();
CANTxMsg TxMsg7();
CANTxMsg TxMsg8();
CANTxMsg TxMsg9();
CANTxMsg TxMsg10();
CANTxMsg TxMsg11();
CANTxMsg TxMsg12();
CANTxMsg TxMsg13();
CANTxMsg TxMsg14();
CANTxMsg TxMsg15();
CANTxMsg TxMsg16();
CANTxMsg TxMsg17();
CANTxMsg TxMsg18();
CANTxMsg TxMsg19();
CANTxMsg TxMsg20();
CANTxMsg TxMsg21();
CANTxMsg TxMsg22();
CANTxMsg TxMsg23();
CANTxMsg TxMsg24();
CANTxMsg TxMsg25();
CANTxMsg TxMsg26();
CANTxMsg TxMsg27();
CANTxMsg TxMsg28();
CANTxMsg TxMsg29();
CANTxMsg TxMsg30();
CANTxMsg TxMsg31();
CANTxMsg TxMsg32();
CANTxMsg TxMsg33();
CANTxMsg TxMsg34();
CANTxMsg TxMsg35();
CANTxMsg TxMsg36();
CANTxMsg TxMsg37();
CANTxMsg TxMsg38();
CANTxMsg TxMsg39();
CANTxMsg TxMsg40();
CANTxMsg TxMsg41();
CANTxMsg TxMsg42();
CANTxMsg TxMsg43();
CANTxMsg TxMsg44();
CANTxMsg TxMsg45();
CANTxMsg TxMsg46();
CANTxMsg TxMsg47();
CANTxMsg TxMsg48();
CANTxMsg TxMsg49();
CANTxMsg TxMsg50();
CANTxMsg TxMsg51();
CANTxMsg TxMsg52();

[[maybe_unused]] static CANTxMsg (*TxMsgs[NUM_TX_MSGS])() = {
    TxMsg0,
    TxMsg1,
    TxMsg2,
    TxMsg3,
    TxMsg4,
    TxMsg5,
    TxMsg6,
    TxMsg7,
    TxMsg8,
    TxMsg9,
    TxMsg10,
    TxMsg11,
    TxMsg12,
    TxMsg13,
    TxMsg14,
    TxMsg15,
    TxMsg16,
    TxMsg17,
    TxMsg18,
    TxMsg19,
    TxMsg20,
    TxMsg21,
    TxMsg22,
    TxMsg23,
    TxMsg24,
    TxMsg25,
    TxMsg26,
    TxMsg27,
    TxMsg28,
    TxMsg29,
    TxMsg30,
    TxMsg31,
    TxMsg32,
    TxMsg33,
    TxMsg34,
    TxMsg35,
    TxMsg36,
    TxMsg37,
    TxMsg38,
    TxMsg39,
    TxMsg40,
    TxMsg41,
    TxMsg42,
    TxMsg43,
    TxMsg44,
    TxMsg45,
    TxMsg46,
    TxMsg47,
    TxMsg48,
    TxMsg49,
    TxMsg50,
    TxMsg51,
    TxMsg52};

