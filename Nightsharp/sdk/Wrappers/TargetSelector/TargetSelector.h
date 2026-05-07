#pragma once

#include "PriorityData.h"
#include "../../Core/Game.h"
#include "../../Enumerations/DamageType.h"
#include "../../Enumerations/TargetSelectorMode.h"
#include "../../GameObjects/ObjectManager.h"
#include "../../UI/Drawing.h"
#include "../../UI/UI.h"
#include "../../Utils/Invulnerable.h"

#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace SDK {

class TargetSelector {
public:
    using GetTargetOverride = AIHeroClient(*)(float, DamageType, bool, const Vector3&, const std::vector<AIHeroClient>*);
    using GetTargetsOverride = std::vector<AIHeroClient>(*)(float, DamageType, bool, const Vector3&, const std::vector<AIHeroClient>*);
    using GetSelectedTargetOverride = AIHeroClient(*)();
    using SetSelectedTargetOverride = void(*)(const AIHeroClient&);
    using ClearSelectedTargetOverride = void(*)();
    using GetPriorityOverride = int(*)(const AIHeroClient&);

    static void Initialize() {
        if (s_initialized) {
            return;
        }
        s_initialized = true;

        s_menu = UI::CreateMenu("targetselector", "Target Selector");
        s_menu.AddBool("focus", "Focus Selected Target", true);
        s_menu.AddBool("forceFocus", "Only Attack Selected Target", false);
        s_menu.AddList("mode", "Mode",
            { "Priorities", "Closest", "Least Health", "Most Attack Damage",
              "Most Ability Power", "Near Mouse", "Less Attacks To Kill",
              "Less Casts To Kill", "Weight" },
            static_cast<int>(TargetSelectorMode::Weight));

        s_priorityMenu = s_menu.AddMenu("priority", "Priority");
        s_priorityMenu.AddBool("autoPriority", "Auto Priority", true);

        s_drawingMenu = s_menu.AddMenu("drawing", "Drawing");
        s_drawingMenu.AddBool("selectedEnabled", "Selected Target", true);
        s_drawingMenu.AddSlider("selectedRadius", "Selected Radius", 35, 10, 125);
        s_drawingMenu.AddColor("selectedColor", "Selected Color", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        s_drawingMenu.AddBool("weightBestEnabled", "Weight Best Target", true);
        s_drawingMenu.AddSlider("weightBestRadius", "Weight Radius", 55, 10, 150);
        s_drawingMenu.AddColor("weightBestColor", "Weight Color", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        s_drawingMenu.AddBool("weightText", "Weight Scores", false);
        s_drawingMenu.AddSlider("weightRange", "Weight Range", 1500, 500, 3000);
        s_drawingMenu.AddSlider("clickBuffer", "Click Buffer", 100, 25, 250);

        EnsurePrioritySliders();
    }

    static Menu* GetMenu() {
        Initialize();
        return s_menu.Raw();
    }

    static void SetExternalControl(bool enabled) {
        Initialize();
        s_externalControl = enabled;
    }

    static bool IsExternalControlEnabled() {
        return s_externalControl;
    }

    static void SetOverride(GetTargetOverride getTarget,
                            GetTargetsOverride getTargets,
                            GetSelectedTargetOverride getSelected,
                            SetSelectedTargetOverride setSelected,
                            ClearSelectedTargetOverride clearSelected,
                            GetPriorityOverride getPriority) {
        s_overrideGetTarget = getTarget;
        s_overrideGetTargets = getTargets;
        s_overrideGetSelectedTarget = getSelected;
        s_overrideSetSelectedTarget = setSelected;
        s_overrideClearSelectedTarget = clearSelected;
        s_overrideGetPriority = getPriority;
    }

    static void ClearOverride() {
        s_overrideGetTarget = nullptr;
        s_overrideGetTargets = nullptr;
        s_overrideGetSelectedTarget = nullptr;
        s_overrideSetSelectedTarget = nullptr;
        s_overrideClearSelectedTarget = nullptr;
        s_overrideGetPriority = nullptr;
    }

    static AIHeroClient GetSelectedTarget() {
        if (s_overrideGetSelectedTarget) {
            return s_overrideGetSelectedTarget();
        }
        return s_selectedTarget;
    }

    static void SetTarget(const AIHeroClient& target) {
        if (s_overrideSetSelectedTarget) {
            s_overrideSetSelectedTarget(target);
            return;
        }
        s_selectedTarget = target;
    }

    static void ClearSelectedTarget() {
        if (s_overrideClearSelectedTarget) {
            s_overrideClearSelectedTarget();
            return;
        }
        s_selectedTarget = {};
    }

    static int GetPriority(const AIHeroClient& target) {
        if (s_overrideGetPriority) {
            return s_overrideGetPriority(target);
        }
        return PriorityFor(target);
    }

    static AIHeroClient GetTarget(float range,
                                  DamageType damageType = DamageType::True,
                                  bool ignoreShields = true,
                                  const Vector3& from = Vector3(),
                                  const std::vector<AIHeroClient>* ignoreChampions = nullptr) {
        if (s_overrideGetTarget) {
            return s_overrideGetTarget(range, damageType, ignoreShields, from, ignoreChampions);
        }
        auto targets = GetTargets(range, damageType, ignoreShields, from, ignoreChampions);
        return targets.empty() ? AIHeroClient() : targets.front();
    }

    static std::vector<AIHeroClient> GetTargets(float range,
                                                DamageType damageType = DamageType::True,
                                                bool ignoreShields = true,
                                                const Vector3& from = Vector3(),
                                                const std::vector<AIHeroClient>* ignoreChampions = nullptr) {
        if (s_overrideGetTargets) {
            return s_overrideGetTargets(range, damageType, ignoreShields, from, ignoreChampions);
        }
        Initialize();

        const Vector3 source = from.IsZero() ? ObjectManager::Player().Position() : from;
        if (s_menu.Bool("focus", true) && s_menu.Bool("forceFocus", false) &&
            IsValidTarget(s_selectedTarget, FLT_MAX, damageType, ignoreShields, source)) {
            return { s_selectedTarget };
        }

        auto enemies = ObjectManager::EnemyHeroes();
        std::vector<AIHeroClient> targets;
        targets.reserve(enemies.size());

        for (const auto& hero : enemies) {
            if (!IsValidTarget(hero, range, damageType, ignoreShields, source)) {
                continue;
            }
            if (ignoreChampions && Contains(*ignoreChampions, hero)) {
                continue;
            }
            targets.push_back(hero);
        }

        OrderTargets(targets, source);

        if (s_menu.Bool("focus", true) && s_selectedTarget.IsValid()) {
            std::stable_sort(targets.begin(), targets.end(), [](const AIHeroClient& a, const AIHeroClient& b) {
                return a.Compare(s_selectedTarget) && !b.Compare(s_selectedTarget);
            });
        }

        return targets;
    }

    static void Update() {
        Initialize();
        if (s_externalControl) {
            return;
        }
        EnsurePrioritySliders();
        UpdateClickSelection();

        const int now = Game::TickCount();
        if (now - s_lastWeightUpdate >= 125) {
            s_lastWeightUpdate = now;
            UpdateWeightCache();
        }
    }

    static void Render() {
        Initialize();
        if (s_externalControl) {
            return;
        }

        if (s_drawingMenu.Bool("selectedEnabled", true) && s_menu.Bool("focus", true) &&
            s_selectedTarget.IsValidTarget()) {
            const float radius = s_selectedTarget.BoundingRadius() +
                static_cast<float>(s_drawingMenu.Slider("selectedRadius", 35));
            Drawing::DrawCircle(s_selectedTarget.Position(), radius,
                ToColor(s_drawingMenu.Raw()->GetColorValue("selectedColor", ImVec4(1, 0, 0, 1))),
                2.0f, 24, true);
        }

        if (CurrentMode() != TargetSelectorMode::Weight) {
            return;
        }

        if (s_drawingMenu.Bool("weightBestEnabled", true) && s_weightBestTarget.IsValidTarget()) {
            const float radius = s_weightBestTarget.BoundingRadius() +
                static_cast<float>(s_drawingMenu.Slider("weightBestRadius", 55));
            Drawing::DrawCircle(s_weightBestTarget.Position(), radius,
                ToColor(s_drawingMenu.Raw()->GetColorValue("weightBestColor", ImVec4(0, 1, 0, 1))),
                2.0f, 24, true);
        }

        if (s_drawingMenu.Bool("weightText", false)) {
            char text[32] = {};
            for (const auto& entry : s_weightTargets) {
                AIHeroClient hero(entry.Address);
                if (!hero.IsValidTarget()) {
                    continue;
                }
                wsprintfA(text, "%.1f", entry.Score);
                Vector3 labelPos = hero.Position();
                labelPos.y += 120.0f;
                Drawing::DrawText(labelPos, text, IM_COL32(255, 255, 255, 230), true, true);
            }
        }
    }

private:
    struct WeightEntry {
        uintptr_t Address = 0;
        float Score = 0.0f;
    };

    static inline bool s_initialized = false;
    static inline bool s_externalControl = false;
    static inline UI::MenuNode s_menu = {};
    static inline UI::MenuNode s_priorityMenu = {};
    static inline UI::MenuNode s_drawingMenu = {};
    static inline AIHeroClient s_selectedTarget = {};
    static inline AIHeroClient s_weightBestTarget = {};
    static inline std::vector<WeightEntry> s_weightTargets = {};
    static inline bool s_lastLeftDown = false;
    static inline int s_lastWeightUpdate = 0;
    static inline GetTargetOverride s_overrideGetTarget = nullptr;
    static inline GetTargetsOverride s_overrideGetTargets = nullptr;
    static inline GetSelectedTargetOverride s_overrideGetSelectedTarget = nullptr;
    static inline SetSelectedTargetOverride s_overrideSetSelectedTarget = nullptr;
    static inline ClearSelectedTargetOverride s_overrideClearSelectedTarget = nullptr;
    static inline GetPriorityOverride s_overrideGetPriority = nullptr;

    static bool Contains(const std::vector<AIHeroClient>& list, const AIHeroClient& hero) {
        return std::any_of(list.begin(), list.end(), [&hero](const AIHeroClient& ignored) {
            return ignored.Compare(hero);
        });
    }

    static ImU32 ToColor(const ImVec4& color) {
        return IM_COL32(
            static_cast<int>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(color.w, 0.0f, 1.0f) * 255.0f));
    }

    static TargetSelectorMode CurrentMode() {
        const int index = s_menu.ListIndex("mode", static_cast<int>(TargetSelectorMode::Weight));
        if (index < static_cast<int>(TargetSelectorMode::Priority) ||
            index > static_cast<int>(TargetSelectorMode::Weight)) {
            return TargetSelectorMode::Weight;
        }
        return static_cast<TargetSelectorMode>(index);
    }

    static bool IsValidTarget(const AIHeroClient& hero,
                              float range,
                              DamageType damageType,
                              bool ignoreShields,
                              const Vector3& from) {
        if (!hero.IsValid() || !hero.IsAlive() || hero.Health() <= 0.0f) {
            return false;
        }
        const auto player = ObjectManager::Player();
        const float finalRange = range <= 0.0f ? player.GetRealAutoAttackRange(hero) : range;
        if (finalRange > 0.0f && hero.DistanceSquared(from) > finalRange * finalRange) {
            return false;
        }
        return !Utils::Invulnerable::Check(hero, damageType, ignoreShields);
    }

    static void EnsurePrioritySliders() {
        if (!s_priorityMenu.IsValid()) {
            return;
        }

        const bool autoPriority = s_priorityMenu.Bool("autoPriority", true);
        for (const auto& hero : ObjectManager::EnemyHeroes()) {
            const std::string champion = hero.CharacterName();
            if (champion.empty() || s_priorityMenu.HasItem(champion)) {
                continue;
            }
            const int defaultPriority = TargetSelectorData::GetDefaultPriority(champion);
            s_priorityMenu.AddSlider(champion, champion, autoPriority ? defaultPriority : 1, 1, 5);
        }
    }

    static int PriorityFor(const AIHeroClient& hero) {
        const std::string champion = hero.CharacterName();
        if (!champion.empty() && s_priorityMenu.IsValid()) {
            return TargetSelectorData::ClampPriority(
                s_priorityMenu.Slider(champion, TargetSelectorData::GetDefaultPriority(champion)));
        }
        return 1;
    }

    static float AttackCountToKill(const AIHeroClient& hero) {
        const auto player = ObjectManager::Player();
        const float damage = std::max(1.0f, player.GetAutoAttackDamage(hero));
        return std::ceil(std::max(1.0f, hero.Health()) / damage);
    }

    static float WeightScore(const AIHeroClient& hero, const Vector3& from) {
        const auto player = ObjectManager::Player();
        const Vector3 cursor = Game::CursorPos();
        const float hpPct = std::clamp(hero.HealthPercent(), 0.0f, 100.0f);
        const float priority = static_cast<float>(PriorityFor(hero)) * 100.0f;
        const float lowHealth = 100.0f - hpPct;
        const float nearPlayer = std::max(0.0f, 1200.0f - hero.Distance(from)) * 0.04f;
        const float nearCursor = cursor.IsZero() ? 0.0f : std::max(0.0f, 1600.0f - hero.Distance(cursor)) * 0.03f;
        const float killable = player.GetAutoAttackDamage(hero) >= hero.Health() ? 75.0f : 0.0f;
        return priority + lowHealth + nearPlayer + nearCursor + killable;
    }

    static void OrderTargets(std::vector<AIHeroClient>& targets, const Vector3& from) {
        switch (CurrentMode()) {
        case TargetSelectorMode::Priority:
            std::sort(targets.begin(), targets.end(), [](const AIHeroClient& a, const AIHeroClient& b) {
                const int pa = PriorityFor(a);
                const int pb = PriorityFor(b);
                return pa == pb ? a.Health() < b.Health() : pa > pb;
            });
            break;
        case TargetSelectorMode::Closest:
            std::sort(targets.begin(), targets.end(), [&from](const AIHeroClient& a, const AIHeroClient& b) {
                return a.DistanceSquared(from) < b.DistanceSquared(from);
            });
            break;
        case TargetSelectorMode::LeastHealth:
            std::sort(targets.begin(), targets.end(), [](const AIHeroClient& a, const AIHeroClient& b) {
                return a.Health() < b.Health();
            });
            break;
        case TargetSelectorMode::MostAttackDamage:
            std::sort(targets.begin(), targets.end(), [](const AIHeroClient& a, const AIHeroClient& b) {
                return a.TotalAttackDamage() > b.TotalAttackDamage();
            });
            break;
        case TargetSelectorMode::MostAbilityPower:
            std::sort(targets.begin(), targets.end(), [](const AIHeroClient& a, const AIHeroClient& b) {
                return a.AbilityPower() > b.AbilityPower();
            });
            break;
        case TargetSelectorMode::NearMouse: {
            const Vector3 cursor = Game::CursorPos();
            std::sort(targets.begin(), targets.end(), [&cursor](const AIHeroClient& a, const AIHeroClient& b) {
                return a.DistanceSquared(cursor) < b.DistanceSquared(cursor);
            });
            break;
        }
        case TargetSelectorMode::LessAttacksToKill:
        case TargetSelectorMode::LessCastsToKill:
            std::sort(targets.begin(), targets.end(), [](const AIHeroClient& a, const AIHeroClient& b) {
                const float aa = AttackCountToKill(a);
                const float ab = AttackCountToKill(b);
                return aa == ab ? a.Health() < b.Health() : aa < ab;
            });
            break;
        case TargetSelectorMode::Weight:
        default:
            std::sort(targets.begin(), targets.end(), [&from](const AIHeroClient& a, const AIHeroClient& b) {
                return WeightScore(a, from) > WeightScore(b, from);
            });
            break;
        }
    }

    static void UpdateClickSelection() {
        const bool leftDown = (::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (!leftDown || s_lastLeftDown || !Game::CanProcessInput()) {
            s_lastLeftDown = leftDown;
            return;
        }
        s_lastLeftDown = leftDown;

        const Vector3 cursor = Game::CursorPos();
        const float clickBuffer = static_cast<float>(s_drawingMenu.Slider("clickBuffer", 100));
        AIHeroClient closest = {};
        float closestDist = FLT_MAX;
        for (const auto& hero : ObjectManager::EnemyHeroes()) {
            if (!hero.IsValidTarget()) {
                continue;
            }
            const float dist = hero.Distance(cursor);
            if (dist < hero.BoundingRadius() + clickBuffer && dist < closestDist) {
                closest = hero;
                closestDist = dist;
            }
        }
        s_selectedTarget = closest;
    }

    static void UpdateWeightCache() {
        s_weightTargets.clear();
        s_weightBestTarget = {};

        if (CurrentMode() != TargetSelectorMode::Weight ||
            (!s_drawingMenu.Bool("weightBestEnabled", true) && !s_drawingMenu.Bool("weightText", false))) {
            return;
        }

        const auto player = ObjectManager::Player();
        if (!player.IsValid()) {
            return;
        }

        const float range = static_cast<float>(s_drawingMenu.Slider("weightRange", 1500));
        auto targets = GetTargets(range, DamageType::True, true, player.Position(), nullptr);
        s_weightTargets.reserve(targets.size());
        for (const auto& target : targets) {
            s_weightTargets.push_back({ target.Address(), WeightScore(target, player.Position()) });
        }

        if (!targets.empty()) {
            s_weightBestTarget = targets.front();
        }
    }
};

} // namespace SDK
