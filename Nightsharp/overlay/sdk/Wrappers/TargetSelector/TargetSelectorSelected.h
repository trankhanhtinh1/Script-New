#pragma once

#include "../../Core/Game.h"
#include "../../Enumerations/WindowsMessages.h"
#include "../../Extensions/Unit.h"
#include "../../GameObjects/GameObjects.h"
#include "../../UI/UI.h"

#include <cfloat>

namespace SDK {

class TargetSelectorSelected {
public:
    explicit TargetSelectorSelected(Menu* menu) {
        Ptr() = this;
        menu_ = menu;

        auto* focusBool = menu_->Add(new MenuBool("focus", "Focus Selected Target", true));
        focusBool->ValueChanged = [](MenuItem* sender, void*) {
            Ptr()->focus_ = static_cast<MenuBool*>(sender)->Value;
        };
        auto* forceBool = menu_->Add(new MenuBool("forceFocus", "Only Attack Selected Target", false));
        forceBool->ValueChanged = [](MenuItem* sender, void*) {
            Ptr()->force_ = static_cast<MenuBool*>(sender)->Value;
        };

        focus_ = menu_->Get<MenuBool>("focus")->Value;
        force_ = menu_->Get<MenuBool>("forceFocus")->Value;

        Game::OnWndProc += &OnWndProcHandler;
    }

    bool Focus() const { return focus_; }
    void Focus(bool value) {
        focus_ = value;
        menu_->Get<MenuBool>("focus")->Value = focus_;
    }

    bool Force() const { return force_; }
    void Force(bool value) {
        force_ = value;
        menu_->Get<MenuBool>("forceFocus")->Value = force_;
    }

    float ClickBuffer() const { return clickBuffer_; }
    void ClickBuffer(float value) { clickBuffer_ = value; }

    AIHeroClient Target() const { return target_; }
    void Target(const AIHeroClient& hero) { target_ = hero; }

private:
    static TargetSelectorSelected*& Ptr() {
        static TargetSelectorSelected* ptr = nullptr;
        return ptr;
    }

    static void OnWndProcHandler(Game::WndEventArgs& args) {
        if (args.Msg != static_cast<std::uint32_t>(WindowsMessages::LBUTTONDOWN)) return;

        auto* self = Ptr();
        const auto cursorPos = Game::CursorPosRaw();

        AIHeroClient closest;
        float minDist = FLT_MAX;

        for (const auto& hero : GameObjects::EnemyHeroes()) {
            if (!Extensions::IsValidTarget(hero)) continue;

            const float dist = hero.Distance(cursorPos);
            if (dist < hero.BoundingRadius() + self->clickBuffer_) {
                if (dist < minDist) {
                    minDist = dist;
                    closest = hero;
                }
            }
        }

        self->target_ = closest;
    }

    Menu* menu_ = nullptr;
    AIHeroClient target_ = {};
    bool focus_ = true;
    bool force_ = false;
    float clickBuffer_ = 100.0f;
};

} // namespace SDK
