MCU = cortex-m4
MCUDIR = boards/stm32f302

# List of all the board related files.
BOARDSRC = ./boards/linboard_v1/board.c

# Required include directories
BOARDINC = ./boards/linboard_v1

# Shared variables
ALLCSRC += $(BOARDSRC)
ALLINC  += $(BOARDINC)

include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_stm32f3xx.mk
include $(CHIBIOS)/os/hal/ports/STM32/STM32F3xx/platform.mk
include $(CHIBIOS)/os/common/ports/ARMv7-M/compilers/GCC/mk/port.mk

CPPSRC_BOARD += comms/usb.cpp \
				core/sleep.cpp \
				hardware/mb85rc.cpp

UINC_BOARD +=

include core/lua.mk
