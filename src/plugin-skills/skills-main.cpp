#include <D2RLPlugin/api.h>
#include "plugin-shared.h"
#include "skills-private.h"
#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <vector>

// ── D2R function type aliases ─────────────────────────────────────────────────

using CompileTxt_t    = void(__fastcall*)(uint8_t context, const char* txtName,
                                          const char* binName, const char* param4,
                                          D2TxtFieldDesc* fields, uint64_t recordSize,
                                          D2TxtContainer* output);
using Consume_t       = int64_t(__fastcall*)(int64_t unit, int* playerUnit,
                                             int skillId, int skillLevel);
using DrainStat_t     = void(__fastcall*)(void* unit, int statId, int delta);
using GetManaCost_t   = int(__fastcall*)(uint8_t unitType, int skillId, int skillLevel);
using CheckStat_t     = bool(__fastcall*)(int* playerUnit, int64_t* skillStruct,
                                          int param3, int currentMana);
using ClientPredict_t = void(__fastcall*)(int* playerUnit, int skillId, int skillLevel);
using GetSkillLevel_t = int(__fastcall*)(int* playerUnit, int64_t* skillStruct, int param3);
using GetMaxSkillLevelForContext_t = int(__fastcall*)(uint8_t context, uint32_t index);

// ── Addresses (offsets from exe base 0x140000000) ────────────────────────────

static constexpr uint64_t OFF_CompileSkillsTxt = 0x302380; // DATATBLS_CompileSkillsTxt
static constexpr uint64_t OFF_CompileTxt       = 0x2ff970; // DATATBLS_CompileTxt
// OFF_Consume was a transcription bug for a long time (missing digit: 0x36830
// instead of 0x436830), which meant InstallInlineHook was hooking into the
// middle of an unrelated static-initializer function. Verified against
// debug.exe via decompile: FUN_140436830 embeds the literal source path
// "...\Skills\Skills.cpp" and calls D2Common_SKILLMANA_GetManaCost then
// D2GAME_SKILLS_BloodMana_6FD025E0, matching profile's
// D2GAME_SKILLMANA_Consume_6FD10A50 exactly.
//
// AuraConsume and ConsumeWeaponCharge are both **fully inlined** into this
// debug function with no standalone call boundary at all (confirmed via full
// decompile: the charge-drain logic and the BloodMana-or-mana-drain logic are
// both directly inline in FUN_140436830's body). The OFF_AuraConsume/
// OFF_ConsumeWeaponCharge constants that used to exist here were stale
// **profile.exe** addresses (exact matches to profile's named
// D2GAME_SKILLMANA_AuraConsume_6FD10C90/ConsumeWeaponCharge functions) that
// would have InstallInlineHook'd garbage in debug.exe — removed. See
// Hook_Consume for how the ManaCostsLife/Stamina redirect (AuraConsume's old
// job) is now reimplemented directly against the merged function via a
// before/after mana-stat delta, which needs no knowledge of Consume's
// internal charge-vs-mana branching. ChargedPctDrainStat (ConsumeWeaponCharge's
// old job) could not be safely reimplemented the same way — see the comment
// on bEnableChargedPctDrainStat's handling below.
static constexpr uint64_t OFF_Consume          = 0x436830;  // D2GAME_SKILLMANA_Consume (AuraConsume/ConsumeWeaponCharge both inlined here)
static constexpr uint64_t OFF_DrainStat        = 0x2f34f0; // FUN_140227470
static constexpr uint64_t OFF_GetManaCost      = 0x33aa00; // D2Common_SKILLMANA_GetManaCost (real standalone function, not inlined)
static constexpr uint64_t OFF_CheckStat        = 0x340900; // D2Common_SKILLMANA_CheckStat (real standalone function; debug build only reads 2 args, recomputes stat 6/8 internally instead of taking them as params 3/4 like profile does -- verify CheckStat_t/Hook_CheckStat before relying on param3/param4)
static constexpr uint64_t OFF_ClientPredict    = 0x2188b0; // FUN_140197080 (client-side mana prediction)
static constexpr uint64_t OFF_GetUseState      = 0x33f360; // SKILLS_GetUseState_6FDB0B70
static constexpr uint64_t OFF_GetUseState_Call = 0x21b180; // call site inside D2CLIENT_GetUnusableUseState
static constexpr uint64_t OFF_ClassicWW        = 0x5691e2;
// OFF_EnableWWCtC's *real* identity was misleading in the old naming: the
// patch site is NOT inside Whirlwind's own attack loop. It's a JNZ bail-out
// guard inside the generic "chance to cast on hit/attack" event caster.
// Confirmed via the "OnHitOrAttack" string literal (unique in both binaries):
// that string anchors profile's SUNIT_EvFunc_ItemApplyHitOrAttack, which --
// when the CtC roll succeeds -- calls SKILLS_SrvDo114_NecDoBoneSpear (a
// misleadingly-named shared "cast the proc'd skill" routine, not actually
// Bone Spear-specific) to perform the cast. Debug's counterpart, found via
// the same string anchor -> FUN_140583b30 -> FUN_1405896e0, refactored the
// old inline `CMP [table+0x298],0x36 / TEST [state+0xaf4],0x400000` guard
// into a real call: `MOV EDX,0x36; CALL FUN_1403351b0; TEST EAX,EAX; JNZ`
// (FUN_1403351b0 -> FUN_1402f6220 turns out to be a generic
// "does unit have bit-flag N set" state test, called here with N=0x36/54 --
// matches the profile bit position 0xaf4*8+22=0x436, i.e. bit 54, exactly).
// NOPing this JNZ (same 6-byte length as profile's near JNZ) forces the
// cast to always proceed regardless of that state, exactly mirroring
// profile's patch semantics one-for-one.
static constexpr uint64_t OFF_EnableWWCtC      = 0x589736;
static constexpr uint64_t OFF_Telekinesis      = 0x554936;
static constexpr uint64_t OFF_GetSkillLevel    = 0x3400a0; // SKILLS_GetSkillLevel (profile 0x264b20)
static constexpr uint64_t OFF_GetMaxSkillLevelForContext = 0x300c70; // debug-only helper; profile inlines this as a raw D2GAME_sgptDataTables[ctx*2]+0x14e0 double-deref, no separate profile function exists

// ── Expected original bytes (verified against d2r_debug_91923.exe) ───────────
// D2RLoader requires non-null expected bytes for PatchBytes/PatchRel32/
// InstallInlineHook calls so it can verify the patch site before writing.
static constexpr uint8_t EXP_CompileTxtCallOffsets[][5] = {
	{ 0xE8, 0xBB, 0x8D, 0xFF, 0xFF },
	{ 0xE8, 0x4F, 0x8C, 0xFF, 0xFF },
	{ 0xE8, 0x1A, 0x8B, 0xFF, 0xFF },
	{ 0xE8, 0xCD, 0x89, 0xFF, 0xFF },
};
static constexpr uint8_t EXP_Consume[16] = {
	0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x18, 0x56, 0x57, 0x41, 0x56, 0x48, 0x83,
};
static constexpr uint8_t EXP_CheckStat[14] = {
	0x40, 0x55, 0x56, 0x41, 0x54, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x28, 0x45, 0x33, 0xC0,
};
static constexpr uint8_t EXP_ClientPredict[20] = {
	0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x18,
	0x48, 0x89, 0x74, 0x24, 0x20, 0x57, 0x48, 0x83, 0xEC, 0x20,
};
static constexpr uint8_t EXP_GetUseState_Call[5] = { 0xE8, 0xDB, 0x41, 0x12, 0x00 };
static constexpr uint8_t EXP_ClassicWW[5]        = { 0xE8, 0x19, 0x1A, 0x00, 0x00 };
static constexpr uint8_t EXP_EnableWWCtC[6]      = { 0x0F, 0x85, 0xC1, 0x00, 0x00, 0x00 };
static constexpr uint8_t EXP_Telekinesis[5]      = { 0xE8, 0x35, 0xA8, 0xDE, 0xFF };

// skills.txt record layout constants
static constexpr uint64_t SKILLS_RECORD_STRIDE = 0x2EC;  // bytes per record
static constexpr uint64_t SKILLS_FLAGS_OFFSET  = 0x24;   // uint64_t flags QWORD
static constexpr uint32_t SKILLSRECORD_TYPE_BOOL = 29;
static constexpr int      MANA_COSTS_LIFE_BIT    = 47;   // first free bit
static constexpr int      MANA_COSTS_STAMINA_BIT = 48;   // second free bit

// ── Plugin state ──────────────────────────────────────────────────────────────

static SkillPluginOptions            g_skillPluginOptions {};
static uintptr_t                     g_ExeBase  = 0;

// Compiled skills.txt records (game-owned memory — do not free).
static void*    g_SkillsRecords = nullptr;
static uint64_t g_SkillsCount  = 0;

using GetUseState_t    = int(__fastcall*)(int* playerUnit, int64_t* pSkill);
using NeedManaSound_t  = int(__fastcall*)(int* skillEntity, uint32_t* outPriority);
using DiagB2300_t      = void(__fastcall*)(void* param_1, void* param_2);
using DiagDaf0_t       = void(__fastcall*)(int* param_1, void* param_2, uint8_t param_3, char param_4, uint32_t* param_5);
using PlaySoundEffect_t = int64_t(__fastcall*)(int soundId, int* entity, int param3, int param4, int param5);

static Consume_t       Original_Consume       = nullptr;
static CheckStat_t     Original_CheckStat     = nullptr;
static ClientPredict_t Original_ClientPredict = nullptr;
static void* Original_CanBePickedUpWithTelekinesis = nullptr;

// ── Bit-field descriptor injection ───────────────────────────────────────────

// Scans origFields for a boolean field packed into the flags QWORD (offset == SKILLS_FLAGS_OFFSET)
// to infer the game's bit-field type code. We rely on count encoding the bit index (0 = LSB).
static uint32_t DetectBoolType(D2TxtFieldDesc* origFields) {
	for (int i = 0; origFields[i].pName && std::strcmp(origFields[i].pName, "end") != 0; ++i) {
		if (origFields[i].offset == SKILLS_FLAGS_OFFSET)
			return origFields[i].type;
	}
	return 0;
}

// Descriptors for ManaCostsLife and ManaCostsStamina — filled in once we know g_BoolFieldType.
static D2TxtFieldDesc g_ManaCostsLifeDesc = { "ManaCostsLife", SKILLSRECORD_TYPE_BOOL, MANA_COSTS_LIFE_BIT, SKILLS_FLAGS_OFFSET, 0 };
static D2TxtFieldDesc g_ManaCostsStaminaDesc = { "ManaCostsStamina", SKILLSRECORD_TYPE_BOOL, MANA_COSTS_STAMINA_BIT, SKILLS_FLAGS_OFFSET, 0 };

// ── Hook: replacement for DATATBLS_CompileTxt call inside CompileSkillsTxt ───
//
// Called instead of the real CompileTxt whenever DATATBLS_CompileSkillsTxt would
// have called it. For the "skills" table, appends ManaCostsLife to the descriptor
// array and saves the resulting records pointer for the mana hooks.

void __fastcall Hook_CompileTxt_Call(uint8_t context, const char* txtName,
                                      const char* binName, const char* param4,
                                      D2TxtFieldDesc* origFields, uint64_t recordSize,
                                      D2TxtContainer* output)
{
	auto RealCompileTxt = (CompileTxt_t)(g_ExeBase + OFF_CompileTxt);

	if (std::strcmp(txtName, "skills") != 0) {
		// Not the skills table (e.g. skilldesc) — pass through unchanged.
		RealCompileTxt(context, txtName, binName, param4, origFields, recordSize, output);
		return;
	}

	// Find the "end" sentinel (type=0, pName="end") that terminates the descriptor list.
	// Insert our field immediately before it so the compiler sees it.
	int n = 0;
	while (origFields[n].pName && std::strcmp(origFields[n].pName, "end") != 0) ++n;

	std::vector<D2TxtFieldDesc> extended(origFields, origFields + n);
	if (g_skillPluginOptions.bEnableManaCostsLife)    extended.push_back(g_ManaCostsLifeDesc);
	if (g_skillPluginOptions.bEnableManaCostsStamina) extended.push_back(g_ManaCostsStaminaDesc);
	extended.push_back(origFields[n]);   // preserve the "end" sentinel

	RealCompileTxt(context, txtName, binName, param4,
	               extended.data(), recordSize, output);

	// Save records for SkillManaCostsLife() lookups.
	if (output && output->pData) {
		g_SkillsRecords = output->pData->pRecords;
		g_SkillsCount   = output->pData->nCount;
	}
}

static bool SkillRecManaCostsLife(const uint8_t* rec)
{
	return ((*reinterpret_cast<const uint64_t*>(rec + SKILLS_FLAGS_OFFSET)) >> MANA_COSTS_LIFE_BIT) & 1;
}

static bool SkillManaCostsLife(int skillId) noexcept {
	if (!g_SkillsRecords || skillId < 0 || static_cast<uint64_t>(skillId) >= g_SkillsCount)
		return false;
	const uint8_t* rec = static_cast<const uint8_t*>(g_SkillsRecords)
	                     + static_cast<uint64_t>(skillId) * SKILLS_RECORD_STRIDE;
	return SkillRecManaCostsLife(rec);

}

static bool SkillRecManaCostsStamina(const uint8_t* rec)
{
	return ((*reinterpret_cast<const uint64_t*>(rec + SKILLS_FLAGS_OFFSET)) >> MANA_COSTS_STAMINA_BIT) & 1;
}

static bool SkillManaCostsStamina(int skillId) noexcept {
	if (!g_SkillsRecords || skillId < 0 || static_cast<uint64_t>(skillId) >= g_SkillsCount)
		return false;
	const uint8_t* rec = static_cast<const uint8_t*>(g_SkillsRecords)
	                     + static_cast<uint64_t>(skillId) * SKILLS_RECORD_STRIDE;
	return SkillRecManaCostsStamina(rec);
}

// ── Hook: D2Common_SKILLMANA_CheckStat ───────────────────────────────────────
// Called by GetUseState to decide if the skill can be cast (returns false → red orb).
//
// The profile build took the current stat value as an explicit param4, so the
// original approach here was to just substitute life/stamina for mana and let
// Original_CheckStat do the comparison. That trick does NOT work against the
// debug build: its CheckStat ignores param3/param4 entirely and always
// re-derives both mana (stat 8) and life (stat 6) itself via direct GetStat
// calls, picking between them based on its own internal "Blood Mana" state
// flag -- passing a substituted currentAlt through would silently be ignored.
//
// Fix: when a skill's cost is redirected to life/stamina, bypass
// Original_CheckStat entirely and replicate its level/cost computation
// ourselves (decompile-verified against debug FUN_140340900), evaluated
// against the correct alternate stat instead of mana.
bool __fastcall Hook_CheckStat(int* playerUnit, int64_t* skillStruct,
                                int param3, int currentMana)
{
    if (playerUnit && skillStruct && g_SkillsRecords) {
        auto recPtr  = *reinterpret_cast<const uintptr_t*>(skillStruct);
        auto base    = reinterpret_cast<uintptr_t>(g_SkillsRecords);
        auto byteOff = recPtr - base;
        if (recPtr >= base && byteOff < g_SkillsCount * SKILLS_RECORD_STRIDE
                           && byteOff % SKILLS_RECORD_STRIDE == 0) {
            int skillId = static_cast<int>(byteOff / SKILLS_RECORD_STRIDE);
            int altStatId = 0;
            if      (SkillManaCostsLife(skillId))    altStatId = 6;  // life
            else if (SkillManaCostsStamina(skillId)) altStatId = 10; // stamina
            if (altStatId) {
                auto* unitStrc = reinterpret_cast<D2UnitStrc*>(playerUnit);
                if (unitStrc->statList) {
                    // Replicate CheckStat's own level derivation: base level
                    // (skillStruct+0x40) + bonus levels, clamped to [0, maxLevel].
                    auto GetSkillLevel = reinterpret_cast<GetSkillLevel_t>(g_ExeBase + OFF_GetSkillLevel);
                    int64_t baseLevel  = skillStruct[8]; // offset 0x40
                    int bonusLevel     = GetSkillLevel(playerUnit, skillStruct, 0);
                    int level = static_cast<int>(baseLevel) + bonusLevel;
                    if (level < 0) level = 0;

                    auto GetMaxSkillLevel = reinterpret_cast<GetMaxSkillLevelForContext_t>(g_ExeBase + OFF_GetMaxSkillLevelForContext);
                    int maxLevel = GetMaxSkillLevel(unitStrc->itemTableEntry, 0);
                    if (level > maxLevel) level = maxLevel;

                    auto GetManaCost = reinterpret_cast<GetManaCost_t>(g_ExeBase + OFF_GetManaCost);
                    int manaCost = GetManaCost(unitStrc->itemTableEntry, skillId, level);

                    int currentAlt = PSh_GetStat(g_ExeBase, unitStrc->statList, altStatId);
                    return currentAlt >= manaCost;
                }
            }
        }
    }
    return Original_CheckStat(playerUnit, skillStruct, param3, currentMana);
}

// ── Hook: FUN_140197080 (client-side mana prediction) ────────────────────────
// Called client-side (local player only) to optimistically drain mana before the
// server confirms the cast. Without this hook, the client pre-drains mana, the
// server drains life (our hook), and the server's authoritative stat packet snaps
// the display back — visible rubber-banding for ~1 server tick.
// Fix: drain life immediately on the client to match what the server will do.
// We skip the ring-buffer that the original writes (it's for mana reconciliation
// only); accepting a possible sub-tick snap is far less noticeable than a full
// mana rubber-band.

void __fastcall Hook_ClientPredict(int* playerUnit, int skillId, int skillLevel)
{
    if (*playerUnit == 0 && g_SkillsRecords) {
        int altStatId = 0;
        if      (SkillManaCostsLife(skillId))    altStatId = 6;
        else if (SkillManaCostsStamina(skillId)) altStatId = 10;

        if (altStatId) {
            auto* unitStrc   = reinterpret_cast<D2UnitStrc*>(playerUnit);
            auto GetManaCost = reinterpret_cast<GetManaCost_t>(g_ExeBase + OFF_GetManaCost);
            int manaCost = GetManaCost(unitStrc->itemTableEntry, skillId, skillLevel);
            if (manaCost >= 1 && unitStrc->statList) {
                auto DrainStat = reinterpret_cast<DrainStat_t>(g_ExeBase + OFF_DrainStat);
                int currentAlt = PSh_GetStat(g_ExeBase, unitStrc->statList, altStatId);
                if (currentAlt >= manaCost)
                    DrainStat(playerUnit, altStatId, -manaCost);
            }
            return;
        }
    }
    Original_ClientPredict(playerUnit, skillId, skillLevel);
}

// ── Hook: D2GAME_SKILLMANA_Consume ───────────────────────────────────────────
// AuraConsume and ConsumeWeaponCharge are both fully inlined into this
// function in the debug build (no standalone call boundary survives — see the
// comment by OFF_Consume's declaration), so both of their old jobs are
// reimplemented here directly instead of via separate inline hooks.
//
// ManaCostsLife/ManaCostsStamina redirect (AuraConsume's old job): rather than
// replicating Consume's internal charge-vs-mana-vs-BloodMana branching
// ourselves (which would require trusting an unverified multi-level pointer
// chain read from the profile decompile — playerUnit+0x100 -> +0x18 ->
// skill struct -> skill id / owner GUID — with no independent confirmation
// available, and a wrong hop there is a crash, not a bug), we let the real
// engine decide and react to the *result*: read mana before and after calling
// Original_Consume, and if it actually decreased, refund it and drain the
// alternate stat by the same amount instead. This is correct in every case
// the original design cared about: a charge-item cast never touches mana, so
// this is a no-op for it; a BloodMana-diverted cast already drained life via
// the engine's own BloodMana path, so mana didn't move and this is a no-op
// for it too; only a genuine mana-cost cast triggers the redirect.
//
// ChargedPctDrainStat (ConsumeWeaponCharge's old job, "give a % chance to
// skip draining a charge") is NOT reimplemented here. Doing this safely would
// need the same charge-item pointer chain mentioned above, this time to
// preemptively skip the drain rather than just observe it — there's no
// after-the-fact "refund a charge" trick available here the way there is for
// a plain additive stat like mana, since charges are stored as a packed
// (current | max<<8) value written via a dedicated setter, not a simple
// STATLIST_AddUnitStat delta. Left unimplemented; bEnableChargedPctDrainStat
// is intentionally not read anywhere. See docs/offset-migration-status.md.

int64_t __fastcall Hook_Consume(int64_t unit, int* playerUnit, int skillId, int skillLevel) {
	int altStatId = 0;
	if      (SkillManaCostsLife(skillId))    altStatId = 6;
	else if (SkillManaCostsStamina(skillId)) altStatId = 10;

	if (altStatId && playerUnit) {
		auto* unitStrc = reinterpret_cast<D2UnitStrc*>(playerUnit);
		if (unitStrc->statList) {
			int manaBefore = PSh_GetStat(g_ExeBase, unitStrc->statList, 8 /* mana */);
			int64_t result = Original_Consume(unit, playerUnit, skillId, skillLevel);
			if (result != 0) {
				int manaAfter = PSh_GetStat(g_ExeBase, unitStrc->statList, 8);
				int drained = manaBefore - manaAfter;
				if (drained > 0) {
					auto DrainStat = reinterpret_cast<DrainStat_t>(g_ExeBase + OFF_DrainStat);
					DrainStat(playerUnit, 8, drained);          // refund mana
					DrainStat(playerUnit, altStatId, -drained); // drain the alternate stat instead
				}
			}
			return result;
		}
	}
	return Original_Consume(unit, playerUnit, skillId, skillLevel);
}

// ── Hook: SKILLS_GetUseState call site inside D2CLIENT_GetUnusableUseState ───
// Intercepts the single CALL at 0x1401a2580; all other callers of GetUseState
// are unaffected.

int __fastcall Hook_GetUseState(int* playerUnit, int64_t* pSkill)
{
    auto Original = reinterpret_cast<GetUseState_t>(g_ExeBase + OFF_GetUseState);
	int value = Original(playerUnit, pSkill);
	if (value == 1 &&
		(SkillRecManaCostsLife((const uint8_t*)*pSkill) || SkillRecManaCostsStamina((const uint8_t*)*pSkill)))
	{
		return 2;
	}
	return value;
}

// Hook: SKILLS_CanBePickedUpWithTelekinesis. Original function located at 140268e30
int __fastcall Hook_CanBePickedUpWithTelekinesis(D2UnitStrc* ItemUnit)
{
	// ITEMS_CheckItemTypeId(ItemUnit, ITEM_TYPE_XXX) --> 140245230 if you want to check this yourself
	return ItemUnit != nullptr && ItemUnit->dwUnitType == D2UnitType::Item;
}

// E8 call sites inside DATATBLS_CompileSkillsTxt that target DATATBLS_CompileTxt.
// Patched via context->PatchRel32(..., Rel32PatchKind::Call).
static constexpr uint64_t COMPILE_TXT_CALL_OFFSETS[] = {
	0x306bb0,
	0x306d1c,
	0x306e51,
	0x306f9e,
};

// ── INI loading ───────────────────────────────────────────────────────────────

void SkillPluginOptions::Load(const D2RL::PluginContext* /*context*/, const nlohmann::json& cfg) {
	bEnableManaCostsLife          = cfg.value("manaCostsLife", false);
	bEnableManaCostsStamina       = cfg.value("manaCostsStamina", false);
	bEnableClassicWW              = cfg.value("classicWhirlwind", false);
	bEnableWWCtc                  = cfg.value("whirlwindCtC", false);
	bTelekinesisPicksUpEverything = cfg.value("telekinesisPicksUpEverything", false);

	auto drain = cfg.value("chargedPctDrainStat", nlohmann::json::object());
	bEnableChargedPctDrainStat = drain.value("enabled", false);
	ChargedPctDrainStat        = drain.value("statId", 0);
}

// ── Plugin exports ────────────────────────────────────────────────────────────

static constexpr D2RL::PluginInfo PluginInfo {
	.infoSize   = D2RL::PluginInfoSize,
	.apiVersion = D2RL_PLUGIN_API_VERSION,
	.id         = "eezstreet-plugin-skills",
	.name       = "eezstreet Skills Plugin",
	.version    = "2.0.0",
	.author     = "eezstreet",
	.description = "Various skill-related changes.",
	.flags      = D2RL::PluginFlags::NativeHooks,
};

D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept -> const D2RL::PluginInfo* {
	return &PluginInfo;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept -> bool {
	if (context == nullptr)
		return false;

	auto cfg = PSh_Json_LoadConfig(context);
	g_skillPluginOptions.Load(context, PSh_Json_GetSection(cfg, "skills"));
	g_ExeBase = context->exeBase;

	if (g_skillPluginOptions.bEnableManaCostsLife || g_skillPluginOptions.bEnableManaCostsStamina) {
		// Redirect all DATATBLS_CompileTxt calls within DATATBLS_CompileSkillsTxt.
		for (size_t i = 0; i < sizeof(COMPILE_TXT_CALL_OFFSETS) / sizeof(COMPILE_TXT_CALL_OFFSETS[0]); ++i)
			(void)context->PatchRel32(COMPILE_TXT_CALL_OFFSETS[i], EXP_CompileTxtCallOffsets[i], sizeof(EXP_CompileTxtCallOffsets[i]),
				reinterpret_cast<uint64_t>(&Hook_CompileTxt_Call) - context->exeBase, 5, D2RL::Rel32PatchKind::Call);

		// Hook Consume; it handles the ManaCostsLife/Stamina redirect itself now
		// (see the comment above Hook_Consume for why).
		if (!context->InstallInlineHook(OFF_Consume, EXP_Consume, sizeof(EXP_Consume), Hook_Consume, &Original_Consume)) {
			D2RL::LogErrorF(context, "plugin-skills: failed to hook Consume");
			return false;
		}

		// Hook CheckStat so the skill orb turns red when life (not mana) is too low.
		if (!context->InstallInlineHook(OFF_CheckStat, EXP_CheckStat, sizeof(EXP_CheckStat), Hook_CheckStat, &Original_CheckStat)) {
			D2RL::LogErrorF(context, "plugin-skills: failed to hook CheckStat");
			return false;
		}

		// Hook client-side mana prediction to drain life instead (prevents rubber-banding).
		// First 6 bytes: push rbx (2) + sub rsp,0x20 (4) — old API needed hookSize=6 here;
		// the new InstallInlineHook has no hookSize param, trusting the loader to size it.
		if (!context->InstallInlineHook(OFF_ClientPredict, EXP_ClientPredict, sizeof(EXP_ClientPredict), Hook_ClientPredict, &Original_ClientPredict)) {
			D2RL::LogErrorF(context, "plugin-skills: failed to hook ClientPredict");
			return false;
		}

		// Redirect the single CALL at 0x1401a2580 inside D2CLIENT_GetUnusableUseState.
		(void)context->PatchRel32(OFF_GetUseState_Call, EXP_GetUseState_Call, sizeof(EXP_GetUseState_Call),
			reinterpret_cast<uint64_t>(&Hook_GetUseState) - context->exeBase, 5, D2RL::Rel32PatchKind::Call);
	}

	if (g_skillPluginOptions.bEnableClassicWW)
	{
		uint8_t classicWWBytes[] = { 0xB8, 0x01, 0x00, 0x00, 0x00 };
		(void)context->PatchBytes(OFF_ClassicWW, EXP_ClassicWW, sizeof(EXP_ClassicWW), classicWWBytes, sizeof(classicWWBytes));
	}

	if (g_skillPluginOptions.bEnableWWCtc)
	{
		uint8_t ctcWWBytes[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
		(void)context->PatchBytes(OFF_EnableWWCtC, EXP_EnableWWCtC, sizeof(EXP_EnableWWCtC), ctcWWBytes, sizeof(ctcWWBytes));
	}

	if (g_skillPluginOptions.bTelekinesisPicksUpEverything)
	{
		(void)context->PatchRel32(OFF_Telekinesis, EXP_Telekinesis, sizeof(EXP_Telekinesis),
			reinterpret_cast<uint64_t>(&Hook_CanBePickedUpWithTelekinesis) - context->exeBase, 5, D2RL::Rel32PatchKind::Call);
	}

	if (g_skillPluginOptions.bEnableChargedPctDrainStat)
	{
		// Not implemented against this build target — see the comment above
		// Hook_Consume for why (needs an unverified charged-item pointer chain
		// this codebase isn't confident enough in to ship).
		D2RL::LogErrorF(context, "plugin-skills: chargedPctDrainStat is enabled in config but not implemented for this build; ignoring");
	}

	return true;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderUnloadPlugin() noexcept {
	// Hooks/patches installed via context->InstallInlineHook/PatchBytes/PatchRel32 are
	// reverted automatically by D2RLoader on unload (ASSUMPTION — verify against real
	// loader behavior before relying on this in production).
	g_SkillsRecords = nullptr;
	g_SkillsCount   = 0;
}
