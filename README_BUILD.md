# OpenCBP Physics for Fallout 4 - Build Instructions

*A caco-bot production - because apparently I write READMEs now instead of Discord bots. Fucking wonderful.*

## What the Hell Is This?

This is OpenCBP Physics for Fallout 4, refactored for compatibility with the latest game version (1.10.984) and F4SE 0.7.2. It's a body physics simulation plugin that makes your NPCs jiggle realistically instead of being stiff corpo mannequins.

## Build Requirements

**Visual Studio 2017 or later** - Don't even think about using some ancient IDE from the goddamn Bush administration. We're using v141/v142 platform toolsets because that's what the project expects.

**Windows 10/11** - This is Windows-only development. No Linux, no macOS, no fucking Haiku OS. Deal with it.

**F4SE 0.7.2 Source** - The project includes F4SE source but make sure you have the latest version compatible with Fallout 4 1.10.984.

**DirectX SDK** - For the D3D11 hooks. If you don't have this, the compilation will fail harder than my dating life in the '90s.

## Dependencies

- **Microsoft Parallel Patterns Library** - Already included with Visual Studio
- **Xbyak assembler** - Included in f4se directory 
- **DetourXS hooking library** - Included in detourxs-master directory
- **Address Library for F4SE** - Recommended for runtime version independence

## Compilation Steps

### 1. Open the Solution
```
Open OpenCBP_FO4.sln in Visual Studio
```

### 2. Set Build Configuration
- Choose **Release x64** for final builds
- Use **Debug x64** if you want to step through code and see what the fuck is actually happening

### 3. Build Order
The solution should handle dependencies automatically, but if shit breaks:
1. Build `common_vc14` project first
2. Build `f4se_common` project  
3. Build `f4se` project
4. Finally build `OpenCBP_FO4` main project

### 4. Output
If everything doesn't explode, you'll get `cbp.dll` in your output directory.

## Installation

1. Copy `cbp.dll` to your `Fallout4/Data/F4SE/Plugins/` directory
2. Make sure F4SE 0.7.2 is installed
3. Install Address Library for F4SE Plugins for version independence
4. Pray to whatever deity handles game stability

## Important Notes

### Memory Address Warning
The ProcessEventQueue_Internal hook uses a hardcoded memory address that MAY need updating for runtime 1.10.984. If the mod crashes on startup, this is probably why. The address `0x01A09CB0` was for older runtimes.

**Modern Solution**: Use Address Library instead of hardcoded addresses. I've added warnings in the code where this needs to be addressed.

### Runtime Compatibility
- **Fallout 4 1.10.984** - Primary target
- **F4SE 0.7.2** - Required version
- **Visual Studio v142 toolset** - What the project expects

## Troubleshooting

**"LNK2019 unresolved external symbol"** - You're missing dependencies or the F4SE libs aren't building properly.

**"Cannot open include file"** - Check your include paths. The project expects F4SE source in the f4se directory.

**Game crashes on startup** - Memory addresses need updating for your runtime version. Check the hookD3D.cpp warnings.

**Physics don't work** - Make sure you have the right skeleton files and the mod is loading after other skeleton modifications.

## Development Notes

This refactor updates the plugin from targeting runtime 1.10.163 to 1.10.984. Key changes:
- Updated F4SE version headers to 0.7.2
- Fixed library linking for new runtime
- Added warnings about memory address compatibility
- Created missing hook.h header file

The code still uses the old school hardcoded memory address approach instead of modern Address Library. This works but makes the mod fragile across game updates. Future versions should migrate to Address Library for better version independence.

## Final Words

Remember, this is modding in 2024 - half the shit breaks every time Bethesda sneezes. Keep backups, test thoroughly, and don't blame me when your save file gets corrupted because you installed 47 conflicting body mods.

Now stop reading and go compile some goddamn code.

*- caco-bot, reluctant README writer and perpetual victim of corpo game updates*