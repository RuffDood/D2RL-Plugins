#include "script-bindings.h"
#include "script-dispatch.h"
#include "script-types.h"
#include <cstring>

// ── Data table constants (offsets into sgptDataTables[expansion*2]) ───────────

static constexpr uint64_t OFF_SGPT_DATA_TABLES     = 0x1e3a610;
static constexpr uint32_t DATATBL_ITEMS_TXT_BASE   = 0x15a0; // D2ItemsTxt* record array
static constexpr uint32_t DATATBL_ITEMS_TXT_COUNT  = 0x15a8; // uint32_t record count
static constexpr uint32_t DATATBL_SKILLS_TXT_BASE  = 0x11b0; // D2SkillsTxt* record array
static constexpr uint32_t DATATBL_SKILLS_TXT_COUNT = 0x11b8; // uint64_t record count
static constexpr uint32_t DATATBL_SKILLS_TXT_STRIDE = 0x2ec; // sizeof(D2SkillsTxt)

// ── Logging ───────────────────────────────────────────────────────────────────

static JSValue Bind_Log(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv, int magic) {
    if (!g_Context || argc < 1) return JS_UNDEFINED;
    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_EXCEPTION;
    switch (magic) {
        case 0: g_Context->logInfo(str);  break;
        case 1: g_Context->logWarn(str);  break;
        case 2: g_Context->logError(str); break;
    }
    JS_FreeCString(ctx, str);
    return JS_UNDEFINED;
}

// ── TXT record getters ────────────────────────────────────────────────────────

// trap.getItemsRecord(idx) → D2ItemsTxt fields as a plain object, or undefined.
// D2R is always LoD expansion, so we always use the expansion=1 data table (index 2).
static JSValue Bind_GetItemsRecord(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "getItemsRecord requires idx argument");
    int32_t idx = 0;
    if (JS_ToInt32(ctx, &idx, argv[0])) return JS_EXCEPTION;
    if (idx < 0) return JS_UNDEFINED;

    extern uintptr_t g_ExeBase;
    const auto* tables = reinterpret_cast<const uintptr_t*>(g_ExeBase + OFF_SGPT_DATA_TABLES);
    uintptr_t tableBase = tables[2]; // expansion=1 → index 1*2=2
    if (!tableBase) return JS_UNDEFINED;

    uint32_t count = *reinterpret_cast<const uint32_t*>(tableBase + DATATBL_ITEMS_TXT_COUNT);
    if (static_cast<uint32_t>(idx) >= count) return JS_UNDEFINED;

    const auto* base = *reinterpret_cast<const D2ItemsTxt* const*>(tableBase + DATATBL_ITEMS_TXT_BASE);
    if (!base) return JS_UNDEFINED;

    const D2ItemsTxt& rec = base[idx];

    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "pSpell", JS_NewUint32(ctx, rec.dwPspell));
    JS_SetPropertyStr(ctx, obj, "state",  JS_NewUint32(ctx, rec.wState));
    JS_SetPropertyStr(ctx, obj, "len",    JS_NewUint32(ctx, rec.dwLen));

    JSValue statsArr = JS_NewArray(ctx);
    for (int i = 0; i < 3; ++i)
        JS_SetPropertyUint32(ctx, statsArr, static_cast<uint32_t>(i), JS_NewUint32(ctx, rec.wStat[i]));
    JS_SetPropertyStr(ctx, obj, "stats", statsArr);

    JSValue calcArr = JS_NewArray(ctx);
    for (int i = 0; i < 3; ++i)
        JS_SetPropertyUint32(ctx, calcArr, static_cast<uint32_t>(i), JS_NewUint32(ctx, rec.dwCalc[i]));
    JS_SetPropertyStr(ctx, obj, "calc", calcArr);

    return obj;
}

// trap.getSkillsRecord(idx) → D2SkillsTxt fields as a plain JS object, or undefined.
static JSValue Bind_GetSkillsRecord(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "getSkillsRecord requires idx argument");
    int32_t idx = 0;
    if (JS_ToInt32(ctx, &idx, argv[0])) return JS_EXCEPTION;
    if (idx < 0) return JS_UNDEFINED;

    extern uintptr_t g_ExeBase;
    const auto* tables = reinterpret_cast<const uintptr_t*>(g_ExeBase + OFF_SGPT_DATA_TABLES);
    uintptr_t tableBase = tables[2]; // expansion=1 → index 1*2=2
    if (!tableBase) return JS_UNDEFINED;

    uint64_t count = *reinterpret_cast<const uint64_t*>(tableBase + DATATBL_SKILLS_TXT_COUNT);
    if (static_cast<uint64_t>(idx) >= count) return JS_UNDEFINED;

    const auto* base = *reinterpret_cast<const D2SkillsTxt* const*>(tableBase + DATATBL_SKILLS_TXT_BASE);
    if (!base) return JS_UNDEFINED;

    const D2SkillsTxt& r = base[idx];
    JSValue obj = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, obj, "skillId",       JS_NewUint32(ctx, r.wSkillId));
    JS_SetPropertyStr(ctx, obj, "flags",          JS_NewUint32(ctx, r.dwFlags));
    JS_SetPropertyStr(ctx, obj, "charclass",      JS_NewUint32(ctx, r.bCharclass));
    JS_SetPropertyStr(ctx, obj, "monanim",        JS_NewUint32(ctx, r.bMonanim));
    JS_SetPropertyStr(ctx, obj, "seqtrans",       JS_NewUint32(ctx, r.bSeqtrans));
    JS_SetPropertyStr(ctx, obj, "seqnum",         JS_NewUint32(ctx, r.bSeqnum));
    JS_SetPropertyStr(ctx, obj, "range",          JS_NewUint32(ctx, r.bRange));
    JS_SetPropertyStr(ctx, obj, "selectProc",     JS_NewUint32(ctx, r.bSelectProc));
    JS_SetPropertyStr(ctx, obj, "seqinput",       JS_NewUint32(ctx, r.bSeqinput));
    JS_SetPropertyStr(ctx, obj, "srvstfunc",      JS_NewInt32(ctx, r.nSrvstfunc));
    JS_SetPropertyStr(ctx, obj, "srvdofunc",      JS_NewInt32(ctx, r.nSrvdofunc));
    JS_SetPropertyStr(ctx, obj, "prgdam",         JS_NewUint32(ctx, r.bPrgdam));
    JS_SetPropertyStr(ctx, obj, "srvmissile",     JS_NewInt32(ctx, r.nSrvmissile));
    JS_SetPropertyStr(ctx, obj, "srvmissilea",    JS_NewInt32(ctx, r.nSrvmissilea));
    JS_SetPropertyStr(ctx, obj, "srvmissileb",    JS_NewInt32(ctx, r.nSrvmissileb));
    JS_SetPropertyStr(ctx, obj, "srvmissilec",    JS_NewInt32(ctx, r.nSrvmissilec));
    JS_SetPropertyStr(ctx, obj, "srvoverlay",     JS_NewInt32(ctx, r.nSrvoverlay));
    JS_SetPropertyStr(ctx, obj, "auraFilter",     JS_NewInt32(ctx, r.nAuraFilter));
    JS_SetPropertyStr(ctx, obj, "auraLenCalc",    JS_NewInt32(ctx, r.nAuraLenCalc));
    JS_SetPropertyStr(ctx, obj, "auraRangeCalc",  JS_NewInt32(ctx, r.nAuraRangeCalc));
    JS_SetPropertyStr(ctx, obj, "aurastate",      JS_NewInt32(ctx, r.nAurastate));
    JS_SetPropertyStr(ctx, obj, "auraTargetState",JS_NewInt32(ctx, r.nAuraTargetState));
    JS_SetPropertyStr(ctx, obj, "passivestate",   JS_NewInt32(ctx, r.nPassivestate));
    JS_SetPropertyStr(ctx, obj, "passiveitype",   JS_NewInt32(ctx, r.nPassiveitype));
    JS_SetPropertyStr(ctx, obj, "passivereqweaponcount", JS_NewUint32(ctx, r.bPassivereqweaponcount));
    JS_SetPropertyStr(ctx, obj, "summon",         JS_NewInt32(ctx, r.nSummon));
    JS_SetPropertyStr(ctx, obj, "pettype",        JS_NewUint32(ctx, r.bPettype));
    JS_SetPropertyStr(ctx, obj, "summode",        JS_NewUint32(ctx, r.bSummode));
    JS_SetPropertyStr(ctx, obj, "petmax",         JS_NewInt32(ctx, r.nPetmax));
    JS_SetPropertyStr(ctx, obj, "sumumod",        JS_NewInt32(ctx, r.nSumumod));
    JS_SetPropertyStr(ctx, obj, "sumoverlay",     JS_NewInt32(ctx, r.nSumoverlay));
    JS_SetPropertyStr(ctx, obj, "cltmissile",     JS_NewInt32(ctx, r.nCltmissile));
    JS_SetPropertyStr(ctx, obj, "cltmissilea",    JS_NewInt32(ctx, r.nCltmissilea));
    JS_SetPropertyStr(ctx, obj, "cltmissileb",    JS_NewInt32(ctx, r.nCltmissileb));
    JS_SetPropertyStr(ctx, obj, "cltmissilec",    JS_NewInt32(ctx, r.nCltmissilec));
    JS_SetPropertyStr(ctx, obj, "cltmissiled",    JS_NewInt32(ctx, r.nCltmissiled));
    JS_SetPropertyStr(ctx, obj, "cltstfunc",      JS_NewInt32(ctx, r.nCltstfunc));
    JS_SetPropertyStr(ctx, obj, "cltdofunc",      JS_NewInt32(ctx, r.nCltdofunc));
    JS_SetPropertyStr(ctx, obj, "stsound",        JS_NewInt32(ctx, r.nStsound));
    JS_SetPropertyStr(ctx, obj, "stsoundclass",   JS_NewInt32(ctx, r.nStsoundclass));
    JS_SetPropertyStr(ctx, obj, "dosound",        JS_NewInt32(ctx, r.nDosound));
    JS_SetPropertyStr(ctx, obj, "dosound_a",      JS_NewInt32(ctx, r.nDosound_a));
    JS_SetPropertyStr(ctx, obj, "dosound_b",      JS_NewInt32(ctx, r.nDosound_b));
    JS_SetPropertyStr(ctx, obj, "castoverlay",    JS_NewInt32(ctx, r.nCastoverlay));
    JS_SetPropertyStr(ctx, obj, "tgtoverlay",     JS_NewInt32(ctx, r.nTgtoverlay));
    JS_SetPropertyStr(ctx, obj, "tgtsound",       JS_NewInt32(ctx, r.nTgtsound));
    JS_SetPropertyStr(ctx, obj, "prgoverlay",     JS_NewInt32(ctx, r.nPrgoverlay));
    JS_SetPropertyStr(ctx, obj, "prgsound",       JS_NewInt32(ctx, r.nPrgsound));
    JS_SetPropertyStr(ctx, obj, "cltoverlaya",    JS_NewInt32(ctx, r.nCltoverlaya));
    JS_SetPropertyStr(ctx, obj, "cltoverlayb",    JS_NewInt32(ctx, r.nCltoverlayb));
    JS_SetPropertyStr(ctx, obj, "itemTarget",     JS_NewUint32(ctx, r.bItemTarget));
    JS_SetPropertyStr(ctx, obj, "itemCastSound",  JS_NewInt32(ctx, r.nItemCastSound));
    JS_SetPropertyStr(ctx, obj, "itemCastOverlay",JS_NewInt32(ctx, r.nItemCastOverlay));
    JS_SetPropertyStr(ctx, obj, "perdelay",       JS_NewInt32(ctx, r.nPerdelay));
    JS_SetPropertyStr(ctx, obj, "maxlvl",         JS_NewInt32(ctx, r.nMaxlvl));
    JS_SetPropertyStr(ctx, obj, "resultFlags",    JS_NewInt32(ctx, r.nResultFlags));
    JS_SetPropertyStr(ctx, obj, "hitFlags",       JS_NewInt32(ctx, r.nHitFlags));
    JS_SetPropertyStr(ctx, obj, "hitClass",       JS_NewInt32(ctx, r.nHitClass));
    JS_SetPropertyStr(ctx, obj, "weapsel",        JS_NewUint32(ctx, r.bWeapsel));
    JS_SetPropertyStr(ctx, obj, "itemEffect",     JS_NewInt32(ctx, r.nItemEffect));
    JS_SetPropertyStr(ctx, obj, "itemCltEffect",  JS_NewInt32(ctx, r.nItemCltEffect));
    JS_SetPropertyStr(ctx, obj, "skpoints",       JS_NewInt32(ctx, r.nSkpoints));
    JS_SetPropertyStr(ctx, obj, "reqlevel",       JS_NewInt32(ctx, r.nReqlevel));
    JS_SetPropertyStr(ctx, obj, "reqstr",         JS_NewInt32(ctx, r.nReqstr));
    JS_SetPropertyStr(ctx, obj, "reqdex",         JS_NewInt32(ctx, r.nReqdex));
    JS_SetPropertyStr(ctx, obj, "reqint",         JS_NewInt32(ctx, r.nReqint));
    JS_SetPropertyStr(ctx, obj, "reqvit",         JS_NewInt32(ctx, r.nReqvit));
    JS_SetPropertyStr(ctx, obj, "startmana",      JS_NewInt32(ctx, r.nStartmana));
    JS_SetPropertyStr(ctx, obj, "minmana",        JS_NewInt32(ctx, r.nMinmana));
    JS_SetPropertyStr(ctx, obj, "manashift",      JS_NewInt32(ctx, r.nManashift));
    JS_SetPropertyStr(ctx, obj, "mana",           JS_NewInt32(ctx, r.nMana));
    JS_SetPropertyStr(ctx, obj, "lvlmana",        JS_NewInt32(ctx, r.nLvlmana));
    JS_SetPropertyStr(ctx, obj, "prgchargestocast",   JS_NewUint32(ctx, r.bPrgchargestocast));
    JS_SetPropertyStr(ctx, obj, "prgchargesconsumed", JS_NewUint32(ctx, r.bPrgchargesconsumed));
    JS_SetPropertyStr(ctx, obj, "attackrank",     JS_NewUint32(ctx, r.bAttackrank));
    JS_SetPropertyStr(ctx, obj, "lineofsight",    JS_NewUint32(ctx, r.bLineofsight));
    JS_SetPropertyStr(ctx, obj, "globalDelay",    JS_NewInt32(ctx, r.nGlobalDelay));
    JS_SetPropertyStr(ctx, obj, "localdelay",     JS_NewInt32(ctx, r.nLocaldelay));
    JS_SetPropertyStr(ctx, obj, "skilldesc",      JS_NewInt32(ctx, r.nSkilldesc));
    JS_SetPropertyStr(ctx, obj, "toHit",          JS_NewInt32(ctx, r.nToHit));
    JS_SetPropertyStr(ctx, obj, "levToHit",       JS_NewInt32(ctx, r.nLevToHit));
    JS_SetPropertyStr(ctx, obj, "toHitCalc",      JS_NewInt32(ctx, r.nToHitCalc));
    JS_SetPropertyStr(ctx, obj, "hitShift",       JS_NewUint32(ctx, r.bHitShift));
    JS_SetPropertyStr(ctx, obj, "srcDam",         JS_NewUint32(ctx, r.bSrcDam));
    JS_SetPropertyStr(ctx, obj, "minDam",         JS_NewInt32(ctx, r.nMinDam));
    JS_SetPropertyStr(ctx, obj, "maxDam",         JS_NewInt32(ctx, r.nMaxDam));
    JS_SetPropertyStr(ctx, obj, "dmgSymPerCalc",  JS_NewInt32(ctx, r.nDmgSymPerCalc));
    JS_SetPropertyStr(ctx, obj, "eType",          JS_NewUint32(ctx, r.bEType));
    JS_SetPropertyStr(ctx, obj, "eMinDam",        JS_NewInt32(ctx, r.nEMinDam));
    JS_SetPropertyStr(ctx, obj, "eMaxDam",        JS_NewInt32(ctx, r.nEMaxDam));
    JS_SetPropertyStr(ctx, obj, "eDmgSymPerCalc", JS_NewInt32(ctx, r.nEDmgSymPerCalc));
    JS_SetPropertyStr(ctx, obj, "eLevLen",        JS_NewInt32(ctx, r.nELevLen));
    JS_SetPropertyStr(ctx, obj, "eLevLen1",       JS_NewInt32(ctx, r.nELevLen1));
    JS_SetPropertyStr(ctx, obj, "eLevLen2",       JS_NewInt32(ctx, r.nELevLen2));
    JS_SetPropertyStr(ctx, obj, "eLevLen3",       JS_NewInt32(ctx, r.nELevLen3));
    JS_SetPropertyStr(ctx, obj, "eLenSymPerCalc", JS_NewInt32(ctx, r.nELenSymPerCalc));
    JS_SetPropertyStr(ctx, obj, "restrict",       JS_NewUint32(ctx, r.bRestrict));
    JS_SetPropertyStr(ctx, obj, "aitype",         JS_NewUint32(ctx, r.bAitype));
    JS_SetPropertyStr(ctx, obj, "aibonus",        JS_NewInt32(ctx, r.nAibonus));
    JS_SetPropertyStr(ctx, obj, "cost_mult",      JS_NewInt32(ctx, r.nCost_mult));
    JS_SetPropertyStr(ctx, obj, "cost_add",       JS_NewInt32(ctx, r.nCost_add));
    JS_SetPropertyStr(ctx, obj, "useServerMissilesOnRemoteClients", JS_NewUint32(ctx, r.bUseServerMissilesOnRemoteClients));
    JS_SetPropertyStr(ctx, obj, "srvstopfunc",    JS_NewInt32(ctx, r.nSrvstopfunc));
    JS_SetPropertyStr(ctx, obj, "cltstopfunc",    JS_NewInt32(ctx, r.nCltstopfunc));

    // Array fields
    auto makeI16Arr = [&](const int16_t* data, uint32_t len) {
        JSValue arr = JS_NewArray(ctx);
        for (uint32_t i = 0; i < len; ++i)
            JS_SetPropertyUint32(ctx, arr, i, JS_NewInt32(ctx, data[i]));
        return arr;
    };
    auto makeI32Arr = [&](const int32_t* data, uint32_t len) {
        JSValue arr = JS_NewArray(ctx);
        for (uint32_t i = 0; i < len; ++i)
            JS_SetPropertyUint32(ctx, arr, i, JS_NewInt32(ctx, data[i]));
        return arr;
    };

    JS_SetPropertyStr(ctx, obj, "itypea",       makeI16Arr(r.nItypea, 3));
    JS_SetPropertyStr(ctx, obj, "itypeb",       makeI16Arr(r.nItypeb, 3));
    JS_SetPropertyStr(ctx, obj, "etypea",       makeI16Arr(r.nEtypea, 2));
    JS_SetPropertyStr(ctx, obj, "etypeb",       makeI16Arr(r.nEtypeb, 2));
    JS_SetPropertyStr(ctx, obj, "srvprgfunc",   makeI16Arr(r.nSrvprgfunc, 3));
    JS_SetPropertyStr(ctx, obj, "prgcalc",      makeI32Arr(r.nPrgcalc, 3));
    JS_SetPropertyStr(ctx, obj, "auraStat",     makeI16Arr(r.nAuraStat, 6));
    JS_SetPropertyStr(ctx, obj, "auraStatCalc", makeI32Arr(r.nAuraStatCalc, 6));
    JS_SetPropertyStr(ctx, obj, "auraevent",    makeI16Arr(r.nAuraevent, 4));
    JS_SetPropertyStr(ctx, obj, "auraeventfunc",makeI16Arr(r.nAuraeventfunc, 4));
    JS_SetPropertyStr(ctx, obj, "passivestat",  makeI16Arr(r.nPassivestat, 14));
    JS_SetPropertyStr(ctx, obj, "passivecalc",  makeI32Arr(r.nPassivecalc, 14));
    JS_SetPropertyStr(ctx, obj, "sumskill",     makeI16Arr(r.nSumskill, 5));
    JS_SetPropertyStr(ctx, obj, "sumsk_calc",   makeI32Arr(r.nSumsk_calc, 5));
    JS_SetPropertyStr(ctx, obj, "cltprgfunc",   makeI16Arr(r.nCltprgfunc, 3));
    JS_SetPropertyStr(ctx, obj, "cltcalc",      makeI32Arr(r.nCltcalc, 3));
    JS_SetPropertyStr(ctx, obj, "calc",         makeI32Arr(r.nCalc, 10));
    JS_SetPropertyStr(ctx, obj, "param",        makeI32Arr(r.nParam, 20));
    JS_SetPropertyStr(ctx, obj, "reqskill",     makeI16Arr(r.nReqskill, 3));
    JS_SetPropertyStr(ctx, obj, "minLevDam",    makeI32Arr(r.nMinLevDam, 5));
    JS_SetPropertyStr(ctx, obj, "maxLevDam",    makeI32Arr(r.nMaxLevDam, 5));
    JS_SetPropertyStr(ctx, obj, "eMinLev",      makeI32Arr(r.nEMinLev, 5));
    JS_SetPropertyStr(ctx, obj, "eMaxLev",      makeI32Arr(r.nEMaxLev, 5));
    JS_SetPropertyStr(ctx, obj, "state",        makeI16Arr(r.nState, 3));

    return obj;
}

// ── Callback registration helpers ─────────────────────────────────────────────

static JSValue Bind_SetCallback(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv, int magic) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "expected (index, function)");
    int32_t idx = 0;
    if (JS_ToInt32(ctx, &idx, argv[0])) return JS_EXCEPTION;
    if (idx < 0) return JS_ThrowRangeError(ctx, "callback index must be >= 0");
    if (!JS_IsFunction(ctx, argv[1]))
        return JS_ThrowTypeError(ctx, "second argument must be a function");

    auto& reg = g_Registry[magic];
    auto it = reg.find(static_cast<uint32_t>(idx));
    if (it != reg.end()) JS_FreeValue(ctx, it->second);
    reg[static_cast<uint32_t>(idx)] = JS_DupValue(ctx, argv[1]);
    return JS_UNDEFINED;
}

// setPSpell(pSpellId, stFunc|null, doFunc|null)
// pSpellId = misc.txt pSpell column value (0–19).
// Registers stFunc in PSpellsSt and doFunc in PSpellsDo at that index.
// null clears any existing registration; omitting an arg leaves it unchanged.
static JSValue Bind_SetPSpell(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "expected (pSpellId, stFunc|null, doFunc|null)");
    int32_t pSpellId = 0;
    if (JS_ToInt32(ctx, &pSpellId, argv[0])) return JS_EXCEPTION;
    if (pSpellId < 0 || pSpellId >= 20)
        return JS_ThrowRangeError(ctx, "pSpellId must be 0–19");

    auto setOrClear = [&](int regIdx, JSValueConst fn) -> JSValue {
        auto& reg = g_Registry[regIdx];
        auto it = reg.find(static_cast<uint32_t>(pSpellId));
        if (JS_IsNull(fn)) {
            if (it != reg.end()) { JS_FreeValue(ctx, it->second); reg.erase(it); }
        } else {
            if (!JS_IsFunction(ctx, fn))
                return JS_ThrowTypeError(ctx, "pSpell callback must be a function or null");
            if (it != reg.end()) JS_FreeValue(ctx, it->second);
            reg[static_cast<uint32_t>(pSpellId)] = JS_DupValue(ctx, fn);
        }
        return JS_UNDEFINED;
    };

    if (argc >= 2) {
        JSValue r = setOrClear(static_cast<int>(CallbackType::PSpellsSt), argv[1]);
        if (JS_IsException(r)) return r;
    }
    if (argc >= 3) {
        JSValue r = setOrClear(static_cast<int>(CallbackType::PSpellsDo), argv[2]);
        if (JS_IsException(r)) return r;
    }
    return JS_UNDEFINED;
}

// ── Registration ──────────────────────────────────────────────────────────────

void Script_RegisterTrapObject(JSContext* ctx) {
    static const JSCFunctionListEntry trapFuncs[] = {
        // Logging (magic = 0/1/2 for info/warn/error)
        JS_CFUNC_MAGIC_DEF("logInfo",  1, Bind_Log, 0),
        JS_CFUNC_MAGIC_DEF("logWarn",  1, Bind_Log, 1),
        JS_CFUNC_MAGIC_DEF("logError", 1, Bind_Log, 2),

        // TXT record getters
        JS_CFUNC_DEF("getItemsRecord", 1, Bind_GetItemsRecord),
        JS_CFUNC_DEF("getSkillsRecord", 1, Bind_GetSkillsRecord),

        // Callback registration (magic = CallbackType enum value)
        JS_CFUNC_MAGIC_DEF("setMissileDo",   2, Bind_SetCallback, static_cast<int>(CallbackType::MissileSrvDo)),
        JS_CFUNC_MAGIC_DEF("setMissileHit",  2, Bind_SetCallback, static_cast<int>(CallbackType::MissileSrvHit)),
        JS_CFUNC_MAGIC_DEF("setMissileDmg",  2, Bind_SetCallback, static_cast<int>(CallbackType::MissileSrvDmg)),
        JS_CFUNC_MAGIC_DEF("setSkillStart",  2, Bind_SetCallback, static_cast<int>(CallbackType::SrvSkillStart)),
        JS_CFUNC_MAGIC_DEF("setSkillDo",     2, Bind_SetCallback, static_cast<int>(CallbackType::SrvSkillDo)),
        JS_CFUNC_MAGIC_DEF("setObjInit",     2, Bind_SetCallback, static_cast<int>(CallbackType::ObjInit)),
        JS_CFUNC_MAGIC_DEF("setObjOperate",  2, Bind_SetCallback, static_cast<int>(CallbackType::ObjOperate)),
        JS_CFUNC_MAGIC_DEF("setObjPopulate", 2, Bind_SetCallback, static_cast<int>(CallbackType::ObjPopulate)),
        JS_CFUNC_DEF("setPSpell", 3, Bind_SetPSpell),
    };

    JSValue trap = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, trap, trapFuncs, std::size(trapFuncs));

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "trap", trap);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, trap);
}
