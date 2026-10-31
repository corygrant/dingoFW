// Included at the end of lua.h (LUA_USER_H, core/lua.mk), ahead of the llimits.h defaults: no stdio or clock on the module
#pragma once

#ifdef __cplusplus
extern "C" {
#endif
void LuaWriteString(const char *s, size_t l);
void LuaWriteLine(void);
#ifdef __cplusplus
}
#endif

// print() goes to the message read back with LuaErr
#define lua_writestring(s, l)      LuaWriteString((s), (l))
#define lua_writeline()            LuaWriteLine()
#define lua_writestringerror(s, p) ((void)(s), (void)(p))

#define luai_makeseed()            0u
#define LUAI_MAXCCALLS             32 // C stack depth (parser nesting, nested pcall/metamethods), sized for the Lua thread stack
