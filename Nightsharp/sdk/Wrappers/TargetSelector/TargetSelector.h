#pragma once

#include "ITargetSelector.h"
#include "TargetSelectorRegistry.h"
#include "TargetSelectorSdk.h"
#include "../Spells/Spell.h"

#include <algorithm>
#include <string>
#include <vector>

namespace SDK {

class TargetSelector {
public:
    explicit TargetSelector(Menu* parentMenu)
        : sdkImplementation_(parentMenu)
    {
        Instance_() = this;
        AddTargetSelector("SDK", &sdkImplementation_);
        SetTargetSelector("SDK");
    }

    ~TargetSelector() {
        RemoveTargetSelector("SDK");
        if (Instance_() == this) Instance_() = nullptr;
    }

    static TargetSelector* Instance() { return Instance_(); }
    static ITargetSelector* Implementation() { return Registry_().Implementation(); }

    static bool AddTargetSelector(const std::string& name, ITargetSelector* implementation) {
        return Registry_().Add(name, implementation);
    }

    static bool SetTargetSelector(const std::string& name) {
        return Registry_().Set(name);
    }

    static ITargetSelector* GetTargetSelector(const std::string& name) {
        return Registry_().Get(name);
    }

    static bool RemoveTargetSelector(const std::string& name) {
        return Registry_().Remove(name);
    }

    static const std::string& CurrentTargetSelectorName() { return Registry_().CurrentName(); }

    TargetSelectorHumanizer* Humanizer() const { return sdkImplementation_.Humanizer(); }
    TargetSelectorMode* Mode() const { return sdkImplementation_.Mode(); }
    TargetSelectorSelected* Selected() const { return sdkImplementation_.Selected(); }

    AIHeroClient GetSelectedTarget() const {
        return Implementation() ? Implementation()->GetSelectedTarget() : AIHeroClient();
    }

    void SetTarget(const AIHeroClient& target) {
        if (Implementation()) Implementation()->SetTarget(target);
    }

    int GetPriority(const AIHeroClient& target) const {
        return Implementation() ? Implementation()->GetPriority(target) : 1;
    }

    AIHeroClient GetTarget(
        Spell* spell,
        bool ignoreShields = true,
        const std::vector<AIHeroClient>* ignoreChampions = nullptr)
    {
        if (!spell) return {};
        float maxBounding = 50.0f;
        for (const auto& hero : GameObjects::EnemyHeroes()) {
            if (hero.BoundingRadius() > maxBounding) maxBounding = hero.BoundingRadius();
        }
        return GetTarget(
            spell->Range + spell->Width + maxBounding,
            spell->DamageType,
            ignoreShields,
            spell->From,
            ignoreChampions);
    }

    AIHeroClient GetTarget(
        float range,
        DamageType damageType = DamageType::True,
        bool ignoreShields = true,
        const Vector3& from = Vector3(),
        const std::vector<AIHeroClient>* ignoreChampions = nullptr)
    {
        return Implementation()
            ? Implementation()->GetTarget(range, damageType, ignoreShields, from, ignoreChampions)
            : AIHeroClient();
    }

    AIHeroClient GetTargetNoCollision(
        Spell* spell,
        bool ignoreShields = true,
        const std::vector<AIHeroClient>* ignoreChampions = nullptr)
    {
        if (!spell) return {};
        auto targets = GetTargets(
            spell->Range, spell->DamageType, ignoreShields, spell->From, ignoreChampions);
        for (auto& target : targets) {
            if (spell->GetPrediction(target).Hitchance != HitChance::Collision) return target;
        }
        return {};
    }

    std::vector<AIHeroClient> GetTargets(
        float range,
        DamageType damageType = DamageType::True,
        bool ignoreShields = true,
        const Vector3& from = Vector3(),
        const std::vector<AIHeroClient>* ignoreChampions = nullptr)
    {
        return Implementation()
            ? Implementation()->GetTargets(range, damageType, ignoreShields, from, ignoreChampions)
            : std::vector<AIHeroClient>();
    }

    static bool IsValidTarget(
        const AIHeroClient& hero,
        float range,
        DamageType damageType = DamageType::True,
        bool ignoreShields = true,
        const Vector3& from = Vector3())
    {
        return TargetSelectorSdk::IsValidTarget(hero, range, damageType, ignoreShields, from);
    }

private:
    static TargetSelector*& Instance_() {
        static TargetSelector* instance = nullptr;
        return instance;
    }

    static TargetSelectorRegistry<ITargetSelector>& Registry_() {
        static TargetSelectorRegistry<ITargetSelector> registry;
        return registry;
    }

    TargetSelectorSdk sdkImplementation_;
};

inline AIHeroClient Spell::GetTarget(
    float extraRange,
    bool accountForCollision,
    const std::vector<AIHeroClient>& champsToIgnore) const
{
    auto* selector = TargetSelector::Instance();
    if (!selector) return {};
    const std::vector<AIHeroClient>* ignore = champsToIgnore.empty() ? nullptr : &champsToIgnore;
    if (accountForCollision) {
        return selector->GetTargetNoCollision(const_cast<Spell*>(this), true, ignore);
    }
    return selector->GetTarget(CurrentRange() + extraRange, DamageType, true, From, ignore);
}

} // namespace SDK
