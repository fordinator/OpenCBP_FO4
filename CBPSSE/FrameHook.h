#pragma once

#include "f4se/PluginAPI.h"

// How UpdateActors() gets called once per frame.
enum class FrameHookMode
{
    None = 0,
    Auto,           // AddressLibrary if the .bin + ID resolve, else F4SETask
    AddressLibrary, // DetourXS on ProcessEventQueue_Internal, RVA from Address Library
    F4SETask        // F4SETaskInterface::AddTaskPermanent (F4SE owns the address)
};

FrameHookMode ParseFrameHookMode(const char* text);
const char* FrameHookModeName(FrameHookMode mode);

// Installs the per-frame hook. Call once, after F4SE reports game data ready.
bool InstallFrameHook(F4SETaskInterface* task, UInt32 runtimeVersion, FrameHookMode requested);
FrameHookMode ActiveFrameHookMode();
