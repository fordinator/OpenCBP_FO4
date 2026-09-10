#include "common/ITypes.h"
#include <string>
#include "f4se/PluginAPI.h"
#include "f4se_common/f4se_version.h"
#include "f4se/GameAPI.h"
#include "f4se/GameEvents.h"
#include "log.h"
#include "config.h"
#include "FrameHook.h"
#include "PapyrusOCBP.h"

// This build targets exactly one runtime. Everything that touches game memory
// goes through the F4SE 0.7.9 headers, which are themselves locked to this exe,
// so we tell F4SE the truth: not version independent, 1.11.240 only.
#define OCBP_TARGET_RUNTIME     RUNTIME_VERSION_1_11_240
#define OCBP_TARGET_RUNTIME_STR "1.11.240"
#define OCBP_PLUGIN_VERSION     26

bool RegisterFuncs(VirtualMachine* vm);

PluginHandle            g_pluginHandle = kPluginHandle_Invalid;
F4SEMessagingInterface* g_messagingInterface = nullptr;
F4SETaskInterface*      g_task = nullptr;
F4SEPapyrusInterface*   g_papyrus = nullptr;
UInt32                  g_runtimeVersion = 0;

void MessageHandler(F4SEMessagingInterface::Message* msg)
{
    switch (msg->type)
    {
    case F4SEMessagingInterface::kMessage_GameDataReady:
    {
        logger.Error("kMessage_GameDataReady: loading config\n");
        LoadConfig();
        logger.Error("kMessage_GameDataReady: installing frame hook\n");
        InstallFrameHook(g_task, g_runtimeVersion, hookMode);
        logger.Error("OpenCBP load complete (hook=%s)\n", FrameHookModeName(ActiveFrameHookMode()));
    }
    break;
    case F4SEMessagingInterface::kMessage_GameLoaded:   logger.Info("kMessage_GameLoaded\n");   break;
    case F4SEMessagingInterface::kMessage_NewGame:      logger.Info("kMessage_NewGame\n");      break;
    case F4SEMessagingInterface::kMessage_PreLoadGame:  logger.Info("kMessage_PreLoadGame\n");  break;
    case F4SEMessagingInterface::kMessage_PostLoad:     logger.Info("kMessage_PostLoad\n");     break;
    case F4SEMessagingInterface::kMessage_PostPostLoad: logger.Info("kMessage_PostPostLoad\n"); break;
    case F4SEMessagingInterface::kMessage_PostLoadGame: logger.Info("kMessage_PostLoadGame\n"); break;
    case F4SEMessagingInterface::kMessage_PreSaveGame:  logger.Info("kMessage_PreSaveGame\n");  break;
    case F4SEMessagingInterface::kMessage_PostSaveGame: logger.Info("kMessage_PostSaveGame\n"); break;
    case F4SEMessagingInterface::kMessage_DeleteGame:   logger.Info("kMessage_DeleteGame\n");   break;
    case F4SEMessagingInterface::kMessage_InputLoaded:  logger.Info("kMessage_InputLoaded\n");  break;
    }
}

extern "C"
{
    __declspec(dllexport) F4SEPluginVersionData F4SEPlugin_Version =
    {
        F4SEPluginVersionData::kVersion,

        OCBP_PLUGIN_VERSION,
        "OCBP plugin",
        "takosako, fordinator",

        0,  // addressIndependence: none. The Address Library resolves OUR hook,
            // but the F4SE game headers we call are version-locked, so we do
            // not claim independence.
        0,  // structureIndependence: none, same reason.
        { OCBP_TARGET_RUNTIME, 0 },     // 1.11.240 only

        PACKED_F4SE_VERSION,            // built against F4SE 0.7.9
        0, 0, { 0 },
    };
};

extern "C"
{
    bool F4SEPlugin_Load(const F4SEInterface* f4se)
    {
        logger.Error("OpenCBP Physics F4SE plugin v%u, built for Fallout 4 %s / F4SE %s\n",
                     OCBP_PLUGIN_VERSION, OCBP_TARGET_RUNTIME_STR, CURRENT_RELEASE_F4SE_STR);

        g_pluginHandle = f4se->GetPluginHandle();

        if (f4se->isEditor)
        {
            logger.Error("loaded in editor, marking as incompatible\n");
            return false;
        }
        if (f4se->runtimeVersion != OCBP_TARGET_RUNTIME)
        {
            logger.Error("unsupported runtime version %08X (this build is for %s only)\n",
                         f4se->runtimeVersion, OCBP_TARGET_RUNTIME_STR);
            return false;
        }
        g_runtimeVersion = f4se->runtimeVersion;

        g_task = (F4SETaskInterface*)f4se->QueryInterface(kInterface_Task);
        if (!g_task)
        {
            logger.Error("couldn't get task interface\n");
            return false;
        }

        g_messagingInterface = (F4SEMessagingInterface*)f4se->QueryInterface(kInterface_Messaging);
        if (!g_messagingInterface)
        {
            logger.Error("couldn't get messaging interface\n");
            return false;
        }

        g_papyrus = (F4SEPapyrusInterface*)f4se->QueryInterface(kInterface_Papyrus);
        if (g_papyrus)
            g_papyrus->Register(RegisterFuncs);
        else
            logger.Error("couldn't get papyrus interface; OCBP_API natives unavailable\n");

        g_messagingInterface->RegisterListener(g_pluginHandle, "F4SE", MessageHandler);

        logger.Error("F4SEPlugin_Load complete\n");
        return true;
    }
};

bool RegisterFuncs(VirtualMachine* vm)
{
    papyrusOCBP::RegisterFuncs(vm);
    return true;
}

BOOL WINAPI DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD     fdwReason,
    _In_ LPVOID    lpvReserved
)
{
    return true;
}
