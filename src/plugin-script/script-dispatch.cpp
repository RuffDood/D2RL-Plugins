#include "script-dispatch.h"
#include "script-types.h"
#include <array>
#include <cstring>

// pSpell info record returned by SKILLITEM_GetPSpellInfoEntry.
struct D2PSpellInfoStrc {
	int32_t nDispatchIndex; // index into pSpell_Handler's g_apfnPSpellDispatch table (0–10)
	int32_t nPSpellId;      // index into pSpell_EffectDispatch's effect table (0–15)
	int32_t nParam;
};

// ── Registry storage ──────────────────────────────────────────────────────────

std::unordered_map<uint32_t, JSValue> g_Registry[static_cast<int>(CallbackType::Count)];

// ── Originals (set by PSh_InstallHook) ───────────────────────────────────────

// Missile DO:  MISSMODE_SrvDoDispatcher (FUN_1403d26b0) — 2-param, reads pSrvDoFunc at missile txt+0x2c
using MissileSrvDo_t  = void(__fastcall*)(D2GameStrc*, D2UnitStrc*);
// Missile HIT+DMG: MISSMODE_SrvDmgHitDispatcher (FUN_1403d1910) — combined handler, reads pSrvHitFunc at txt+0x2e
using MissileSrvHit_t = void(__fastcall*)(D2GameStrc*, D2UnitStrc*, void*, int32_t);
// Missile DMG: dispatched inside FUN_1403d1910 at txt+0x30; typedef kept for future separate hook
using MissileSrvDmg_t = void(__fastcall*)(D2GameStrc*, D2UnitStrc*, D2UnitStrc*, void*);
// Skill Start dispatcher (D2GAME_SkillHandler1): 5 params — game, unit, skillId, skillLvl, param5
using SkillStartDispatch_t = int32_t(__fastcall*)(D2GameStrc*, D2UnitStrc*, int32_t, int32_t, int32_t);
// Skill Do dispatcher (D2GAME_SKILLS_Handler_6FD12BA0): 7 params — a5/a6/a7 must be forwarded
using SkillDispatch_t      = int32_t(__fastcall*)(D2GameStrc*, D2UnitStrc*, int32_t, int32_t, int32_t, int32_t, int32_t);
// Object Init dispatcher (OBJECTS_InitHandler): game, unit, unitId, room, x, y
using ObjInit_t       = void(__fastcall*)(D2GameStrc*, D2UnitStrc*, int32_t, void*, int32_t, int32_t);
// Object Operate dispatcher (OBJECTS_OperateHandler): game, player, object
using ObjOperate_t    = int32_t(__fastcall*)(D2GameStrc*, void*, D2UnitStrc*);
// Object Populate: void __fastcall(D2GameStrc*, D2UnitStrc*)
using ObjPopulate_t   = void(__fastcall*)(D2GameStrc*, D2UnitStrc*);
// pSpell effect dispatcher (SKILLITEM_pSpell_EffectDispatch): game, caster, item, extra
using PSpellEffect_t  = void(__fastcall*)(D2GameStrc*, D2UnitStrc*, D2UnitStrc*, D2UnitStrc*);
// pSpell handler/validator (SKILLITEM_pSpell_Handler): game, caster, item, bForceUse; returns non-zero to proceed
using PSpellHandler_t = uint64_t(__fastcall*)(D2GameStrc*, D2UnitStrc*, D2UnitStrc*, int64_t);
// SKILLITEM_GetPSpellInfoEntry(uint8_t expansion, D2UnitStrc* item) → D2PSpellInfoStrc*
using GetPSpellInfo_t = D2PSpellInfoStrc*(__fastcall*)(uint8_t, D2UnitStrc*);

static MissileSrvDo_t  g_OrigMissileSrvDo  = nullptr;
static MissileSrvHit_t g_OrigMissileSrvHit = nullptr;
static MissileSrvDmg_t g_OrigMissileSrvDmg = nullptr;
static SkillStartDispatch_t g_OrigSkillStart    = nullptr;
static SkillDispatch_t g_OrigSkillDo       = nullptr;
static ObjInit_t       g_OrigObjInit       = nullptr;
static ObjOperate_t    g_OrigObjOperate    = nullptr;
static ObjPopulate_t   g_OrigObjPopulate   = nullptr;
static PSpellEffect_t  g_OrigPSpellsDo     = nullptr;
static PSpellHandler_t g_OrigPSpellsSt     = nullptr;

// ── Index extraction ──────────────────────────────────────────────────────────

// D2GAME_sgptDataTables at exe+0x1e3a610: array of 8-byte pointers indexed by (tableIdx*2).
// Table type index at game+0x106. Missile class ID at missile+4 (Ghidra names it "unitFlags", actually dwClassId).
// Missile txt records: 0x1cc bytes each, array pointer at tableBase+0x1120, count at tableBase+0x1128.
static constexpr uint64_t OFF_D2GAME_sgptDataTables = 0x1e3a610;

static const uint8_t* GetMissileTxtRecord(const D2GameStrc* game, const D2UnitStrc* missile) noexcept {
    extern uintptr_t g_ExeBase;
    const auto* tables = reinterpret_cast<const uintptr_t*>(g_ExeBase + OFF_D2GAME_sgptDataTables);
    uint8_t tableIdx = *reinterpret_cast<const uint8_t*>(reinterpret_cast<const uint8_t*>(game) + 0x106);
    uintptr_t tableBase = tables[static_cast<ptrdiff_t>(tableIdx) * 2];
    if (!tableBase) return nullptr;
    uint32_t classId = *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(missile) + 4);
    uint64_t count = *reinterpret_cast<const uint64_t*>(tableBase + 0x1128);
    if (static_cast<uint64_t>(classId) >= count) return nullptr;
    uintptr_t arrayBase = *reinterpret_cast<const uintptr_t*>(tableBase + 0x1120);
    if (!arrayBase) return nullptr;
    return reinterpret_cast<const uint8_t*>(arrayBase + static_cast<uint64_t>(classId) * 0x1cc);
}

static uint32_t GetMissileSrvDoIdx(const D2GameStrc* game, const D2UnitStrc* missile) noexcept {
    const auto* rec = GetMissileTxtRecord(game, missile);
    return rec ? *reinterpret_cast<const uint16_t*>(rec + 0x2c) : 0;
}

static uint32_t GetMissileSrvHitIdx(const D2GameStrc* game, const D2UnitStrc* missile) noexcept {
    const auto* rec = GetMissileTxtRecord(game, missile);
    return rec ? *reinterpret_cast<const uint16_t*>(rec + 0x2e) : 0;
}

static uint32_t GetMissileSrvDmgIdx(const D2GameStrc* game, const D2UnitStrc* missile) noexcept {
    const auto* rec = GetMissileTxtRecord(game, missile);
    return rec ? *reinterpret_cast<const uint16_t*>(rec + 0x30) : 0;
}
// For skills, the index is passed directly as skillId (3rd __fastcall arg = R8).
static uint32_t GetSkillIdx(int32_t skillId) noexcept { return static_cast<uint32_t>(skillId); }

// D2UnitStrc+0x10 = pObjectData (D2ObjectDataStrc*); D2ObjectDataStrc+0x00 = pObjectsTxt.
// nInitFn/nPopulateFn/nOperateFn are byte fields in D2R's Objects.txt record at 0x15c/0x15d/0x15e.
static const uint8_t* GetObjTxtRecord(const D2UnitStrc* obj) noexcept {
    const auto* pObjData = *reinterpret_cast<const uint8_t* const*>(
        reinterpret_cast<const uint8_t*>(obj) + 0x10);
    if (!pObjData) return nullptr;
    return *reinterpret_cast<const uint8_t* const*>(pObjData);
}
static uint32_t GetObjInitIdx(const D2UnitStrc* obj) noexcept {
    const auto* pTxt = GetObjTxtRecord(obj);
    return pTxt ? pTxt[0x15c] : 0;
}
static uint32_t GetObjOperateIdx(const D2UnitStrc* obj) noexcept {
    const auto* pTxt = GetObjTxtRecord(obj);
    return pTxt ? pTxt[0x15e] : 0;
}
static uint32_t GetObjPopulateIdx(const D2UnitStrc* obj) noexcept {
    const auto* pTxt = GetObjTxtRecord(obj);
    return pTxt ? pTxt[0x15d] : 0;
}

// ── pSpell info lookup ────────────────────────────────────────────────────────

static const D2PSpellInfoStrc* GetPSpellInfoEntry(const D2GameStrc* game, const D2UnitStrc* item) noexcept {
    extern uintptr_t g_ExeBase;
    auto fn = reinterpret_cast<GetPSpellInfo_t>(g_ExeBase + OFF_SKILLITEM_GetPSpellInfoEntry);
    return fn(game->expansion, const_cast<D2UnitStrc*>(item));
}

// ── JS exception logger ───────────────────────────────────────────────────────

static void LogJSException(const char* callsite) {
    if (!g_ctx || !g_Context) return;
    JSValue exc = JS_GetException(g_ctx);
    JSValue str = JS_ToString(g_ctx, exc);
    const char* msg = JS_ToCString(g_ctx, str);
    if (msg) {
        D2RPluginLogErrorF(g_Context, "plugin-script: JS exception in %s: %s", callsite, msg);
        JS_FreeCString(g_ctx, msg);
    }
    JS_FreeValue(g_ctx, str);
    JS_FreeValue(g_ctx, exc);
}

// ── CallScript helpers ────────────────────────────────────────────────────────

static int32_t CallScript_i32_GameUnit(JSValue fn, D2GameStrc* game, D2UnitStrc* unit) {
    JSValue args[2] = {
        Script_MakeGameObject(g_ctx, game),
        Script_MakeUnitObject(g_ctx, unit),
    };
    JSValue ret = JS_Call(g_ctx, fn, JS_UNDEFINED, 2, args);
    JS_FreeValue(g_ctx, args[0]);
    JS_FreeValue(g_ctx, args[1]);
    int32_t result = 0;
    if (JS_IsException(ret)) {
        LogJSException("missile/skill callback");
    } else {
        JS_ToInt32(g_ctx, &result, ret);
    }
    JS_FreeValue(g_ctx, ret);
    return result;
}

static int32_t CallScript_i32_GameUnitUnitI32(JSValue fn, D2GameStrc* game, D2UnitStrc* u1,
                                               D2UnitStrc* u2, int32_t extra) {
    JSValue args[4] = {
        Script_MakeGameObject(g_ctx, game),
        Script_MakeUnitObject(g_ctx, u1),
        Script_MakeUnitObject(g_ctx, u2),
        JS_NewInt32(g_ctx, extra),
    };
    JSValue ret = JS_Call(g_ctx, fn, JS_UNDEFINED, 4, args);
    for (int i = 0; i < 4; ++i) JS_FreeValue(g_ctx, args[i]);
    int32_t result = 0;
    if (JS_IsException(ret)) {
        LogJSException("missile hit callback");
    } else {
        JS_ToInt32(g_ctx, &result, ret);
    }
    JS_FreeValue(g_ctx, ret);
    return result;
}

static void CallScript_void_GameUnitUnit(JSValue fn, D2GameStrc* game,
                                         D2UnitStrc* u1, D2UnitStrc* u2) {
    JSValue args[3] = {
        Script_MakeGameObject(g_ctx, game),
        Script_MakeUnitObject(g_ctx, u1),
        Script_MakeUnitObject(g_ctx, u2),
    };
    JSValue ret = JS_Call(g_ctx, fn, JS_UNDEFINED, 3, args);
    for (int i = 0; i < 3; ++i) JS_FreeValue(g_ctx, args[i]);
    if (JS_IsException(ret)) LogJSException("missile dmg callback");
    JS_FreeValue(g_ctx, ret);
}

static uint64_t CallScript_bool_GameUnitUnit(JSValue fn, D2GameStrc* game,
                                              D2UnitStrc* u1, D2UnitStrc* u2) {
    JSValue args[3] = {
        Script_MakeGameObject(g_ctx, game),
        Script_MakeUnitObject(g_ctx, u1),
        Script_MakeUnitObject(g_ctx, u2),
    };
    JSValue ret = JS_Call(g_ctx, fn, JS_UNDEFINED, 3, args);
    for (int i = 0; i < 3; ++i) JS_FreeValue(g_ctx, args[i]);
    uint64_t result = 0;
    if (JS_IsException(ret)) {
        LogJSException("pSpell handler callback");
    } else {
        int b = JS_ToBool(g_ctx, ret);
        result = (b > 0) ? 1 : 0;
    }
    JS_FreeValue(g_ctx, ret);
    return result;
}

static int32_t CallScript_i32_SkillFunc(JSValue fn, D2GameStrc* game, D2UnitStrc* unit,
                                         int32_t skillId, int32_t skillLevel) {
    JSValue args[4] = {
        Script_MakeGameObject(g_ctx, game),
        Script_MakeUnitObject(g_ctx, unit),
        JS_NewInt32(g_ctx, skillId),
        JS_NewInt32(g_ctx, skillLevel),
    };
    JSValue ret = JS_Call(g_ctx, fn, JS_UNDEFINED, 4, args);
    for (int i = 0; i < 4; ++i) JS_FreeValue(g_ctx, args[i]);
    int32_t result = 0;
    if (JS_IsException(ret)) {
        LogJSException("skill callback");
    } else {
        JS_ToInt32(g_ctx, &result, ret);
    }
    JS_FreeValue(g_ctx, ret);
    return result;
}

// ── Hook implementations ──────────────────────────────────────────────────────

static void __fastcall Hook_MissileSrvDo(D2GameStrc* game, D2UnitStrc* missile) {
    auto& reg = g_Registry[static_cast<int>(CallbackType::MissileSrvDo)];
    auto it = reg.find(GetMissileSrvDoIdx(game, missile));
    if (it != reg.end()) { CallScript_i32_GameUnit(it->second, game, missile); return; }
    g_OrigMissileSrvDo(game, missile);
}

static void __fastcall Hook_MissileSrvHit(D2GameStrc* game, D2UnitStrc* missile, void* target, int32_t a4) {
    auto& reg = g_Registry[static_cast<int>(CallbackType::MissileSrvHit)];
    auto it = reg.find(GetMissileSrvHitIdx(game, missile));
    if (it != reg.end()) { CallScript_i32_GameUnitUnitI32(it->second, game, missile, static_cast<D2UnitStrc*>(target), 0); return; }
    g_OrigMissileSrvHit(game, missile, target, a4);
}

static void __fastcall Hook_MissileSrvDmg(D2GameStrc* game, D2UnitStrc* missile,
                                           D2UnitStrc* target, void* dmg) {
    auto& reg = g_Registry[static_cast<int>(CallbackType::MissileSrvDmg)];
    auto it = reg.find(GetMissileSrvDmgIdx(game, missile));
    if (it != reg.end()) { CallScript_void_GameUnitUnit(it->second, game, missile, target); return; }
    g_OrigMissileSrvDmg(game, missile, target, dmg);
}

static int32_t __fastcall Hook_SkillStart(D2GameStrc* game, D2UnitStrc* unit,
                                           int32_t skillId, int32_t skillLevel,
                                           int32_t a5) {
    auto& reg = g_Registry[static_cast<int>(CallbackType::SrvSkillStart)];
    auto it = reg.find(GetSkillIdx(skillId));
    if (it != reg.end()) return CallScript_i32_SkillFunc(it->second, game, unit, skillId, skillLevel);
    return g_OrigSkillStart(game, unit, skillId, skillLevel, a5);
}

static int32_t __fastcall Hook_SkillDo(D2GameStrc* game, D2UnitStrc* unit,
                                        int32_t skillId, int32_t skillLevel,
                                        int32_t a5, int32_t a6, int32_t a7) {
    auto& reg = g_Registry[static_cast<int>(CallbackType::SrvSkillDo)];
    auto it = reg.find(GetSkillIdx(skillId));
    if (it != reg.end()) return CallScript_i32_SkillFunc(it->second, game, unit, skillId, skillLevel);
    return g_OrigSkillDo(game, unit, skillId, skillLevel, a5, a6, a7);
}

static void __fastcall Hook_ObjInit(D2GameStrc* game, D2UnitStrc* obj,
                                    int32_t unitId, void* room, int32_t x, int32_t y) {
    auto& reg = g_Registry[static_cast<int>(CallbackType::ObjInit)];
    auto it = reg.find(GetObjInitIdx(obj));
    if (it != reg.end()) { CallScript_i32_GameUnit(it->second, game, obj); return; }
    g_OrigObjInit(game, obj, unitId, room, x, y);
}

static int32_t __fastcall Hook_ObjOperate(D2GameStrc* game, void* player, D2UnitStrc* obj) {
    auto& reg = g_Registry[static_cast<int>(CallbackType::ObjOperate)];
    auto it = reg.find(GetObjOperateIdx(obj));
    if (it != reg.end()) return CallScript_i32_GameUnit(it->second, game, obj);
    return g_OrigObjOperate(game, player, obj);
}

static void __fastcall Hook_ObjPopulate(D2GameStrc* game, D2UnitStrc* obj) {
    auto& reg = g_Registry[static_cast<int>(CallbackType::ObjPopulate)];
    auto it = reg.find(GetObjPopulateIdx(obj));
    if (it != reg.end()) { CallScript_i32_GameUnit(it->second, game, obj); return; }
    g_OrigObjPopulate(game, obj);
}

// Recovers the misc.txt pSpell index by pointer arithmetic against g_pPSpellInfo.
static int32_t GetPSpellId(const D2GameStrc* game, const D2UnitStrc* item) noexcept {
    const D2PSpellInfoStrc* info = GetPSpellInfoEntry(game, item);
    if (!info) return -1;
    const auto* base = reinterpret_cast<const D2PSpellInfoStrc*>(g_ExeBase + OFF_G_PSPELLINFO);
    int32_t id = static_cast<int32_t>(info - base);
    return (id >= 0 && id < 20) ? id : -1;
}

static void __fastcall Hook_PSpellsDo(D2GameStrc* game, D2UnitStrc* caster,
                                       D2UnitStrc* item, D2UnitStrc* extra) {
    int32_t pSpellId = GetPSpellId(game, item);
    if (pSpellId >= 0) {
        auto& reg = g_Registry[static_cast<int>(CallbackType::PSpellsDo)];
        auto it = reg.find(static_cast<uint32_t>(pSpellId));
        if (it != reg.end()) { CallScript_bool_GameUnitUnit(it->second, game, caster, item); return; }
    }
    g_OrigPSpellsDo(game, caster, item, extra);
}

static uint64_t __fastcall Hook_PSpellsSt(D2GameStrc* game, D2UnitStrc* caster,
                                           D2UnitStrc* item, int64_t bForceUse) {
    int32_t pSpellId = GetPSpellId(game, item);
    if (pSpellId >= 0) {
        auto& reg = g_Registry[static_cast<int>(CallbackType::PSpellsSt)];
        auto it = reg.find(static_cast<uint32_t>(pSpellId));
        if (it != reg.end()) return CallScript_bool_GameUnitUnit(it->second, game, caster, item);
    }
    return g_OrigPSpellsSt(game, caster, item, bForceUse);
}

// ── Hook install/remove ───────────────────────────────────────────────────────

struct DispatchHookDef {
    uint64_t  offset;
    void*     hookFn;
    void**    originalOut;
    bool      hasRegistrations;
};

static bool s_HooksInstalled = false;

void Script_InstallDispatchHooks(const D2RLoaderPluginContext* context) {
    if (s_HooksInstalled) return;

    // Build list of hook definitions; skip any with offset == 0 or no registrations.
    DispatchHookDef defs[] = {
        { OFF_MissileSrvDoDispatcher,  reinterpret_cast<void*>(Hook_MissileSrvDo),  reinterpret_cast<void**>(&g_OrigMissileSrvDo),  !g_Registry[0].empty() },
        { OFF_MissileSrvHitDispatcher, reinterpret_cast<void*>(Hook_MissileSrvHit), reinterpret_cast<void**>(&g_OrigMissileSrvHit), !g_Registry[1].empty() },
        { OFF_MissileSrvDmgDispatcher, reinterpret_cast<void*>(Hook_MissileSrvDmg), reinterpret_cast<void**>(&g_OrigMissileSrvDmg), !g_Registry[2].empty() },
        { OFF_SkillStartDispatcher,    reinterpret_cast<void*>(Hook_SkillStart),    reinterpret_cast<void**>(&g_OrigSkillStart),    !g_Registry[3].empty() },
        { OFF_SkillDoDispatcher,       reinterpret_cast<void*>(Hook_SkillDo),       reinterpret_cast<void**>(&g_OrigSkillDo),       !g_Registry[4].empty() },
        { OFF_ObjInitDispatcher,       reinterpret_cast<void*>(Hook_ObjInit),       reinterpret_cast<void**>(&g_OrigObjInit),       !g_Registry[5].empty() },
        { OFF_ObjOperateDispatcher,    reinterpret_cast<void*>(Hook_ObjOperate),    reinterpret_cast<void**>(&g_OrigObjOperate),    !g_Registry[6].empty() },
        { OFF_ObjPopulateDispatcher,   reinterpret_cast<void*>(Hook_ObjPopulate),   reinterpret_cast<void**>(&g_OrigObjPopulate),   !g_Registry[7].empty() },
        { OFF_PSpellsDoDispatcher,     reinterpret_cast<void*>(Hook_PSpellsDo),     reinterpret_cast<void**>(&g_OrigPSpellsDo),     !g_Registry[8].empty() },
        { OFF_PSpellsStDispatcher,     reinterpret_cast<void*>(Hook_PSpellsSt),     reinterpret_cast<void**>(&g_OrigPSpellsSt),     !g_Registry[9].empty() },
    };

    for (auto& def : defs) {
        if (def.offset == 0 || !def.hasRegistrations) continue;
        if (!PSh_InstallHook(PLUGINID_SCRIPT, context, def.offset, def.hookFn, def.originalOut)) {
            D2RPluginLogErrorF(context, "plugin-script: failed to install hook at offset 0x%llX", def.offset);
        }
    }

    s_HooksInstalled = true;
}

void Script_RemoveDispatchHooks() {
    if (!s_HooksInstalled) return;

    const uint64_t offsets[] = {
        OFF_MissileSrvDoDispatcher,  OFF_MissileSrvHitDispatcher, OFF_MissileSrvDmgDispatcher,
        OFF_SkillStartDispatcher,    OFF_SkillDoDispatcher,
        OFF_ObjInitDispatcher,       OFF_ObjOperateDispatcher,    OFF_ObjPopulateDispatcher,
        OFF_PSpellsDoDispatcher,     OFF_PSpellsStDispatcher,
    };
    for (uint64_t off : offsets) {
        if (off != 0) PSh_RemoveHook(PLUGINID_SCRIPT, nullptr, off);
    }

    g_OrigMissileSrvDo  = nullptr;
    g_OrigMissileSrvHit = nullptr;
    g_OrigMissileSrvDmg = nullptr;
    g_OrigSkillStart    = nullptr;
    g_OrigSkillDo       = nullptr;
    g_OrigObjInit       = nullptr;
    g_OrigObjOperate    = nullptr;
    g_OrigObjPopulate   = nullptr;
    g_OrigPSpellsDo     = nullptr;
    g_OrigPSpellsSt     = nullptr;

    s_HooksInstalled = false;
}

void Script_ClearRegistry() {
    for (auto& map : g_Registry) {
        for (auto& [idx, fn] : map) JS_FreeValue(g_ctx, fn);
        map.clear();
    }
}
