#include <Windows.h>

#include <NightSharp.SDK.h>

namespace {

bool g_enabled = true;
bool g_drawStatus = true;
unsigned g_updateCount = 0;
unsigned g_renderCount = 0;

bool NIGHTSHARP_PLUGIN_CALL PluginCanLoad() {
    return true;
}

void NIGHTSHARP_PLUGIN_CALL PluginOnLoad() {
    g_enabled = true;
    g_drawStatus = true;
}

void NIGHTSHARP_PLUGIN_CALL PluginOnUnload() {
    g_updateCount = 0;
    g_renderCount = 0;
}

void NIGHTSHARP_PLUGIN_CALL PluginOnUpdate() {
    ++g_updateCount;
    if (!g_enabled) {
        return;
    }

    const auto player = GameObjects::Player();
    if (!player.IsValid()) {
        return;
    }

    (void)Game::Time();
    (void)player.Position();
    (void)GameObjects::EnemyHeroes().size();
}

void NIGHTSHARP_PLUGIN_CALL PluginOnRender() {
    ++g_renderCount;
    if (!g_enabled || !g_drawStatus) {
        return;
    }

    (void)GameObjects::EnemyHeroes().size();
}

void NIGHTSHARP_PLUGIN_CALL PluginOnMenu() {}

const auto kDescriptor = NIGHTSHARP_PLUGIN_DESCRIPTOR(
    "Sample Utility",
    "sample.utility",
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
