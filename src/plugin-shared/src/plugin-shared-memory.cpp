#include <plugin-shared.h>
#include <D2RLPlugin/logging.h>
#include <Windows.h>
#include <cstring>

extern "C" void* PSh_AllocNear(void* hint, size_t size) noexcept
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	uintptr_t gran = si.dwAllocationGranularity; // typically 65536
	uintptr_t base = reinterpret_cast<uintptr_t>(hint) & ~(gran - 1);

	for (uintptr_t delta = gran; delta < 0x70000000u; delta += gran)
	{
		void* p = VirtualAlloc(reinterpret_cast<void*>(base + delta), size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (p) return p;

		if (base > delta)
		{
			p = VirtualAlloc(reinterpret_cast<void*>(base - delta), size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			if (p) return p;
		}
	}
	return nullptr;
}

extern "C" bool PSh_ManifestPatchCallSite(
	const D2RL::PluginContext* context,
	const char* manifestId,
	uint64_t callOffset,
	const void* expected,
	uint32_t expectedSize,
	void* hookFn) noexcept
{
	if (!PSh_ValidatePluginTarget(context) || manifestId == nullptr
		|| *manifestId == '\0' || hookFn == nullptr)
		return false;

	void* callSite = reinterpret_cast<void*>(context->exeBase + callOffset);

	if (expected && expectedSize > 0 && memcmp(callSite, expected, expectedSize) != 0)
	{
		D2RL::LogErrorF(context, "PSh_PatchCallSite: expected bytes mismatch at %p", callSite);
		return false;
	}

	// FF 25 00000000 <imm64> — absolute indirect jmp through the 8 bytes that follow it.
	constexpr size_t kStubSize = 14;
	void* stub = PSh_AllocNear(callSite, kStubSize);
	if (!stub)
	{
		D2RL::LogErrorF(context, "PSh_PatchCallSite: PSh_AllocNear failed for call site at %p: %lu",
			callSite, GetLastError());
		return false;
	}

	uint8_t* s = static_cast<uint8_t*>(stub);
	s[0] = 0xFF; s[1] = 0x25;
	s[2] = 0x00; s[3] = 0x00; s[4] = 0x00; s[5] = 0x00;
	*reinterpret_cast<uint64_t*>(s + 6) = reinterpret_cast<uint64_t>(hookFn);

	FlushInstructionCache(GetCurrentProcess(), stub, kStubSize);

	// Register the CALL through D2RLoader instead of writing the executable
	// directly. The loader can then reject collisions consistently and retains
	// ownership of the patch for failure/unload cleanup.
	const auto stubRva = reinterpret_cast<uint64_t>(stub) - context->exeBase;
	if (!context->PatchRel32(
			callOffset,
			expected,
			expectedSize,
			stubRva,
			5,
			D2RL::Rel32PatchKind::Call))
	{
		D2RL::LogErrorF(context, "PSh_PatchCallSite: D2RLoader rejected call site at %p", callSite);
		VirtualFree(stub, 0, MEM_RELEASE);
		return false;
	}

	// The relay intentionally remains allocated for the process lifetime. This
	// keeps an in-flight native call safe while D2RLoader owns/restores the CALL.
	return true;
}
