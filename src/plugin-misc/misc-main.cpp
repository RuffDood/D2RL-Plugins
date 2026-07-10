#include <D2RLPlugin/api.h>
#include <plugin-shared.h>
#include <plugin-shared-json.h>

// ── Addresses (offsets from exe base 0x140000000) ────────────────────────────

// Inside FUN_140188700 (/players command handler)
static constexpr uint64_t OFF_Players_AtoiCall    = 0x18885b; // E8 CALL to atoi (parses the numeric arg)
static constexpr uint64_t OFF_Players_ApplyCall   = 0x18887f; // E8 CALL to FUN_140d2f020 (applies player count)

// Callees saved so hooks can forward through
static constexpr uint64_t OFF_Players_Atoi        = 0x12da3a4; // atoi used at the /players call site
static constexpr uint64_t OFF_SetPlayerCount      = 0xd2f020;  // FUN_140d2f020(session, count)

// Expected original bytes (verified against d2r_debug_91923.exe). D2RLoader
// requires non-null expected bytes for PatchRel32 calls so it can verify the
// patch site before writing.
static constexpr uint8_t EXP_Players_AtoiCall[5]  = { 0xE8, 0x44, 0x1B, 0x15, 0x01 };
static constexpr uint8_t EXP_Players_ApplyCall[5] = { 0xE8, 0x9C, 0x67, 0xBA, 0x00 };

// ── D2R function types ────────────────────────────────────────────────────────

using PlayersAtoi_t    = int  (__fastcall*)(const char* str);
using SetPlayerCount_t = void (__fastcall*)(void* session, int count);

// ── Plugin state ──────────────────────────────────────────────────────────────

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

static constexpr D2RL::PluginInfo PluginInfo {
	.infoSize   = D2RL::PluginInfoSize,
	.apiVersion = D2RL_PLUGIN_API_VERSION,
	.id         = "eezstreet-plugin-misc",
	.name       = "eezstreet Misc Plugin",
	.version    = "2.0.0",
	.author     = "eezstreet",
	.description = "Miscellaneous changes.",
	.flags      = D2RL::PluginFlags::None,
};

D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept -> const D2RL::PluginInfo* {
	return &PluginInfo;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept -> bool {
	if (context == nullptr) {
		return false;
	}

	auto cfg = PSh_Json_LoadConfig(context);
	auto misc = PSh_Json_GetSection(cfg, "misc");
	g_PlayersCommandLimit = misc.value("playersCommandLimit", 8);

	if (g_PlayersCommandLimit > 8) {
		Real_PlayersAtoi    = reinterpret_cast<PlayersAtoi_t>(context->exeBase + OFF_Players_Atoi);
		Real_SetPlayerCount = reinterpret_cast<SetPlayerCount_t>(context->exeBase + OFF_SetPlayerCount);

		(void)context->PatchRel32(OFF_Players_AtoiCall, EXP_Players_AtoiCall, sizeof(EXP_Players_AtoiCall),
			reinterpret_cast<uint64_t>(&Hook_PlayersAtoi) - context->exeBase, 5, D2RL::Rel32PatchKind::Call);
		(void)context->PatchRel32(OFF_Players_ApplyCall, EXP_Players_ApplyCall, sizeof(EXP_Players_ApplyCall),
			reinterpret_cast<uint64_t>(&Hook_SetPlayerCount) - context->exeBase, 5, D2RL::Rel32PatchKind::Call);
	}

	return true;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderUnloadPlugin() noexcept {
	// Call-site patches installed via context->PatchRel32 are reverted automatically
	// by D2RLoader on unload (ASSUMPTION — verify against real loader behavior before
	// relying on this in production).
}
