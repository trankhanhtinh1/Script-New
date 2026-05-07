#pragma once

#include "PluginManager.h"
#include "core/TargetSelectorPlugin.h"
#include "core/OrbwalkerPlugin.h"
#include "core/OffsetInspectorPlugin.h"
#include "7UPAIO/EzrealPlugin.h"

// NOTE: Built-in plugins (RenderTestPlugin, EzrealPlugin, …) lived in the
// deleted core/ + sdk/ tree, so there is nothing to register yet. The
// function is kept as an explicit no-op to preserve the call site in
// Overlay::Run() without re-plumbing once new plugins are re-added.

namespace Plugins {
namespace PluginBootstrap {

    inline void EnsureRegistered() {
        static bool s_registered = false;
        if (s_registered) {
            return;
        }
        s_registered = true;

        PluginManager::Get().Register<TargetSelectorPlugin>();
        PluginManager::Get().Register<OrbwalkerPlugin>();
        PluginManager::Get().Register<OffsetInspectorPlugin>();
        PluginManager::Get().Register<SevenUPAIO::EzrealPlugin>();
    }

} // namespace PluginBootstrap
} // namespace Plugins
