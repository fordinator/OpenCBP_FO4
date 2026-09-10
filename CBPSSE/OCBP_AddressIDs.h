#pragma once

#include <cstdint>

// Address Library IDs used by OpenCBP. One entry per game function we touch.
//
// IDs come from the 1.11.137+ ("Anniversary") Address Library ID space and are
// stable across every 1.11.x runtime that database covers. The RVA is resolved
// at runtime from Data\F4SE\Plugins\version-<ver>.bin, never baked in here.
//
// HOW TO FILL IN / RE-VERIFY AN ID FOR A NEW GAME VERSION
//   1. Find the function's RVA for that exe. For ProcessEventQueue_Internal the
//      authoritative source is F4SE's own f4se/Hooks_Threads.cpp
//      (RelocAddr ProcessEventQueue_Internal(0x........)).
//   2. Reverse-look it up in that version's .bin. Easiest: leave the ID at 0,
//      build, run once - cbp.log prints "reverse lookup of RVA 0x... -> ID N".
//   3. Paste N below. Done; the DLL no longer cares which 1.11.x exe it is on.
namespace OCBP_AddressIDs
{
    // void ProcessEventQueue_Internal(void* thisPtr)
    // The game's per-frame event-queue pump. F4SE hooks the CALL to it; we
    // detour the function itself and run the physics tick after it returns.
    // F4SE 0.7.9 / runtime 1.11.240 RVA: 0x01B1E2F0
    constexpr std::uint64_t kProcessEventQueue_Internal = 2287625;   // resolved 2026-09-09 from version-1-11-240-0.bin (RVA 0x01B1E2F0)

    // RVA of the same function on the runtime this build targets, used ONLY to
    // derive the ID above when it is still 0. Never used for hooking.
    constexpr std::uint64_t kBootstrapRVA_ProcessEventQueue_Internal_1_11_240 = 0x01B1E2F0;
}
