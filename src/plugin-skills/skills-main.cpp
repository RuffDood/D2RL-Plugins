#include "plugin.h"
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
using AuraConsume_t   = int64_t(__fastcall*)(int* playerUnit, int manaCost);
using DrainStat_t     = void(__fastcall*)(void* unit, int statId, int delta);
using GetManaCost_t   = int(__fastcall*)(uint8_t unitType, int skillId, int skillLevel);
using CheckStat_t     = bool(__fastcall*)(int* playerUnit, int64_t* skillStruct,
                                          int param3, int currentMana);
using ClientPredict_t = void(__fastcall*)(int* playerUnit, int skillId, int skillLevel);
using ConsumeWeaponCharge_t = int64_t(__fastcall*)(int64_t unit, int* playerUnit,
                                                    int64_t chargeItem, int skillId);

// ── Addresses (offsets from exe base 0x140000000) ────────────────────────────

static constexpr uint64_t OFF_CompileSkillsTxt = 0x214840; // DATATBLS_CompileSkillsTxt
static constexpr uint64_t OFF_CompileTxt       = 0x21c680; // DATATBLS_CompileTxt
static constexpr uint64_t OFF_Consume          = 0x30e7a0; // D2GAME_SKILLMANA_Consume
static constexpr uint64_t OFF_AuraConsume      = 0x30e850; // D2GAME_SKILLMANA_AuraConsume
static constexpr uint64_t OFF_DrainStat        = 0x227470; // FUN_140227470
static constexpr uint64_t OFF_GetManaCost      = 0x268da0; // D2Common_SKILLMANA_GetManaCost
static constexpr uint64_t OFF_CheckStat        = 0x264990; // D2Common_SKILLMANA_CheckStat (use-state mana check)
static constexpr uint64_t OFF_ClientPredict    = 0x197080; // FUN_140197080 (client-side mana prediction)
static constexpr uint64_t OFF_ConsumeWeaponCharge = 0x30e550; // D2GAME_SKILLMANA_ConsumeWeaponCharge
static constexpr uint64_t OFF_GetUseState      = 0x264590; // SKILLS_GetUseState_6FDB0B70
static constexpr uint64_t OFF_GetUseState_Call = 0x1a2580; // call site inside D2CLIENT_GetUnusableUseState
static constexpr uint64_t OFF_ClassicWW        = 0x41A49D;
static constexpr uint64_t OFF_EnableWWCtC      = 0x40AA35;
static constexpr uint64_t OFF_Telekinesis      = 0x426991;

// skills.txt record layout constants
static constexpr uint64_t SKILLS_RECORD_STRIDE = 0x2EC;  // bytes per record
static constexpr uint64_t SKILLS_FLAGS_OFFSET  = 0x24;   // uint64_t flags QWORD
static constexpr uint32_t SKILLSRECORD_TYPE_BOOL = 29;
static constexpr int      MANA_COSTS_LIFE_BIT    = 47;   // first free bit
static constexpr int      MANA_COSTS_STAMINA_BIT = 48;   // second free bit

// ── Plugin state ──────────────────────────────────────────────────────────────

static SkillPluginOptions            g_skillPluginOptions {};
static uintptr_t                     g_ExeBase  = 0;
static const D2RLoaderPluginContext* g_Context  = nullptr;

// Compiled skills.txt records (game-owned memory — do not free).
static void*    g_SkillsRecords = nullptr;
static uint64_t g_SkillsCount  = 0;

using GetUseState_t    = int(__fastcall*)(int* playerUnit, int64_t* pSkill);
using NeedManaSound_t  = int(__fastcall*)(int* skillEntity, uint32_t* outPriority);
using DiagB2300_t      = void(__fastcall*)(void* param_1, void* param_2);
using DiagDaf0_t       = void(__fastcall*)(int* param_1, void* param_2, uint8_t param_3, char param_4, uint32_t* param_5);
using PlaySoundEffect_t = int64_t(__fastcall*)(int soundId, int* entity, int param3, int param4, int param5);

static Consume_t       Original_Consume       = nullptr;
static AuraConsume_t   Original_AuraConsume   = nullptr;
static CheckStat_t     Original_CheckStat     = nullptr;
static ClientPredict_t Original_ClientPredict = nullptr;
static ConsumeWeaponCharge_t Original_ConsumeWeaponCharge = nullptr;
static void* Original_CanBePickedUpWithTelekinesis = nullptr;

// Thread-local carries skillId from Hook_Consume into Hook_AuraConsume.
thread_local int g_currentSkillId = -1;

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
// param_4 is normally the current mana; we swap it for current life when ManaCostsLife=1.
// Life and mana values are in fixed-point (×256), matching the units GetManaCost uses.

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
                    int currentAlt = PSh_GetStat(g_ExeBase, unitStrc->statList, altStatId);
                    return Original_CheckStat(playerUnit, skillStruct, param3, currentAlt);
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

// ── Hook: D2GAME_SKILLMANA_ConsumeWeaponCharge ───────────────────────────────
// Gives a percent chance (read from the player's ChargedPctDrainStat stat,
// value 0-100) to skip draining a charge when a charged item's skill is cast.
//
// statCode encoding matches D2MOO's D2SLayerStatIdStrc::MakeFromStatId: low
// 16 bits = layer (0), high 16 bits = stat ID — i.e. statId << 16. The 3rd
// arg to GetStat is normally an ItemStatCost record used only to apply a
// per-stat minimum-value floor; passing 0 skips that and returns the raw
// stat value, which is what we want for a plain percent stat.
//
// Rolls via PSh_RollUnit, the same per-unit LCG the engine itself uses for
// other rolls (see SKILLS_FindPotion_DropPotion @ 0x140417018), rather than
// an unrelated RNG stream.
//
// Safe to skip the drain entirely: Consume (and thus this function) is only
// called to pay the resource cost *after* the skill has already executed
// (see callers of D2GAME_SKILLMANA_Consume) and its return value isn't even
// checked by the caller — so returning 1 without decrementing/broadcasting
// the charge has no other effect than leaving the charge count untouched.

int64_t __fastcall Hook_ConsumeWeaponCharge(int64_t unit, int* playerUnit,
                                             int64_t chargeItem, int skillId)
{
	if (g_skillPluginOptions.bEnableChargedPctDrainStat && playerUnit) {
		auto* unitStrc = reinterpret_cast<D2UnitStrc*>(playerUnit);
		if (unitStrc->statList) {
			int pct = PSh_GetStat(g_ExeBase, unitStrc->statList, g_skillPluginOptions.ChargedPctDrainStat);
			if (pct > 0) {
				if (pct > 100) pct = 100;
				uint32_t roll = static_cast<uint32_t>(PSh_RollUnit(unitStrc)) % 100;
				if (static_cast<int>(roll) < pct)
					return 1; // proc: charge is not drained, skill still casts normally
			}
		}
	}
	return Original_ConsumeWeaponCharge(unit, playerUnit, chargeItem, skillId);
}

// ── Hook: D2GAME_SKILLMANA_Consume ───────────────────────────────────────────
// Sets thread-local skillId so Hook_AuraConsume can see which skill is being cast.

int64_t __fastcall Hook_Consume(int64_t unit, int* playerUnit, int skillId, int skillLevel) {
	g_currentSkillId = skillId;
	int64_t result = Original_Consume(unit, playerUnit, skillId, skillLevel);
	g_currentSkillId = -1;
	return result;
}

// ── Hook: D2GAME_SKILLMANA_AuraConsume ───────────────────────────────────────
// Drains life (stat 6) instead of mana when ManaCostsLife is set.
// Mirrors D2GAME_SKILLS_BloodMana: no pre-check, kills player if life < cost.

int64_t __fastcall Hook_AuraConsume(int* playerUnit, int manaCost) {
	if (g_currentSkillId >= 0) {
		int altStatId = 0;
		if      (SkillManaCostsLife(g_currentSkillId))    altStatId = 6;
		else if (SkillManaCostsStamina(g_currentSkillId)) altStatId = 10;
		if (altStatId) {
			if (!playerUnit || *playerUnit != 0)
				return 1;
			auto DrainStat = (DrainStat_t)(g_ExeBase + OFF_DrainStat);
			DrainStat(playerUnit, altStatId, -manaCost);
			return 1;
		}
	}
	return Original_AuraConsume(playerUnit, manaCost);
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
// Patched via PSh_PatchCallSite; removed via PSh_RemoveHook.
static constexpr uint64_t COMPILE_TXT_CALL_OFFSETS[] = {
	0x2190AD,
	0x21921F,
	0x219351,
	0x2194B2,
};

// ── INI loading ───────────────────────────────────────────────────────────────

void SkillPluginOptions::Load(const D2RLoaderPluginContext* /*context*/, const nlohmann::json& cfg) {
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

static constexpr D2RLoaderPluginInfo PluginInfo {
	.apiVersion = D2RLOADER_PLUGIN_API_VERSION,
	.id         = "plugin-skills",
	.name       = "Skills Plugin",
	.version    = "0.0.1",
	.author     = "eezstreet",
	.flags      = D2RLoaderPluginFlag_None,
};

D2RLOADER_PLUGIN_EXPORT const D2RLoaderPluginInfo* __cdecl D2RLoaderGetPluginInfo() noexcept {
	return &PluginInfo;
}

D2RLOADER_PLUGIN_EXPORT bool __cdecl D2RLoaderLoadHooks(const D2RLoaderPluginContext* context) noexcept {
	if (!context || context->apiVersion < D2RLOADER_PLUGIN_API_VERSION)
		return false;

	auto cfg = PSh_Json_LoadConfig(context);
	g_skillPluginOptions.Load(context, PSh_Json_GetSection(cfg, "skills"));
	g_ExeBase = context->exeBase;
	g_Context = context;

	if (g_skillPluginOptions.bEnableManaCostsLife || g_skillPluginOptions.bEnableManaCostsStamina) {
		// Redirect all DATATBLS_CompileTxt calls within DATATBLS_CompileSkillsTxt.
		for (uint64_t off : COMPILE_TXT_CALL_OFFSETS)
			PSh_PatchCallSite(PLUGINID_SKILLS, context, off, reinterpret_cast<void*>(Hook_CompileTxt_Call));

		// Hook Consume to propagate skillId to AuraConsume via thread-local.
		if (!PSh_InstallHook(PLUGINID_SKILLS, context, OFF_Consume,
		                     reinterpret_cast<void*>(Hook_Consume),
		                     reinterpret_cast<void**>(&Original_Consume))) {
			D2RPluginLogErrorF(context, "plugin-skills: failed to hook Consume");
			for (uint64_t off : COMPILE_TXT_CALL_OFFSETS)
				PSh_RemoveHook(PLUGINID_SKILLS, context, off);
			return false;
		}

		if (!PSh_InstallHook(PLUGINID_SKILLS, context, OFF_AuraConsume,
		                     reinterpret_cast<void*>(Hook_AuraConsume),
		                     reinterpret_cast<void**>(&Original_AuraConsume))) {
			D2RPluginLogErrorF(context, "plugin-skills: failed to hook AuraConsume");
			PSh_RemoveHook(PLUGINID_SKILLS, context, OFF_Consume);
			for (uint64_t off : COMPILE_TXT_CALL_OFFSETS)
				PSh_RemoveHook(PLUGINID_SKILLS, context, off);
			return false;
		}

		// Hook CheckStat so the skill orb turns red when life (not mana) is too low.
		if (!PSh_InstallHook(PLUGINID_SKILLS, context, OFF_CheckStat,
		                     reinterpret_cast<void*>(Hook_CheckStat),
		                     reinterpret_cast<void**>(&Original_CheckStat))) {
			D2RPluginLogErrorF(context, "plugin-skills: failed to hook CheckStat");
			PSh_RemoveHook(PLUGINID_SKILLS, context, OFF_AuraConsume);
			PSh_RemoveHook(PLUGINID_SKILLS, context, OFF_Consume);
			for (uint64_t off : COMPILE_TXT_CALL_OFFSETS)
				PSh_RemoveHook(PLUGINID_SKILLS, context, off);
			return false;
		}

		// Hook client-side mana prediction to drain life instead (prevents rubber-banding).
		// First 6 bytes: push rbx (2) + sub rsp,0x20 (4) — requires hookSize=6.
		if (!PSh_InstallHook(PLUGINID_SKILLS, context, OFF_ClientPredict,
		                     reinterpret_cast<void*>(Hook_ClientPredict),
		                     reinterpret_cast<void**>(&Original_ClientPredict), 6)) {
			D2RPluginLogErrorF(context, "plugin-skills: failed to hook ClientPredict");
			PSh_RemoveHook(PLUGINID_SKILLS, context, OFF_CheckStat);
			PSh_RemoveHook(PLUGINID_SKILLS, context, OFF_AuraConsume);
			PSh_RemoveHook(PLUGINID_SKILLS, context, OFF_Consume);
			for (uint64_t off : COMPILE_TXT_CALL_OFFSETS)
				PSh_RemoveHook(PLUGINID_SKILLS, context, off);
			return false;
		}

		// Redirect the single CALL at 0x1401a2580 inside D2CLIENT_GetUnusableUseState.
		PSh_PatchCallSite(PLUGINID_SKILLS, context, OFF_GetUseState_Call,
		                  reinterpret_cast<void*>(Hook_GetUseState));
	}

	if (g_skillPluginOptions.bEnableClassicWW)
	{
		uint8_t classicWWBytes[] = { 0xB8, 0x01, 0x00, 0x00, 0x00 };
		PSh_PatchBytes(PLUGINID_SKILLS, context, OFF_ClassicWW, 5, classicWWBytes);
	}

	if (g_skillPluginOptions.bEnableWWCtc)
	{
		uint8_t ctcWWBytes[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
		PSh_PatchBytes(PLUGINID_SKILLS, context, OFF_EnableWWCtC, 6, ctcWWBytes);
	}

	if (g_skillPluginOptions.bTelekinesisPicksUpEverything)
	{
		PSh_PatchCallSite(PLUGINID_SKILLS, context, OFF_Telekinesis,
			reinterpret_cast<void*>(Hook_CanBePickedUpWithTelekinesis));
	}

	if (g_skillPluginOptions.bEnableChargedPctDrainStat)
	{
		if (!PSh_InstallHook(PLUGINID_SKILLS, context, OFF_ConsumeWeaponCharge,
		                     reinterpret_cast<void*>(Hook_ConsumeWeaponCharge),
		                     reinterpret_cast<void**>(&Original_ConsumeWeaponCharge))) {
			D2RPluginLogErrorF(context, "plugin-skills: failed to hook ConsumeWeaponCharge");
			return false;
		}
	}

	return true;
}

D2RLOADER_PLUGIN_EXPORT void __cdecl D2RLoaderUnload() noexcept {
	// Pass nullptr so ResolveExeBase uses g_CachedExeBase — g_Context is stale by this point.
	PSh_RemoveHook(PLUGINID_SKILLS, nullptr, OFF_ConsumeWeaponCharge);
	PSh_RemoveHook(PLUGINID_SKILLS, nullptr, OFF_GetUseState_Call);
	PSh_RemoveHook(PLUGINID_SKILLS, nullptr, OFF_ClientPredict);
	PSh_RemoveHook(PLUGINID_SKILLS, nullptr, OFF_CheckStat);
	PSh_RemoveHook(PLUGINID_SKILLS, nullptr, OFF_AuraConsume);
	PSh_RemoveHook(PLUGINID_SKILLS, nullptr, OFF_Consume);
	for (uint64_t off : COMPILE_TXT_CALL_OFFSETS)
		PSh_RemoveHook(PLUGINID_SKILLS, nullptr, off);
	g_SkillsRecords = nullptr;
	g_SkillsCount   = 0;
	g_Context       = nullptr;
}
