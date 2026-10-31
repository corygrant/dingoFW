# Builds every OpenBLT port, output in bootloader/<mcu>/build
#   stm32f303: canboard_v2
#   stm32f446: dingopdm_v7, dingopdmmax_v1, pt-dpdm4_1
#   stm32g474: dingopdm_v8
PORTS := stm32f303 stm32f446 stm32g474

all clean:
	for p in $(PORTS); do $(MAKE) -C $$p $@ || exit 1; done

.PHONY: all clean
