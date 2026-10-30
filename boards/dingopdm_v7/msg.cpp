#include "msg.h"
#include "can_frame.h"
#include "config.h"
#include "profet.h"
#include "device.h"
#include "status.h"


CANTxMsg TxMsg0()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 0 (Digital inputs 1-2) and Device Status
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 0);
    stMsg.frame.DLC = 8; // Bytes to send
    stMsg.frame.data8[0] = (GetDigInputVal(1) << 1) + GetDigInputVal(0);
    stMsg.frame.data8[1] = static_cast<uint8_t>(GetDeviceState()) + (DEVICE_TYPE << 4);
    stMsg.frame.data16[1] = (uint16_t)GetTotalCurrent();
    stMsg.frame.data16[2] = (uint16_t)(GetBattVolt() * 10.0);
    stMsg.frame.data16[3] = (uint16_t)(GetBoardTemp() * 10.0);

    stMsg.bSend = true; // Always send

    return stMsg;
}

CANTxMsg TxMsg1()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 1 (Out 1-4 Current)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 1);
    stMsg.frame.DLC = 8; // Bytes to send
    stMsg.frame.data16[0] = (uint16_t)GetOutputCurrent(0);
    stMsg.frame.data16[1] = (uint16_t)GetOutputCurrent(1);
    stMsg.frame.data16[2] = (uint16_t)GetOutputCurrent(2);
    stMsg.frame.data16[3] = (uint16_t)GetOutputCurrent(3);

    stMsg.bSend = true; // Always send

    return stMsg;
}

CANTxMsg TxMsg2()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 2 (Out 5-8 Current)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 2);
    stMsg.frame.DLC = 8; // Bytes to send
    stMsg.frame.data16[0] = (uint16_t)GetOutputCurrent(4);
    stMsg.frame.data16[1] = (uint16_t)GetOutputCurrent(5);
    stMsg.frame.data16[2] = (uint16_t)GetOutputCurrent(6);
    stMsg.frame.data16[3] = (uint16_t)GetOutputCurrent(7);

    stMsg.bSend = true; // Always send

    return stMsg;
}

CANTxMsg TxMsg3()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 3 (Out 1-8 Status)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 3);
    stMsg.frame.DLC = 8; // Bytes to send
    stMsg.frame.data8[0] = (static_cast<uint8_t>(GetOutputState(1)) << 4) + static_cast<uint8_t>(GetOutputState(0));
    stMsg.frame.data8[1] = (static_cast<uint8_t>(GetOutputState(3)) << 4) + static_cast<uint8_t>(GetOutputState(2));
    stMsg.frame.data8[2] = (static_cast<uint8_t>(GetOutputState(5)) << 4) + static_cast<uint8_t>(GetOutputState(4));
    stMsg.frame.data8[3] = (static_cast<uint8_t>(GetOutputState(7)) << 4) + static_cast<uint8_t>(GetOutputState(6));
    stMsg.frame.data8[4] = (GetWiperFastOut() << 1) + GetWiperSlowOut();
    stMsg.frame.data8[5] = (static_cast<uint8_t>(GetWiperState()) << 4) + static_cast<uint8_t>(GetWiperSpeed());
    stMsg.frame.data8[6] = (GetFlasherVal(3) << 3) + (GetFlasherVal(2) << 2) +
                           (GetFlasherVal(1) << 1) + GetFlasherVal(0);
    stMsg.frame.data8[7] = 0;

    stMsg.bSend = true; // Always send

    return stMsg;
}

CANTxMsg TxMsg4()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 4 (Out 1-8 Reset Count)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 4);
    stMsg.frame.DLC = 8; // Bytes to send
    stMsg.frame.data8[0] = GetOutputOcCount(0);
    stMsg.frame.data8[1] = GetOutputOcCount(1);
    stMsg.frame.data8[2] = GetOutputOcCount(2);
    stMsg.frame.data8[3] = GetOutputOcCount(3);
    stMsg.frame.data8[4] = GetOutputOcCount(4);
    stMsg.frame.data8[5] = GetOutputOcCount(5);
    stMsg.frame.data8[6] = GetOutputOcCount(6);
    stMsg.frame.data8[7] = GetOutputOcCount(7);

    stMsg.bSend = true; // Always send

    return stMsg;
}

CANTxMsg TxMsg5()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 5 (CAN Inputs and Virtual Inputs)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 5);
    stMsg.frame.DLC = 8; // Bytes to send
    stMsg.frame.data32[0] = GetCanInOutputs();
    stMsg.frame.data32[1] = GetVirtIns();

    stMsg.bSend = GetAnyCanInEnable() || GetAnyVirtInEnable();

    return stMsg;
}

CANTxMsg TxMsg6()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 6 (Counters and Conditions)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 6);
    stMsg.frame.DLC = 8; // Bytes to send
    stMsg.frame.data8[0] = (uint8_t)GetCounterVal(0);
    stMsg.frame.data8[1] = (uint8_t)GetCounterVal(1);
    stMsg.frame.data8[2] = (uint8_t)GetCounterVal(2);
    stMsg.frame.data8[3] = (uint8_t)GetCounterVal(3);
    stMsg.frame.data32[1] = GetConditions();

    stMsg.bSend = GetAnyCounterEnable() || GetAnyConditionEnable();

    return stMsg;
}

CANTxMsg TxMsg7()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 7 (CAN Input Values 1-2)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 7);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(0), 0, 32, GetCanInFactor(0), GetCanInOffset(0));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(1), 32, 32, GetCanInFactor(1), GetCanInOffset(1));

    stMsg.bSend = GetCanInEnable(0) || GetCanInEnable(1);

    return stMsg;
}

CANTxMsg TxMsg8()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 8 (CAN Input Values 3-4)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 8);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(2), 0, 32, GetCanInFactor(2), GetCanInOffset(2));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(3), 32, 32, GetCanInFactor(3), GetCanInOffset(3));

    stMsg.bSend = GetCanInEnable(2) || GetCanInEnable(3);

    return stMsg;
}

CANTxMsg TxMsg9()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 9 (CAN Input Values 5-6)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 9);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(4), 0, 32, GetCanInFactor(4), GetCanInOffset(4));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(5), 32, 32, GetCanInFactor(5), GetCanInOffset(5));

    stMsg.bSend = GetCanInEnable(4) || GetCanInEnable(5);

    return stMsg;
}

CANTxMsg TxMsg10()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 10 (CAN Input Values 7-8)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 10);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(6), 0, 32, GetCanInFactor(6), GetCanInOffset(6));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(7), 32, 32, GetCanInFactor(7), GetCanInOffset(7));

    stMsg.bSend = GetCanInEnable(6) || GetCanInEnable(7);

    return stMsg;
}

CANTxMsg TxMsg11()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 11 (CAN Input Values 9-10)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 11);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(8), 0, 32, GetCanInFactor(8), GetCanInOffset(8));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(9), 32, 32, GetCanInFactor(9), GetCanInOffset(9));

    stMsg.bSend = GetCanInEnable(8) || GetCanInEnable(9);

    return stMsg;
}

CANTxMsg TxMsg12()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 12 (CAN Input Values 11-12)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 12);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(10), 0, 32, GetCanInFactor(10), GetCanInOffset(10));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(11), 32, 32, GetCanInFactor(11), GetCanInOffset(11));

    stMsg.bSend = GetCanInEnable(10) || GetCanInEnable(11);

    return stMsg;
}

CANTxMsg TxMsg13()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 13 (CAN Input Values 13-14)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 13);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(12), 0, 32, GetCanInFactor(12), GetCanInOffset(12));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(13), 32, 32, GetCanInFactor(13), GetCanInOffset(13));

    stMsg.bSend = GetCanInEnable(12) || GetCanInEnable(13);

    return stMsg;
}

CANTxMsg TxMsg14()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 14 (CAN Input Values 15-16)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 14);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(14), 0, 32, GetCanInFactor(14), GetCanInOffset(14));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(15), 32, 32, GetCanInFactor(15), GetCanInOffset(15));

    stMsg.bSend = GetCanInEnable(14) || GetCanInEnable(15);

    return stMsg;
}

CANTxMsg TxMsg15()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 15 (CAN Input Values 17-18)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 15);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(16), 0, 32, GetCanInFactor(16), GetCanInOffset(16));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(17), 32, 32, GetCanInFactor(17), GetCanInOffset(17));

    stMsg.bSend = GetCanInEnable(16) || GetCanInEnable(17);

    return stMsg;
}

CANTxMsg TxMsg16()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 16 (CAN Input Values 19-20)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 16);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(18), 0, 32, GetCanInFactor(18), GetCanInOffset(18));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(19), 32, 32, GetCanInFactor(19), GetCanInOffset(19));

    stMsg.bSend = GetCanInEnable(18) || GetCanInEnable(19);

    return stMsg;
}

CANTxMsg TxMsg17()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 17 (CAN Input Values 21-22)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 17);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(20), 0, 32, GetCanInFactor(20), GetCanInOffset(20));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(21), 32, 32, GetCanInFactor(21), GetCanInOffset(21));

    stMsg.bSend = GetCanInEnable(20) || GetCanInEnable(21);

    return stMsg;
}

CANTxMsg TxMsg18()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 18 (CAN Input Values 23-24)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 18);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(22), 0, 32, GetCanInFactor(22), GetCanInOffset(22));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(23), 32, 32, GetCanInFactor(23), GetCanInOffset(23));

    stMsg.bSend = GetCanInEnable(22) || GetCanInEnable(23);

    return stMsg;
}

CANTxMsg TxMsg19()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 19 (CAN Input Values 25-26)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 19);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(24), 0, 32, GetCanInFactor(24), GetCanInOffset(24));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(25), 32, 32, GetCanInFactor(25), GetCanInOffset(25));

    stMsg.bSend = GetCanInEnable(24) || GetCanInEnable(25);

    return stMsg;
}

CANTxMsg TxMsg20()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 20 (CAN Input Values 27-28)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 20);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(26), 0, 32, GetCanInFactor(26), GetCanInOffset(26));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(27), 32, 32, GetCanInFactor(27), GetCanInOffset(27));

    stMsg.bSend = GetCanInEnable(26) || GetCanInEnable(27);

    return stMsg;
}

CANTxMsg TxMsg21()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 21 (CAN Input Values 29-30)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 21);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(28), 0, 32, GetCanInFactor(28), GetCanInOffset(28));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(29), 32, 32, GetCanInFactor(29), GetCanInOffset(29));

    stMsg.bSend = GetCanInEnable(28) || GetCanInEnable(29);

    return stMsg;
}

CANTxMsg TxMsg22()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 22 (CAN Input Values 31-32)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 22);
    stMsg.frame.DLC = 8; // Bytes to send
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(30), 0, 32, GetCanInFactor(30), GetCanInOffset(30));
    Dbc::EncodeFloat(stMsg.frame.data8, GetCanInVal(31), 32, 32, GetCanInFactor(31), GetCanInOffset(31));

    stMsg.bSend = GetCanInEnable(30) || GetCanInEnable(31);

    return stMsg;
}

CANTxMsg TxMsg23()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 23 (Output Duty Cycle)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 23);
    stMsg.frame.DLC = 8;
    stMsg.frame.data8[0] = GetOutputDC(0);
    stMsg.frame.data8[1] = GetOutputDC(1);
    stMsg.frame.data8[2] = GetOutputDC(2);
    stMsg.frame.data8[3] = GetOutputDC(3);
    stMsg.frame.data8[4] = GetOutputDC(4);
    stMsg.frame.data8[5] = GetOutputDC(5);
    stMsg.frame.data8[6] = GetOutputDC(6);
    stMsg.frame.data8[7] = GetOutputDC(7);

    stMsg.bSend = GetAnyPwmEnable();

    return stMsg;
}

CANTxMsg TxMsg24()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 24 (Keypad Button States)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 24);
    stMsg.frame.DLC = 8;
    stMsg.frame.data8[0] =  GetKeypadButtons(0) & 0xFF;
    stMsg.frame.data8[1] = (GetKeypadButtons(0) >> 8) & 0xFF;
    stMsg.frame.data8[2] = (GetKeypadButtons(0) >> 16) & 0xFF;
    stMsg.frame.data8[3] =  GetKeypadButtons(1) & 0xFF;
    stMsg.frame.data8[4] = (GetKeypadButtons(1) >> 8) & 0xFF;
    stMsg.frame.data8[5] = (GetKeypadButtons(1) >> 16) & 0xFF;
    stMsg.frame.data8[6] = 0;
    stMsg.frame.data8[7] = 0;

    stMsg.bSend = GetAnyKeypadEnable();

    return stMsg;
}

CANTxMsg TxMsg25()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 25 (Keypad 1 Dials)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 25);
    stMsg.frame.DLC = 8;
    stMsg.frame.data8[0] =  (uint16_t)GetKeypadDialVal(0,0) & 0xFF;
    stMsg.frame.data8[1] = ((uint16_t)GetKeypadDialVal(0,0) >> 8) & 0xFF;
    stMsg.frame.data8[2] = ((uint16_t)GetKeypadDialVal(0,1) & 0xFF);
    stMsg.frame.data8[3] = ((uint16_t)GetKeypadDialVal(0,1) >> 8) & 0xFF;
    stMsg.frame.data8[4] =  (uint16_t)GetKeypadDialVal(0,2) & 0xFF;
    stMsg.frame.data8[5] = ((uint16_t)GetKeypadDialVal(0,2) >> 8) & 0xFF;
    stMsg.frame.data8[6] =  (uint16_t)GetKeypadDialVal(0,3) & 0xFF;
    stMsg.frame.data8[7] = ((uint16_t)GetKeypadDialVal(0,3) >> 8) & 0xFF;

    stMsg.bSend = GetKeypadEnable(0);

    return stMsg;
}

CANTxMsg TxMsg26()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 26 (Keypad 2 Dials)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 26);
    stMsg.frame.DLC = 8;
    stMsg.frame.data8[0] =  (uint16_t)GetKeypadDialVal(1,0) & 0xFF;
    stMsg.frame.data8[1] = ((uint16_t)GetKeypadDialVal(1,0) >> 8) & 0xFF;
    stMsg.frame.data8[2] = ((uint16_t)GetKeypadDialVal(1,1) & 0xFF);
    stMsg.frame.data8[3] = ((uint16_t)GetKeypadDialVal(1,1) >> 8) & 0xFF;
    stMsg.frame.data8[4] =  (uint16_t)GetKeypadDialVal(1,2) & 0xFF;
    stMsg.frame.data8[5] = ((uint16_t)GetKeypadDialVal(1,2) >> 8) & 0xFF;
    stMsg.frame.data8[6] =  (uint16_t)GetKeypadDialVal(1,3) & 0xFF;
    stMsg.frame.data8[7] = ((uint16_t)GetKeypadDialVal(1,3) >> 8) & 0xFF;

    stMsg.bSend = GetKeypadEnable(1);

    return stMsg;
}

CANTxMsg TxMsg27()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 27 (Timer outputs and running states)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 27);
    stMsg.frame.DLC = 8;
    stMsg.frame.data8[0] = GetTimerOutputs();
    stMsg.frame.data8[1] = GetTimersRunning();
    stMsg.frame.data16[1] = 0;
    stMsg.frame.data32[1] = 0;

    stMsg.bSend = GetAnyTimerEnable();

    return stMsg;
}

CANTxMsg TxMsg28()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 28 (Timer 1-4 Elapsed, 10ms units)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 28);
    stMsg.frame.DLC = 8;
    stMsg.frame.data16[0] = GetTimerElapsed(0);
    stMsg.frame.data16[1] = GetTimerElapsed(1);
    stMsg.frame.data16[2] = GetTimerElapsed(2);
    stMsg.frame.data16[3] = GetTimerElapsed(3);

    stMsg.bSend = GetAnyTimerEnable();

    return stMsg;
}

CANTxMsg TxMsg29()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 29 (Timer 5-8 Elapsed, 10ms units)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 29);
    stMsg.frame.DLC = 8;
    stMsg.frame.data16[0] = GetTimerElapsed(4);
    stMsg.frame.data16[1] = GetTimerElapsed(5);
    stMsg.frame.data16[2] = GetTimerElapsed(6);
    stMsg.frame.data16[3] = GetTimerElapsed(7);

    stMsg.bSend = GetAnyTimerEnable();

    return stMsg;
}

CANTxMsg TxMsg30()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 30 (Math Block 1-2 Values, x1000)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 30);
    stMsg.frame.DLC = 8;
    stMsg.frame.data32[0] = (int32_t)(GetMathVal(0) * 1000.0f);
    stMsg.frame.data32[1] = (int32_t)(GetMathVal(1) * 1000.0f);

    stMsg.bSend = GetMathEnable(0) || GetMathEnable(1);

    return stMsg;
}

CANTxMsg TxMsg31()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 31 (Math Block 3-4 Values, x1000)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 31);
    stMsg.frame.DLC = 8;
    stMsg.frame.data32[0] = (int32_t)(GetMathVal(2) * 1000.0f);
    stMsg.frame.data32[1] = (int32_t)(GetMathVal(3) * 1000.0f);

    stMsg.bSend = GetMathEnable(2) || GetMathEnable(3);

    return stMsg;
}

CANTxMsg TxMsg32()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 32 (Math Block 5-6 Values, x1000)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 32);
    stMsg.frame.DLC = 8;
    stMsg.frame.data32[0] = (int32_t)(GetMathVal(4) * 1000.0f);
    stMsg.frame.data32[1] = (int32_t)(GetMathVal(5) * 1000.0f);

    stMsg.bSend = GetMathEnable(4) || GetMathEnable(5);

    return stMsg;
}

CANTxMsg TxMsg33()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 33 (Math Block 7-8 Values, x1000)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 33);
    stMsg.frame.DLC = 8;
    stMsg.frame.data32[0] = (int32_t)(GetMathVal(6) * 1000.0f);
    stMsg.frame.data32[1] = (int32_t)(GetMathVal(7) * 1000.0f);

    stMsg.bSend = GetMathEnable(6) || GetMathEnable(7);

    return stMsg;
}

CANTxMsg TxMsg34()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 34 (Digital Input 1-2 PWM Duty 0.01%, Freq 0.1Hz)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 34);
    stMsg.frame.DLC = 8;
    stMsg.frame.data16[0] = GetDigInPwmDuty(0);
    stMsg.frame.data16[1] = GetDigInPwmFreq(0);
    stMsg.frame.data16[2] = GetDigInPwmDuty(1);
    stMsg.frame.data16[3] = GetDigInPwmFreq(1);

    stMsg.bSend = GetDigInPwmEnable(0) || GetDigInPwmEnable(1);

    return stMsg;
}

CANTxMsg TxMsg35()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 35 (Digital Input 3-4 PWM Duty 0.01%, Freq 0.1Hz)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 35);
    stMsg.frame.DLC = 8;
    stMsg.frame.data16[0] = GetDigInPwmDuty(2);
    stMsg.frame.data16[1] = GetDigInPwmFreq(2);
    stMsg.frame.data16[2] = GetDigInPwmDuty(3);
    stMsg.frame.data16[3] = GetDigInPwmFreq(3);

    stMsg.bSend = GetDigInPwmEnable(2) || GetDigInPwmEnable(3);

    return stMsg;
}

CANTxMsg TxMsg36()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 36 (Digital Input 5-6 PWM Duty 0.01%, Freq 0.1Hz)
    //=======================================================
    CanSetId(stMsg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET + 36);
    stMsg.frame.DLC = 8;
    stMsg.frame.data16[0] = GetDigInPwmDuty(4);
    stMsg.frame.data16[1] = GetDigInPwmFreq(4);
    stMsg.frame.data16[2] = GetDigInPwmDuty(5);
    stMsg.frame.data16[3] = GetDigInPwmFreq(5);

    stMsg.bSend = GetDigInPwmEnable(4) || GetDigInPwmEnable(5);

    return stMsg;
}
