#pragma once

#include "TargetSelector/TargetSelector.h"
#include "Orbwalking/Orbwalker.h"
#include "../UI/UI.h"

namespace SDK::SdkWrappers {

inline Menu* g_rootMenu = nullptr;
inline TargetSelector* g_targetSelector = nullptr;
inline Orbwalker* g_orbwalker = nullptr;
inline bool g_initialized = false;

inline const char* RootMenuName() {
    return "SDK Wrappers";
}

inline void Initialize() {
    if (g_initialized) return;
    g_initialized = true;

    g_rootMenu = new Menu("sdksystem", RootMenuName(), true);

    g_targetSelector = new TargetSelector(g_rootMenu);
    g_orbwalker = new Orbwalker(g_rootMenu);

    g_rootMenu->Attach();
}

inline void Shutdown() {
    if (!g_initialized) return;

    if (g_orbwalker) {
        g_orbwalker->SetEnabled(false);
    }

    if (g_rootMenu) {
        MenuManager::Instance().Remove(g_rootMenu);
        delete g_rootMenu;
        g_rootMenu = nullptr;
    }

    delete g_orbwalker;
    delete g_targetSelector;
    g_orbwalker = nullptr;
    g_targetSelector = nullptr;
    g_initialized = false;
}

} // namespace SDK::SdkWrappers
