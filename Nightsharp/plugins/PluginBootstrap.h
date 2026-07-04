#pragma once
// ============================================================================
// PluginBootstrap.h — Plugin registration entry point
//
// Ported from Old/plugins/PluginBootstrap.h.
// Registers all plugins with PluginManager, then triggers auto-load.
// ============================================================================

#include "PluginManager.h"
#include "ExternalPluginLoader.h"

#include "Core/ObjectLifecycleTestPlugins.h"
#include "Core/PlayerEventFilterPlugin.h"
#include "Core/SpellTrackingDebugPlugin.h"
#include "Utility/AttackRangeDrawPlugin.h"
#include "Utility/MovementStateDrawPlugin.h"
#include "Utility/NavGridDrawPlugin.h"
#include "Utility/ObjectDefinitionDrawPlugin.h"
#include "Utility/VisibilityInvulnerabilityOffsetPlugin.h"
#include "Champion/EzrealSemiPlugin.h"
#include "Champion/EzrealMissileLifecyclePlugin.h"
#include "Champion/JaxSemiPlugin.h"
#include "Champion/XerathSemiPlugin.h"
#include "../SDK/Wrappers/SdkWrappersInit.h"
#include "../DebugLog.h"
#include "../menu/MenuSettingsConfig.h"

#ifndef NIGHTSHARP_ENABLE_SDK_WRAPPERS
#define NIGHTSHARP_ENABLE_SDK_WRAPPERS 1
#endif

#ifndef NIGHTSHARP_ENABLE_SAMPLE_PLUGINS
#define NIGHTSHARP_ENABLE_SAMPLE_PLUGINS 1
#endif

#ifndef NIGHTSHARP_ENABLE_LIFECYCLE_TEST_PLUGINS
#define NIGHTSHARP_ENABLE_LIFECYCLE_TEST_PLUGINS 1
#endif


namespace Plugins {
namespace PluginBootstrap {

    inline bool g_registered = false;
    inline bool g_shutdown = false;

    inline void ApplyDebugAutoLoadOverrides() {
        static constexpr const char* kDebugPluginIds[] = {
            "core.player_event_filter",
            "core.spell_tracking_debug",
            "utility.attack_range_draw_test",
            "utility.movement_state_draw",
            "utility.navgrid_wall_brush_draw",
            "champion.ezreal_cast_test",
            "champion.ezreal_q_missile_lifecycle",
            "champion.jax_cast_test",
            "champion.xerath_cast_test",
            "core.object_delete_lifecycle_test",
            "utility.object_definition_draw",
        };

        for (const char* id : kDebugPluginIds) {
            const int idx = PluginRegistry::FindByInternalId(id);
            if (idx < 0) {
                continue;
            }

            auto& entry = PluginRegistry::Plugins[idx];
            if (entry.AlwaysLoad) {
                NightSharpDebug::Logf("[PluginBootstrap] Suppressing debug/test auto-load id=%s",
                                      id);
            }
            entry.AlwaysLoad = false;
            entry.Loaded = false;
        }
    }

    inline void EnsureRegistered() {
        if (g_registered) {
            NightSharpDebug::Logf("[PluginBootstrap] EnsureRegistered skipped: already registered");
            return;
        }
        g_registered = true;
        g_shutdown = false;

#if NIGHTSHARP_ENABLE_SDK_WRAPPERS
        // Initialize default SDK Wrappers.
        NightSharpDebug::Logf("[PluginBootstrap] Initialize SDK Wrappers begin");
        ::SDK::SdkWrappers::Initialize();
        PluginRegistry::Register("Orbwalker", "orbwalker", PluginRegistry::PluginKind::SDK, true, PluginRegistry::PluginCategory::Core);
        PluginRegistry::Register("Target Selector", "targetselector", PluginRegistry::PluginKind::SDK, true, PluginRegistry::PluginCategory::Core);        
        NightSharpDebug::Logf("[PluginBootstrap] Initialize SDK Wrappers complete");
#else
        NightSharpDebug::Logf("[PluginBootstrap] SDK Wrappers disabled for FPS test");
#endif

        // ─── Core plugins ────────────────────────────────────────────────
#if NIGHTSHARP_ENABLE_LIFECYCLE_TEST_PLUGINS
        NightSharpDebug::Logf("[PluginBootstrap] Register lifecycle test plugins begin");
        PluginManager::Get().Register<ObjectDeleteLifecycleTestPlugin>();
        NightSharpDebug::Logf("[PluginBootstrap] Register lifecycle test plugins complete");
#endif

#if NIGHTSHARP_ENABLE_SAMPLE_PLUGINS
        NightSharpDebug::Logf("[PluginBootstrap] Register core plugins begin");
        PluginManager::Get().Register<PlayerEventFilterPlugin>();
        PluginManager::Get().Register<SpellTrackingDebugPlugin>();
        NightSharpDebug::Logf("[PluginBootstrap] Register core plugins complete");

        NightSharpDebug::Logf("[PluginBootstrap] Register utility plugins begin");
        PluginManager::Get().Register<AttackRangeDrawPlugin>();
        PluginManager::Get().Register<MovementStateDrawPlugin>();
        PluginManager::Get().Register<NavGridDrawPlugin>();
        PluginManager::Get().Register<ObjectDefinitionDrawPlugin>();
        PluginManager::Get().Register<VisibilityInvulnerabilityOffsetPlugin>();
        NightSharpDebug::Logf("[PluginBootstrap] Register utility plugins complete");

        NightSharpDebug::Logf("[PluginBootstrap] Register champion test plugins begin");
        PluginManager::Get().Register<EzrealSemiPlugin>();
        PluginManager::Get().Register<EzrealMissileLifecyclePlugin>();
        PluginManager::Get().Register<JaxSemiPlugin>();
        PluginManager::Get().Register<XerathSemiPlugin>();
        NightSharpDebug::Logf("[PluginBootstrap] Register champion test plugins complete");

#else
        NightSharpDebug::Logf("[PluginBootstrap] Sample/debug plugins disabled for FPS test");
#endif

        NightSharpDebug::Logf("[PluginBootstrap] Register external release plugins begin");
        ExternalPluginLoader::RegisterReleasePlugins();
        NightSharpDebug::Logf("[PluginBootstrap] Register external release plugins complete");

        NightSharpDebug::Logf("[PluginBootstrap] Register external dev plugins begin");
        ExternalPluginLoader::RegisterDevPlugins();
        NightSharpDebug::Logf("[PluginBootstrap] Register external dev plugins complete");

        NightSharpDebug::Logf("[PluginBootstrap] LoadConfig begin");
        PluginRegistry::LoadConfig();
        NightSharpDebug::Logf("[PluginBootstrap] LoadConfig complete");
        ApplyDebugAutoLoadOverrides();

        NightSharpDebug::Logf("[PluginBootstrap] LoadAuto begin");
        PluginManager::Get().LoadAuto();
        NightSharpDebug::Logf("[PluginBootstrap] LoadAuto complete");

        NightSharpDebug::Logf("[PluginBootstrap] Apply menu settings config begin");
        NightSharpMenu::MenuSettingsConfig::ApplyNewMenuValues();
        NightSharpDebug::Logf("[PluginBootstrap] Apply menu settings config complete");
    }

    inline void Shutdown() {
        if (g_shutdown) {
            return;
        }
        g_shutdown = true;
        NightSharpDebug::Logf("[PluginBootstrap] Shutdown begin");
        NightSharpMenu::MenuSettingsConfig::SaveAllNow();
        ::SDK::SdkWrappers::Shutdown();
        PluginManager::Get().Shutdown();
        ExternalPluginLoader::Shutdown();
        PluginRegistry::Reset();
        g_registered = false;
        NightSharpDebug::Logf("[PluginBootstrap] Shutdown complete");
    }

} // namespace PluginBootstrap
} // namespace Plugins
