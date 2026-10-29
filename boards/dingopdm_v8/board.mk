MCU = cortex-m4
MCUDIR = boards/stm32g474

# List of all the board related files.
BOARDSRC = ./boards/dingopdm_v8/board.c

# Required include directories
BOARDINC = ./boards/dingopdm_v8

# Shared variables
ALLCSRC += $(BOARDSRC)
ALLINC  += $(BOARDINC)

include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_stm32g4xx.mk
include $(CHIBIOS)/os/hal/ports/STM32/STM32G4xx/platform.mk
include $(CHIBIOS)/os/common/ports/ARMv7-M/compilers/GCC/mk/port.mk

BOOTLOADERASM = $(MCUDIR)/enter_bootloader.S
ALLXASMSRC += $(BOOTLOADERASM)

CPPSRC_BOARD += comms/usb.cpp \
				core/sleep.cpp \
				functions/profet.cpp \
				functions/pwm.cpp \
				functions/analog_input.cpp \
				functions/digital_input.cpp \
				functions/starter.cpp \
				functions/wiper/wiper_digin.cpp \
				functions/wiper/wiper_intin.cpp \
				functions/wiper/wiper_mixin.cpp \
				functions/wiper/wiper.cpp \
				functions/keypad/blink/blink_button.cpp \
				functions/keypad/blink/blink_dial.cpp \
				functions/keypad/blink/blink_analog_input.cpp \
				functions/keypad/blink/blink_keypad.cpp \
				functions/keypad/grayhill/grayhill_button.cpp \
				functions/keypad/grayhill/grayhill_keypad.cpp \
				functions/keypad/keypad_button.cpp \
				functions/keypad/keypad.cpp \
				hardware/mcp9808.cpp \
				hardware/mb85rc.cpp \
				functions/neopixels.cpp

UINC_BOARD += 	./functions/wiper \
				./functions/keypad/blink \
				./functions/keypad/grayhill \
				./functions/keypad