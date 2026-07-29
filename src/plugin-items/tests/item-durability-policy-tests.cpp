#include "item-durability-policy.h"

#include <cassert>
#include <exception>
#include <fstream>

using namespace RuffnecKk::ItemDurability;

template<class Callback>
void ExpectInvalid(Callback&& callback) {
	bool rejected = false;
	try {
		callback();
	} catch (const std::exception&) {
		rejected = true;
	}
	assert(rejected);
}

int main(int argc, char** argv) {
	static_assert(IsBowOrCrossbowItemTypeCode(PackItemTypeCode('b', 'o', 'w')));
	static_assert(IsBowOrCrossbowItemTypeCode(PackItemTypeCode('x', 'b', 'o', 'w')));
	static_assert(!IsBowOrCrossbowItemTypeCode(PackItemTypeCode('a', 'b', 'o', 'w')));

	assert(!PreventsLoss(0, 0));
	assert(PreventsLoss(50, 49));
	assert(!PreventsLoss(50, 50));
	assert(PreventsLoss(100, 99));
	assert(EffectiveChanceBasisPoints(4, 0) == 400);
	assert(EffectiveChanceBasisPoints(4, 50) == 200);
	assert(EffectiveChanceBasisPoints(10, 75) == 250);
	assert(EffectiveChanceBasisPoints(10, 100) == 0);

	assert(TargetEtherealMaxDurability(20, 25) == 6);
	assert(TargetEtherealMaxDurability(20, 50) == 11);
	assert(TargetEtherealMaxDurability(20, 75) == 16);
	assert(ApplyVanillaEtherealHalving(
		EncodeForVanillaEtherealHalving(20, 50)) == 11);
	assert(TargetEtherealMaxDurability(30, 100) == 30);
	assert(TargetEtherealMaxDurability(20, 200) == 40);
	assert(TargetEtherealMaxDurability(500, 200) == 255);
	assert(ApplyVanillaEtherealHalving(EncodeEtherealMaximumTarget(255)) == 255);

	const auto absent = ParseConfig(nlohmann::json::object());
	assert(!absent.enabled);
	assert(absent.etherealMaximumPercent == 50);

	const auto vanilla = nlohmann::json::parse(R"json({
		"itemDurability": {
			"enabled": false,
			"normalResistancePercent": 0,
			"etherealResistancePercent": 0,
			"etherealMaximumPercent": 50,
			"forceMaximumDurability": false,
			"bowsAndCrossbowsHaveDurability": false,
			"diagnostics": false
		}
	})json");
	const auto policy = ParseConfig(vanilla);
	assert(!policy.enabled);
	assert(policy.normalResistancePercent == 0);
	assert(policy.etherealResistancePercent == 0);
	assert(policy.etherealMaximumPercent == 50);
	assert(!policy.forceMaximumDurability);
	assert(!policy.bowsAndCrossbowsHaveDurability);
	assert(!policy.diagnostics);

	ExpectInvalid([] { ParseConfig(nlohmann::json::array()); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"itemDurability":true})json")); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"itemDurability":{"enabled":false}})json")); });
	ExpectInvalid([&] {
		auto invalid = vanilla;
		invalid["itemDurability"]["normalResistancePercent"] = 101;
		ParseConfig(invalid);
	});
	ExpectInvalid([&] {
		auto invalid = vanilla;
		invalid["itemDurability"]["etherealResistancePercent"] = -1;
		ParseConfig(invalid);
	});
	ExpectInvalid([&] {
		auto invalid = vanilla;
		invalid["itemDurability"]["etherealMaximumPercent"] = 0;
		ParseConfig(invalid);
	});
	ExpectInvalid([&] {
		auto invalid = vanilla;
		invalid["itemDurability"]["enabled"] = 1;
		ParseConfig(invalid);
	});
	ExpectInvalid([&] {
		auto invalid = vanilla;
		invalid["itemDurability"]["extra"] = false;
		ParseConfig(invalid);
	});

	if (argc == 2) {
		std::ifstream stream(argv[1], std::ios::binary);
		assert(stream.good());
		const auto templateConfig = nlohmann::json::parse(
			stream, nullptr, true, true);
		const auto templatePolicy = ParseConfig(templateConfig.at("items"));
		assert(!templatePolicy.enabled);
		assert(templatePolicy.normalResistancePercent == 0);
		assert(templatePolicy.etherealResistancePercent == 0);
		assert(templatePolicy.etherealMaximumPercent == 50);
		assert(!templatePolicy.forceMaximumDurability);
		assert(!templatePolicy.bowsAndCrossbowsHaveDurability);
	}

	return 0;
}
