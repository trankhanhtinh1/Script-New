#pragma once

#include "PluginManager.h"
#include "core/OrbwalkerPlugin.h"
#include "core/TargetSelectorPlugin.h"
#include "core/TargetSelectorPlugin2.h"
#include "core/RenderTestPlugin.h"
#include "champions/EzrealPlugin.h"
#include "champions/JinxPlugin.h"
#include "champions/XerathPlugin.h"
#include "champions/KarthusPlugin.h"
#include "champions/KogmawPlugin.h"
#include "champions/ViktorPlugin.h"
#include "champions/ZedPlugin.h"
#include "champions/KalistaPlugin.h"
#include "champions/RTXPower/Ezreal.h"

namespace Plugins {
namespace PluginBootstrap {

    inline void EnsureRegistered() {
        static bool registered = false;
        if (registered) return;
        registered = true;

        auto& manager = PluginManager::Get();
        manager.Register<OrbwalkerPlugin>();
        manager.Register<TargetSelectorPlugin>();
        manager.Register<TargetSelectorPlugin2>();
        manager.Register<RenderTestPlugin>();
        manager.Register<EzrealPlugin>();
        manager.Register<JinxPlugin>();
        manager.Register<XerathPlugin>();
        manager.Register<KarthusPlugin>();
        manager.Register<KogmawPlugin>();
        manager.Register<ViktorPlugin>();
        manager.Register<ZedPlugin>();
        manager.Register<KalistaPlugin>();
        manager.Register<RTXPowerPlugin>();
    }

} // namespace PluginBootstrap
} // namespace Plugins
