# CLAUDE.md

Guidance for Claude Code when working in this repository.

## Project overview

Take All Lockpicks VR is an SKSE plugin DLL for **Skyrim VR** built on CommonLibSSE-NG (multi-runtime: SE/AE/VR, but the hooks only install when the VR runtime is detected). It makes single-press lockpick looting take the whole stack, matching Skyrim SE/AE behavior.

Source layout: `src/Main.cpp` (plugin entry, installs hooks on `kDataLoaded`), `src/Hooks.cpp` + `include/Hooks.h` (all mod logic), `src/Logging.cpp` (spdlog file logger, info level), `include/PCH.h` (precompiled header, force-included everywhere).

## Building

Requires MSVC (VS 2022+), CMake 3.29+, vcpkg (`VCPKG_ROOT` env var), Ninja. Build from a VS developer environment:

```
cmake --preset build-release-msvc   # or build-debug-msvc
cmake --build build/release-msvc    # or build/debug-msvc
```

Output DLL+PDB are copied to `contrib/PluginRelease/skse/plugins/` (Release) or `contrib/PluginDebug/...` (Debug). CommonLibSSE-NG is a git submodule at `extern/CommonLibSSE-NG` — clone with `--recurse-submodules`.

**IntelliSense shows false errors** (`name followed by '::' must be a class or namespace name`, `std has no member int32_t`) in `src/` and `include/` because it doesn't model the force-included PCH. Trust the real build, not the IDE diagnostics.

## Critical engine knowledge (empirically verified in-game on Skyrim VR 1.4.15)

These facts cost multiple crash-log debugging cycles. Do not "simplify" them away without in-game re-verification:

1. **Vtable indices in CommonLibSSE headers are HEX.** `RemoveItem` is slot `0x56`, not 56 decimal. VR shares SE's `TESObjectREFR` vtable indices below 0x84 (VR divergence starts at `AttachWeapon`, see `RelocateVirtual(0x84, 0x85, ...)` calls in NG's TESObjectREFR.cpp).
2. **Two vtables must be patched.** `Actor` overrides `RemoveItem`, so patching `TESObjectREFR`'s vtable alone never affects corpses. The plugin patches `TESObjectREFR` (containers) and `Character` (NPCs/corpses), each with its own saved original.
3. **VR loot reason codes differ from SE expectations.** Looting a body or container → `kStoreInContainer` (4). Follower trade → `kStoreInTeammate` (5). Merchant barter → `kSelling` (2). Vanilla-style `kRemove`/`kSteal` are NOT used by the VR container menu. Gold arrives with the full stack count under reason 4; lockpicks arrive with count=1 per press; quantity-prompt confirmations arrive with the chosen count.
4. **CRASH TRAP: the removal source ref is a proxy.** The VR container menu passes a proxy object as `a_this` (FormID `0xFFFFFFFE`; barter uses `0xFFFFFFFF`) whose memory layout does NOT match CommonLibSSE-NG's VR model. Confirmed crashes: NG's `GetInventoryCounts`/`GetInventoryChanges` → `ExtraDataList::HasType` (read a lock-like value `2` where a presence pointer was expected), and even a raw read of the extra-list head at `extraList` offset +0 (got packed-int garbage). **Never dereference into that proxy — no extraList access, no inventory helpers, no NG reimplementations. Only forward it to the saved original function.** Native engine calls handle it fine.
5. **The take-all mechanism relies on engine clamping.** When count==1 + lockpick + reason 4/5 + destination is the player + source is not the player, the hook re-issues the same call with count=30000 (within int16 range) and a null `ExtraDataList`. The engine clamps removal to what the container holds — the same guarantee behind the Papyrus `RemoveItem(item, 999999)` idiom, which bottoms out in this same virtual. Exactly one engine call; no counting, no sweep, no duplication risk.
6. **The `count == 1` gate is deliberate and final.** It preserves the vanilla quantity prompt on larger stacks (user-confirmed desired behavior, matching SE/AE) and keeps merchant purchases exact. Do not auto-promote prompted counts and do not try to suppress the quantity prompt.

## Releasing

Publish **only the Release DLL** (`contrib/PluginRelease/skse/plugins/TakeAllLockpicksVR.dll`) and **never distribute PDB files**. The build trims local filesystem paths out of the binaries (`/d1trimfile` + `/PDBALTPATH` in CMakeLists.txt) and the Release DLL is verified free of embedded local paths — but the Debug DLL still contains absolute build-machine paths inside assert strings from the vcpkg-prebuilt spdlog debug library, and PDBs always contain full source paths. Keep `contrib/`, `build/`, and `*.log` gitignored: SKSE/crash logs and build outputs embed local paths.

## Testing

There is no automated test; verification requires launching Skyrim VR with the DLL installed. The plugin logs to `Documents\My Games\Skyrim VR\SKSE\TakeAllLockpicksVR.log` (install lines only in release; per-take diagnostic logging was removed before v1.0.0 — recover it from git history if debugging is needed, it prints item/count/reason/extraList/source/destination for every relevant `RemoveItem`). When testing changes, exercise: corpse loot (multi and single pick), chest loot, follower take, merchant purchase (must stay exact), giving picks to a follower (must stay exact), and the quantity prompt (choice must be respected; the sole known quirk: explicitly choosing 1 still takes the whole stack — documented on Nexus, accepted behavior).

Useful references for crash analysis: CrashLoggerSSE output names the faulting module and NG source lines; the VR Address Library database (`version-1-4-15-0.csv`, gitignored, may be present locally in the repo root) maps SE address-library IDs to VR offsets.
