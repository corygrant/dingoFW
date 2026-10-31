# Lua 5.5 (lua/ submodule), included by board.mk on boards with HAS_LUA
# Base, table, string and math libraries only, no io/os/package/debug on the module

LUASRC := $(addprefix lua/,lapi.c lauxlib.c lbaselib.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c \
          llex.c lmathlib.c lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c lstrlib.c ltable.c \
          ltablib.c ltm.c lundump.c lvm.c lzio.c)

ALLCSRC += $(LUASRC)
CPPSRC_BOARD += core/lua_script.cpp
UINC_BOARD += ./lua

# 32-bit int and float: the M4 FPU is single precision, and half the TValue size. Every file that includes lua.h needs it
LUADEFS := -DLUA_32BITS -DLUA_USER_H='"lua_user.h"'
USE_COPT += $(LUADEFS)
USE_CPPOPT += $(LUADEFS)

# The interpreter is built -Os whatever the app's -O, it's ~3x smaller and faster than -O0 and nobody steps through it
$(addprefix $(BUILDDIR)/obj/,$(notdir $(LUASRC:.c=.o))): OPT += -Os

# For tests/lua_check.c, only defined when asked for or it becomes the default goal
ifeq ($(MAKECMDGOALS),print-src)
print-src:
	@echo $(LUASRC)
endif
