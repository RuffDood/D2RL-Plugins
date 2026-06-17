#include "plugin.h"
#include <plugin-shared.h>
#include "quests-private.h"
#include <cwchar>

// ── Addresses (offsets from exe base 0x140000000) ────────────────────────────

static constexpr uint64_t OFF_DenOfEvilPatch1 = 0x456CDA;
static constexpr uint64_t OFF_DenOfEvilPatch2 = 0x456CEA;
static constexpr uint64_t OFF_DenOfEvilPatch3 = 0x456D04;
static constexpr uint64_t OFF_FallenAngelPatch1 = 0x4660FE;
static constexpr uint64_t OFF_FallenAngelPatch2 = 0x46610E;
static constexpr uint64_t OFF_FallenAngelPatch3 = 0x466128;
static constexpr uint64_t OFF_BlackBookPatch1 = 0x46C9C7;
static constexpr uint64_t OFF_BlackBookPatch2 = 0x46C9E2;
static constexpr uint64_t OFF_GoldenBirdStatPatch1 = 0x3F23D7;
static constexpr uint64_t OFF_GoldenBirdStatPatch2 = 0x3F23E6;
static constexpr uint64_t OFF_GoldenBirdAmountPatch1 = 0x3F23CB;
static constexpr uint64_t OFF_GoldenBirdAmountPatch2 = 0x3F23EE;
static constexpr uint64_t OFF_SkillBookStatPatch1 = 0x3F2594;
static constexpr uint64_t OFF_SkillBookStatPatch2 = 0x3F2585;
static constexpr uint64_t OFF_SkillBookAmountPatch1 = 0x3F2581;
static constexpr uint64_t OFF_SkillBookAmountPatch2 = 0x3F259C;
static constexpr uint64_t OFF_AkaraRingItem = 0x463B1D;
static constexpr uint64_t OFF_AkaraRingItemQuality = 0x463B17;
static constexpr uint64_t OFF_AkaraRingCallSite = 0x463B34;
static constexpr uint64_t OFF_OrmusRingItem = 0x47781D;
static constexpr uint64_t OFF_OrmusRingItemQuality = 0x477830;
static constexpr uint64_t OFF_OrmusRingCallSite = 0x477834;
static constexpr uint64_t OFF_GiveQuestItem = 0x35E3B0;
static constexpr uint64_t OFF_QualKehkItem1       = 0x19EB430;
static constexpr uint64_t OFF_QualKehkItem2       = 0x19EB434;
static constexpr uint64_t OFF_QualKehkItem3       = 0x19EB438;
static constexpr uint64_t OFF_QualKehkCallSite    = 0x3D4AFF;
static constexpr uint64_t OFF_ImbueSocket1 = 0x246597;
static constexpr uint64_t OFF_ImbueSocket2 = 0x2468b6;

// ── Plugin state ──────────────────────────────────────────────────────────────

static QuestPluginOptions g_questPluginOptions;

using GiveQuestItemFn_t = int* (__fastcall*)(const D2GameStrc* pGame, void* pPlayer, uint32_t itemCode, int param4, uint32_t quality, int param6);
static GiveQuestItemFn_t g_GiveQuestItemFn = nullptr;
static uintptr_t g_exeBase = 0;
static constexpr const wchar_t* QuestPluginSection = L"PluginPack.Quests";

// ── DifferentPerDifficulty call-site hooks ────────────────────────────────────
static int* __fastcall Hook_AkaraCainRingDiffReward(const D2GameStrc* pGame, void* pPlayer, uint32_t, int param4, uint32_t, int param6)
{
	const uint8_t di = static_cast<uint8_t>(pGame->difficultyLevel) <= 2 ? static_cast<uint8_t>(pGame->difficultyLevel) : 0;
	return g_GiveQuestItemFn(pGame, pPlayer,
		g_questPluginOptions.AkaraCainRingItem.RewardPerDifficulty[di],
		param4,
		g_questPluginOptions.AkaraCainRingQuality.RewardPerDifficulty[di],
		param6);
}

static int* __fastcall Hook_OrmusGidbinnRingDiffReward(const D2GameStrc* pGame, void* pPlayer, uint32_t, int param4, uint32_t, int param6)
{
	const uint8_t di = static_cast<uint8_t>(pGame->difficultyLevel) <= 2 ? static_cast<uint8_t>(pGame->difficultyLevel) : 0;
	return g_GiveQuestItemFn(pGame, pPlayer,
		g_questPluginOptions.OrmusGidbinnRingItem.RewardPerDifficulty[di],
		param4,
		g_questPluginOptions.OrmusGidbinnRingQuality.RewardPerDifficulty[di],
		param6);
}

static int* __fastcall Hook_QualKehkRuneDiffReward(const D2GameStrc* pGame, void* pPlayer, uint32_t itemCode, int param4, uint32_t quality, int param6)
{
	const uint8_t di = static_cast<uint8_t>(pGame->difficultyLevel) <= 2 ? static_cast<uint8_t>(pGame->difficultyLevel) : 0;
	const auto* tbl = reinterpret_cast<const uint32_t*>(g_exeBase + OFF_QualKehkItem1);
	uint32_t newItem;
	if (itemCode == tbl[0])
		newItem = g_questPluginOptions.QualKehkRuneItem1.RewardPerDifficulty[di];
	else if (itemCode == tbl[1])
		newItem = g_questPluginOptions.QualKehkRuneItem2.RewardPerDifficulty[di];
	else
		newItem = g_questPluginOptions.QualKehkRuneItem3.RewardPerDifficulty[di];
	return g_GiveQuestItemFn(pGame, pPlayer, newItem, param4, quality, param6);
}

// ── INI loading ───────────────────────────────────────────────────────────────

// Reads all four per-difficulty variants of an item-code key from the INI.
// Keys follow the pattern: baseKey, baseKey+"Normal", baseKey+"Nightmare", baseKey+"Hell".
static ItemQuestReward<uint32_t> ReadItemCodeReward(
	const D2RLoaderPluginContext* ctx, const wchar_t* section,
	const wchar_t* baseKey, const wchar_t* def)
{
	wchar_t key[128];
	ItemQuestReward<uint32_t> r;
	r.Reward                 = PSh_Ini_GetItemCode(ctx, section, baseKey, def);
	swprintf_s(key, L"%ls%ls", baseKey, L"Normal");    r.RewardPerDifficulty[0] = PSh_Ini_GetItemCode(ctx, section, key, def);
	swprintf_s(key, L"%ls%ls", baseKey, L"Nightmare"); r.RewardPerDifficulty[1] = PSh_Ini_GetItemCode(ctx, section, key, def);
	swprintf_s(key, L"%ls%ls", baseKey, L"Hell");      r.RewardPerDifficulty[2] = PSh_Ini_GetItemCode(ctx, section, key, def);
	return r;
}

// Reads all four per-difficulty variants of a quality (uint8_t) key from the INI.
static ItemQuestReward<uint8_t> ReadQualityReward(
	const D2RLoaderPluginContext* ctx, const wchar_t* section,
	const wchar_t* baseKey, int def)
{
	wchar_t key[128];
	ItemQuestReward<uint8_t> r;
	r.Reward                 = static_cast<uint8_t>(PSh_Ini_GetInt(ctx, section, baseKey, def));
	swprintf_s(key, L"%ls%ls", baseKey, L"Normal");    r.RewardPerDifficulty[0] = static_cast<uint8_t>(PSh_Ini_GetInt(ctx, section, key, def));
	swprintf_s(key, L"%ls%ls", baseKey, L"Nightmare"); r.RewardPerDifficulty[1] = static_cast<uint8_t>(PSh_Ini_GetInt(ctx, section, key, def));
	swprintf_s(key, L"%ls%ls", baseKey, L"Hell");      r.RewardPerDifficulty[2] = static_cast<uint8_t>(PSh_Ini_GetInt(ctx, section, key, def));
	return r;
}

void QuestPluginOptions::Load(const D2RLoaderPluginContext* context, const wchar_t* section) {
	DenOfEvilRewardEnabled    = static_cast<RewardType>(PSh_Ini_GetInt(context, section, L"EnableDenOfEvilRewardChange", 0));
	DenOfEvilSkillPointReward = static_cast<uint8_t>(PSh_Ini_GetInt(context, section, L"DenOfEvilSkillPointReward", 1));
	IzualRewardEnabled        = static_cast<RewardType>(PSh_Ini_GetInt(context, section, L"EnableIzualRewardChange", 0));
	IzualSkillPointReward     = static_cast<uint8_t>(PSh_Ini_GetInt(context, section, L"IzualSkillPointReward", 2));
	BlackBookRewardEnabled    = static_cast<RewardType>(PSh_Ini_GetInt(context, section, L"EnableBlackBookRewardChange", 0));
	BlackBookStatPointReward  = static_cast<uint8_t>(PSh_Ini_GetInt(context, section, L"BlackBookStatPointReward", 5));
	GoldenBirdRewardEnabled = static_cast<RewardType>(PSh_Ini_GetInt(context, section, L"EnableGoldenBirdRewardChange", 0));
	GoldenBirdRewardStat = static_cast<uint8_t>(PSh_Ini_GetInt(context, section, L"GoldenBirdRewardStat", 7));
	GoldenBirdRewardAmount = static_cast<uint16_t>(PSh_Ini_GetInt(context, section, L"GoldenBirdRewardAmount", 0x1400)); // note: on HP/MP this is in 256ths
	SkillBookRewardEnabled = static_cast<RewardType>(PSh_Ini_GetInt(context, section, L"EnableRadamentSkillBookRewardChange", 0));
	SkillBookRewardStat = static_cast<uint8_t>(PSh_Ini_GetInt(context, section, L"RadamentSkillBookRewardStat", 5));
	SkillBookRewardAmount = static_cast<uint8_t>(PSh_Ini_GetInt(context, section, L"RadamentSkillBookRewardAmount", 1));

	AkaraCainRingRewardEnabled = static_cast<RewardType>(PSh_Ini_GetInt(context, section, L"EnableAkaraCainRewardChange", 0));
	AkaraCainRingItem          = ReadItemCodeReward(context, section, L"AkaraCainRewardItem", L" nir");
	AkaraCainRingQuality       = ReadQualityReward(context, section, L"AkaraCainRewardItemQuality", 6);

	OrmusGidbinnRingRewardEnabled = static_cast<RewardType>(PSh_Ini_GetInt(context, section, L"EnableOrmusGidbinnRewardChange", 0));
	OrmusGidbinnRingItem          = ReadItemCodeReward(context, section, L"OrmusGidbinnRewardItem", L" nir");
	OrmusGidbinnRingQuality       = ReadQualityReward(context, section, L"OrmusGidbinnRewardItemQuality", 6);

	QualKehkRuneRewardEnabled     = static_cast<RewardType>(PSh_Ini_GetInt(context, section, L"EnableQualKehkRewardChange", 0));
	QualKehkRuneItem1             = ReadItemCodeReward(context, section, L"QualKehkRewardItem1", L" 70r");
	QualKehkRuneItem2             = ReadItemCodeReward(context, section, L"QualKehkRewardItem2", L" 80r");
	QualKehkRuneItem3             = ReadItemCodeReward(context, section, L"QualKehkRewardItem3", L" 90r");

	ImbueAllowSockets             = PSh_Ini_GetInt(context, section, L"ImbueAllowSocketedItems", 0);
}

// ── Plugin exports ────────────────────────────────────────────────────────────

static constexpr D2RLoaderPluginInfo PluginInfo {
	.apiVersion = D2RLOADER_PLUGIN_API_VERSION,
	.id         = "plugin-quests",
	.name       = "Quests Plugin",
	.version    = "1.0.0",
	.author     = "eezstreet",
	.flags      = D2RLoaderPluginFlag_None,
};

static unsigned char DenOfEvil_PushPopPatch[] = {0x6A, 0x05, 0x41, 0x58};

D2RLOADER_PLUGIN_EXPORT const D2RLoaderPluginInfo* __cdecl D2RLoaderGetPluginInfo() noexcept {
	return &PluginInfo;
}

D2RLOADER_PLUGIN_EXPORT bool __cdecl D2RLoaderLoadHooks(const D2RLoaderPluginContext* context) noexcept {
	if (!context || context->apiVersion < D2RLOADER_PLUGIN_API_VERSION) {
		return false;
	}

	g_questPluginOptions.Load(context, QuestPluginSection);

	if (g_questPluginOptions.DenOfEvilRewardEnabled == RewardType::SamePerDifficulty)
	{	// Fixed reward on each difficulty
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_DenOfEvilPatch1, 1, &g_questPluginOptions.DenOfEvilSkillPointReward);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_DenOfEvilPatch2, 4, DenOfEvil_PushPopPatch);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_DenOfEvilPatch3, 1, &g_questPluginOptions.DenOfEvilSkillPointReward);
	}
	if (g_questPluginOptions.IzualRewardEnabled == RewardType::SamePerDifficulty)
	{	// Fixed reward on each difficulty
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_FallenAngelPatch1, 1, &g_questPluginOptions.IzualSkillPointReward);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_FallenAngelPatch2, 4, DenOfEvil_PushPopPatch);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_FallenAngelPatch3, 1, &g_questPluginOptions.IzualSkillPointReward);
	}
	if (g_questPluginOptions.BlackBookRewardEnabled == RewardType::SamePerDifficulty)
	{	// Fixed reward on each difficulty
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_BlackBookPatch1, 1, &g_questPluginOptions.BlackBookStatPointReward);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_BlackBookPatch2, 1, &g_questPluginOptions.BlackBookStatPointReward);
	}
	if (g_questPluginOptions.GoldenBirdRewardEnabled == RewardType::SamePerDifficulty)
	{
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_GoldenBirdStatPatch1, 1, &g_questPluginOptions.GoldenBirdRewardStat);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_GoldenBirdStatPatch2, 1, &g_questPluginOptions.GoldenBirdRewardStat);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_GoldenBirdAmountPatch1, 4, (unsigned char*)&g_questPluginOptions.GoldenBirdRewardAmount);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_GoldenBirdAmountPatch2, 4, (unsigned char*)&g_questPluginOptions.GoldenBirdRewardAmount);
	}
	if (g_questPluginOptions.SkillBookRewardEnabled == RewardType::SamePerDifficulty)
	{
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_SkillBookAmountPatch1, 1, &g_questPluginOptions.SkillBookRewardAmount);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_SkillBookAmountPatch2, 1, &g_questPluginOptions.SkillBookRewardAmount);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_SkillBookStatPatch1, 1, &g_questPluginOptions.SkillBookRewardStat);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_SkillBookStatPatch2, 1, &g_questPluginOptions.SkillBookRewardStat);
	}

	if (g_questPluginOptions.AkaraCainRingRewardEnabled == RewardType::SamePerDifficulty)
	{	// Fixed item on each difficulty
		// Turn this into a XOR EAX,EAX; MOV AL, (reward)
		uint32_t rewardBytes = 0x31C0B000;
		rewardBytes &= 0xFFFFFF00;
		rewardBytes |= g_questPluginOptions.AkaraCainRingQuality.Reward;

		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_AkaraRingItem, 4, (unsigned char*)&g_questPluginOptions.AkaraCainRingItem.Reward);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_AkaraRingItemQuality, 4, (unsigned char*)&rewardBytes);
	}
	else if (g_questPluginOptions.AkaraCainRingRewardEnabled == RewardType::DifferentPerDifficulty)
	{	// Different item/quality depending on difficulty
		g_GiveQuestItemFn = reinterpret_cast<GiveQuestItemFn_t>(context->exeBase + OFF_GiveQuestItem);
		PSh_PatchCallSite(PLUGINID_QUESTS, context, OFF_AkaraRingCallSite, reinterpret_cast<void*>(Hook_AkaraCainRingDiffReward));
	}

	if (g_questPluginOptions.OrmusGidbinnRingRewardEnabled == RewardType::SamePerDifficulty)
	{	// Fixed item on each difficulty
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_OrmusRingItem, 4, (unsigned char*)&g_questPluginOptions.OrmusGidbinnRingItem.Reward);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_OrmusRingItemQuality, 1, &g_questPluginOptions.OrmusGidbinnRingQuality.Reward);
	}
	else if (g_questPluginOptions.OrmusGidbinnRingRewardEnabled == RewardType::DifferentPerDifficulty)
	{	// Different item/quality depending on difficulty
		g_GiveQuestItemFn = reinterpret_cast<GiveQuestItemFn_t>(context->exeBase + OFF_GiveQuestItem);
		PSh_PatchCallSite(PLUGINID_QUESTS, context, OFF_OrmusRingCallSite, reinterpret_cast<void*>(Hook_OrmusGidbinnRingDiffReward));
	}

	if (g_questPluginOptions.QualKehkRuneRewardEnabled == RewardType::SamePerDifficulty)
	{
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_QualKehkItem1, 3, (unsigned char*)&g_questPluginOptions.QualKehkRuneItem1.Reward);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_QualKehkItem2, 3, (unsigned char*)&g_questPluginOptions.QualKehkRuneItem2.Reward);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_QualKehkItem3, 3, (unsigned char*)&g_questPluginOptions.QualKehkRuneItem3.Reward);
	}
	else if (g_questPluginOptions.QualKehkRuneRewardEnabled == RewardType::DifferentPerDifficulty)
	{
		g_exeBase = context->exeBase;
		g_GiveQuestItemFn = reinterpret_cast<GiveQuestItemFn_t>(context->exeBase + OFF_GiveQuestItem);
		PSh_PatchCallSite(PLUGINID_QUESTS, context, OFF_QualKehkCallSite, reinterpret_cast<void*>(Hook_QualKehkRuneDiffReward));
	}

	if (g_questPluginOptions.ImbueAllowSockets)
	{
		unsigned char doubleNOP[] = { 0x90, 0x90 };
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_ImbueSocket1, 2, doubleNOP);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_ImbueSocket2, 2, doubleNOP);
	}

	return true;
}

D2RLOADER_PLUGIN_EXPORT void __cdecl D2RLoaderUnload() noexcept {
}
