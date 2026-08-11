# Take All Lockpicks VR

An SKSE plugin for **Skyrim VR** that makes looting lockpicks work the way it does in Skyrim Special Edition and Anniversary Edition: taking lockpicks from a body or container takes the whole stack at once, like gold, instead of one pick at a time.

## The problem

In Skyrim SE/AE, activating a stack of lockpicks in a container or on a corpse transfers the entire stack in one action. In Skyrim VR, the same action transfers **one lockpick per press**, so looting a bandit carrying eight picks means eight identical button presses. This plugin restores the SE/AE behavior in VR.

## What it does

- Taking lockpicks from a **corpse**, **container**, or a **follower** with a single press takes the whole stack.
- The vanilla **quantity prompt** for larger stacks is preserved — if the menu asks how many you want and you choose a number, your choice is respected exactly.
- **Merchants are unaffected**: buying a lockpick gives you exactly the quantity you paid for.
- **Giving or storing** lockpicks (player → follower, player → container) is unaffected.
- No configuration, no INI, no MCM. If you don't want the behavior, remove the plugin.
- Does nothing at all when loaded outside Skyrim VR (the hooks are only installed when the VR runtime is detected).

## Requirements

- [Skyrim VR](https://store.steampowered.com/app/611670/The_Elder_Scrolls_V_Skyrim_VR/) (1.4.15)
- [SKSEVR](https://skse.silverlock.org/)
- [VR Address Library for SKSEVR](https://www.nexusmods.com/skyrimspecialedition/mods/58101)

## Installation

Install with your mod manager of choice, or manually copy `TakeAllLockpicksVR.dll` into `Data\SKSE\Plugins\`.

To verify it loaded, check the log at
`Documents\My Games\Skyrim VR\SKSE\TakeAllLockpicksVR.log` — it should contain:

```
Installed TESObjectREFR::RemoveItem hook (containers)
Installed Character::RemoveItem hook (bodies)
```

## How it works (technical)

The plugin patches the `RemoveItem` virtual function (vtable slot `0x56`) in the vtables of both `TESObjectREFR` (world containers) and `Character` (NPCs and corpses — `Actor` overrides `RemoveItem`, so each class must be patched in its own vtable).

Empirically, Skyrim VR's container menu calls `RemoveItem` with:

| Action | `ITEM_REMOVE_REASON` | Count passed |
| --- | --- | --- |
| Looting a body/container | `kStoreInContainer` (4) | 1 per press for lockpicks; full stack for gold |
| Taking from a follower | `kStoreInTeammate` (5) | 1, or the amount chosen in the quantity prompt |
| Buying from a merchant | `kSelling` (2) | the purchased amount |

When a call is a **single lockpick** (`count == 1`) moving **to the player** with reason 4 or 5, the plugin re-issues the same call with a large count and no extra-data list. The engine clamps removal to what the container actually holds — the same behavior the classic Papyrus `RemoveItem(item, 999999)` "remove all" idiom relies on — so the entire stack transfers in one operation, with no possibility of duplication. Calls with an explicit count (quantity prompt), other reasons (barter), or other directions (giving/storing) pass through untouched.

One notable Skyrim VR quirk discovered during development: the object reference the VR container menu passes as the removal source is a **proxy** (FormID `0xFFFFFFFE`) whose memory layout does not match what CommonLibSSE-NG models for VR. Reading its extra data or inventory through library helpers crashes; the plugin therefore never inspects that object and only forwards it to the original engine function.

## Building from source

Prerequisites: Visual Studio 2022+ (C++ workload), CMake 3.29+, [vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` set.

```
git clone --recurse-submodules <this repo>
cmake --preset build-release-msvc
cmake --build build/release-msvc
```

Run from a Visual Studio developer prompt (or any shell where `cl.exe` is available). The built DLL and PDB are copied to `contrib/PluginRelease/skse/plugins/` (or `PluginDebug` for the debug preset), ready to be zipped as a mod archive.

The project is built on [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/ng) (vendored as a git submodule under `extern/`). For reverse-engineering work, the VR Address Library database for Skyrim VR 1.4.15 (`version-1-4-15-0.csv`, available from the [VR Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/58101) project) is a useful companion.

GitHub Actions CI builds both presets on every push to `main`.

## Compatibility

- Skyrim VR only; safely inert in SE/AE.
- Patches two vtable slots rather than writing code hooks, so it composes with mods that hook the container menu itself (tested alongside a load order including HIGGS, VRIK, PLANCK, hdtSMP, Community Shaders, and others).
- Mods that also replace `TESObjectREFR::RemoveItem` or `Character::RemoveItem` vtable entries will chain with this plugin in load order; last one loaded wins the slot but each calls the previous, which is the standard SKSE convention.

## License

[MIT](LICENSE)

## Credits

- [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/ng) by the CommonLibSSE / CommonLibVR contributors
- [SKSE](https://skse.silverlock.org/) team
- [VR Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/58101) by alandtse
