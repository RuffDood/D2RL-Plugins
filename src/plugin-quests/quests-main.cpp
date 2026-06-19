#include "plugin.h"
#include <plugin-shared.h>
#include "quests-private.h"
#include <plugin-shared-json.h>

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
	int slot = (itemCode == tbl[0]) ? 0 : (itemCode == tbl[1]) ? 1 : 2;
	uint32_t newItem = g_questPluginOptions.QualKehkRuneItems[slot].RewardPerDifficulty[di];
	return g_GiveQuestItemFn(pGame, pPlayer, newItem, param4, quality, param6);
}

// ── JSON loading ──────────────────────────────────────────────────────────────

static RewardType ParseRewardMode(const nlohmann::json& obj)
{
	std::string mode = obj.value("mode", "disabled");
	if (mode == "same")      return RewardType::SamePerDifficulty;
	if (mode == "different") return RewardType::DifferentPerDifficulty;
	return RewardType::Disabled;
}

// Encode a JSON string item code (e.g. "rin", "r07") to a little-endian uint32_t.
// Defaults to the provided fallback string if the key is absent.
static uint32_t ParseItemCode(const nlohmann::json& obj, const char* key, const char* fallback)
{
	std::string code = obj.value(key, fallback);
	return PSh_EncodeItemCode(code.c_str());
}

static ItemQuestReward<uint32_t> ReadItemCodeReward(const nlohmann::json& obj,
                                                     const char* itemKey, const char* fallback)
{
	ItemQuestReward<uint32_t> r;
	r.Reward = ParseItemCode(obj, itemKey, fallback);

	auto pd = obj.value("perDifficulty", nlohmann::json::array());
	for (int i = 0; i < 3; i++) {
		if (i < static_cast<int>(pd.size()) && pd[i].is_object())
			r.RewardPerDifficulty[i] = ParseItemCode(pd[i], "item", fallback);
		else
			r.RewardPerDifficulty[i] = r.Reward;
	}
	return r;
}

static ItemQuestReward<uint8_t> ReadQualityReward(const nlohmann::json& obj,
                                                   const char* qualKey, uint8_t fallback)
{
	ItemQuestReward<uint8_t> r;
	r.Reward = static_cast<uint8_t>(obj.value(qualKey, static_cast<int>(fallback)));

	auto pd = obj.value("perDifficulty", nlohmann::json::array());
	for (int i = 0; i < 3; i++) {
		if (i < static_cast<int>(pd.size()) && pd[i].is_object())
			r.RewardPerDifficulty[i] = static_cast<uint8_t>(pd[i].value("quality", static_cast<int>(fallback)));
		else
			r.RewardPerDifficulty[i] = r.Reward;
	}
	return r;
}

void QuestPluginOptions::Load(const D2RLoaderPluginContext* /*context*/, const nlohmann::json& cfg)
{
	auto doe = cfg.value("denOfEvil", nlohmann::json::object());
	DenOfEvilRewardEnabled    = ParseRewardMode(doe);
	DenOfEvilSkillPointReward = static_cast<uint8_t>(doe.value("skillPoints", 1));

	auto iz = cfg.value("izual", nlohmann::json::object());
	IzualRewardEnabled    = ParseRewardMode(iz);
	IzualSkillPointReward = static_cast<uint8_t>(iz.value("skillPoints", 2));

	auto bb = cfg.value("blackBook", nlohmann::json::object());
	BlackBookRewardEnabled   = ParseRewardMode(bb);
	BlackBookStatPointReward = static_cast<uint8_t>(bb.value("statPoints", 5));

	auto gb = cfg.value("goldenBird", nlohmann::json::object());
	GoldenBirdRewardEnabled = ParseRewardMode(gb);
	GoldenBirdRewardStat    = static_cast<uint8_t>(gb.value("stat", 7));
	GoldenBirdRewardAmount  = static_cast<uint32_t>(gb.value("amount", 0x1400)); // in 256ths for HP/MP

	auto sb = cfg.value("radamentSkillBook", nlohmann::json::object());
	SkillBookRewardEnabled = ParseRewardMode(sb);
	SkillBookRewardStat    = static_cast<uint8_t>(sb.value("stat", 5));
	SkillBookRewardAmount  = static_cast<uint8_t>(sb.value("amount", 1));

	auto akara = cfg.value("akaraCainRing", nlohmann::json::object());
	AkaraCainRingRewardEnabled = ParseRewardMode(akara);
	AkaraCainRingItem          = ReadItemCodeReward(akara, "item", "rin");
	AkaraCainRingQuality       = ReadQualityReward(akara, "quality", 6);

	auto ormus = cfg.value("ormusGidbinnRing", nlohmann::json::object());
	OrmusGidbinnRingRewardEnabled = ParseRewardMode(ormus);
	OrmusGidbinnRingItem          = ReadItemCodeReward(ormus, "item", "rin");
	OrmusGidbinnRingQuality       = ReadQualityReward(ormus, "quality", 6);

	auto qk = cfg.value("qualKehkRunes", nlohmann::json::object());
	QualKehkRuneRewardEnabled = ParseRewardMode(qk);
	static const char* runeDefaults[3] = { "r07", "r08", "r09" };
	auto items = qk.value("items", nlohmann::json::array());
	auto perDiff = qk.value("perDifficulty", nlohmann::json::array());
	for (int slot = 0; slot < 3; slot++) {
		const char* def = runeDefaults[slot];
		std::string sameCode = (slot < static_cast<int>(items.size()) && items[slot].is_string())
		                       ? items[slot].get<std::string>() : def;
		QualKehkRuneItems[slot].Reward = PSh_EncodeItemCode(sameCode.c_str());
		for (int di = 0; di < 3; di++) {
			uint32_t code = QualKehkRuneItems[slot].Reward;
			if (di < static_cast<int>(perDiff.size()) && perDiff[di].is_array()) {
				auto& row = perDiff[di];
				if (slot < static_cast<int>(row.size()) && row[slot].is_string())
					code = PSh_EncodeItemCode(row[slot].get<std::string>().c_str());
			}
			QualKehkRuneItems[slot].RewardPerDifficulty[di] = code;
		}
	}

	ImbueAllowSockets = cfg.value("imbueAllowSockets", false) ? 1 : 0;
}

// ── Plugin exports ────────────────────────────────────────────────────────────

static constexpr D2RLoaderPluginInfo PluginInfo {
	.apiVersion = D2RLOADER_PLUGIN_API_VERSION,
	.id         = "plugin-quests",
	.name       = "Quests Plugin",
	.version    = "0.0.1",
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

	auto cfg = PSh_Json_LoadConfig(context);
	g_questPluginOptions.Load(context, PSh_Json_GetSection(cfg, "quests"));

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
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_QualKehkItem1, 3, (unsigned char*)&g_questPluginOptions.QualKehkRuneItems[0].Reward);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_QualKehkItem2, 3, (unsigned char*)&g_questPluginOptions.QualKehkRuneItems[1].Reward);
		PSh_PatchBytes(PLUGINID_QUESTS, context, OFF_QualKehkItem3, 3, (unsigned char*)&g_questPluginOptions.QualKehkRuneItems[2].Reward);
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
