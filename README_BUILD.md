# OpenCBP Physics for Fallout 4 - Build & Maintenance Notes

*A caco-bot production. Still writing READMEs instead of Discord bots. Still fucking wonderful.*

## What This Is

OpenCBP Physics for Fallout 4: soft-body ("jiggle") bone physics as an F4SE plugin.
Spring-damper on skeleton nodes, no Havok, no collision, purely cosmetic. Tuned entirely
through `Data\F4SE\Plugins\ocbp.ini`.

## Target

| Thing | Version | Where it's pinned |
|---|---|---|
| Fallout 4 | **1.11.240.0** (Anniversary, "creation club" update) | `CBPSSE/main.cpp` `OCBP_TARGET_RUNTIME`, `CBPSSE.vcxproj` `RUNTIME_VERSION=0x010B0F00` |
| F4SE | **0.7.9** | vendored source in `f4se/` (synced from ianpatt/f4se master) |
| Address Library for F4SE Plugins | 1.11.240 database (`version-1-11-240-0.bin`) | read at runtime by `CBPSSE/AddressLibrary.cpp` |

This build is **1.11.240 only** and says so: `F4SEPlugin_Load` refuses any other runtime,
`F4SEPlugin_Version.compatibleVersions` lists exactly one version, and it does not claim
address/structure independence (the F4SE headers it calls are version-locked, so claiming
otherwise would be a lie F4SE can't catch).

## Build Requirements

- **Visual Studio 2022 or 2026** (v143 or v145 toolset, C++17). Older toolsets: on your own head be it.
- **Windows 10/11 SDK**.
- Nothing else. F4SE, common, xbyak, DetourXS are all vendored. **No C++ AMP** - MSVC removed it; the old `#include "amp.h"` is gone.

## Solution Layout

```
OpenCBP_FO4.sln
  CBPSSE\CBPSSE.vcxproj        -> cbp.dll           (the plugin; project name OpenCBP_FO4)
  f4se\f4se\f4se.vcxproj       -> f4se_1_11_240.lib (F4SE game headers, static lib)
  f4se\f4se_common\...         -> f4se_common.lib
  common\common_vc14.vcxproj   -> common_vc14.lib   (ianpatt/common)
detourxs-master\               inline hooking (LDE64x64.lib)
```

The three `*.vcxproj` files under `f4se\` are **ours** (static lib, v143, C++17, include
paths for this sln). When syncing F4SE source, copy `.h/.cpp/.rc/.txt` from upstream and
**leave the vcxproj files alone**.

## Build

Verified 2026-09-09 with **VS 2026 Build Tools 18.9 (MSVC 14.51, toolset v145, SDK 10.0.26100)**.
The vcxproj files say v143 / SDK 10.0.18362; override both on the command line and it builds clean:

```
MSBuild.exe OpenCBP_FO4.sln -m -p:Configuration=Release -p:Platform=x64 ^
  -p:PlatformToolset=v145 -p:WindowsTargetPlatformVersion=10.0.26100.0 -p:PostBuildEventUseInBuild=false
```

(`MSBuild.exe` lives under `<VS install>\MSBuild\Current\Bin\`; find the install with `vswhere -latest -property installationPath`.)
In the IDE: Retarget Solution to your toolset/SDK, then:

1. Open `OpenCBP_FO4.sln`.
2. **Release | x64**. Debug|x64 exists; Win32 configurations are dead weight from the SSE ancestor and won't link.
3. Build. Output is `x64\Release\cbp.dll`. The Release post-build event copies it to
   `$(Fallout4Path)\cbp.dll` - either set that env var to your `Data\F4SE\Plugins` folder or
   blank the PostBuildEvent.
4. Ship `cbp.dll` + `ocbp.ini` in `Data\F4SE\Plugins\`. The user also needs F4SE 0.7.9 and
   Address Library for F4SE Plugins (All In One) installed.

## How the Per-Frame Hook Works Now

`CBPSSE/FrameHook.cpp`. Two ways to get `UpdateActors()` called once per frame, chosen by
`[General] hookMode` in `ocbp.ini`:

| hookMode | What it does | Depends on |
|---|---|---|
| `auto` (default) | try `addresslib`, fall back to `f4setask` | - |
| `addresslib` | DetourXS trampoline over `ProcessEventQueue_Internal`, RVA looked up by ID in `version-1-11-240-0.bin` at runtime | Address Library installed, ID filled in |
| `f4setask` | `F4SETaskInterface::AddTaskPermanent` - F4SE calls us from its own message-queue hook every frame | F4SE 0.7.x (task interface v2) |

No RVA is compiled into the DLL any more. `addresslib` needs the Address Library ID for the
function; `f4setask` needs nothing at all because F4SE owns the address.

Everything is logged to `Data\F4SE\Plugins\cbp.log` (rewritten on every launch). First thing
to read when it doesn't work.

## Updating for the Next Fallout 4 Patch

Bethesda sneezes, F4SE updates in days, Address Library some days after that. Steps:

1. **Sync F4SE.** `git clone https://github.com/ianpatt/f4se` and copy every `.h/.cpp/.rc/.txt`
   from `f4se/`, `f4se_common/`, `xbyak/` over ours. Do NOT copy their vcxproj files.
2. **Bump the target.** In `CBPSSE/main.cpp` change `OCBP_TARGET_RUNTIME` and
   `OCBP_TARGET_RUNTIME_STR`. In `CBPSSE/CBPSSE.vcxproj` set `RUNTIME_VERSION=` to the hex
   value from `f4se_common/f4se_version.h` (both Debug and Release), and fix the
   `f4se_1_11_240.lib` name. In `f4se/f4se/f4se.vcxproj` change `<TargetName>` to match.
3. **Re-verify the Address Library ID.** Open upstream `f4se/Hooks_Threads.cpp`, note the new
   `ProcessEventQueue_Internal` RVA, put it in
   `CBPSSE/OCBP_AddressIDs.h` as `kBootstrapRVA_...`, set `kProcessEventQueue_Internal = 0`,
   build, run once. `cbp.log` prints `Reverse lookup of RVA 0x... -> ID N`. If N matches the
   old ID, nothing changed (expected within the 1.11.x ID space). Paste N, rebuild.
   Until you do, `auto` mode runs on `f4setask` and physics still works.
4. Sanity-check the struct fields we poke, in case a header moved: `Actor::unkF0` (loaded
   data / root node), `Actor::biped` (`BipedAnim::object[slot].parent.object` / `.armorAddon` - this
   replaced `Actor::equipData` in 0.7.9), `ExtraDataList::HasType(kExtraData_PowerArmor)`,
   `TESRace::editorId`, `TESNPC::GetSex`, `TESObjectCELL::objectList`, `NiAVObject::NiUpdateData`.
   All exist in 0.7.9; `grep` them after every sync.

## Troubleshooting

- **cbp.log says `unsupported runtime version`** - wrong exe for this build. See above.
- **cbp.log says `cannot open ...version-1-11-240-0.bin`** - Address Library not installed
  (or the wrong one). Plugin falls back to `f4setask` in `auto`; physics still runs.
- **cbp.log says `kProcessEventQueue_Internal is unset`** - the ID hasn't been filled in yet.
  Also falls back. Copy the printed ID into `OCBP_AddressIDs.h`.
- **Physics don't work at all** - check the skeleton has the bones named in `[Attach]`, that
  the actor isn't in power armor (skipped on purpose), and that `ocbp.ini` is actually in
  `Data\F4SE\Plugins`.
- **LNK2019 / cannot open f4se_1_11_240.lib** - build order. `common_vc14` -> `f4se_common` ->
  `f4se` -> `OpenCBP_FO4`. The sln has the project references; if VS ignores them, build them by hand.

## Provenance

Original OpenCBP by takosako; FO4 port & CBPC-1.5 updates by ericncream; 1.10.984 update
and this 1.11.240 / Address Library refactor by fordinator (with caco-bot doing the typing).

*- caco-bot, still here, still profane, now apparently compiling things after all*
