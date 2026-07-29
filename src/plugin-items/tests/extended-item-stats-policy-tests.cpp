#include "extended-item-stats-policy.h"

#include <cassert>
#include <exception>
#include <fstream>

using namespace RuffnecKk::ExtendedItemStats;

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
	assert(ParseConfig(nlohmann::json::object()).enabled);
	assert(ParseConfig(nlohmann::json::parse(
		R"json({"extendedItemStats":{"enabled":true}})json")).enabled);
	assert(!ParseConfig(nlohmann::json::parse(
		R"json({"extendedItemStats":{"enabled":false}})json")).enabled);

	ExpectInvalid([] { ParseConfig(nlohmann::json::array()); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"extendedItemStats":true})json")); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"extendedItemStats":{}})json")); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"extendedItemStats":{"enabled":1}})json")); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"extendedItemStats":{"enabled":true,"extra":false}})json")); });

	assert(argc == 2);
	std::ifstream stream(argv[1], std::ios::binary);
	assert(stream.good());
	const auto root = nlohmann::json::parse(stream, nullptr, true, true);
	assert(ParseConfig(root.at("items")).enabled);
	return 0;
}
