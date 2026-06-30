#pragma once

#include "TargetSelector/TargetSelector.h"
#include "Orbwalking/Orbwalker.h"
#include "../UI/UI.h"

namespace SDK::SdkWrappers {

inline Menu* g_tsRootMenu = nullptr;
inline Menu* g_orbRootMenu = nullptr;
inline TargetSelector* g_targetSelector = nullptr;
inline Orbwalker* g_orbwalker = nullptr;
inline bool g_initialized = false;

inline const char* RootMenuName() {
    return "SDK Wrappers";
}

inline void Initialize() {
    if (g_initialized) return;
    g_initialized = true;

    g_tsRootMenu = new Menu("targetselector", "Target Selector", true);
    g_targetSelector = new TargetSelector(g_tsRootMenu);
    g_tsRootMenu->Attach();

    g_orbRootMenu = new Menu("orbwalker", "Orbwalker", true);
    g_orbwalker = new Orbwalker(g_orbRootMenu);
    g_orbRootMenu->Attach();
}

inline void Shutdown() {
    if (!g_initialized) return;

    if (g_orbwalker) {
        g_orbwalker->SetEnabled(false);
    }

    delete g_orbwalker;
    delete g_targetSelector;
    g_orbwalker = nullptr;
    g_targetSelector = nullptr;

    if (g_orbRootMenu) {
        MenuManager::Instance().Remove(g_orbRootMenu);
        delete g_orbRootMenu;
        g_orbRootMenu = nullptr;
    }

    if (g_tsRootMenu) {
        MenuManager::Instance().Remove(g_tsRootMenu);
        delete g_tsRootMenu;
        g_tsRootMenu = nullptr;
    }
    g_initialized = false;
}

} // namespace SDK::SdkWrappers
