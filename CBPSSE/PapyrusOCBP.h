#pragma once

#define _SILENCE_AMP_DEPRECATION_WARNINGS

#include "f4se/GameTypes.h"
#include "f4se/PapyrusVM.h"
#include <string>

#include <concurrent_unordered_map.h>

class VirtualMachine;
struct StaticFunctionTag;

namespace papyrusOCBP
{
    extern concurrency::concurrent_unordered_map<UInt32, concurrency::concurrent_unordered_map<std::string, bool>> boneIgnores;
    void RegisterFuncs(VirtualMachine* vm);
};