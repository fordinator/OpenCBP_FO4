#pragma once
#include "f4se/GameReferences.h"
#include "config.h"

namespace actorUtils
{
    struct EquippedArmor
    {
        const TESForm* armor;
        const TESForm* model;
    };

    std::string GetActorRaceEID(Actor* actor);
    NiAVObject* GetBaseSkeleton(Actor* actor);
    bool IsActorInPowerArmor(Actor* actor);
    bool IsActorFilteredActor(Actor* actor, UInt32 priority);
    bool IsActorMale(Actor* actor);
    bool IsActorTrackable(Actor* actor);
    bool IsActorValid(Actor* actor);
    bool IsBoneInWhitelist(Actor* actor, std::string boneName);
    const EquippedArmor GetActorEquippedArmor(Actor* actor, UInt32 slot);
    UInt64 BuildActorKey(Actor* actor);
    config_t BuildConfigForActor(Actor* actor, UInt64 hashKey);

    struct ArmorSupportTier
    {
        float scaleMul;
        float stiffnessMul;
        float dampingMul;
    };

    ArmorSupportTier GetActorArmorSupport(Actor* actor, bool isBreastBone, bool isButtBone);
}