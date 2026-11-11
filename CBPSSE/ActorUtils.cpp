#include <algorithm>
#include <atomic>
#include <concurrent_unordered_set.h>
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

// One-shot scene-graph dump so cbp.log shows what the actor's 3D actually
// looks like on this game version. Fires once for the first failure and once
// for the first success, then stays quiet.
static void DumpNodeTree(NiAVObject* node, int depth, int maxDepth)
{
    if (!node) return;
    const char* nm = node->m_name.c_str();
    NiNode* asNode = node->GetAsNiNode();
    logger.Error("    %*s%s%s\n", depth * 2, "", nm ? nm : "<unnamed>", asNode ? "/" : "");
    if (!asNode || depth >= maxDepth) return;
    for (UInt32 i = 0; i < asNode->m_children.m_emptyRunStart; i++)
    {
        NiAVObject* child = asNode->m_children.m_data[i];
        if (child) DumpNodeTree(child, depth + 1, maxDepth);
    }
}

NiAVObject* actorUtils::GetBaseSkeleton(Actor* actor)
{
    static BSFixedString skeletonNif_name("skeleton.nif");
    static std::atomic<bool> dumpedFailure = false;
    static std::atomic<bool> dumpedSuccess = false;
    static concurrency::concurrent_unordered_set<UInt32> loggedFailure;

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

    NiNode* root = loadedState->rootNode;
    const char* how = "child";
    NiAVObject* obj = root->GetObjectByName(&skeletonNif_name);

    // GetObjectByName searches descendants; if the loaded root IS skeleton.nif,
    // accept the root itself.
    if (!obj && root->m_name.c_str() && _stricmp(root->m_name.c_str(), "skeleton.nif") == 0)
    {
        obj = root;
        how = "root";
    }

    if (!obj)
    {
        if (loggedFailure.insert(actor->formID).second)
        {
            logger.Error("%s: no skeleton.nif under root '%s' for actor %08x (further failures for this actor not logged)\n",
                         __func__, root->m_name.c_str() ? root->m_name.c_str() : "<unnamed>", actor->formID);
        }
        if (!dumpedFailure.exchange(true))
        {
            logger.Error("%s: scene graph dump (FAILURE) for actor %08x, root parent '%s':\n", __func__, actor->formID,
                         (root->m_parent && root->m_parent->m_name.c_str()) ? root->m_parent->m_name.c_str() : "<none>");
            DumpNodeTree(root, 0, 3);
        }
        return NULL;
    }

    if (!dumpedSuccess.exchange(true))
    {
        logger.Error("%s: found skeleton.nif as %s for actor %08x; scene graph dump (SUCCESS), root parent '%s':\n", __func__, how, actor->formID,
                     (root->m_parent && root->m_parent->m_name.c_str()) ? root->m_parent->m_name.c_str() : "<none>");
        DumpNodeTree(root, 0, 3);
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

actorUtils::ArmorSupportTier actorUtils::GetActorArmorSupport(Actor* actor, bool isBreastBone, bool isButtBone)
{
    ArmorSupportTier defaultSupport{ 1.0f, 1.0f, 1.0f };
    if (!actor || !IsActorValid(actor))
        return defaultSupport;

    // 1. Power Armor check: completely eliminates physics inside power armor
    if (IsActorInPowerArmor(actor))
    {
        return ArmorSupportTier{ 0.0f, 10.0f, 5.0f };
    }

    BipedAnim* biped = actor->biped.get();
    if (!biped)
        return defaultSupport;

    // Slots of interest:
    // Breasts: 11 (Torso Armor 41), 3 (Body Outfit 33), 6 (Underwear Top 36)
    // Butt/Thighs: 3 (Body Outfit 33), 7 (Underwear Bottom 37), 10 (Left Leg 40), 12 (Right Leg 42)
    const UInt32 breastSlots[] = { 11, 3, 6 };
    const UInt32 buttSlots[] = { 3, 7, 10, 12 };

    const UInt32* slotsToCheck = isBreastBone ? breastSlots : (isButtBone ? buttSlots : nullptr);
    size_t numSlots = isBreastBone ? (sizeof(breastSlots) / sizeof(breastSlots[0])) :
                      (isButtBone ? (sizeof(buttSlots) / sizeof(buttSlots[0])) : 0);

    if (!slotsToCheck || numSlots == 0)
        return defaultSupport;

    // Determine actor's naked skin form if present so we don't treat bare skin as armor
    TESObjectARMO* raceSkin = nullptr;
    if (actor->race)
    {
        raceSkin = actor->race->skinForm.skin;
    }
    TESObjectARMO* npcSkin = nullptr;
    if (actor->baseForm)
    {
        TESNPC* npc = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
        if (npc)
        {
            npcSkin = npc->skinForm.skin;
        }
    }

    const TESForm* inspectedForms[8];
    size_t inspectedCount = 0;

    float minScale = 1.0f;
    float maxStiffness = 1.0f;
    float maxDamping = 1.0f;
    bool foundAnyEquipped = false;

    for (size_t s = 0; s < numSlots; ++s)
    {
        UInt32 slotIdx = slotsToCheck[s];
        if (slotIdx >= BIPOBJECT::kEditorCount)
            continue;

        const BIPOBJECT& obj = biped->object[slotIdx];
        if (!obj.parent.object)
            continue;

        const TESForm* armorForm = (const TESForm*)obj.parent.object;
        if (!armorForm)
            continue;

        // Skip naked skin records
        if ((raceSkin && armorForm == raceSkin) || (npcSkin && armorForm == npcSkin))
            continue;

        // Deduplicate
        bool alreadyInspected = false;
        for (size_t i = 0; i < inspectedCount; ++i)
        {
            if (inspectedForms[i] == armorForm)
            {
                alreadyInspected = true;
                break;
            }
        }
        if (alreadyInspected)
            continue;

        if (inspectedCount < 8)
            inspectedForms[inspectedCount++] = armorForm;

        foundAnyEquipped = true;

        // Baseline support for clothed actors without specific tags
        float itemScale = 0.90f;
        float itemStiffness = 1.05f;
        float itemDamping = 1.05f;

        BGSKeywordForm* pKeywords = DYNAMIC_CAST((TESForm*)armorForm, TESForm, BGSKeywordForm);
        if (pKeywords && pKeywords->keywords && pKeywords->numKeywords > 0)
        {
            for (UInt32 k = 0; k < pKeywords->numKeywords; ++k)
            {
                BGSKeyword* kwd = pKeywords->keywords[k];
                if (!kwd || !kwd->keyword.c_str())
                    continue;

                const char* kw = kwd->keyword.c_str();

                if (isBreastBone)
                {
                    // Power Armor
                    if (strstr(kw, "ArmorTypePower"))
                    {
                        itemScale = 0.0f; itemStiffness = 10.0f; itemDamping = 5.0f;
                        break;
                    }
                    // Heavy / full plate
                    else if (strstr(kw, "sakr_kwd_armorTopFull") || strstr(kw, "ArmorTypeHeavy") || strstr(kw, "ArmorTypeMetal") || strstr(kw, "ArmorTypeCombat"))
                    {
                        if (itemScale > 0.35f) { itemScale = 0.35f; itemStiffness = 1.80f; itemDamping = 1.30f; }
                    }
                    // Full top / coat / sweater
                    else if (strstr(kw, "sakr_kwd_topFull") || strstr(kw, "sakr_kwd_armorTopBreast") || strstr(kw, "ArmorTypeMedium") || strstr(kw, "ArmorTypeLeather"))
                    {
                        if (itemScale > 0.60f) { itemScale = 0.60f; itemStiffness = 1.35f; itemDamping = 1.15f; }
                    }
                    // Normal bra / tight top / corset / light armor
                    else if (strstr(kw, "sakr_kwd_braNormal") || strstr(kw, "sakr_kwd_topTagTight") || strstr(kw, "ArmorTypeLight"))
                    {
                        if (itemScale > 0.75f) { itemScale = 0.75f; itemStiffness = 1.20f; itemDamping = 1.10f; }
                    }
                    // Standard civilian clothing / dress / shirt / rags
                    else if (strstr(kw, "ArmorClothing"))
                    {
                        if (itemScale > 0.92f) { itemScale = 0.92f; itemStiffness = 1.05f; itemDamping = 1.02f; }
                    }
                    // Skimpy / Cleavage / Bikini / Crop / Tank / Halter / SideBoob / Sheer
                    else if (strstr(kw, "sakr_kwd_braBikini") || strstr(kw, "sakr_kwd_topCleavage") ||
                             strstr(kw, "sakr_kwd_topLowCutCleavage") || strstr(kw, "sakr_kwd_topTagCropTop") ||
                             strstr(kw, "sakr_kwd_topTagTankTop") || strstr(kw, "sakr_kwd_topTagHalterTop") ||
                             strstr(kw, "sakr_kwd_topTagSideBoob") || strstr(kw, "sakr_kwd_braTagSheer") ||
                             strstr(kw, "sakr_kwd_topTagSheer"))
                    {
                        itemScale = 0.88f; itemStiffness = 1.10f; itemDamping = 1.05f;
                    }
                    // Micro-Bikini / Lewd bare
                    else if (strstr(kw, "sakr_kwd_braMicroBikini") || strstr(kw, "sakr_kwd_armorTopLewd"))
                    {
                        itemScale = 1.00f; itemStiffness = 1.00f; itemDamping = 1.00f;
                    }
                }
                else if (isButtBone)
                {
                    // Power Armor
                    if (strstr(kw, "ArmorTypePower"))
                    {
                        itemScale = 0.0f; itemStiffness = 10.0f; itemDamping = 5.0f;
                        break;
                    }
                    // Heavy plate
                    else if (strstr(kw, "sakr_kwd_armorBottomFull") || strstr(kw, "ArmorTypeHeavy") || strstr(kw, "ArmorTypeMetal"))
                    {
                        if (itemScale > 0.35f) { itemScale = 0.35f; itemStiffness = 2.00f; itemDamping = 1.40f; }
                    }
                    // Pants long / tight pants / long skirt
                    else if (strstr(kw, "sakr_kwd_pantsLong") || strstr(kw, "sakr_kwd_skirtLong") ||
                             strstr(kw, "sakr_kwd_pantsTagTight") || strstr(kw, "sakr_kwd_skirtTagTight") ||
                             strstr(kw, "sakr_kwd_armorBottomButt") || strstr(kw, "ArmorTypeMedium") || strstr(kw, "ArmorTypeLeather"))
                    {
                        if (itemScale > 0.65f) { itemScale = 0.65f; itemStiffness = 1.45f; itemDamping = 1.20f; }
                    }
                    // Shorts / Short Skirt / Normal panties
                    else if (strstr(kw, "sakr_kwd_pantsShorts") || strstr(kw, "sakr_kwd_skirtShort") ||
                             strstr(kw, "sakr_kwd_pantyNormal") || strstr(kw, "ArmorTypeLight") || strstr(kw, "ArmorClothing"))
                    {
                        if (itemScale > 0.78f) { itemScale = 0.78f; itemStiffness = 1.25f; itemDamping = 1.10f; }
                    }
                    // Hot pants / mini skirt / micro skirt / thong pants
                    else if (strstr(kw, "sakr_kwd_pantsHotPants") || strstr(kw, "sakr_kwd_skirtMini") ||
                             strstr(kw, "sakr_kwd_skirtMicro") || strstr(kw, "sakr_kwd_pantsThong") ||
                             strstr(kw, "sakr_kwd_pantyTagSheer") || strstr(kw, "sakr_kwd_skirtTagSheer"))
                    {
                        itemScale = 0.90f; itemStiffness = 1.10f; itemDamping = 1.05f;
                    }
                    // Thong / G-string / Lewd bare
                    else if (strstr(kw, "sakr_kwd_pantyThong") || strstr(kw, "sakr_kwd_pantyGString") || strstr(kw, "sakr_kwd_armorBottomLewd"))
                    {
                        itemScale = 1.00f; itemStiffness = 1.00f; itemDamping = 1.00f;
                    }
                }
            }
        }

        if (itemScale < minScale) minScale = itemScale;
        if (itemStiffness > maxStiffness) maxStiffness = itemStiffness;
        if (itemDamping > maxDamping) maxDamping = itemDamping;
    }

    if (!foundAnyEquipped)
    {
        return ArmorSupportTier{ 1.0f, 1.0f, 1.0f };
    }

    return ArmorSupportTier{ minScale, maxStiffness, maxDamping };
}