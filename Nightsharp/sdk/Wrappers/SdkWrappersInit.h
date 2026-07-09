#pragma once

#include "../Bootstrap.h"

namespace SDK::SdkWrappers {

inline Menu* g_tsRootMenu = nullptr;
inline TargetSelector* g_targetSelector = nullptr;
inline Orbwalker* g_orbwalker = nullptr;
inline bool g_initialized = false;

inline const char* RootMenuName() {
    return "SDK Wrappers";
}

inline void Initialize() {
    if (g_initialized) return;
    g_initialized = true;

    Bootstrap::Init();
    g_tsRootMenu = Variables::EnsoulSharpMenu;
    g_targetSelector = Variables::TargetSelector;
    g_orbwalker = Variables::Orbwalker;
}

inline void Shutdown() {
    if (!g_initialized) return;

    Bootstrap::Shutdown();
    g_orbwalker = nullptr;
    g_targetSelector = nullptr;
    g_tsRootMenu = nullptr;
    g_initialized = false;
}

// Runtime Load/Unload cho registry row "orbwalker" (Plugin Manager):
// Unload = suspend bản SDK (unhook event + ẩn menu), Load = resume.
// Resume bị từ chối khi một orbwalker khác (vd. OrbwalkerKuro) đang override,
// để không bao giờ có hai orbwalker cùng chạy.
inline bool ResumeSdkOrbwalkerRuntime(void*) {
    if (Orbwalker::CurrentOrbwalkerName() != "SDK") {
        return false;
    }
    if (IOrbwalker* impl = Orbwalker::GetOrbwalker("SDK")) {
        impl->Resume();
        return true;
    }
    return false;
}

inline bool SuspendSdkOrbwalkerRuntime(void*) {
    if (IOrbwalker* impl = Orbwalker::GetOrbwalker("SDK")) {
        impl->Suspend();
        return true;
    }
    return false;
}

} // namespace SDK::SdkWrappers
