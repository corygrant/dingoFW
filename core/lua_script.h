#pragma once

#include "port.h"

#if HAS_LUA

#if !HAS_EXT_MEMORY
#error "Lua keeps its script in FRAM, the board needs HAS_EXT_MEMORY"
#endif

#include "hal.h"

// FRAM layout: config + CRC from 0, then [length][CRC32][script source] from LUA_FRAM_ADDR
#define LUA_FRAM_ADDR   0x2000U
#define LUA_SCRIPT_ADDR (LUA_FRAM_ADDR + 8U)
#define LUA_SCRIPT_MAX  (EXT_MEMORY_SIZE - LUA_SCRIPT_ADDR)

extern float fLuaOut[NUM_LUA_OUTPUTS];

void InitLua();
void LuaCanRx(const CANRxFrame &frame, uint8_t nBus);
void CheckLuaMsgs(CANRxFrame *frame);

#endif
