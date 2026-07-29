#include <plugin-shared.h>
#include <D2RLPlugin/logging.h>
#include <Windows.h>
#include <cstring>
#include <utility>

namespace {

struct DeferredHookOperation {
	std::string label;
	bool required{};
	std::function<bool()> action;
};

struct HookTransactionState {
	bool collecting{};
	std::vector<DeferredHookOperation> operations;
};

HookTransactionState Transaction;

} // namespace

bool PSh_HookTransactionBegin() noexcept
{
	if (Transaction.collecting) {
		return false;
	}
	try {
		Transaction.operations.clear();
		Transaction.operations.reserve(256);
		Transaction.collecting = true;
		return true;
	}
	catch (...) {
		Transaction.operations.clear();
		Transaction.collecting = false;
		return false;
	}
}

void PSh_HookTransactionAbort() noexcept
{
	Transaction.collecting = false;
	Transaction.operations.clear();
}

bool PSh_HookTransactionIsCollecting() noexcept
{
	return Transaction.collecting;
}

bool PSh_HookTransactionEnqueue(
	const char* label,
	bool required,
	std::function<bool()> action) noexcept
{
	if (!Transaction.collecting || !label || *label == '\0' || !action) {
		return false;
	}
	try {
		Transaction.operations.push_back({ label, required, std::move(action) });
		return true;
	}
	catch (...) {
		return false;
	}
}

PSh_HookTransactionCommitResult PSh_HookTransactionCommit(
	const D2RL::PluginContext* context) noexcept
{
	PSh_HookTransactionCommitResult result{};
	if (!Transaction.collecting) {
		return result;
	}

	Transaction.collecting = false;
	auto operations = std::move(Transaction.operations);
	Transaction.operations.clear();
	result.totalOperations = operations.size();
	for (const auto& operation : operations) {
		bool installed{};
		try {
			installed = operation.action();
		}
		catch (...) {
			installed = false;
		}
		if (installed) {
			++result.completedOperations;
			continue;
		}
		if (!operation.required) {
			D2RL::LogWarnF(
				context,
				"PluginPack: optional deferred operation '%s' was refused.",
				operation.label.c_str());
			continue;
		}
		D2RL::LogErrorF(
			context,
			"PluginPack: deferred hook operation '%s' was refused after %zu/%zu operations.",
			operation.label.c_str(),
			result.completedOperations,
			result.totalOperations);
		return result;
	}

	result.success = true;
	D2RL::LogInfoF(
		context,
		"PluginPack: deferred load committed %zu/%zu operations.",
		result.completedOperations,
		result.totalOperations);
	return result;
}

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

static bool PSh_ManifestPatchCallSiteImmediate(
	const D2RL::PluginContext* context,
	uint64_t callOffset,
	const void* expected,
	uint32_t expectedSize,
	void* hookFn) noexcept
{
	if (!PSh_ValidatePluginTarget(context) || hookFn == nullptr)
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

extern "C" bool PSh_ManifestPatchCallSite(
	const D2RL::PluginContext* context,
	const char* manifestId,
	uint64_t callOffset,
	const void* expected,
	uint32_t expectedSize,
	void* hookFn) noexcept
{
	if (!PSh_ManifestSiteIsValid(
			context,
			manifestId,
			callOffset,
			expected,
			expectedSize)
		|| hookFn == nullptr) {
		return false;
	}
	if (!PSh_HookTransactionIsCollecting()) {
		return PSh_ManifestPatchCallSiteImmediate(
			context,
			callOffset,
			expected,
			expectedSize,
			hookFn);
	}

	try {
		auto expectedCopy = PSh_CopyHookBytes(expected, expectedSize);
		return PSh_HookTransactionEnqueue(manifestId, true,
			[context, callOffset, hookFn,
				expectedCopy = std::move(expectedCopy)]() noexcept {
				return PSh_ManifestPatchCallSiteImmediate(
					context,
					callOffset,
					expectedCopy.empty() ? nullptr : expectedCopy.data(),
					static_cast<uint32_t>(expectedCopy.size()),
					hookFn);
			});
	}
	catch (...) {
		return false;
	}
}
