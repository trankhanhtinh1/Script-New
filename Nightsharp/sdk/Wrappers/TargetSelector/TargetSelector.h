#pragma once

#include "../../Enumerations/DamageType.h"
#include "../../Enumerations/TargetSelectorMode.h"
#include "../../Core/Objects.h"
#include "../Spells/Spell.h"
#include "../../UI/UI.h"
#include "HeroVisibleEntry.h"
#include "TargetSelectorDrawing.h"
#include "TargetSelectorHumanizer.h"
#include "TargetSelectorMode.h"
#include "TargetSelectorSelected.h"

#include <algorithm>
#include <string>
#include <vector>

namespace SDK {

class TargetSelector {
public:
    using OverrideGetTargetFn = AIHeroClient(*)(float, DamageType, const Vector3&);
    using OverrideGetTargetsFn = std::vector<AIHeroClient>(*)(float, DamageType, const Vector3&);
    using OverrideGetSelectedTargetFn = AIHeroClient(*)();
    using OverrideSetSelectedTargetFn = void(*)(const AIHeroClient&);
    using OverrideClearSelectedTargetFn = void(*)();
    using OverrideGetPriorityFn = int(*)(const AIHeroClient&);

    static void Initialize() {
        if (s_initialized) {
            return;
        }
        s_initialized = true;

        CrashTelemetry::SetStage("SDK::TS::CreateMenu");
        auto root = UI::CreateMenu("targetselector", "Target Selector");
        s_menu = root.Raw();
        CrashTelemetry::SetStage("SDK::TS::ModeManager");
        TargetSelectorModes::TargetSelectorModeManager::Initialize(s_menu);
        CrashTelemetry::SetStage("SDK::TS::Selected");
        TargetSelectorSelected::Initialize(s_menu);
        CrashTelemetry::SetStage("SDK::TS::Drawing");
        TargetSelectorDrawing::Initialize(s_menu);
        CrashTelemetry::SetStage("SDK::TS::Humanizer");
        TargetSelectorHumanizer::Initialize(s_menu);
        CrashTelemetry::SetStage("SDK::TS::Done");
    }

    static Menu* GetMenu() {
        return s_menu;
    }

    static void SetOverride(OverrideGetTargetFn getTarget,
                            OverrideGetTargetsFn getTargets,
                            OverrideGetSelectedTargetFn getSelectedTarget = nullptr,
                            OverrideSetSelectedTargetFn setSelectedTarget = nullptr,
                            OverrideClearSelectedTargetFn clearSelectedTarget = nullptr,
                            OverrideGetPriorityFn getPriority = nullptr) {
        s_overrideGetTarget = getTarget;
        s_overrideGetTargets = getTargets;
        s_overrideGetSelectedTarget = getSelectedTarget;
        s_overrideSetSelectedTarget = setSelectedTarget;
        s_overrideClearSelectedTarget = clearSelectedTarget;
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

    static bool HasOverride() {
        return s_overrideGetTarget != nullptr && s_overrideGetTargets != nullptr;
    }

    static void Update() {
        if (HasOverride()) {
            return;
        }
        TargetSelectorSelected::Update();
    }

    static AIHeroClient GetSelectedTarget() {
        if (HasOverride() && s_overrideGetSelectedTarget) {
            return s_overrideGetSelectedTarget();
        }
        return TargetSelectorSelected::Target();
    }

    static void SetSelectedTarget(const AIHeroClient& target) {
        if (HasOverride() && s_overrideSetSelectedTarget) {
            s_overrideSetSelectedTarget(target);
            return;
        }
        TargetSelectorSelected::SetTarget(target);
    }

    static void ClearSelectedTarget() {
        if (HasOverride() && s_overrideClearSelectedTarget) {
            s_overrideClearSelectedTarget();
            return;
        }
        TargetSelectorSelected::ClearTarget();
    }

    static AIHeroClient GetForcedTarget() {
        return TargetSelectorSelected::ForcedTarget();
    }

    static void SetForcedTarget(const AIHeroClient& target) {
        TargetSelectorSelected::SetForcedTarget(target);
    }

    static void ClearForcedTarget() {
        TargetSelectorSelected::ClearForcedTarget();
    }

    static int GetPriority(const AIHeroClient& hero) {
        if (HasOverride() && s_overrideGetPriority) {
            return s_overrideGetPriority(hero);
        }
        return TargetSelectorModes::TargetSelectorModeManager::GetPriority(hero);
    }

    static TargetSelectorMode Mode() {
        return TargetSelectorModes::TargetSelectorModeManager::Current(s_menu);
    }

    static void SetMode(TargetSelectorMode mode) {
        TargetSelectorModes::TargetSelectorModeManager::SetCurrent(s_menu, mode);
    }

    static AIHeroClient GetTarget(const Spell& spell, bool ignoreShields = true) {
        (void)ignoreShields;
        return GetTarget(spell.GetRange(), spell.DamageType, spell.GetSource());
    }

    static AIHeroClient GetTarget(float range, DamageType damageType = DamageType::True, const Vector3& from = Vector3()) {
        if (HasOverride()) {
            return s_overrideGetTarget(range, damageType, from);
        }
        const auto targets = GetTargets(range, damageType, from);
        return targets.empty() ? AIHeroClient() : targets.front();
    }

    static bool IsValidTarget(const AIHeroClient& hero,
                              float range,
                              DamageType damageType = DamageType::True,
                              const Vector3& from = Vector3()) {
        if (!hero.IsValid()) {
            return false;
        }
        const auto origin = from.IsZero() ? ObjectManager::Player().Position() : from;
        if (!hero.IsValidTarget(range, origin)) {
            return false;
        }
        return TargetSelectorModes::ITargetSelectorMode::EffectiveHealth(hero, damageType) > 0.0f;
    }

    static std::vector<AIHeroClient> GetTargets(float range, DamageType damageType = DamageType::True, const Vector3& from = Vector3()) {
        if (HasOverride()) {
            return s_overrideGetTargets(range, damageType, from);
        }
        const auto player = ObjectManager::Player();
        const Vector3 origin = from.IsZero() ? player.Position() : from;

        std::vector<AIHeroClient> out;
        out.reserve(16);

        auto pushUnique = [&](const AIHeroClient& hero) {
            if (!hero.IsValid() || !hero.IsValidTarget(range, origin)) {
                return;
            }
            for (const auto& existing : out) {
                if (existing.NetworkId() == hero.NetworkId()) {
                    return;
                }
            }
            out.push_back(hero);
        };

        const auto forced = TargetSelectorSelected::ForcedTarget();
        if (forced.IsValidTarget(range, origin)) {
            pushUnique(forced);
            return out;
        }

        const auto selected = TargetSelectorSelected::Target();
        if (TargetSelectorSelected::Force(s_menu) && selected.IsValidTarget(range, origin)) {
            pushUnique(selected);
            return out;
        }

        const auto enemies = ObjectManager::EnemyHeroes();
        for (const auto& enemy : enemies) {
            if (!enemy.IsValidTarget(range, origin)) {
                continue;
            }
            pushUnique(enemy);
        }

        out = TargetSelectorModes::TargetSelectorModeManager::Order(s_menu, out, origin, damageType);
        const auto stabilized = TargetSelectorHumanizer::Choose(s_menu, out, range, origin);
        if (stabilized.IsValid()) {
            for (auto it = out.begin(); it != out.end(); ++it) {
                if (it->NetworkId() == stabilized.NetworkId()) {
                    std::rotate(out.begin(), it, it + 1);
                    break;
                }
            }
        }

        const auto selectedFocus = TargetSelectorSelected::Target();
        if (TargetSelectorSelected::Focus(s_menu) && selectedFocus.IsValid()) {
            for (auto it = out.begin(); it != out.end(); ++it) {
                if (it->NetworkId() == selectedFocus.NetworkId()) {
                    std::rotate(out.begin(), it, it + 1);
                    break;
                }
            }
        }

        return out;
    }

    static void Render() {
        if (HasOverride()) {
            return;
        }
        const auto current = GetTarget(1500.0f, DamageType::Physical, ObjectManager::Player().Position());
        TargetSelectorDrawing::Render(s_menu, TargetSelectorSelected::Target(), TargetSelectorSelected::ForcedTarget(), current);
    }

private:
    static inline bool s_initialized = false;
    static inline Menu* s_menu = nullptr;
    static inline OverrideGetTargetFn s_overrideGetTarget = nullptr;
    static inline OverrideGetTargetsFn s_overrideGetTargets = nullptr;
    static inline OverrideGetSelectedTargetFn s_overrideGetSelectedTarget = nullptr;
    static inline OverrideSetSelectedTargetFn s_overrideSetSelectedTarget = nullptr;
    static inline OverrideClearSelectedTargetFn s_overrideClearSelectedTarget = nullptr;
    static inline OverrideGetPriorityFn s_overrideGetPriority = nullptr;
};

} // namespace SDK
