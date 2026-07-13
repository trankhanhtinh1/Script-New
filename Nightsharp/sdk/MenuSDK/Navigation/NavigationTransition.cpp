#include "NavigationTransition.h"

#include <algorithm>
#include <cmath>

namespace NightSharp::Menu {

void NavigationTransition::Reset() {
    panels_ = {};
    content_ = {};
    targetPath_.clear();
    pendingHoverPath_.clear();
    targetPanelCount_ = 0;
    direction_ = NavigationDirection::Forward;
    previousMouse_ = {};
    currentMouse_ = {};
    pointerDeltaTime_ = 0.0f;
    pendingHoverTime_ = 0.0f;
    pointerInitialized_ = false;
}

bool NavigationTransition::SetTarget(
    const MenuPath& displayPath,
    const std::vector<MenuPath>& panelPaths,
    bool hasContent) {
    const bool pathChanged = targetPath_ != displayPath;
    if (pathChanged) {
        direction_ = ResolveDirection(targetPath_, displayPath);
        targetPath_ = displayPath;
    }

    const std::size_t desiredCount = std::min(
        panelPaths.size(),
        panels_.size());
    bool changed = pathChanged || desiredCount != targetPanelCount_;
    for (std::size_t depth = 0; depth < panels_.size(); ++depth) {
        PanelTransitionSlot& slot = panels_[depth];
        const bool desired = depth < desiredCount;
        if (desired) {
            const MenuPath& nextPath = panelPaths[depth];
            if (!slot.hasCurrent || slot.currentPath != nextPath) {
                if (slot.hasCurrent) {
                    slot.outgoingPath = slot.currentPath;
                    slot.hasOutgoing = true;
                }
                slot.currentPath = nextPath;
                slot.hasCurrent = true;
                slot.direction = direction_;
                slot.progress = 0.0f;
                changed = true;
            }
        } else if (slot.hasCurrent) {
            slot.outgoingPath = slot.currentPath;
            slot.hasOutgoing = true;
            slot.currentPath.clear();
            slot.hasCurrent = false;
            slot.direction = direction_;
            slot.progress = 0.0f;
            changed = true;
        }
    }
    targetPanelCount_ = desiredCount;

    const bool contentMatches =
        content_.hasCurrent == hasContent &&
        (!hasContent || content_.currentPath == displayPath);
    if (!contentMatches) {
        if (content_.hasCurrent) {
            content_.outgoingPath = content_.currentPath;
            content_.hasOutgoing = true;
        }
        content_.currentPath = hasContent ? displayPath : MenuPath{};
        content_.hasCurrent = hasContent;
        content_.direction = direction_;
        content_.progress = 0.0f;
        changed = true;
    }
    return changed;
}

void NavigationTransition::Update(
    float deltaTime,
    float panelDuration,
    float contentDuration) {
    for (PanelTransitionSlot& slot : panels_) {
        if (slot.progress >= 1.0f) {
            continue;
        }
        Advance(slot.progress, deltaTime, panelDuration);
        if (slot.progress >= 1.0f) {
            slot.outgoingPath.clear();
            slot.hasOutgoing = false;
        }
    }

    if (content_.progress < 1.0f) {
        Advance(content_.progress, deltaTime, contentDuration);
        if (content_.progress >= 1.0f) {
            content_.outgoingPath.clear();
            content_.hasOutgoing = false;
        }
    }
}

const std::array<PanelTransitionSlot, NavigationTransition::MaxPanelCount>&
NavigationTransition::Panels() const {
    return panels_;
}

const ContentTransitionSlot& NavigationTransition::Content() const {
    return content_;
}

const MenuPath& NavigationTransition::TargetPath() const {
    return targetPath_;
}

std::size_t NavigationTransition::TargetPanelCount() const {
    return targetPanelCount_;
}

NavigationDirection NavigationTransition::Direction() const {
    return direction_;
}

void NavigationTransition::BeginPointerFrame(
    const ImVec2& mousePosition,
    float deltaTime) {
    if (!pointerInitialized_) {
        previousMouse_ = mousePosition;
        currentMouse_ = mousePosition;
        pointerInitialized_ = true;
    } else {
        previousMouse_ = currentMouse_;
        currentMouse_ = mousePosition;
    }
    pointerDeltaTime_ = std::clamp(deltaTime, 0.0f, 0.05f);
}

bool NavigationTransition::ConsiderHover(
    const MenuPath& candidate,
    const MenuPath& current,
    bool movingTowardChild,
    float delaySeconds) {
    if (candidate == current) {
        pendingHoverPath_.clear();
        pendingHoverTime_ = 0.0f;
        return true;
    }
    if (!movingTowardChild || current.empty()) {
        pendingHoverPath_.clear();
        pendingHoverTime_ = 0.0f;
        return true;
    }
    if (pendingHoverPath_ != candidate) {
        pendingHoverPath_ = candidate;
        pendingHoverTime_ = 0.0f;
        return false;
    }

    pendingHoverTime_ += pointerDeltaTime_;
    if (pendingHoverTime_ >= std::max(delaySeconds, 0.0f)) {
        pendingHoverPath_.clear();
        pendingHoverTime_ = 0.0f;
        return true;
    }
    return false;
}

bool NavigationTransition::IsMovingToward(bool childOnRight) const {
    const float deltaX = currentMouse_.x - previousMouse_.x;
    return childOnRight ? deltaX > 0.10f : deltaX < -0.10f;
}

bool NavigationTransition::PointerMoved(float threshold) const {
    const float deltaX = currentMouse_.x - previousMouse_.x;
    const float deltaY = currentMouse_.y - previousMouse_.y;
    return deltaX * deltaX + deltaY * deltaY > threshold * threshold;
}

float NavigationTransition::Ease(float progress) {
    progress = std::clamp(progress, 0.0f, 1.0f);
    return progress * progress * (3.0f - 2.0f * progress);
}

bool NavigationTransition::IsInsideBridge(
    const ImVec2& point,
    const ImVec4& first,
    const ImVec4& second,
    float padding) {
    const float left = std::min(first.x, second.x) - padding;
    const float right = std::max(first.z, second.z) + padding;
    const float top = std::max(first.y, second.y) - padding;
    const float bottom = std::min(first.w, second.w) + padding;
    return top <= bottom &&
        point.x >= left && point.x <= right &&
        point.y >= top && point.y <= bottom;
}

bool NavigationTransition::IsPrefix(
    const MenuPath& prefix,
    const MenuPath& path) {
    return prefix.size() <= path.size() &&
        std::equal(prefix.begin(), prefix.end(), path.begin());
}

NavigationDirection NavigationTransition::ResolveDirection(
    const MenuPath& previous,
    const MenuPath& next) {
    if (previous.empty() && !next.empty()) {
        return NavigationDirection::Forward;
    }
    if (IsPrefix(previous, next) && next.size() > previous.size()) {
        return NavigationDirection::Forward;
    }
    if (IsPrefix(next, previous) && next.size() < previous.size()) {
        return NavigationDirection::Backward;
    }
    return NavigationDirection::Replace;
}

void NavigationTransition::Advance(
    float& progress,
    float deltaTime,
    float duration) {
    duration = std::max(duration, 0.001f);
    progress = std::min(
        1.0f,
        progress + std::clamp(deltaTime, 0.0f, 0.05f) / duration);
}

}
