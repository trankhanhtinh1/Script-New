#pragma once

#include "../IPlugin.h"
#include "../../menu/PluginRegistry.h"
#include "../../sdk/Enumerations/DamageType.h"
#include "../../sdk/GameObjects/ObjectManager.h"
#include "../../sdk/UI/Drawing.h"
#include "../../sdk/UI/UI.h"
#include "../../sdk/Utils/Invulnerable.h"
#include "../../sdk/Wrappers/TargetSelector/PriorityData.h"
#include "../../sdk/Wrappers/TargetSelector/TargetSelector.h"

#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace Plugins {

class TargetSelectorPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Target Selector"; }
    const char* GetInternalId() const override { return "plugin_targetselector"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return true; }

    void OnLoad() override {
        if (!m_menu) {
            m_menu = SDK::Menu::Create("plugin_targetselector", "Target Selector");
            m_menu->Add<SDK::MenuBool>("forceSelected", "Force Selected Target", true);
            m_menu->Add<SDK::MenuBool>("onlySelected", "Only Attack Selected Target", false);
            m_menu->Add<SDK::MenuList>(
                "mode",
                "Mode",
                std::vector<std::string>{ "Smart AD/AP", "Lowest Health", "Most Priority", "Closest", "Near Mouse" },
                0);

            m_priorityMenu = m_menu->AddSubMenu("priority", "Priority");
            m_priorityMenu->Add<SDK::MenuBool>("autoPriority", "Auto Priority", true);

            auto* draw = m_menu->AddSubMenu("drawing", "Drawing");
            draw->Add<SDK::MenuBool>("drawSelected", "Draw Selected Target", true);
            draw->Add<SDK::MenuBool>("drawBestDebug", "Draw Best Target (Debug)", false);
            draw->Add<SDK::MenuSlider>("clickBuffer", "Click Buffer", 110, 25, 250);
            draw->Add<SDK::MenuColor>("selectedColor", "Selected Color", 1.0f, 0.1f, 0.1f, 1.0f);
            draw->Add<SDK::MenuColor>("bestColor", "Best Target Color", 0.1f, 1.0f, 0.25f, 0.85f);
        }

        s_instance = this;
        SDK::TargetSelector::SetExternalControl(true);
        SDK::TargetSelector::SetOverride(
            &TargetSelectorPlugin::S_GetTarget,
            &TargetSelectorPlugin::S_GetTargets,
            &TargetSelectorPlugin::S_GetSelectedTarget,
            &TargetSelectorPlugin::S_SetSelectedTarget,
            &TargetSelectorPlugin::S_ClearSelectedTarget,
            &TargetSelectorPlugin::S_GetPriority);
        HideSdkEntry();
    }

    void OnUnload() override {
        if (s_instance == this) {
            SDK::TargetSelector::ClearOverride();
            SDK::TargetSelector::SetExternalControl(false);
            s_instance = nullptr;
        }
        m_selectedTarget = {};
        m_bestTarget = {};
        ShowSdkEntry();
    }

    void OnUpdate() override {
        if (!m_menu) return;
        HideSdkEntry();
        const int now = SDK::Game::TickCount();
        if (now - m_lastPriorityUpdateTick >= 1000) {
            m_lastPriorityUpdateTick = now;
            EnsurePrioritySliders();
        }
        UpdateClickSelection();
        auto* draw = m_menu->GetSubMenu("drawing");
        const bool drawBest = draw && draw->GetBoolValue("drawBestDebug", false);
        if (drawBest && now - m_lastBestTargetUpdateTick >= 250) {
            m_lastBestTargetUpdateTick = now;
            m_bestTarget = GetTarget(1500.0f, SDK::DamageType::Physical, true, SDK::ObjectManager::Player().Position(), nullptr);
        } else if (!drawBest) {
            m_bestTarget = {};
        }
    }

    void OnRender() override {
        if (!m_menu) return;
        auto* draw = m_menu->GetSubMenu("drawing");
        if (!draw) return;

        if (draw->GetBoolValue("drawSelected", true) && IsAliveTarget(m_selectedTarget)) {
            auto* color = draw->Get<SDK::MenuColor>("selectedColor");
            const ImU32 col = color ? color->GetImU32() : IM_COL32(255, 30, 30, 255);
            SDK::Drawing::DrawCircle(m_selectedTarget.Position(), m_selectedTarget.BoundingRadius(), col, 2.0f);
        }

        if (draw->GetBoolValue("drawBestDebug", false) && IsAliveTarget(m_bestTarget) &&
            (!m_selectedTarget.IsValid() || !m_bestTarget.Compare(m_selectedTarget))) {
            auto* color = draw->Get<SDK::MenuColor>("bestColor");
            const ImU32 col = color ? color->GetImU32() : IM_COL32(25, 255, 70, 215);
            SDK::Drawing::DrawCircle(m_bestTarget.Position(), m_bestTarget.BoundingRadius() + 40.0f, col, 1.6f);
        }
    }

    SDK::MenuUI::Menu* GetMenuRoot() override { return m_menu; }

    SDK::AIHeroClient GetTarget(float range,
                                SDK::DamageType damageType,
                                bool ignoreShields,
                                const SDK::Vector3& from,
                                const std::vector<SDK::AIHeroClient>* ignoreChampions) {
        const SDK::Vector3 source = from.IsZero() ? SDK::ObjectManager::Player().Position() : from;

        if (m_menu && m_menu->GetBoolValue("onlySelected", false)) {
            return IsValidTarget(m_selectedTarget, range, damageType, ignoreShields, source) ? m_selectedTarget : SDK::AIHeroClient();
        }

        auto targets = GetTargets(range, damageType, ignoreShields, source, ignoreChampions);
        if (targets.empty()) return SDK::AIHeroClient();

        if (m_menu && m_menu->GetBoolValue("forceSelected", true) &&
            IsValidTarget(m_selectedTarget, range, damageType, ignoreShields, source)) {
            return m_selectedTarget;
        }

        return targets.front();
    }

    std::vector<SDK::AIHeroClient> GetTargets(float range,
                                              SDK::DamageType damageType,
                                              bool ignoreShields,
                                              const SDK::Vector3& from,
                                              const std::vector<SDK::AIHeroClient>* ignoreChampions) {
        const SDK::Vector3 source = from.IsZero() ? SDK::ObjectManager::Player().Position() : from;
        std::vector<SDK::AIHeroClient> targets;
        auto enemies = SDK::ObjectManager::EnemyHeroes();
        targets.reserve(enemies.size());

        for (const auto& hero : enemies) {
            if (!IsValidTarget(hero, range, damageType, ignoreShields, source)) continue;
            if (ignoreChampions && Contains(*ignoreChampions, hero)) continue;
            targets.push_back(hero);
        }

        SortTargets(targets, damageType, source);
        return targets;
    }

    SDK::AIHeroClient GetSelectedTarget() const { return m_selectedTarget; }

    void SetSelectedTarget(const SDK::AIHeroClient& target) {
        m_selectedTarget = IsAliveTarget(target) ? target : SDK::AIHeroClient();
    }

    void ClearSelectedTarget() {
        m_selectedTarget = {};
    }

    int GetPriority(const SDK::AIHeroClient& hero) const {
        if (!hero.IsValid()) return 1;
        const std::string name = hero.CharacterName();
        if (name.empty() || !m_priorityMenu) return 1;
        return SDK::TargetSelectorData::ClampPriority(
            m_priorityMenu->GetSliderValue(name, SDK::TargetSelectorData::GetDefaultPriority(name)));
    }

private:
    SDK::MenuUI::Menu* m_menu = nullptr;
    SDK::MenuUI::Menu* m_priorityMenu = nullptr;
    SDK::AIHeroClient m_selectedTarget = {};
    SDK::AIHeroClient m_bestTarget = {};
    bool m_leftWasDown = false;
    int m_lastPriorityUpdateTick = 0;
    int m_lastBestTargetUpdateTick = 0;

    static inline TargetSelectorPlugin* s_instance = nullptr;

    static SDK::AIHeroClient S_GetTarget(float range,
                                         SDK::DamageType damageType,
                                         bool ignoreShields,
                                         const SDK::Vector3& from,
                                         const std::vector<SDK::AIHeroClient>* ignoreChampions) {
        return s_instance ? s_instance->GetTarget(range, damageType, ignoreShields, from, ignoreChampions) : SDK::AIHeroClient();
    }

    static std::vector<SDK::AIHeroClient> S_GetTargets(float range,
                                                       SDK::DamageType damageType,
                                                       bool ignoreShields,
                                                       const SDK::Vector3& from,
                                                       const std::vector<SDK::AIHeroClient>* ignoreChampions) {
        return s_instance ? s_instance->GetTargets(range, damageType, ignoreShields, from, ignoreChampions) : std::vector<SDK::AIHeroClient>{};
    }

    static SDK::AIHeroClient S_GetSelectedTarget() {
        return s_instance ? s_instance->GetSelectedTarget() : SDK::AIHeroClient();
    }

    static void S_SetSelectedTarget(const SDK::AIHeroClient& target) {
        if (s_instance) s_instance->SetSelectedTarget(target);
    }

    static void S_ClearSelectedTarget() {
        if (s_instance) s_instance->ClearSelectedTarget();
    }

    static int S_GetPriority(const SDK::AIHeroClient& target) {
        return s_instance ? s_instance->GetPriority(target) : 1;
    }

    static bool Contains(const std::vector<SDK::AIHeroClient>& list, const SDK::AIHeroClient& hero) {
        return std::any_of(list.begin(), list.end(), [&hero](const SDK::AIHeroClient& item) {
            return item.Compare(hero);
        });
    }

    static bool IsAliveTarget(const SDK::AIHeroClient& hero) {
        return hero.IsValid() && hero.IsAlive() && hero.Health() > 0.0f;
    }

    bool IsValidTarget(const SDK::AIHeroClient& hero,
                       float range,
                       SDK::DamageType damageType,
                       bool ignoreShields,
                       const SDK::Vector3& from) const {
        if (!IsAliveTarget(hero)) return false;
        const auto player = SDK::ObjectManager::Player();
        const float finalRange = range <= 0.0f ? player.GetRealAutoAttackRange(hero) : range;
        if (finalRange > 0.0f && hero.DistanceSquared(from) > finalRange * finalRange) return false;
        return !SDK::Utils::Invulnerable::Check(hero, damageType, ignoreShields);
    }

    float ScoreSmart(const SDK::AIHeroClient& hero, SDK::DamageType damageType, const SDK::Vector3& from) const {
        const auto player = SDK::ObjectManager::Player();
        const float hpPct = std::clamp(hero.HealthPercent(), 0.0f, 100.0f);
        const float priority = static_cast<float>(GetPriority(hero)) * 115.0f;
        const float lowHealth = 100.0f - hpPct;
        const float distance = std::max(0.0f, 1400.0f - hero.Distance(from)) * 0.05f;
        const float aaKill = player.GetAutoAttackDamage(hero) >= hero.Health() ? 90.0f : 0.0f;
        const float damageBias = damageType == SDK::DamageType::Magical ? hero.AbilityPower() * 0.08f : hero.TotalAttackDamage() * 0.08f;
        return priority + lowHealth + distance + aaKill + damageBias;
    }

    void SortTargets(std::vector<SDK::AIHeroClient>& targets, SDK::DamageType damageType, const SDK::Vector3& from) const {
        const int mode = m_menu ? m_menu->GetListIndex("mode", 0) : 0;
        switch (mode) {
        case 1:
            std::sort(targets.begin(), targets.end(), [](const SDK::AIHeroClient& a, const SDK::AIHeroClient& b) {
                return a.Health() < b.Health();
            });
            break;
        case 2:
            std::sort(targets.begin(), targets.end(), [this](const SDK::AIHeroClient& a, const SDK::AIHeroClient& b) {
                const int pa = GetPriority(a);
                const int pb = GetPriority(b);
                return pa == pb ? a.Health() < b.Health() : pa > pb;
            });
            break;
        case 3:
            std::sort(targets.begin(), targets.end(), [&from](const SDK::AIHeroClient& a, const SDK::AIHeroClient& b) {
                return a.DistanceSquared(from) < b.DistanceSquared(from);
            });
            break;
        case 4: {
            const SDK::Vector3 cursor = SDK::Game::CursorPos();
            std::sort(targets.begin(), targets.end(), [&cursor](const SDK::AIHeroClient& a, const SDK::AIHeroClient& b) {
                return a.DistanceSquared(cursor) < b.DistanceSquared(cursor);
            });
            break;
        }
        default:
            std::sort(targets.begin(), targets.end(), [this, damageType, &from](const SDK::AIHeroClient& a, const SDK::AIHeroClient& b) {
                return ScoreSmart(a, damageType, from) > ScoreSmart(b, damageType, from);
            });
            break;
        }
    }

    void EnsurePrioritySliders() {
        if (!m_priorityMenu) return;
        const bool autoPriority = m_priorityMenu->GetBoolValue("autoPriority", true);
        for (const auto& hero : SDK::ObjectManager::EnemyHeroes()) {
            const std::string name = hero.CharacterName();
            if (name.empty() || m_priorityMenu->HasItem(name)) continue;
            const int priority = autoPriority ? SDK::TargetSelectorData::GetDefaultPriority(name) : 1;
            m_priorityMenu->Add<SDK::MenuSlider>(name, name, priority, 1, 5);
        }
    }

    void UpdateClickSelection() {
        const bool leftDown = (::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (!leftDown || m_leftWasDown || !SDK::Game::CanProcessInput()) {
            m_leftWasDown = leftDown;
            return;
        }
        m_leftWasDown = leftDown;

        const auto* draw = m_menu ? m_menu->GetSubMenu("drawing") : nullptr;
        const float clickBuffer = static_cast<float>(draw ? draw->GetSliderValue("clickBuffer", 110) : 110);
        const SDK::Vector3 cursor = SDK::Game::CursorPos();
        SDK::AIHeroClient best = {};
        float bestDist = FLT_MAX;
        for (const auto& hero : SDK::ObjectManager::EnemyHeroes()) {
            if (!IsAliveTarget(hero)) continue;
            const float dist = hero.Distance(cursor);
            if (dist <= hero.BoundingRadius() + clickBuffer && dist < bestDist) {
                best = hero;
                bestDist = dist;
            }
        }
        m_selectedTarget = best;
    }

    static void HideSdkEntry() {
        const int idx = PluginRegistry::FindByInternalId("targetselector");
        if (idx >= 0) PluginRegistry::Plugins[idx].Loaded = false;
    }

    static void ShowSdkEntry() {
        const int idx = PluginRegistry::FindByInternalId("targetselector");
        if (idx >= 0) PluginRegistry::Plugins[idx].Loaded = PluginRegistry::Plugins[idx].AlwaysLoad;
    }
};

} // namespace Plugins
