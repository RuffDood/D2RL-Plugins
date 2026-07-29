#include "bulk-skill-point-allocation-policy.h"

#include <json.hpp>

#include <cassert>
#include <fstream>
#include <stdexcept>

using namespace RuffnecKk::BulkSkillPointAllocation;

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
	static_assert(ResolveMode(false, false) == AllocationMode::Single);
	static_assert(ResolveMode(false, true) == AllocationMode::CtrlBatch);
	static_assert(ResolveMode(true, false) == AllocationMode::ShiftAll);
	static_assert(ResolveMode(true, true) == AllocationMode::CtrlBatch);
	static_assert(NativeSkillPacketExtra(AllocationMode::Single, 1) == 0);
	static_assert(NativeSkillPacketExtra(AllocationMode::CtrlBatch, 5) == 4);
	static_assert(NativeSkillPacketExtra(AllocationMode::CtrlBatch, 1'000) == 999);
	static_assert(NativeSkillPacketExtra(AllocationMode::ShiftAll, 1) == 0xFFFF);

	const auto absent = ParseConfig(nlohmann::json::object());
	assert(!absent.enabled);
	assert(absent.skillPointsPerCtrlClick == 5);
	assert(!absent.confirmShiftAllocation);

	const auto enabled = ParseConfig(nlohmann::json::parse(R"json({
		"bulkSkillPointAllocation": {
			"enabled": true,
			"skillPointsPerCtrlClick": 25,
			"confirmShiftAllocation": true,
			"shiftConfirmationKey": "customKey",
			"shiftConfirmationFallback": "Custom fallback"
		}
	})json"));
	assert(enabled.enabled);
	assert(enabled.skillPointsPerCtrlClick == 25);
	assert(enabled.confirmShiftAllocation);
	assert(enabled.shiftConfirmationKey == "customKey");
	assert(enabled.shiftConfirmationFallback == "Custom fallback");

	ExpectInvalid([] { ParseConfig(nlohmann::json::array()); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"bulkSkillPointAllocation":true})json")); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"bulkSkillPointAllocation":{}})json")); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"bulkSkillPointAllocation":{"enabled":false,"skillPointsPerCtrlClick":0}})json")); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"bulkSkillPointAllocation":{"enabled":false,"skillPointsPerCtrlClick":1001}})json")); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"bulkSkillPointAllocation":{"enabled":false,"extra":false}})json")); });

	if (argc == 2) {
		std::ifstream stream(argv[1], std::ios::binary);
		assert(stream.good());
		const auto templateConfig = nlohmann::json::parse(
			stream, nullptr, true, true);
		const auto policy = ParseConfig(templateConfig.at("skills"));
		assert(!policy.enabled);
		assert(policy.skillPointsPerCtrlClick == 5);
		assert(!policy.confirmShiftAllocation);
	}

	return 0;
}
