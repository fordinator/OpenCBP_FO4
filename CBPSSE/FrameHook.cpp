#include "FrameHook.h"
#include "AddressLibrary.h"
#include "OCBP_AddressIDs.h"
#include "log.h"

#include "../detourxs-master/detourxs.h"
#include "f4se/GameThreads.h"
#include "f4se_common/Relocation.h"

#include <cstring>

void UpdateActors();    // scan.cpp

namespace
{
    FrameHookMode s_active = FrameHookMode::None;

    // --- Address Library path -------------------------------------------------
    typedef void (*_ProcessEventQueue_Internal)(void* thisPtr);
    _ProcessEventQueue_Internal s_origProcessEventQueue = nullptr;
    DetourXS s_detour;

    void hk_ProcessEventQueue_Internal(void* thisPtr)
    {
        s_origProcessEventQueue(thisPtr);
        UpdateActors();
    }

    bool InstallAddressLibraryHook(UInt32 runtimeVersion)
    {
        if (!g_addressLibrary.IsLoaded() && !g_addressLibrary.Load(runtimeVersion))
            return false;

        const std::uint64_t id = OCBP_AddressIDs::kProcessEventQueue_Internal;

        if (id == 0)
        {
            // Bootstrap for a freshly-targeted game version: derive the ID from
            // the RVA F4SE publishes, print it, and refuse to hook on a guess.
            const std::uint64_t rva = OCBP_AddressIDs::kBootstrapRVA_ProcessEventQueue_Internal_1_11_240;
            std::uint64_t found = 0;
            if (g_addressLibrary.FindIdByOffset(rva, found))
            {
                logger.Error("FrameHook: kProcessEventQueue_Internal is unset. Reverse lookup of RVA 0x%08llX -> ID %llu. "
                             "Paste that into OCBP_AddressIDs.h and rebuild.\n",
                             (unsigned long long)rva, (unsigned long long)found);
            }
            else
            {
                logger.Error("FrameHook: kProcessEventQueue_Internal is unset and RVA 0x%08llX is not in %s.\n",
                             (unsigned long long)rva, g_addressLibrary.Path().c_str());
            }
            return false;
        }

        const std::uintptr_t target = g_addressLibrary.Resolve(id);
        if (!target)
        {
            logger.Error("FrameHook: ID %llu not present in %s - the Address Library is older than this plugin.\n",
                         (unsigned long long)id, g_addressLibrary.Path().c_str());
            return false;
        }

        logger.Error("FrameHook: ProcessEventQueue_Internal ID %llu -> RVA 0x%08llX\n",
                     (unsigned long long)id, (unsigned long long)(target - RelocationManager::s_baseAddr));

        if (!s_detour.Create((LPVOID)target, (LPVOID)hk_ProcessEventQueue_Internal, (LPVOID*)&s_origProcessEventQueue)
            || !s_origProcessEventQueue)
        {
            logger.Error("FrameHook: DetourXS failed to install at 0x%p\n", (void*)target);
            return false;
        }
        return true;
    }

    // --- F4SE task path -------------------------------------------------------
    class UpdateActorsTask : public ITaskDelegate
    {
    public:
        virtual void Run() override { UpdateActors(); }
    };

    bool InstallTaskHook(F4SETaskInterface* task)
    {
        if (!task)
        {
            logger.Error("FrameHook: no F4SETaskInterface\n");
            return false;
        }
        if (task->interfaceVersion < 2 || !task->AddTaskPermanent)
        {
            logger.Error("FrameHook: F4SETaskInterface v%u has no AddTaskPermanent (need F4SE 0.7.x, interface v2)\n",
                         task->interfaceVersion);
            return false;
        }
        // Owned by F4SE for the life of the process; never freed on purpose.
        task->AddTaskPermanent(new UpdateActorsTask());
        return true;
    }
}

FrameHookMode ParseFrameHookMode(const char* text)
{
    if (!text || !*text) return FrameHookMode::Auto;
    if (!_stricmp(text, "auto")) return FrameHookMode::Auto;
    if (!_stricmp(text, "addresslib") || !_stricmp(text, "addresslibrary")) return FrameHookMode::AddressLibrary;
    if (!_stricmp(text, "f4setask") || !_stricmp(text, "task")) return FrameHookMode::F4SETask;
    logger.Error("FrameHook: unknown hookMode '%s', using auto\n", text);
    return FrameHookMode::Auto;
}

const char* FrameHookModeName(FrameHookMode mode)
{
    switch (mode)
    {
    case FrameHookMode::Auto:           return "auto";
    case FrameHookMode::AddressLibrary: return "addresslib";
    case FrameHookMode::F4SETask:       return "f4setask";
    default:                            return "none";
    }
}

FrameHookMode ActiveFrameHookMode()
{
    return s_active;
}

bool InstallFrameHook(F4SETaskInterface* task, UInt32 runtimeVersion, FrameHookMode requested)
{
    if (s_active != FrameHookMode::None)
    {
        logger.Error("FrameHook: already installed (%s), ignoring\n", FrameHookModeName(s_active));
        return true;
    }

    logger.Error("FrameHook: requested mode %s\n", FrameHookModeName(requested));

    if (requested == FrameHookMode::AddressLibrary || requested == FrameHookMode::Auto)
    {
        if (InstallAddressLibraryHook(runtimeVersion))
        {
            s_active = FrameHookMode::AddressLibrary;
            logger.Error("FrameHook: active mode addresslib\n");
            return true;
        }
        if (requested == FrameHookMode::AddressLibrary)
        {
            logger.Error("FrameHook: addresslib requested and failed - physics disabled. Set hookMode=auto to allow fallback.\n");
            return false;
        }
        logger.Error("FrameHook: addresslib unavailable, falling back to f4setask\n");
    }

    if (InstallTaskHook(task))
    {
        s_active = FrameHookMode::F4SETask;
        logger.Error("FrameHook: active mode f4setask\n");
        return true;
    }

    logger.Error("FrameHook: no hook installed - physics disabled\n");
    return false;
}
