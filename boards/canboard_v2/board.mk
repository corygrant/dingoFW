MCU = cortex-m3
MCUDIR = boards/cortex-m3
USE_FPU = no
USE_FPU_OPT =

# Function alignment padding costs ~3.8KB, more than the 62KB flash can spare at -O0
USE_OPT := $(filter-out -falign-functions=16,$(USE_OPT))

# List of all the board related files.
BOARDSRC = ./boards/canboard_v2/board.c

# Required include directories
BOARDINC = ./boards/canboard_v2

# Shared variables
ALLCSRC += $(BOARDSRC)
ALLINC  += $(BOARDINC)

include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_stm32f3xx.mk
include $(CHIBIOS)/os/hal/ports/STM32/STM32F3xx/platform.mk
include $(CHIBIOS)/os/common/ports/ARMv7-M/compilers/GCC/mk/port.mk

CPPSRC_BOARD += functions/analog_input.cpp \
				functions/digital_input.cpp \
				functions/digital_output.cpp