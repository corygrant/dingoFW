// One stored Lua program on its own thread, below every other thread. The source lives in FRAM (lua_script.h)
// and is parsed on the module. Each call into the script is capped at LUA_INSTR_LIMIT VM instructions,
// a script that errors or hits the cap stops until the next upload.
//
// Script API: readVar(i), setLuaOut(slot, v), canRxAdd(id), txCan(bus, id, ext, {bytes}), setTickRate(hz),
// millis(), luaLog(msg) / print(...), Timer.new() / t:reset() / t:getElapsedSeconds()
// Callbacks: onTick(), onCanRx(bus, id, dlc, {bytes}). Buses are numbered from 1.
#include "lua_script.h"

#if HAS_LUA
#include <cstring>
#include "ch.hpp"
#include "config.h"
#include "crc.h"
#include "device.h"
#include "enums.h"
#include "can_frame.h"
#include "device_config.h"
#include "mailbox.h"
#include "param_protocol.h"
#include "hardware/mb85rc.h"
#include "lua_prelude.h"
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

extern MB85RC fram;

static_assert(sizeof(DeviceConfig) + sizeof(uint32_t) <= LUA_FRAM_ADDR, "Config + CRC run into the Lua script in FRAM");

#define LUA_THREAD_STACK (12 * 1024) // The parser recurses
#define LUA_INSTR_LIMIT 200000      // Per call into the script
#define LUA_RX_IDS 16
#define LUA_RX_QUEUE 16

float fLuaOut[NUM_LUA_OUTPUTS];

static CH_HEAP_AREA(luaHeapArea, LUA_HEAP_SIZE);
static memory_heap_t luaHeap;
static lua_State *pLua;
static volatile bool bReload = true;
static bool bRunning;
static volatile uint16_t nScriptLen; // Stored length, for LuaRead
static uint32_t nTickMs, nLastTick;

// Last print()/luaLog() line or error, read back with LuaErr
static char acMsg[128];
static size_t nMsgLen;
static bool bMsgNewLine = true;

static volatile uint32_t nRxIds[LUA_RX_IDS];
static volatile uint8_t nNumRxIds;
struct LuaRxFrame
{
    CanSlot<CANRxFrame> slot;
    uint8_t nBus;
};
static objects_fifo_t rxFifo;
static LuaRxFrame rxFrames[LUA_RX_QUEUE];
static msg_t rxMsgs[LUA_RX_QUEUE];


extern "C" void LuaWriteString(const char *s, size_t l)
{
    if (bMsgNewLine)
    {
        nMsgLen = 0;
        bMsgNewLine = false;
    }
    size_t n = (l < sizeof(acMsg) - 1 - nMsgLen) ? l : sizeof(acMsg) - 1 - nMsgLen;
    memcpy(&acMsg[nMsgLen], s, n);
    nMsgLen += n;
    acMsg[nMsgLen] = '\0';
}

extern "C" void LuaWriteLine(void)
{
    bMsgNewLine = true;
}

static void SetMsg(const char *pMsg)
{
    bMsgNewLine = true;
    LuaWriteString(pMsg, strlen(pMsg));
    bMsgNewLine = true;
}

static void *Alloc(void *, void *ptr, size_t nOld, size_t nNew)
{
    if (nNew == 0)
    {
        if (ptr)
            chHeapFree(ptr);
        return nullptr;
    }
    void *p = chHeapAlloc(&luaHeap, nNew);
    if (!p)
        return (ptr && nNew <= nOld) ? ptr : nullptr; // Lua expects a shrink never to fail
    if (ptr)
    {
        memcpy(p, ptr, nOld < nNew ? nOld : nNew);
        chHeapFree(ptr);
    }
    return p;
}

static void Hook(lua_State *L, lua_Debug *)
{
    luaL_error(L, "script exceeded instruction budget");
}

//=============================================================================
// Script storage
//=============================================================================
struct LuaHeader
{
    uint32_t nLength;
    uint32_t nCrc;
};

// FRAM reads DMA straight into this, keep it off CCM
static uint8_t aReadBuf[64];

static bool ScriptCrc(uint32_t nLen, uint32_t *pCrc)
{
    uint32_t nCrc = 0xFFFFFFFF;
    for (uint32_t nOff = 0; nOff < nLen; nOff += sizeof(aReadBuf))
    {
        uint16_t n = (nLen - nOff < sizeof(aReadBuf)) ? nLen - nOff : sizeof(aReadBuf);
        if (!fram.Read(LUA_SCRIPT_ADDR + nOff, aReadBuf, n))
            return false;
        nCrc = CalculateCRC32Partial(aReadBuf, n, nCrc);
    }
    *pCrc = ~nCrc;
    return true;
}

struct FramReader
{
    uint32_t nOff;
    uint32_t nLen;
};

static const char *ReadScript(lua_State *, void *ud, size_t *pSize)
{
    FramReader *r = (FramReader *)ud;
    uint16_t n = (r->nLen - r->nOff < sizeof(aReadBuf)) ? r->nLen - r->nOff : sizeof(aReadBuf);
    if ((n == 0) || !fram.Read(LUA_SCRIPT_ADDR + r->nOff, aReadBuf, n))
        n = 0; // A failed read ends the chunk early, the parser reports it
    r->nOff += n;
    *pSize = n;
    return (const char *)aReadBuf;
}

//=============================================================================
// Script API
//=============================================================================
static int ReadVar(lua_State *L)
{
    lua_Integer i = luaL_checkinteger(L, 1);
    lua_pushnumber(L, (i >= 0 && i < VAR_MAP_SIZE) ? *pVarMap[i] : 0);
    return 1;
}

static int SetLuaOut(lua_State *L)
{
    lua_Integer i = luaL_checkinteger(L, 1);
    float f = (float)luaL_checknumber(L, 2);
    if (i >= 0 && i < NUM_LUA_OUTPUTS)
        fLuaOut[i] = f;
    return 0;
}

static int CanRxAdd(lua_State *L)
{
    uint32_t nId = (uint32_t)luaL_checkinteger(L, 1);
    for (uint8_t i = 0; i < nNumRxIds; i++)
        if (nRxIds[i] == nId)
            return 0;
    if (nNumRxIds < LUA_RX_IDS)
    {
        nRxIds[nNumRxIds] = nId;
        nNumRxIds = nNumRxIds + 1; // After the id, the device thread may be reading
    }
    return 0;
}

static int TxCan(lua_State *L)
{
    lua_Integer nBus = luaL_checkinteger(L, 1);
    uint32_t nId = (uint32_t)luaL_checkinteger(L, 2);
    bool bExt = lua_toboolean(L, 3);
    luaL_checktype(L, 4, LUA_TTABLE);

    CANTxFrame tx = {};
    CanSetId(tx, nId, bExt);
    lua_Integer n = luaL_len(L, 4);
    tx.DLC = n > 8 ? 8 : (n < 0 ? 0 : n);
    for (uint8_t i = 0; i < tx.DLC; i++)
    {
        lua_geti(L, 4, i + 1);
        tx.data8[i] = (uint8_t)lua_tointeger(L, -1);
        lua_pop(L, 1);
    }

    // Same gate as the cyclic messages
    if ((nBus >= 1) && (nBus <= NUM_CAN_BUSES) && !IsParamOpInProgress() && !*pVarMap[stConfig.stDevice.nMuteCanTxInput])
        PostTxFrame(&tx, nBus - 1);
    return 0;
}

static int SetTickRate(lua_State *L)
{
    lua_Integer nHz = luaL_checkinteger(L, 1);
    nTickMs = 1000 / (nHz < 1 ? 1 : (nHz > 1000 ? 1000 : nHz));
    return 0;
}

// 32-bit Lua integers wrap like SYS_TIME, (millis() - t) stays right across the wrap
static int Millis(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)SYS_TIME);
    return 1;
}

//=============================================================================
// Protected calls: everything that can allocate runs inside lua_pcall
//=============================================================================
static int Load(lua_State *L)
{
    static const luaL_Reg libs[] = {
        {LUA_GNAME, luaopen_base}, {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string}, {LUA_MATHLIBNAME, luaopen_math}};
    for (const luaL_Reg &lib : libs)
    {
        luaL_requiref(L, lib.name, lib.func, 1);
        lua_pop(L, 1);
    }
    static const luaL_Reg api[] = {
        {"readVar", ReadVar}, {"setLuaOut", SetLuaOut}, {"canRxAdd", CanRxAdd},
        {"txCan", TxCan}, {"setTickRate", SetTickRate}, {"millis", Millis}};
    for (const luaL_Reg &fn : api)
        lua_register(L, fn.name, fn.func);

    if (luaL_loadbufferx(L, acLuaPrelude, sizeof(acLuaPrelude) - 1, "prelude", "t") != LUA_OK)
        lua_error(L);
    lua_call(L, 0, 0);

    // Source text only, a binary chunk could crash the VM
    FramReader reader = {0, *(const uint32_t *)lua_touserdata(L, 1)};
    if (lua_load(L, ReadScript, &reader, "script", "t") != LUA_OK)
        lua_error(L);
    lua_call(L, 0, 0);
    return 0;
}

static int CallTick(lua_State *L)
{
    if (lua_getglobal(L, "onTick") == LUA_TFUNCTION)
        lua_call(L, 0, 0);
    return 0;
}

static int CallCanRx(lua_State *L)
{
    const LuaRxFrame *pRx = (const LuaRxFrame *)lua_touserdata(L, 1);
    if (lua_getglobal(L, "onCanRx") != LUA_TFUNCTION)
        return 0;

    CANRxFrame f;
    CanLoad(f, pRx->slot);
    lua_pushinteger(L, pRx->nBus + 1);
    lua_pushinteger(L, CanId(f));
    lua_pushinteger(L, f.DLC);
    lua_createtable(L, 8, 0);
    for (uint8_t i = 0; i < 8; i++)
    {
        lua_pushinteger(L, f.data8[i]);
        lua_seti(L, -2, i + 1);
    }
    lua_call(L, 4, 0);
    return 0;
}

static void Run(lua_CFunction fn, const void *pArg)
{
    lua_pushcfunction(pLua, fn);
    lua_pushlightuserdata(pLua, const_cast<void *>(pArg));
    lua_sethook(pLua, Hook, LUA_MASKCOUNT, LUA_INSTR_LIMIT);
    int nRes = lua_pcall(pLua, 1, 0, 0);
    lua_sethook(pLua, nullptr, 0, 0);
    if (nRes != LUA_OK)
    {
        const char *pErr = lua_tostring(pLua, -1);
        SetMsg(pErr ? pErr : "error");
        lua_pop(pLua, 1);
        bRunning = false; // Don't retry a failing script every tick
    }
}

static void Reload()
{
    // A fresh heap and state: nothing of the previous program (globals, callbacks, CAN ids) survives
    bRunning = false;
    nNumRxIds = 0;
    LuaRxFrame *pRx;
    while (chFifoReceiveObjectTimeout(&rxFifo, (void **)&pRx, TIME_IMMEDIATE) == MSG_OK)
        chFifoReturnObject(&rxFifo, pRx);
    for (float &fOut : fLuaOut)
        fOut = 0;
    SetMsg("");
    nTickMs = 10;

    LuaHeader hdr;
    uint32_t nCrc;
    if (!fram.Read(LUA_FRAM_ADDR, (uint8_t *)&hdr, sizeof(hdr)) || (hdr.nLength == 0) || (hdr.nLength > LUA_SCRIPT_MAX))
    {
        nScriptLen = 0; // No stored program: nothing runs and the outputs stay 0
        return;
    }
    nScriptLen = hdr.nLength;

    if (!ScriptCrc(hdr.nLength, &nCrc) || (nCrc != hdr.nCrc))
    {
        SetMsg("script CRC mismatch");
        return;
    }

    chHeapObjectInit(&luaHeap, luaHeapArea, sizeof(luaHeapArea));
    pLua = lua_newstate(Alloc, nullptr, 0);
    if (!pLua)
    {
        SetMsg("not enough memory");
        return;
    }

    bRunning = true;
    Run(Load, &hdr.nLength);
    nLastTick = SYS_TIME;
}

struct LuaThread : chibios_rt::BaseStaticThread<LUA_THREAD_STACK>
{
    void main()
    {
        setName("LuaThread");

        while (true)
        {
            if (bReload)
            {
                bReload = false;
                Reload();
            }

            LuaRxFrame *pRx;
            while (bRunning && chFifoReceiveObjectTimeout(&rxFifo, (void **)&pRx, TIME_IMMEDIATE) == MSG_OK)
            {
                LuaRxFrame rx = *pRx;
                chFifoReturnObject(&rxFifo, pRx);
                Run(CallCanRx, &rx);
            }

            if (bRunning && (SYS_TIME - nLastTick) >= nTickMs)
            {
                nLastTick = SYS_TIME;
                Run(CallTick, nullptr);
            }

            chThdSleepMilliseconds(2);
        }
    }
};
static LuaThread luaThread;

void InitLua()
{
    chFifoObjectInit(&rxFifo, sizeof(LuaRxFrame), LUA_RX_QUEUE, rxFrames, rxMsgs);
    // Lowest priority: a script can never hold off the control loop, CAN or the param thread
    luaThread.start(NORMALPRIO - 2);
}

// ponytail: with the CAN filter enabled only ids that pass it reach onCanRx, add the canRxAdd ids
// to UpdateCanFilters() if scripts need foreign ids on a filtered bus
void LuaCanRx(const CANRxFrame &frame, uint8_t nBus)
{
    uint32_t nId = CanId(frame);
    for (uint8_t i = 0; i < nNumRxIds; i++)
    {
        if (nRxIds[i] != nId)
            continue;
        LuaRxFrame *pRx = (LuaRxFrame *)chFifoTakeObjectTimeout(&rxFifo, TIME_IMMEDIATE);
        if (pRx == nullptr)
            return; // Queue full, drop
        CanStore(pRx->slot, frame);
        pRx->nBus = nBus;
        chFifoSendObject(&rxFifo, pRx);
        return;
    }
}

// Upload and read back in 5 byte chunks, straight to and from FRAM. Frame layouts at MsgCmd::LuaWrite
void CheckLuaMsgs(CANRxFrame *frame)
{
    MsgCmd eCmd = static_cast<MsgCmd>(frame->data8[0]);
    if ((frame->DLC != 8) ||
        ((eCmd != MsgCmd::LuaWrite) && (eCmd != MsgCmd::LuaWriteComplete) &&
         (eCmd != MsgCmd::LuaRead) && (eCmd != MsgCmd::LuaErr)))
        return;

    CANTxFrame txMsg = {};
    CanSetId(txMsg, stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET);
    txMsg.DLC = 8;
    txMsg.data8[0] = frame->data8[0];
    txMsg.data8[1] = frame->data8[1];
    txMsg.data8[2] = frame->data8[2];
    uint16_t nOff = (frame->data8[1] << 8) | frame->data8[2];
    uint16_t nAvail;

    switch (eCmd)
    {
    case MsgCmd::LuaWrite:
        if (nOff < LUA_SCRIPT_MAX)
            fram.Write(LUA_SCRIPT_ADDR + nOff, &frame->data8[3], (LUA_SCRIPT_MAX - nOff < 5) ? LUA_SCRIPT_MAX - nOff : 5);
        txMsg.DLC = 3;
        break;

    case MsgCmd::LuaWriteComplete:
    {
        // The CRC covers what FRAM holds, a lost chunk shows up in the host's read back
        LuaHeader hdr = {(nOff < LUA_SCRIPT_MAX) ? nOff : LUA_SCRIPT_MAX, 0};
        bool bOk = ScriptCrc(hdr.nLength, &hdr.nCrc) && fram.Write(LUA_FRAM_ADDR, (uint8_t *)&hdr, sizeof(hdr));
        nScriptLen = bOk ? hdr.nLength : 0;
        txMsg.data8[3] = bOk;
        txMsg.DLC = 4;
        bReload = true;
        break;
    }

    case MsgCmd::LuaRead:
        // 0 past the end of the stored program
        nAvail = (nOff < nScriptLen) ? nScriptLen - nOff : 0;
        if (nAvail > 0)
            fram.Read(LUA_SCRIPT_ADDR + nOff, &txMsg.data8[3], nAvail < 5 ? nAvail : 5);
        break;

    default: // LuaErr
        nAvail = (nOff < strlen(acMsg)) ? strlen(acMsg) - nOff : 0;
        if (nAvail > 0)
            memcpy(&txMsg.data8[3], &acMsg[nOff], nAvail < 5 ? nAvail : 5);
        break;
    }

    PostTxFrame(&txMsg);
}
#endif
