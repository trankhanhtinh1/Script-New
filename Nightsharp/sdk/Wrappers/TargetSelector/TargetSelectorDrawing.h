#pragma once

#include "TargetSelectorMode.h"
#include "TargetSelectorSelected.h"
#include "Modes/Weight.h"
#include "../../Extensions/Unit.h"
#include "../../UI/Drawing.h"
#include "../../UI/UI.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

namespace SDK {

class TargetSelectorDrawing {
public:
    TargetSelectorDrawing(Menu* menu, TargetSelectorSelected* selected, TargetSelectorMode* mode, std::function<AIHeroClient()> getTarget = nullptr) {
        Ptr() = this;
        selected_ = selected;
        mode_ = mode;
        getTarget_ = getTarget;

        auto* drawingMenu = new Menu("drawing", "Drawing");

        auto* targetMenu = new Menu("target", "Target");
        targetMenu->Add(new MenuColor("color", "Color", 0xFF00FF00u));
        targetMenu->Add(new MenuSlider("radius", "Radius", 35, 0, 200));
        targetMenu->Add(new MenuBool("enabled", "Enabled", true));
        drawingMenu->Add(targetMenu);

        auto* selectedMenu = new Menu("selected", "Selected");
        selectedMenu->Add(new MenuColor("color", "Color", 0xFFFF0000u));
        selectedMenu->Add(new MenuSlider("radius", "Radius", 35, 0, 200));
        selectedMenu->Add(new MenuBool("enabled", "Enabled", true));
        drawingMenu->Add(selectedMenu);

        auto* weightsMenu = new Menu("weights", "Weights");
        weightsMenu->Add(new MenuBool("enabled", "Enabled", false));
        drawingMenu->Add(weightsMenu);

        menu->Add(drawingMenu);
        drawingMenu_ = drawingMenu;

        Drawing::OnDraw += &OnDrawHandler;
    }

private:
    static TargetSelectorDrawing*& Ptr() {
        static TargetSelectorDrawing* ptr = nullptr;
        return ptr;
    }

    static void OnDrawHandler() {
        auto* self = Ptr();
        if (!self || !self->drawingMenu_) return;

        // Draw Current Target indicator
        if (self->getTarget_) {
            auto* targetMenu = self->drawingMenu_->GetSubMenu("target");
            if (targetMenu) {
                auto* enabledBool = targetMenu->Get<MenuBool>("enabled");
                if (enabledBool && enabledBool->Value) {
                    const AIHeroClient target = self->getTarget_();
                    if (target.IsValid() && !target.IsDead() && target.IsVisible() && Extensions::IsValidTarget(target)) {
                        auto* radiusSlider = targetMenu->Get<MenuSlider>("radius");
                        const float worldRadius = target.BoundingRadius() + static_cast<float>(radiusSlider ? radiusSlider->Value : 35);
                        unsigned int color = 0xFF00FF00;
                        auto* colorItem = targetMenu->Get<MenuColor>("color");
                        if (colorItem) {
                            color = colorItem->Value;
                        }
                        Drawing::DrawCircle(target.Position(), worldRadius, color, 2.0f);
                    }
                }
            }
        }

        // Draw Selected target indicator
        if (self->selected_) {
            auto* selMenu = self->drawingMenu_->GetSubMenu("selected");
            if (selMenu) {
                auto* enabledBool = selMenu->Get<MenuBool>("enabled");
                if (enabledBool && enabledBool->Value && self->selected_->Focus()) {
                    const AIHeroClient target = self->selected_->Target();
                    if (target.IsValid() && !target.IsDead() && target.IsVisible() && Extensions::IsValidTarget(target)) {
                        auto* radiusSlider = selMenu->Get<MenuSlider>("radius");
                        const float worldRadius = target.BoundingRadius() + static_cast<float>(radiusSlider ? radiusSlider->Value : 35);
                        unsigned int color = 0xFFFF0000;
                        auto* colorItem = selMenu->Get<MenuColor>("color");
                        if (colorItem) {
                            color = colorItem->Value;
                        }
                        Drawing::DrawCircle(target.Position(), worldRadius, color, 2.0f);
                    }
                }
            }
        }

        // Draw Weight mode score indicators
        if (self->mode_ && self->mode_->Current() && std::strcmp(self->mode_->Current()->Name(), "weight") == 0) {
            auto* weightsMenu = self->drawingMenu_->GetSubMenu("weights");
            if (weightsMenu) {
                auto* enabledBool = weightsMenu->Get<MenuBool>("enabled");
                if (enabledBool && enabledBool->Value) {
                    for (const auto& hero : GameObjects::EnemyHeroes()) {
                        if (hero.IsValid() && !hero.IsDead() && hero.IsVisible()) {
                            Vec2 screenPos;
                            if (Drawing::WorldToScreen(hero.Position(), screenPos)) {
                                float score = Modes::Weight::GetLastScore(hero.NetworkId());
                                char buf[64];
                                std::snprintf(buf, sizeof(buf), "Weight: %.1f", score);
                                Drawing::DrawText(screenPos.x - 20.0f, screenPos.y + 20.0f, 0xFFFFFFFF, buf);
                            }
                        }
                    }
                }
            }
        }
    }

    TargetSelectorSelected* selected_ = nullptr;
    TargetSelectorMode* mode_ = nullptr;
    std::function<AIHeroClient()> getTarget_ = nullptr;
    Menu* drawingMenu_ = nullptr;
};

} // namespace SDK
