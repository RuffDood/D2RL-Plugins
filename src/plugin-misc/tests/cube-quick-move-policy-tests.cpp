#include "cube-quick-move-policy.h"

#include <array>
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
    using namespace RuffnecKk::CubeQuickMove;

    static_assert(ShouldRecomputeBottomRight(true, 1, 3, 1, 2));
    static_assert(ShouldRecomputeBottomRight(true, 1, 3, 2, 2));
    static_assert(ShouldRecomputeBottomRight(true, 1, 3, 2, 3));
    static_assert(!ShouldRecomputeBottomRight(false, 1, 3, 2, 2));
    static_assert(!ShouldRecomputeBottomRight(true, 0, 3, 2, 2));
    static_assert(!ShouldRecomputeBottomRight(true, 1, 0, 2, 2));
    static_assert(!ShouldRecomputeBottomRight(true, 1, 3, 1, 1));
    static_assert(!ShouldRecomputeBottomRight(true, 1, 3, 2, 1));
    static_assert(!ShouldRecomputeBottomRight(true, 1, 3, 0, 2));

    const auto missing = ParseConfig(nlohmann::json::object());
    assert(!missing.enabled);
    const auto enabled = ParseConfig(nlohmann::json::parse(
        R"json({"cubeQuickMoveBottomRight":{"enabled":true}})json"
    ));
    assert(enabled.enabled);
    assert(Throws([] {
        ParseConfig(nlohmann::json::parse(
            R"json({"cubeQuickMoveBottomRight":true})json"
        ));
    }));
    assert(Throws([] {
        ParseConfig(nlohmann::json::parse(
            R"json({"cubeQuickMoveBottomRight":{"enabled":1}})json"
        ));
    }));
    assert(Throws([] {
        ParseConfig(nlohmann::json::parse(
            R"json({"cubeQuickMoveBottomRight":{"enabled":true,"mode":"all"}})json"
        ));
    }));

    std::array<std::uintptr_t, 12> cells{};
    std::int32_t x{-1};
    std::int32_t y{-1};
    assert(TryFindBottomRight(cells.data(), 3, 4, 2, 2, &x, &y));
    assert(x == 1 && y == 2);
    cells[2 + 3 * 3] = 1;
    assert(TryFindBottomRight(cells.data(), 3, 4, 2, 2, &x, &y));
    assert(x == 1 && y == 1);
    cells.fill(1);
    assert(!TryFindBottomRight(cells.data(), 3, 4, 2, 2, &x, &y));
    assert(!TryFindBottomRight(nullptr, 3, 4, 2, 2, &x, &y));

    assert(argc == 2);
    std::ifstream shippedConfig(argv[1]);
    assert(shippedConfig.is_open());
    const auto root = nlohmann::json::parse(shippedConfig, nullptr, true, true);
    const auto shipped = ParseConfig(root.at("misc"));
    assert(!shipped.enabled);
    return 0;
}
