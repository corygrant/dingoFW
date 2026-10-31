// Host check of the module's Lua build: same LUA_32BITS + lua_user.h config, the prelude, a representative
// script, and an allocator capped at the v7 LUA_HEAP_SIZE. Prints the peak heap.
//   gcc -Ilua -Icore -DLUA_32BITS -DLUA_USER_H='"lua_user.h"' tests/lua_check.c $(make -s -f core/lua.mk print-src) \
//       -lm -o lua_check && ./lua_check
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lua_prelude.h"

#define HEAP_SIZE (40 * 1024)

static size_t nUsed, nPeak;
static char acMsg[128];
static unsigned nMillis = 0xFFFFFF00u; // Wraps during the run
static float fOut[32];
static int nTx;

void LuaWriteString(const char *s, size_t l) { strncat(acMsg, s, l < sizeof(acMsg) - 1 - strlen(acMsg) ? l : sizeof(acMsg) - 1 - strlen(acMsg)); }
void LuaWriteLine(void) {}

static void *Alloc(void *ud, void *p, size_t nOld, size_t nNew)
{
    (void)ud;
    if (!p)
        nOld = 0;
    if (nNew == 0)
    {
        nUsed -= nOld;
        free(p);
        return NULL;
    }
    if (nUsed - nOld + nNew > HEAP_SIZE)
        return (p && nNew <= nOld) ? p : NULL;
    nUsed = nUsed - nOld + nNew;
    if (nUsed > nPeak)
        nPeak = nUsed;
    return realloc(p, nNew);
}

static int Millis(lua_State *L) { lua_pushinteger(L, (lua_Integer)nMillis); return 1; }
static int ReadVar(lua_State *L) { lua_pushnumber(L, (lua_Number)luaL_checkinteger(L, 1) / 2); return 1; }
static int SetLuaOut(lua_State *L) { fOut[luaL_checkinteger(L, 1)] = (float)luaL_checknumber(L, 2); return 0; }
static int TxCan(lua_State *L) { luaL_checktype(L, 4, LUA_TTABLE); nTx += (int)luaL_len(L, 4); return 0; }
static int Nop(lua_State *L) { (void)L; return 0; }

static const char acScript[] =
    "canRxAdd(0x123)\n"
    "setTickRate(50)\n"
    "local t = Timer.new()\n"
    "local last\n"
    "function onCanRx(bus, id, dlc, d) last = d[1] + d[2] * 256 end\n"
    "function onTick()\n"
    "  local v = readVar(7)\n"
    "  setLuaOut(0, v * 2)\n"
    "  setLuaOut(1, t:getElapsedSeconds())\n"
    "  if last then setLuaOut(2, last) end\n"
    "  txCan(1, 0x600, false, {1, 2, 3, string.byte('A')})\n"
    "end\n"
    "luaLog(string.format('ok %.2f %d', 1.5, 7) .. ' ' .. tostring(0.25))\n";

int main(void)
{
    lua_State *L = lua_newstate(Alloc, NULL, 0);
    assert(L);
    luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_settop(L, 0);
    lua_register(L, "millis", Millis);
    lua_register(L, "readVar", ReadVar);
    lua_register(L, "setLuaOut", SetLuaOut);
    lua_register(L, "txCan", TxCan);
    lua_register(L, "canRxAdd", Nop);
    lua_register(L, "setTickRate", Nop);

    assert(luaL_loadbufferx(L, acLuaPrelude, sizeof(acLuaPrelude) - 1, "prelude", "t") == LUA_OK);
    assert(lua_pcall(L, 0, 0, 0) == LUA_OK);
    if (luaL_loadbufferx(L, acScript, sizeof(acScript) - 1, "script", "t") != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK)
    {
        printf("script error: %s\n", lua_tostring(L, -1));
        return 1;
    }
    assert(strcmp(acMsg, "ok 1.50 7 0.25") == 0);

    for (int i = 0; i < 1000; i++)
    {
        nMillis += 20;
        lua_getglobal(L, "onCanRx");
        lua_pushinteger(L, 1);
        lua_pushinteger(L, 0x123);
        lua_pushinteger(L, 8);
        lua_createtable(L, 8, 0);
        for (int j = 0; j < 8; j++)
        {
            lua_pushinteger(L, j == 0 ? i & 0xFF : (j == 1 ? i >> 8 : 0));
            lua_seti(L, -2, j + 1);
        }
        assert(lua_pcall(L, 4, 0, 0) == LUA_OK);
        lua_getglobal(L, "onTick");
        assert(lua_pcall(L, 0, 0, 0) == LUA_OK);
    }

    assert(sizeof(lua_Number) == 4 && sizeof(lua_Integer) == 4);
    assert(fOut[0] == 7.0f);                         // readVar(7) * 2
    assert(fOut[1] > 19.9f && fOut[1] < 20.1f);      // 1000 ticks of 20ms, across the millis() wrap
    assert(fOut[2] == 999.0f);
    assert(nTx == 4000);
    lua_gc(L, LUA_GCCOLLECT);
    printf("lua_check ok, peak heap %zu of %d bytes, %zu live after a full GC\n", nPeak, HEAP_SIZE, nUsed);
    lua_close(L);
    return 0;
}
