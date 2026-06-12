#pragma once
#include <plugin.h>
#include <cstdint>

#define PLUGINID_ITEMS	0xEE000001
#define PLUGINID_LEVELS 0xEE000002
#define PLUGINID_MISC   0xEE000003
#define PLUGINID_QUESTS 0xEE000004
#define PLUGINID_SKILLS 0xEE000005

// ── D2R datatable types ───────────────────────────────────────────────────────

// Descriptor for one column in a D2R .txt data table (32 bytes).
// Passed as an array (null-pName terminated) to DATATBLS_CompileTxt.
// For bit-field columns (boolean flags packed into a uint64_t):
//   offset = byte offset of the containing uint64_t in the record
//   count  = bit index within that uint64_t (0 = LSB)
//   type   = determined at runtime by scanning existing bool descriptors
struct D2TxtFieldDesc {
	const char* pName;   // +0x00  column header (nullptr = array terminator)
	uint32_t    type;    // +0x08  1 = char[], 2 = int/short, 3+ = bit-field (see above)
	uint32_t    count;   // +0x0c  strings: max chars; bit-fields: bit index; ints: 0
	uint64_t    offset;  // +0x10  byte offset of field within the record struct
	uint64_t    _pad;    // +0x18  always 0
};
static_assert(sizeof(D2TxtFieldDesc) == 32, "D2TxtFieldDesc must be 32 bytes");

// Receives the compiled record array after a DATATBLS_CompileTxt call.
struct D2TxtDataArea {
	void*    pRecords;  // heap pointer to record array (game-owned)
	uint64_t nCount;    // number of compiled records
	uint64_t flags;     // high bit set = uses external/inline storage
};

// Wraps D2TxtDataArea with a vtable for internal heap allocation.
// Set vtable = exeBase + 0x16df480 (reuse the game's container vtable).
struct D2TxtContainer {
	void*          vtable;
	D2TxtDataArea* pData;
};

// ── Memory utilities ──────────────────────────────────────────────────────────

// Allocates `size` bytes of PAGE_EXECUTE_READWRITE memory within ±2 GB of `hint`.
// Required for near stubs that redirect 5-byte relative CALL/JMP instructions.
// Returns nullptr on failure. Free with VirtualFree(ptr, 0, MEM_RELEASE).
D2RLOADER_PLUGIN_EXPORT void* PSh_AllocNear(void* hint, size_t size) noexcept;

// ── Byte patching ─────────────────────────────────────────────────────────────

D2RLOADER_PLUGIN_EXPORT void PSh_PatchBytes(uint32_t pluginId, const D2RLoaderPluginContext* context, uint64_t offset, uint32_t length, unsigned char* bytes) noexcept;
D2RLOADER_PLUGIN_EXPORT void PSh_UnpatchBytes(uint32_t pluginId, const D2RLoaderPluginContext* context, uint64_t offset) noexcept;

// ── Function hooks ────────────────────────────────────────────────────────────

// Installs a 5-byte (default) or 6-byte E9 near-jmp hook at (exeBase + offset).
// The first `hookSize` bytes at the target must be complete, relocatable instructions
// (no RIP-relative addressing). Writes a near allocation containing the displaced
// trampoline and an FF25 stub, then patches the target with E9 to the stub.
// *originalOut is set to the trampoline — call it to execute the original function.
// Use hookSize=6 when the first two instructions together span 6 bytes
// (e.g. push rbx [2] + sub rsp,N [4]). Verify in Ghidra before changing.
D2RLOADER_PLUGIN_EXPORT bool PSh_InstallHook(uint32_t pluginId, const D2RLoaderPluginContext* context, uint64_t offset, void* hookFn, void** originalOut, uint32_t hookSize = 5) noexcept;
D2RLOADER_PLUGIN_EXPORT void PSh_RemoveHook(uint32_t pluginId, const D2RLoaderPluginContext* context, uint64_t offset) noexcept;