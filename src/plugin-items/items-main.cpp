#include "plugin.h"
#include "items-private.h"
#include <cstring>
#include <vector>
#include <windows.h>

// ── Addresses (offsets from exe base 0x140000000) ────────────────────────────

static constexpr uint64_t OFF_MagicItemsSpawnIdentified  = 0x319B80;
static constexpr uint64_t OFF_RareItemsSpawnIdentified   = 0x433494;
static constexpr uint64_t OFF_GetInventoryGoldLimit      = 0x256290; // D2GAME_GetInventoryGoldLimit
// Jump table for the quality switch inside FUN_140244e30 (ITEMS_FindMatchingRuneword).
// Instruction: MOV ECX,[R13 + RAX*4 + 0x2450b4]  where R13=imageBase, RAX=(quality-4).
// Six 4-byte entries covering quality 4 (Magic) through 9 (Tempered).
// Writing 0x00244E95 to an entry redirects that quality to the "pass" branch
// (the instruction immediately after the switch, where rune matching proceeds normally).
static constexpr uint64_t OFF_GoldPenaltyCall            = 0x3968AA; // lone CALL site that applies the gold penalty
static constexpr uint64_t OFF_RunewordQualityJumpTable   = 0x2450B4;
static constexpr unsigned char RUNEWORD_QUALITY_PASS[]   = { 0x95, 0x4E, 0x24, 0x00 }; // RVA 0x244E95 LE

static constexpr uint64_t OFF_FillStoreInventory         = 0x3e7e70; // D2GAME_NPC_FillStoreInventory
static constexpr uint64_t OFF_GenerateStoreItem          = 0x3e7200; // D2GAME_NPC_GenerateStoreItem
static constexpr uint64_t OFF_ComputeItemLevel           = 0x3e7dc0; // item level cap helper
static constexpr uint64_t OFF_GetItemIdFromCode          = 0x7b6a90; // items.txt code → ID
static constexpr uint64_t OFF_GetMaxStack                = 0x243cf0; // get total max stack for quivers
static constexpr uint64_t OFF_SetUnitStat                = 0x2272f0; // STATLIST_SetUnitStat
static constexpr uint64_t OFF_FillGamble                 = 0x3eee20; // D2GAME_STORES_FillGamble
static constexpr uint64_t OFF_FillGamble_JgeOpcode       = 0x3ef024; // JGE byte that enforces ring/amulet
static constexpr uint64_t OFF_sgptDataTables             = 0x1e3a610;
static constexpr uint64_t OFF_CompileTxt                 = 0x21c680;  // DATATBLS_CompileTxt
static constexpr uint64_t OFF_CompileTxtCallInTC         = 0x2850c2;  // CompileTxt call site inside TC compiler (FUN_140284a40)
static constexpr uint64_t OFF_CreateCompiledTCStruct     = 0x283d10;  // FUN_140283d10 (raw record → compiled TC struct)
static constexpr uint64_t OFF_CreateCompiledTCStructCall = 0x285164;  // call site of FUN_140283d10 inside TC compiler loop
static constexpr uint64_t OFF_TCDropFunction             = 0x382ad0;  // FUN_140382ad0 (monster death drops entry)
static constexpr uint64_t OFF_ConditionGate              = 0x321b90;  // FUN_140321b90 (TC condition gate, receives compiled struct)
static constexpr uint64_t OFF_ConditionCalcEval          = 0x2a7950;  // FUN_1402a7950 (ConditionCalc expression evaluator)
static constexpr uint64_t OFF_PhysResist                 = 0x3B31CD;
static constexpr uint64_t OFF_ElementalResist = 0x3B31D5;
static constexpr uint64_t OFF_AbsorbCap = 0x3B3346;

// ── TC raw record layout (stride 0x304, output of DATATBLS_CompileTxt) ─────
static constexpr uint64_t TC_RECORD_STRIDE               = 0x304;
static constexpr uint32_t TCREC_CONDCALC_OFF             = 0x2fc;  // uint32_t compiled expression index
static constexpr uint32_t TCREC_USEPLAYER_OFF            = 0x301;  // uint8_t UsePlayerForConditionCalc flag

// Vendor difficulty upgrade constants inside D2GAME_NPC_GenerateStoreItem (0x3e7200).
// *ScaleByte = the shift-count operand in a SHL EAX,n instruction (1 byte).
// *BaseImm   = the imm32 operand in an ADD EAX,imm instruction (4 bytes, little-endian).
static constexpr uint64_t OFF_NMUberScaleByte     = 0x3e72fe; // SHL EAX,0x6  → ×64
static constexpr uint64_t OFF_NMUberBaseImm       = 0x3e7300; // ADD EAX,0xfa0 → +4000
static constexpr uint64_t OFF_HellUltraScaleByte  = 0x3e73d4; // SHL EAX,0x4  → ×16
static constexpr uint64_t OFF_HellUltraBaseImm    = 0x3e73d6; // ADD EAX,0x3e8 → +1000
static constexpr uint64_t OFF_HellUberScaleByte   = 0x3e7428; // SHL EAX,0x7  → ×128
static constexpr uint64_t OFF_HellUberBaseImm     = 0x3e742a; // ADD EAX,0x1388 → +5000

// ── D2ItemsTxt field offsets (confirmed from Ghidra, stride = 0x1c0) ─────────
static constexpr uint32_t ITEMREC_STRIDE      = 0x1c0;
//static constexpr uint32_t ITEMREC_LEVEL       = 0x10d; // bLevel  (uint8_t)
//static constexpr uint32_t ITEMREC_VERSION     = 0x0fe; // wVersion (uint16_t)
//static constexpr uint32_t ITEMREC_BITFIELD1   = 0x0e4; // dwBitField1 (uint32_t)
//static constexpr uint32_t ITEMREC_CODE        = 0x080; // dwCode (uint32_t) — used for quiver check

// ── Data-table field offsets (relative to sgptDataTables[expansion*2]) ───────
static constexpr uint32_t DATATBL_ITEMS_TXT_ARR   = 0x220;  // void* passed to GetItemIdFromCode
static constexpr uint32_t DATATBL_ITEMS_TXT_BASE  = 0x15a0; // D2ItemsTxt* record array base
static constexpr uint32_t DATATBL_ITEMS_TXT_COUNT = 0x15a8; // uint32_t record count
static constexpr uint32_t DATATBL_GAMBLE_POOL_PTR = 0x16b8; // int32_t* gamble selection pool
static constexpr uint32_t DATATBL_GAMBLE_LIMITS   = 0x16d0; // uint32_t[100] pool size per item level

// ── Quiver type-code check ────────────────────────────────────────────────────
// Catches dwCode == 0x20767161 ('aqv ') arrows or 0x20767163 ('cqv ') bolts.
static constexpr uint32_t QUIVER_CODE_MASK = 0xfffffffd;
static constexpr uint32_t QUIVER_CODE_BASE = 0x20767161;

// ── Stat IDs ─────────────────────────────────────────────────────────────────
static constexpr int STAT_LEVEL    = 0x0C;
static constexpr int STAT_QUANTITY = 0x46;

// ── D2R function type aliases ─────────────────────────────────────────────────

using GetInventoryGoldLimit_t  = int(*)(int64_t unitPtr);
using FillStoreInventory_t     = void(*)(D2GameStrc* pGame, D2UnitStrc* pPlayer, D2UnitStrc* pNpc);
using FillGamble_t             = void(*)(D2GameStrc* pGame, int64_t param2, D2UnitStrc* pPlayer);
using CompileTxt_t             = void(__fastcall*)(uint8_t expansion, const char* p2, const char* p3,
                                                    const char* p4, D2TxtFieldDesc* fields,
                                                    uint64_t stride, D2TxtContainer* output);
using CreateCompiledTCStruct_t = void*(__fastcall*)(uint8_t expansion, const void* rawRecord);
using TCDropFunction_t         = void(__fastcall*)(void* p1, void* p2, uint32_t p3, void* damageEvent);
using ConditionGate_t          = int(__fastcall*)(void* gameCtx, void* compiledTCStruct, D2UnitStrc* unit, uint8_t pickFlag);
using ConditionCalcEval_t      = int(__fastcall*)(uint8_t expansion, D2UnitStrc* unit, uint32_t exprIdx);
// RCX=pNpc RDX=dwCode R8=pGame R9=nQuality [RSP+20]=nItemLevel [RSP+28]=nPlayerLevel
using GenerateStoreItem_t      = D2UnitStrc*(*)(D2UnitStrc* pNpc, uint32_t dwCode, D2GameStrc* pGame,
                                                 int nQuality, int nItemLevel, int nPlayerLevel);
using ComputeItemLevel_t       = int(*)(D2GameStrc* pGame, D2UnitStrc* pNpc);
using GetItemIdFromCode_t      = int(*)(void* itemsTxtArr, uint32_t dwCode);
using SetUnitStat_t            = void(*)(D2UnitStrc* pItem, int statId, int value, int layer);
using GetMaxStack_t            = int(*)(D2UnitStrc* pItem);

// ── Plugin state ──────────────────────────────────────────────────────────────

static ItemPluginOptions        g_pluginOptions;
static uintptr_t                g_exeBase = 0;
static GetInventoryGoldLimit_t  Original_GetInventoryGoldLimit = nullptr;
static FillStoreInventory_t     Original_FillStoreInventory    = nullptr;
static FillGamble_t             Original_FillGamble            = nullptr;
static TCDropFunction_t         Original_TCDropFunction        = nullptr;
static ConditionCalcEval_t      Original_ConditionCalcEval     = nullptr;
static ConditionGate_t          Original_ConditionGate         = nullptr;

// Player unit captured at the monster-death-drop entry for the duration of that drop.
thread_local D2UnitStrc* g_currentDropPlayer    = nullptr;
// Set by Hook_ConditionGate for the specific TC record being evaluated.
thread_local bool        g_currentCalcUsePlayer = false;
static GenerateStoreItem_t      Fn_GenerateStoreItem           = nullptr;
static ComputeItemLevel_t       Fn_ComputeItemLevel            = nullptr;
static GetItemIdFromCode_t      Fn_GetItemIdFromCode           = nullptr;
static SetUnitStat_t            Fn_SetUnitStat                 = nullptr;
static GetMaxStack_t            Fn_GetMaxStack                 = nullptr;

// ── Inline helpers ────────────────────────────────────────────────────────────

// Advance the game-level LCG (pGame->rngSeedLow/High) and return the low 32 bits.
static inline uint32_t AdvanceGameRng(D2GameStrc* pGame) {
	uint64_t next = (uint64_t)pGame->rngSeedLow * 0x6AC690C5 + pGame->rngSeedHigh;
	pGame->rngSeedLow  = (uint32_t)next;
	pGame->rngSeedHigh = (uint32_t)(next >> 32);
	return pGame->rngSeedLow;
}

// Roll a value in [lo, hi] using the game RNG (equivalent to D2MOO RollLimitedRandom).
static inline uint32_t RollGameRngRange(D2GameStrc* pGame, uint32_t lo, uint32_t hi) {
	if (lo >= hi) return lo;
	uint32_t range = hi - lo + 1;
	uint32_t roll  = AdvanceGameRng(pGame);
	uint32_t idx   = (range & (range - 1)) == 0 ? (roll & (range - 1)) : (roll % range);
	return lo + idx;
}

// Return pointer to the dataTbl base for the current expansion.
static inline intptr_t GetDataTable(D2GameStrc* pGame) {
	return reinterpret_cast<intptr_t*>(g_exeBase + OFF_sgptDataTables)[pGame->expansion * 2];
}

// Return a const pointer to the raw D2ItemsTxt record bytes for dwCode, or nullptr.
static inline const D2ItemsTxt* GetItemRecord(intptr_t dataTbl, uint32_t dwCode) {
	void*    arr   = *reinterpret_cast<void**>(dataTbl + DATATBL_ITEMS_TXT_ARR);
	int      id    = Fn_GetItemIdFromCode(arr, dwCode);
	if (id < 0) return nullptr;
	uint32_t count = *reinterpret_cast<uint32_t*>(dataTbl + DATATBL_ITEMS_TXT_COUNT);
	if ((uint32_t)id >= count) return nullptr;
	const uint8_t* base = *reinterpret_cast<const uint8_t**>(dataTbl + DATATBL_ITEMS_TXT_BASE);
	if (!base) return nullptr;
	return (D2ItemsTxt*)(base + (size_t)id * sizeof(D2ItemsTxt));
}

// ── Hook: DATATBLS_CompileTxt call site inside TC compiler ───────────────────
//
// Intercepts the single CompileTxt call inside FUN_140284a40 (TreasureClassEx
// compiler). Appends UsePlayerForConditionCalc as a bool field (type=4) at byte
// offset 0x301 — padding within the existing 0x304 stride, so stride is unchanged.

static void __fastcall Hook_CompileTxt_TC(uint8_t expansion, const char* p2, const char* p3,
                                           const char* p4, D2TxtFieldDesc* origFields,
                                           uint64_t stride, D2TxtContainer* output)
{
    int n = 0;
    while (origFields[n].pName && std::strcmp(origFields[n].pName, "end") != 0) ++n;

    static const D2TxtFieldDesc usePlayerDesc = { "UsePlayerForConditionCalc", 4, 0, TCREC_USEPLAYER_OFF, 0 };

    std::vector<D2TxtFieldDesc> extended(origFields, origFields + n);
    extended.push_back(usePlayerDesc);
    extended.push_back(origFields[n]);  // preserve sentinel

    auto RealCompileTxt = reinterpret_cast<CompileTxt_t>(g_exeBase + OFF_CompileTxt);
    RealCompileTxt(expansion, p2, p3, p4, extended.data(), stride, output);
}

// ── Hook: FUN_140283d10 call site inside TC compiler loop ────────────────────
//
// Called once per TC row; RCX=expansion CL, RDX=raw record, RAX=compiled struct.
// Stamps bit 0x08 into compiled[0x25] when UsePlayerForConditionCalc is set in
// the raw record. That bit is confirmed unused by the executable (no reads/writes).

static void* __fastcall Hook_CreateCompiledTCStruct(uint8_t expansion, const uint8_t* rawRecord)
{
    auto Real = reinterpret_cast<CreateCompiledTCStruct_t>(g_exeBase + OFF_CreateCompiledTCStruct);
    uint8_t* compiled = static_cast<uint8_t*>(Real(expansion, rawRecord));
    if (compiled && rawRecord && rawRecord[TCREC_USEPLAYER_OFF])
        compiled[0x25] |= 0x08;
    return compiled;
}

// ── Hook: FUN_140382ad0 (monster death drop entry) ───────────────────────────
//
// Captures the killer player unit (at damageEvent+0x10) for the duration of the
// entire drop chain. hookSize=6: MOV R11,RSP (3) + PUSH RBP (1) + PUSH R13 (2).

static void __fastcall Hook_TCDropFunction(void* p1, void* p2, uint32_t p3, void* damageEvent)
{
    D2UnitStrc* player = nullptr;
    if (damageEvent) {
        player = *reinterpret_cast<D2UnitStrc**>(static_cast<uint8_t*>(damageEvent) + 0x10);
        if (player && player->dwUnitType != D2UnitType::Player)
            player = nullptr;
    }
    g_currentDropPlayer = player;
    Original_TCDropFunction(p1, p2, p3, damageEvent);
    g_currentDropPlayer = nullptr;
}

// ── Hook: FUN_140321b90 (TC condition gate) ───────────────────────────────────
//
// Called before each TC ConditionCalc evaluation with the compiled TC struct as
// param_2. Reads UsePlayerForConditionCalc directly from bit 0x08 of compiled[0x25],
// stamped at compile time by Hook_CreateCompiledTCStruct.
// hookSize=5: MOV qword ptr [RSP+0x8],RBX (5 bytes).

static int __fastcall Hook_ConditionGate(void* gameCtx, void* compiledTCStruct,
                                          D2UnitStrc* unit, uint8_t pickFlag)
{
    bool prev = g_currentCalcUsePlayer;
    g_currentCalcUsePlayer = g_currentDropPlayer && compiledTCStruct &&
        (static_cast<uint8_t*>(compiledTCStruct)[0x25] & 0x08) != 0;

    int result = Original_ConditionGate(gameCtx, compiledTCStruct, unit, pickFlag);
    g_currentCalcUsePlayer = prev;
    return result;
}

// ── Hook: FUN_1402a7950 (ConditionCalc expression evaluator) ─────────────────
//
// When the condition gate flagged UsePlayerForConditionCalc for this TC record,
// substitutes the player for the monster unit before evaluating the expression.
// hookSize=5: MOV qword ptr [RSP+0x8],RBX (5 bytes).

static int __fastcall Hook_ConditionCalcEval(uint8_t expansion, D2UnitStrc* unit, uint32_t exprIdx)
{
    D2UnitStrc* evalUnit =
        (g_currentCalcUsePlayer && g_currentDropPlayer) ? g_currentDropPlayer : unit;
    return Original_ConditionCalcEval(expansion, evalUnit, exprIdx);
}

// ── Hook: D2GAME_NPC_FillStoreInventory ──────────────────────────────────────
//
// Faithful C++ rewrite of FUN_1403e7e70.  Finds the VendorChainEntry for pNpc,
// updates its tick timestamp, then generates proxy (normal + magic) and perm store
// items by calling the original D2GAME_NPC_GenerateStoreItem (FUN_1403e7200).
//
static void Hook_FillStoreInventory(D2GameStrc* pGame, D2UnitStrc* pPlayer, D2UnitStrc* pNpc)
{
	// ── Find the VendorChainEntry for this NPC ───────────────────────────────
	const uint16_t npcId = pNpc ? (uint16_t)pNpc->unitFlags : (uint16_t)-1;

	VendorChainEntry* entryArr   = pGame->pVendorChain;
	uint64_t          entryCount = pGame->nVendorChain;
	VendorChainEntry* entry = nullptr;
	for (uint64_t i = 0; i < entryCount; ++i) {
		if (entryArr[i].npcId == npcId) {
			entry = &entryArr[i];
			break;
		}
	}
	if (!entry) return; // NPC not registered (assert in original)

	// ── Refresh tick timestamp ────────────────────────────────────────────────
	entry->qwTicks = GetTickCount64();

	// ── Player level + item level cap ─────────────────────────────────────────
	int playerLevel = 0;
	if (pPlayer && pPlayer->statList) {
		playerLevel = PSh_GetStat(g_exeBase, pPlayer->statList, STAT_LEVEL);
	}
	int itemLevel = Fn_ComputeItemLevel(pGame, pNpc);

	intptr_t dataTbl = GetDataTable(pGame);

	// ── Proxy items loop ──────────────────────────────────────────────────────
	int nSpawned = 0;
	auto* cache = static_cast<NpcItemCacheEntry*>(entry->pItemCache);

	for (uint64_t i = 0; i < entry->nItems; ++i) {
		const NpcItemCacheEntry& ci = cache[i];
		const D2ItemsTxt* rec = GetItemRecord(dataTbl, ci.dwCode);
		if (!rec) continue;
		if (rec->nLevel > itemLevel) continue;
		if (rec->wVersion >= 100 && pGame->wItemFormat < 100) continue;

		// Normal items — only when itemLevel < 25
		uint32_t nNormal = 0;
		if (itemLevel < g_pluginOptions.VOHNormalItemLevelMaxThreshold) {
			nNormal = RollGameRngRange(pGame, ci.nMin, ci.nMax);
		}

		for (uint32_t j = 0; j < nNormal; ++j) {
			// Quality roll: low 32 bits of RNG advance % 100
			uint32_t roll    = AdvanceGameRng(pGame) % 100;
			D2ItemQuality      quality = D2ItemQuality::Normal;

			if (itemLevel < g_pluginOptions.VOHLowQualityMaxLevelThreshold &&
				roll >= 100 - g_pluginOptions.VOHLowQualityDowngradeChance)
			{
				quality = D2ItemQuality::LowQuality;
			}
			else if (itemLevel >= g_pluginOptions.VOHSuperiorLevelMinLevelThreshold &&
				roll >= 100 - g_pluginOptions.VOHSuperiorUpgradeChance)
			{
				quality = D2ItemQuality::Superior;
			}

			if (!Fn_GenerateStoreItem(pNpc, ci.dwCode, pGame, (int)quality, itemLevel, playerLevel)) {
				++nSpawned;
			}
			if (nSpawned > 32) return;
		}

		// Magic items — requires bitfield1 & 1 and magicLevel check
		if ((rec->dwBitField1 & 1) && (int)(uint8_t)ci.nMagicLevel <= itemLevel) {
			uint32_t nMagicBase = 1;
			if (itemLevel >= g_pluginOptions	.VOHNormalItemLevelMaxThreshold) {
				nMagicBase = (AdvanceGameRng(pGame) & 1) + 2;
			}
			uint32_t nMagic = RollGameRngRange(pGame,
			                                   ci.nMagicMin,
			                                   ci.nMagicMax + (uint8_t)nMagicBase - 1u);
			for (uint32_t j = 0; j < nMagic; ++j) {
				uint32_t roll = (AdvanceGameRng(pGame) % 1024);
				D2ItemQuality quality = g_pluginOptions.bEnableVOHRandomRareVendorItems &&
					roll < g_pluginOptions.VOHRareItemChance ? D2ItemQuality::Rare : D2ItemQuality::Magic;

				if (!Fn_GenerateStoreItem(pNpc, ci.dwCode, pGame, (int)quality, itemLevel, playerLevel)) {
					++nSpawned;
				}
			}
		}
	}

	// ── Perm items loop ───────────────────────────────────────────────────────
	for (uint64_t i = 0; i < entry->nPerms; ++i) {
		uint32_t code = entry->pPermCache[i];
		D2UnitStrc* pStoreItem = Fn_GenerateStoreItem(pNpc, code, pGame, (int)D2ItemQuality::Normal, itemLevel, playerLevel);
		if (pStoreItem) {
			const D2ItemsTxt* rec = GetItemRecord(dataTbl, code);
			if (rec) {
				if (((rec->dwCode - QUIVER_CODE_BASE) & QUIVER_CODE_MASK) == 0) {
					int maxStack = Fn_GetMaxStack(pStoreItem);
					Fn_SetUnitStat(pStoreItem, STAT_QUANTITY, maxStack, 0);
				}
			}
		} else {
			++nSpawned;
		}
		if (nSpawned > 32) return;
	}
}

// ── Hook: D2GAME_STORES_FillGamble (Bitfield1Flag4Only) ──────────────────────
//
// Temporarily replaces the gamble pool (dataTbl+0x16b8) and per-level limits
// (dataTbl+0x16d0) with a filtered copy that contains only items whose
// dwBitField1 & 4 is set, then calls the original FillGamble.
// The original pool/limits are restored before returning.
//
static constexpr int GAMBLE_POOL_MAX = 4096;

static void Hook_FillGamble_Bitfield(D2GameStrc* pGame, int64_t param2, D2UnitStrc* pPlayer)
{
	intptr_t dataTbl = GetDataTable(pGame);

	int32_t** poolPtrField  = reinterpret_cast<int32_t**>(dataTbl + DATATBL_GAMBLE_POOL_PTR);
	uint32_t* origLimits    = reinterpret_cast<uint32_t*>(dataTbl + DATATBL_GAMBLE_LIMITS);
	int32_t*  origPool      = *poolPtrField;
	int       maxChoose     = (origPool && origLimits) ? (int)origLimits[99] : 0;

	const uint8_t* itemsTxtBase  = *reinterpret_cast<const uint8_t**>(dataTbl + DATATBL_ITEMS_TXT_BASE);
	uint32_t       itemsTxtCount = *reinterpret_cast<uint32_t*>(dataTbl + DATATBL_ITEMS_TXT_COUNT);

	if (maxChoose <= 0 || !origPool || !itemsTxtBase) {
		Original_FillGamble(pGame, param2, pPlayer);
		return;
	}

	// Build filtered pool, preserving original level-ordering.
	// filtLimits[L] = how many filtered items came from origPool[0..origLimits[L]-1].
	static int32_t  filtPool[GAMBLE_POOL_MAX];
	static uint32_t filtLimits[100];

	uint32_t filtCount = 0;
	int      origSoFar = 0;
	for (int level = 0; level < 100; ++level) {
		int limit = (int)origLimits[level];
		while (origSoFar < limit && origSoFar < maxChoose) {
			int32_t itemId = origPool[origSoFar];
			if ((uint32_t)itemId < itemsTxtCount) {
				const D2ItemsTxt* rec = GetItemRecord((intptr_t)itemsTxtBase, itemId);
				if (rec->dwBitField1 & g_pluginOptions.GambleBitfield) {
					if (filtCount < GAMBLE_POOL_MAX) {
						filtPool[filtCount++] = itemId;
					}
				}
			}
			++origSoFar;
		}
		filtLimits[level] = filtCount;
	}

	if (filtCount == 0) {
		// No qualifying items — fall back to original behaviour.
		Original_FillGamble(pGame, param2, pPlayer);
		return;
	}

	// Temporarily swap in the filtered pool and limits.
	// D2 game sessions are single-threaded per game, so this is safe.
	uint32_t savedLimits[100];
	memcpy(savedLimits, origLimits, sizeof(savedLimits));
	*poolPtrField = filtPool;
	memcpy(origLimits, filtLimits, sizeof(filtLimits));

	Original_FillGamble(pGame, param2, pPlayer);

	// Restore original pool and limits.
	*poolPtrField = origPool;
	memcpy(origLimits, savedLimits, sizeof(savedLimits));
}

// ── Hook: D2GAME_GetInventoryGoldLimit ───────────────────────────────────────

static int __fastcall Hook_GetInventoryGoldLimit(int64_t unitPtr)
{
	if (g_pluginOptions.InventoryGoldLimitChange == GoldOption::Flat) {
		return unitPtr ? static_cast<int>(g_pluginOptions.InventoryGoldLimit) : 0;
	}
	// PerLevel: original returns level * 10000; substitute our per-level multiplier.
	int result = Original_GetInventoryGoldLimit(unitPtr);
	if (result == 0) return 0;
	return (result / 10000) * static_cast<int>(g_pluginOptions.InventoryGoldLimit);
}

// ── INI loading ───────────────────────────────────────────────────────────────

void ItemPluginOptions::Load(const D2RLoaderPluginContext* context, const wchar_t* section)
{
	bMagicItemsSpawnIdentified = PSh_Ini_GetInt(context, section, L"EnableMagicItemsSpawnIdentified", 0);
	bRareItemsSpawnIdentified = PSh_Ini_GetInt(context, section, L"EnableRareItemsSpawnIdentified", 0);
	bDisableGoldPenalty = PSh_Ini_GetInt(context, section, L"DisableGoldPenalty", 0) != 0;

	InventoryGoldLimitChange = static_cast<GoldOption>(PSh_Ini_GetInt(context, section, L"EnableInventoryGoldLimitChange", 0));
	InventoryGoldLimit = PSh_Ini_GetInt(context, section, L"InventoryGoldLimit", 10000);

	// Index 0=Magic 1=Set 2=Rare 3=Unique 4=Crafted 5=Tempered
	static const wchar_t* runewordQualityKeys[] = {
		L"RunewordQualityMagic",
		L"RunewordQualitySet",
		L"RunewordQualityRare",
		L"RunewordQualityUnique",
		L"RunewordQualityCrafted",
		L"RunewordQualityTempered",
	};
	for (int i = 0; i < 6; i++) {
		bRunewordQualities[i] = PSh_Ini_GetInt(context, section, runewordQualityKeys[i], 0) != 0;
	}

	GambleFilter = static_cast<GambleOption>(PSh_Ini_GetInt(context, section, L"GambleFilter", 0));
	GambleBitfield = PSh_Ini_GetInt(context, section, L"GambleBitfield", 4);

	bEnableVendorOverhaul = PSh_Ini_GetInt(context, section, L"EnableVendorOverhaul", 0) != 0;
	VOHNormalItemLevelMaxThreshold = PSh_Ini_GetInt(context, section, L"VendorOverhaulNormalItemLevelMaxLevelThreshold", 25);
	VOHMagicMinLevelThreshold = PSh_Ini_GetInt(context, section, L"VendorOverhaulMagicMinLevelThreshold", 0);
	VOHSuperiorLevelMinLevelThreshold = PSh_Ini_GetInt(context, section, L"VendorOverhaulSuperiorLevelMinLevelThreshold", 5);
	VOHLowQualityMaxLevelThreshold = PSh_Ini_GetInt(context, section, L"VendorOverhaulLowQualityMaxLevelThreshold", 5);
	VOHSuperiorUpgradeChance = PSh_Ini_GetInt(context, section, L"VendorOverhaulSuperiorUpgradeChance", 25);
	VOHLowQualityDowngradeChance = PSh_Ini_GetInt(context, section, L"VendorOverhaulLowQualityDowngradeChance", 10);
	bEnableVOHRandomRareVendorItems = PSh_Ini_GetInt(context, section, L"VendorOverhaulEnableRandomRareItems", 0) != 0;
	VOHRareItemChance = PSh_Ini_GetInt(context, section, L"VendorOverhaulRareItemChance", 0);


	VendorNightmareUpgradeBaseChance = PSh_Ini_GetInt(context, section, L"VendorNightmareUpgradeBaseChance", 4000);
	VendorHellUberUpgradeBaseChance = PSh_Ini_GetInt(context, section, L"VendorHellUberUpgradeBaseChance", 5000);
	VendorHellUpgradeBaseChance = PSh_Ini_GetInt(context, section, L"VendorHellUpgradeBaseChance", 1000);
	VendorNightmareUpgradeLevelScale = PSh_Ini_GetInt(context, section, L"VendorNightmareUpgradeLevelScale", 64);
	VendorHellUberUpgradeLevelScale = PSh_Ini_GetInt(context, section, L"VendorHellUberUpgradeLevelScale", 128);
	VendorHellUpgradeLevelScale = PSh_Ini_GetInt(context, section, L"VendorHellUpgradeLevelScale", 16);


	bEnablePlayerConditionCalc = PSh_Ini_GetInt(context, section, L"EnablePlayerConditionCalc", 0) != 0;

	bEnablePhysResistMaxChange = PSh_Ini_GetInt(context, section, L"EnablePhysResistMaxChange", 0) != 0;
	MaxPhysResist = PSh_Ini_GetInt(context, section, L"MaxPhysResist", 50);
	bEnableElementalResistMaxChange = PSh_Ini_GetInt(context, section, L"EnableElementalResistMaxChange", 0) != 0;
	MaxElementalResist = PSh_Ini_GetInt(context, section, L"MaxElementalResist", 95);
	bEnableAbsorbCapChange = PSh_Ini_GetInt(context, section, L"EnableAbsorbCapChange", 0) != 0;
	MaxAbsorbPct = PSh_Ini_GetInt(context, section, L"MaxAbsorbCapPct", 40);
}

// Returns the SHL shift count for a given power-of-2 multiplier (floor log2).
static uint8_t ShiftCount(int n) {
	if (n <= 1) return 0;
	uint8_t s = 0;
	while ((2 << s) <= n && s < 30) s++;
	return s;
}

// ── Plugin exports ────────────────────────────────────────────────────────────

static constexpr D2RLoaderPluginInfo PluginInfo {
	.apiVersion = D2RLOADER_PLUGIN_API_VERSION,
	.id         = "plugin-items",
	.name       = "Items Plugin",
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

	g_pluginOptions.Load(context, L"PluginPack.Items");
	g_exeBase = context->exeBase;

	// Resolve internal function pointers used by both hooks.
	Fn_GenerateStoreItem = reinterpret_cast<GenerateStoreItem_t>(g_exeBase + OFF_GenerateStoreItem);
	Fn_ComputeItemLevel  = reinterpret_cast<ComputeItemLevel_t>(g_exeBase + OFF_ComputeItemLevel);
	Fn_GetItemIdFromCode = reinterpret_cast<GetItemIdFromCode_t>(g_exeBase + OFF_GetItemIdFromCode);
	Fn_SetUnitStat       = reinterpret_cast<SetUnitStat_t>(g_exeBase + OFF_SetUnitStat);
	Fn_GetMaxStack       = reinterpret_cast<GetMaxStack_t>(g_exeBase + OFF_GetMaxStack);

	if (g_pluginOptions.bEnableVendorOverhaul)
	{
		// First instruction is MOV qword ptr [RSP+0x18],R8 = 5 bytes (4C 89 44 24 18).
		if (!PSh_InstallHook(PLUGINID_ITEMS, context, OFF_FillStoreInventory,
		                     reinterpret_cast<void*>(Hook_FillStoreInventory),
		                     reinterpret_cast<void**>(&Original_FillStoreInventory), 5))
		{
			D2RPluginLogErrorF(context, "plugin-items: failed to hook FillStoreInventory");
		}

		// Difficulty upgrade threshold patches inside D2GAME_NPC_GenerateStoreItem.
		{
			uint32_t nmBase   = (uint32_t)g_pluginOptions.VendorNightmareUpgradeBaseChance;
			uint32_t hellBase = (uint32_t)g_pluginOptions.VendorHellUberUpgradeBaseChance;
			uint32_t ultBase  = (uint32_t)g_pluginOptions.VendorHellUpgradeBaseChance;
			PSh_PatchBytes(PLUGINID_ITEMS, context, OFF_NMUberBaseImm,    4, reinterpret_cast<unsigned char*>(&nmBase));
			PSh_PatchBytes(PLUGINID_ITEMS, context, OFF_HellUberBaseImm,  4, reinterpret_cast<unsigned char*>(&hellBase));
			PSh_PatchBytes(PLUGINID_ITEMS, context, OFF_HellUltraBaseImm, 4, reinterpret_cast<unsigned char*>(&ultBase));

			unsigned char nmScale   = ShiftCount(g_pluginOptions.VendorNightmareUpgradeLevelScale);
			unsigned char hellScale = ShiftCount(g_pluginOptions.VendorHellUberUpgradeLevelScale);
			unsigned char ultScale  = ShiftCount(g_pluginOptions.VendorHellUpgradeLevelScale);
			PSh_PatchBytes(PLUGINID_ITEMS, context, OFF_NMUberScaleByte,    1, &nmScale);
			PSh_PatchBytes(PLUGINID_ITEMS, context, OFF_HellUberScaleByte,  1, &hellScale);
			PSh_PatchBytes(PLUGINID_ITEMS, context, OFF_HellUltraScaleByte, 1, &ultScale);
		}
	}

	if (g_pluginOptions.GambleFilter == GambleOption::NoRingAmuletGuarantee)
	{
		// Change JGE (0x7D) → JMP (0xEB) at the ring/amulet override check in FillGamble.
		// This makes the jump unconditional, permanently skipping the forced ring/amulet logic.
		unsigned char patch[] = { 0xEB };
		PSh_PatchBytes(PLUGINID_ITEMS, context, OFF_FillGamble_JgeOpcode, 1, patch);
	}
	else if (g_pluginOptions.GambleFilter == GambleOption::Bitfield)
	{
		// First 7 bytes: PUSH RBP (1) + PUSH RSI (1) + PUSH RDI (1) + PUSH R14 (2) + PUSH R15 (2).
		if (!PSh_InstallHook(PLUGINID_ITEMS, context, OFF_FillGamble,
		                     reinterpret_cast<void*>(Hook_FillGamble_Bitfield),
		                     reinterpret_cast<void**>(&Original_FillGamble), 7))
		{
			D2RPluginLogErrorF(context, "plugin-items: failed to hook FillGamble");
		}
	}

	if (g_pluginOptions.bDisableGoldPenalty)
	{
		// CALL is 5 bytes (E8 + 4-byte rel32); replace with NOPs to skip the penalty entirely.
		unsigned char nops[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
		PSh_PatchBytes(PLUGINID_ITEMS, context, OFF_GoldPenaltyCall, 5, nops);
	}

	if (g_pluginOptions.bMagicItemsSpawnIdentified)
	{
		unsigned char patch[] = { 0x83, 0x4A, 0x18, 0x10 };
		PSh_PatchBytes(PLUGINID_ITEMS, context, OFF_MagicItemsSpawnIdentified, 4, patch);
	}

	if (g_pluginOptions.bRareItemsSpawnIdentified)
	{
		unsigned char patch[] = { 0x83, 0x48, 0x18, 0x10 };
		PSh_PatchBytes(PLUGINID_ITEMS, context, OFF_RareItemsSpawnIdentified, 4, patch);
	}

	if (g_pluginOptions.InventoryGoldLimitChange != GoldOption::Disabled)
	{
		// SUB RSP,0x28 (4 bytes) + TEST RCX,RCX (3 bytes) = 7 bytes; hookSize=7.
		if (!PSh_InstallHook(PLUGINID_ITEMS, context, OFF_GetInventoryGoldLimit,
		                     reinterpret_cast<void*>(Hook_GetInventoryGoldLimit),
		                     reinterpret_cast<void**>(&Original_GetInventoryGoldLimit), 7))
		{
			D2RPluginLogErrorF(context, "plugin-items: failed to hook GetInventoryGoldLimit");
		}
	}

	for (int i = 0; i < 6; i++)
	{
		if (g_pluginOptions.bRunewordQualities[i])
		{
			PSh_PatchBytes(PLUGINID_ITEMS, context,
			               OFF_RunewordQualityJumpTable + static_cast<uint64_t>(i) * 4,
			               4, RUNEWORD_QUALITY_PASS);
		}
	}

	if (g_pluginOptions.bEnablePlayerConditionCalc)
	{
		PSh_PatchCallSite(PLUGINID_ITEMS, context, OFF_CompileTxtCallInTC,
		                  reinterpret_cast<void*>(Hook_CompileTxt_TC));
		PSh_PatchCallSite(PLUGINID_ITEMS, context, OFF_CreateCompiledTCStructCall,
		                  reinterpret_cast<void*>(Hook_CreateCompiledTCStruct));
		// hookSize=6: MOV R11,RSP (3) + PUSH RBP (1) + PUSH R13 (2)
		if (!PSh_InstallHook(PLUGINID_ITEMS, context, OFF_TCDropFunction,
		                     reinterpret_cast<void*>(Hook_TCDropFunction),
		                     reinterpret_cast<void**>(&Original_TCDropFunction), 6))
		{
			D2RPluginLogErrorF(context, "plugin-items: failed to hook TCDropFunction");
		}
		// hookSize=5: MOV qword ptr [RSP+0x8],RBX (5 bytes)
		if (!PSh_InstallHook(PLUGINID_ITEMS, context, OFF_ConditionGate,
		                     reinterpret_cast<void*>(Hook_ConditionGate),
		                     reinterpret_cast<void**>(&Original_ConditionGate), 5))
		{
			D2RPluginLogErrorF(context, "plugin-items: failed to hook ConditionGate");
		}
		// hookSize=5: MOV qword ptr [RSP+0x8],RBX (5 bytes)
		if (!PSh_InstallHook(PLUGINID_ITEMS, context, OFF_ConditionCalcEval,
		                     reinterpret_cast<void*>(Hook_ConditionCalcEval),
		                     reinterpret_cast<void**>(&Original_ConditionCalcEval), 5))
		{
			D2RPluginLogErrorF(context, "plugin-items: failed to hook ConditionCalcEval");
		}
	}

	return true;
}

D2RLOADER_PLUGIN_EXPORT void __cdecl D2RLoaderUnload() noexcept {
	PSh_RemoveHook(PLUGINID_ITEMS, nullptr, OFF_GetInventoryGoldLimit);
	PSh_RemoveHook(PLUGINID_ITEMS, nullptr, OFF_FillStoreInventory);
	PSh_RemoveHook(PLUGINID_ITEMS, nullptr, OFF_FillGamble);
	PSh_RemoveHook(PLUGINID_ITEMS, nullptr, OFF_TCDropFunction);
	PSh_RemoveHook(PLUGINID_ITEMS, nullptr, OFF_ConditionGate);
	PSh_RemoveHook(PLUGINID_ITEMS, nullptr, OFF_ConditionCalcEval);
}
