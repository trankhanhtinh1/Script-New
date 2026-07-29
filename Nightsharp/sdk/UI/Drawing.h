#pragma once

#include "../../Core/CoreRuntime.h"
#include "../../Core/CoreView.h"
#include "../../Core/Globals.h"
#include "../../Core/Vector.h"
#include "../../imgui/imgui.h"
#include "../Core/Game.h"
#include "DrawingVisibility.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace SDK::Drawing {

namespace detail {
    template <typename Handler, int MaxHandlers = 64>
    struct HandlerList {
        Handler handlers[MaxHandlers] = {};
        int count = 0;

        bool Add(Handler handler) {
            if (!handler) {
                return false;
            }
            for (int i = 0; i < count; ++i) {
                if (handlers[i] == handler) {
                    return true;
                }
            }
            if (count >= MaxHandlers) {
                return false;
            }
            handlers[count++] = handler;
            return true;
        }

        bool Remove(Handler handler) {
            if (!handler) {
                return false;
            }
            for (int i = 0; i < count; ++i) {
                if (handlers[i] != handler) {
                    continue;
                }
                for (int j = i; j + 1 < count; ++j) {
                    handlers[j] = handlers[j + 1];
                }
                handlers[--count] = nullptr;
                return true;
            }
            return false;
        }

        void Fire() const {
            for (int i = 0; i < count; ++i) {
                if (auto handler = handlers[i]) {
                    __try {
                        handler();
                    } __except (1) {}
                }
            }
        }

        void Clear() {
            for (int i = 0; i < count; ++i) {
                handlers[i] = nullptr;
            }
            count = 0;
        }
    };

    inline bool DrawingEnabled = true;
    inline bool F7WndProcInstalled = false;

    inline ImU32 FromArgb(std::uint32_t argb) {
        const int a = static_cast<int>((argb >> 24) & 0xFF);
        const int r = static_cast<int>((argb >> 16) & 0xFF);
        const int g = static_cast<int>((argb >> 8) & 0xFF);
        const int b = static_cast<int>(argb & 0xFF);
        return IM_COL32(r, g, b, a);
    }

    inline std::uint32_t ToArgb(int r, int g, int b, int a = 255) {
        return (static_cast<std::uint32_t>(a & 0xFF) << 24) |
               (static_cast<std::uint32_t>(r & 0xFF) << 16) |
               (static_cast<std::uint32_t>(g & 0xFF) << 8) |
               static_cast<std::uint32_t>(b & 0xFF);
    }

    inline constexpr int kCircleSegments32 = 32;
    inline constexpr int kCircleSegments64 = 64;
    inline constexpr int kCircleSegments128 = 128;
    inline constexpr int kMaxCircleSegments = kCircleSegments128;
    inline constexpr int kMaxPolylinePoints = 128;
    inline constexpr int kRainbowColorCount = 128;
    inline constexpr float kScreenRejectPadding = 128.0f;

    struct UnitCirclePoint {
        float x = 0.0f;
        float z = 0.0f;
    };

    struct CircleLut {
        int segments = 0;
        UnitCirclePoint points[kMaxCircleSegments + 1] = {};
    };

    inline constexpr CircleLut MakeCircleLut(int segments, float stepCos, float stepSin) {
        CircleLut lut = {};
        lut.segments = segments;

        float unitX = 1.0f;
        float unitZ = 0.0f;
        for (int i = 0; i <= segments && i <= kMaxCircleSegments; ++i) {
            lut.points[i] = { unitX, unitZ };

            const float nextX = unitX * stepCos - unitZ * stepSin;
            const float nextZ = unitX * stepSin + unitZ * stepCos;
            unitX = nextX;
            unitZ = nextZ;
        }

        lut.points[segments] = { 1.0f, 0.0f };
        return lut;
    }

    inline constexpr CircleLut CircleLut32 =
        MakeCircleLut(kCircleSegments32, 0.9807852804f, 0.1950903220f);
    inline constexpr CircleLut CircleLut64 =
        MakeCircleLut(kCircleSegments64, 0.9951847267f, 0.0980171403f);
    inline constexpr CircleLut CircleLut128 =
        MakeCircleLut(kCircleSegments128, 0.9987954562f, 0.0490676743f);

    inline const CircleLut& SelectCircleLut(int segments) {
        if (segments <= kCircleSegments32) {
            return CircleLut32;
        }
        if (segments <= kCircleSegments64) {
            return CircleLut64;
        }
        return CircleLut128;
    }

    inline constexpr ImU32 PackRgba(int r, int g, int b, int a = 255) {
        return (static_cast<ImU32>(r & 0xFF) << IM_COL32_R_SHIFT) |
               (static_cast<ImU32>(g & 0xFF) << IM_COL32_G_SHIFT) |
               (static_cast<ImU32>(b & 0xFF) << IM_COL32_B_SHIFT) |
               (static_cast<ImU32>(a & 0xFF) << IM_COL32_A_SHIFT);
    }

    inline constexpr int ToColorByte(float value) {
        return static_cast<int>(value * 255.0f + 0.5f);
    }

    inline constexpr ImU32 MakeRainbowColor(int index) {
        const float scaledHue =
            (static_cast<float>(index) / static_cast<float>(kRainbowColorCount)) * 6.0f;
        const int sector = static_cast<int>(scaledHue);
        const float fraction = scaledHue - static_cast<float>(sector);
        const float inverse = 1.0f - fraction;

        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        switch (sector % 6) {
        case 0:
            r = 1.0f;
            g = fraction;
            break;
        case 1:
            r = inverse;
            g = 1.0f;
            break;
        case 2:
            g = 1.0f;
            b = fraction;
            break;
        case 3:
            g = inverse;
            b = 1.0f;
            break;
        case 4:
            r = fraction;
            b = 1.0f;
            break;
        default:
            r = 1.0f;
            b = inverse;
            break;
        }

        return PackRgba(ToColorByte(r), ToColorByte(g), ToColorByte(b));
    }

    struct RainbowLut {
        ImU32 colors[kRainbowColorCount] = {};
    };

    inline constexpr RainbowLut MakeRainbowLut() {
        RainbowLut lut = {};
        for (int i = 0; i < kRainbowColorCount; ++i) {
            lut.colors[i] = MakeRainbowColor(i);
        }
        return lut;
    }

    inline constexpr RainbowLut RainbowColors = MakeRainbowLut();

    inline int NormalizeRainbowIndex(int index) {
        index %= kRainbowColorCount;
        return index < 0 ? index + kRainbowColorCount : index;
    }

    inline ImU32 RainbowColor(int index) {
        return RainbowColors.colors[NormalizeRainbowIndex(index)];
    }

    inline bool HasRendererSize(const Vec2& size) {
        return size.x > 0.0f && size.y > 0.0f;
    }

    inline bool IsScreenCircleOutside(const Vec2& center,
                                      float radius,
                                      const Vec2& size,
                                      float padding = 0.0f) {
        if (!HasRendererSize(size)) {
            return false;
        }

        return center.x + radius < -padding ||
               center.x - radius > size.x + padding ||
               center.y + radius < -padding ||
               center.y - radius > size.y + padding;
    }

    inline bool IsScreenLineOutside(const Vec2& start,
                                    const Vec2& end,
                                    const Vec2& size,
                                    float padding = 0.0f) {
        if (!HasRendererSize(size)) {
            return false;
        }

        const float minX = (std::min)(start.x, end.x);
        const float maxX = (std::max)(start.x, end.x);
        const float minY = (std::min)(start.y, end.y);
        const float maxY = (std::max)(start.y, end.y);
        return maxX < -padding ||
               minX > size.x + padding ||
               maxY < -padding ||
               minY > size.y + padding;
    }

    inline bool IsScreenBoundsOutside(float minX,
                                      float maxX,
                                      float minY,
                                      float maxY,
                                      const Vec2& size,
                                      float padding = 0.0f) {
        if (!HasRendererSize(size)) {
            return false;
        }

        return maxX < -padding ||
               minX > size.x + padding ||
               maxY < -padding ||
               minY > size.y + padding;
    }

    inline bool IsPlausibleWorldCircle(const Vec3& center, float radius) {
        return center.IsValid() &&
               std::isfinite(radius) &&
               radius > 0.0f &&
               radius <= 30000.0f &&
               center.x + radius >= -30000.0f &&
               center.x - radius <= 30000.0f &&
               center.y >= -5000.0f &&
               center.y <= 5000.0f &&
               center.z + radius >= -30000.0f &&
               center.z - radius <= 30000.0f;
    }

    inline void F7WndProcHandler(SDK::Game::WndEventArgs& args) {
        if (args.Msg == WM_KEYDOWN && args.WParam == VK_F7) {
            DrawingEnabled = !DrawingEnabled;
        }
    }

    inline void UpdateHotkey() {
        if (!F7WndProcInstalled) {
            SDK::Game::AddOnWndProc(&F7WndProcHandler);
            F7WndProcInstalled = true;
        }
    }

    struct ViewProjectionFrameCache {
        int frame = -1;
        bool valid = false;
        ::CoreView::Matrix4x4 matrix = {};
        Vec2 rendererSize = {};
    };

    inline ViewProjectionFrameCache& GetViewProjectionFrameCache() {
        static ViewProjectionFrameCache cache = {};
        const int frame = ImGui::GetCurrentContext() ? ImGui::GetFrameCount() : -1;
        if (cache.frame != frame) {
            cache = {};
            cache.frame = frame;
            cache.valid =
                ::CoreView::ReadViewProjection(cache.matrix) &&
                ::CoreView::GetRendererSize(cache.rendererSize);
        }
        return cache;
    }

    inline bool ProjectWorldToScreen(const Vec3& world,
                                     const ViewProjectionFrameCache& cache,
                                     Vec2& screen) {
        if (cache.valid &&
            ::CoreView::ProjectWorldToScreen(
                world,
                cache.matrix,
                cache.rendererSize,
                screen)) {
            return true;
        }
        return ::CoreView::WorldToScreen(world, screen);
    }

    inline bool ProjectWorldToScreen(const Vec3& world, Vec2& screen) {
        return ProjectWorldToScreen(world, GetViewProjectionFrameCache(), screen);
    }

    inline void IncludeScreenPoint(const Vec2& point,
                                   float& minX,
                                   float& maxX,
                                   float& minY,
                                   float& maxY) {
        minX = (std::min)(minX, point.x);
        maxX = (std::max)(maxX, point.x);
        minY = (std::min)(minY, point.y);
        maxY = (std::max)(maxY, point.y);
    }

    inline bool IsWorldCircleOutsideScreen(const Vec3& center,
                                           float radius,
                                           const ViewProjectionFrameCache& cache) {
        if (!cache.valid) {
            return false;
        }

        Vec2 centerScreen = {};
        if (!::CoreView::ProjectWorldToScreen(
                center,
                cache.matrix,
                cache.rendererSize,
                centerScreen)) {
            return false;
        }

        float minX = centerScreen.x;
        float maxX = centerScreen.x;
        float minY = centerScreen.y;
        float maxY = centerScreen.y;
        int projectedSamples = 0;

        const Vec3 samples[4] = {
            { center.x + radius, center.y, center.z },
            { center.x - radius, center.y, center.z },
            { center.x, center.y, center.z + radius },
            { center.x, center.y, center.z - radius }
        };

        for (const Vec3& sample : samples) {
            Vec2 screen = {};
            if (!::CoreView::ProjectWorldToScreen(
                    sample,
                    cache.matrix,
                    cache.rendererSize,
                    screen)) {
                continue;
            }

            IncludeScreenPoint(screen, minX, maxX, minY, maxY);
            ++projectedSamples;
        }

        if (projectedSamples == 0) {
            return false;
        }

        const Vec2 size = cache.rendererSize;
        return maxX < -kScreenRejectPadding ||
               minX > size.x + kScreenRejectPadding ||
               maxY < -kScreenRejectPadding ||
               minY > size.y + kScreenRejectPadding;
    }

    inline HandlerList<void(*)()> DrawHandlers;
    inline HandlerList<void(*)()> AlwaysDrawHandlers;
    inline HandlerList<void(*)()> EndSceneHandlers;
    inline HandlerList<void(*)()> PreResetHandlers;
    inline HandlerList<void(*)()> PostResetHandlers;
    inline volatile LONG CaptureVisibleUntilTick = 0;
} // namespace detail

using DrawHandler = void(*)();
using Matrix4x4 = ::CoreView::Matrix4x4;

inline std::uint32_t Color(int r, int g, int b, int a = 255) {
    return detail::ToArgb(r, g, b, a);
}

inline bool IsEnabled() {
    detail::UpdateHotkey();
    return detail::DrawingEnabled && !IsAllDrawingHidden();
}

inline void SetEnabled(bool enabled) {
    detail::DrawingEnabled = enabled;
}

inline bool ToggleEnabled() {
    detail::DrawingEnabled = !detail::DrawingEnabled;
    return detail::DrawingEnabled;
}

inline ImDrawList* GetDrawList(bool foreground = true) {
    if (!ImGui::GetCurrentContext()) {
        return nullptr;
    }
    return foreground
        ? ImGui::GetForegroundDrawList()
        : ImGui::GetBackgroundDrawList();
}

inline void MarkCaptureVisibleContent(DWORD durationMs = 150) {
    const DWORD now = GetTickCount();
    InterlockedExchange(
        &detail::CaptureVisibleUntilTick,
        static_cast<LONG>(now + durationMs));
}

inline bool HasCaptureVisibleContent() {
    const DWORD until = static_cast<DWORD>(
        InterlockedCompareExchange(&detail::CaptureVisibleUntilTick, 0, 0));
    return static_cast<LONG>(until - GetTickCount()) > 0;
}

inline void ClearCaptureVisibleContent() {
    InterlockedExchange(&detail::CaptureVisibleUntilTick, 0);
}

inline int Width() {
    Vec2 size = {};
    return ::CoreView::GetRendererSize(size) ? static_cast<int>(size.x) : 0;
}

inline int Height() {
    Vec2 size = {};
    return ::CoreView::GetRendererSize(size) ? static_cast<int>(size.y) : 0;
}

inline float GetRendererWidth() {
    return static_cast<float>(Width());
}

inline float GetRendererHeight() {
    return static_cast<float>(Height());
}

inline Vec2 GetRendererSize() {
    Vec2 size = {};
    (void)::CoreView::GetRendererSize(size);
    return size;
}

inline bool ReadView(Matrix4x4& out) {
    return ::CoreView::ReadView(out);
}

inline Matrix4x4 View() {
    Matrix4x4 out = {};
    (void)::SDK::Drawing::ReadView(out);
    return out;
}

inline bool ReadProjection(Matrix4x4& out) {
    return ::CoreView::ReadProjection(out);
}

inline Matrix4x4 Projection() {
    Matrix4x4 out = {};
    (void)::SDK::Drawing::ReadProjection(out);
    return out;
}

inline bool ScreenToWorld(const Vec2& screen, Vec3& world) {
    (void)CoreRuntime::EnsureInitialized();
    return ::CoreView::ScreenToWorld(screen, world);
}

inline Vec3 ScreenToWorld(const Vec2& screen) {
    Vec3 world = {};
    (void)ScreenToWorld(screen, world);
    return world;
}

inline bool WorldToScreen(const Vec3& world, Vec2& screen) {
    if (!IsEnabled()) {
        screen = {};
        return false;
    }
    (void)CoreRuntime::EnsureInitialized();
    return detail::ProjectWorldToScreen(world, screen);
}

inline Vec2 WorldToScreen(const Vec3& world) {
    Vec2 screen = {};
    (void)WorldToScreen(world, screen);
    return screen;
}

inline bool WorldToScreenAlways(const Vec3& world, Vec2& screen) {
    (void)CoreRuntime::EnsureInitialized();
    return detail::ProjectWorldToScreen(world, screen);
}

inline Vec2 WorldToScreenAlways(const Vec3& world) {
    Vec2 screen = {};
    (void)WorldToScreenAlways(world, screen);
    return screen;
}

inline bool WorldToMinimap(const Vec3& world, Vec2& minimap) {
    (void)CoreRuntime::EnsureInitialized();
    return ::CoreView::WorldToMinimap(world, minimap);
}

inline Vec2 WorldToMinimap(const Vec3& world) {
    Vec2 minimap = {};
    (void)WorldToMinimap(world, minimap);
    return minimap;
}

inline bool MinimapToWorld(const Vec2& minimap, Vec3& world) {
    (void)CoreRuntime::EnsureInitialized();
    return ::CoreView::MinimapToWorld(minimap, world);
}

inline Vec3 MinimapToWorld(const Vec2& minimap) {
    Vec3 world = {};
    (void)MinimapToWorld(minimap, world);
    return world;
}

inline bool OnScreen(const Vec2& point) {
    if (!IsEnabled() || !point.IsValid()) {
        return false;
    }
    const Vec2 size = GetRendererSize();
    return point.x >= 0.0f && point.y >= 0.0f &&
           point.x <= size.x && point.y <= size.y;
}

inline bool OnScreenAlways(const Vec2& point) {
    if (!point.IsValid()) {
        return false;
    }
    const Vec2 size = GetRendererSize();
    return point.x >= 0.0f && point.y >= 0.0f &&
           point.x <= size.x && point.y <= size.y;
}

inline bool OnScreen(const Vec3& world) {
    Vec2 screen = {};
    return WorldToScreen(world, screen) && OnScreen(screen);
}

inline bool OnScreenAlways(const Vec3& world) {
    Vec2 screen = {};
    return WorldToScreenAlways(world, screen) && OnScreenAlways(screen);
}

inline void DrawLine(float x1,
                     float y1,
                     float x2,
                     float y2,
                     float width,
                     std::uint32_t color,
                     bool foreground = true) {
    if (!IsEnabled() || width <= 0.0f) {
        return;
    }

    const Vec2 start{ x1, y1 };
    const Vec2 end{ x2, y2 };
    if (!start.IsValid() || !end.IsValid()) {
        return;
    }

    const Vec2 rendererSize = GetRendererSize();
    if (detail::IsScreenLineOutside(start, end, rendererSize, detail::kScreenRejectPadding)) {
        return;
    }

    if (auto* draw = GetDrawList(foreground)) {
        draw->AddLine(
            ImVec2(start.x, start.y),
            ImVec2(end.x, end.y),
            detail::FromArgb(color),
            width);
    }
}

inline void DrawLine(const Vec2& start,
                     const Vec2& end,
                     float width,
                     std::uint32_t color,
                     bool foreground = true) {
    if (!start.IsValid() || !end.IsValid()) {
        return;
    }
    DrawLine(start.x, start.y, end.x, end.y, width, color, foreground);
}

inline void DrawLine(const Vec2& start,
                     const Vec2& end,
                     std::uint32_t color,
                     float thickness = 1.5f,
                     bool foreground = true) {
    DrawLine(start, end, thickness, color, foreground);
}

inline void DrawLine(const Vec3& start,
                     const Vec3& end,
                     std::uint32_t color,
                     float thickness = 1.5f,
                     bool foreground = true) {
    if (!IsEnabled() ||
        thickness <= 0.0f ||
        !start.IsValid() ||
        !end.IsValid()) {
        return;
    }

    auto* draw = GetDrawList(foreground);
    if (!draw) {
        return;
    }

    const auto& cache = detail::GetViewProjectionFrameCache();
    if (!cache.valid) {
        return;
    }

    Vec2 screenStart = {};
    Vec2 screenEnd = {};
    if (!detail::ProjectWorldToScreen(start, cache, screenStart) ||
        !detail::ProjectWorldToScreen(end, cache, screenEnd) ||
        !screenStart.IsValid() ||
        !screenEnd.IsValid()) {
        return;
    }

    if (detail::IsScreenLineOutside(
            screenStart,
            screenEnd,
            cache.rendererSize,
            detail::kScreenRejectPadding)) {
        return;
    }

    draw->AddLine(
        ImVec2(screenStart.x, screenStart.y),
        ImVec2(screenEnd.x, screenEnd.y),
        detail::FromArgb(color),
        thickness);
}

inline void DrawPolyline(const Vec2* points,
                         int count,
                         float thickness,
                         std::uint32_t color,
                         bool closed = false,
                         bool foreground = true) {
    if (!IsEnabled() || !points || count < 2 || thickness <= 0.0f) {
        return;
    }

    auto* draw = GetDrawList(foreground);
    if (!draw) {
        return;
    }

    const int cappedCount = (std::min)(count, detail::kMaxPolylinePoints);
    ImVec2 screenPoints[detail::kMaxPolylinePoints] = {};
    int visible = 0;
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;

    for (int i = 0; i < cappedCount; ++i) {
        const Vec2& point = points[i];
        if (!point.IsValid()) {
            continue;
        }

        screenPoints[visible++] = ImVec2(point.x, point.y);
        if (visible == 1) {
            minX = maxX = point.x;
            minY = maxY = point.y;
        } else {
            detail::IncludeScreenPoint(point, minX, maxX, minY, maxY);
        }
    }

    if (visible < 2) {
        return;
    }

    const Vec2 rendererSize = GetRendererSize();
    if (detail::IsScreenBoundsOutside(
            minX,
            maxX,
            minY,
            maxY,
            rendererSize,
            detail::kScreenRejectPadding)) {
        return;
    }

    draw->AddPolyline(
        screenPoints,
        visible,
        detail::FromArgb(color),
        closed && visible == count && count <= detail::kMaxPolylinePoints,
        thickness);
}

inline void DrawPolylineWorld(const Vec3* points,
                              int count,
                              float thickness,
                              std::uint32_t color,
                              bool closed = false,
                              bool foreground = true) {
    if (!IsEnabled() || !points || count < 2 || thickness <= 0.0f) {
        return;
    }

    auto* draw = GetDrawList(foreground);
    if (!draw) {
        return;
    }

    const auto& cache = detail::GetViewProjectionFrameCache();
    if (!cache.valid) {
        return;
    }

    const int cappedCount = (std::min)(count, detail::kMaxPolylinePoints);
    ImVec2 screenPoints[detail::kMaxPolylinePoints] = {};
    int visible = 0;
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;

    for (int i = 0; i < cappedCount; ++i) {
        if (!points[i].IsValid()) {
            continue;
        }

        Vec2 screen = {};
        if (!detail::ProjectWorldToScreen(points[i], cache, screen) ||
            !screen.IsValid()) {
            continue;
        }

        screenPoints[visible++] = ImVec2(screen.x, screen.y);
        if (visible == 1) {
            minX = maxX = screen.x;
            minY = maxY = screen.y;
        } else {
            detail::IncludeScreenPoint(screen, minX, maxX, minY, maxY);
        }
    }

    if (visible < 2) {
        return;
    }

    if (detail::IsScreenBoundsOutside(
            minX,
            maxX,
            minY,
            maxY,
            cache.rendererSize,
            detail::kScreenRejectPadding)) {
        return;
    }

    draw->AddPolyline(
        screenPoints,
        visible,
        detail::FromArgb(color),
        closed && visible == count && count <= detail::kMaxPolylinePoints,
        thickness);
}

inline void DrawCircle(const Vec2& position,
                       float radius,
                       float thickness,
                       std::uint32_t color,
                       int segments = 64,
                       bool foreground = true) {
    if (!IsEnabled() ||
        !position.IsValid() ||
        !std::isfinite(radius) ||
        radius <= 0.0f ||
        thickness <= 0.0f) {
        return;
    }

    const Vec2 rendererSize = GetRendererSize();
    if (detail::IsScreenCircleOutside(
            position,
            radius,
            rendererSize,
            detail::kScreenRejectPadding)) {
        return;
    }

    auto* draw = GetDrawList(foreground);
    if (!draw) {
        return;
    }

    const auto& lut = detail::SelectCircleLut(segments);
    ImVec2 points[detail::kMaxCircleSegments] = {};
    for (int i = 0; i < lut.segments; ++i) {
        points[i] = ImVec2(
            position.x + lut.points[i].x * radius,
            position.y + lut.points[i].z * radius);
    }

    draw->AddPolyline(
        points,
        lut.segments,
        detail::FromArgb(color),
        true,
        thickness);
}

inline void DrawCircle(const Vec3& center,
                       float radius,
                       std::uint32_t color,
                       float thickness = 1.5f,
                       int segments = 64,
                       bool foreground = true) {
    if (!IsEnabled() ||
        !detail::IsPlausibleWorldCircle(center, radius) ||
        thickness <= 0.0f ||
        segments < 8) {
        return;
    }

    auto* draw = GetDrawList(foreground);
    if (!draw) {
        return;
    }

    const auto& cache = detail::GetViewProjectionFrameCache();
    if (!cache.valid) {
        return;
    }

    if (detail::IsWorldCircleOutsideScreen(center, radius, cache)) {
        return;
    }

    const auto& lut = detail::SelectCircleLut(segments);
    ImVec2 points[detail::kMaxCircleSegments] = {};
    int visible = 0;
    for (int i = 0; i < lut.segments; ++i) {
        const Vec3 point{
            center.x + lut.points[i].x * radius,
            center.y,
            center.z + lut.points[i].z * radius
        };

        Vec2 screen = {};
        if (detail::ProjectWorldToScreen(point, cache, screen) && screen.IsValid()) {
            points[visible++] = ImVec2(screen.x, screen.y);
        }
    }

    if (visible >= 2) {
        draw->AddPolyline(
            points,
            visible,
            detail::FromArgb(color),
            visible == lut.segments,
            thickness);
    }
}

inline void DrawCircleAlways(const Vec3& center,
                             float radius,
                             std::uint32_t color,
                             float thickness = 1.5f,
                             int segments = 64,
                             bool foreground = true) {
    if (!detail::IsPlausibleWorldCircle(center, radius) ||
        thickness <= 0.0f ||
        segments < 8) {
        return;
    }

    auto* draw = GetDrawList(foreground);
    if (!draw) {
        return;
    }

    const auto& cache = detail::GetViewProjectionFrameCache();
    if (!cache.valid) {
        return;
    }

    if (detail::IsWorldCircleOutsideScreen(center, radius, cache)) {
        return;
    }

    const auto& lut = detail::SelectCircleLut(segments);
    ImVec2 points[detail::kMaxCircleSegments] = {};
    int visible = 0;
    for (int i = 0; i < lut.segments; ++i) {
        const Vec3 point{
            center.x + lut.points[i].x * radius,
            center.y,
            center.z + lut.points[i].z * radius
        };

        Vec2 screen = {};
        if (detail::ProjectWorldToScreen(point, cache, screen) && screen.IsValid()) {
            points[visible++] = ImVec2(screen.x, screen.y);
        }
    }

    if (visible >= 2) {
        draw->AddPolyline(
            points,
            visible,
            detail::FromArgb(color),
            visible == lut.segments,
            thickness);
    }
}

inline void DrawText(float x,
                     float y,
                     std::uint32_t color,
                     const char* text,
                     bool foreground = true) {
    if (!IsEnabled() || !text || !*text) {
        return;
    }
    if (auto* draw = GetDrawList(foreground)) {
        draw->AddText(ImVec2(x, y), detail::FromArgb(color), text);
    }
}

inline void DrawText(const Vec2& position,
                     const char* text,
                     std::uint32_t color = 0xFFFFFFFFu,
                     bool centered = false,
                     bool foreground = true) {
    if (!position.IsValid() || !text || !*text) {
        return;
    }

    ImVec2 screen(position.x, position.y);
    if (centered && ImGui::GetCurrentContext()) {
        const ImVec2 textSize = ImGui::CalcTextSize(text);
        screen.x -= textSize.x * 0.5f;
        screen.y -= textSize.y * 0.5f;
    }
    DrawText(screen.x, screen.y, color, text, foreground);
}

inline void DrawText(const Vec3& world,
                     const char* text,
                     std::uint32_t color = 0xFFFFFFFFu,
                     bool centered = false,
                     bool foreground = true) {
    Vec2 screen = {};
    if (WorldToScreen(world, screen)) {
        DrawText(screen, text, color, centered, foreground);
    }
}

inline std::uint32_t ColorHSV(float h, float s, float v, float a = 1.0f) {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
    return Color(
        static_cast<int>(r * 255.0f),
        static_cast<int>(g * 255.0f),
        static_cast<int>(b * 255.0f),
        static_cast<int>(a * 255.0f));
}

inline void DrawCircleRainbowStatic(const Vec3& center,
                                    float radius,
                                    float thickness = 2.0f,
                                    int segments = 64,
                                    bool foreground = true) {
    if (!IsEnabled() ||
        !detail::IsPlausibleWorldCircle(center, radius) ||
        thickness <= 0.0f ||
        segments < 8) {
        return;
    }

    auto* draw = GetDrawList(foreground);
    if (!draw) {
        return;
    }

    const auto& cache = detail::GetViewProjectionFrameCache();
    if (!cache.valid) {
        return;
    }

    if (detail::IsWorldCircleOutsideScreen(center, radius, cache)) {
        return;
    }

    const auto& lut = detail::SelectCircleLut(segments);
    ImVec2 points[detail::kMaxCircleSegments] = {};
    bool projected[detail::kMaxCircleSegments] = {};
    int visible = 0;

    for (int i = 0; i < lut.segments; ++i) {
        const Vec3 point{
            center.x + lut.points[i].x * radius,
            center.y,
            center.z + lut.points[i].z * radius
        };

        Vec2 screen = {};
        if (detail::ProjectWorldToScreen(point, cache, screen) && screen.IsValid()) {
            points[i] = ImVec2(screen.x, screen.y);
            projected[i] = true;
            ++visible;
        }
    }

    if (visible < 2) {
        return;
    }

    for (int i = 0; i < lut.segments; ++i) {
        const int next = i + 1 == lut.segments ? 0 : i + 1;
        if (!projected[i] || !projected[next]) {
            continue;
        }

        const int colorIndex = i * detail::kRainbowColorCount / lut.segments;
        draw->AddLine(
            points[i],
            points[next],
            detail::RainbowColor(colorIndex),
            thickness);
    }
}

inline void DrawCircleRainbow(const Vec3& center,
                              float radius,
                              float thickness = 2.0f,
                              int segments = 64,
                              bool foreground = true,
                              float speed = 1.0f) {
    if (!IsEnabled() ||
        !detail::IsPlausibleWorldCircle(center, radius) ||
        thickness <= 0.0f ||
        segments < 8) {
        return;
    }

    auto* draw = GetDrawList(foreground);
    if (!draw) {
        return;
    }

    const auto& cache = detail::GetViewProjectionFrameCache();
    if (!cache.valid) {
        return;
    }

    if (detail::IsWorldCircleOutsideScreen(center, radius, cache)) {
        return;
    }

    const auto& lut = detail::SelectCircleLut(segments);
    ImVec2 points[detail::kMaxCircleSegments] = {};
    bool projected[detail::kMaxCircleSegments] = {};
    int visible = 0;

    for (int i = 0; i < lut.segments; ++i) {
        const Vec3 point{
            center.x + lut.points[i].x * radius,
            center.y,
            center.z + lut.points[i].z * radius
        };

        Vec2 screen = {};
        if (detail::ProjectWorldToScreen(point, cache, screen) && screen.IsValid()) {
            points[i] = ImVec2(screen.x, screen.y);
            projected[i] = true;
            ++visible;
        }
    }

    if (visible < 2) {
        return;
    }

    int phase = 0;
    if (ImGui::GetCurrentContext() && std::isfinite(speed)) {
        phase = static_cast<int>(
            static_cast<float>(ImGui::GetTime()) *
            speed *
            static_cast<float>(detail::kRainbowColorCount));
    }

    for (int i = 0; i < lut.segments; ++i) {
        const int next = i + 1 == lut.segments ? 0 : i + 1;
        if (!projected[i] || !projected[next]) {
            continue;
        }

        const int colorIndex =
            i * detail::kRainbowColorCount / lut.segments + phase;
        draw->AddLine(
            points[i],
            points[next],
            detail::RainbowColor(colorIndex),
            thickness);
    }
}

inline Vec2 HpBarScreenPos(const Vec3& worldPos, float hpBarHeight) {
    Vec3 offsetPos = worldPos;
    offsetPos.y += hpBarHeight;

    Vec2 screenPos = {};
    if (!WorldToScreen(offsetPos, screenPos)) {
        return {};
    }

    screenPos.y -= GetRendererHeight() * 0.00083333335f * hpBarHeight;
    return screenPos;
}

template <typename TGameObject>
inline Vec2 HpBarScreenPos(const TGameObject& obj) {
    return HpBarScreenPos(obj.Position(), obj.GetHpBarHeight());
}

inline bool AddOnDraw(DrawHandler handler) { return detail::DrawHandlers.Add(handler); }
inline bool RemoveOnDraw(DrawHandler handler) { return detail::DrawHandlers.Remove(handler); }
inline bool AddOnAlwaysDraw(DrawHandler handler) { return detail::AlwaysDrawHandlers.Add(handler); }
inline bool RemoveOnAlwaysDraw(DrawHandler handler) { return detail::AlwaysDrawHandlers.Remove(handler); }
inline bool AddOnEndScene(DrawHandler handler) { return detail::EndSceneHandlers.Add(handler); }
inline bool RemoveOnEndScene(DrawHandler handler) { return detail::EndSceneHandlers.Remove(handler); }
inline bool AddOnPreReset(DrawHandler handler) { return detail::PreResetHandlers.Add(handler); }
inline bool RemoveOnPreReset(DrawHandler handler) { return detail::PreResetHandlers.Remove(handler); }
inline bool AddOnPostReset(DrawHandler handler) { return detail::PostResetHandlers.Add(handler); }
inline bool RemoveOnPostReset(DrawHandler handler) { return detail::PostResetHandlers.Remove(handler); }

inline void DispatchDraw() {
    detail::UpdateHotkey();
    if (IsAllDrawingHidden()) {
        return;
    }
    if (IsEnabled()) {
        detail::DrawHandlers.Fire();
    }
    detail::AlwaysDrawHandlers.Fire();
}

inline void DispatchEndScene() {
    detail::UpdateHotkey();
    if (IsAllDrawingHidden()) {
        return;
    }
    if (IsEnabled()) {
        detail::EndSceneHandlers.Fire();
    }
}

inline void DispatchPreReset() { detail::PreResetHandlers.Fire(); }
inline void DispatchPostReset() { detail::PostResetHandlers.Fire(); }

inline void Reset() {
    detail::DrawHandlers.Clear();
    detail::AlwaysDrawHandlers.Clear();
    detail::EndSceneHandlers.Clear();
    detail::PreResetHandlers.Clear();
    detail::PostResetHandlers.Clear();
    detail::F7WndProcInstalled = false;
    ClearCaptureVisibleContent();
}

struct DrawEventSlot {
    bool (*Add)(DrawHandler) = nullptr;
    bool (*Remove)(DrawHandler) = nullptr;
    bool operator+=(DrawHandler handler) const { return Add ? Add(handler) : false; }
    bool operator-=(DrawHandler handler) const { return Remove ? Remove(handler) : false; }
    bool operator()(DrawHandler handler) const { return (*this += handler); }
};

inline DrawEventSlot OnDraw{ &AddOnDraw, &RemoveOnDraw };
inline DrawEventSlot OnAlwaysDraw{ &AddOnAlwaysDraw, &RemoveOnAlwaysDraw };
inline DrawEventSlot OnEndScene{ &AddOnEndScene, &RemoveOnEndScene };
inline DrawEventSlot OnPreReset{ &AddOnPreReset, &RemoveOnPreReset };
inline DrawEventSlot OnPostReset{ &AddOnPostReset, &RemoveOnPostReset };

} // namespace SDK::Drawing
