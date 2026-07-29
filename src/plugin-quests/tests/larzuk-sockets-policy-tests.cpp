#include "larzuk-sockets-policy.h"

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
    using namespace RuffnecKk::ForceLarzukSockets;

    static_assert(QualityIndex(4) == 0);
    static_assert(QualityIndex(6) == 1);
    static_assert(QualityIndex(5) == 2);
    static_assert(QualityIndex(7) == 3);
    static_assert(QualityIndex(8) == 4);
    static_assert(!QualityIndex(2));
    static_assert(!QualityIndex(9));
    static_assert(IsValidRule({1, 1}));
    static_assert(IsValidRule({2, 6}));
    static_assert(!IsValidRule({0, 1}));
    static_assert(!IsValidRule({3, 2}));
    static_assert(!IsValidRule({1, 7}));
    static_assert(EffectiveLegalMaximum(6, 1, 1) == 1);
    static_assert(EffectiveLegalMaximum(6, 1, 2) == 2);
    static_assert(EffectiveLegalMaximum(4, 2, 3) == 4);
    static_assert(EffectiveLegalMaximum(6, 0, 2) == 0);
    static_assert(ResolveSockets({2, 2}, 6, 0) == 2);
    static_assert(ResolveSockets({4, 6}, 2, 123) == 2);
    static_assert(ResolveSockets({1, 3}, 6, 0) == 1);
    static_assert(ResolveSockets({1, 3}, 6, 1) == 2);
    static_assert(ResolveSockets({1, 3}, 6, 2) == 3);
    static_assert(ResolveSockets({1, 4}, 6, 7) == 4);

    const auto missing = ParseConfig(nlohmann::json::object());
    assert(!missing.enabled);
    assert(!HasRules(missing.rules));

    const auto directVanilla = ParseConfig(nlohmann::json::parse(
        R"json({"larzukSockets":{"enabled":true,"normal":{"magic":null}}})json"
    ));
    assert(directVanilla.enabled);
    assert(!HasRules(directVanilla.rules));

    assert(Throws([] {
        ParseConfig(nlohmann::json::parse(R"json({"larzukSockets":true})json"));
    }));
    assert(Throws([] {
        ParseConfig(nlohmann::json::parse(
            R"json({"larzukSockets":{"normal":{"magic":null}}})json"
        ));
    }));
    assert(Throws([] {
        ParseConfig(nlohmann::json::parse(
            R"json({"larzukSockets":{"enabled":false,"diagnostics":false}})json"
        ));
    }));
    assert(Throws([] {
        ParseConfig(nlohmann::json::parse(
            R"json({"larzukSockets":{"normal":{"magic":{"minSockets":0,"maxSockets":2}}}})json"
        ));
    }));
    assert(Throws([] {
        ParseConfig(nlohmann::json::parse(
            R"json({"larzukSockets":{"normal":{"magic":{"minSockets":1,"maxSockets":2,"roll":"fixed"}}}})json"
        ));
    }));

    assert(argc == 2);
    std::ifstream shippedConfig(argv[1]);
    assert(shippedConfig.is_open());
    const auto root = nlohmann::json::parse(shippedConfig, nullptr, true, true);
    const auto shipped = ParseConfig(root.at("quests"));
    assert(!shipped.enabled);
    assert(HasRules(shipped.rules));
    for (std::size_t difficulty = 0; difficulty < DifficultyCount; ++difficulty) {
        const auto* magic = FindRule(shipped.rules, static_cast<std::uint8_t>(difficulty), 4);
        assert(magic && magic->has_value());
        assert((*magic)->minSockets == 1 && (*magic)->maxSockets == 2);
        for (const auto quality : {6, 5, 7, 8}) {
            const auto* rule = FindRule(
                shipped.rules,
                static_cast<std::uint8_t>(difficulty),
                quality
            );
            assert(rule && rule->has_value());
            assert((*rule)->minSockets == 1 && (*rule)->maxSockets == 1);
        }
    }
    return 0;
}
