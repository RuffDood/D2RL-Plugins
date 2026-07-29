#include "enhanced-damage-min-max-fix-policy.h"

#include <json.hpp>

#include <cassert>
#include <fstream>
#include <stdexcept>

using namespace RuffnecKk::EnhancedDamageMinMaxFix;

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
	static_assert(PackStat(ItemMaxDamagePercentStat) == 0x00110000);
	static_assert(PackStat(ItemMinDamagePercentStat) == 0x00120000);
	static_assert(PackStat(ItemMaxDamagePercentStat, 1) == 0x00110001);
	static_assert(IsEnhancedDamagePackedStat(0x00110000));
	static_assert(IsEnhancedDamagePackedStat(0x00120000));
	static_assert(!IsEnhancedDamagePackedStat(0x00110001));
	static_assert(!IsEnhancedDamagePackedStat(17));

	assert(!ParseConfig(nlohmann::json::object()).enabled);
	assert(!ParseConfig(nlohmann::json::parse(
		R"json({"enhancedDamageMinMaxFix":{"enabled":false}})json")).enabled);
	assert(ParseConfig(nlohmann::json::parse(
		R"json({"enhancedDamageMinMaxFix":{"enabled":true}})json")).enabled);

	ExpectInvalid([] { ParseConfig(nlohmann::json::array()); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"enhancedDamageMinMaxFix":true})json")); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"enhancedDamageMinMaxFix":{}})json")); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"enhancedDamageMinMaxFix":{"enabled":1}})json")); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"enhancedDamageMinMaxFix":{"enabled":false,"extra":false}})json")); });

	assert(ShouldRestoreSuppressedUpdate(
		ItemUnitType,
		AddItemStatPercentOperation,
		PackStat(ItemMaxDamagePercentStat),
		false,
		510,
		0));
	assert(ShouldRestoreSuppressedUpdate(
		ItemUnitType,
		AddItemStatPercentOperation,
		PackStat(ItemMinDamagePercentStat),
		false,
		505,
		500));
	assert(!ShouldRestoreSuppressedUpdate(
		ItemUnitType,
		AddItemStatPercentOperation,
		PackStat(ItemMaxDamagePercentStat),
		true,
		510,
		0));
	assert(!ShouldRestoreSuppressedUpdate(
		ItemUnitType,
		AddItemStatPercentOperation,
		PackStat(ItemMaxDamagePercentStat),
		false,
		500,
		500));
	assert(!ShouldRestoreSuppressedUpdate(
		0,
		AddItemStatPercentOperation,
		PackStat(ItemMaxDamagePercentStat),
		false,
		510,
		0));
	assert(!ShouldRestoreSuppressedUpdate(
		ItemUnitType,
		12,
		PackStat(ItemMaxDamagePercentStat),
		false,
		510,
		0));

	if (argc == 2) {
		std::ifstream stream(argv[1], std::ios::binary);
		assert(stream.good());
		const auto templateConfig = nlohmann::json::parse(
			stream, nullptr, true, true);
		assert(!ParseConfig(templateConfig.at("items")).enabled);
	}

	return 0;
}
