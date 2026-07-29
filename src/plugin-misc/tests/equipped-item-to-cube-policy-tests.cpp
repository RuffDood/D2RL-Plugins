#include "equipped-item-to-cube-policy.h"

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
    using namespace RuffnecKk::EquippedItemToCube;

    const auto missing = ParseConfig(nlohmann::json::object());
    assert(!missing.enabled);
    const auto enabled = ParseConfig(nlohmann::json::parse(
        R"json({"equippedItemToCube":{"enabled":true}})json"
    ));
    assert(enabled.enabled);
    assert(Throws([] {
        ParseConfig(nlohmann::json::parse(
            R"json({"equippedItemToCube":true})json"
        ));
    }));
    assert(Throws([] {
        ParseConfig(nlohmann::json::parse(
            R"json({"equippedItemToCube":{"enabled":1}})json"
        ));
    }));
    assert(Throws([] {
        ParseConfig(nlohmann::json::parse(
            R"json({"equippedItemToCube":{"enabled":true,"modifier":"ctrl"}})json"
        ));
    }));

    ItemTransferPacket inventoryPacket{};
    inventoryPacket[0] = InventoryTransferOpcode;
    WriteU32(inventoryPacket, 1, 37);
    WriteU32(inventoryPacket, 13, CubeInventoryPage);
    WriteU32(inventoryPacket, 17, 5u | (3u << 16));

    static_assert(IsEquippedBodyLocation(1));
    static_assert(IsEquippedBodyLocation(10));
    static_assert(!IsEquippedBodyLocation(0));
    static_assert(!IsEquippedBodyLocation(11));
    assert(ShouldRewriteCubeTransfer(true, inventoryPacket, 4));
    assert(!ShouldRewriteCubeTransfer(false, inventoryPacket, 4));
    assert(!ShouldRewriteCubeTransfer(true, inventoryPacket, 0));
    assert(!ShouldRewriteCubeTransfer(true, inventoryPacket, BodyLocationCount));

    auto wrongOpcode = inventoryPacket;
    wrongOpcode[0] = 0x55;
    assert(!ShouldRewriteCubeTransfer(true, wrongOpcode, 4));
    auto wrongPage = inventoryPacket;
    WriteU32(wrongPage, 13, 0);
    assert(!ShouldRewriteCubeTransfer(true, wrongPage, 4));

    const auto equippedPacket = RewriteAsEquippedTransfer(inventoryPacket, 4);
    assert(equippedPacket[0] == EquippedTransferOpcode);
    assert(ReadU32(equippedPacket, 1) == 37);
    assert(ReadU32(equippedPacket, 5) == SelfTargetGuid);
    assert(ReadU32(equippedPacket, 9) == 4);
    assert(ReadU32(equippedPacket, 13) == CubeInventoryPage);
    assert(ReadU32(equippedPacket, 17) == (5u | (3u << 16)));

    assert(argc == 2);
    std::ifstream shippedConfig(argv[1]);
    assert(shippedConfig.is_open());
    const auto root = nlohmann::json::parse(shippedConfig, nullptr, true, true);
    const auto shipped = ParseConfig(root.at("misc"));
    assert(shipped.enabled);
    return 0;
}
