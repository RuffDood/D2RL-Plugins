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

The shipped JSON preserves vanilla behavior for every configurable feature. `ExtendedItemStats` is internal infrastructure with no public key: normal vanilla items are unchanged, while the pack safely supports item payloads up to 4096 bytes and windows only oversized tooltips.

## Plugins

| DLL | JSON section | Description |
|---|---|---|
| `plugin-items.dll` | `items` | Item rules, fixes, limits, vendor options, 4096-byte item transport, and scrollable oversized tooltips |
| `plugin-levels.dll` | `levels` | Level and area tweaks |
| `plugin-misc.dll` | `misc` | Miscellaneous tweaks such as the `/players` command limit |
| `plugin-quests.dll` | `quests` | Quest reward overrides |
| `plugin-skills.dll` | `skills` | Skill-system extensions and bulk skill-point allocation |

## Configuration

`D2RPlugins.json` contains every public option with documented, vanilla-preserving defaults. Change only the feature blocks you want to enable.

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

MIT
