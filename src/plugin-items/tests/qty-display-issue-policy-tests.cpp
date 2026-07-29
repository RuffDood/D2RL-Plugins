#include "qty-display-issue-policy.h"

#include <algorithm>
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
	using namespace RuffnecKk::QtyDisplayIssue;

	const auto missing = ParseConfig(nlohmann::json::object());
	assert(!missing.enabled);

	const auto disabled = ParseConfig(nlohmann::json::parse(
		R"json({"qtyDisplayIssue":{"enabled":false}})json"
	));
	assert(!disabled.enabled);

	const auto enabled = ParseConfig(nlohmann::json::parse(
		R"json({"qtyDisplayIssue":{"enabled":true}})json"
	));
	assert(enabled.enabled);

	assert(Throws([] {
		ParseConfig(nlohmann::json::parse(
			R"json({"qtyDisplayIssue":true})json"
		));
	}));
	assert(Throws([] {
		ParseConfig(nlohmann::json::parse(
			R"json({"qtyDisplayIssue":{"enabled":1}})json"
		));
	}));
	assert(Throws([] {
		ParseConfig(nlohmann::json::parse(
			R"json({"qtyDisplayIssue":{"enabled":true,"format":"custom"}})json"
		));
	}));

	const auto replacement = BuildQuantitySuppressionPatch();
	static_assert(QuantitySuppressionSignatureSize == 33);
	static_assert(QuantitySuppressionBranchOffset == 21);
	assert(QuantitySuppressionExpected[QuantitySuppressionBranchOffset] == 0x75);
	assert(QuantitySuppressionExpected[QuantitySuppressionBranchOffset + 1] == 0x0F);
	assert(replacement[QuantitySuppressionBranchOffset] == 0x90);
	assert(replacement[QuantitySuppressionBranchOffset + 1] == 0x90);
	const auto changedBytes = std::count_if(
		QuantitySuppressionExpected.begin(),
		QuantitySuppressionExpected.end(),
		[&, index = std::size_t{}](std::uint8_t byte) mutable {
			return byte != replacement[index++];
		}
	);
	assert(changedBytes == 2);

	assert(argc == 2);
	std::ifstream shippedConfig(argv[1]);
	assert(shippedConfig.is_open());
	const auto root = nlohmann::json::parse(shippedConfig, nullptr, true, true);
	const auto shipped = ParseConfig(root.at("items"));
	assert(!shipped.enabled);
	return 0;
}
