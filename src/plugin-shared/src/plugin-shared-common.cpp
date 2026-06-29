#include <plugin-shared.h>

static constexpr uint64_t OFF_GetStat = 0x224720;

using GetStat_t = int(__fastcall*)(int64_t statContainer, uint64_t statCode, int64_t unused);

D2RLOADER_PLUGIN_EXPORT int PSh_GetStat(uintptr_t exeBase, D2StatListStrc* statList,
                                         int statId, int64_t minOverride) noexcept {
    auto GetStat = reinterpret_cast<GetStat_t>(exeBase + OFF_GetStat);
    return GetStat(reinterpret_cast<int64_t>(statList), static_cast<uint64_t>(statId) << 16, minOverride);
}

static uint64_t PSh_RollSeed(uint32_t& seedLow, uint32_t& seedHigh) noexcept {
	uint64_t next = static_cast<uint64_t>(seedLow) * 0x6AC690C5ULL + seedHigh;
	seedLow = static_cast<uint32_t>(next);
	seedHigh = static_cast<uint32_t>(next >> 32);
	return next;
}

D2RLOADER_PLUGIN_EXPORT uint64_t PSh_RollUnit(D2UnitStrc* unit) noexcept {
	return PSh_RollSeed(unit->seedLow, unit->seedHigh);
}

D2RLOADER_PLUGIN_EXPORT uint64_t PSh_RollGame(D2GameStrc* game) noexcept {
	return PSh_RollSeed(game->rngSeedLow, game->rngSeedHigh);
}