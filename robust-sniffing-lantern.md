# Compiler-optimization audit: dingoFW

## Context

This project builds at `-O0 -ggdb ...` by default (`Makefile:18`) with `USE_LTO = yes` (`Makefile:45`). No board has ever been built at `-O2`/`-O3`. `-O0` masks two whole classes of bug that `-O2`/`-O3` (especially combined with whole-program LTO) can expose:

1. **Missing `volatile` on state shared across ChibiOS thread boundaries.** At `-O0` the compiler reloads every global from memory on every access, so a missing `volatile` is invisible. At `-O2`/`-O3` the compiler is entitled to assume single-threaded semantics for plain (non-`volatile`, non-atomic, non-mutex-protected) memory: it can cache a value in a register across a loop or a whole function, or conclude a value is invariant across the visible call graph and never re-read it. On a single-core Cortex-M there's no real memory-ordering hazard to solve (word-and-smaller aligned accesses are already atomic in hardware) — the *only* thing missing is telling the compiler "don't cache this," which is exactly what `volatile` does. This project already knows the pattern and applies it correctly in two places (`core/param_protocol.cpp`'s `g_bParamOpInProgress`/`nParamOpStartTime`, `functions/neopixels.h`'s `m_busy`) but missed it almost everywhere else that has the identical shape.
2. **Undefined behavior that happens to "work" at `-O0`** (e.g. a left-shift by an amount ≥ the operand's bit width) but that the C++ standard permits the compiler to handle in *any* way once optimizations are enabled to exploit the UB for code generation.

Three parallel research passes covered: (a) every ChibiOS thread/callback and what shared state it touches, (b) strict-aliasing/shift/uninitialized-variable/struct-packing UB, (c) busy-wait timing loops and raw register access. Every finding below was independently spot-checked by direct file reads before being included.

Two additional serious bugs surfaced during the UB pass that are **not** optimization-specific (they're live memory-safety bugs at any optimization level) — they're listed separately in Tier 3 since they're outside the literal scope of the request, but they're severe enough that you should decide explicitly whether to fix them now.

---

## Tier 1 — Missing `volatile` across thread boundaries (the core finding)

All of these are ChibiOS-thread-to-ChibiOS-thread (not ISR) sharing. Cross-referenced against the two places the codebase already does this correctly, for pattern consistency.

### 1a. ADC/DMA sample buffers — most severe instance
`adcsample_t adc1_samples[...]` (and `adc2_samples` on `canboard_v2`) is written **directly by DMA hardware** (`circular = true`, `.end_cb = NULL`, continuous conversion) with no C/C++ statement ever assigning it after `adcStartConversion()`. Not `volatile`. Declared per-board:
- `boards/dingopdm_v7/port.cpp:67,77-103,117`
- `boards/pt-dpdm4_1/port.cpp:72,83-103`
- `boards/dingopdmmax_v1/port.cpp:69,78-103`
- `boards/canboard_v2/port.cpp:67-68,70-121` (two buffers)

Read via `GetAdcRaw()` (e.g. `boards/dingopdm_v7/port.cpp:128-131`) from three separate thread contexts: `DeviceThread` (`functions/analog_input.cpp:11`, `functions/profet.cpp` current-sense), `SlowThread` (`GetBattVolt()`), and `CanCyclicTxThread` (every board's `msg.cpp` calls `GetBattVolt()`/current accessors directly for cyclic TX). Since there's no source-level write at all, this is the single easiest target for an LTO-enabled `-O2` build to treat as compile-time-invariant (worst case: every analog/battery/current reading gets constant-folded to `0`, its initializer).

### 1b. Systemic: `DeviceThread` → `CanCyclicTxThread`, the whole status layer
`CanCyclicTxThread` (`comms/can_bxcan.cpp:27-53` / `comms/can_fdcan.cpp:42-67`) runs every `CAN_TX_CYCLIC_MSG_DELAY` and calls the board's `TxMsgs[]` array (`boards/*/msg.cpp`), which is built entirely from `core/status.cpp` accessors. Every one of those accessors reads a plain member that `DeviceThread::CyclicUpdate()`/`States()` (`core/device.cpp:257-362`) is the sole writer of. None of it — not the fields, not the accessor layer — is `volatile`, mutex-, or mailbox-protected; confirmed by grep (zero `volatile` hits in `core/status.cpp` or the functional-module headers). Representative pairs (all four current boards affected identically):

| Field (written in `DeviceThread`) | Write site | Read via | Used in |
|---|---|---|---|
| `digIn[i].fVal` | `functions/digital_input.cpp:27` | `GetDigInputVal()`, `core/status.cpp:129` | `boards/*/msg.cpp` |
| `pf[i].fCurrent/eState/nOcCount` | `functions/profet.cpp:34,44,87,95,144` | `GetOutputCurrent/State/OcCount`, `core/status.cpp:45-107` | `boards/*/msg.cpp` |
| `eState`/`fState` | `core/device.cpp:184-254` | `GetDeviceState()`, `core/status.cpp:32` | `boards/*/msg.cpp` |
| `wiper.*Out`/`eState` | `functions/wiper/wiper.cpp` | `GetWiper*`, `core/status.cpp:242-260` | `boards/*/msg.cpp` |
| `flasher[i].fVal` | `functions/flasher.cpp` | `GetFlasherVal()`, `core/status.cpp:273` | `boards/*/msg.cpp` |
| `canIn[i].fOutput/fVal` | `functions/can_input.cpp` | `GetCanIn*`, `core/status.cpp:156-205` | `boards/*/msg.cpp` |
| `virtIn[i].fVal` | `functions/virtual_input.cpp` | `GetVirtIn*`, `core/status.cpp:217-234` | `boards/*/msg.cpp` |
| `counter[i].fVal` | `functions/counter.cpp` | `GetCounterVal()`, `core/status.cpp:291` | `boards/*/msg.cpp` |
| `condition[i].fVal` | `functions/condition.cpp` | `GetConditions()`, `core/status.cpp:309` | `boards/*/msg.cpp` |
| `pf[i].nDutyCycle` | `functions/pwm.cpp` | `GetOutputDC()`, `core/status.cpp:109` | `boards/*/msg.cpp` |
| `analogIn[i].*` | `functions/analog_input.cpp:12,37,49` | `GetAnalogInputMv` etc., `core/status.cpp:398-420` | `boards/pt-dpdm4_1`, `canboard_v2` |

This is the largest blast-radius item — it's not one bug, it's the whole telemetry/status path on every board.

### 1c. `SlowThread` → `DeviceThread` (original finding)
`fBattVolt`, `fTempSensor`, `bDeviceOverTemp`, `bDeviceCriticalTemp` (`core/device.cpp:63-67`) written every 250ms in `SlowThread::main()` (`core/device.cpp:102,106-108`), read every ~2ms in `DeviceThread::States()` (`core/device.cpp:172,187,218`) — including the over-temp/critical-temp safety shutdown checks. None `volatile`.

### 1d. `CanRxThread` → `DeviceThread`
`nLastCanRxTime` written in `CanRxThread` (`comms/can_bxcan.cpp:21,98`; `comms/can_fdcan.cpp:36,118`), read via `GetLastCanRxTime()` in `core/sleep.cpp:51,70` (`CheckEnterSleep()`, called from `DeviceThread`). Not `volatile`. (`core/sleep.cpp`'s own comment — "Had issue with SYS_TIME being < GetLastCanRxTime when msgs come in quickly" — describes a symptom consistent with this exact class of bug.)

### 1e. `DeviceThread` → `KeypadThread`
Keypad button LED state (`eLedOnColor`, `eLedBlinkColor`, Grayhill's `bLed[3]`) is written by `UpdateButtonLedBlinkMarine()`/equivalent inside `DeviceThread`'s `CyclicUpdate()` (`core/device.cpp:356` → `functions/keypad/keypad_button.h:48-52`), then read by `GetTxMsgBlinkMarine`/`GetTxMsgGrayhill` (`functions/keypad/blink/blink_keypad.cpp:33-72`, `functions/keypad/grayhill/grayhill_keypad.cpp:29-31`) from `KeypadThread`. Same for `pDimmingInput`, a `float*` into the shared `pVarMap[]` dereferenced from `KeypadThread` (`functions/keypad/blink/blink_keypad.cpp:106,124`). None `volatile`.

### 1f. `RequestBootloader()` — fragile, not actually protected by its own code
`boards/cortex-m4/mcu_utils.cpp:22`: `*((unsigned long *)0x2001FFF0) = 0xDEADBEEF;` through a non-`volatile` pointer, immediately followed by `NVIC_SystemReset()`. It currently survives `-O2`/`-O3` only because `NVIC_SystemReset()` inlines to code whose first statement is `__DSB()` — inline asm with a `"memory"` clobber — which incidentally prevents the compiler from proving the store is dead. That's an accident of a CMSIS header's implementation, not a guarantee; any future refactor of this 5-line function (or of `NVIC_SystemReset()` itself) could silently let dead-store elimination remove the write, which `boards/cortex-m4/enter_bootloader.S` depends on to detect "jump to DFU" after reset.

### 1g. (Informational only, not fixable at the app level) `comms/usb.cpp` `GetUsbConnected()`
Resolves to ChibiOS's own `USBD1.state`, written by the USB driver/ISR and read from `core/sleep.cpp` — not `volatile` inside ChibiOS's HAL struct either. This lives in library code; flagging for awareness only, no app-level fix proposed.

**Correct existing patterns (reference, no change needed):** `g_bParamOpInProgress`/`nParamOpStartTime` (`core/param_protocol.cpp:63-64`), `NeoPixels::m_busy` (`functions/neopixels.h:33`), and all CAN frame RX/TX handoff via `chibios_rt::Mailbox`/`Mutex` (`comms/mailbox.cpp`) — mailbox/mutex operations are real ChibiOS calls that act as compiler barriers, so nothing there needs `volatile`.

**Fix pattern:** mark each identified shared scalar `volatile`, matching the two existing correct examples. These are single-core Cortex-M, word-or-smaller, naturally-aligned values, so `volatile` alone (no atomics/mutexes) is sufficient to stop the compiler from caching/eliding the reads — it does not change runtime behavior at `-O0`, only removes the compiler's license to "optimize away" the memory traffic at higher levels.

---

## Tier 2 — Undefined behavior (shift-count ≥ width)

`functions/keypad/blink/blink_keypad.cpp`, `BuildLedMsg()` — confirmed by direct read:
- **Stacked path** (`KeypadModel::Blink12Key`, lines 30-42): three loops share one `nIndex` counter (0→35 across all three colors for a 12-button keypad); `ColorToRed/Green/Blue()` return `bool`, promoted to `int` before the `<<`. The Blue loop's last 4 iterations shift by 32-35 — shifting a promoted `int` (32-bit) by ≥32 is undefined behavior in every C++ standard, including C++20 (this project's standard, per `Makefile:30`; C++20 only removed UB for *sign-bit overflow*, not width-exceeding shift counts).
- **Padded path** (all other Blink keypad models, lines 54-73): for `Blink15Key`/`Blink15Key2Dial` (15 buttons) the Blue loop reaches `nBitPosition = 46`; for `Blink10Key` (10 buttons) it reaches 41. Both ≥32, same UB.
- **Reachable in normal operation, not a corner case:** `KeypadModel` is a fully user-writable CAN param (`core/param_defs.h:228`), and `BuildLedMsg()` runs on every cyclic keypad TX tick for any board with a Blink10Key/12Key/15Key/15Key2Dial keypad configured.
- On Cortex-M, `-O0` typically compiles the shift straight to an `LSL` with a register operand, and ARM's `LSL`/`LSR` by ≥32 architecturally yields 0 — so today this silently manifests as "some LEDs never light," easy to miss in testing. At `-O2`/`-O3` GCC is entitled to assume the shift count is always <32 for value-range propagation and dead-code elimination, so the behavior for those iterations becomes unpredictable/flag-dependent rather than a consistent "0".
- **Existing correct pattern to copy:** `functions/keypad/grayhill/grayhill_keypad.cpp:23-35` (`IndicatorMsg()`) does the equivalent bit-packing correctly — casts to `(uint64_t)` before shifting and has an explicit `if (nBitPosition >= 64) break;` guard.

---

## Tier 3 — Adjacent bugs found during the audit (not optimization-specific — flagging for a scope decision)

These are real, live memory-safety bugs independent of optimization level (they'd misbehave at `-O0` too, given the right input) — but their *blast radius* (what stack memory gets corrupted) is sensitive to how the optimizer lays out locals, which is why they surfaced during this pass. Listed separately since they're outside the literal "compiler optimization" scope of the request.

### 3a. `comms/usb.cpp` — uninitialized `CANRxFrame` + unchecked DLC → stack OOB write
Confirmed by direct read (`comms/usb.cpp:397-432,434-484`):
- `Parse()` only writes `*frame` inside `if (data[0] == 't')` — no `else`, no return status.
- `UsbRxThread` declares `CANRxFrame msg;` with no initializer (line 439), calls `Parse(rxBuf, rxIndex, &msg)` unconditionally (line 468), then unconditionally reads `msg.SID`/`msg.DLC`/`msg.data8[]` (lines 476-483) even when `Parse()` never touched `msg` (any USB line not starting with `'t'`) — a genuine uninitialized-read.
- Even on the `'t'`-prefixed path, `frame->DLC = data[4];` (line 419) is a raw ASCII-hex-decoded nibble with no `> 8` clamp. The following copy loop (`for (i < frame->DLC) frame->data8[i] = ...`, lines 423-427, and again at `canTx.data8[i] = msg.data8[i]`, lines 482-483) then writes past the 8-byte `data8[8]` array for any DLC of 9-15 — an out-of-bounds stack write in the 1024-byte `waUsbRxThread` thread area.

### 3b. `utils/dbc.cpp` `DecodeBE`/`EncodeBE` — unchecked bit-walk past the 8-byte CAN payload
`nStartBit` and `nBitLength` are each independently range-checked by the param protocol (`core/param_defs.h`), but their *combination* isn't: the big-endian byte-walk in `DecodeBE`/`EncodeBE` (`utils/dbc.cpp:68-97,130-156`) increments `byteIndex` with no `< 8` ceiling. This is live on every current board today: the hard-coded `Dbc::EncodeFloat(stMsg.frame.data8, val, 32, 32, ...)` call in every board's cyclic status message (e.g. `boards/dingopdm_v7/msg.cpp`) walks `byteIndex` up to 8 (one past the end of `data8[8]`) whenever that CAN input/output's `eByteOrder` param is set to `ByteOrder::BigEndian` — a fully user-writable setting, reachable with no crafted input. `functions/can_outputs.cpp`'s `CalcDlc()` (lines 112-118) has the same missing ceiling and compounds it.

---

## Disposition

This was requested and scoped as a **report only — no code changes in this session**. Nothing above has been modified; this document is the complete deliverable.

For if/when fixes are done later, the stated preference is recorded here so it isn't re-litigated:
- **Tier 1** (missing `volatile`): full sweep — mark every identified field in 1a-1f `volatile` in one pass (matching the existing `g_bParamOpInProgress`/`m_busy` pattern), not just the highest-risk subset.
- **Tier 2** (shift UB in `BuildLedMsg()`): fix by building the mask in a `uint64_t` and shifting by a width-safe amount, mirroring `grayhill_keypad.cpp`'s `IndicatorMsg()` (which already has the correct `uint64_t` cast + `>= 64` guard pattern).
- **Tier 3** (USB uninitialized-read/OOB write, DBC big-endian OOB bit-walk): explicitly out of scope for now — not optimization-specific, left for the user to track/act on separately.

## Suggested verification, once fixes are made

- Build every board (`BOARD=canboard_v2|dingopdm_v7|dingopdmmax_v1|pt-dpdm4_1 make`) at the existing default `-O0` first to confirm no behavior change, then re-build with `USE_OPT` overridden to `-O2` (e.g. `make USE_OPT="-O2 -ggdb -fomit-frame-pointer -falign-functions=16 -fsingle-precision-constant"`) for all four boards to confirm they still build clean with `-Wall -Wextra -Werror=shadow`.
- No hardware-in-the-loop test is available in this environment; bench-test at least one board at `-O2` afterward (battery/temp reporting over time, keypad LED colors on a Blink12Key/15Key unit, sleep/wake behavior, USB<->CAN passthrough) before shipping an optimized build.
