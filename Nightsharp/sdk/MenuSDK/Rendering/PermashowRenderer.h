#pragma once

#include "../Animation/Animator.h"
#include "../Core/Permashow.h"
#include "../Styling/Theme.h"

namespace NightSharp::Menu {

class PermashowRenderer {
public:
    explicit PermashowRenderer(PermashowRegistry& registry);

    void Render(
        const Theme& theme,
        ImFont* font,
        const ImVec2& displaySize,
        float deltaTime);
    void Reset();

private:
    ImU32 Color(
        const ImVec4& value,
        float alpha) const;
    ImVec2 TargetPosition(
        const PermashowLayout& layout,
        const ImVec2& displaySize,
        const ImVec2& panelSize) const;

    PermashowRegistry& registry_;
    Animator animator_;
    ImVec2 position_{};
    bool positionInitialized_ = false;
};

}
