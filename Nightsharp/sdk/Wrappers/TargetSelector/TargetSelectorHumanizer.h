#pragma once

#include "HeroVisibleEntry.h"
#include "../../Core/Variables.h"
#include "../../Events/Events.h"
#include "../../GameObjects/GameObjects.h"
#include "../../GameObjects/ObjectManager.h"
#include "../../UI/UI.h"

#include <algorithm>
#include <cmath>
#include <vector>

#ifndef NIGHTSHARP_TARGETSELECTOR_HUMANIZER_LIFECYCLE
#define NIGHTSHARP_TARGETSELECTOR_HUMANIZER_LIFECYCLE 1
#endif

namespace SDK {

class TargetSelectorHumanizer {
public:
    explicit TargetSelectorHumanizer(Menu* menu) {
        Ptr() = this;
        menu_ = menu;

        auto* slider = menu_->Add(new MenuSlider("fowDelay", "Target Acquire Delay", 250, 0, 1500));
        slider->ValueChanged = [](MenuItem* sender, void*) {
            Ptr()->fowDelay_ = static_cast<MenuSlider*>(sender)->Value;
        };
        fowDelay_ = menu_->Get<MenuSlider>("fowDelay")->Value;

        for (const auto& hero : GameObjects::EnemyHeroes()) {
            entries_.emplace_back(hero);
        }

        Resume();
    }

    ~TargetSelectorHumanizer() {
        Suspend();
        if (Ptr() == this) Ptr() = nullptr;
    }

    void Suspend() {
        if (suspended_) return;
        Events::hook.OnUpdate -= &OnGameUpdateHandler;
#if NIGHTSHARP_TARGETSELECTOR_HUMANIZER_LIFECYCLE
        Events::hook.OnDeleteObject -= &OnDeleteObjectHandler;
        Events::hook.OnCreateObject -= &OnCreateObjectHandler;
#endif
        suspended_ = true;
    }

    void Resume() {
        if (!suspended_) return;
        Ptr() = this;
#if NIGHTSHARP_TARGETSELECTOR_HUMANIZER_LIFECYCLE
        Events::hook.OnCreateObject += &OnCreateObjectHandler;
        Events::hook.OnDeleteObject += &OnDeleteObjectHandler;
#endif
        Events::hook.OnUpdate += &OnGameUpdateHandler;
        suspended_ = false;
    }

    int FowDelay() const { return fowDelay_; }
    void FowDelay(int value) {
        fowDelay_ = std::min(1500, std::max(0, value));
        menu_->Get<MenuSlider>("fowDelay")->Value = fowDelay_;
    }

    std::vector<AIHeroClient> FilterTargets(std::vector<AIHeroClient> heroes) {
        if (fowDelay_ <= 0) {
            return heroes;
        }

        return heroes;
    }

private:
    static TargetSelectorHumanizer*& Ptr() {
        static TargetSelectorHumanizer* ptr = nullptr;
        return ptr;
    }

    static void OnCreateObjectHandler(const Events::ObjectEventArgs& args) {
        auto* self = Ptr();
        if (!self) {
            return;
        }

        AIHeroClient hero = MakeHero(args);
        if (!hero.IsValid()) {
            return;
        }

        auto& entries = self->entries_;
        auto it = std::find_if(entries.begin(), entries.end(),
            [&hero](const HeroVisibleEntry& e) { return hero.Compare(e.Hero); });
        if (it == entries.end()) {
            entries.emplace_back(hero);
        }
    }

    static void OnDeleteObjectHandler(const Events::ObjectEventArgs& args) {
        auto* self = Ptr();
        if (!self) {
            return;
        }

        AIHeroClient hero = MakeHero(args);
        if (!hero.IsValid()) {
            return;
        }

        auto& entries = self->entries_;
        for (int i = static_cast<int>(entries.size()) - 1; i >= 0; i--) {
            if (entries[i].Hero.Compare(hero)) {
                entries.erase(entries.begin() + i);
            }
        }
    }

    static AIHeroClient MakeHero(const Events::ObjectEventArgs& args) {
        if (!args.Sender.IsValid()) {
            return {};
        }

        auto type = args.Sender.Type;
        if (type == ::Core::Objects::ObjectType::Unknown) {
            type = ObjectManager::detail::InferExtendedType(args.Sender.Ptr);
        }
        if (type != ::Core::Objects::ObjectType::AIHeroClient) {
            return {};
        }

        AIHeroClient hero(args.Sender.Ptr);
        return hero.IsValid() ? hero : AIHeroClient();
    }

    static void OnGameUpdateHandler(const Events::GameUpdateEventArgs&) {
        auto* self = Ptr();
        if (!self) {
            return;
        }
        for (auto& entry : self->entries_) {
            const bool visible = entry.Hero.IsVisible();
            if (entry.Visible != visible) {
                entry.Visible = visible;
                entry.LastVisibleChangeTick = Variables::TickCount();
            }
        }
    }

    Menu* menu_ = nullptr;
    std::vector<HeroVisibleEntry> entries_;
    int fowDelay_ = 250;
    bool suspended_ = true;
};

} // namespace SDK
