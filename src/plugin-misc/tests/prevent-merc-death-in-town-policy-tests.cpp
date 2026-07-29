#include "prevent-merc-death-in-town-policy.h"

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
    using namespace RuffnecKk::PreventMercDeathInTown;

    assert(IsHirelingClass(271));
    assert(IsHirelingClass(338));
    assert(IsHirelingClass(359));
    assert(IsHirelingClass(560));
    assert(IsHirelingClass(561));
    assert(!IsHirelingClass(0));
    assert(!IsHirelingClass(270));
    assert(IsProjectedLethal(256, -256));
    assert(IsProjectedLethal(1, -2));
    assert(!IsProjectedLethal(256, -255));
    assert(!IsProjectedLethal(0, 0));
    assert(!IsProjectedLethal(1, 1));

    const auto missing = ParseConfig(nlohmann::json::object());
    assert(!missing.enabled);
    const auto enabled = ParseConfig(nlohmann::json::parse(
        R"json({"preventMercDeathInTown":{"enabled":true}})json"));
    assert(enabled.enabled);
    assert(Throws([] {
        ParseConfig(nlohmann::json::parse(
            R"json({"preventMercDeathInTown":{"enabled":true,"unknown":1}})json"));
    }));
    assert(Throws([] {
        ParseConfig(nlohmann::json::parse(
            R"json({"preventMercDeathInTown":{"enabled":1}})json"));
    }));

    assert(argc == 2);
    std::ifstream shippedConfig(argv[1]);
    assert(shippedConfig.is_open());
    const auto root = nlohmann::json::parse(shippedConfig, nullptr, true, true);
    const auto shipped = ParseConfig(root.at("misc"));
    assert(!shipped.enabled);
}
