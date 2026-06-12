#include "plugin.h"
#include "plugin-shared.h"
#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <vector>

static void DebugLog(const char* fmt, ...) noexcept {
	char buf[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	FILE* f = fopen("C:\\Users\\Nick\\Documents\\plugin-skills-debug.log", "a");
	if (f) { fputs(buf, f); fputc('\n', f); fclose(f); }
}

// ── D2R function type aliases ─────────────────────────────────────────────────

using CompileTxt_t    = void(__fastcall*)(uint8_t context, const char* txtName,
                                          const char* binName, const char* param4,
                                          D2TxtFieldDesc* fields, uint64_t recordSize,
                                          D2TxtContainer* output);
using Consume_t       = int64_t(__fastcall*)(int64_t unit, int* playerUnit,
                                             int skillId, int skillLevel);
using AuraConsume_t   = int64_t(__fastcall*)(int* playerUnit, int manaCost);
using DrainStat_t     = void(__fastcall*)(void* unit, int statId, int delta);
using GetStat_t       = int(__fastcall*)(int64_t statContainer, uint64_t statCode, int64_t unused);
using GetManaCost_t   = int(__fastcall*)(uint8_t unitType, int skillId, int skillLevel);
using CheckStat_t     = bool(__fastcall*)(int* playerUnit, int64_t* skillStruct,
                                          int param3, int currentMana);
using ClientPredict_t = void(__fastcall*)(int* playerUnit, int skillId, int skillLevel);

// ── Addresses (offsets from exe base 0x140000000) ────────────────────────────

static constexpr uint64_t OFF_CompileSkillsTxt = 0x214840; // DATATBLS_CompileSkillsTxt
static constexpr uint64_t OFF_CompileTxt       = 0x21c680; // DATATBLS_CompileTxt
static constexpr uint64_t OFF_Consume          = 0x30e7a0; // D2GAME_SKILLMANA_Consume
static constexpr uint64_t OFF_AuraConsume      = 0x30e850; // D2GAME_SKILLMANA_AuraConsume
static constexpr uint64_t OFF_DrainStat        = 0x227470; // FUN_140227470
static constexpr uint64_t OFF_GetStat          = 0x224720; // FUN_140224720 (read a stat value)
static constexpr uint64_t OFF_GetManaCost      = 0x268da0; // D2Common_SKILLMANA_GetManaCost
static constexpr uint64_t OFF_CheckStat        = 0x264990; // D2Common_SKILLMANA_CheckStat (use-state mana check)
static constexpr uint64_t OFF_ClientPredict    = 0x197080; // FUN_140197080 (client-side mana prediction)

// skills.txt record layout constants
static constexpr uint64_t SKILLS_RECORD_STRIDE = 0x2EC;  // bytes per record
static constexpr uint64_t SKILLS_FLAGS_OFFSET  = 0x24;   // uint64_t flags QWORD
static constexpr int      MANA_COSTS_LIFE_BIT  = 47;     // first free bit

// ── Plugin state ──────────────────────────────────────────────────────────────

static uintptr_t                     g_ExeBase  = 0;
static const D2RLoaderPluginContext* g_Context  = nullptr;

// Near stubs and patched offsets for each redirected CALL inside DATATBLS_CompileSkillsTxt.
static std::vector<void*>    g_NearStubs;
static std::vector<uint64_t> g_PatchedCallOffsets;

// Compiled skills.txt records (game-owned memory — do not free).
static void*    g_SkillsRecords = nullptr;
static uint64_t g_SkillsCount  = 0;

// Type code for bit-field columns; detected at runtime from existing descriptors.
static uint32_t g_BoolFieldType = 0;
static bool     g_BoolTypeKnown = false;

static Consume_t       Original_Consume       = nullptr;
static AuraConsume_t   Original_AuraConsume   = nullptr;
static CheckStat_t     Original_CheckStat     = nullptr;
static ClientPredict_t Original_ClientPredict = nullptr;

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

// Our ManaCostsLife descriptor — filled in once we know g_BoolFieldType.
static char g_ManaCostsLifeName[] = "ManaCostsLife";
static D2TxtFieldDesc g_ManaCostsLifeDesc = {};

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

	uint8_t compileTxtFlag = *reinterpret_cast<const uint8_t*>(g_ExeBase + 0x1D8BDB0);
	DebugLog("Hook_CompileTxt_Call: txtName=%s binName=%s param4=%s compileTxtFlag=%d",
	         txtName  ? txtName  : "(null)",
	         binName  ? binName  : "(null)",
	         param4   ? param4   : "(null)",
	         compileTxtFlag);

	if (std::strcmp(txtName, "skills") != 0) {
		// Not the skills table (e.g. skilldesc) — pass through unchanged.
		RealCompileTxt(context, txtName, binName, param4, origFields, recordSize, output);
		return;
	}

	// Detect bool type from existing descriptors on first call.
	if (!g_BoolTypeKnown) {
		g_BoolFieldType = DetectBoolType(origFields);
		// Log first 8 entries at offset 0x24 so we can verify type/count semantics.
		int logged = 0;
		for (int i = 0; origFields[i].pName && logged < 8; ++i) {
			if (origFields[i].offset == SKILLS_FLAGS_OFFSET) {
				DebugLog("  boolField[%d]: name=%s type=%u count=%u offset=0x%llX",
				         i, origFields[i].pName, origFields[i].type, origFields[i].count,
				         (unsigned long long)origFields[i].offset);
				++logged;
			}
		}
		if (g_BoolFieldType != 0) {
			g_ManaCostsLifeDesc = { g_ManaCostsLifeName, g_BoolFieldType,
			                        MANA_COSTS_LIFE_BIT, SKILLS_FLAGS_OFFSET, 0 };
			g_BoolTypeKnown = true;
		}
	}

	if (!g_BoolTypeKnown) {
		// Could not detect bool type — fall back to unmodified compilation.
		D2RPluginLogWarnF(g_Context,
			"plugin-skills: could not detect bool field type in skills.txt descriptor; "
			"ManaCostsLife column will not be available");
		RealCompileTxt(context, txtName, binName, param4, origFields, recordSize, output);
		return;
	}

	// Find the "end" sentinel (type=0, pName="end") that terminates the descriptor list.
	// Insert our field immediately before it so the compiler sees it.
	int n = 0;
	while (origFields[n].pName && std::strcmp(origFields[n].pName, "end") != 0) ++n;

	std::vector<D2TxtFieldDesc> extended(origFields, origFields + n);
	extended.push_back(g_ManaCostsLifeDesc);
	extended.push_back(origFields[n]);   // preserve the "end" sentinel

	RealCompileTxt(context, txtName, binName, param4,
	               extended.data(), recordSize, output);

	// Save records for SkillManaCostsLife() lookups.
	if (output && output->pData) {
		g_SkillsRecords = output->pData->pRecords;
		g_SkillsCount   = output->pData->nCount;
	}
	DebugLog("Hook_CompileTxt_Call: skills compiled, %llu records, boolType=%u, records=%p",
	         (unsigned long long)g_SkillsCount, g_BoolFieldType, g_SkillsRecords);
}

static bool SkillManaCostsLife(int skillId) noexcept {
	if (!g_SkillsRecords || skillId < 0 || static_cast<uint64_t>(skillId) >= g_SkillsCount)
		return false;
	const uint8_t* rec = static_cast<const uint8_t*>(g_SkillsRecords)
	                     + static_cast<uint64_t>(skillId) * SKILLS_RECORD_STRIDE;
	return ((*reinterpret_cast<const uint64_t*>(rec + SKILLS_FLAGS_OFFSET)) >> MANA_COSTS_LIFE_BIT) & 1;
}

// ── Hook: D2Common_SKILLMANA_CheckStat ───────────────────────────────────────
// Called by GetUseState to decide if the skill can be cast (returns false → red orb).
// param_4 is normally the current mana; we swap it for current life when ManaCostsLife=1.
// param_1+0x88 (== *(int64_t*)(playerUnit+0x22)) is the stat container pointer.
// GetStat(container, 0x60000) returns life; GetStat(container, 0x80000) returns mana.
// Both values are in fixed-point (×256), matching the units GetManaCost uses.

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
            if (SkillManaCostsLife(skillId)) {
                auto statCont = *reinterpret_cast<int64_t*>(
                    reinterpret_cast<char*>(playerUnit) + 0x88);
                if (statCont) {
                    auto GetStat    = reinterpret_cast<GetStat_t>(g_ExeBase + OFF_GetStat);
                    int currentLife = GetStat(statCont, 0x60000, 0);
                    return Original_CheckStat(playerUnit, skillStruct, param3, currentLife);
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
    if (*playerUnit == 0 && g_SkillsRecords && SkillManaCostsLife(skillId)) {
        uint8_t unitType = *reinterpret_cast<uint8_t*>(
            reinterpret_cast<char*>(playerUnit) + 0x1bd);
        auto GetManaCost = reinterpret_cast<GetManaCost_t>(g_ExeBase + OFF_GetManaCost);
        int manaCost = GetManaCost(unitType, skillId, skillLevel);
        if (manaCost >= 1) {
            auto GetStat  = reinterpret_cast<GetStat_t>(g_ExeBase + OFF_GetStat);
            auto DrainStat = reinterpret_cast<DrainStat_t>(g_ExeBase + OFF_DrainStat);
            auto statCont  = *reinterpret_cast<int64_t*>(
                reinterpret_cast<char*>(playerUnit) + 0x88);
            if (statCont) {
                int currentLife = GetStat(statCont, 0x60000, 0);
                if (currentLife >= manaCost)
                    DrainStat(playerUnit, 6, -manaCost);
            }
        }
        return;
    }
    Original_ClientPredict(playerUnit, skillId, skillLevel);
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
	// Dump raw flags QWORD to diagnose bit layout
	uint64_t rawFlags = 0;
	if (g_SkillsRecords && g_currentSkillId >= 0 && static_cast<uint64_t>(g_currentSkillId) < g_SkillsCount) {
		const uint8_t* rec = static_cast<const uint8_t*>(g_SkillsRecords)
		                     + static_cast<uint64_t>(g_currentSkillId) * SKILLS_RECORD_STRIDE;
		rawFlags = *reinterpret_cast<const uint64_t*>(rec + SKILLS_FLAGS_OFFSET);
	}
	DebugLog("Hook_AuraConsume: skillId=%d manaCost=%d manaCostsLife=%d rawFlags=0x%016llX",
	         g_currentSkillId, manaCost, SkillManaCostsLife(g_currentSkillId),
	         (unsigned long long)rawFlags);
	if (g_currentSkillId >= 0 && SkillManaCostsLife(g_currentSkillId)) {
		if (!playerUnit || *playerUnit != 0)
			return 1;
		auto DrainStat = (DrainStat_t)(g_ExeBase + OFF_DrainStat);
		DrainStat(playerUnit, 6, -manaCost);
		return 1;
	}
	return Original_AuraConsume(playerUnit, manaCost);
}

// ── Call-site patching inside DATATBLS_CompileSkillsTxt ──────────────────────
//
// Scans the body of DATATBLS_CompileSkillsTxt for every E8 rel32 CALL that
// resolves to DATATBLS_CompileTxt. For each one, allocates a near stub (within
// ±2 GB) that abs-jumps to Hook_CompileTxt_Call, then patches the 5-byte CALL
// to target the stub instead.

// The four E8 call sites inside DATATBLS_CompileSkillsTxt that target DATATBLS_CompileTxt.
// Found via Ghidra; all use E8 rel32 encoding.
static constexpr uint64_t COMPILE_TXT_CALL_OFFSETS[] = {
	0x2190AD,
	0x21921F,
	0x219351,
	0x2194B2,
};

static void PatchOneCallSite(const D2RLoaderPluginContext* ctx, uint64_t callOffset) {
	void* callSite = reinterpret_cast<void*>(g_ExeBase + callOffset);

	void* stub = PSh_AllocNear(callSite, 32);
	if (!stub) {
		DebugLog("InstallCompileSkillsTxtPatches: PSh_AllocNear failed for offset 0x%llX",
		         (unsigned long long)callOffset);
		return;
	}

	uint8_t* s = static_cast<uint8_t*>(stub);
	s[0] = 0xFF; s[1] = 0x25;
	s[2] = 0x00; s[3] = 0x00; s[4] = 0x00; s[5] = 0x00;
	*reinterpret_cast<uint64_t*>(s + 6) = reinterpret_cast<uint64_t>(Hook_CompileTxt_Call);

	intptr_t newRel  = reinterpret_cast<uint8_t*>(stub)
	                   - (static_cast<uint8_t*>(callSite) + 5);
	int32_t  newRel32 = static_cast<int32_t>(newRel);
	uint8_t  patch[5] = { 0xE8, 0,0,0,0 };
	std::memcpy(patch + 1, &newRel32, 4);

	PSh_PatchBytes(PLUGINID_SKILLS, ctx, callOffset, 5, patch);
	g_NearStubs.push_back(stub);
	g_PatchedCallOffsets.push_back(callOffset);
	DebugLog("InstallCompileSkillsTxtPatches: patched 0x%llX -> stub %p",
	         (unsigned long long)callOffset, stub);
}

static void InstallCompileSkillsTxtPatches(const D2RLoaderPluginContext* ctx) {
	for (uint64_t off : COMPILE_TXT_CALL_OFFSETS)
		PatchOneCallSite(ctx, off);
}

static void RemoveCompileSkillsTxtPatches(const D2RLoaderPluginContext* ctx) {
	for (uint64_t off : g_PatchedCallOffsets)
		PSh_UnpatchBytes(PLUGINID_SKILLS, ctx, off);
	g_PatchedCallOffsets.clear();

	for (void* stub : g_NearStubs)
		VirtualFree(stub, 0, MEM_RELEASE);
	g_NearStubs.clear();
}

// ── Plugin exports ────────────────────────────────────────────────────────────

static constexpr D2RLoaderPluginInfo PluginInfo {
	.apiVersion = D2RLOADER_PLUGIN_API_VERSION,
	.id         = "plugin-skills",
	.name       = "Skills Plugin",
	.version    = "1.0.0",
	.author     = "eezstreet",
	.flags      = D2RLoaderPluginFlag_None,
};

D2RLOADER_PLUGIN_EXPORT const D2RLoaderPluginInfo* __cdecl D2RLoaderGetPluginInfo() noexcept {
	return &PluginInfo;
}

D2RLOADER_PLUGIN_EXPORT bool __cdecl D2RLoaderLoadHooks(const D2RLoaderPluginContext* context) noexcept {
	if (!context || context->apiVersion < D2RLOADER_PLUGIN_API_VERSION)
		return false;

	g_ExeBase = context->exeBase;
	g_Context = context;

	// Redirect all DATATBLS_CompileTxt calls within DATATBLS_CompileSkillsTxt.
	InstallCompileSkillsTxtPatches(context);

	// Hook Consume to propagate skillId to AuraConsume via thread-local.
	if (!PSh_InstallHook(PLUGINID_SKILLS, context, OFF_Consume,
	                     reinterpret_cast<void*>(Hook_Consume),
	                     reinterpret_cast<void**>(&Original_Consume))) {
		D2RPluginLogErrorF(context, "plugin-skills: failed to hook Consume");
		RemoveCompileSkillsTxtPatches(context);
		return false;
	}

	if (!PSh_InstallHook(PLUGINID_SKILLS, context, OFF_AuraConsume,
	                     reinterpret_cast<void*>(Hook_AuraConsume),
	                     reinterpret_cast<void**>(&Original_AuraConsume))) {
		D2RPluginLogErrorF(context, "plugin-skills: failed to hook AuraConsume");
		PSh_RemoveHook(PLUGINID_SKILLS, context, OFF_Consume);
		RemoveCompileSkillsTxtPatches(context);
		return false;
	}

	// Hook CheckStat so the skill orb turns red when life (not mana) is too low.
	if (!PSh_InstallHook(PLUGINID_SKILLS, context, OFF_CheckStat,
	                     reinterpret_cast<void*>(Hook_CheckStat),
	                     reinterpret_cast<void**>(&Original_CheckStat))) {
		D2RPluginLogErrorF(context, "plugin-skills: failed to hook CheckStat");
		PSh_RemoveHook(PLUGINID_SKILLS, context, OFF_AuraConsume);
		PSh_RemoveHook(PLUGINID_SKILLS, context, OFF_Consume);
		RemoveCompileSkillsTxtPatches(context);
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
		RemoveCompileSkillsTxtPatches(context);
		return false;
	}

	return true;
}

D2RLOADER_PLUGIN_EXPORT void __cdecl D2RLoaderUnload() noexcept {
	// Pass nullptr so ResolveExeBase uses g_CachedExeBase — g_Context is stale by this point.
	PSh_RemoveHook(PLUGINID_SKILLS, nullptr, OFF_ClientPredict);
	PSh_RemoveHook(PLUGINID_SKILLS, nullptr, OFF_CheckStat);
	PSh_RemoveHook(PLUGINID_SKILLS, nullptr, OFF_AuraConsume);
	PSh_RemoveHook(PLUGINID_SKILLS, nullptr, OFF_Consume);
	RemoveCompileSkillsTxtPatches(nullptr);
	g_SkillsRecords = nullptr;
	g_SkillsCount   = 0;
	g_Context       = nullptr;
}
