#include "request_msg.h"
#include "can_frame.h"
#include "device.h"
#include "can.h"
#include "config.h"
#include "config_handler.h"
#include "mcu_utils.h"
#include "device_config.h"
#include "mailbox.h"
#include "enums.h"
#include "lua_script.h"

// External variables from pdm.cpp that we need access to
extern DeviceConfig stConfig;
extern bool bSleepRequest;

// The bootloader can't read the config, hand it the CAN settings, layout in bootloader/common/dingo_blt.h
static void RequestBootloaderCan()
{
    volatile uint32_t *pHandoff = (volatile uint32_t *)BL_HANDOFF_ADDR;
    pHandoff[1] = stConfig.stDevice.nBaseId;
    pHandoff[2] = static_cast<uint32_t>(stConfig.stDevice.eCanSpeed[0]);
    pHandoff[0] = 0xB00710AD; // Magic last
    __DSB();

    NVIC_SystemReset();
}

void CheckRequestMsgs(CANRxFrame *frame)
{
    //Check for settings request message, (Base ID - 1)
    if(CanId(*frame) != (uint32_t)(stConfig.stDevice.nBaseId + CONFIG_RX_OFFSET))
        return;

    #if CAN_SLEEP
    // Check for sleep request
    if ((frame->DLC == 8) && 
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::Sleep)) &&
        (frame->data8[1] == 'Q') && (frame->data8[2] == 'U') && 
        (frame->data8[3] == 'I') && (frame->data8[4] == 'T'))
    {
        CANTxFrame txMsg;
        CanSetId(txMsg, stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET);
        txMsg.DLC = 8;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::Sleep);
        txMsg.data8[1] = 'Q';
        txMsg.data8[2] = 'U';
        txMsg.data8[3] = 'I';
        txMsg.data8[4] = 'T';
        txMsg.data8[5] = 1; // Acknowledge sleep request
        txMsg.data8[6] = 0;
        txMsg.data8[7] = 0;

        PostTxFrame(&txMsg);

        bSleepRequest = true;
    }
    #endif

    // Check for burn request
    if ((frame->DLC == 8) && 
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::BurnSettings)) &&
        (frame->data8[1] == 1) &&
        (frame->data8[2] == 3) && 
        (frame->data8[3] == 8))
    {
        CANTxFrame txMsg;
        CanSetId(txMsg, stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET);
        txMsg.DLC = 8;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::BurnSettings);
        txMsg.data8[1] = 1;
        txMsg.data8[2] = 3;
        txMsg.data8[3] = 8;
        txMsg.data8[4] = WriteConfig();
        txMsg.data8[5] = 0;
        txMsg.data8[6] = 0;
        txMsg.data8[7] = 0;
        PostTxFrame(&txMsg);
    }

    // Check for bootloader request
    // Byte 6: 1 = OpenBLT over CAN, otherwise USB DFU
    if ((frame->DLC == 8) &&
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::Bootloader)) && 
        (frame->data8[1] == 'B') && (frame->data8[2] == 'O') && 
        (frame->data8[3] == 'O') && (frame->data8[4] == 'T') && (frame->data8[5] == 'L'))
    {
        if (frame->data8[6] == 1)
            RequestBootloaderCan();
        #if HAS_USB
        else
            RequestBootloader();
        #endif
    }

    // Check for restart request - applies settings that need a reset (e.g. CAN bitrate)
    // Unburned settings are lost, host should burn first
    if ((frame->DLC == 8) &&
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::Restart)) &&
        (frame->data8[1] == 'R') && (frame->data8[2] == 'E') &&
        (frame->data8[3] == 'S') && (frame->data8[4] == 'E') && (frame->data8[5] == 'T'))
    {
        CANTxFrame txMsg;
        CanSetId(txMsg, stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET);
        txMsg.DLC = 8;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::Restart);
        txMsg.data8[1] = 'R';
        txMsg.data8[2] = 'E';
        txMsg.data8[3] = 'S';
        txMsg.data8[4] = 'E';
        txMsg.data8[5] = 'T';
        txMsg.data8[6] = 1; // Acknowledge restart request
        txMsg.data8[7] = 0;

        PostTxFrame(&txMsg);

        // Let the ack go out before resetting, same margin as sleep
        chThdSleepMilliseconds(100);

        NVIC_SystemReset();
    }

    // Check for version request
    if ((frame->DLC == 8) &&
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::Version)))
    {
        CANTxFrame txMsg;
        CanSetId(txMsg, stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET);
        txMsg.DLC = 8;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::Version);
        txMsg.data8[1] = 0;
        txMsg.data8[2] = 0;
        txMsg.data8[3] = 0;
        txMsg.data8[4] = MAJOR_VERSION;
        txMsg.data8[5] = MINOR_VERSION;
        txMsg.data8[6] = BUILD >> 8;
        txMsg.data8[7] = BUILD & 0xFF;

        PostTxFrame(&txMsg);
    }

    #if HAS_LUA
    CheckLuaMsgs(frame);
    #endif
}
