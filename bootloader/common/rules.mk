# Shared build for the dingo OpenBLT ports. A port makefile sets these, then includes this file:
#   PROJ        output name
#   FAMILY      OpenBLT target suffix, Source/ARMCM4_STM32$(FAMILY)
#   DEVICE      CMSIS device define
#   HAL_DIR     ST HAL driver folder (Inc/ Src/), taken from an OpenBLT demo
#   CMSIS_DIR   CMSIS folder (Include/ Device/ST/<family>/Include/)
#   HAL_CONF    folder with <family>_hal_conf.h
#   SYSTEM_C    system_<family>.c
#   STARTUP_S   startup_<device>.s

OPENBLT := ../openblt/Target
SRC     := $(OPENBLT)/Source
TGT     := $(SRC)/ARMCM4_STM32$(FAMILY)
BUILD   := build

CSRC := $(wildcard $(SRC)/*.c) \
        $(addprefix $(TGT)/,can.c cpu.c flash.c nvm.c timer.c GCC/cpu_comp.c) \
        $(wildcard $(HAL_DIR)/Src/*.c) \
        $(SYSTEM_C) \
        ../common/hooks.c \
        $(filter-out flash_layout.c,$(wildcard *.c))

INC := . ../common $(SRC) $(TGT) $(HAL_DIR)/Inc $(CMSIS_DIR)/Include $(wildcard $(CMSIS_DIR)/Device/ST/*/Include) $(HAL_CONF)

MCUFLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS   := $(MCUFLAGS) -Os -g3 -Wall -ffunction-sections -fdata-sections -fno-strict-aliasing \
            -DUSE_FULL_LL_DRIVER -DUSE_HAL_DRIVER -D$(DEVICE) -DHSE_VALUE=8000000U \
            $(addprefix -I,$(INC))
LDFLAGS  := $(MCUFLAGS) -T$(PROJ).ld -L../common -specs=nano.specs -nostartfiles \
            -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(PROJ).map

OBJS := $(addprefix $(BUILD)/,$(notdir $(CSRC:.c=.o) $(STARTUP_S:.s=.o)))
vpath %.c $(sort $(dir $(CSRC)))
vpath %.s $(dir $(STARTUP_S))

CC := arm-none-eabi-gcc
OC := arm-none-eabi-objcopy

all: $(BUILD)/$(PROJ).elf
	$(OC) -O binary $< $(BUILD)/$(PROJ).bin
	$(OC) -O ihex $< $(BUILD)/$(PROJ).hex
	arm-none-eabi-size $<

$(BUILD)/$(PROJ).elf: $(OBJS) $(PROJ).ld ../common/sections.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.s | $(BUILD)
	$(CC) $(MCUFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
-include $(OBJS:.o=.d)
