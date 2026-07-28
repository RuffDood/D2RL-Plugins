#include "items-ethereal-policy.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace {
struct Record {
	std::array<char, 4> code{};
	std::array<
		std::uint8_t,
		ruffneckk::plugin_items::ethereal::ItemTypeRecordStride - 4
	> padding{};
};
static_assert(sizeof(Record) == ruffneckk::plugin_items::ethereal::ItemTypeRecordStride);

template<class Callback>
bool Throws(Callback&& callback) {
	try {
		callback();
	} catch (const std::exception&) {
		return true;
	}
	return false;
}
}

int main(int argc, char** argv) {
	using namespace ruffneckk::plugin_items::ethereal;

	ItemTypeCode belt{};
	assert(NormalizeItemTypeCode(" BeLt ", belt));
	assert(belt.text[0] == 'b' && belt.text[3] == 't');

	ItemTypeCode gem{};
	assert(NormalizeItemTypeCode("gem", gem));
	assert(gem.bytes[3] == ' ');

	ItemTypeCode invalid{};
	assert(!NormalizeItemTypeCode("too-long", invalid));
	assert(!NormalizeItemTypeCode("a-b", invalid));

	std::array<Record, 3> records{};
	std::memcpy(records[0].code.data(), "armo", 4);
	std::memcpy(records[1].code.data(), "belt", 4);
	std::memcpy(records[2].code.data(), "gem ", 4);
	assert(FindItemTypeId(records.data(), records.size(), sizeof(Record), belt) == 1);
	assert(FindItemTypeId(records.data(), records.size(), sizeof(Record), gem) == 2);
	assert(FindItemTypeId(nullptr, records.size(), sizeof(Record), belt) == -1);
	assert(FindItemTypeId(records.data(), 4097, sizeof(Record), belt) == -1);

	const auto defaults = ParseConfig(nlohmann::json::object());
	assert(!defaults.exclusions.enabled);
	assert(defaults.exclusions.itemTypeCount == 0);
	assert(!defaults.rules.enabled);
	assert(defaults.rules.chancePercent == VanillaChancePercent);
	assert(!HasDirectRulePatches(defaults));

	const auto configured = ParseConfig(nlohmann::json::parse(R"json(
		{
		  "magicItemsSpawnIdentified": false,
		  "etherealExclusions": {
			"enabled": true,
			"itemTypes": ["belt", "BELT", "armo"]
		  },
		  "etherealItemRules": {
			"enabled": true,
			"chancePercent": 6,
			"allowSetItems": true,
			"allowIndestructibleItems": true
		  }
		}
	)json"));
	assert(configured.exclusions.enabled);
	assert(configured.exclusions.itemTypeCount == 2);
	assert(configured.rules.enabled);
	assert(configured.rules.chancePercent == 6);
	assert(PatchChance(configured));
	assert(PatchSetItems(configured));
	assert(PatchIndestructibleItems(configured));
	assert(HasDirectRulePatches(configured));

	assert(Throws([] {
		ParseConfig(nlohmann::json::parse(
			R"json({"etherealExclusions":{"enabled":true,"extra":1}})json"
		));
	}));
	assert(Throws([] {
		ParseConfig(nlohmann::json::parse(
			R"json({"etherealExclusions":{"itemTypes":["too-long"]}})json"
		));
	}));
	assert(Throws([] {
		ParseConfig(nlohmann::json::parse(
			R"json({"etherealItemRules":{"chancePercent":-1}})json"
		));
	}));
	assert(Throws([] {
		ParseConfig(nlohmann::json::parse(
			R"json({"etherealItemRules":{"chancePercent":101}})json"
		));
	}));

	assert(argc == 2);
	std::ifstream shippedConfig(argv[1]);
	assert(shippedConfig.is_open());
	const auto root = nlohmann::json::parse(shippedConfig, nullptr, true, true);
	assert(root.at("skills").at("selfHealParams").is_boolean());
	assert(!root.at("skills").at("selfHealParams").get<bool>());
	const auto shipped = ParseConfig(root.at("items"));
	assert(!shipped.exclusions.enabled);
	assert(shipped.exclusions.itemTypeCount == 0);
	assert(!shipped.rules.enabled);
	assert(shipped.rules.chancePercent == VanillaChancePercent);
	assert(!shipped.rules.allowSetItems);
	assert(!shipped.rules.allowIndestructibleItems);
	assert(!HasDirectRulePatches(shipped));
	return 0;
}
