DEMO      := ../openblt/Target/Demo
PROJ      := openblt_stm32f446
FAMILY    := F4
DEVICE    := STM32F446xx
# The F446RE demo has the F446 CMSIS device header but no CAN HAL, the P405 demo has the CAN HAL
HAL_DIR   := $(DEMO)/ARMCM4_STM32F4_Olimex_STM32P405_GCC/Boot/lib/STM32F4xx_HAL_Driver
HAL_CONF  := $(DEMO)/ARMCM4_STM32F4_Olimex_STM32P405_GCC/Boot/lib
CMSIS_DIR := $(DEMO)/ARMCM4_STM32F4_Nucleo_F446RE_CubeIDE/Boot/Drivers/CMSIS
SYSTEM_C  := $(DEMO)/ARMCM4_STM32F4_Nucleo_F446RE_CubeIDE/Boot/Core/Src/system_stm32f4xx.c
STARTUP_S := $(DEMO)/ARMCM4_STM32F4_Nucleo_F446RE_CubeIDE/Boot/Core/Startup/startup_stm32f446retx.s

include ../common/rules.mk
