// Timer and luaLog on top of the C API (core/lua_script.cpp), same names as the CoffeeDingoFW scripts
#pragma once

static const char acLuaPrelude[] =
    "luaLog = print\n"
    "Timer = {}\n"
    "Timer.__index = Timer\n"
    "function Timer.new() return setmetatable({t = millis()}, Timer) end\n"
    "function Timer:reset() self.t = millis() end\n"
    "function Timer:getElapsedSeconds() return (millis() - self.t) / 1000 end\n";
