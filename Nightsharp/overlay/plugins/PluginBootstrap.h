#pragma once
// ============================================================================
// PluginBootstrap.h — Plugin registration entry point
//
// Ported from Old/plugins/PluginBootstrap.h.
// Registers all plugins with PluginManager, then triggers auto-load.
// ============================================================================

#include "PluginManager.h"

#include "Core/PlayerEventFilterPlugin.h"
#include "Core/SpellTrackingDebugPlugin.h"
#include "Utility/AttackRangeDrawPlugin.h"
#include "Utility/MovementStateDrawPlugin.h"
#include "Utility/NavGridDrawPlugin.h"
#include "Champion/EzrealSemiPlugin.h"
#include "Champion/EzrealMissileLifecyclePlugin.h"
#include "Champion/JaxSemiPlugin.h"
#include "Champion/XerathSemiPlugin.h"
#include "../SDK/Wrappers/SdkWrappersInit.h"
#include "../DebugLog.h"


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

        // Initialize default SDK Wrappers (TargetSelector & Orbwalker)
        NightSharpDebug::Logf("[PluginBootstrap] Initialize SDK Wrappers begin");
        ::SDK::SdkWrappers::Initialize();
        PluginRegistry::Register("Target Selector", "targetselector", PluginRegistry::PluginKind::SDK, true, PluginRegistry::PluginCategory::Core);
        PluginRegistry::Register("Orbwalker", "orbwalker", PluginRegistry::PluginKind::SDK, true, PluginRegistry::PluginCategory::Core);
        NightSharpDebug::Logf("[PluginBootstrap] Initialize SDK Wrappers complete");

        // ─── Core plugins ────────────────────────────────────────────────
        NightSharpDebug::Logf("[PluginBootstrap] Register core plugins begin");
        PluginManager::Get().Register<PlayerEventFilterPlugin>();
        PluginManager::Get().Register<SpellTrackingDebugPlugin>();
        NightSharpDebug::Logf("[PluginBootstrap] Register core plugins complete");

        NightSharpDebug::Logf("[PluginBootstrap] Register utility plugins begin");
        PluginManager::Get().Register<AttackRangeDrawPlugin>();
        PluginManager::Get().Register<MovementStateDrawPlugin>();
        PluginManager::Get().Register<NavGridDrawPlugin>();
        NightSharpDebug::Logf("[PluginBootstrap] Register utility plugins complete");

        NightSharpDebug::Logf("[PluginBootstrap] Register champion test plugins begin");
        PluginManager::Get().Register<EzrealSemiPlugin>();
        PluginManager::Get().Register<EzrealMissileLifecyclePlugin>();
        PluginManager::Get().Register<JaxSemiPlugin>();
        PluginManager::Get().Register<XerathSemiPlugin>();
        NightSharpDebug::Logf("[PluginBootstrap] Register champion test plugins complete");

        // Load persisted config (AlwaysLoad state from plugins.ini)
        NightSharpDebug::Logf("[PluginBootstrap] LoadConfig begin");
        PluginRegistry::LoadConfig();
        NightSharpDebug::Logf("[PluginBootstrap] LoadConfig complete");
        ApplyDebugAutoLoadOverrides();

        // Auto-load plugins that have AlwaysLoad=true
        NightSharpDebug::Logf("[PluginBootstrap] LoadAuto begin");
        PluginManager::Get().LoadAuto();
        NightSharpDebug::Logf("[PluginBootstrap] LoadAuto complete");
    }

    inline void Shutdown() {
        if (g_shutdown) {
            return;
        }
        g_shutdown = true;
        NightSharpDebug::Logf("[PluginBootstrap] Shutdown begin");
        ::SDK::SdkWrappers::Shutdown();
        PluginManager::Get().Shutdown();
        PluginRegistry::Reset();
        g_registered = false;
        NightSharpDebug::Logf("[PluginBootstrap] Shutdown complete");
    }

} // namespace PluginBootstrap
} // namespace Plugins
