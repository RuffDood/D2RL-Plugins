#include "script-types.h"
#include "script-dispatch.h"
#include <cstdint>

// ── Class IDs ─────────────────────────────────────────────────────────────────

static JSClassID s_D2UnitClassId = 0;
static JSClassID s_D2GameClassId = 0;

// ── D2Unit class ──────────────────────────────────────────────────────────────

static void D2Unit_Finalizer(JSRuntime* /*rt*/, JSValue val) {
    // The native pointer is not owned by us — nothing to free.
    (void)val;
}

// Generic getter that reads a uint32_t field at a given byte offset.
static JSValue D2Unit_GetU32(JSContext* ctx, JSValueConst this_val, int magic) {
    auto* unit = static_cast<D2UnitStrc*>(JS_GetOpaque(this_val, s_D2UnitClassId));
    if (!unit) return JS_ThrowInternalError(ctx, "D2Unit: null native pointer");
    return JS_NewUint32(ctx, *reinterpret_cast<const uint32_t*>(
        reinterpret_cast<const uint8_t*>(unit) + magic));
}

// unit.getStat(statId) — wraps PSh_GetStat
static JSValue D2Unit_GetStat(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* unit = static_cast<D2UnitStrc*>(JS_GetOpaque(this_val, s_D2UnitClassId));
    if (!unit) return JS_ThrowInternalError(ctx, "D2Unit: null native pointer");
    if (argc < 1) return JS_ThrowTypeError(ctx, "getStat requires statId argument");
    int32_t statId = 0;
    if (JS_ToInt32(ctx, &statId, argv[0])) return JS_EXCEPTION;
    if (!unit->statList) return JS_NewInt32(ctx, 0);
    extern uintptr_t g_ExeBase;
    return JS_NewInt32(ctx, PSh_GetStat(g_ExeBase, unit->statList, statId));
}

// unit.runSkill(game, skillId, skillLevel, consumeMana, fromItem, useTarget)
// Calls D2GAME_SKILLS_Handler (OFF_SkillDoDispatcher) directly.
static JSValue D2Unit_RunSkill(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* unit = static_cast<D2UnitStrc*>(JS_GetOpaque(this_val, s_D2UnitClassId));
    if (!unit) return JS_ThrowInternalError(ctx, "D2Unit: null native pointer");
    if (argc < 6) return JS_ThrowTypeError(ctx, "runSkill requires (game, skillId, skillLevel, consumeMana, fromItem, useTarget)");
    auto* game = static_cast<D2GameStrc*>(JS_GetOpaque(argv[0], s_D2GameClassId));
    if (!game) return JS_ThrowTypeError(ctx, "runSkill: first argument must be a D2Game");
    int32_t skillId = 0, skillLevel = 0;
    if (JS_ToInt32(ctx, &skillId, argv[1])) return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &skillLevel, argv[2])) return JS_EXCEPTION;
    int consumeMana = JS_ToBool(ctx, argv[3]);
    int fromItem    = JS_ToBool(ctx, argv[4]);
    int useTarget   = JS_ToBool(ctx, argv[5]);
    if (OFF_SkillDoDispatcher == 0) {
        if (g_Context) g_Context->logWarn("runSkill: SkillDoDispatcher offset not set");
        return JS_UNDEFINED;
    }
    extern uintptr_t g_ExeBase;
    using SkillDispatch_t = int32_t(__fastcall*)(D2GameStrc*, D2UnitStrc*, int32_t, int32_t, int32_t, int32_t, int32_t);
    reinterpret_cast<SkillDispatch_t>(g_ExeBase + OFF_SkillDoDispatcher)(
        game, unit, skillId, skillLevel, consumeMana, fromItem, useTarget);
    return JS_UNDEFINED;
}

// unit.setAnimMode(mode) — calls UNITS_ChangeAnimMode
static JSValue D2Unit_SetAnimMode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* unit = static_cast<D2UnitStrc*>(JS_GetOpaque(this_val, s_D2UnitClassId));
    if (!unit) return JS_ThrowInternalError(ctx, "D2Unit: null native pointer");
    if (argc < 1) return JS_ThrowTypeError(ctx, "setAnimMode requires mode argument");
    if (OFF_UNITS_ChangeAnimMode == 0) {
        if (g_Context) g_Context->logWarn("setAnimMode: UNITS_ChangeAnimMode offset not set");
        return JS_UNDEFINED;
    }
    int32_t mode = 0;
    if (JS_ToInt32(ctx, &mode, argv[0])) return JS_EXCEPTION;
    extern uintptr_t g_ExeBase;
    using ChangeAnimMode_t = void(__fastcall*)(void*, int32_t);
    reinterpret_cast<ChangeAnimMode_t>(g_ExeBase + OFF_UNITS_ChangeAnimMode)(unit, mode);
    return JS_UNDEFINED;
}

// unit.evaluateSkillCalc(game, calcIdx, skillId) — wraps SKILLS_EvaluateSkillFormula
static JSValue D2Unit_EvaluateSkillCalc(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* unit = static_cast<D2UnitStrc*>(JS_GetOpaque(this_val, s_D2UnitClassId));
    if (!unit) return JS_ThrowInternalError(ctx, "D2Unit: null native pointer");
    if (argc < 3) return JS_ThrowTypeError(ctx, "evaluateSkillCalc requires (game, calcIdx, skillId)");
    auto* game = static_cast<D2GameStrc*>(JS_GetOpaque(argv[0], s_D2GameClassId));
    if (!game) return JS_ThrowTypeError(ctx, "evaluateSkillCalc: first argument must be a D2Game");
    int32_t calcIdx = 0, skillId = 0;
    if (JS_ToInt32(ctx, &calcIdx, argv[1])) return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &skillId, argv[2])) return JS_EXCEPTION;
    if (OFF_SKILLS_EvaluateSkillFormula == 0) {
        if (g_Context) g_Context->logWarn("evaluateSkillCalc: SKILLS_EvaluateSkillFormula offset not set");
        return JS_NewInt32(ctx, 0);
    }
    extern uintptr_t g_ExeBase;
    using EvaluateSkillFormula_t = int32_t(__fastcall*)(uint8_t, D2UnitStrc*, int32_t, int32_t);
    int32_t result = reinterpret_cast<EvaluateSkillFormula_t>(g_ExeBase + OFF_SKILLS_EvaluateSkillFormula)(
        game->expansion, unit, calcIdx, skillId);
    return JS_NewInt32(ctx, result);
}

// unit.roll(range) — wraps PSh_RollUnit
static JSValue D2Unit_Roll(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* unit = static_cast<D2UnitStrc*>(JS_GetOpaque(this_val, s_D2UnitClassId));
    if (!unit) return JS_ThrowInternalError(ctx, "D2Unit: null native pointer");
    uint64_t raw = PSh_RollUnit(unit);
    if (argc >= 1) {
        int32_t range = 0;
        if (JS_ToInt32(ctx, &range, argv[0])) return JS_EXCEPTION;
        if (range > 0) return JS_NewInt32(ctx, static_cast<int32_t>(static_cast<uint32_t>(raw) % static_cast<uint32_t>(range)));
    }
    return JS_NewUint32(ctx, static_cast<uint32_t>(raw));
}

// ── D2Game class ──────────────────────────────────────────────────────────────

static void D2Game_Finalizer(JSRuntime* /*rt*/, JSValue val) {
    (void)val;
}

static JSValue D2Game_GetDifficulty(JSContext* ctx, JSValueConst this_val) {
    auto* game = static_cast<D2GameStrc*>(JS_GetOpaque(this_val, s_D2GameClassId));
    if (!game) return JS_ThrowInternalError(ctx, "D2Game: null native pointer");
    return JS_NewUint32(ctx, static_cast<uint32_t>(game->difficultyLevel));
}

static JSValue D2Game_GetExpansion(JSContext* ctx, JSValueConst this_val) {
    auto* game = static_cast<D2GameStrc*>(JS_GetOpaque(this_val, s_D2GameClassId));
    if (!game) return JS_ThrowInternalError(ctx, "D2Game: null native pointer");
	return JS_NewUint32(ctx, static_cast<uint32_t>(game->expansion));
}

static JSValue D2Game_Roll(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
	auto* game = static_cast<D2GameStrc*>(JS_GetOpaque(this_val, s_D2GameClassId));
	if (!game) return JS_ThrowInternalError(ctx, "D2Game: null native pointer");
	uint64_t raw = PSh_RollGame(game);
	if (argc >= 1) {
		int32_t range = 0;
		if (JS_ToInt32(ctx, &range, argv[0])) return JS_EXCEPTION;
		if (range > 0) return JS_NewInt32(ctx, static_cast<int32_t>(static_cast<uint32_t>(raw) % static_cast<uint32_t>(range)));
	}
	return JS_NewUint32(ctx, static_cast<uint32_t>(raw));
}

// ── Registration ──────────────────────────────────────────────────────────────

void Script_RegisterGameTypes(JSContext* ctx) {
    // ── D2Unit ────────────────────────────────────────────────────────────────

    JS_NewClassID(JS_GetRuntime(ctx), &s_D2UnitClassId);

    JSClassDef d2UnitDef{};
    d2UnitDef.class_name = "D2Unit";
    d2UnitDef.finalizer  = D2Unit_Finalizer;
    JS_NewClass(JS_GetRuntime(ctx), s_D2UnitClassId, &d2UnitDef);

    JSValue unitProto = JS_NewObject(ctx);

    // Getters using magic = byte offset into D2UnitStrc
    static const JSCFunctionListEntry unitProps[] = {
        JS_CGETSET_MAGIC_DEF("unitType",    D2Unit_GetU32, nullptr, offsetof(D2UnitStrc, dwUnitType)),
        JS_CGETSET_MAGIC_DEF("unitFlags",   D2Unit_GetU32, nullptr, offsetof(D2UnitStrc, unitFlags)),
        JS_CGETSET_MAGIC_DEF("animMode",    D2Unit_GetU32, nullptr, offsetof(D2UnitStrc, dwAnimMode)),
        JS_CGETSET_MAGIC_DEF("seedLow",     D2Unit_GetU32, nullptr, offsetof(D2UnitStrc, seedLow)),
        JS_CGETSET_MAGIC_DEF("seedHigh",    D2Unit_GetU32, nullptr, offsetof(D2UnitStrc, seedHigh)),
        JS_CGETSET_MAGIC_DEF("flags",       D2Unit_GetU32, nullptr, offsetof(D2UnitStrc, dwFlags)),
        // For item units, unitFlags == class ID == items.txt row index.
        JS_CGETSET_MAGIC_DEF("itemsRecord", D2Unit_GetU32, nullptr, offsetof(D2UnitStrc, unitFlags)),
        JS_CFUNC_DEF("getStat",            1, D2Unit_GetStat),
        JS_CFUNC_DEF("roll",               1, D2Unit_Roll),
        JS_CFUNC_DEF("setAnimMode",        1, D2Unit_SetAnimMode),
        JS_CFUNC_DEF("runSkill",           6, D2Unit_RunSkill),
        JS_CFUNC_DEF("evaluateSkillCalc",  3, D2Unit_EvaluateSkillCalc),
    };
    JS_SetPropertyFunctionList(ctx, unitProto, unitProps, std::size(unitProps));
    JS_SetClassProto(ctx, s_D2UnitClassId, unitProto);

    // ── D2Game ────────────────────────────────────────────────────────────────

    JS_NewClassID(JS_GetRuntime(ctx), &s_D2GameClassId);

    JSClassDef d2GameDef{};
    d2GameDef.class_name = "D2Game";
    d2GameDef.finalizer  = D2Game_Finalizer;
    JS_NewClass(JS_GetRuntime(ctx), s_D2GameClassId, &d2GameDef);

    JSValue gameProto = JS_NewObject(ctx);

    static const JSCFunctionListEntry gameProps[] = {
        JS_CGETSET_DEF("difficulty", D2Game_GetDifficulty, nullptr),
        JS_CGETSET_DEF("expansion",  D2Game_GetExpansion,  nullptr),
		JS_CFUNC_DEF("roll", 1, D2Game_Roll),
    };
    JS_SetPropertyFunctionList(ctx, gameProto, gameProps, std::size(gameProps));
    JS_SetClassProto(ctx, s_D2GameClassId, gameProto);
}

JSValue Script_MakeUnitObject(JSContext* ctx, D2UnitStrc* unit) {
    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(s_D2UnitClassId));
    JS_SetOpaque(obj, unit);
    return obj;
}

JSValue Script_MakeGameObject(JSContext* ctx, D2GameStrc* game) {
    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(s_D2GameClassId));
    JS_SetOpaque(obj, game);
    return obj;
}
