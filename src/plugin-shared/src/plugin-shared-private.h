#pragma once

#include <plugin.h>

struct PSCodePointEntry {
	uint32_t pluginId;
	uint64_t length;
	void* originalMemory;
	uint32_t oldProtect;
};

/*
 * Hook mechanism: 5-byte E9 near-jmp + near allocation
 *
 * Layout at target function entry after hook is installed:
 *
 *   [target+0]  E9 xx xx xx xx   <- near jmp to near_stub (overwrites 5 original bytes)
 *   [target+5]  (original code continues, untouched)
 *
 * Near allocation (PSh_AllocNear, within ±2 GB of target):
 *
 *   [nearMem+0]   <5 original bytes>          <- trampoline: displaced prologue
 *   [nearMem+5]   E9 xx xx xx xx              <- trampoline: near jmp back to target+5
 *   [nearMem+10]  FF 25 00 00 00 00           <- near stub: RIP-relative indirect jmp
 *   [nearMem+16]  <8-byte address of hookFn>  <- near stub: abs target
 *
 * *originalOut is set to nearMem (the trampoline). Calling it from inside your
 * hook function executes the displaced prologue bytes and then falls through to
 * target+5, continuing the original function as normal.
 *
 * Why 5 bytes / E9 (not a 14-byte FF 25 abs-jmp at the target):
 *   A 14-byte overwrite at the target almost always splits an instruction boundary
 *   in MSVC x64 prologues. A 5-byte E9 fits cleanly inside the first instruction of
 *   any function that begins with a standard prologue save (e.g. mov [rsp+8], rbx
 *   = 48 89 5C 24 08, exactly 5 bytes). The indirect jmp for the hook itself lives
 *   in the near allocation, so there is no ±2 GB limitation on hookFn's address.
 *
 * Constraints:
 *   - The first N bytes of the target function must be N complete, relocatable
 *     instructions (no RIP-relative addressing). Verify in Ghidra before hooking.
 *     Typical MSVC x64 prologues use register saves (push rbx, etc.) and stack
 *     adjustments (sub rsp, N) — both are safe to displace.
 *   - PSh_AllocNear must find memory within ±2 GB of the target (true for any D2R
 *     code section since the exe fits in a 4 GB range).
 *   - The trampoline E9 back to target+N also requires nearMem within ±2 GB of
 *     target+N — satisfied by the same PSh_AllocNear call.
 *   - Pass hookSize to PSh_InstallHook. Default is 5. Use 6 when the first two
 *     instructions span 6 bytes (e.g. push rbx [2] + sub rsp,N [4]).
 */
static constexpr size_t PSH_MAX_HOOK_SIZE = 8;  // max supported hook displacement
static constexpr size_t PSH_NEAR_ALLOC    = 32; // bytes in the near allocation

struct PSHookEntry {
	uint32_t pluginId;
	uint8_t  hookSize;                         // bytes overwritten at the target
	uint8_t  originalBytes[PSH_MAX_HOOK_SIZE]; // displaced prologue bytes
	void*    trampolinePage;                   // the near allocation (trampoline + near stub)
};
