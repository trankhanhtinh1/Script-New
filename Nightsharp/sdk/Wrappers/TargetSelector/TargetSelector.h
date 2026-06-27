#pragma once

#include "TargetSelectorDrawing.h"
#include "TargetSelectorHumanizer.h"
#include "TargetSelectorMode.h"
#include "TargetSelectorSelected.h"
#include "../Spells/Spell.h"

#include "../../Extensions/Unit.h"
#include "../../UI/UI.h"
#include "../../Utils/AutoAttack.h"
#include "../../Utils/Invulnerable.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace SDK {

class TargetSelector {
public:
    explicit TargetSelector(Menu* parentMenu) {
        Instance_() = this;
        parentMenu_ = parentMenu;
        internalMenu_ = new Menu("targetselector", "TargetSelector");
        parentMenu_->Add(internalMenu_);

        selected_ = new TargetSelectorSelected(internalMenu_);
        humanizer_ = new TargetSelectorHumanizer(internalMenu_);
        mode_ = new TargetSelectorMode(internalMenu_);
        drawing_ = new TargetSelectorDrawing(internalMenu_, selected_, mode_);
    }

    static TargetSelector* Instance() { return Instance_(); }

    TargetSelectorHumanizer* Humanizer() const { return humanizer_; }
    TargetSelectorMode* Mode() const { return mode_; }
    TargetSelectorSelected* Selected() const { return selected_; }

    AIHeroClient GetSelectedTarget() {
        return selected_ ? selected_->Target() : AIHeroClient();
    }

    void SetTarget(const AIHeroClient& target) {
        if (selected_) selected_->Target(target);
    }

    AIHeroClient GetTarget(
        Spell* spell,
        bool ignoreShields = true,
        const std::vector<AIHeroClient>* ignoreChampions = nullptr)
    {
        float maxBounding = 50.0f;
        for (const auto& hero : GameObjects::EnemyHeroes()) {
            if (hero.BoundingRadius() > maxBounding) {
                maxBounding = hero.BoundingRadius();
            }
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
        auto targets = GetTargets(range, damageType, ignoreShields, from, ignoreChampions);
        return targets.empty() ? AIHeroClient() : targets[0];
    }

    AIHeroClient GetTargetNoCollision(
        Spell* spell,
        bool ignoreShields = true,
        const std::vector<AIHeroClient>* ignoreChampions = nullptr)
    {
        auto targets = GetTargets(spell->Range, spell->DamageType, ignoreShields, spell->From, ignoreChampions);
        for (auto& t : targets) {
            if (spell->GetPrediction(t).Hitchance != HitChance::Collision) {
                return t;
            }
        }
        return AIHeroClient();
    }

    std::vector<AIHeroClient> GetTargets(
        float range,
        DamageType damageType = DamageType::True,
        bool ignoreShields = true,
        const Vector3& from = Vector3(),
        const std::vector<AIHeroClient>* ignoreChampions = nullptr)
    {
        if (selected_->Focus() && selected_->Force()) {
            if (IsValidTarget(selected_->Target(), FLT_MAX, damageType, ignoreShields, from)) {
                return { selected_->Target() };
            }
        }

        auto temp = GameObjects::EnemyHeroes();
        auto targets = humanizer_->FilterTargets(std::move(temp));

        targets.erase(
            std::remove_if(targets.begin(), targets.end(),
                [ignoreChampions](const AIHeroClient& h) {
                    if (!ignoreChampions || ignoreChampions->empty()) return false;
                    for (const auto& ign : *ignoreChampions) {
                        if (ign.Compare(h)) return true;
                    }
                    return false;
                }),
            targets.end());

        targets.erase(
            std::remove_if(targets.begin(), targets.end(),
                [this, range, damageType, ignoreShields, &from](const AIHeroClient& h) {
                    return !IsValidTarget(h, range, damageType, ignoreShields, from);
                }),
            targets.end());

        targets = mode_->OrderChampions(targets);

        if (selected_->Focus() && selected_->Target().IsValid()) {
            const auto& selTarget = selected_->Target();
            std::stable_sort(targets.begin(), targets.end(),
                [&selTarget](const AIHeroClient& a, const AIHeroClient& b) {
                    const bool aIs = a.Compare(selTarget);
                    const bool bIs = b.Compare(selTarget);
                    return aIs && !bIs;
                });
        }

        return targets;
    }

    static bool IsValidTarget(
        const AIHeroClient& hero,
        float range,
        DamageType damageType = DamageType::True,
        bool ignoreShields = true,
        const Vector3& from = Vector3())
    {
        if (!Extensions::IsValidTarget(hero)) {
            return false;
        }

        if (range < FLT_MAX) {
            const Vector3 origin = from.IsZero()
                ? (GameObjects::Player().IsValid() ? GameObjects::Player().Position() : Vector3())
                : from;
            const float effectiveRange = range <= 0.0f ? Utils::AutoAttack::GetRealAutoAttackRange(hero) : range;

            if (origin.IsValid() && !origin.IsZero() &&
                origin.DistanceSqr2D(hero.Position()) >= effectiveRange * effectiveRange) {
                return false;
            }
        }

        return !Utils::Invulnerable::Check(hero, damageType, ignoreShields);
    }

private:
    static TargetSelector*& Instance_() {
        static TargetSelector* instance = nullptr;
        return instance;
    }

    Menu* parentMenu_ = nullptr;
    Menu* internalMenu_ = nullptr;
    TargetSelectorHumanizer* humanizer_ = nullptr;
    TargetSelectorSelected* selected_ = nullptr;
    TargetSelectorMode* mode_ = nullptr;
    TargetSelectorDrawing* drawing_ = nullptr;
};

} // namespace SDK
