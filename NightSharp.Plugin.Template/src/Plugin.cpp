#include <Windows.h>

#include <NightSharp.SDK.h>

namespace {

bool g_enabled = true;
int g_debugLevel = 1;
unsigned g_updateCount = 0;

bool NIGHTSHARP_PLUGIN_CALL PluginCanLoad() {
    return true;
}

void NIGHTSHARP_PLUGIN_CALL PluginOnLoad() {
    g_enabled = true;
    g_debugLevel = 1;
}

void NIGHTSHARP_PLUGIN_CALL PluginOnUnload() {
    g_updateCount = 0;
}

void NIGHTSHARP_PLUGIN_CALL PluginOnUpdate() {
    ++g_updateCount;

    if (!g_enabled) {
        return;
    }

    // Add gameplay logic here. Keep it SDK-facing: use Game, GameObjects,
    // Spell, Prediction, Events, Orbwalker, TargetSelector.
}

void NIGHTSHARP_PLUGIN_CALL PluginOnRender() {
    if (!g_enabled) {
        return;
    }
}

void NIGHTSHARP_PLUGIN_CALL PluginOnMenu() {
    (void)g_updateCount;
    (void)g_debugLevel;
}

const auto kDescriptor = NIGHTSHARP_PLUGIN_DESCRIPTOR(
    "Template Plugin",
    "template.plugin",
    "NightSharp",
    NightSharp::Plugin::Category::Utility,
    nullptr,
    true);

} // namespace

NIGHTSHARP_PLUGIN_EXPORT(
    kDescriptor,
    &PluginCanLoad,
    &PluginOnLoad,
    &PluginOnUnload,
    &PluginOnUpdate,
    &PluginOnRender,
    &PluginOnMenu)

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
