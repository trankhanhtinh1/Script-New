#pragma once

// ============================================================================
// ZDEvade.h — ZDEvade: skillshot detection + drawing only (evade logic stripped)
//
// Architecture: Detection -> SpellDetector. Drawing -> OnRender.
// Evade decision/action removed — rebuild from scratch.
// ============================================================================

#include "../IPlugin.h"
#include "../../Core/Globals.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../SDK/MenuSDK/Integration/MenuSDKBridge.h"

#include "Debug/ZDLog.h"
#include "Debug/CandidateDebug.h"
#include "Detection/ThreatDetector.h"
#include "Evade/EvadeController.h"
#include "Database/SpellData.h"
#include "Database/SpellDatabase.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Plugins {

using ZDEvade::ZDLog;

class ZDEvadePlugin final : public IPlugin {
public:
    const char* GetName() const override { return "ZDEvade"; }
    const char* GetInternalId() const override { return "core.zdevade"; }
    const char* GetAuthor() const override { return "ziblldev9898"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return false; }

    void OnMenuRegister() override {
        NightSharpMenu::MenuSDKBridge::Instance().RegisterPlugin(
            GetInternalId(),
            GetName(),
            GetAuthor(),
            GetRegistryIndex(),
            "core");
    }

    ~ZDEvadePlugin() override {
        DestroyMenuSDK();
        NightSharpMenu::MenuSDKBridge::Instance().UnregisterPlugin(GetInternalId());
    }

    void OnLoad() override {
        s_instance = this;

        ZDEvade::ThreatDetector::Initialize();
        CreateMenuSDK();

        SDK::Events::AddOnGameUpdate(&ZDEvadePlugin::OnGameUpdateStatic);
        SDK::Orbwalker::OnBeforeMove += &ZDEvadePlugin::OnBeforeMoveStatic;

        ZDLog("[ZDEvade] loaded engine=new");
    }

    void OnUnload() override {
        SDK::Events::RemoveOnGameUpdate(&ZDEvadePlugin::OnGameUpdateStatic);
        SDK::Orbwalker::OnBeforeMove -= &ZDEvadePlugin::OnBeforeMoveStatic;
        m_controller.Reset();
        ZDEvade::ThreatDetector::Shutdown();

        DestroyMenuSDK();

        if (s_instance == this) {
            s_instance = nullptr;
        }
        ZDLog("[ZDEvade] unloaded");
    }

    void OnRender() override {
        if (!Enabled() || !ImGui::GetCurrentContext()) return;

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return;
        const float planeY = player.Position().y;
        const int now = SDK::Variables::TickCount();
        const auto threats = ZDEvade::ThreatDetector::Snapshot();
        const RenderState renderState = GetRenderState();

        if (DrawSpells()) {
            for (const auto& threat : threats) {
                const int danger = threat.Danger();
                std::uint32_t color = 0xFFFF8800;
                if (danger >= 4) color = 0xFFFF0000;
                else if (danger >= 3) color = 0xFFFF4400;
                else if (danger >= 2) color = 0xFFFFFF00;

                if (threat.Type() == ZDEvade::ZDSpellType::Line) {
                    const Vec2 start = threat.HeadAtTick(now);
                    const Vec2 perpendicular(-threat.direction.y, threat.direction.x);
                    const Vec2 startLeft = start + perpendicular * threat.Radius();
                    const Vec2 startRight = start - perpendicular * threat.Radius();
                    const Vec2 endLeft = threat.endPos + perpendicular * threat.Radius();
                    const Vec2 endRight = threat.endPos - perpendicular * threat.Radius();
                    SDK::Drawing::DrawLine(Vec3::From2D(startLeft, planeY), Vec3::From2D(endLeft, planeY), color, 2.0f);
                    SDK::Drawing::DrawLine(Vec3::From2D(startRight, planeY), Vec3::From2D(endRight, planeY), color, 2.0f);
                    SDK::Drawing::DrawLine(Vec3::From2D(startLeft, planeY), Vec3::From2D(startRight, planeY), color, 2.0f);
                    SDK::Drawing::DrawLine(Vec3::From2D(endLeft, planeY), Vec3::From2D(endRight, planeY), color, 2.0f);
                    SDK::Drawing::DrawText(Vec3::From2D(start, planeY), threat.SpellName().c_str(), color, true);
                } else if (threat.Type() == ZDEvade::ZDSpellType::Circular) {
                    SDK::Drawing::DrawCircle(Vec3::From2D(threat.endPos, planeY), threat.Radius(), color, 2.0f);
                    SDK::Drawing::DrawText(Vec3::From2D(threat.endPos, planeY), threat.SpellName().c_str(), color, true);
                } else if (threat.Type() == ZDEvade::ZDSpellType::Ring) {
                    SDK::Drawing::DrawCircle(Vec3::From2D(threat.endPos, planeY), threat.Radius(), color, 2.0f);
                    SDK::Drawing::DrawCircle(Vec3::From2D(threat.endPos, planeY), threat.InnerRadius(), color, 2.0f);
                    SDK::Drawing::DrawText(Vec3::From2D(threat.endPos, planeY), threat.SpellName().c_str(), color, true);
                } else if (threat.Type() == ZDEvade::ZDSpellType::Cone) {
                    const float halfAngle = threat.Angle() * 0.5f * 3.14159265358979323846f / 180.0f;
                    const Vec2 left = threat.startPos + ZDEvade::EvadeGeometry::Rotate(threat.direction, halfAngle) * threat.Range();
                    const Vec2 right = threat.startPos + ZDEvade::EvadeGeometry::Rotate(threat.direction, -halfAngle) * threat.Range();
                    SDK::Drawing::DrawLine(Vec3::From2D(threat.startPos, planeY), Vec3::From2D(left, planeY), color, 2.0f);
                    SDK::Drawing::DrawLine(Vec3::From2D(threat.startPos, planeY), Vec3::From2D(right, planeY), color, 2.0f);
                    SDK::Drawing::DrawLine(Vec3::From2D(left, planeY), Vec3::From2D(right, planeY), color, 2.0f);
                } else {
                    SDK::Drawing::DrawCircle(Vec3::From2D(threat.endPos, planeY), std::max(threat.Radius(), 80.0f), color, 2.0f);
                }
            }
        }

        if (DrawCandidates()) {
            const int count = std::min(140, static_cast<int>(renderState.candidates.size()));
            for (int index = 0; index < count; ++index) {
                const auto& candidate = renderState.candidates[static_cast<std::size_t>(index)];
                const std::uint32_t color = candidate.strictSafe
                    ? 0xFF22DD55
                    : candidate.walkable ? 0xFFFFAA22 : 0xFF777777;
                SDK::Drawing::DrawCircle(Vec3::From2D(candidate.position, planeY), 18.0f, color, 1.5f, 24);
            }
        }

        if (renderState.locked.valid) {
            const std::uint32_t color = renderState.locked.strictSafe ? 0xFF00FF66 : 0xFFFF3300;
            SDK::Drawing::DrawCircle(Vec3::From2D(renderState.locked.position, planeY), 48.0f, color, 3.0f);
            SDK::Drawing::DrawLine(player.ServerPosition(), Vec3::From2D(renderState.locked.position, planeY), color, 2.0f);
        }
    }

private:
    struct RenderState {
        ZDEvade::EvadeControllerState state = ZDEvade::EvadeControllerState::Idle;
        ZDEvade::CandidateEvaluation locked;
        std::vector<ZDEvade::CandidateEvaluation> candidates;
    };

    struct SdkSpellMenuBinding {
        NightSharp::Menu::MenuItemHandle enabled;
        NightSharp::Menu::MenuItemHandle danger;
        NightSharp::Menu::MenuItemHandle health;
    };

    static inline ZDEvadePlugin* s_instance = nullptr;

    NightSharp::Menu::MenuItemHandle sdkEnabledMenu_;
    NightSharp::Menu::MenuItemHandle sdkWalkingEnabledMenu_;
    NightSharp::Menu::MenuItemHandle sdkEvadeSpellsMenu_;
    NightSharp::Menu::MenuItemHandle sdkFallbackMenu_;
    NightSharp::Menu::MenuItemHandle sdkMinimumDangerMenu_;
    NightSharp::Menu::MenuItemHandle sdkEvadeSpellDangerMenu_;
    NightSharp::Menu::MenuItemHandle sdkEvadeSpellMarginMenu_;
    NightSharp::Menu::MenuItemHandle sdkEndpointBufferMenu_;
    NightSharp::Menu::MenuItemHandle sdkPathBufferMenu_;
    NightSharp::Menu::MenuItemHandle sdkReleaseBufferMenu_;
    NightSharp::Menu::MenuItemHandle sdkInputDelayMenu_;
    NightSharp::Menu::MenuItemHandle sdkMinimumMarginMenu_;
    NightSharp::Menu::MenuItemHandle sdkPreferredClearanceMenu_;
    NightSharp::Menu::MenuItemHandle sdkSearchRadiusMenu_;
    NightSharp::Menu::MenuItemHandle sdkMoveIntervalMenu_;
    NightSharp::Menu::MenuItemHandle sdkMoveRefreshMenu_;
    NightSharp::Menu::MenuItemHandle sdkReplanIntervalMenu_;
    NightSharp::Menu::MenuItemHandle sdkDrawSpellsMenu_;
    NightSharp::Menu::MenuItemHandle sdkDrawCandidatesMenu_;

    ZDEvade::EvadeController m_controller;
    std::unordered_map<std::string, SdkSpellMenuBinding> sdkSpellBindings_;
    ZDEvade::ThreatRuleMap m_threatRules;
    mutable SRWLOCK m_renderStateLock = SRWLOCK_INIT;
    RenderState m_renderState;

    bool Enabled() const {
        return !sdkEnabledMenu_ || sdkEnabledMenu_->value;
    }

    bool DrawSpells() const {
        return !sdkDrawSpellsMenu_ || sdkDrawSpellsMenu_->value;
    }

    bool DrawCandidates() const {
        return sdkDrawCandidatesMenu_ && sdkDrawCandidatesMenu_->value;
    }

    static void OnGameUpdateStatic(const SDK::Events::GameUpdateEventArgs&) {
        if (s_instance) s_instance->Tick();
    }

    static void OnBeforeMoveStatic(SDK::OrbwalkingActionArgs& args) {
        if (!s_instance || !args.Process) return;
        if (s_instance->m_controller.ShouldBlockMove(
                args.Position.To2D(),
                s_instance->BuildConfig())) args.Process = false;
    }

    ZDEvade::EvadeRuntimeConfig BuildConfig() const {
        ZDEvade::EvadeRuntimeConfig config;
        const auto boolValue = [](const NightSharp::Menu::MenuItemHandle& item, bool fallback) {
            return item ? item->value : fallback;
        };
        const auto intValue = [](const NightSharp::Menu::MenuItemHandle& item, int fallback) {
            return item ? item->integer : fallback;
        };
        config.enabled = Enabled();
        config.walkingEnabled = boolValue(sdkWalkingEnabledMenu_, true);
        config.evadeSpellsEnabled = boolValue(sdkEvadeSpellsMenu_, true);
        config.leastDangerFallback = boolValue(sdkFallbackMenu_, true);
        config.minimumDanger = intValue(sdkMinimumDangerMenu_, 1);
        config.evadeSpellMinimumDanger = intValue(sdkEvadeSpellDangerMenu_, 3);
        config.evadeSpellMarginThresholdMs = static_cast<float>(
            intValue(sdkEvadeSpellMarginMenu_, 45));
        config.threatRules = &m_threatRules;
        config.moveIntervalMs = intValue(sdkMoveIntervalMenu_, 85);
        config.moveRefreshMs = intValue(sdkMoveRefreshMenu_, 320);
        config.replanIntervalMs = intValue(sdkReplanIntervalMenu_, 90);
        config.fallbackReplanIntervalMs = std::max(25, config.replanIntervalMs / 2);
        config.planner.endpointBuffer = static_cast<float>(
            intValue(sdkEndpointBufferMenu_, 32));
        config.planner.pathBuffer = static_cast<float>(
            intValue(sdkPathBufferMenu_, 12));
        config.planner.releaseBuffer = static_cast<float>(
            intValue(sdkReleaseBufferMenu_, 48));
        config.planner.inputDelayMs = static_cast<float>(
            intValue(sdkInputDelayMenu_, 55)) +
            static_cast<float>(std::max(0, SDK::Game::Ping())) * 0.5f;
        config.planner.minimumTimeMarginMs = static_cast<float>(
            intValue(sdkMinimumMarginMenu_, 25));
        config.planner.preferredClearance = static_cast<float>(
            intValue(sdkPreferredClearanceMenu_, 24));
        config.planner.maxSearchRadius = static_cast<float>(
            intValue(sdkSearchRadiusMenu_, 760));
        return config;
    }

    void Tick() {
        RefreshThreatRules();
        m_controller.Update(BuildConfig());
        RenderState next;
        next.state = m_controller.State();
        next.locked = m_controller.Locked();
        if (DrawCandidates()) next.candidates = m_controller.LastPlan().candidates;
        AcquireSRWLockExclusive(&m_renderStateLock);
        m_renderState = std::move(next);
        ReleaseSRWLockExclusive(&m_renderStateLock);
    }

    RenderState GetRenderState() const {
        AcquireSRWLockShared(&m_renderStateLock);
        RenderState result = m_renderState;
        ReleaseSRWLockShared(&m_renderStateLock);
        return result;
    }

    void CreateMenuSDK() {
        DestroyMenuSDK();
        auto builder = NightSharpMenu::MenuSDKBridge::Instance().RegisterMenu(
            GetInternalId(),
            GetName());

        auto main = builder.Section("main", "Main");
        sdkEnabledMenu_ = main.Checkbox("enabled", "Enable ZDEvade", true);
        sdkWalkingEnabledMenu_ = main.Checkbox("walking", "Walking Evade", true);
        sdkEvadeSpellsMenu_ = main.Checkbox("evadeSpells", "Direct Evade Spells", true);
        sdkFallbackMenu_ = main.Checkbox("leastDangerFallback", "Least Danger Fallback", true);
        sdkMinimumDangerMenu_ = main.Slider("minimumDanger", "Minimum Danger", 1, 1, 4);
        sdkEvadeSpellDangerMenu_ = main.Slider(
            "evadeSpellDanger", "Evade Spell Minimum Danger", 3, 1, 4);
        sdkEvadeSpellMarginMenu_ = main.Slider(
            "evadeSpellMargin", "Evade Spell Margin Threshold", 45, 0, 250);

        auto spellsSection = builder.Section("spells", "Enemy Spells");
        sdkSpellBindings_.clear();
        std::unordered_set<std::string> enemyNames;
        for (const auto& enemy : SDK::ObjectManager::EnemyHeroes()) {
            if (!enemy.IsValid()) continue;
            enemyNames.insert(enemy.CharacterName());
        }
        int index = 0;
        for (const auto& spell : ZDEvade::SpellDatabase::Spells) {
            if (spell.spellName.empty()) continue;
            if (spell.charName != "AllChampions" &&
                enemyNames.find(spell.charName) == enemyNames.end()) {
                continue;
            }
            if (sdkSpellBindings_.find(spell.spellName) != sdkSpellBindings_.end()) {
                continue;
            }
            const std::string id = "spell_" + std::to_string(index++);
            const std::string label = spell.charName + " - " +
                (spell.name.empty() ? spell.spellName : spell.name);
            auto spellSection = spellsSection.Section(id, label);
            SdkSpellMenuBinding binding;
            const bool enabled = !spell.defaultOff;
            const int danger = std::clamp(spell.dangerlevel, 1, 4);
            const int health = spell.dangerlevel == 1 ? 90 : 100;
            binding.enabled = spellSection.Checkbox("enabled", "Dodge", enabled);
            binding.danger = spellSection.Slider(
                "danger", "Danger", danger, 1, 4);
            binding.health = spellSection.Slider(
                "health", "Dodge Only Below HP", health, 0, 100);
            sdkSpellBindings_.emplace(spell.spellName, std::move(binding));
        }

        auto safety = builder.Section("safety", "Safety and Timing");
        sdkEndpointBufferMenu_ = safety.Slider(
            "endpointBuffer", "Endpoint Buffer", 32, 0, 120);
        sdkPathBufferMenu_ = safety.Slider(
            "pathBuffer", "Path Buffer", 12, 0, 100);
        sdkReleaseBufferMenu_ = safety.Slider(
            "releaseBuffer", "Release Buffer", 48, 0, 140);
        sdkInputDelayMenu_ = safety.Slider(
            "inputDelay", "Extra Input Delay", 55, 0, 200);
        sdkMinimumMarginMenu_ = safety.Slider(
            "minimumMargin", "Minimum Time Margin", 25, 0, 250);
        sdkPreferredClearanceMenu_ = safety.Slider(
            "preferredClearance", "Preferred Clearance", 24, 0, 100);
        sdkSearchRadiusMenu_ = safety.Slider(
            "searchRadius", "Maximum Search Radius", 760, 300, 1200);

        auto control = builder.Section("control", "Control");
        sdkMoveIntervalMenu_ = control.Slider(
            "moveInterval", "Move Interval", 85, 25, 250);
        sdkMoveRefreshMenu_ = control.Slider(
            "moveRefresh", "Move Refresh", 320, 100, 600);
        sdkReplanIntervalMenu_ = control.Slider(
            "replanInterval", "Replan Interval", 90, 20, 250);

        auto draw = builder.Section("draw", "Draw");
        sdkDrawSpellsMenu_ = draw.Checkbox("drawSpells", "Draw Skillshots", true);
        sdkDrawCandidatesMenu_ = draw.Checkbox("drawCandidates", "Draw Candidates", false);
        RefreshThreatRules();
    }

    void DestroyMenuSDK() {
        auto& bridge = NightSharpMenu::MenuSDKBridge::Instance();
        bridge.UnregisterMenu(GetInternalId());
        sdkEnabledMenu_.reset();
        sdkWalkingEnabledMenu_.reset();
        sdkEvadeSpellsMenu_.reset();
        sdkFallbackMenu_.reset();
        sdkMinimumDangerMenu_.reset();
        sdkEvadeSpellDangerMenu_.reset();
        sdkEvadeSpellMarginMenu_.reset();
        sdkEndpointBufferMenu_.reset();
        sdkPathBufferMenu_.reset();
        sdkReleaseBufferMenu_.reset();
        sdkInputDelayMenu_.reset();
        sdkMinimumMarginMenu_.reset();
        sdkPreferredClearanceMenu_.reset();
        sdkSearchRadiusMenu_.reset();
        sdkMoveIntervalMenu_.reset();
        sdkMoveRefreshMenu_.reset();
        sdkReplanIntervalMenu_.reset();
        sdkDrawSpellsMenu_.reset();
        sdkDrawCandidatesMenu_.reset();
        sdkSpellBindings_.clear();
        m_threatRules.clear();
        AcquireSRWLockExclusive(&m_renderStateLock);
        m_renderState = {};
        ReleaseSRWLockExclusive(&m_renderStateLock);
    }

    void RefreshThreatRules() {
        for (const auto& entry : sdkSpellBindings_) {
            ZDEvade::ThreatRule& rule = m_threatRules[entry.first];
            rule.enabled = !entry.second.enabled || entry.second.enabled->value;
            rule.danger = entry.second.danger ? entry.second.danger->integer : 1;
            rule.dodgeHealthPercent = static_cast<float>(
                entry.second.health ? entry.second.health->integer : 100);
        }
    }
};

} // namespace Plugins
