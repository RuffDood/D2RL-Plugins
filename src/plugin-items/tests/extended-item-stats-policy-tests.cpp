#include "extended-item-stats-policy.h"

#include "../../../tests/test-check.h"
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
	TEST_REQUIRE(rejected);
}

int main(int argc, char** argv) {
	const auto absent = ParseConfig(nlohmann::json::object());
	TEST_REQUIRE(absent.enabled);
	TEST_REQUIRE(!absent.oversizedItemDataTransport);
	TEST_REQUIRE(!absent.showScrollBar);
	const auto defaultTransport = ParseConfig(nlohmann::json::parse(
		R"json({"extendedItemStats":{"enabled":true}})json"));
	TEST_REQUIRE(defaultTransport.enabled);
	TEST_REQUIRE(!defaultTransport.oversizedItemDataTransport);
	TEST_REQUIRE(!defaultTransport.showScrollBar);
	const auto enabledTransport = ParseConfig(nlohmann::json::parse(
		R"json({"extendedItemStats":{"enabled":true,"oversizedItemDataTransport":true,"showScrollBar":true}})json"));
	TEST_REQUIRE(enabledTransport.enabled);
	TEST_REQUIRE(enabledTransport.oversizedItemDataTransport);
	TEST_REQUIRE(enabledTransport.showScrollBar);
	TEST_REQUIRE(!ParseConfig(nlohmann::json::parse(
		R"json({"extendedItemStats":{"enabled":false}})json")).enabled);

	ExpectInvalid([] { ParseConfig(nlohmann::json::array()); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"extendedItemStats":true})json")); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"extendedItemStats":{}})json")); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"extendedItemStats":{"enabled":1}})json")); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"extendedItemStats":{"enabled":true,"oversizedItemDataTransport":1}})json")); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"extendedItemStats":{"enabled":true,"showScrollBar":1}})json")); });
	ExpectInvalid([] { ParseConfig(nlohmann::json::parse(
		R"json({"extendedItemStats":{"enabled":true,"extra":false}})json")); });

	TEST_REQUIRE(argc == 2);
	std::ifstream stream(argv[1], std::ios::binary);
	TEST_REQUIRE(stream.good());
	const auto root = nlohmann::json::parse(stream, nullptr, true, true);
	const auto publicConfig = ParseConfig(root.at("items"));
	TEST_REQUIRE(publicConfig.enabled);
	TEST_REQUIRE(!publicConfig.oversizedItemDataTransport);
	TEST_REQUIRE(!publicConfig.showScrollBar);
	return 0;
}
