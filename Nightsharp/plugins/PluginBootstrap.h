#pragma once

#include "PluginManager.h"
#include "core/TargetSelectorPlugin.h"
#include "core/OrbwalkerPlugin.h"
#include "7UPAIO/EzrealPlugin.h"

#ifndef NIGHTSHARP_ENABLE_EZREAL_MISSILE_DEBUG
#define NIGHTSHARP_ENABLE_EZREAL_MISSILE_DEBUG 0
#endif

#if NIGHTSHARP_ENABLE_EZREAL_MISSILE_DEBUG
#include "debug/EzrealMissileDebugPlugin.h"
#endif

#ifndef NIGHTSHARP_ENABLE_EZEVADE
#define NIGHTSHARP_ENABLE_EZEVADE 1
#endif

#if NIGHTSHARP_ENABLE_EZEVADE
#include "EzEvade/Program.h"
#endif

#ifndef NIGHTSHARP_ENABLE_OFFSET_INSPECTOR
#define NIGHTSHARP_ENABLE_OFFSET_INSPECTOR 0
#endif

#if NIGHTSHARP_ENABLE_OFFSET_INSPECTOR
#include "core/OffsetInspectorPlugin.h"
#endif

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
#if NIGHTSHARP_ENABLE_OFFSET_INSPECTOR
        PluginManager::Get().Register<OffsetInspectorPlugin>();
#endif
        PluginManager::Get().Register<SevenUPAIO::EzrealPlugin>();
#if NIGHTSHARP_ENABLE_EZEVADE
        PluginManager::Get().Register<EzEvadePlugin::Plugin>();
#endif
#if NIGHTSHARP_ENABLE_EZREAL_MISSILE_DEBUG
        PluginManager::Get().Register<Debug::EzrealMissileDebugPlugin>();
#endif
    }

} // namespace PluginBootstrap
} // namespace Plugins
