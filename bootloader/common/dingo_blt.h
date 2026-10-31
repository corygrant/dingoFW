// Settings shared by every dingo bootloader port. Each port's blt_conf.h defines the MCU specifics, then includes this.
//
// The app hands the bootloader its CAN settings in a RAM block that survives the reset (see RequestBootloaderCan()
// in boards/<mcu>/mcu_utils.cpp), the bootloader never reads the app config. That keeps the bootloader independent
// of the DeviceConfig layout, it's flashed once and never updated with the app.
//   [0] magic  [1] base id  [2] CanBitrate index
// Without the magic (power cycle mid-update, no valid app) the port's DINGO_DEFAULT_BASE_ID at 500K is used.
#pragma once

#define DINGO_BL_CAN_MAGIC  (0xB00710ADu) // Stay in the bootloader, CAN update
#define DINGO_BL_DFU_MAGIC  (0xDEADBEEFu) // Jump to the ST ROM USB DFU bootloader (same word the app always used)

// XCP ids relative to the module base id
#define DINGO_XCP_RX_OFFSET (12u) // Host -> bootloader
#define DINGO_XCP_TX_OFFSET (13u) // Bootloader -> host

#define BOOT_CPU_XTAL_SPEED_KHZ          (8000)
#define BOOT_CPU_SYSTEM_SPEED_KHZ        (8000) // Run straight off the 8MHz HSE, every CAN bitrate divides evenly
#define BOOT_CPU_BYTE_ORDER_MOTOROLA     (0)
#define BOOT_CPU_USER_PROGRAM_START_HOOK (0)

blt_int32u DingoCanBaudrate(void);
blt_int32u DingoXcpTxId(void);
blt_int32u DingoXcpRxId(void);

#define BOOT_COM_CAN_ENABLE              (1)
#define BOOT_COM_CAN_BAUDRATE            (DingoCanBaudrate())
#define BOOT_COM_CAN_TX_MSG_ID           (DingoXcpTxId())
#define BOOT_COM_CAN_RX_MSG_ID           (DingoXcpRxId())
#define BOOT_COM_CAN_CHANNEL_INDEX       (0)
#define BOOT_COM_CAN_FD_ENABLE           (0) // Classic CAN only
#define BOOT_COM_RS232_ENABLE            (0)

// No timed backdoor: start the app at once unless it asked for the bootloader, a PDM can't wait 500ms for outputs
#define BOOT_BACKDOOR_HOOKS_ENABLE       (1)
#define BOOT_FLASH_CUSTOM_LAYOUT_ENABLE  (1)
#define BOOT_NVM_HOOKS_ENABLE            (0)
// Presence check instead of OpenBLT's vector table checksum, so an app loaded over SWD for debugging still starts
#define BOOT_NVM_CHECKSUM_HOOKS_ENABLE   (1)
#define BOOT_COP_HOOKS_ENABLE            (0)
#define BOOT_EVENTS_ENABLE               (0)
#define BOOT_XCP_SEED_KEY_ENABLE         (0)

void DingoHandoffInit(void);
