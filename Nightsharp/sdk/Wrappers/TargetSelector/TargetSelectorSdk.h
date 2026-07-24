#pragma once

#include "ITargetSelector.h"
#include "TargetSelectorDrawing.h"
#include "TargetSelectorHumanizer.h"
#include "TargetSelectorMode.h"
#include "TargetSelectorSelected.h"
#include "../../Extensions/Unit.h"
#include "../../UI/UI.h"
#include "../../Utils/AutoAttack.h"
#include "../../Utils/Invulnerable.h"

#include <algorithm>
#include <cfloat>
#include <fstream>
#include <vector>

#ifndef NIGHTSHARP_ORBWALKER_LOGGING
#define NIGHTSHARP_ORBWALKER_LOGGING 0
#endif

namespace SDK {

class TargetSelectorSdk final : public ITargetSelector {
public:
    explicit TargetSelectorSdk(Menu* parentMenu) {
        internalMenu_ = new Menu("targetselector", "Target Selector");
        if (parentMenu) {
            parentMenu->Add(internalMenu_);
        } else {
            internalMenu_->Root = true;
            internalMenu_->Attach();
        }

        selected_ = new TargetSelectorSelected(internalMenu_);
        humanizer_ = new TargetSelectorHumanizer(internalMenu_);
        mode_ = new TargetSelectorMode(internalMenu_);
        drawing_ = new TargetSelectorDrawing(
            internalMenu_, selected_, mode_, [this]() { return GetTarget(2000.0f); });
    }

    ~TargetSelectorSdk() override {
        delete drawing_;
        delete mode_;
        delete humanizer_;
        delete selected_;
        drawing_ = nullptr;
        mode_ = nullptr;
        humanizer_ = nullptr;
        selected_ = nullptr;
    }

    TargetSelectorHumanizer* Humanizer() const { return humanizer_; }
    TargetSelectorMode* Mode() const { return mode_; }
    TargetSelectorSelected* Selected() const { return selected_; }

    AIHeroClient GetSelectedTarget() const override {
        return selected_ ? selected_->Target() : AIHeroClient();
    }

    void SetTarget(const AIHeroClient& target) override {
        if (selected_) selected_->Target(target);
    }

    AIHeroClient GetTarget(
        float range,
        DamageType damageType = DamageType::True,
        bool ignoreShields = true,
        const Vector3& from = Vector3(),
        const std::vector<AIHeroClient>* ignoreChampions = nullptr) override
    {
        auto targets = GetTargets(range, damageType, ignoreShields, from, ignoreChampions);
        return targets.empty() ? AIHeroClient() : targets[0];
    }

    std::vector<AIHeroClient> GetTargets(
        float range,
        DamageType damageType = DamageType::True,
        bool ignoreShields = true,
        const Vector3& from = Vector3(),
        const std::vector<AIHeroClient>* ignoreChampions = nullptr) override
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
                [range, damageType, ignoreShields, &from](const AIHeroClient& h) {
                    return !IsValidTarget(h, range, damageType, ignoreShields, from);
                }),
            targets.end());

        targets = mode_->OrderChampions(targets);

        if (selected_->Focus() && selected_->Target().IsValid()) {
            const auto& selectedTarget = selected_->Target();
            std::stable_sort(targets.begin(), targets.end(),
                [&selectedTarget](const AIHeroClient& a, const AIHeroClient& b) {
                    const bool aIs = a.Compare(selectedTarget);
                    const bool bIs = b.Compare(selectedTarget);
                    return aIs && !bIs;
                });
        }
        return targets;
    }

    int GetPriority(const AIHeroClient& target) const override {
        auto* priority = Modes::Priority::Instance();
        return priority ? priority->GetHeroPriority(target) : 1;
    }

    void Suspend() override {
        if (suspended_) return;
        if (drawing_) drawing_->Suspend();
        if (humanizer_) humanizer_->Suspend();
        if (selected_) selected_->Suspend();
        if (internalMenu_) internalMenu_->Visible = false;
        suspended_ = true;
    }

    void Resume() override {
        if (!suspended_) return;
        if (selected_) selected_->Resume();
        if (humanizer_) humanizer_->Resume();
        if (drawing_) drawing_->Resume();
        if (internalMenu_) internalMenu_->Visible = true;
        suspended_ = false;
    }

    static bool IsValidTarget(
        const AIHeroClient& hero,
        float range,
        DamageType damageType = DamageType::True,
        bool ignoreShields = true,
        const Vector3& from = Vector3())
    {
#if NIGHTSHARP_ORBWALKER_LOGGING
        std::ofstream tsLog("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
        tsLog << "  [TS] Eval: " << hero.CharacterName() << "\n";
#endif
        if (!Extensions::IsValidTarget(hero)) {
#if NIGHTSHARP_ORBWALKER_LOGGING
            tsLog << "    -> Failed Extensions::IsValidTarget\n";
#endif
            return false;
        }

        if (range < FLT_MAX) {
            const Vector3 origin = from.IsZero()
                ? (GameObjects::Player().IsValid() ? GameObjects::Player().Position() : Vector3())
                : from;
            const float effectiveRange = range <= 0.0f
                ? Utils::AutoAttack::GetRealAutoAttackRange(GameObjects::Player(), hero)
                : range;
            const float distanceSquared = origin.DistanceSqr2D(hero.Position());

#if NIGHTSHARP_ORBWALKER_LOGGING
            tsLog << "    -> DistSqr: " << distanceSquared
                  << " EffRangeSqr: " << (effectiveRange * effectiveRange) << "\n";
#endif
            if (origin.IsValid() && !origin.IsZero() &&
                distanceSquared >= effectiveRange * effectiveRange) {
#if NIGHTSHARP_ORBWALKER_LOGGING
                tsLog << "    -> Failed Distance Check\n";
#endif
                return false;
            }
        }

        if (Utils::Invulnerable::Check(hero, damageType, ignoreShields)) {
#if NIGHTSHARP_ORBWALKER_LOGGING
            tsLog << "    -> Failed Invulnerable Check\n";
#endif
            return false;
        }

#if NIGHTSHARP_ORBWALKER_LOGGING
        tsLog << "    -> SUCCESS\n";
#endif
        return true;
    }

private:
    Menu* internalMenu_ = nullptr;
    TargetSelectorHumanizer* humanizer_ = nullptr;
    TargetSelectorSelected* selected_ = nullptr;
    TargetSelectorMode* mode_ = nullptr;
    TargetSelectorDrawing* drawing_ = nullptr;
    bool suspended_ = false;
};

} // namespace SDK
