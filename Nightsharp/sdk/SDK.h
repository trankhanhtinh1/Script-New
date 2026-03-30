#pragma once

#include "../core/CrashTelemetry.h"
#include "../menu/PluginRegistry.h"
#include "Core/Constants.h"
#include "Wrappers/Damages/Damage.h"
#include "Wrappers/Damages/DamageJson.h"
#include "Wrappers/Damages/DamageMastery.h"
#include "Wrappers/Damages/DamagePassives.h"
#include "UI/Cursor.h"
#include "Enumerations\CollisionObjects.h"
#include "Enumerations\DangerLevel.h"
#include "Enumerations\DamageType.h"
#include "Enumerations\GameObjectOrder.h"
#include "Enumerations\GapcloserType.h"
#include "Enumerations\HealthPredictionType.h"
#include "Enumerations\MinionTeam.h"
#include "Enumerations\MinionTypes.h"
#include "Enumerations\MinionOrderTypes.h"
#include "Enumerations\HitChance.h"
#include "Enumerations\JungleType.h"
#include "Enumerations\OrbwalkerMode.h"
#include "Enumerations\OrbwalkingType.h"
#include "Enumerations\LogLevel.h"
#include "Enumerations\NotificationIconType.h"
#include "Enumerations\PerformanceType.h"
#include "Enumerations\SkillshotType.h"
#include "Enumerations\SpellSlot.h"
#include "Enumerations\SpellType.h"
#include "Enumerations\TeleportStatus.h"
#include "Enumerations\TeleportType.h"
#include "Enumerations\TargetSelectorMode.h"
#include "Enumerations\TurretType.h"
#include "Extensions/Extensions.h"
#include "Events/AntiGapcloser.h"
#include "Events/Dash.h"
#include "UI/Drawing.h"
#include "UI/PermaShow.h"
#include "UI/Utils.h"
#include "UI/Notifications/Notifications.h"
#include "Events\Events.h"
#include "Core/Game.h"
#include "Wrappers/GameObjects.h"
#include "GameObjects/ObjectManager.h"
#include "Math/HealthPrediction.h"
#include "Events/Interrupter.h"
#include "Events/Stealth.h"
#include "Events/Teleport.h"
#include "Events/Turret.h"
#include "Wrappers/Spells/LastCast.h"
#include "Wrappers/Spells/LastCastedSpellEntry.h"
#include "Wrappers/Spells/LastCastPacketSentEntry.h"
#include "Wrappers/Map.h"
#include "Math\Collision.h"
#include "Math\Geometry.h"
#include "Wrappers/Orbwalking/Orbwalker.h"
#include "Wrappers/Orbwalking/OrbwalkerBase.h"
#include "Wrappers/Orbwalking/OrbwalkerSelector.h"
#include "Math/Prediction.h"
#include "Wrappers/Spells/Spell.h"
#include "Wrappers/Spells/Tracker/Detector.h"
#include "Wrappers/Spells/Tracker/Tracker.h"
#include "Wrappers/Spells/Tracker/Skillshots/_ZiggsR.h"
#include "Wrappers/Spells/Database/SpellDatabase.h"
#include "Wrappers/Spells/Database/SpellDatabaseEntry.h"
#include "Wrappers/Spells/SpellTypes.h"
#include "Wrappers/Spells/SpellTypes/SkillshotMissileArc.h"
#include "Wrappers/TargetSelector/TargetSelector.h"
#include "Wrappers/TargetSelector/HeroVisibleEntry.h"
#include "Wrappers/TargetSelector/ITargetSelectorMode.h"
#include "Wrappers/TargetSelector/TargetSelectorDrawing.h"
#include "Wrappers/TargetSelector/TargetSelectorHumanizer.h"
#include "Wrappers/TargetSelector/TargetSelectorMode.h"
#include "Wrappers/TargetSelector/TargetSelectorSelected.h"
#include "UI/IMenu/IMenu.h"
#include "UI/UI.h"
#include "Core/Variables.h"
#include "Signals/Signal.h"
#include "Signals/SignalManager.h"
#include "Utils/Utils.h"

#include <Windows.h>

namespace SDK {
namespace Bootstrap {

    inline bool g_initialized = false;
    inline int g_targetSelectorPluginIndex = -1;
    inline int g_orbwalkerPluginIndex = -1;

    inline void TraceInitStage(const char* stage) {
        CrashTelemetry::SetStage(stage);
        if (stage && *stage) {
            OutputDebugStringA(stage);
            OutputDebugStringA("\r\n");

            HANDLE hFile = CreateFileA(
                "C:\\Users\\Public\\ns_stage.txt",
                FILE_APPEND_DATA,
                FILE_SHARE_READ,
                nullptr,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);
            if (hFile != INVALID_HANDLE_VALUE) {
                char buffer[512] = {};
                wsprintfA(buffer, "[NightSharp] %s\r\n", stage);
                DWORD written = 0;
                WriteFile(hFile, buffer, static_cast<DWORD>(lstrlenA(buffer)), &written, nullptr);
                CloseHandle(hFile);
            }
        }
    }

    inline void Init(Menu* parent = nullptr) {
        (void)parent;
        if (g_initialized) {
            return;
        }
        g_initialized = true;

        TraceInitStage("SDK::Bootstrap::Init::TargetSelector");
        TargetSelector::Initialize();
        TraceInitStage("SDK::Bootstrap::Init::Orbwalker");
        Orbwalker::Initialize();
        TraceInitStage("SDK::Bootstrap::Init::Events");
        Events::Initialize();
        TraceInitStage("SDK::Bootstrap::Init::Prediction");
        Prediction::Initialize();
        TraceInitStage("SDK::Bootstrap::Init::SpellTracker");
        SpellTracker::Initialize();
        TraceInitStage("SDK::Bootstrap::Init::Signals");
        Signals::SignalManager::Initialize();
        TraceInitStage("SDK::Bootstrap::Init::DelayAction");
        Utils::DelayAction::Initialize();
        TraceInitStage("SDK::Bootstrap::Init::ActionQueue");
        Utils::ActionQueue::Initialize();
        TraceInitStage("SDK::Bootstrap::Init::TickOperation");
        Utils::TickOperation::Initialize();
        TraceInitStage("SDK::Bootstrap::Init::ResourceLoader");
        Utils::ResourceLoader::Initialize();
        TraceInitStage("SDK::Bootstrap::Init::PermaShow");
        UI::PermaShow::Initialize();
        TraceInitStage("SDK::Bootstrap::Init::DynamicInitializer");
        Utils::DynamicInitializer::RunPending();
        TraceInitStage("SDK::Bootstrap::Init::Storage");
        Utils::Storage::LoadAllRegistered();

        TraceInitStage("SDK::Bootstrap::Init::RegisterTargetSelector");
        g_targetSelectorPluginIndex = PluginRegistry::Register(
            "Target Selector",
            "targetselector",
            PluginRegistry::PluginKind::SDK,
            TargetSelector::GetMenu(),
            true);

        TraceInitStage("SDK::Bootstrap::Init::RegisterOrbwalker");
        g_orbwalkerPluginIndex = PluginRegistry::Register(
            "Orbwalker",
            "orbwalker",
            PluginRegistry::PluginKind::SDK,
            Orbwalker::GetMenu(),
            true);

        TraceInitStage("SDK::Bootstrap::Init::DispatchLoad");
        Events::DispatchLoad();
        TraceInitStage("SDK::Bootstrap::Init::Done");
    }

    inline void Shutdown() {
        LastCast::Reset();
        Events::Reset();
        Prediction::Reset();
        SpellTracker::Reset();
        Signals::SignalManager::Reset();
        Utils::DelayAction::Reset();
        Utils::ActionQueue::Reset();
        Utils::TickOperation::Reset();
        Utils::Storage::SaveAllRegistered();
        Utils::Cache::Reset();
        Utils::Render::Reset();
        Utils::ResourceLoader::Reset();
        Utils::ResourceFactory::Reset();
        Utils::DynamicInitializer::Reset();
        DamageLibrary::Shutdown();
        UI::PermaShow::Reset();
        g_initialized = false;
    }

    inline void Update() {
        if (!g_initialized || !CoreAPI::State::IsRuntimeReady()) {
            return;
        }

        CrashTelemetry::SetStage("SDK::Bootstrap::Update::MenuKeyBinds");
        Menu::UpdateAllKeyBinds();
        CrashTelemetry::SetStage("SDK::Bootstrap::Update::TargetSelector");
        TargetSelector::Update();
        CrashTelemetry::SetStage("SDK::Bootstrap::Update::Events");
        Events::Update();
        CrashTelemetry::SetStage("SDK::Bootstrap::Update::Prediction");
        Prediction::Update();
        CrashTelemetry::SetStage("SDK::Bootstrap::Update::SpellTracker");
        SpellTracker::Update();
        CrashTelemetry::SetStage("SDK::Bootstrap::Update::Signals");
        Signals::SignalManager::Update();
        CrashTelemetry::SetStage("SDK::Bootstrap::Update::Cache");
        Utils::Cache::Update();
        CrashTelemetry::SetStage("SDK::Bootstrap::Update::Render");
        Utils::Render::Update();
        CrashTelemetry::SetStage("SDK::Bootstrap::Update::GameDispatchUpdate");
        Game::DispatchUpdate();
        CrashTelemetry::SetStage("SDK::Bootstrap::Update::OrbwalkerUpdate");
        Orbwalker::Update();
    }

    inline void Render() {
        if (!g_initialized || !CoreAPI::State::IsRuntimeReady()) {
            return;
        }

        TargetSelector::Render();
        Orbwalker::Render();
        UI::PermaShow::Render();
        UI::Notifications::Notifications::Render();
        Utils::Render::DrawObjects();
        Utils::Render::EndSceneObjects();
        Game::DispatchDraw();
    }
}
} // namespace SDK
