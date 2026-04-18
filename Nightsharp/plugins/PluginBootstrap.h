#pragma once

#include "PluginManager.h"
#include "core/OrbwalkerPlugin.h"
#include "core/RenderTestPlugin.h"
#include "core/OffsetScannerPlugin.h"
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

    inline bool g_bootstrapRegistered = false;

    inline void EnsureRegistered() {
        if (g_bootstrapRegistered) {
            CrashTelemetry::AppendStageLine("[NightSharp][Bootstrap] EnsureRegistered::AlreadyRegistered\r\n");
            return;
        }
        g_bootstrapRegistered = true;

        auto& manager = PluginManager::Get();

        #define NS_REGISTER(NAME, T) \
            CrashTelemetry::SetStage("Bootstrap::Register::" NAME); \
            CrashTelemetry::AppendStageLine("[NightSharp][Bootstrap] Register::" NAME " begin\r\n"); \
            manager.Register<T>(); \
            CrashTelemetry::AppendStageLine("[NightSharp][Bootstrap] Register::" NAME " done\r\n");

        NS_REGISTER("OrbwalkerPlugin",     OrbwalkerPlugin)
        NS_REGISTER("RenderTestPlugin",    RenderTestPlugin)
        NS_REGISTER("OffsetScannerPlugin", OffsetScannerPlugin)
        NS_REGISTER("EzrealPlugin",        EzrealPlugin)
        NS_REGISTER("JinxPlugin",          JinxPlugin)
        NS_REGISTER("XerathPlugin",        XerathPlugin)
        NS_REGISTER("KarthusPlugin",       KarthusPlugin)
        NS_REGISTER("KogmawPlugin",        KogmawPlugin)
        NS_REGISTER("ViktorPlugin",        ViktorPlugin)
        NS_REGISTER("ZedPlugin",           ZedPlugin)
        NS_REGISTER("KalistaPlugin",       KalistaPlugin)
        NS_REGISTER("RTXPowerPlugin",      RTXPowerPlugin)

        #undef NS_REGISTER
    }

} // namespace PluginBootstrap
} // namespace Plugins
