#pragma once

#include "../../Enumerations/DamageType.h"
#include "../../Core/Objects.h"
#include "../Spells/Spell.h"
#include "../../UI/UI.h"
#include "TargetSelectorDefault.h"

#include <string>
#include <vector>

namespace SDK {

enum class TargetSelectorMode : int {
    Priority = 0,
    Closest = 1,
    LeastHealth = 2,
    MostAttackDamage = 3,
    MostAbilityPower = 4,
    NearMouse = 5,
    LessAttacksToKill = 6,
    LessCastsToKill = 7,
    Weight = 8
};

class TargetSelector {
public:
    using OverrideGetTargetFn = AIHeroClient(*)(float, DamageType, const Vector3&);
    using OverrideGetTargetsFn = std::vector<AIHeroClient>(*)(float, DamageType, const Vector3&);
    using OverrideGetSelectedTargetFn = AIHeroClient(*)();
    using OverrideSetSelectedTargetFn = void(*)(const AIHeroClient&);
    using OverrideClearSelectedTargetFn = void(*)();
    using OverrideGetPriorityFn = int(*)(const AIHeroClient&);

    static void Initialize() {
        if (s_initialized) return;
        s_initialized = true;

        auto root = UI::CreateMenu("targetselector", "Target Selector");
        s_menu = root.Raw();
        TargetSelectorDefault::Initialize(s_menu);
    }

    static void Shutdown() {
        ClearOverride();
        TargetSelectorDefault::Reset();
        s_menu = nullptr;
        s_mode = TargetSelectorMode::Priority;
        s_initialized = false;
    }

    static Menu* GetMenu() { return s_menu; }

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
        if (!HasOverride()) {
            TargetSelectorDefault::Update();
        }
    }

    static AIHeroClient GetSelectedTarget() {
        if (HasOverride() && s_overrideGetSelectedTarget) {
            return s_overrideGetSelectedTarget();
        }
        return TargetSelectorDefault::GetSelectedTarget();
    }

    static void SetSelectedTarget(const AIHeroClient& target) {
        if (HasOverride() && s_overrideSetSelectedTarget) {
            s_overrideSetSelectedTarget(target);
            return;
        }
        TargetSelectorDefault::SetSelectedTarget(target);
    }

    static void ClearSelectedTarget() {
        if (HasOverride() && s_overrideClearSelectedTarget) {
            s_overrideClearSelectedTarget();
            return;
        }
        TargetSelectorDefault::ClearSelectedTarget();
    }

    static AIHeroClient GetForcedTarget() {
        return TargetSelectorDefault::ForcedTarget();
    }

    static void SetForcedTarget(const AIHeroClient& target) {
        TargetSelectorDefault::SetForcedTarget(target);
    }

    static void ClearForcedTarget() {
        TargetSelectorDefault::ClearForcedTarget();
    }

    static int GetPriority(const AIHeroClient& hero) {
        if (HasOverride() && s_overrideGetPriority) {
            return s_overrideGetPriority(hero);
        }
        return TargetSelectorDefault::GetPriority(hero);
    }

    static TargetSelectorMode Mode() { return s_mode; }
    static void SetMode(TargetSelectorMode mode) { s_mode = mode; }

    static AIHeroClient GetTarget(const Spell& spell, bool ignoreShields = true) {
        (void)ignoreShields;
        return GetTarget(spell.GetRange(), spell.DamageType, spell.GetSource());
    }

    static AIHeroClient GetTarget(float range, DamageType damageType = DamageType::True, const Vector3& from = Vector3()) {
        if (HasOverride()) {
            return s_overrideGetTarget(range, damageType, from);
        }
        return TargetSelectorDefault::GetTarget(range, damageType, from);
    }

    static bool IsValidTarget(const AIHeroClient& hero,
                              float range,
                              DamageType damageType = DamageType::True,
                              const Vector3& from = Vector3()) {
        if (!hero.IsValid()) return false;
        const auto origin = from.IsZero() ? ObjectManager::Player().Position() : from;
        if (!hero.IsValidTarget(range, origin)) return false;
        return TargetSelectorDefault::EffectiveHealth(hero, damageType) > 0.0f;
    }

    static std::vector<AIHeroClient> GetTargets(float range, DamageType damageType = DamageType::True, const Vector3& from = Vector3()) {
        if (HasOverride()) {
            return s_overrideGetTargets(range, damageType, from);
        }
        return TargetSelectorDefault::GetTargets(range, damageType, from);
    }

    static void Render() {
        if (!HasOverride()) {
            TargetSelectorDefault::Render();
        }
    }

private:
    static inline bool s_initialized = false;
    static inline Menu* s_menu = nullptr;
    static inline TargetSelectorMode s_mode = TargetSelectorMode::Priority;
    static inline OverrideGetTargetFn s_overrideGetTarget = nullptr;
    static inline OverrideGetTargetsFn s_overrideGetTargets = nullptr;
    static inline OverrideGetSelectedTargetFn s_overrideGetSelectedTarget = nullptr;
    static inline OverrideSetSelectedTargetFn s_overrideSetSelectedTarget = nullptr;
    static inline OverrideClearSelectedTargetFn s_overrideClearSelectedTarget = nullptr;
    static inline OverrideGetPriorityFn s_overrideGetPriority = nullptr;
};

} // namespace SDK
