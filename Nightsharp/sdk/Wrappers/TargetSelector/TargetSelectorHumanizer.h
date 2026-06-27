#pragma once

#include "HeroVisibleEntry.h"
#include "../../Core/Variables.h"
#include "../../Events/Events.h"
#include "../../UI/UI.h"

#include <algorithm>
#include <cmath>
#include <vector>

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

        Events::hook.OnCreateObject += &OnCreateObjectHandler;
        Events::hook.OnDeleteObject += &OnDeleteObjectHandler;
        Events::hook.OnUpdate += &OnGameUpdateHandler;
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

        auto snapshot = heroes;
        for (const auto& hero : snapshot) {
            auto it = std::find_if(entries_.begin(), entries_.end(),
                [&hero](const HeroVisibleEntry& e) { return hero.Compare(e.Hero); });

            if (it == entries_.end() ||
                std::abs(Variables::TickCount() - it->LastVisibleChangeTick) < fowDelay_) {
                heroes.erase(
                    std::remove_if(heroes.begin(), heroes.end(),
                        [&hero](const AIHeroClient& h) { return h.Compare(hero); }),
                    heroes.end());
            }
        }

        return heroes;
    }

private:
    static TargetSelectorHumanizer*& Ptr() {
        static TargetSelectorHumanizer* ptr = nullptr;
        return ptr;
    }

    static void OnCreateObjectHandler(const Events::ObjectEventArgs& args) {
        if (!args.Sender.IsValid() ||
            args.Sender.Type != ::Core::Objects::ObjectType::AIHeroClient) {
            return;
        }

        AIHeroClient hero(args.Sender.Ptr);
        if (!hero.IsValid()) return;

        auto& entries = Ptr()->entries_;
        auto it = std::find_if(entries.begin(), entries.end(),
            [&hero](const HeroVisibleEntry& e) { return hero.Compare(e.Hero); });
        if (it == entries.end()) {
            entries.emplace_back(hero);
        }
    }

    static void OnDeleteObjectHandler(const Events::ObjectEventArgs& args) {
        if (!args.Sender.IsValid() ||
            args.Sender.Type != ::Core::Objects::ObjectType::AIHeroClient) {
            return;
        }

        AIHeroClient hero(args.Sender.Ptr);
        if (!hero.IsValid()) return;

        auto& entries = Ptr()->entries_;
        for (int i = static_cast<int>(entries.size()) - 1; i >= 0; i--) {
            if (entries[i].Hero.Compare(hero)) {
                entries.erase(entries.begin() + i);
            }
        }
    }

    static void OnGameUpdateHandler(const Events::GameUpdateEventArgs&) {
        for (auto& entry : Ptr()->entries_) {
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
};

} // namespace SDK
