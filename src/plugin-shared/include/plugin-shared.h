#pragma once
#include <plugin.h>
#include <cstddef>
#include <cstdint>

#define PLUGINID_ITEMS	0xEE000001
#define PLUGINID_LEVELS 0xEE000002
#define PLUGINID_MISC   0xEE000003
#define PLUGINID_QUESTS 0xEE000004
#define PLUGINID_SKILLS 0xEE000005

// ── D2R types ───────────────────────────────────────────────────────

enum class D2Difficulty : uint8_t {
	Normal,
	Nightmare,
	Hell,
};

enum class D2UnitType : uint32_t {
	Player,
	Monster,
	Object,
	Missile,
	Item,
	Tile,
	Max,
};

enum class D2ItemQuality : uint32_t {
	None,
	LowQuality,
	Normal,
	Superior,
	Magic,
	Set,
	Rare,
	Unique,
	Crafted,
	Tempered,
};

enum class D2Vendor : uint8_t {
	Akara,
	Gheed,
	Charsi,
	Fara,
	Lysander,
	Drognan,
	Hratli,
	Alkor,
	Ormus,
	Elzix,
	Asheara,
	Cain,
	Halbu,
	Jamella,
	Malah,
	Larzuk,
	Anya,
};

//
// TXT types
//

// Descriptor for one column in a D2R .txt data table (32 bytes).
// Passed as an array (null-pName terminated) to DATATBLS_CompileTxt.
// For bit-field columns (boolean flags packed into a uint64_t):
//   offset = byte offset of the containing uint64_t in the record
//   count  = bit index within that uint64_t (0 = LSB)
//   type   = determined at runtime by scanning existing bool descriptors
struct D2TxtFieldDesc {
	const char* pName;   // +0x00  column header (nullptr = array terminator)
	uint32_t    type;    // +0x08  1 = char[], 2 = int/short, 3+ = bit-field (see above)
	uint32_t    count;   // +0x0c  strings: max chars; bit-fields: bit index; ints: 0
	uint64_t    offset;  // +0x10  byte offset of field within the record struct
	uint64_t    _pad;    // +0x18  always 0
};
static_assert(sizeof(D2TxtFieldDesc) == 32, "D2TxtFieldDesc must be 32 bytes");

// Receives the compiled record array after a DATATBLS_CompileTxt call.
struct D2TxtDataArea {
	void*    pRecords;  // heap pointer to record array (game-owned)
	uint64_t nCount;    // number of compiled records
	uint64_t flags;     // high bit set = uses external/inline storage
};

// Wraps D2TxtDataArea with a vtable for internal heap allocation.
// Set vtable = exeBase + 0x16df480 (reuse the game's container vtable).
struct D2TxtContainer {
	void*          vtable;
	D2TxtDataArea* pData;
};

//
// Individual game types
//
#pragma pack(1)
struct D2ItemsTxt
{
	char     szFlippyFile[32];              // 0x000  flippyfile   (type 1, count=0x1f)
	char     szInvFile[32];                 // 0x020  invfile
	char     szUniqueInvFile[32];           // 0x040  uniqueinvfile
	char     szSetInvFile[32];              // 0x060  setinvfile

	union { uint32_t dwCode; char szCode[4]; }; // 0x080  code  (type 0a)
	uint32_t dwNormCode;                    // 0x084  normcode
	uint32_t dwUberCode;                    // 0x088  ubercode
	uint32_t dwUltraCode;                   // 0x08C  ultracode
	uint32_t dwAlternateGfx;               // 0x090  alternategfx
	uint32_t dwPspell;                      // 0x094  pSpell
	uint16_t wState;                        // 0x098  state
	uint16_t wCurseState[2];               // 0x09A  cstate1, cstate2
	uint16_t wStat[3];                      // 0x09E  stat1, stat2, stat3
	uint32_t dwCalc[3];                     // 0x0A4  calc1, calc2, calc3
	uint32_t dwLen;                         // 0x0B0  len
	uint8_t  nSpellDesc;                    // 0x0B4  spelldesc
	uint8_t  pad0xB5;                       // 0x0B5
	uint16_t wSpellDescStr;                 // 0x0B6  spelldescstr
	uint16_t wSpellDescStr2;               // 0x0B8  spelldescstr2
	uint8_t  pad0xBA[2];                   // 0x0BA
	uint32_t dwSpellDescCalc;              // 0x0BC  spelldesccalc
	uint8_t  nSpellDescColor;              // 0x0C0  spelldesccolor
	uint8_t  pad0xC1[3];                   // 0x0C1
	uint32_t dwBetterGem;                  // 0x0C4  BetterGem
	uint32_t dwWeapClass;                  // 0x0C8  wclass
	uint32_t dwWeapClass2Hand;             // 0x0CC  2handedwclass
	uint32_t dwTransmogrifyType;           // 0x0D0  TMogType  (NEW vs classic)
	int32_t  dwMinAc;                       // 0x0D4  minac
	int32_t  dwMaxAc;                       // 0x0D8  maxac
	uint32_t dwGambleCost;                 // 0x0DC  gamble cost
	int32_t  dwSpeed;                       // 0x0E0  speed
	uint32_t dwBitField1;                  // 0x0E4  bitfield1
	uint32_t dwCost;                        // 0x0E8  cost
	uint32_t dwMinStack;                    // 0x0EC  minstack
	uint32_t dwMaxStack;                    // 0x0F0  maxstack
	uint32_t dwSpawnStack;                  // 0x0F4  spawnstack
	uint32_t dwGemOffset;                  // 0x0F8  gemoffset
	uint16_t wNameStr;                      // 0x0FC  namestr
	uint16_t wVersion;                      // 0x0FE  version
	uint16_t wAutoPrefix;                  // 0x100  auto prefix
	uint16_t wMissileType;                 // 0x102  missiletype
	uint32_t dwDropConditionCalc;          // 0x104  DropConditionCalc  (NEW)
	uint32_t dwUsageConditionCalc;         // 0x108  UsageConditionCalc (NEW)
	uint8_t  nRarity;                       // 0x10C  rarity
	uint8_t  nLevel;                        // 0x10D  level
	uint8_t  nShowLevel;                   // 0x10E  ShowLevel  (NEW)
	int8_t   nMinDam;                       // 0x10F  mindam
	int8_t   nMaxDam;                       // 0x110  maxdam
	uint8_t  nMinMisDam;                   // 0x111  minmisdam
	uint8_t  nMaxMisDam;                   // 0x112  maxmisdam
	int8_t   n2HandMinDam;                 // 0x113  2handmindam
	int8_t   n2HandMaxDam;                 // 0x114  2handmaxdam
	int8_t   nRangeAdder;                  // 0x115  rangeadder
	int16_t  nStrBonus;                     // 0x116  strbonus
	int16_t  nDexBonus;                     // 0x118  dexbonus
	uint16_t wReqStr;                       // 0x11A  reqstr
	uint16_t wReqDex;                       // 0x11C  reqdex
	uint8_t  nInvWidth;                     // 0x11E  invwidth
	uint8_t  nInvHeight;                    // 0x11F  invheight
	int8_t   nBlock;                        // 0x120  block
	int8_t   nDurability;                   // 0x121  durability
	uint8_t  nNoDurability;                // 0x122  nodurability
	int8_t   nMissile;                      // 0x123  missile
	uint8_t  nComponent;                    // 0x124  component
	int8_t   nArmorComp[6];               // 0x125  rArm, lArm, torso, legs, rspad, lspad
	int8_t   n2Handed;                      // 0x12B  2handed
	uint8_t  nUseable;                      // 0x12C  useable
	uint8_t  pad0x12D;                      // 0x12D
	uint16_t wType[2];                      // 0x12E  type, type2  (type 0f)
	int8_t   nSubType;                      // 0x132  subtype
	uint8_t  pad0x133;                      // 0x133
	uint16_t wDropSound;                    // 0x134  dropsound
	uint16_t wUseSound;                     // 0x136  usesound
	uint8_t  nDropSfxFrame;               // 0x138  dropsfxframe
	uint8_t  nUnique;                       // 0x139  unique
	uint8_t  nQuest;                        // 0x13A  quest
	uint8_t  nQuestDiffCheck;             // 0x13B  questdiffcheck
	uint8_t  nTransparent;                 // 0x13C  transparent
	uint8_t  nTransTbl;                     // 0x13D  transtbl
	uint8_t  pad0x13E;                      // 0x13E
	uint8_t  nLightRadius;                 // 0x13F  lightradius
	uint8_t  nBelt;                         // 0x140  belt
	uint8_t  nAutoBelt;                     // 0x141  autobelt
	uint8_t  nStackable;                    // 0x142  stackable
	uint8_t  nSpawnable;                    // 0x143  spawnable
	int8_t   nSpellIcon;                    // 0x144  spellicon
	uint8_t  nDurWarning;                  // 0x145  durwarning
	uint8_t  nQuantityWarning;             // 0x146  qntwarning
	int8_t   nHasInv;                       // 0x147  hasinv
	int8_t   nGemSockets;                  // 0x148  gemsockets
	int8_t   nTransmogrify;               // 0x149  Transmogrify
	int8_t   nTmogMin;                      // 0x14A  TMogMin
	int8_t   nTmogMax;                      // 0x14B  TMogMax
	uint8_t  nHitClass;                     // 0x14C  hit class  (type 0d)
	int8_t   n1or2Handed;                  // 0x14D  1or2handed
	uint8_t  nGemApplyType;               // 0x14E  gemapplytype
	uint8_t  nLevelReq;                     // 0x14F  levelreq
	uint8_t  nMagicLevel;                  // 0x150  magic lvl
	int8_t   nTransform;                    // 0x151  Transform
	int8_t   nInvTrans;                     // 0x152  InvTrans
	int8_t   nCompactSave;                 // 0x153  compactsave
	uint8_t  nSkipName;                     // 0x154  SkipName
	uint8_t  nNameable;                     // 0x155  Nameable
	uint8_t  nEventItem;                    // 0x156  EventItem  (NEW)
	// Vendor min/max quantities and magic ranges (17 NPCs each)
	uint8_t  nVendorMin[17];               // 0x157  Akara..Anya Min
	uint8_t  nVendorMax[17];               // 0x168  Akara..Anya Max
	uint8_t  nVendorMagicMin[17];          // 0x179  Akara..Anya MagicMin
	uint8_t  nVendorMagicMax[17];          // 0x18A  Akara..Anya MagicMax
	uint8_t  nVendorMagicLvl[17];          // 0x19B  Akara..Anya MagicLvl
	uint32_t dwNightmareUpgrade;           // 0x1AC  NightmareUpgrade
	uint32_t dwHellUpgrade;               // 0x1B0  HellUpgrade
	uint8_t  nPermStoreItem;               // 0x1B4  PermStoreItem
	uint8_t  nMultibuy;                    // 0x1B5  multibuy
	uint8_t  pad0x1B6[2];                  // 0x1B6
	uint32_t dwDiabloCloneWeight;          // 0x1B8  diablocloneweight  (NEW)
	uint8_t  nUICatOverride;              // 0x1BC  UICatOverride  (NEW, enum lookup)
	uint8_t  nAdvancedStashStackable;     // 0x1BD  AdvancedStashStackable  (NEW)
	uint8_t  pad0x1BE[2];                  // 0x1BE  padding to 0x1C0
};
static_assert(sizeof(D2ItemsTxt) == 0x1C0, "D2ItemsTxt size mismatch");
#pragma pack()

// D2ItemDataTbl — lives at sgptDataTable[context*2] + offsets below
// (struct layout in sgptDataTable not yet mapped; access via loader fields)
struct D2ItemDataTbl
{
	uint64_t nItemsTxtRecordCount;          // total record count (weapons+armor+misc)
	D2ItemsTxt* pItemsTxt;                  // pointer to combined record array
	uint64_t nWeaponsCount;
	D2ItemsTxt* pWeapons;                   // start of weapons sub-array
	uint64_t nArmorCount;
	D2ItemsTxt* pArmor;                     // start of armor sub-array
	uint64_t nMiscCount;
	D2ItemsTxt* pMisc;                      // start of misc sub-array
};

//
// D2Game types
//

// Proxy item entry in a vendor's item cache (12 bytes, stride confirmed by Ghidra).
struct NpcItemCacheEntry {
	uint8_t  nMin;        // +0x00
	uint8_t  nMax;        // +0x01
	uint8_t  nMagicMin;   // +0x02
	uint8_t  nMagicMax;   // +0x03
	uint32_t dwCode;      // +0x04
	uint8_t  nMagicLevel; // +0x08
	uint8_t  _pad[3];     // +0x09..+0x0b
};
static_assert(sizeof(NpcItemCacheEntry) == 12, "NpcItemCacheEntry size mismatch");

// Per-NPC vendor state entry (0x78 bytes, stride confirmed by Ghidra).
struct VendorChainEntry {
	uint16_t         npcId;        // +0x00  matches (uint16_t)pNpc->unitFlags
	uint8_t          _pad0[0x36];  // +0x02..+0x37
	uint64_t         qwTicks;      // +0x38  GetTickCount64() refresh timestamp
	NpcItemCacheEntry* pItemCache; // +0x40
	uint64_t         nItems;       // +0x48
	uint8_t          _pad1[0x08];  // +0x50..+0x57
	uint32_t*        pPermCache;   // +0x58  array of uint32_t item codes
	uint64_t         nPerms;       // +0x60
	uint8_t          _pad2[0x10];  // +0x68..+0x77
};
static_assert(sizeof(VendorChainEntry) == 0x78, "VendorChainEntry size mismatch");

struct D2GameStrc {
	uint8_t          _unk0[0x104];         // +0x000
	D2Difficulty     difficultyLevel;      // +0x104
	uint8_t          _unk105;              // +0x105
	uint8_t          expansion;            // +0x106
	uint8_t          _unk107[0x21];        // +0x107..+0x127
	uint16_t         wItemFormat;          // +0x128
	uint8_t          _unk12a[0x370e];      // +0x12a..+0x3837
	VendorChainEntry* pVendorChain;        // +0x3838
	uint64_t         nVendorChain;         // +0x3840
	uint8_t          _unk3848[0x1e08];     // +0x3848..+0x564f
	uint32_t         rngSeedLow;           // +0x5650
	uint32_t         rngSeedHigh;          // +0x5654
};
static_assert(offsetof(D2GameStrc, difficultyLevel) == 0x104, "D2GameStrc layout mismatch");
static_assert(offsetof(D2GameStrc, expansion)       == 0x106, "D2GameStrc layout mismatch");
static_assert(offsetof(D2GameStrc, wItemFormat)     == 0x128, "D2GameStrc layout mismatch");
static_assert(offsetof(D2GameStrc, pVendorChain)    == 0x3838, "D2GameStrc layout mismatch");
static_assert(offsetof(D2GameStrc, nVendorChain)    == 0x3840, "D2GameStrc layout mismatch");
static_assert(offsetof(D2GameStrc, rngSeedLow)      == 0x5650, "D2GameStrc layout mismatch");
static_assert(offsetof(D2GameStrc, rngSeedHigh)     == 0x5654, "D2GameStrc layout mismatch");

struct D2QuestDataStrc {
	int32_t nQuestNo;     // +00
	int32_t nUnk0x04;     // +04, possibly padding
	D2GameStrc* pGame;    // +08
};

struct D2UnitStrc;

// Mirrors the Ghidra-recovered D2StatListStrc layout (112 bytes). Only fields
// confirmed via Ghidra are named; the trailing region is internal engine
// linkage (sibling stat-list pointers) that callers don't need directly.
struct D2StatListStrc {
	void*        pMemPool;    // +0x00
	D2UnitStrc*  pUnit;       // +0x08
	uint32_t     ownerType;   // +0x10
	uint32_t     ownerId;     // +0x14
	uint32_t     flags;       // +0x18
	uint32_t     expireFrame; // +0x1c
	int32_t      stateNumber; // +0x20
	uint32_t     skillNumber; // +0x24
	uint32_t     skillLevel;  // +0x28
	uint8_t      _pad0[68];   // +0x2c..+0x6f
};
static_assert(offsetof(D2StatListStrc, expireFrame) == 0x1c, "D2StatListStrc layout mismatch");
static_assert(sizeof(D2StatListStrc) == 112, "D2StatListStrc must be 112 bytes");

// Mirrors the Ghidra-recovered D2UnitStrc layout (448 bytes). Only fields with
// known uses are named; gaps are explicit padding so offsets stay correct.
struct D2UnitStrc {
	D2UnitType       dwUnitType;     // +0x00
	uint32_t         unitFlags;      // +0x04
	uint8_t          _pad0[31];      // +0x09..+0x27 (skips an unnamed byte at +0x08)
	uint32_t         seedLow;        // +0x28
	uint32_t         seedHigh;       // +0x2c
	uint8_t          _pad1[88];      // +0x30..+0x87
	D2StatListStrc*  statList;       // +0x88
	uint8_t          _pad2[112];     // +0x90..+0xff
	int64_t     _unk0x100;      // +0x100
	uint8_t     _pad3[28];      // +0x108..+0x123
	uint32_t    dwFlags;        // +0x124
	uint8_t     _pad4[149];     // +0x128..+0x1bc
	uint8_t     itemTableEntry; // +0x1bd
	uint8_t     _pad5[2];       // +0x1be..+0x1bf
};
static_assert(offsetof(D2UnitStrc, seedLow)        == 0x28,  "D2UnitStrc layout mismatch");
static_assert(offsetof(D2UnitStrc, seedHigh)       == 0x2c,  "D2UnitStrc layout mismatch");
static_assert(offsetof(D2UnitStrc, statList)       == 0x88,  "D2UnitStrc layout mismatch");
static_assert(offsetof(D2UnitStrc, dwFlags)        == 0x124, "D2UnitStrc layout mismatch");
static_assert(offsetof(D2UnitStrc, itemTableEntry) == 0x1bd, "D2UnitStrc layout mismatch");
static_assert(sizeof(D2UnitStrc) == 448, "D2UnitStrc must be 448 bytes");

// ── Memory utilities ──────────────────────────────────────────────────────────

// Allocates `size` bytes of PAGE_EXECUTE_READWRITE memory within ±2 GB of `hint`.
// Required for near stubs that redirect 5-byte relative CALL/JMP instructions.
// Returns nullptr on failure. Free with VirtualFree(ptr, 0, MEM_RELEASE).
D2RLOADER_PLUGIN_EXPORT void* PSh_AllocNear(void* hint, size_t size) noexcept;

// ── Byte patching ─────────────────────────────────────────────────────────────

D2RLOADER_PLUGIN_EXPORT void PSh_PatchBytes(uint32_t pluginId, const D2RLoaderPluginContext* context, uint64_t offset, uint32_t length, const unsigned char* bytes) noexcept;
D2RLOADER_PLUGIN_EXPORT void PSh_UnpatchBytes(uint32_t pluginId, const D2RLoaderPluginContext* context, uint64_t offset) noexcept;

// ── Function hooks ────────────────────────────────────────────────────────────

// Installs a 5-byte (default) or 6-byte E9 near-jmp hook at (exeBase + offset).
// The first `hookSize` bytes at the target must be complete, relocatable instructions
// (no RIP-relative addressing). Writes a near allocation containing the displaced
// trampoline and an FF25 stub, then patches the target with E9 to the stub.
// *originalOut is set to the trampoline — call it to execute the original function.
// Use hookSize=6 when the first two instructions together span 6 bytes
// (e.g. push rbx [2] + sub rsp,N [4]). Verify in Ghidra before changing.
D2RLOADER_PLUGIN_EXPORT bool PSh_InstallHook(uint32_t pluginId, const D2RLoaderPluginContext* context, uint64_t offset, void* hookFn, void** originalOut, uint32_t hookSize = 5) noexcept;
D2RLOADER_PLUGIN_EXPORT void PSh_RemoveHook(uint32_t pluginId, const D2RLoaderPluginContext* context, uint64_t offset) noexcept;

// Redirects the 5-byte E8 CALL at (exeBase + callOffset) to hookFn via a near FF25 stub,
// leaving the original callee function intact. Registered in g_registeredHooks so the
// patch is removed with PSh_RemoveHook(pluginId, context, callOffset).
// hookFn signature must match the callee's calling convention and parameters.
D2RLOADER_PLUGIN_EXPORT bool PSh_PatchCallSite(uint32_t pluginId, const D2RLoaderPluginContext* context, uint64_t callOffset, void* hookFn) noexcept;

// ── RNG ───────────────────────────────────────────────────────────────────────

// Advances unit's RNG seed pair (seedLow/seedHigh) and returns the raw 64-bit
// result, using the same LCG the engine uses for per-unit rolls (see
// SKILLS_FindPotion_DropPotion @ 0x140417018): next = seedLow * 0x6AC690C5 + seedHigh.
// Callers reduce (uint32_t)result as needed (e.g. % 100 for a percent roll).
D2RLOADER_PLUGIN_EXPORT uint64_t PSh_RollUnit(D2UnitStrc* unit) noexcept;

// ── Stats ─────────────────────────────────────────────────────────────────────

// Calls the game's STATLIST_GetStat (0x140224720) to binary-search statList for
// statCode (encoded as `statId << 16`) and return its current value, or 0 if the
// stat isn't present. `minOverride` is normally 0; the game only consults it for
// a small set of internal min-value floors (see STATLIST_GetStat in Ghidra) that
// plugin code doesn't need. Requires exeBase (context->exeBase) since the call
// target lives in the game executable, not plugin-shared.
D2RLOADER_PLUGIN_EXPORT int PSh_GetStat(uintptr_t exeBase, D2StatListStrc* statList,
                                         int statId, int64_t minOverride = 0) noexcept;

// ── INI helpers ───────────────────────────────────────────────────────────────
// All functions check the mod-local INI (mods/<mod>/<mod>.mpq/D2RLoader.ini)
// first, then fall back to the base D2RLoader.ini, then to defaultValue.

D2RLOADER_PLUGIN_EXPORT const wchar_t* PSh_Ini_GetString(const D2RLoaderPluginContext* context, const wchar_t* sectionName, const wchar_t* optionName, const wchar_t* defaultValue);
D2RLOADER_PLUGIN_EXPORT int            PSh_Ini_GetInt(const D2RLoaderPluginContext* context, const wchar_t* sectionName, const wchar_t* optionName, int defaultValue);
D2RLOADER_PLUGIN_EXPORT uint32_t       PSh_Ini_GetItemCode(const D2RLoaderPluginContext* context, const wchar_t* sectionName, const wchar_t* optionName, const wchar_t* defaultValue);
D2RLOADER_PLUGIN_EXPORT uint32_t       PSh_Ini_GetItemTypeCode(const D2RLoaderPluginContext* context, const wchar_t* sectionName, const wchar_t* optionName, const wchar_t* defaultValue);

// ── Utilities ─────────────────────────────────────────────────────────────────
constexpr uint32_t PSh_EncodeItemCode(const char* itemCode)
{
	if (!itemCode) return 0;
	const char lastChar = 0x20;
	return lastChar |
		(((uint32_t)itemCode[2]) << 8) |
		(((uint32_t)itemCode[1]) << 16) |
		(((uint32_t)itemCode[0]) << 24);
}

constexpr uint32_t PSh_EncodeItemTypeCode(const char* itemTypeCode)
{
	if (!itemTypeCode) return 0;
	const char lastChar = itemTypeCode[3] == 0x20 || itemTypeCode[3] == 0x00 ? 0x20 : itemTypeCode[3];
	return lastChar |
		(((uint32_t)itemTypeCode[2]) << 8) |
		(((uint32_t)itemTypeCode[1]) << 16) |
		(((uint32_t)itemTypeCode[0]) << 24);
}