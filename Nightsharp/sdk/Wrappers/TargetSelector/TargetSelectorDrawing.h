#pragma once

#include "TargetSelectorMode.h"
#include "TargetSelectorSelected.h"
#include "../../Extensions/Unit.h"
#include "../../UI/Drawing.h"
#include "../../UI/UI.h"

#include <algorithm>
#include <cmath>

namespace SDK {

class TargetSelectorDrawing {
public:
    TargetSelectorDrawing(Menu* menu, TargetSelectorSelected* selected, TargetSelectorMode* mode) {
        Ptr() = this;
        selected_ = selected;
        mode_ = mode;

        auto* drawingMenu = new Menu("drawing", "Drawing");

        auto* selectedMenu = new Menu("selected", "Selected");
        selectedMenu->Add(new MenuColor("color", "Color", 0xFFFF0000u));
        selectedMenu->Add(new MenuSlider("radius", "Radius", 35, 0, 200));
        selectedMenu->Add(new MenuBool("enabled", "Enabled", true));
        drawingMenu->Add(selectedMenu);

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
        if (!self || !self->selected_ || !self->drawingMenu_) return;

        auto* selMenu = self->drawingMenu_->GetSubMenu("selected");
        if (!selMenu) return;

        auto* enabledBool = selMenu->Get<MenuBool>("enabled");
        if (!enabledBool || !enabledBool->Value) return;

        if (!self->selected_->Focus()) return;
        const AIHeroClient target = self->selected_->Target();
        if (!target.IsValid() || !Extensions::IsValidTarget(target)) return;

        Vec2 center;
        if (!Drawing::WorldToScreen(target.Position(), center)) return;

        auto* radiusSlider = selMenu->Get<MenuSlider>("radius");
        const float worldRadius = target.BoundingRadius() + static_cast<float>(radiusSlider ? radiusSlider->Value : 35);

        Vec2 edge;
        const Vec3 edgeWorld(target.Position().x + worldRadius, target.Position().y, target.Position().z);
        if (!Drawing::WorldToScreen(edgeWorld, edge)) return;

        const float screenRadius = std::max(1.0f, center.Distance(edge));

        unsigned int color = 0xFFFF0000;
        auto* colorItem = selMenu->Get<MenuColor>("color");
        if (colorItem) {
            const auto c = colorItem->Value;
            const unsigned int a = (c >> 24) & 0xFF;
            const unsigned int r = c & 0xFF;
            const unsigned int g = (c >> 8) & 0xFF;
            const unsigned int b = (c >> 16) & 0xFF;
            color = (a << 24) | (r << 16) | (g << 8) | b;
        }

        Drawing::DrawCircle(center, screenRadius, 2.0f, color);
    }

    TargetSelectorSelected* selected_ = nullptr;
    TargetSelectorMode* mode_ = nullptr;
    Menu* drawingMenu_ = nullptr;
};

} // namespace SDK
