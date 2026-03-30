#pragma once

#include "PluginManager.h"
#include "core/RenderTestPlugin.h"

namespace Plugins {
namespace PluginBootstrap {

    inline void EnsureRegistered() {
        static bool registered = false;
        if (registered) return;
        registered = true;

        auto& manager = PluginManager::Get();
        manager.Register<RenderTestPlugin>();
    }

} // namespace PluginBootstrap
} // namespace Plugins
