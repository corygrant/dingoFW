// dingoPDM v7, dingoPDM-Max v1, PT-DPDM4: same crystal, CAN pins and transceiver standby, one image
#ifndef BLT_CONF_H
#define BLT_CONF_H

#define DINGO_DEVICE_H        "stm32f4xx.h"
#define DINGO_DEFAULT_BASE_ID (0x0DEu)
#define DINGO_HANDOFF_ADDR    (0x2001FFF0u) // Top of SRAM, bootloader RAM stops well below
#define DINGO_SYSTEM_MEMORY   (0x1FFF0000u)
#define DINGO_RAM_START       (0x20000000u)
#define DINGO_RAM_END         (0x20020000u)
#define DINGO_APP_END         (0x08080000u)

#define BOOT_NVM_SIZE_KB      (512)

#include "dingo_blt.h"

#endif
