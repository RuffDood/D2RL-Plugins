# D2RL-Plugins

A collection of gameplay plugins for Diablo II: Resurrected, built for [D2RLoader](https://discord.gg/fv9mchnAVn). The five DLLs hook the game without replacing game files.

## Requirements

- D2RLoader 1.0.1 or later.
- MSVC 2022 and CMake 3.24 or later to build from source.

## Building

Configure the root `CMakeLists.txt` and build with the MSVC x64 toolchain. CMake fetches the pinned ImGui and MinHook dependencies used by `plugin-items.dll`.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The Release build produces these runtime DLLs under `build/bin/Release/`:

- `plugin-items.dll`
- `plugin-levels.dll`
- `plugin-misc.dll`
- `plugin-quests.dll`
- `plugin-skills.dll`

`plugin-shared` is linked statically and is not a sixth runtime DLL.

### Hook ownership gate

`hook-manifest.json` is the source of truth for every executable memory write made by the five DLLs. Each entry declares one owner, feature, write kind, RVA, guarded byte span, expected bytes, and source file for D2R `3.2.92777`.

CMake validates the manifest during configuration and before every build. A duplicate ID, unknown owner, malformed byte guard, or overlap between write spans stops the build. Any change that adds, removes, moves, or resizes a hook or patch must update the manifest in the same commit.

`plugin-shared` owns the canonical minimal `D2UnitStrc` accessors. Feature modules must not duplicate incompatible unit layouts.

## Installation

1. Copy the five DLLs to either `<D2R>/d2rloader/plugins/` or `<D2R>/mods/<mod>/d2rloader/plugins/`.
2. Copy `D2RPlugins.json` to the active mod data directory as `<modDirectory>/D2RPlugins.json`, or beside `D2RLoader.exe` for the global fallback.

The shipped JSON enables four selected configurable features by default: Charm Aura Trigger Fix, Enhanced Damage Min/Max Fix, Qty Display Fix, and Equipped Item to Cube. Other newly added configurable features remain disabled. Extended Item Stats is an always-active `plugin-items.dll` patch with complete scrollable stat lists, oversized-item transport, and a visible graphical scroll bar; it has no public configuration key.

See [COMMUNITY-INTEGRATION.md](COMMUNITY-INTEGRATION.md) for the complete
feature inventory, internal safety design, validation results, and recommended
player test batches.

## Plugins

| DLL | JSON section | Description |
|---|---|---|
| `plugin-items.dll` | `items` | Item rules, fixes, limits, vendor options, and scrollable full-stat tooltips |
| `plugin-levels.dll` | `levels` | Level and area tweaks |
| `plugin-misc.dll` | `misc` | Miscellaneous tweaks, including Cube actions and town safety |
| `plugin-quests.dll` | `quests` | Quest reward overrides, including configurable Larzuk socket counts |
| `plugin-skills.dll` | `skills` | Skill-system extensions and bulk skill-point allocation |

## Configuration

`D2RPlugins.json` contains every public option with documented defaults. Charm Aura Trigger Fix, Enhanced Damage Min/Max Fix, Qty Display Fix, and Equipped Item to Cube are enabled in the shipped player configuration; other new configurable features remain disabled or preserve vanilla behavior. Extended Item Stats is not configurable: `plugin-items.dll` always installs its complete tooltip, oversized-item transport, scrolling input, and graphical scroll bar paths. Magic Find Formula is available as `items.magicFindFormula`; `vanilla` preserves the native curve and `linear` removes only the positive Unique, Set, and Rare diminishing returns. Transmute Hotkey is available as `misc.transmuteHotkey`; enabling it triggers the visible native Transmute action from the configured keyboard chord or mouse button. Single keys and combinations are accepted; with `consume=true`, a successfully captured shortcut does not also reach the game, while the key keeps its normal behavior outside the Cube. Vendor Stock Refresh is available as `items.vendorStockRefresh`; enabling it exposes and dynamically positions the native refresh button in normal vendor panels. Prevent Merc Death in Town is available as `misc.preventMercDeathInTown`; enabling it suppresses only projected-lethal persistent-damage ticks against mercenaries currently in town.

If the mod-local JSON is missing, the pack uses the global JSON beside
`D2RLoader.exe`. If a configuration file exists but is malformed or contains an
invalid high-risk value, the affected DLL refuses to load and writes the exact
reason to its log instead of silently applying a different configuration.

### Migrating the RuffnecKk ethereal memory patch

Remove RuffnecKk's legacy `ethereal-item-rules.json` memory patch when upgrading
to this PluginPack. Its ethereal generation rate and set-item behavior are now
owned by `plugin-items.dll` under `items.etherealItemRules`. The integrated
`chancePercent` accepts a normal decimal percentage from 0 through 100 instead
of a hexadecimal byte value, while `allowSetItems`, `allowIndestructibleItems`,
and `excludedItemTypes` expose the remaining rules in the same JSON block.

Do not enable the legacy memory patch and the integrated feature together: they
target the same executable write sites and would create duplicate ownership.

```jsonc
{
  "items": {
    "gambleScreenLimit": {
      "enabled": true
    }
  },
  "skills": {
    "bulkSkillPointAllocation": {
      "enabled": true,
      "skillPointsPerCtrlClick": 5
    }
  }
}
```

## License

The PluginPack is available under the [MIT License](LICENSE).
`plugin-items.dll` also includes third-party components listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
