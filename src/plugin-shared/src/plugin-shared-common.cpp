#include <plugin-shared.h>

static constexpr uint64_t OFF_GetStat = 0xf9b10;

using GetStat_t = int(__fastcall*)(int64_t statContainer, uint64_t statCode, int64_t unused);

extern "C" int PSh_GetStat(uintptr_t exeBase, D2StatListStrc* statList,
                            int statId, int64_t minOverride) noexcept {
    auto GetStat = reinterpret_cast<GetStat_t>(exeBase + OFF_GetStat);
    return GetStat(reinterpret_cast<int64_t>(statList), static_cast<uint64_t>(statId) << 16, minOverride);
}

extern "C" uint64_t PSh_RollUnit(D2UnitStrc* unit) noexcept {
	uint64_t next = static_cast<uint64_t>(unit->seedLow) * 0x6AC690C5ULL + unit->seedHigh;
	unit->seedLow  = static_cast<uint32_t>(next);
	unit->seedHigh = static_cast<uint32_t>(next >> 32);
	return next;
}