#pragma once

#include "PluginManager.h"
#include "core/RenderTestPlugin.h"
#include "champions/EzrealCastHotkeyTestPlugin.h"
#include "champions/EzrealPlugin.h"

namespace Plugins {
namespace PluginBootstrap {

    inline void EnsureRegistered() {
        static bool registered = false;
        if (registered) return;
        registered = true;

        auto& manager = PluginManager::Get();
        manager.Register<RenderTestPlugin>();
        manager.Register<EzrealCastHotkeyTestPlugin>();
        manager.Register<EzrealPlugin>();
    }

} // namespace PluginBootstrap
} // namespace Plugins
