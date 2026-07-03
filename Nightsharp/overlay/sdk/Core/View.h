#pragma once

#include "../../Core/CoreView.h"
#include "Objects.h"

namespace SDK::View {

using Matrix4x4 = ::CoreView::Matrix4x4;

inline int Width() {
    return ::CoreView::Width();
}

inline int Height() {
    return ::CoreView::Height();
}

inline bool ReadView(Matrix4x4& out) {
    return ::CoreView::ReadView(out);
}

inline bool ReadProjection(Matrix4x4& out) {
    return ::CoreView::ReadProjection(out);
}

inline bool ReadViewProjection(Matrix4x4& out) {
    return ::CoreView::ReadViewProjection(out);
}

inline bool WorldToScreen(const Vector3& world, Vector2& screen) {
    return ::CoreView::WorldToScreen(world, screen);
}

inline Vector2 WorldToScreen(const Vector3& world) {
    return ::CoreView::WorldToScreen(world);
}

inline bool ScreenToWorld(const Vector2& screen, Vector3& world) {
    return ::CoreView::ScreenToWorld(screen, world);
}

inline Vector3 ScreenToWorld(const Vector2& screen) {
    return ::CoreView::ScreenToWorld(screen);
}

inline bool WorldToMinimap(const Vector3& world, Vector2& minimap) {
    return ::CoreView::WorldToMinimap(world, minimap);
}

inline Vector2 WorldToMinimap(const Vector3& world) {
    return ::CoreView::WorldToMinimap(world);
}

inline bool MinimapToWorld(const Vector2& minimap, Vector3& world) {
    return ::CoreView::MinimapToWorld(minimap, world);
}

inline Vector3 MinimapToWorld(const Vector2& minimap) {
    return ::CoreView::MinimapToWorld(minimap);
}

inline bool OnScreen(const Vector2& point) {
    return ::CoreView::OnScreen(point);
}

} // namespace SDK::View

namespace SDK::Core {
namespace View = ::SDK::View;
}
