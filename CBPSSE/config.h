#pragma once

#define _SILENCE_AMP_DEPRECATION_WARNINGS

#include <unordered_set>
#include <set>
#include <map>
#include <vector>

#include <concurrent_vector.h>
#include <concurrent_unordered_map.h>
#include <concurrent_unordered_set.h>

#include "f4se/GameReferences.h"
#include "unordered_dense.h"

#pragma warning(disable : 4996)

class Configuration
{
};

struct whitelistSex
{
    bool male;
    bool female;
};

typedef concurrency::concurrent_unordered_map<std::string, float> configEntry_t; // Map settings for a particular bone
typedef concurrency::concurrent_unordered_map<std::string, configEntry_t> config_t; // Settings for a set of bones
typedef concurrency::concurrent_unordered_map<std::string, concurrency::concurrent_unordered_map<std::string, whitelistSex>> whitelist_t;


struct armorOverrideData
{
    bool isFilterInverted;
    concurrency::concurrent_unordered_set<UInt32> slots;
    concurrency::concurrent_unordered_set<UInt32> armors;
    config_t config;
};

struct actorOverrideData
{
    bool isFilterInverted;
    concurrency::concurrent_unordered_set<UInt32> actors;
    config_t config;
};

extern bool playerOnly;
extern bool femaleOnly;
extern bool maleOnly;
extern bool npcOnly;
extern bool useWhitelist;

extern int configReloadCount;
extern config_t config;
extern concurrency::concurrent_unordered_map<UInt32, armorOverrideData> configArmorOverrideMap;
extern concurrency::concurrent_unordered_map<UInt32, actorOverrideData> configActorOverrideMap;
extern whitelist_t whitelist;
extern std::vector<std::string> raceWhitelist;
extern concurrency::concurrent_unordered_set<UInt32> usedSlots;
extern concurrency::concurrent_unordered_map<UInt64, config_t> cachedConfigs;
extern std::set<UInt32> priorities;

bool LoadConfig();
void DumpWhitelistToLog();