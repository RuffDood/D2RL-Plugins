# RuffnecKk integration notes

This branch integrates 16 independently configurable RuffnecKk features into
the five existing eezstreet PluginPack DLLs. It does not add a sixth runtime
DLL, change the PluginPack installation layout, or replace the single
`D2RPlugins.json` configuration file.

## Inventory

| Owner | Feature | Configuration |
|---|---|---|
| `plugin-items.dll` | Gamble Screen Limit | `items.gambleScreenLimit` |
| `plugin-items.dll` | Ground Item Label Limit | `items.groundItemLabels` |
| `plugin-items.dll` | Item Durability | `items.itemDurability` |
| `plugin-items.dll` | Charm Aura Trigger Fix | `items.charmAuraTriggerFix` |
| `plugin-items.dll` | Enhanced Damage Min/Max Fix | `items.enhancedDamageMinMaxFix` |
| `plugin-items.dll` | unified EthItemRules | `items.etherealItemRules` |
| `plugin-items.dll` | Extended Item Stats | `items.extendedItemStats` |
| `plugin-items.dll` | Repair Costs Cap | `items.repairCostsCap` |
| `plugin-items.dll` | Qty Display Fix | `items.qtyDisplayIssue` |
| `plugin-misc.dll` | Cube Quick Move Bottom-Right | `misc.cubeQuickMoveBottomRight` |
| `plugin-misc.dll` | Equipped Item to Cube | `misc.equippedItemToCube` |
| `plugin-misc.dll` | Assign Transmute Hotkey | `misc.transmuteHotkey` |
| `plugin-items.dll` | Vendor Stock Refresh | `items.vendorStockRefresh` |
| `plugin-misc.dll` | Prevent Merc Death in Town | `misc.preventMercDeathInTown` |
| `plugin-quests.dll` | Force Larzuk Sockets | `quests.larzukSockets` |
| `plugin-skills.dll` | Bulk Skill Point Allocation | `skills.bulkSkillPointAllocation` |

`NoEtherealItemTypes` and the former Ethereal Item Rules implementation are one
feature and one JSON block here.

## Default behavior

The shipped `D2RPlugins.json` is the player-facing default. Charm Aura Trigger
Fix, Enhanced Damage Min/Max Fix, Qty Display Fix, Equipped Item to Cube, and
Extended Item Stats are enabled by default. Every other newly added configurable
effect remains disabled, and its remaining values match vanilla where a vanilla value exists.
The Larzuk table contains the 15 visible vanilla socket rules but its independent
switch is disabled, so it installs no hook. Extended Item Stats supplies bounded
4096-byte item transport and only changes tooltip presentation when an oversized
payload actually requires it; its public switch can disable the entire feature.

## Internal hook safety

`hook-manifest.json` contains 132 uniquely owned write sites. Configuration and
every build fail if two owners overlap. Shared call paths use one owner and
call-through consumers:

- `0x373890` belongs to EthItemRules; Enhanced Damage and Charm Aura call its
  live entry instead of installing another hook.
- `0x2F48C0` belongs to Item Durability when maximum-durability behavior needs
  it. Prevent Merc Death in Town validates the untouched body at `+5` and calls
  the live entry, so both features load together.
- `0x2A7810` belongs to Extended Item Stats; Equipped Item to Cube calls through
  that resolver.
- Item-record and tooltip consumers use shared owner-and-consumer pipelines.
  Optional features therefore compose without requiring a player-selected load
  order or compatibility setting.

The final D2R `3.2.92777` validation built all five Release DLLs from one commit,
passed 20/20 CTest tests, and completed vanilla-default and jointly enabled cold
starts at 24/24 startup stages. In the active run, all 16 integrated features
reached their intended active paths. D2RLoader
reported `scanned=16 active=14 disabled=2
rejected=0 failed=0`; the two disabled entries were lower-priority global
duplicates of mod-local plugins. No fresh crash was produced.

## Player test plan

The automated gates prove compilation, signatures, hook ownership, load order,
configuration parsing, cold start, and the absence of loader rejection. They do
not replace visible gameplay tests. A player does not need to test every feature
in a separate game launch; use these focused batches:

1. **Item UI and limits:** Gamble Screen Limit, Ground Item Label Limit, Qty
   Display Fix, and Extended Item Stats tooltips.
2. **Item rules and durability:** EthItemRules, Item Durability, Repair Costs
   Cap, Enhanced Damage Min/Max Fix, and Charm Aura Trigger Fix. Include repair
   individual/Repair All, transition/corpse recovery, save/reload, and one
   host/joiner session.
3. **Cube workflow:** Cube Quick Move, Equipped Item to Cube, and Transmute
   Hotkey with open, absent, and full Cube cases plus keyboard and mouse input.
4. **Town services:** Vendor Stock Refresh and Prevent Merc Death in Town,
   including vendor reopen, poison/Open Wounds, town exit, portal, and waypoint.
5. **Quest and skills:** Larzuk across quality/difficulty boundaries and Bulk
   Skill Point Allocation with Ctrl, Shift, insufficient points, and optional
   confirmation.

Keep a vanilla-default launch as the control, then enable only the batch being
tested. Reuse the already validated standalone behavior as the comparison
oracle when an integrated result is uncertain.

## Upstream review

This branch deliberately follows eezstreet's existing structure: five DLLs,
the original plugin IDs and metadata, one JSON file, internal feature modules,
canonical shared ABI types, and a build-time ownership manifest. RuffnecKk
credit is attached to the contributed feature sources and logs without
rebranding the eezstreet DLLs. Upstream can review or merge the work feature by
feature even though the branch also supports one complete build.
