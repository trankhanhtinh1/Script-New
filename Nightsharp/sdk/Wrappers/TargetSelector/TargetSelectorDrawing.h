#pragma once

#include "../../UI/Drawing.h"
#include "../../UI/UI.h"
#include "../../Core/Objects.h"

namespace SDK {

class TargetSelectorDrawing {
public:
    static void Initialize(Menu* root) {
        if (s_initialized || !root) {
            return;
        }
        s_initialized = true;

        auto drawing = UI::Wrap(root).AddMenu("drawing", "Drawing");
        drawing.AddBool("drawSelected", "Draw Selected", true);
        drawing.AddBool("drawForced", "Draw Forced", true);
        drawing.AddBool("drawCurrent", "Draw Current", true);
        drawing.AddBool("drawRange", "Draw Selection Range", false);
    }

    static void Render(Menu* root,
                       const AIHeroClient& selected,
                       const AIHeroClient& forced,
                       const AIHeroClient& current) {
        if (!root) {
            return;
        }

        const auto drawing = UI::Wrap(root).SubMenu("drawing");
        if (!drawing.IsValid()) {
            return;
        }

        auto player = ObjectManager::Player();
        if (drawing.Bool("drawRange", false) && player.IsValid()) {
            Drawing::DrawCircle(player.Position(),
                                player.AttackRange() + player.BoundingRadius() + 100.0f,
                                IM_COL32(100, 140, 255, 160));
        }

        if (drawing.Bool("drawSelected", true) && selected.IsValid()) {
            Drawing::DrawCircle(selected.Position(), selected.BoundingRadius() + 65.0f, IM_COL32(255, 210, 64, 255), 2.0f);
            Drawing::DrawText(selected.Position(), "Selected", IM_COL32(255, 230, 120, 255), true);
        }

        if (drawing.Bool("drawForced", true) && forced.IsValid()) {
            Drawing::DrawCircle(forced.Position(), forced.BoundingRadius() + 90.0f, IM_COL32(255, 96, 96, 255), 2.0f);
            Drawing::DrawText(forced.Position(), "Forced", IM_COL32(255, 128, 128, 255), true);
        }

        if (drawing.Bool("drawCurrent", true) && current.IsValid()) {
            Drawing::DrawCircle(current.Position(), current.BoundingRadius() + 42.0f, IM_COL32(120, 255, 180, 255), 2.0f);
            Drawing::DrawText(current.Position(), "Current", IM_COL32(140, 255, 200, 255), true);
        }
    }

private:
    static inline bool s_initialized = false;
};

} // namespace SDK
