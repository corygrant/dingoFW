# OpenBLT and Lua

Notes from adding the OpenBLT CAN bootloader (#78) and Lua scripting (#75), 2026-10-31. Everything below builds; none of it has run on hardware yet.

## Build results

| | Flash | RAM |
|---|---|---|
| Bootloaders (F446 / F303 / G474) | 7.2KB of 16KB / 6.3KB of 8KB / 7.8KB of 16KB | — |
| v7, Max, PT-DPDM4 with Lua | ~325–330KB of 496KB | ~90% used, ~12.5KB left |
| v8 | unchanged, now at `0x08004000` | unchanged |
| CANBoard | unchanged, now at `0x08002000` | unchanged |

Checks done: all five apps and the three bootloaders build. Each built app passes the bootloader's start-up check (simulated against the image). `tests/lua_check.c` runs the module's Lua configuration plus a sample script on the PC under the 40KB heap limit, and passes.

## OpenBLT

- **Source:** upstream OpenBLT as a submodule (`bootloader/openblt`, v01.22.00). Our own code is `bootloader/common` plus a small folder per chip (`stm32f446`, `stm32f303`, `stm32g474`). ST's driver libraries come from OpenBLT's own examples, nothing extra is copied into the repo.
- **Which image goes where:** `stm32f446` for dingoPDM v7, dingoPDM-Max and PT-DPDM4 (same crystal, CAN pins and transceiver standby). `stm32g474` for dingoPDM v8. `stm32f303` for the CANBoard.
- **Flash layout:** the bootloader takes the first 16KB on the PDMs and 8KB on the CANBoard, and the app is linked after it. Without the bootloader on the chip the app doesn't start. A mass erase removes it.
- **CAN settings:** on a bootloader request the app leaves its base ID and bitrate at the top of RAM (`BL_HANDOFF_ADDR`, `0x2001FFF0` on F4/G4, `0x10000FF0` CCM on F303), then resets. The bootloader never reads the config, so a config layout change can't break it.
- **Startup:** the app starts straight away, there's no 500ms wait on every power-up. The bootloader only stays active when the app asked for it, or when there's no valid app (stack pointer in RAM, reset vector inside the app region).
- **Bootloader command (cmd 33, "BOOTL"):** byte 6 = 1 starts a CAN update. The host then talks XCP on base ID +12 (to the module) and +13 (from the module), at the module's bitrate. Byte 6 = 0 still goes to the ST USB DFU bootloader, which the bootloader now handles, so `enter_bootloader.S` is gone.
- **Config is safe:** updates never erase the config pages on v8 (last 8KB) or the CANBoard (last 2KB).
- **Tooling:** the app build also writes `build/<board>.srec`, the file a CAN update uses. `make -C bootloader` (or `pixi run build-bl`) builds the bootloaders into `bootloader/<mcu>/build/`. The VS Code debug configs write the bootloader and the app together.

## Lua

- **Version:** Lua 5.5.1 as a submodule (`lua/`), `LUA_32BITS` to match the single-precision FPU, interpreter compiled `-Os` (`core/lua.mk`). Base, table, string and math libraries only.
- **Script storage:** FRAM only. A board without FRAM can't enable Lua, the build stops with an error. FRAM layout: config + CRC from 0, then `[length][CRC32][source]` from `0x2000`. The script is read straight from FRAM into the parser, so it costs no RAM in `stConfig`.
- **Script API** (same as the CoffeeDingoFW scripts): `readVar(i)`, `setLuaOut(slot, v)`, `txCan(bus, id, ext, {bytes})`, `canRxAdd(id)` with `onCanRx(bus, id, dlc, {bytes})`, `onTick()` with `setTickRate(hz)`, `millis()`, `luaLog(msg)` / `print(...)`, `Timer.new()` / `t:reset()` / `t:getElapsedSeconds()`. Buses are numbered from 1. `txCan` respects the mute input.
- **CAN commands** on the config RX ID (base + 1), replies on base + 0:

  | Cmd | Request | Reply |
  |---|---|---|
  | 40 LuaWrite | `[40, offHi, offLo, 5 bytes]` | `[40, offHi, offLo]` (DLC 3) |
  | 41 LuaWriteComplete | `[41, lenHi, lenLo]` | `[41, lenHi, lenLo, stored 0/1]` (DLC 4), then the script reloads |
  | 42 LuaRead | `[42, offHi, offLo]` | `[42, offHi, offLo, 5 bytes]`, 0 past the end |
  | 43 LuaErr | `[43, offHi, offLo]` | 5 bytes of the last message or error, 0 past the end |

- **Lua outputs:** 32 new var-map entries at the end of the list, so no existing input references move.
- **Runtime:** own thread below every other thread, 40KB heap, 12KB stack. Each call into the script is capped at 200k VM instructions, and a failing script stops until the next upload.
- **v8:** Lua stays off until the FRAM is fitted. Then it's `HAS_EXT_MEMORY`, `HAS_LUA` and `include core/lua.mk` in `board.mk`. A heap in SRAM2+CCM must keep clear of the top 16 bytes of RAM (bootloader handoff), and FRAM reads use DMA, so read buffers can't be in CCM.

To run the host check:

```bash
gcc -Ilua -Icore -DLUA_32BITS -DLUA_USER_H='"lua_user.h"' tests/lua_check.c $(make -s -f core/lua.mk print-src) \
    -lm -o lua_check && ./lua_check
```

## Open before release

- **Release files need the new address.** CI still publishes `.bin` files, which carry no address. Written at `0x08000000` like before, they overwrite the bootloader. App `.bin` now goes at `0x08004000` (PDMs) or `0x08002000` (CANBoard). The release workflows also don't build or publish the bootloaders or the `.srec` files yet.
- **FRAM size.** Some units may have the 16KB MB85RC128A, so `EXT_MEMORY_SIZE` is `0x4000` and the script limit is about 8KB. If every unit has the 32KB MB85RC256V, `0x8000` gives about 24KB.
- **Recovery address.** If power drops mid-update, the module comes back in the bootloader on the default ID (`0x0DE` on the PDMs, `0x640` on the CANBoard) at 500K, not its own ID.
- **RAM on the F446 boards** is tight with Lua (~12.5KB left). About 30KB of the new flash is newlib's number formatting, which Lua needs to turn numbers into text.
- **Disk size.** OpenBLT's example folders take 1.5GB checked out (about 35MB download). CONTRIBUTING has a sparse-checkout command that cuts it to 42MB.
- **Licence.** OpenBLT is GPLv3. It's a separate binary, so the MIT app is unaffected, but `bootloader/openblt` is GPLv3.
- **Host side.** Firmware updates and script editing from dingoConfig (dingoConfig#82, dingoConfig#80) still need building.

## Hardware test order

1. Flash the bootloader and the app over SWD (the VS Code debug config does both). Check the app boots and the outputs behave normally.
2. Send cmd 33 with byte 6 = 0: it should still reach USB DFU.
3. Send cmd 33 with byte 6 = 1, then use OpenBLT's BootCommander to flash the `.srec` on base ID +12/+13.
4. Cut power partway through a CAN update. The module should come back in the bootloader on the default ID.
5. Upload a Lua script with commands 40/41, read it back with 42, and read any error text with 43.
