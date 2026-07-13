#pragma once

#include "../Core/MenuModel.h"
#include "imgui.h"

#include <array>
#include <cstddef>
#include <vector>

namespace NightSharp::Menu {

enum class NavigationDirection {
    Forward,
    Backward,
    Replace,
};

struct PanelTransitionSlot {
    MenuPath currentPath;
    MenuPath outgoingPath;
    NavigationDirection direction = NavigationDirection::Forward;
    float progress = 1.0f;
    bool hasCurrent = false;
    bool hasOutgoing = false;
};

struct ContentTransitionSlot {
    MenuPath currentPath;
    MenuPath outgoingPath;
    NavigationDirection direction = NavigationDirection::Forward;
    float progress = 1.0f;
    bool hasCurrent = false;
    bool hasOutgoing = false;
};

class NavigationTransition {
public:
    static constexpr std::size_t MaxPanelCount = 8;

    void Reset();
    bool SetTarget(
        const MenuPath& displayPath,
        const std::vector<MenuPath>& panelPaths,
        bool hasContent);
    void Update(
        float deltaTime,
        float panelDuration,
        float contentDuration);

    const std::array<PanelTransitionSlot, MaxPanelCount>& Panels() const;
    const ContentTransitionSlot& Content() const;
    const MenuPath& TargetPath() const;
    std::size_t TargetPanelCount() const;
    NavigationDirection Direction() const;

    void BeginPointerFrame(const ImVec2& mousePosition, float deltaTime);
    bool ConsiderHover(
        const MenuPath& candidate,
        const MenuPath& current,
        bool movingTowardChild,
        float delaySeconds);
    bool IsMovingToward(bool childOnRight) const;
    bool PointerMoved(float threshold) const;

    static float Ease(float progress);
    static bool IsInsideBridge(
        const ImVec2& point,
        const ImVec4& first,
        const ImVec4& second,
        float padding);

private:
    static bool IsPrefix(const MenuPath& prefix, const MenuPath& path);
    static NavigationDirection ResolveDirection(
        const MenuPath& previous,
        const MenuPath& next);
    static void Advance(float& progress, float deltaTime, float duration);

    std::array<PanelTransitionSlot, MaxPanelCount> panels_{};
    ContentTransitionSlot content_{};
    MenuPath targetPath_;
    MenuPath pendingHoverPath_;
    std::size_t targetPanelCount_ = 0;
    NavigationDirection direction_ = NavigationDirection::Forward;
    ImVec2 previousMouse_{};
    ImVec2 currentMouse_{};
    float pointerDeltaTime_ = 0.0f;
    float pendingHoverTime_ = 0.0f;
    bool pointerInitialized_ = false;
};

}
