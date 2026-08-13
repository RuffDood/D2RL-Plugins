#include "remote-stash-policy.h"

#include "../../../tests/test-check.h"

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

int main() {
    using namespace ruffneckk::remote_stash;

    const auto defaults = ParseHotkeyConfig(nlohmann::json::object());
    TEST_REQUIRE(!defaults.enabled);
    TEST_REQUIRE(defaults.hotkeyText == "None");
    TEST_REQUIRE(defaults.hotkey.virtualKey == 0);

    const auto enabled = ParseHotkeyConfig(nlohmann::json::parse(
        R"json({"enabled":true,"hotkey":"SHIFT+S"})json"));
    TEST_REQUIRE(enabled.enabled);
    TEST_REQUIRE(enabled.hotkey.virtualKey == 'S');
    TEST_REQUIRE(enabled.hotkey.shift);
    TEST_REQUIRE(!enabled.hotkey.control);

    const auto legacy = ParseHotkeyConfig(nlohmann::json::parse(
        R"json({"enabled":false,"hotkey":"None","consume":true})json"));
    TEST_REQUIRE(!legacy.enabled);
    TEST_REQUIRE(legacy.hotkeyText == "None");

    TEST_REQUIRE(Throws([] {
        ParseHotkeyConfig(nlohmann::json::parse(
            R"json({"enabled":true,"hotkey":"None"})json"));
    }));
    TEST_REQUIRE(Throws([] {
        ParseHotkeyConfig(nlohmann::json::parse(
            R"json({"enabled":true,"hotkey":"S","unknown":1})json"));
    }));
    TEST_REQUIRE(Throws([] {
        ParseHotkeyConfig(nlohmann::json::parse(
            R"json({"enabled":"yes","hotkey":"S"})json"));
    }));
    return 0;
}
