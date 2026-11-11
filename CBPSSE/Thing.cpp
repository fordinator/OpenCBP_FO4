#include "ActorUtils.h"
#include "Thing.h"
#include "log.h"
#include "f4se\NiNodes.h"
#include <time.h>
#include <algorithm>
#include <cstring>

constexpr float DEG_TO_RAD = 3.14159265 / 180;
const char* skeletonNif_boneName = "skeleton.nif";
const float DIFF_LIMIT = 350.0f;

// TODO Make these logger macros
//#define DEBUG 1
//#define TRANSFORM_DEBUG 1

// <BoneName <ActorFormID, data>>
pos_map Thing::origLocalPos;
rot_map Thing::origLocalRot;
rot_map Thing::origChestWorldRot;

//shared_mutex Thing::thing_map_lock;

void Thing::ShowPos(NiPoint3& p)
{
    logger.Info("%8.4f %8.4f %8.4f\n", p.x, p.y, p.z);
}

void Thing::ShowRot(NiMatrix43& r)
{
    logger.Info("%8.4f %8.4f %8.4f %8.4f\n", r.data[0][0], r.data[0][1], r.data[0][2], r.data[0][3]);
    logger.Info("%8.4f %8.4f %8.4f %8.4f\n", r.data[1][0], r.data[1][1], r.data[1][2], r.data[1][3]);
    logger.Info("%8.4f %8.4f %8.4f %8.4f\n", r.data[2][0], r.data[2][1], r.data[2][2], r.data[2][3]);
}

static inline void SetIdentity(NiMatrix43& m)
{
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c)
            m.data[r][c] = (r == c) ? 1.0f : 0.0f;
}

#pragma pack(push, 8)
struct FakeFixedString
{
    struct Entry
    {
        uint32_t pad[2];   // 0x00
        uint32_t state;    // 0x08 - 0 (no kState_External flag 0x4000)
        uint32_t length;   // 0x0C
        Entry* externData; // 0x10 - nullptr
        char str[64];      // 0x18 - null-terminated C string
    };
    Entry entry;
    Entry* pEntry;

    FakeFixedString(const char* s)
    {
        memset(&entry, 0, sizeof(entry));
        strncpy_s(entry.str, sizeof(entry.str), s, _TRUNCATE);
        entry.length = (uint32_t)strlen(entry.str);
        entry.state = 0;
        entry.externData = nullptr;
        pEntry = &entry;
    }
};
#pragma pack(pop)

static float QueryMorphInternal(void* bodyMorphInterface, void* getMorphFn, Actor* actor, bool isFemale, const char* morphName)
{
    FakeFixedString ffs(morphName);
    typedef float (*GetMorphFn)(void* thisPtr, Actor* actor, bool isFemale, const void* pMorphFixedStr, void* keyword);
    GetMorphFn fn = (GetMorphFn)getMorphFn;
    return fn(bodyMorphInterface, actor, isFemale, &ffs.pEntry, nullptr);
}

static float GetActorBreastSliderValue(Actor* actor)
{
    if (!actor)
        return 0.0f;

    HMODULE hF4EE = GetModuleHandleA("f4ee.dll");
    if (!hF4EE)
        return 0.0f;

    uintptr_t f4eeBase = (uintptr_t)hF4EE;
    void* bodyMorphInterface = (void*)(f4eeBase + 0x10DD30);

    __try
    {
        void** vtable = *(void***)bodyMorphInterface;
        if (!vtable || (uintptr_t)vtable != (f4eeBase + 0xCD840))
            return 0.0f;

        void* getMorphFn = vtable[10];
        if (!getMorphFn)
            return 0.0f;

        bool isFemale = !actorUtils::IsActorMale(actor);

        float breasts      = QueryMorphInternal(bodyMorphInterface, getMorphFn, actor, isFemale, "Breasts");
        float doubleMelon  = QueryMorphInternal(bodyMorphInterface, getMorphFn, actor, isFemale, "DoubleMelon");
        float fantasy      = QueryMorphInternal(bodyMorphInterface, getMorphFn, actor, isFemale, "BreastFantasy");
        float centerBig    = QueryMorphInternal(bodyMorphInterface, getMorphFn, actor, isFemale, "BreastCenterBig");
        float newSH        = QueryMorphInternal(bodyMorphInterface, getMorphFn, actor, isFemale, "BreastsNewSH");
        float sevenBUpper  = QueryMorphInternal(bodyMorphInterface, getMorphFn, actor, isFemale, "7B Upper");
        float bigTorso     = QueryMorphInternal(bodyMorphInterface, getMorphFn, actor, isFemale, "BigTorso");

        float small1       = QueryMorphInternal(bodyMorphInterface, getMorphFn, actor, isFemale, "BreastsSmall");
        float small2       = QueryMorphInternal(bodyMorphInterface, getMorphFn, actor, isFemale, "BreastsSmall2");
        float gone         = QueryMorphInternal(bodyMorphInterface, getMorphFn, actor, isFemale, "BreastsGone");
        float flatness2    = QueryMorphInternal(bodyMorphInterface, getMorphFn, actor, isFemale, "BreastFlatness2");

        float pos = breasts 
                  + 0.85f * doubleMelon 
                  + 0.50f * fantasy 
                  + 0.30f * centerBig 
                  + 0.30f * newSH 
                  + 0.25f * sevenBUpper 
                  + 0.15f * bigTorso;

        float neg = 0.70f * small1 
                  + 0.70f * small2 
                  + 1.00f * gone 
                  + 0.80f * flatness2;

        return pos - neg;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0.0f;
    }
}

Thing::Thing(NiAVObject* obj, BSFixedString& name, Actor* actor)
    : thingObj(obj)
    , boneName(name)
    , velocity(NiPoint3(0, 0, 0))
    , m_actor(actor)
{
    // NiPoint3 / NiMatrix43 have no constructors. Every member gets a sane value
    // BEFORE anything that can fail, so a Thing that couldn't find its skeleton
    // yet is inert instead of full of stack garbage.
    isEnabled = true;
    initialized = false;
    oldWorldPos = obj->m_worldTransform.pos;
    oldWorldPosRot = obj->m_worldTransform.pos;
    oldRotZ = 0.0f;
    rightSide = false;
    SetIdentity(firstWorldRot);
    SetIdentity(origWorldRot);
    time = clock();
    dynamicBreastScale = 1.0f;
    dynamicStiffnessMul = 1.0f;
    dynamicDampingMul = 1.0f;
    lastMorphCheckTime = 0;

    IsBreastBone = ContainsNoCase(std::string(boneName.c_str()), "Breast");
    IsButtBone = ContainsNoCase(std::string(boneName.c_str()), "Butt") ||
                 ContainsNoCase(std::string(boneName.c_str()), "Thigh");

    auto skeletonObj = actorUtils::GetBaseSkeleton(actor);
    if (skeletonObj == NULL)
    {
        logger.Error("%s: Didn't find thing %s's base skeleton.nif for actor %08x; will retry on update\n", __func__, boneName.c_str(), actor->formID);
        return;
    }

    InitFromSkeleton(skeletonObj, obj);
}

void Thing::InitFromSkeleton(NiAVObject* skeletonObj, NiAVObject* obj)
{
    auto firstWorldPos = skeletonObj->m_worldTransform.rot * obj->m_worldTransform.pos;
    auto firstSkeletonPos = skeletonObj->m_worldTransform.rot * skeletonObj->m_worldTransform.pos;

    rightSide = (firstWorldPos.x - firstSkeletonPos.x) >= 0.0;

    // Set initial positions
    oldWorldPos = obj->m_worldTransform.pos;
    oldWorldPosRot = obj->m_worldTransform.pos;
    velocity = NiPoint3(0, 0, 0);

    origWorldRot = obj->m_localTransform.rot.Transpose() * obj->m_worldTransform.rot;

    time = clock();
    initialized = true;
}

Thing::~Thing()
{
}

NiPoint3 Thing::CalculateGravitySupine(Actor* actor)
{
    NiPoint3 varGravitySupine = NiPoint3(0.0, 0.0, 0.0);

    if (!IsBreastBone) //other bones don't need to edited gravity by SPINE2 obj
    {
        //other nodes are based on parent obj
        varGravitySupine = NiPoint3(0.0, 0.0, 0.0);
        return varGravitySupine;
    }

    //Get the reference bone to know which way the breasts are orientated
    //thing_ReadNode_lock.lock();
    BSFixedString chest_str("Chest");

    NiAVObject* breastGravityReferenceBone = actor->unkF0->rootNode->GetObjectByName(&chest_str);

    //thing_ReadNode_lock.unlock();

    float gravityRatio = 0.0f;
    if (breastGravityReferenceBone != nullptr)
    {
        //auto breastRot = breastGravityReferenceBone->m_worldTransform.rot;
        //Get the orientation (here the Z element of the rotation matrix (approx 1.0 when standing up, approx -1.0 when upside down))
        auto chestOrientation = breastGravityReferenceBone->m_worldTransform.rot.data[1][2];
        gravityRatio = chestOrientation >= 0.0 ? chestOrientation : 0.0;
    }

    varGravitySupine = NiPoint3(gravitySupineX, gravitySupineY, gravitySupineZ) * gravityRatio;

    return varGravitySupine;
}

void Thing::StoreOriginalTransforms(Actor* actor)
{
    // Save the bones' original local values, per actor, if they already haven't
    auto origLocalPos_iter = origLocalPos.find(boneName.c_str());
    auto origLocalRot_iter = origLocalRot.find(boneName.c_str());

    auto obj = thingObj;

    //thing_map_lock.lock();
    if (IsBreastBone)
    {
        BSFixedString chest_name("Chest");
        NiAVObject* chestObj = actor->unkF0->rootNode->GetObjectByName(&chest_name);

        if (chestObj && chestObj->m_parent)
        {
            auto chestWorldRot_iter = origChestWorldRot.find(boneName.c_str());
            if (chestWorldRot_iter == origChestWorldRot.end())
            {
                origChestWorldRot[boneName.c_str()][actor->formID] = chestObj->m_parent->m_worldTransform.rot * chestObj->m_localTransform.rot;
            }
            else
            {
                auto & actorRotMap = chestWorldRot_iter->second;
                auto actor_iter = actorRotMap.find(actor->formID);
                if (actor_iter == actorRotMap.end())
                {
                    origChestWorldRot[boneName.c_str()][actor->formID] = chestObj->m_parent->m_worldTransform.rot * chestObj->m_localTransform.rot;
                }
            }
        }
    }

    // Original Bone Position
    if (origLocalPos_iter == origLocalPos.end())
    {
        origLocalPos[boneName.c_str()][actor->formID] = obj->m_localTransform.pos;
#ifdef DEBUG
        logger.Error("for bone %s, actor %08x: \n", boneName.c_str(), actor->formID);
        logger.Error("firstRun pos Set: \n");
        ShowPos(obj->m_localTransform.pos);
#endif
    }
    else
    {
        auto actorPosMap = origLocalPos.at(boneName.c_str());
        auto actor_iter = actorPosMap.find(actor->formID);
        if (actor_iter == actorPosMap.end())
        {
            origLocalPos[boneName.c_str()][actor->formID] = obj->m_localTransform.pos;
#ifdef DEBUG
            logger.Error("for bone %s, actor %08x: \n", boneName.c_str(), actor->formID);
            logger.Error("firstRun pos Set: \n");
            ShowPos(obj->m_localTransform.pos);
#endif
        }
    }

    // Original Bone Rotation
    if (origLocalRot_iter == origLocalRot.end())
    {
        origLocalRot[boneName.c_str()][actor->formID] = obj->m_localTransform.rot;
#ifdef DEBUG
        logger.Error("for bone %s, actor %08x: \n", boneName.c_str(), actor->formID);
        logger.Error("firstRun rot Set:\n");
        ShowRot(obj->m_localTransform.rot);
#endif
    }
    else
    {
        auto actorRotMap = origLocalRot.at(boneName.c_str());
        auto actor_iter = actorRotMap.find(actor->formID);
        if (actor_iter == actorRotMap.end())
        {
            origLocalRot[boneName.c_str()][actor->formID] = obj->m_localTransform.rot;
#ifdef DEBUG
            logger.Error("for bone %s, actor %08x: \n", boneName.c_str(), actor->formID);
            logger.Error("firstRun rot Set: \n");
            ShowRot(obj->m_localTransform.rot);
#endif
        }
    }
    //thing_map_lock.unlock();
}

void Thing::UpdateConfig(configEntry_t& centry)
{
    stiffness = centry["stiffness"];
    stiffness2 = centry["stiffness2"];
    damping = centry["damping"];

    maxOffsetX = centry["maxoffsetX"];
    maxOffsetY = centry["maxoffsetY"];
    maxOffsetZ = centry["maxoffsetZ"];

    linearX = centry["linearX"];
    linearY = centry["linearY"];
    linearZ = centry["linearZ"];

    rotationalX = centry["rotationalX"];
    rotationalY = centry["rotationalY"];
    rotationalZ = centry["rotationalZ"];

    rotateLinearX = centry["rotateLinearX"];
    rotateLinearY = centry["rotateLinearY"];
    rotateLinearZ = centry["rotateLinearZ"];

    rotateRotationX = centry["rotateRotationX"];
    rotateRotationY = centry["rotateRotationY"];
    rotateRotationZ = centry["rotateRotationZ"];

    scaleMultiplierX = 1.0f;// centry["scaleMultiplierX"];
    scaleMultiplierY = 1.0f;// centry["scaleMultiplierY"];
    scaleMultiplierZ = 1.0f;// centry["scaleMultiplierZ"];

    posOffsetX = centry["posOffsetX"];
    posOffsetY = centry["posOffsetY"];

    timeTick = centry["timetick"];

    if (centry.find("timeStep") != centry.end())
        timeStep = centry["timeStep"];
    else
        timeStep = 0.016f;

    gravityBias = centry["gravityBias"];
    gravityCorrection = centry["gravityCorrection"];
    cogOffsetY = centry["cogOffsetY"];
    cogOffsetX = centry["cogOffsetX"];
    cogOffsetZ = centry["cogOffsetZ"];
    if (timeTick <= 1)
        timeTick = 1;

    absRotX = centry["absRotX"] != 0.0;

    linearSpreadforceXtoY = centry["linearSpreadforceXtoY"];
    linearSpreadforceXtoZ = centry["linearSpreadforceXtoZ"];
    linearSpreadforceYtoX = centry["linearSpreadforceYtoX"];
    linearSpreadforceYtoZ = centry["linearSpreadforceYtoZ"];
    linearSpreadforceZtoX = centry["linearSpreadforceZtoX"];
    linearSpreadforceZtoY = centry["linearSpreadforceZtoY"];

    gravitySupineX = centry["gravitySupineX"];
    gravitySupineY = centry["gravitySupineY"];
    gravitySupineZ = centry["gravitySupineZ"];
}

static float clamp(float val, float min, float max)
{
    if (val < min) return min;
    else if (val > max) return max;
    return val;
}

static float remap(float value, float start1, float stop1, float start2, float stop2)
{
    return start2 + (stop2 - start2) * ((value - start1) / (stop1 - start1));
}

void Thing::Reset(Actor* actor)
{
    auto loadedState = actor->unkF0;
    if (!loadedState || !loadedState->rootNode)
    {
        logger.Error("No loaded state for actor %08x\n", actor->formID);
        return;
    }
    auto obj = loadedState->rootNode->GetObjectByName(&boneName);

    if (!obj)
    {
        logger.Error("Couldn't get name for loaded state for actor %08x\n", actor->formID);
        return;
    }

    obj->m_localTransform.pos = origLocalPos[boneName.c_str()][actor->formID];
    obj->m_localTransform.rot = origLocalRot[boneName.c_str()][actor->formID];
}

// Returns 
template <typename T> int sgn(T val)
{
    return (T(0) < val) - (val < T(0));
}

NiAVObject* Thing::IsThingActorValid(Actor* actor)
{
    if (!actorUtils::IsActorValid(actor))
    {
        //logger.Error("%s: No valid actor in Thing::Update\n", __func__);
        return NULL;
    }
    auto loadedState = actor->unkF0;
    if (!loadedState || !loadedState->rootNode)
    {
        //logger.Error("%s: No loaded state for actor %08x\n", __func__, actor->formID);
        return NULL;
    }
    auto obj = loadedState->rootNode->GetObjectByName(&boneName);

    if (!obj)
    {
        //logger.Error("%s: Couldn't get name for loaded state for actor %08x\n", __func__, actor->formID);
        return NULL;
    }

    if (!obj->m_parent)
    {
        //logger.Error("%s: Couldn't get bone %s parent for actor %08x\n", __func__, boneName.c_str(), actor->formID);
        return NULL;
    }

    return obj;
}

void Thing::UpdateThing(Actor* actor)
{
    auto forceMultiplier = 1.0;

    thingObj = IsThingActorValid(actor);
    auto obj = thingObj;

    if (!obj)
    {
        return;
    }

    auto newTime = clock();
    auto deltaT = newTime - time;

    time = newTime;
    if (deltaT > 64) deltaT = 64;
    if (deltaT < 8) deltaT = 8;

    if ((IsBreastBone || IsButtBone) && actor)
    {
        if (lastMorphCheckTime == 0 || (newTime - lastMorphCheckTime) > 1200)
        {
            lastMorphCheckTime = newTime;

            float morphScale = 1.0f;
            float morphStiff = 1.0f;
            float morphDamp = 1.0f;

            if (IsBreastBone)
            {
                float s = GetActorBreastSliderValue(actor);
                float s_clamped = (s < -1.0f) ? -1.0f : (s > 2.0f ? 2.0f : s);

                morphScale = 1.0f + 0.35f * s_clamped;
                if (morphScale < 0.40f) morphScale = 0.40f;
                if (morphScale > 1.70f) morphScale = 1.70f;

                morphStiff = 1.0f - 0.15f * s_clamped;
                if (morphStiff < 0.70f) morphStiff = 0.70f;
                if (morphStiff > 1.30f) morphStiff = 1.30f;

                morphDamp = 1.0f - 0.10f * s_clamped;
                if (morphDamp < 0.80f) morphDamp = 0.80f;
                if (morphDamp > 1.20f) morphDamp = 1.20f;
            }

            // SAKR & Armor Clothing Support
            auto armorSupport = actorUtils::GetActorArmorSupport(actor, IsBreastBone, IsButtBone);

            dynamicBreastScale = morphScale * armorSupport.scaleMul;
            dynamicStiffnessMul = morphStiff * armorSupport.stiffnessMul;
            dynamicDampingMul = morphDamp * armorSupport.dampingMul;

            if (dynamicBreastScale < 0.0f) dynamicBreastScale = 0.0f;
            if (dynamicBreastScale > 2.0f) dynamicBreastScale = 2.0f;

            logger.Info("OpenCBP: actor %08x [%s] morphScale=%.2f armScale=%.2f finalScale=%.2f finalStiff=%.2f\n",
                actor->formID, boneName.c_str(), morphScale, armorSupport.scaleMul, dynamicBreastScale, dynamicStiffnessMul);
        }
    }

#if TRANSFORM_DEBUG
    auto sceneObj = obj;
    while (sceneObj->m_parent && sceneObj->m_name != "skeleton.nif")
    {
        logger.Info(sceneObj->m_name);
        logger.Info("\n---\n");
        logger.Error("Actual m_localTransform.pos: ");
        ShowPos(sceneObj->m_localTransform.pos);
        logger.Error("Actual m_worldTransform.pos: ");
        ShowPos(sceneObj->m_worldTransform.pos);
        logger.Info("---\n");
        //logger.Error("Actual m_localTransform.rot Matrix:\n");
        ShowRot(sceneObj->m_localTransform.rot);
        //logger.Error("Actual m_worldTransform.rot Matrix:\n");
        ShowRot(sceneObj->m_worldTransform.rot);
        logger.Info("---\n");
        //if (sceneObj->m_parent) {
        //	logger.Error("Calculated m_worldTransform.pos: ");
        //	ShowPos((sceneObj->m_parent->m_worldTransform.rot.Transpose() * sceneObj->m_localTransform.pos) + sceneObj->m_parent->m_worldTransform.pos);
        //	logger.Error("Calculated m_worldTransform.rot Matrix:\n");
        //	ShowRot(sceneObj->m_localTransform.rot * sceneObj->m_parent->m_worldTransform.rot);
        //}
        sceneObj = sceneObj->m_parent;
    }
#endif

    StoreOriginalTransforms(actor);

    auto skeletonObj = actorUtils::GetBaseSkeleton(actor);
    if (skeletonObj == NULL)
    {
        logger.Error("%s: Didn't find thing %s's base skeleton.nif for actor %08x \n", __func__, boneName.c_str(), actor->formID);
        return;
    }

#if DEBUG
    logger.Error("bone %s for actor %08x with parent %s\n", boneName.c_str(), actor->formID, skeletonObj->m_name.c_str());
    ShowRot(skeletonObj->m_worldTransform.rot);
    //ShowPos(obj->m_parent->m_worldTransform.rot.Transpose() * obj->m_localTransform.pos);
#endif

    if (!initialized)
    {
        // Skeleton was not available when this Thing was built; finish set-up now
        // and skip this frame so the first step starts from a real position.
        logger.Error("%s: late init of bone %s for actor %08x\n", __func__, boneName.c_str(), actor->formID);
        InitFromSkeleton(skeletonObj, obj);
        return;
    }

    NiPoint3 origWorldPos = (obj->m_parent->m_worldTransform.rot * origLocalPos[boneName.c_str()][actor->formID]) + obj->m_parent->m_worldTransform.pos;
    NiPoint3 origWorldPosRot = origWorldPos;

    // Cog offset is in parent local space, rotated to world space via forward parent rotation
    NiPoint3 target = origWorldPos + (obj->m_parent->m_worldTransform.rot * NiPoint3(cogOffsetX, cogOffsetY, cogOffsetZ));

#if DEBUG
    logger.Error("World Position: ");
    ShowPos(obj->m_worldTransform.pos);
    //logger.Error("Parent World Position difference: ");
    //ShowPos(obj->m_worldTransform.pos - obj->m_parent->m_worldTransform.pos);
    ShowPos(skelSpaceInvTransform * NiPoint3(cogOffsetX, cogOffsetY, cogOffsetZ));
    //logger.Error("Target Rotation:\n");
    //ShowRot(skelSpaceInvTransform);
    logger.Error("cogOffset x Transformation:");
    ShowPos(skelSpaceInvTransform * NiPoint3(cogOffsetX, 0, 0));
    logger.Error("cogOffset y Transformation:");
    ShowPos(skelSpaceInvTransform * NiPoint3(0, cogOffsetY, 0));
    logger.Error("cogOffset z Transformation:");
    ShowPos(skelSpaceInvTransform * NiPoint3(0, 0, cogOffsetZ));
#endif

    // diff is Difference in position between old and new world position
    // diff is world space
    NiPoint3 diff = (target - oldWorldPos) * forceMultiplier;

#if DEBUG
    logger.Error("Diff after gravity correction %f: ", varGravityCorrection);
    ShowPos(diff);
#endif

    if (fabs(diff.x) > DIFF_LIMIT || fabs(diff.y) > DIFF_LIMIT || fabs(diff.z) > DIFF_LIMIT)
    {
        logger.Error("%s: bone %s transform reset for actor %08x (diff: %.2f, %.2f, %.2f > limit %.1f)\n",
            __func__, boneName.c_str(), actor ? actor->formID : 0, diff.x, diff.y, diff.z, DIFF_LIMIT);
        obj->m_localTransform.pos = origLocalPos[boneName.c_str()][actor->formID];
        obj->m_localTransform.rot = origLocalRot[boneName.c_str()][actor->formID];

        oldWorldPos = target;
        oldWorldPosRot = origWorldPos;

        velocity = NiPoint3(0, 0, 0);
        time = clock();
        return;
    }

    // move the bones based on the supplied weightings
    // Convert the world translations into local coordinates

    NiMatrix43 rotateLinear;
    rotateLinear.SetEulerAngles(rotateLinearX * DEG_TO_RAD, rotateLinearY * DEG_TO_RAD, rotateLinearZ * DEG_TO_RAD);

    auto timeMultiplier = timeTick / (float)deltaT;

    // Transform to local space
    //diff = obj->m_parent->m_worldTransform.rot * (diff * timeMultiplier);
    diff = (diff * timeMultiplier);

    NiPoint3 posDelta = NiPoint3(0, 0, 0);

    // Compute the "Spring" Force

    NiPoint3 diff2(diff.x * diff.x * sgn(diff.x),
        diff.y * diff.y * sgn(diff.y),
        diff.z * diff.z * sgn(diff.z));

    float curStiffness = stiffness * dynamicStiffnessMul;
    float curStiffness2 = stiffness2 * dynamicStiffnessMul;
    float curDamping = damping * dynamicDampingMul;

    NiPoint3 force = (diff * curStiffness) + (diff2 * curStiffness2) - NiPoint3(0.0f, 0.0f, gravityBias);

#if DEBUG
    logger.Error("Diff2: ");
    ShowPos(diff2);
    logger.Error("Force with stiffness %f, stiffness2 %f, gravity bias %f: ", curStiffness, curStiffness2, gravityBias);
    ShowPos(force);
#endif

    do
    {
        // Assume mass is 1, so Accelleration is Force, can vary mass by changinf force
        //velocity = (velocity + (force * timeStep)) * (1 - (damping * timeStep));
        velocity = (velocity + (force * timeStep)) - (velocity * (curDamping * timeStep));

        // New position accounting for time
        posDelta += (velocity * timeStep);
        deltaT -= timeTick;
    } while (deltaT >= timeTick);

    NiPoint3 newPos = oldWorldPos + posDelta;
    NiPoint3 newPosRot = origWorldPosRot;

#if DEBUG
    //logger.Error("posDelta: ");
    //ShowPos(posDelta);
    logger.Error("newPos: ");
    ShowPos(newPos);
#endif
    // clamp the difference to stop the breast severely lagging at low framerates
    diff = newPos - target;

    // Idle deadband: actively extinguish micro-jitter when completely motionless
    float velSq = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
    float diffSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    if (velSq < 0.005f && diffSq < 0.002f)
    {
        velocity *= 0.5f;
        if (velSq < 0.0001f)
        {
            velocity = NiPoint3(0.0f, 0.0f, 0.0f);
        }
    }

    diff.x = clamp(diff.x, -maxOffsetX, maxOffsetX);
    diff.y = clamp(diff.y, -maxOffsetY, maxOffsetY);
    diff.z = clamp(diff.z, -maxOffsetZ, maxOffsetZ);

#if DEBUG
    logger.Error("diff from newPos: ");
    ShowPos(diff);
    //logger.Error("oldWorldPos: ");
    //ShowPos(oldWorldPos);
#endif

    // Project the world-space displacement vector into parent bone's local coordinate system (inverse of rot is Transpose())
    NiPoint3 localDiff = obj->m_parent->m_worldTransform.rot.Transpose() * diff;

    localDiff *= dynamicBreastScale;

    // Clamp against settings (in parent local space)
    localDiff.x = clamp(localDiff.x, -maxOffsetX, maxOffsetX);
    localDiff.y = clamp(localDiff.y, -maxOffsetY, maxOffsetY);
    localDiff.z = clamp(localDiff.z, -maxOffsetZ, maxOffsetZ);

    auto beforeLocalDiff = localDiff;

    localDiff.x += (beforeLocalDiff.y * linearSpreadforceXtoY) + (beforeLocalDiff.z * linearSpreadforceXtoZ);
    localDiff.y += (beforeLocalDiff.x * linearSpreadforceYtoX) + (beforeLocalDiff.z * linearSpreadforceYtoZ);
    localDiff.z += (beforeLocalDiff.x * linearSpreadforceZtoX) + (beforeLocalDiff.y * linearSpreadforceZtoY);

    localDiff.x = clamp(localDiff.x, -maxOffsetX, maxOffsetX);
    localDiff.y = clamp(localDiff.y, -maxOffsetY, maxOffsetY);
    localDiff.z = clamp(localDiff.z, -maxOffsetZ, maxOffsetZ);

    localDiff.x *= linearX;
    localDiff.y *= linearY;
    localDiff.z *= linearZ;

    // Store a copy of localDiff for transforming rotation motions
    auto rotDiff = localDiff;

    auto varGravitySupine = CalculateGravitySupine(actor);
    localDiff.x += varGravitySupine.x * linearX;
    localDiff.y += varGravitySupine.y * linearY;
    localDiff.z += varGravitySupine.z * linearZ;

    // Apply rotateLinear if configured
    localDiff = rotateLinear * localDiff;

    auto newLocalPos = origLocalPos[boneName.c_str()][actor->formID] + localDiff;

    if (gravityCorrection != 0.0f)
    {
        newLocalPos += rotateLinear * (obj->m_parent->m_worldTransform.rot.Transpose() * NiPoint3(0.0f, 0.0f, gravityCorrection * linearZ));
    }

    oldWorldPos = diff + target;

    //if (ContainsNoCase(std::string(boneName.c_str()), "Breast_CBP_R_02") ||
    //    ContainsNoCase(std::string(boneName.c_str()), "Breast_CBP_L_02")
    //    )
    //{
    //    //logger.Error("skeletonObj->m_localTransform.rot.Transpose()\n");
    //    //ShowRot(skeletonObj->m_localTransform.rot.Transpose());
    //    //logger.Error("rotatedInvWorldTrans\n");
    //    //ShowRot(rotatedInvWorldTrans);
    //    logger.Error("newWorldPos: ");
    //    ShowPos(newWorldPos);
    //    logger.Error("newLocalPos: ");
    //    ShowPos(newLocalPos);
    //}

#if DEBUG
    logger.Error("rotatedInvWorldTrans x=10 Transformation:");
    ShowPos(rotatedInvWorldTrans * NiPoint3(10, 0, 0));
    logger.Error("rotatedInvWorldTrans y=10 Transformation:");
    ShowPos(rotatedInvWorldTrans * NiPoint3(0, 10, 0));
    logger.Error("rotatedInvWorldTrans z=10 Transformation:");
    ShowPos(rotatedInvWorldTrans * NiPoint3(0, 0, 10));
    logger.Error("oldWorldPos: ");
    ShowPos(oldWorldPos);
    logger.Error("localTransform.pos: ");
    ShowPos(obj->m_localTransform.pos);
    logger.Error("rotDiff: ");
    ShowPos(rotDiff);

#endif

    obj->m_localTransform.pos = newLocalPos;

    // Calculate rotational motion
    if (absRotX) rotDiff.x = fabs(rotDiff.x);

    // Rotational motion scale
    rotDiff.x *= rotationalX;
    rotDiff.y *= rotationalY;
    rotDiff.z *= rotationalZ;


#if DEBUG
    logger.Error("localTransform.pos after: ");
    ShowPos(obj->m_localTransform.pos);
    logger.Error("origLocalPos:");
    ShowPos(origLocalPos[boneName.c_str()][actor->formID]);
    logger.Error("origLocalRot:");
    ShowRot(origLocalRot[boneName.c_str()][actor->formID]);

#endif
    // Rotate rotation according to settings.
    NiMatrix43 rotateRotation;
    rotateRotation.SetEulerAngles(rotateRotationX * DEG_TO_RAD,
        rotateRotationY * DEG_TO_RAD,
        rotateRotationZ * DEG_TO_RAD);

    NiMatrix43 standardRot;

    rotDiff = rotateRotation * rotDiff;
    standardRot.SetEulerAngles(rotDiff.x, rotDiff.y, rotDiff.z);
    // Calculate the new local rot as an offset from the original local rot
    obj->m_localTransform.rot = standardRot * origLocalRot[boneName.c_str()][actor->formID];

#if DEBUG
    logger.Error("end update()\n");
#endif

    //logger.Error("end update()\n");
    /*QueryPerformanceCounter(&endingTime);
    elapsedMicroseconds.QuadPart = endingTime.QuadPart - startingTime.QuadPart;
    elapsedMicroseconds.QuadPart *= 1000000000LL;
    elapsedMicroseconds.QuadPart /= frequency.QuadPart;
    _MESSAGE("Thing.update() Update Time = %lld ns\n", elapsedMicroseconds.QuadPart);*/

}