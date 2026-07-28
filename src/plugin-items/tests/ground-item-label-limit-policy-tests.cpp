#include "ground-item-label-limit-policy.h"

#include <cassert>
#include <fstream>
#include <stdexcept>

namespace {

template<class Callback>
bool Throws(Callback&& callback) {
	try {
		callback();
	} catch (const std::exception&) {
		return true;
	}
	return false;
}

} // namespace

int main(int argc, char** argv) {
	using namespace RuffnecKk::GroundItemLabelLimit;

	assert(!IsSupportedLimit(32));
	assert(IsSupportedLimit(64));
	assert(IsSupportedLimit(128));
	assert(!IsSupportedLimit(256));
	assert(LabelArrayByteOffset(32) == 0x2880);
	assert(LabelArrayByteOffset(64) == 0x5100);
	assert(LabelArrayByteOffset(128) == 0xA200);

	const auto missing = ParseConfig(nlohmann::json::object());
	assert(!missing.enabled);
	assert(missing.limit == DefaultExpandedLimit);
	assert(EffectiveLimit(missing) == VanillaLimit);

	const auto disabled = ParseConfig(nlohmann::json::parse(
		R"json({"groundItemLabels":{"enabled":false,"limit":64}})json"
	));
	assert(!disabled.enabled);
	assert(EffectiveLimit(disabled) == VanillaLimit);

	const auto enabled64 = ParseConfig(nlohmann::json::parse(
		R"json({"groundItemLabels":{"enabled":true,"limit":64}})json"
	));
	assert(enabled64.enabled);
	assert(EffectiveLimit(enabled64) == 64);

	const auto enabled128 = ParseConfig(nlohmann::json::parse(
		R"json({"groundItemLabels":{"enabled":true,"limit":128}})json"
	));
	assert(enabled128.enabled);
	assert(EffectiveLimit(enabled128) == 128);

	assert(Throws([] {
		ParseConfig(nlohmann::json::parse(R"json({"groundItemLabels":true})json"));
	}));
	assert(Throws([] {
		ParseConfig(nlohmann::json::parse(
			R"json({"groundItemLabels":{"enabled":1,"limit":64}})json"
		));
	}));
	assert(Throws([] {
		ParseConfig(nlohmann::json::parse(
			R"json({"groundItemLabels":{"enabled":true,"limit":32}})json"
		));
	}));
	assert(Throws([] {
		ParseConfig(nlohmann::json::parse(
			R"json({"groundItemLabels":{"enabled":true,"limit":256}})json"
		));
	}));
	assert(Throws([] {
		ParseConfig(nlohmann::json::parse(
			R"json({"groundItemLabels":{"enabled":true,"limit":64,"extra":false}})json"
		));
	}));

	assert(argc == 2);
	std::ifstream shippedConfig(argv[1]);
	assert(shippedConfig.is_open());
	const auto root = nlohmann::json::parse(shippedConfig, nullptr, true, true);
	const auto shipped = ParseConfig(root.at("items"));
	assert(!shipped.enabled);
	assert(shipped.limit == DefaultExpandedLimit);
	assert(EffectiveLimit(shipped) == VanillaLimit);
	return 0;
}
