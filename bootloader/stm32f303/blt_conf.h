// CANBoard v2, F303x8: no USB so no DFU
#ifndef BLT_CONF_H
#define BLT_CONF_H

#define DINGO_DEVICE_H        "stm32f3xx.h"
#define DINGO_DEFAULT_BASE_ID (0x640u)
#define DINGO_HANDOFF_ADDR    (0x10000FF0u) // Top of CCM, the bootloader only uses SRAM
#define DINGO_RAM_START       (0x20000000u)
#define DINGO_RAM_END         (0x20003000u)
#define DINGO_APP_END         (0x0800F800u) // Config page after this, never erased by an update

#define BOOT_NVM_SIZE_KB      (64)

#include "dingo_blt.h"

#endif
