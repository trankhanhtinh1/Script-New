#pragma once

#include "ITargetSelectorMode.h"
#include "Modes/Closest.h"
#include "Modes/LeastHealth.h"
#include "Modes/NearMouse.h"
#include "Modes/LessAttacksToKill.h"
#include "Modes/LessCastsToKill.h"
#include "Modes/MostAbilityPower.h"
#include "Modes/MostAttackDamage.h"
#include "../../UI/UI.h"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace SDK {

class TargetSelectorMode {
public:
    using OnChangeDelegate = std::function<void(ITargetSelectorMode*)>;

    explicit TargetSelectorMode(Menu* menu) {
        Ptr() = this;
        menu_ = menu;

        RegisterBuiltins();

        std::sort(entries_.begin(), entries_.end(),
            [](ITargetSelectorMode* a, ITargetSelectorMode* b) {
                return std::strcmp(a->DisplayName(), b->DisplayName()) < 0;
            });

        std::vector<std::string> modeNames;
        for (const auto* mode : entries_) {
            modeNames.push_back(mode->DisplayName());
        }

        auto* modeList = menu_->Add(new MenuList("mode", "Mode", modeNames, 0));
        modeList->ValueChanged = [](MenuItem* sender, void*) {
            auto* list = static_cast<MenuList*>(sender);
            auto* self = Ptr();
            if (list->Index >= 0 && list->Index < static_cast<int>(self->entries_.size())) {
                ITargetSelectorMode* newMode = self->entries_[list->Index];
                if (self->current_ != newMode) {
                    self->current_ = newMode;
                    if (self->onChange_) {
                        self->onChange_(newMode);
                    }
                }
            }
        };

        const int initialIndex = menu_->Get<MenuList>("mode")->Index;
        current_ = (initialIndex >= 0 && initialIndex < static_cast<int>(entries_.size()))
            ? entries_[initialIndex]
            : (!entries_.empty() ? entries_[0] : nullptr);
    }

    ITargetSelectorMode* Current() const { return current_; }

    void Current(ITargetSelectorMode* mode) {
        auto it = std::find(entries_.begin(), entries_.end(), mode);
        if (it != entries_.end()) {
            if (current_ != mode) {
                current_ = mode;
                if (onChange_) {
                    onChange_(mode);
                }
            }
            const int idx = static_cast<int>(it - entries_.begin());
            menu_->Get<MenuList>("mode")->Index = idx;
        }
    }

    const std::vector<ITargetSelectorMode*>& Entries() const { return entries_; }

    void Register(ITargetSelectorMode* mode) {
        if (!mode || !mode->DisplayName() || !mode->DisplayName()[0]) return;
        for (const auto* m : entries_) {
            if (std::strcmp(m->Name(), mode->Name()) == 0) return;
        }
        mode->AddToMenu(menu_);
        entries_.push_back(mode);
        UpdateMenu();
    }

    void Deregister(ITargetSelectorMode* mode) {
        auto it = std::find(entries_.begin(), entries_.end(), mode);
        if (it == entries_.end()) return;
        entries_.erase(it);
        if (std::find(entries_.begin(), entries_.end(), current_) == entries_.end()) {
            current_ = entries_.empty() ? nullptr : entries_[0];
        }
        UpdateMenu();
    }

    void Overwrite(ITargetSelectorMode* oldMode, ITargetSelectorMode* newMode) {
        auto it = std::find(entries_.begin(), entries_.end(), oldMode);
        if (it != entries_.end()) {
            *it = newMode;
            UpdateMenu();
        }
    }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes) {
        if (!current_) return heroes;
        return current_->OrderChampions(heroes);
    }

    OnChangeDelegate OnChange = nullptr;

private:
    void RegisterBuiltins() {
        entries_.push_back(new Modes::Closest());
        entries_.push_back(new Modes::LeastHealth());
        entries_.push_back(new Modes::NearMouse());
        entries_.push_back(new Modes::LessAttacksToKill());
        entries_.push_back(new Modes::LessCastsToKill());
        entries_.push_back(new Modes::MostAbilityPower());
        entries_.push_back(new Modes::MostAttackDamage());
    }

    void UpdateMenu() {
        auto* list = menu_->Get<MenuList>("mode");
        list->Options.clear();
        for (const auto* mode : entries_) {
            list->Options.push_back(::SDK::UI::TinyString(mode->DisplayName()));
        }
        if (list->Index >= list->Options.size()) {
            list->Index = list->Options.size() > 0 ? list->Options.size() - 1 : 0;
        }
        if (current_ && std::find(entries_.begin(), entries_.end(), current_) == entries_.end()) {
            current_ = entries_.empty() ? nullptr : entries_[0];
        }
    }

    static TargetSelectorMode*& Ptr() {
        static TargetSelectorMode* ptr = nullptr;
        return ptr;
    }

    Menu* menu_ = nullptr;
    std::vector<ITargetSelectorMode*> entries_;
    ITargetSelectorMode* current_ = nullptr;
    OnChangeDelegate onChange_ = nullptr;
};

} // namespace SDK
