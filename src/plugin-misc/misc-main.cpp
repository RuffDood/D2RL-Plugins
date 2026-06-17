#include "plugin.h"
#include <plugin-shared.h>

// ── Addresses (offsets from exe base 0x140000000) ────────────────────────────

// Inside FUN_14013cc10 (/players command handler)
static constexpr uint64_t OFF_Players_AtoiCall    = 0x13d45d; // E8 CALL to atoi (parses the numeric arg)
static constexpr uint64_t OFF_Players_ApplyCall   = 0x13d47e; // E8 CALL to FUN_1408082b0 (applies player count)

// Callees saved so hooks can forward through
static constexpr uint64_t OFF_Players_Atoi        = 0x1581a50; // atoi used at the /players call site
static constexpr uint64_t OFF_SetPlayerCount      = 0x8082b0;  // FUN_1408082b0(session, count)

// ── D2R function types ────────────────────────────────────────────────────────

using PlayersAtoi_t    = int  (__fastcall*)(const char* str);
using SetPlayerCount_t = void (__fastcall*)(void* session, int count);

// ── Plugin state ──────────────────────────────────────────────────────────────

static constexpr const wchar_t* MiscPluginSection = L"PluginPack.Misc";

static int                 g_PlayersCommandLimit = 8;
static int                 g_LastPlayersArg      = 1;
static PlayersAtoi_t       Real_PlayersAtoi      = nullptr;
static SetPlayerCount_t    Real_SetPlayerCount   = nullptr;

// ── Hook: atoi call site inside /players handler ──────────────────────────────
// Captures the raw numeric argument before the game's [1,8] ceiling clamp runs.

static int __fastcall Hook_PlayersAtoi(const char* str) {
	int val = Real_PlayersAtoi(str);
	g_LastPlayersArg = val;
	return val;
}

// ── Hook: FUN_1408082b0 call site inside /players handler ─────────────────────
// Replaces the already-clamped count (EDX ≤ 8) with the raw value re-clamped
// to [1, PlayersCommandLimit].

static void __fastcall Hook_SetPlayerCount(void* session, int /*count*/) {
	int clamped = g_LastPlayersArg;
	if (clamped < 1) clamped = 1;
	if (clamped > g_PlayersCommandLimit) clamped = g_PlayersCommandLimit;
	Real_SetPlayerCount(session, clamped);
}

// ── Plugin info ───────────────────────────────────────────────────────────────

static constexpr D2RLoaderPluginInfo PluginInfo {
	.apiVersion = D2RLOADER_PLUGIN_API_VERSION,
	.id         = "plugin-misc",
	.name       = "Misc Plugin",
	.version    = "1.0.0",
	.author     = "eezstreet",
	.flags      = D2RLoaderPluginFlag_None,
};

D2RLOADER_PLUGIN_EXPORT const D2RLoaderPluginInfo* __cdecl D2RLoaderGetPluginInfo() noexcept {
	return &PluginInfo;
}

D2RLOADER_PLUGIN_EXPORT bool __cdecl D2RLoaderLoadHooks(const D2RLoaderPluginContext* context) noexcept {
	if (!context || context->apiVersion < D2RLOADER_PLUGIN_API_VERSION) {
		return false;
	}

	g_PlayersCommandLimit = PSh_Ini_GetInt(context, MiscPluginSection, L"PlayersCommandLimit", 8);

	if (g_PlayersCommandLimit > 8) {
		Real_PlayersAtoi    = reinterpret_cast<PlayersAtoi_t>(context->exeBase + OFF_Players_Atoi);
		Real_SetPlayerCount = reinterpret_cast<SetPlayerCount_t>(context->exeBase + OFF_SetPlayerCount);

		PSh_PatchCallSite(PLUGINID_MISC, context, OFF_Players_AtoiCall,  reinterpret_cast<void*>(Hook_PlayersAtoi));
		PSh_PatchCallSite(PLUGINID_MISC, context, OFF_Players_ApplyCall, reinterpret_cast<void*>(Hook_SetPlayerCount));
	}

	return true;
}

D2RLOADER_PLUGIN_EXPORT void __cdecl D2RLoaderUnload() noexcept {
	PSh_RemoveHook(PLUGINID_MISC, nullptr, OFF_Players_AtoiCall);
	PSh_RemoveHook(PLUGINID_MISC, nullptr, OFF_Players_ApplyCall);
}
