#include "msg.h"
#include "can_frame.h"
#include "config.h"
#include "status.h"
#include "device.h"

uint8_t nHeartbeat = 0;

CANTxMsg TxMsg3()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 3 (CAN Inputs and Virtual Inputs)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 3);
    stMsg.frame.DLC = 8; // Bytes to send
    stMsg.frame.data32[0] = GetCanInOutputs();
    stMsg.frame.data32[1] = GetVirtIns();

    stMsg.bSend = GetAnyCanInEnable() || GetAnyVirtInEnable();

    return stMsg;
}

CANTxMsg TxMsg4()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 4 (Counters and Conditions)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 4);
    stMsg.frame.DLC = 8; // Bytes to send
    stMsg.frame.data8[0] = (uint8_t)GetCounterVal(0);
    stMsg.frame.data8[1] = (uint8_t)GetCounterVal(1);
    stMsg.frame.data8[2] = (uint8_t)GetCounterVal(2);
    stMsg.frame.data8[3] = (uint8_t)GetCounterVal(3);
    stMsg.frame.data32[1] = GetConditions();

    stMsg.bSend = GetAnyCounterEnable() || GetAnyConditionEnable();

    return stMsg;
}

CANTxMsg TxMsg5()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 5 (CAN Input Values 1-2)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 5);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(0), 0, 32, GetCanInFactor(0), GetCanInOffset(0));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(1), 32, 32, GetCanInFactor(1), GetCanInOffset(1));

    stMsg.bSend = GetCanInEnable(0) || GetCanInEnable(1);

    return stMsg;
}

CANTxMsg TxMsg6()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 6 (CAN Input Values 3-4)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 6);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(2), 0, 32, GetCanInFactor(2), GetCanInOffset(2));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(3), 32, 32, GetCanInFactor(3), GetCanInOffset(3));

    stMsg.bSend = GetCanInEnable(2) || GetCanInEnable(3);

    return stMsg;
}

CANTxMsg TxMsg7()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 7 (CAN Input Values 5-6)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 7);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(4), 0, 32, GetCanInFactor(4), GetCanInOffset(4));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(5), 32, 32, GetCanInFactor(5), GetCanInOffset(5));

    stMsg.bSend = GetCanInEnable(4) || GetCanInEnable(5);

    return stMsg;
}

CANTxMsg TxMsg8()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 8 (CAN Input Values 7-8)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 8);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(6), 0, 32, GetCanInFactor(6), GetCanInOffset(6));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(7), 32, 32, GetCanInFactor(7), GetCanInOffset(7));

    stMsg.bSend = GetCanInEnable(6) || GetCanInEnable(7);

    return stMsg;
}

