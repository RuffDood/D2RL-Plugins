#include "transmute-hotkey-policy.h"

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
    using namespace RuffnecKk::TransmuteHotkey;

    Hotkey hotkey{};
    assert(ParseHotkey("CTRL+SHIFT+T", hotkey));
    assert(hotkey.virtualKey == 'T');
    assert(hotkey.device == InputDevice::Keyboard);
    assert(hotkey.control && hotkey.shift && !hotkey.alt);
    assert(ExactModifiersMatch(hotkey, true, true, false));
    assert(!ExactModifiersMatch(hotkey, true, true, true));
    assert(ParseHotkey("MOUSE4", hotkey));
    assert(hotkey.virtualKey == 0x05 && IsMouseHotkey(hotkey));
    assert(ParseHotkey("ctrl + mouse 5", hotkey));
    assert(hotkey.virtualKey == 0x06 && hotkey.control);
    assert(ParseHotkey("T", hotkey));
    assert(hotkey.virtualKey == 'T' && !hotkey.control && !hotkey.shift && !hotkey.alt);
    assert(ParseHotkey("SHIFT+T", hotkey));
    assert(hotkey.virtualKey == 'T' && hotkey.shift);
    assert(!ParseHotkey("CTRL+CTRL+T", hotkey));
    assert(!ParseHotkey("T+H", hotkey));
    assert(!ParseHotkey("F25", hotkey));

    assert(IsFreshRequest(1'100, 1'000, 250));
    assert(!IsFreshRequest(1'251, 1'000, 250));

    const auto missing = ParseConfig(nlohmann::json::object());
    assert(!missing.enabled);
    assert(missing.hotkeyText == "CTRL+SHIFT+T");
    assert(missing.consume);
    const auto enabled = ParseConfig(nlohmann::json::parse(
        R"json({"transmuteHotkey":{"enabled":true,"hotkey":"MOUSE4","consume":false}})json"));
    assert(enabled.enabled && IsMouseHotkey(enabled.hotkey));
    assert(!enabled.consume);
    assert(Throws([] {
        ParseConfig(nlohmann::json::parse(
            R"json({"transmuteHotkey":{"enabled":true,"unknown":1}})json"));
    }));
    const auto singleKey = ParseConfig(nlohmann::json::parse(
        R"json({"transmuteHotkey":{"enabled":true,"hotkey":"T"}})json"));
    assert(singleKey.enabled && singleKey.hotkey.virtualKey == 'T');

    assert(argc == 2);
    std::ifstream shippedConfig(argv[1]);
    assert(shippedConfig.is_open());
    const auto root = nlohmann::json::parse(shippedConfig, nullptr, true, true);
    const auto shipped = ParseConfig(root.at("misc"));
    assert(!shipped.enabled);
    assert(shipped.hotkeyText == "CTRL+SHIFT+T");
    assert(shipped.consume);
    return 0;
}
