#pragma once

#include "script-private.h"
#include <unordered_map>
#include <cstdint>

// ── Callback types ────────────────────────────────────────────────────────────

enum class CallbackType : uint8_t {
    MissileSrvDo,
    MissileSrvHit,
    MissileSrvDmg,
    SrvSkillStart,
    SrvSkillDo,
    ObjInit,
    ObjOperate,
    ObjPopulate,
    PSpellsDo,
    PSpellsSt,
    Count,
};

// Per-type registry: game table index → JSValue (a JS function, ref-counted).
// Entries are inserted during script execution (trap.set*) and freed on unload.
extern std::unordered_map<uint32_t, JSValue> g_Registry[static_cast<int>(CallbackType::Count)];

// ── Dispatcher offsets (from exeBase) ────────────────────────────────────────
// Fill these in from Ghidra analysis of the D2R executable.
// Each offset is the address of the function that reads the callback table index
// from the unit/game state and dispatches into the function pointer table.
// When these are 0, that callback type is not hooked.

static constexpr uint64_t OFF_MissileSrvDoDispatcher  = 0x3d26b0; // FUN_1403d26b0 — 2-param dispatcher (game, missile), reads pSrvDoFunc at missile txt+0x2c
static constexpr uint64_t OFF_MissileSrvHitDispatcher = 0x3d1910; // FUN_1403d1910 — combined SrvDmgHit handler (game, missile, target, a4), reads pSrvHitFunc at missile txt+0x2e
static constexpr uint64_t OFF_MissileSrvDmgDispatcher = 0; // SrvDmg is dispatched inside FUN_1403d1910 at missile txt+0x30; no separate hookable entry point found yet
static constexpr uint64_t OFF_SkillStartDispatcher    = 0x311be0; // D2GAME_SkillHandler1 — 5-param (game, unit, skillId, skillLvl, param5); dispatches via g_pSkillSrvStartFnTable[stfunc]
static constexpr uint64_t OFF_SkillDoDispatcher       = 0x311dd0; // D2GAME_SKILLS_Handler_6FD12BA0 — 7-param (game, unit, skillId, skillLvl, consumeMana, fromItem, useTarget)
static constexpr uint64_t OFF_ObjInitDispatcher       = 0x363b90; // OBJECTS_InitHandler — 6-param dispatcher
static constexpr uint64_t OFF_ObjOperateDispatcher    = 0x442600; // OBJECTS_OperateHandler — 3-param dispatcher
static constexpr uint64_t OFF_ObjPopulateDispatcher   = 0; // TODO: not yet named in Ghidra
static constexpr uint64_t OFF_PSpellsDoDispatcher     = 0x3f31f0; // SKILLITEM_pSpell_EffectDispatch — 4-param (game, caster, item, extra), dispatches by D2PSpellInfoStrc::nPSpellId (0–15)
static constexpr uint64_t OFF_PSpellsStDispatcher     = 0x3f2ff0; // SKILLITEM_pSpell_Handler — 4-param (game, caster, item, bForceUse), dispatches by D2PSpellInfoStrc::nDispatchIndex (0–10); return non-zero to allow effect

// Game utility functions called from host bindings.
static constexpr uint64_t OFF_UNITS_ChangeAnimMode            = 0x251660; // UNITS_ChangeAnimMode
static constexpr uint64_t OFF_SKILLITEM_GetPSpellInfoEntry    = 0x214710; // SKILLITEM_GetPSpellInfoEntry(uint8_t expansion, D2UnitStrc* item) → D2PSpellInfoStrc*
static constexpr uint64_t OFF_G_PSPELLINFO                   = 0x19e7940; // g_pPSpellInfo: D2PSpellInfoStrc[20] — index = misc.txt pSpell column value
static constexpr uint64_t OFF_SKILLS_EvaluateSkillFormula     = 0x2a70f0; // SKILLS_EvaluateSkillFormula(bExpansion, unit, calcIdx, skillId) → int32

// ── Public interface ──────────────────────────────────────────────────────────

// Install hooks for all callback types that have a non-zero offset AND at least
// one registered entry. Call after all scripts have been loaded.
void Script_InstallDispatchHooks(const D2RLoaderPluginContext* context);

// Remove all installed dispatch hooks. Safe to call when none are installed.
void Script_RemoveDispatchHooks();

// Free all JSValues in all registry maps and clear them.
void Script_ClearRegistry();
