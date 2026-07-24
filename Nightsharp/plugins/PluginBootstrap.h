#pragma once
// ============================================================================
// PluginBootstrap.h — Plugin registration entry point
//
// Ported from Old/plugins/PluginBootstrap.h.
// Registers all plugins with PluginManager, then triggers auto-load.
// ============================================================================

#include "PluginManager.h"

#include "Core/ObjectLifecycleTestPlugins.h"
#include "Core/OrbwalkerKuro/OrbwalkerKuroPlugin.h"
#include "Core/TargetSelectorImpulse/TargetSelectorImpulsePlugin.h"
#include "Core/PlayerBuffDebugPlugin.h"
#include "Core/PlayerEventFilterPlugin.h"
#include "Core/SpellTrackingDebugPlugin.h"
#include "Utility/AttackRangeDrawPlugin.h"
#include "Utility/IsDeadDebugPlugin.h"
#include "Utility/MovementStateDrawPlugin.h"
#include "Utility/NavGridDrawPlugin.h"
#include "Utility/ObjectDefinitionDrawPlugin.h"
#include "Utility/VisibilityInvulnerabilityOffsetPlugin.h"
#include "Utility/YasuoWallDebugPlugin.h"
#include "Utility/DeveloperToolsPlugin.h"
#include "Champion/EzrealSemiPlugin.h"
#include "Champion/EzrealMissileLifecyclePlugin.h"
#include "Champion/JaxSemiPlugin.h"
#include "Champion/XerathSemiCastNew.h"
#include "Champion/XerathSemiPlugin.h"
#include "Champion/7UPAIO/7UPAIO.h"
#include "Champion/KuroAIO/KuroAIO.h"
#include "Champion/SharpShooterAIO/SharpShooterAIO.h"
#include "Champion/ziblldev9898/ziblldev9898.h"
#include "EzEvade/EzEvadePlugin.h"
#include "KuroEvade/KuroEvadePlugin.h"
#include "ZDEvade/ZDEvade.h"
#include "ZDPrediction/ZDPrediction.h"
#include "../SDK/Wrappers/SdkWrappersInit.h"
#include "../menu/ConfigStore.h"
#include "../menu/NightSharpMenu.h"
#include "../DebugLog.h"

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

    // Champion-name provider for ConfigStore's per-champion file naming.
    // Returns the local player's champion (empty until resolved at session start).
    inline const char* ChampionNameProvider() {
        const std::string& name = ::SDK::GameObject::GetCachedChampionName();
        return name.c_str();
    }

    inline void ApplyDebugAutoLoadOverrides() {
        static constexpr const char* kDebugPluginIds[] = {
            "core.player_event_filter",
            "core.player_buff_debug",
            "core.spell_tracking_debug",
            "utility.attack_range_draw_test",
            "utility.movement_state_draw",
            "utility.navgrid_wall_brush_draw",
            "champion.ezreal_cast_test",
            "champion.ezreal_q_missile_lifecycle",
            "champion.jax_cast_test",
            "champion.xerath_semi_cast_new",
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
        const int orbwalkerRegistryIdx = PluginRegistry::Register("Orbwalker", "orbwalker", PluginRegistry::PluginKind::SDK, true, PluginRegistry::PluginCategory::Core);
        // Load/Unload trên row "Orbwalker" giờ thật sự bật/tắt orbwalker SDK;
        // OrbwalkerKuroPlugin cũng dùng chính runtime này để override nó.
        PluginRegistry::BindRuntime(orbwalkerRegistryIdx,
                                    nullptr,
                                    &::SDK::SdkWrappers::ResumeSdkOrbwalkerRuntime,
                                    &::SDK::SdkWrappers::SuspendSdkOrbwalkerRuntime);
        const int targetSelectorRegistryIdx = PluginRegistry::Register("Target Selector", "targetselector", PluginRegistry::PluginKind::SDK, true, PluginRegistry::PluginCategory::Core);
        PluginRegistry::BindRuntime(targetSelectorRegistryIdx,
                                    nullptr,
                                    &::SDK::SdkWrappers::ResumeSdkTargetSelectorRuntime,
                                    &::SDK::SdkWrappers::SuspendSdkTargetSelectorRuntime);
        NightSharpDebug::Logf("[PluginBootstrap] Initialize SDK Wrappers complete");
#else
        NightSharpDebug::Logf("[PluginBootstrap] SDK Wrappers disabled for FPS test");
#endif

        // NightSharpDebug::Logf("[PluginBootstrap] Register EnsoulsharpOrb begin");
        // PluginManager::Get().Register<EnsoulsharpOrbPlugin>();
        // NightSharpDebug::Logf("[PluginBootstrap] Register EnsoulsharpOrb complete");

        // ─── Core plugins ────────────────────────────────────────────────
#if NIGHTSHARP_ENABLE_LIFECYCLE_TEST_PLUGINS
        NightSharpDebug::Logf("[PluginBootstrap] Register lifecycle test plugins begin");
        PluginManager::Get().Register<ObjectDeleteLifecycleTestPlugin>();
        NightSharpDebug::Logf("[PluginBootstrap] Register lifecycle test plugins complete");
#endif

#if NIGHTSHARP_ENABLE_SAMPLE_PLUGINS
        NightSharpDebug::Logf("[PluginBootstrap] Register core plugins begin");
        PluginManager::Get().Register<OrbwalkerKuroPlugin>();
        PluginManager::Get().Register<TargetSelectorImpulsePlugin>();
        PluginManager::Get().Register<PlayerEventFilterPlugin>();
        PluginManager::Get().Register<PlayerBuffDebugPlugin>();
        PluginManager::Get().Register<SpellTrackingDebugPlugin>();
        NightSharpDebug::Logf("[PluginBootstrap] Register core plugins complete");

        NightSharpDebug::Logf("[PluginBootstrap] Register utility plugins begin");
        PluginManager::Get().Register<AttackRangeDrawPlugin>();
        PluginManager::Get().Register<IsDeadDebugPlugin>();
        PluginManager::Get().Register<MovementStateDrawPlugin>();
        PluginManager::Get().Register<NavGridDrawPlugin>();
        PluginManager::Get().Register<ObjectDefinitionDrawPlugin>();
        PluginManager::Get().Register<VisibilityInvulnerabilityOffsetPlugin>();
        PluginManager::Get().Register<YasuoWallDebugPlugin>();
        PluginManager::Get().Register<DeveloperToolsPlugin>();
        NightSharpDebug::Logf("[PluginBootstrap] Register utility plugins complete");

        NightSharpDebug::Logf("[PluginBootstrap] Register champion test plugins begin");
        PluginManager::Get().Register<EzrealSemiPlugin>();
        PluginManager::Get().Register<EzrealMissileLifecyclePlugin>();
        PluginManager::Get().Register<JaxSemiPlugin>();
        PluginManager::Get().Register<XerathSemiCastNew>();
        PluginManager::Get().Register<XerathSemiPlugin>();
        PluginManager::Get().Register<AIO7UPPlugin>();
        PluginManager::Get().Register<KuroAIOPlugin>();
        PluginManager::Get().Register<SharpShooterAIOPlugin>();
        PluginManager::Get().Register<Ziblldev9898Plugin>();
        PluginManager::Get().Register<EzEvadePlugin>();
        PluginManager::Get().Register<KuroEvadePlugin>();
        PluginManager::Get().Register<ZDEvadePlugin>();
        NightSharpDebug::Logf("[PluginBootstrap] Register champion test plugins complete");

#else
        NightSharpDebug::Logf("[PluginBootstrap] Sample/debug plugins disabled for FPS test");
#endif

        NightSharpDebug::Logf("[PluginBootstrap] LoadConfig begin");
        PluginRegistry::LoadConfig();
        NightSharpDebug::Logf("[PluginBootstrap] LoadConfig complete");
        ApplyDebugAutoLoadOverrides();

        // Install config persistence AFTER the registry is populated (so champion
        // category lookup works) and BEFORE LoadAuto (so each plugin's menu gets
        // its saved values applied the moment it attaches during OnLoad).
        NightSharpDebug::Logf("[PluginBootstrap] ConfigStore init begin");
        ConfigStore::Init(&ChampionNameProvider);
        NightSharpDebug::Logf("[PluginBootstrap] ConfigStore init complete");

        // Ensure NightSharp Core Menu is initialized and attached FIRST before plugins load
        NightSharpMenu::EnsureEnsoulCoreMenu();

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
        // Best-effort final flush while menus/roots are still alive.
        ConfigStore::FlushAll();
        PluginManager::Get().Shutdown();
        ::SDK::SdkWrappers::Shutdown();
        PluginRegistry::Reset();
        g_registered = false;
        NightSharpDebug::Logf("[PluginBootstrap] Shutdown complete");
    }

} // namespace PluginBootstrap
} // namespace Plugins
