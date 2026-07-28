#include "gamble-screen-limit-policy.h"

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
	using namespace RuffnecKk::GambleScreenLimit;

	const auto missing = ParseConfig(nlohmann::json::object());
	assert(!missing.enabled);
	assert(EffectiveLimit(missing) == VanillaLimit);

	const auto disabled = ParseConfig(nlohmann::json::parse(
		R"json({"gambleScreenLimit":{"enabled":false}})json"
	));
	assert(!disabled.enabled);
	assert(EffectiveLimit(disabled) == VanillaLimit);

	const auto enabled = ParseConfig(nlohmann::json::parse(
		R"json({"gambleScreenLimit":{"enabled":true}})json"
	));
	assert(enabled.enabled);
	assert(EffectiveLimit(enabled) == ExpandedLimit);

	assert(Throws([] {
		ParseConfig(nlohmann::json::parse(
			R"json({"gambleScreenLimit":true})json"
		));
	}));
	assert(Throws([] {
		ParseConfig(nlohmann::json::parse(
			R"json({"gambleScreenLimit":{"enabled":1}})json"
		));
	}));
	assert(Throws([] {
		ParseConfig(nlohmann::json::parse(
			R"json({"gambleScreenLimit":{"enabled":true,"itemLimit":64}})json"
		));
	}));

	assert(argc == 2);
	std::ifstream shippedConfig(argv[1]);
	assert(shippedConfig.is_open());
	const auto root = nlohmann::json::parse(shippedConfig, nullptr, true, true);
	const auto shipped = ParseConfig(root.at("items"));
	assert(!shipped.enabled);
	assert(EffectiveLimit(shipped) == VanillaLimit);
	return 0;
}
