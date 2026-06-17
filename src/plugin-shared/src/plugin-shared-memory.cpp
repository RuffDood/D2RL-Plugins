#include "plugin.h"
#include <unordered_map>
#include "plugin-shared-private.h"
#include <Windows.h>

/**
 *	This just exists to shut D2RLoader up.
 */
static constexpr D2RLoaderPluginInfo PluginInfo{
	.apiVersion = D2RLOADER_PLUGIN_API_VERSION,
	.id = "plugin-shared",
	.name = "Plugin Shared Data",
	.version = "1.0.0",
	.author = "eezstreet",
	.flags = D2RLoaderPluginFlag_None,
};

D2RLOADER_PLUGIN_EXPORT const D2RLoaderPluginInfo* __cdecl D2RLoaderGetPluginInfo() noexcept {
	return &PluginInfo;
}

D2RLOADER_PLUGIN_EXPORT bool __cdecl D2RLoaderLoadHooks(const D2RLoaderPluginContext* context) noexcept {
	if (!context || context->apiVersion < D2RLOADER_PLUGIN_API_VERSION)
		return false;

	return true;
}

D2RLOADER_PLUGIN_EXPORT void __cdecl D2RLoaderUnload() noexcept {
}


static std::unordered_map<uint64_t, PSCodePointEntry> g_registeredEdits;
static std::unordered_map<uint64_t, PSHookEntry>      g_registeredHooks;
static uintptr_t                                       g_CachedExeBase = 0;

// Returns exeBase from context if available, otherwise falls back to cached value.
// Caches the value whenever context is live so it remains usable at unload time
// (D2RLoader passes a stack-local context to LoadHooks that may be gone by Unload).
static uintptr_t ResolveExeBase(const D2RLoaderPluginContext* context) noexcept
{
	if (context) g_CachedExeBase = context->exeBase;
	return g_CachedExeBase;
}

D2RLOADER_PLUGIN_EXPORT void* PSh_AllocNear(void* hint, size_t size) noexcept
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	uintptr_t gran = si.dwAllocationGranularity; // typically 65536
	uintptr_t base = ((uintptr_t)hint) & ~(gran - 1);

	for (uintptr_t delta = gran; delta < 0x70000000u; delta += gran)
	{
		void* p = VirtualAlloc((void*)(base + delta), size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (p) return p;

		if (base > delta)
		{
			p = VirtualAlloc((void*)(base - delta), size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			if (p) return p;
		}
	}
	return nullptr;
}

D2RLOADER_PLUGIN_EXPORT void PSh_PatchBytes(uint32_t pluginId, const D2RLoaderPluginContext* context, uint64_t offset, uint32_t length, const unsigned char* bytes) noexcept
{
	// TODO pool allocator
	PSCodePointEntry newEntry = {
		.pluginId = pluginId,
		.length = length,
	};

	void* destination = (void*)(ResolveExeBase(context) + offset);

	if (!VirtualProtect(destination, length, PAGE_EXECUTE_READWRITE, (PDWORD)&newEntry.oldProtect))
	{
		D2RPluginLogErrorF(context, "Plugin %X: Could not make writeable at %p: %d",
			pluginId, destination, GetLastError());
		return;
	}

	newEntry.originalMemory = VirtualAlloc(nullptr, newEntry.length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (newEntry.originalMemory)
	{
		memcpy(newEntry.originalMemory, destination, newEntry.length);
		memcpy(destination, bytes, newEntry.length);

		g_registeredEdits[(uint64_t)destination] = newEntry;
	}

	VirtualProtect(destination, length, newEntry.oldProtect, (PDWORD)&newEntry.oldProtect);
	FlushInstructionCache(GetCurrentProcess(), destination, newEntry.length);
}

D2RLOADER_PLUGIN_EXPORT void PSh_UnpatchBytes(uint32_t pluginId, const D2RLoaderPluginContext* context, uint64_t offset) noexcept
{
	void* destination = (void*)(ResolveExeBase(context) + offset);
	auto result = g_registeredEdits.find((uint64_t)destination);
	if (result == g_registeredEdits.end())
	{
		D2RPluginLogWarnF(context, "Plugin %X: Could not find patch %XULL to unpatch",
			pluginId, offset);
		return;
	}

	if (VirtualProtect(destination, result->second.length, PAGE_EXECUTE_READWRITE, (PDWORD)&result->second.oldProtect))
	{
		memcpy(destination, result->second.originalMemory, result->second.length);
		VirtualProtect(destination, result->second.length, result->second.oldProtect, (PDWORD)&result->second.oldProtect);
		FlushInstructionCache(GetCurrentProcess(), destination, result->second.length);
	}
	else
	{
		D2RPluginLogWarnF(context, "Plugin %X: Could not make writeable at %p: %d",
			pluginId, destination, GetLastError());
	}

	VirtualFree(result->second.originalMemory, 0, MEM_RELEASE);
	g_registeredEdits.erase((uint64_t)destination);
}

D2RLOADER_PLUGIN_EXPORT bool PSh_InstallHook(uint32_t pluginId, const D2RLoaderPluginContext* context, uint64_t offset, void* hookFn, void** originalOut, uint32_t hookSize) noexcept
{
	if (hookSize < 5 || hookSize > PSH_MAX_HOOK_SIZE)
	{
		D2RPluginLogErrorF(context, "Plugin %X: invalid hookSize %u (must be 5-%zu)",
			pluginId, hookSize, PSH_MAX_HOOK_SIZE);
		return false;
	}

	void* target = (void*)(ResolveExeBase(context) + offset);

	// Near allocation holds trampoline (hookSize bytes + E9 back) and near stub (FF 25 + addr).
	// Must be within ±2 GB of target for both E9 instructions to reach. See private header.
	void* nearMem = PSh_AllocNear(target, PSH_NEAR_ALLOC);
	if (!nearMem)
	{
		D2RPluginLogErrorF(context, "Plugin %X: PSh_AllocNear failed for hook at %p: %d",
			pluginId, target, GetLastError());
		return false;
	}

	DWORD oldProtect;
	if (!VirtualProtect(target, hookSize, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		D2RPluginLogErrorF(context, "Plugin %X: VirtualProtect failed at %p: %d",
			pluginId, target, GetLastError());
		VirtualFree(nearMem, 0, MEM_RELEASE);
		return false;
	}

	PSHookEntry entry{};
	entry.pluginId       = pluginId;
	entry.hookSize       = (uint8_t)hookSize;
	entry.trampolinePage = nearMem;
	memcpy(entry.originalBytes, target, hookSize);

	uint8_t* tram = (uint8_t*)nearMem;

	// Trampoline [nearMem+0..hookSize+4]: displaced bytes + E9 back to target+hookSize
	memcpy(tram, entry.originalBytes, hookSize);
	int32_t backRel = (int32_t)((uintptr_t)target + hookSize
	                            - ((uintptr_t)(tram + hookSize) + 5));
	tram[hookSize + 0] = 0xE9;
	memcpy(tram + hookSize + 1, &backRel, 4);

	// Near stub [nearMem+hookSize+5..+18]: FF 25 indirect jmp to hookFn
	uint8_t* stub = tram + hookSize + 5;
	stub[0] = 0xFF; stub[1] = 0x25;
	stub[2] = 0x00; stub[3] = 0x00; stub[4] = 0x00; stub[5] = 0x00;
	*(uint64_t*)(stub + 6) = (uint64_t)hookFn;

	// E9 at target: near jmp to near stub (always 5 bytes)
	int32_t fwdRel = (int32_t)((uintptr_t)stub - ((uintptr_t)target + 5));
	uint8_t patch[5] = { 0xE9 };
	memcpy(patch + 1, &fwdRel, 4);
	memcpy(target, patch, 5);
	// Zero out any bytes between E9 and hookSize (avoids stale code if hookSize > 5)
	if (hookSize > 5)
		memset((uint8_t*)target + 5, 0x90, hookSize - 5); // NOP padding

	VirtualProtect(target, hookSize, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), target, hookSize);

	*originalOut = nearMem;
	g_registeredHooks[(uint64_t)target] = entry;
	return true;
}

D2RLOADER_PLUGIN_EXPORT void PSh_RemoveHook(uint32_t pluginId, const D2RLoaderPluginContext* context, uint64_t offset) noexcept
{
	void* target = (void*)(ResolveExeBase(context) + offset);
	auto it = g_registeredHooks.find((uint64_t)target);
	if (it == g_registeredHooks.end())
	{
		D2RPluginLogWarnF(context, "Plugin %X: no hook registered at offset %llX", pluginId, offset);
		return;
	}

	uint8_t hs = it->second.hookSize;
	DWORD oldProtect;
	if (VirtualProtect(target, hs, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		memcpy(target, it->second.originalBytes, hs);
		VirtualProtect(target, hs, oldProtect, &oldProtect);
		FlushInstructionCache(GetCurrentProcess(), target, hs);
	}
	else
	{
		D2RPluginLogWarnF(context, "Plugin %X: VirtualProtect failed during unhook at %p: %d",
			pluginId, target, GetLastError());
	}

	VirtualFree(it->second.trampolinePage, 0, MEM_RELEASE);
	g_registeredHooks.erase(it);
}

D2RLOADER_PLUGIN_EXPORT bool PSh_PatchCallSite(uint32_t pluginId, const D2RLoaderPluginContext* context,
                                                uint64_t callOffset, void* hookFn) noexcept
{
	void* callSite = (void*)(ResolveExeBase(context) + callOffset);

	void* stub = PSh_AllocNear(callSite, PSH_NEAR_ALLOC);
	if (!stub)
	{
		D2RPluginLogErrorF(context, "Plugin %X: PSh_AllocNear failed for call site at %p: %d",
			pluginId, callSite, GetLastError());
		return false;
	}

	uint8_t* s = (uint8_t*)stub;
	s[0] = 0xFF; s[1] = 0x25;
	s[2] = 0x00; s[3] = 0x00; s[4] = 0x00; s[5] = 0x00;
	*(uint64_t*)(s + 6) = (uint64_t)hookFn;

	int32_t rel32 = (int32_t)((uintptr_t)stub - ((uintptr_t)callSite + 5));
	uint8_t patch[5] = { 0xE8 };
	memcpy(patch + 1, &rel32, 4);

	PSHookEntry entry{};
	entry.pluginId       = pluginId;
	entry.hookSize       = 5;
	entry.trampolinePage = stub;
	memcpy(entry.originalBytes, callSite, 5);

	DWORD oldProtect;
	if (!VirtualProtect(callSite, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		D2RPluginLogErrorF(context, "Plugin %X: VirtualProtect failed at %p: %d",
			pluginId, callSite, GetLastError());
		VirtualFree(stub, 0, MEM_RELEASE);
		return false;
	}
	memcpy(callSite, patch, 5);
	VirtualProtect(callSite, 5, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), callSite, 5);

	g_registeredHooks[(uint64_t)callSite] = entry;
	return true;
}