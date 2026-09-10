#pragma once
#include <f4se/NiObjects.h>
#include <f4se/NiTypes.h>
#include <f4se/GameReferences.h>
#include <time.h>
#include "config.h"
#include <shared_mutex>

//using std::shared_mutex;

typedef concurrency::concurrent_unordered_map<const char*, concurrency::concurrent_unordered_map<UInt32, NiPoint3>> pos_map;
typedef concurrency::concurrent_unordered_map<const char*, concurrency::concurrent_unordered_map<UInt32, NiMatrix43>> rot_map;

inline void RefreshNode(NiAVObject* node)
{
    if (node == nullptr || node->m_name == nullptr)
        return;

    NiAVObject::NiUpdateData npd;
    npd.unk00 = nullptr;
    npd.pCamera = nullptr;
    npd.flags = 0;
    npd.unk14 = 0;
    npd.unk18 = 0;
    npd.unk20 = 0;
    npd.unk28 = 0;
    npd.unk30 = 0;
    npd.unk38 = 0;
    node->UpdateWorldData(&npd);
}

class Thing
{
    BSFixedString boneName;
    NiPoint3 oldWorldPos;
    NiPoint3 oldWorldPosRot;
    //NiPoint3 oldLocalDiff;
    float oldRotZ;
    NiPoint3 velocity;
    clock_t time;
    NiAVObject* thingObj;
    bool IsBreastBone;
    NiMatrix43 firstWorldRot;
    NiMatrix43 origWorldRot;
    bool rightSide;
    Actor* m_actor;
    bool initialized;   // skeleton-relative state (origWorldRot, rightSide) has been computed

public:
    bool isEnabled;

    float stiffness = 0.5f;
    float stiffness2 = 0.0f;
    float damping = 0.2f;
    float maxOffsetX = 5.0f;
    float maxOffsetY = 5.0f;
    float maxOffsetZ = 5.0f;
    float cogOffsetX = 0.0f;
    float cogOffsetY = 0.0f;
    float cogOffsetZ = 0.0f;

    float gravityBias = 0.0f;
    float gravityCorrection = 0.0f;
    float timeTick = 4.0f;
    float linearX = 0.0f;
    float linearY = 0.0f;
    float linearZ = 0.0f;
    float rotationalX = 0.0f;
    float rotationalY = 0.0f;
    float rotationalZ = 0.0f;

    float rotateLinearX = 0.0f;
    float rotateLinearY = 0.0f;
    float rotateLinearZ = 0.0f;

    float rotateRotationX = 0.0f;
    float rotateRotationY = 0.0f;
    float rotateRotationZ = 0.0f;

    float scaleMultiplierX = 1.0f;
    float scaleMultiplierY = 1.0f;
    float scaleMultiplierZ = 1.0f;

    float posOffsetX = 0.0f;
    float posOffsetY = 0.0f;

    float timeStep = 0.016f;

    bool absRotX = 0;

    float linearSpreadforceXtoY = 0.0f;
    float linearSpreadforceXtoZ = 0.0f;
    float linearSpreadforceYtoX = 0.0f;
    float linearSpreadforceYtoZ = 0.0f;
    float linearSpreadforceZtoX = 0.0f;
    float linearSpreadforceZtoY = 0.0f;

    float gravitySupineX = 0.0f;
    float gravitySupineY = 0.0f;
    float gravitySupineZ = 0.0f;

    static pos_map origLocalPos;
    static rot_map origLocalRot;
    static rot_map origChestWorldRot;

    // Maps are sorted every edit time, so if it is parallel processing then a high probability of overloading
    //static shared_mutex thing_map_lock;

    Thing(NiAVObject* obj, BSFixedString& name, Actor* actor);
    ~Thing();

    NiAVObject* IsThingActorValid(Actor* actor);
    void InitFromSkeleton(NiAVObject* skeletonObj, NiAVObject* obj);
    void Reset(Actor* actor);
    NiPoint3 CalculateGravitySupine(Actor* actor);
    void StoreOriginalTransforms(Actor* actor);
    void UpdateThing(Actor* actor);
    void UpdateConfig(configEntry_t& centry);

    void ShowPos(NiPoint3& p);
    void ShowRot(NiMatrix43& r);

    static inline bool ContainsNoCase(std::string str, std::string ministr)
    {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        std::transform(ministr.begin(), ministr.end(), ministr.begin(), ::tolower);

        if (str.find(ministr) != std::string::npos)
        {
            return true;
        }
        else
            return false;
    }

};