#include <algorithm>
#include <unordered_map>

#include "ActorUtils.h"
#include "log.h"
#include "unordered_dense.h"

#include "f4se/GameExtraData.h"
#include "f4se/GameObjects.h"
#include <f4se/GameReferences.h>
#include "f4se/GameRTTI.h"

#include <f4se/NiNodes.h>
#include <f4se/NiObjects.h>
#include <f4se/NiTypes.h>

std::string actorUtils::GetActorRaceEID(Actor* actor)
{
    if (!IsActorValid(actor))
    {
        logger.Info("GetActorRaceEID: no actor!\n");
        return "";
    }

    return std::string(actor->race->editorId.c_str());
}

NiAVObject* actorUtils::GetBaseSkeleton(Actor* actor)
{
    BSFixedString skeletonNif_name("skeleton.nif");

    if (!actorUtils::IsActorValid(actor))
    {
        logger.Error("%s: No valid actor\n", __func__);
        return NULL;
    }
    auto loadedState = actor->unkF0;
    if (!loadedState || !loadedState->rootNode)
    {
        logger.Error("%s:No loaded state for actor %08x\n", __func__, actor->formID);
        return NULL;
    }
    auto obj = loadedState->rootNode->GetObjectByName(&skeletonNif_name);

    if (!obj)
    {
        logger.Error("%s: Couldn't get name for loaded state for actor %08x\n", __func__, actor->formID);
        return NULL;
    }

    return obj;
}

bool actorUtils::IsActorFilteredActor(Actor* actor, UInt32 priority)
{
    auto& actorOverrideConfigEntry = configActorOverrideMap[priority];
    auto& overrideActors = actorOverrideConfigEntry.actors;
    auto isFilterInverted = actorOverrideConfigEntry.isFilterInverted;
    bool result;

    if (isFilterInverted)
    {
        logger.Info("%s: actor %08x is actor whitelisted\n", __func__, actor->formID);
        // Result is in the whitelist
        result = !(overrideActors.count(actor->formID) > 0);
    }
    else
    {
        logger.Info("%s: actor %08x is actor blacklisted\n", __func__, actor->formID);
        // Result is in the blacklist
        result = (overrideActors.count(actor->formID) > 0);
    }

    // Simplified to xor
    //result = isFilterInverted ^ (overrideActors.count(actor->formID) == 0);
    
    return result;
}

bool actorUtils::IsActorMale(Actor* actor)
{
    if (!IsActorValid(actor))
    {
        logger.Info("IsActorMale: no actor!\n");
        return false;
    }

    TESNPC* actorNPC = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);

    auto npcSex = actorNPC ? CALL_MEMBER_FN(actorNPC, GetSex)() : 1;

    if (npcSex == 0) //Actor is male
        return true;
    else
        return false;
}

bool actorUtils::IsActorInPowerArmor(Actor* actor)
{
    bool isInPowerArmor = false;
    if (!IsActorValid(actor))
    {
        logger.Info("IsActorInPowerArmor: no actor!\n");
        return true;
    }
    if (!actor->extraDataList)
    {
        logger.Info("IsActorInPowerArmor: no extraDataList!\n");
        return true;
    }

    isInPowerArmor = actor->extraDataList->HasType(kExtraData_PowerArmor);
    //logger.Info("is in power armor: %d\n", isInPowerArmor);
    return isInPowerArmor;
}

bool actorUtils::IsActorTrackable(Actor* actor)
{
    if (!IsActorValid(actor))
    {
        logger.Info("%s: actor %x is not trackable.\n", __func__, actor->formID);
        return false;
    }

    bool inRaceWhitelist = find(raceWhitelist.begin(), raceWhitelist.end(), actorUtils::GetActorRaceEID(actor)) != raceWhitelist.end();
    return (!playerOnly || (actor->formID == 0x14 && playerOnly)) &&
        (!maleOnly || (IsActorMale(actor) && maleOnly)) &&
        (!femaleOnly || (!IsActorMale(actor) && femaleOnly)) &&
        (!npcOnly || (actor->formID != 0x14 && npcOnly)) &&
        (!useWhitelist || (inRaceWhitelist && useWhitelist));
}

bool actorUtils::IsActorValid(Actor* actor)
{
    if (!actor)
    {
        logger.Info("%s: actor %x is null\n", __func__, actor->formID);
        return false;
    }
    if (actor->flags & TESForm::kFlag_IsDeleted)
    {
        logger.Info("%s: actor %x has deleted flag\n", __func__, actor->formID);
        return false;
    }
    if (actor->unkF0 && actor->unkF0->rootNode)
    {
        return true;
    }
    logger.Info("%s: actor %x is not in a valid state\n", __func__, actor->formID);
    return false;
}

bool actorUtils::IsBoneInWhitelist(Actor* actor, std::string boneName)
{
    if (!IsActorValid(actor))
    {
        logger.Info("IsBoneInWhitelist: actor is not valid.\n");
        return false;
    }
    bool result;
    auto raceEID = actorUtils::GetActorRaceEID(actor);
    auto whitelist_bone = whitelist.find(boneName);
    if (whitelist_bone != whitelist.end())
    {
        auto & racesMap = whitelist_bone->second;

        if (racesMap.find(raceEID) != racesMap.end())
        {
            if (IsActorMale(actor))
            {
                result = racesMap.at(raceEID).male;
                //logger.Info("%s: %s male is %d for actor %x.\n", __func__, boneName.c_str(), result,  actor->formID);
                return result;
            }
            else
            {
                result = racesMap.at(raceEID).female;
                //logger.Info("%s: %s female is %d for actor %x.\n", __func__, boneName.c_str(), result, actor->formID);
                return result;
            }
        }
    }
    return false;
}

const actorUtils::EquippedArmor actorUtils::GetActorEquippedArmor(Actor* actor, UInt32 slot)
{
    // F4SE 0.7.9: the old Actor::equipData (ActorEquipData::slots[]) is gone.
    // The third-person biped (Actor::biped at 0x428) holds the same per-slot
    // data: parent.object is the worn item, armorAddon is what used to be "model".
    if (!actorUtils::IsActorValid(actor))
    {
        logger.Error("Actor is not valid");
        return actorUtils::EquippedArmor{ nullptr, nullptr };
    }
    if (slot >= BIPOBJECT::BIPED_OBJECT::kTotal)
    {
        logger.Error("slot %u out of range", slot);
        return actorUtils::EquippedArmor{ nullptr, nullptr };
    }

    BipedAnim* biped = actor->biped.get();
    if (!biped)
    {
        logger.Info("Actor %08x has no biped data", actor->formID);
        return actorUtils::EquippedArmor{ nullptr, nullptr };
    }

    const BIPOBJECT& obj = biped->object[slot];
    if (!obj.parent.object)
    {
        return actorUtils::EquippedArmor{ nullptr, nullptr };
    }
    return actorUtils::EquippedArmor{ obj.parent.object, obj.armorAddon };
}

UInt64 actorUtils::BuildActorKey(Actor* actor)
{
    std::unordered_map<UInt64, UInt64> key;
    ankerl::unordered_dense::hash<UInt64> hash;
    UInt64 hashKey = 0;
    UInt64 actorFormID = (UInt64)actor->formID;

    // key always starts with actors formID (really the refID)
    key[actorFormID] = 1;
    hashKey = hash((UInt64)actor->formID);

    for (auto slot : usedSlots)
    {
        // Check if actor has config's slot equipped
        UInt64 data = 0;
        auto equipped = actorUtils::GetActorEquippedArmor(actor, slot);

        // If there were any slots found, store it
        if (equipped.armor)
        {
            data |= equipped.armor->formID;
            data = data << 32;
        }
        if (equipped.model)
        {
            data |= equipped.model->formID;
        }
        if (data)
        {   
            auto valIter = key.find(data);
            if (valIter != key.end())
            {
                key[data] = key[data] + 1;
                hashKey += hash(data) * key[data];
            }
            else
            {
                key[data] = 1;
                hashKey += hash(data) * key[data];
            }
        }
    }

    //logger.Info("%s: actor %08x has hash key 0x%x\n", __func__, actor->formID, hashKey);

    return hashKey;
}

config_t actorUtils::BuildConfigForActor(Actor* actor, UInt64 hashKey)
{
    // Search cached configs for already existing config
    auto found = cachedConfigs.find(hashKey);
    if (found != cachedConfigs.end())
    {
        logger.Info("%s: Cached config found for actor %08x: %x\n", __func__, actor->formID, hashKey);
        return found->second;
    }

    // Otherwise, build the actor's config
    config_t baseConfig = config;

    for (auto priIter = priorities.rbegin(); priIter != priorities.rend(); ++priIter)
    {
        UInt32 priority = *priIter;

        // Check if there is no armor slot override entry
        if (configArmorOverrideMap.count(priority) == 0
            || (configArmorOverrideMap[priority].armors.empty()
                && configArmorOverrideMap[priority].slots.empty()))
        {
            auto & overrideConfig = configActorOverrideMap[priority].config;

            if (IsActorFilteredActor(actor, priority))
            {
                for (auto & val : overrideConfig)
                {
                    // for each bone, if it is empty, we need to disable it,
                    // otherwise the configEntry is good.
                    if (overrideConfig[val.first].empty())
                    {
                        // This is ok because we're doing this to a premade copy sequentially
                        baseConfig.unsafe_erase(val.first);
                    }
                    else
                    {
                        baseConfig[val.first] = val.second;
                    }
                }
            }
            else
            {
                logger.Info("%s: actor %08x is not filtered for priority %d\n", __func__, actor->formID, priority);
            }
        }
        else // There is an armor slot override entry
        {
            // If priority level has an entry in actor override map AND 
            // actor is not whitelisted or is blacklisted, continue on
            if (configActorOverrideMap.count(priority) > 0)
            {
                if (IsActorFilteredActor(actor, priority))
                {
                    logger.Info("%s: actor %08x is filtered for priority %d\n", __func__, actor->formID, priority);
                    continue;
                }
            }

            auto & orData = configArmorOverrideMap[priority];

            std::vector<actorUtils::EquippedArmor> equippedList;

            // Make a list of actor's slots that are config's slots
            for (auto slot : orData.slots)
            {
                EquippedArmor equipped = actorUtils::GetActorEquippedArmor(actor, slot);
                if (equipped.armor && equipped.model)
                {
                    equippedList.push_back(equipped);
                }
            }

            // whitelist filter
            if (orData.isFilterInverted)
            {
                logger.Info("%s: actor %08x is armor whitelisted\n", __func__, actor->formID);

                //  Check config's filter IDs against found slot's IDs 
                for (auto & equipped : equippedList)
                {
                    // Filter all armors listed
                    if (orData.armors.count(equipped.armor->formID) ||
                        orData.armors.count(equipped.model->formID))
                    {
                        for (auto & val : orData.config)
                        {
                            // for each bone, if it is empty, we need to disable it,
                            // otherwise the configEntry is good.
                            if (orData.config[val.first].empty())
                            {
                                // This is ok because we're doing this to a premade copy sequentially
                                baseConfig.unsafe_erase(val.first);
                            }
                            else
                            {
                                baseConfig[val.first] = val.second;
                            }
                        }
                    }
                }
            }

            // blacklist filter
            if (!orData.isFilterInverted && !equippedList.empty())
            {
                logger.Info("%s: actor %08x is armor blacklisted\n", __func__, actor->formID);

                //  Check config's filter IDs against found slot's IDs 
                for (auto& equipped : equippedList)
                {
                    // Filter all armors not listed
                    if (orData.armors.count(equipped.armor->formID) == 0 &&
                        orData.armors.count(equipped.model->formID) == 0)
                    {
                        for (auto& val : orData.config)
                        {
                            // for each bone, if it is empty, we need to disable it,
                            // otherwise the configEntry is good.
                            if (orData.config[val.first].empty())
                            {
                                // This is ok because we're doing this to a premade copy sequentially
                                baseConfig.unsafe_erase(val.first);
                            }
                            else
                            {
                                baseConfig[val.first] = val.second;
                            }
                        }
                    }
                }
            }
        }
    }

    logger.Info("%s: Inserting cached config for actor %08x: %x\n", __func__, actor->formID, hashKey);
    cachedConfigs.insert(std::make_pair(hashKey, baseConfig));
    //logger.Info("%s: exiting\n", __func__);
    return baseConfig;
}