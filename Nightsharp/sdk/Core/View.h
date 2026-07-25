#pragma once

#include "../../Core/CoreView.h"
#include "../../Core/Vector.h"

namespace SDK::View {

inline Vector2 WorldToScreen(const Vector3& world) {
    return ::CoreView::WorldToScreen(world);
}

inline bool WorldToScreen(const Vector3& world, Vector2& screen) {
    return ::CoreView::WorldToScreen(world, screen);
}

inline Vector3 ScreenToWorld(const Vector2& screen) {
    return ::CoreView::ScreenToWorld(screen);
}

inline bool ScreenToWorld(const Vector2& screen, Vector3& world) {
    return ::CoreView::ScreenToWorld(screen, world);
}

inline bool IsOnScreen(const Vector3& world) {
    return ::CoreView::IsOnScreen(world);
}

inline bool IsOnScreen(const Vector2& screen) {
    return ::CoreView::IsOnScreen(screen);
}

inline bool OnScreen(const Vector3& point) {
    return ::CoreView::OnScreen(point);
}

inline bool OnScreen(const Vector2& point) {
    return ::CoreView::OnScreen(point);
}

} // namespace SDK::View

namespace SDK::Core {
namespace View = ::SDK::View;
}
