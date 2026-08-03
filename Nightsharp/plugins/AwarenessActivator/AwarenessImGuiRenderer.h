#pragma once

#include "../../Core/CoreView.h"
#include "../../imgui/imgui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
namespace NightSharp::Companion {

// Awareness uses one foreground draw list and one cached view-projection per
// frame. This keeps the hot render path out of SDK::Drawing's per-primitive
// validation and lookup helpers while preserving the same screen/world API.
class AwarenessImGuiRenderer final {
public:
    bool BeginFrame(bool performanceMode = false,
                    bool needWorldProjection = true) noexcept {
        draw_ = nullptr;
        active_ = false;
        projectionReady_ = false;
        rendererSize_ = {};
        viewportOrigin_ = {};
        viewProjection_ = {};
        minimapRefreshFrames_ = performanceMode
            ? kPerformanceMinimapRefreshFrames : 1u;
        ++frameSerial_;

        if (!ImGui::GetCurrentContext()) {
            return false;
        }

        draw_ = ImGui::GetForegroundDrawList();
        if (!draw_) {
            return false;
        }
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (viewport && viewport->Size.x > 0.0f &&
            viewport->Size.y > 0.0f) {
            rendererSize_ = {
                viewport->Size.x, viewport->Size.y
            };
            viewportOrigin_ = {
                viewport->Pos.x, viewport->Pos.y
            };
        } else if (!CoreView::GetRendererSize(rendererSize_)) {
            draw_ = nullptr;
            return false;
        }

        // The tactical-map lookup is substantially more expensive than the
        // coordinate transform. Preserve it across frames and invalidate it
        // only when the viewport changes; performance mode also refreshes it
        // periodically so minimap scale/position changes are still detected.
        if (!SameViewport(rendererSize_, cachedRendererSize_) ||
            !SameViewport(viewportOrigin_, cachedViewportOrigin_)) {
            tacticalMap_ = {};
            minimapReady_ = false;
            tacticalMapFrame_ = 0;
            cachedRendererSize_ = rendererSize_;
            cachedViewportOrigin_ = viewportOrigin_;
        }

        // Screen/minimap panels can still render when the game camera is not
        // available for a frame. World primitives simply no-op in that case.
        projectionReady_ = needWorldProjection &&
            CoreView::ReadViewProjection(viewProjection_);
        active_ = true;
        return true;
    }

    bool IsActive() const noexcept { return active_ && draw_ != nullptr; }

    bool WorldToMinimap(const Vec3& world, Vec2& out) const noexcept {
        if (!IsActive() || !world.IsValid()) {
            out = {};
            return false;
        }

        const std::uint64_t age = frameSerial_ - tacticalMapFrame_;
        const bool refreshDue = !minimapReady_ ||
            age >= static_cast<std::uint64_t>(minimapRefreshFrames_);
        if (refreshDue) {
            const CoreMap::TacticalMapState next =
                CoreMap::GetTacticalMap();
            tacticalMapFrame_ = frameSerial_;
            if (next.IsValid()) {
                tacticalMap_ = next;
                minimapReady_ = true;
            } else if (!minimapReady_) {
                out = {};
                return false;
            }
        }

        if (!tacticalMap_.WorldToMinimap(world, out) ||
            !out.IsValid()) {
            out = {};
            return false;
        }
        out = out + viewportOrigin_;
        return out.IsValid();
    }

    bool WorldToScreen(const Vec3& world, Vec2& out) const noexcept {
        out = {};
        if (!IsActive() || !projectionReady_ || !world.IsValid()) {
            return false;
        }
        if (!CoreView::ProjectWorldToScreen(
                world, viewProjection_, rendererSize_, out)) {
            out = {};
            return false;
        }
        out = out + viewportOrigin_;
        return out.IsValid();
    }

    void DrawLine(const Vec2& start,
                  const Vec2& end,
                  std::uint32_t color,
                  float thickness = 1.5f) const noexcept {
        if (!IsActive() || !start.IsValid() || !end.IsValid() ||
            !std::isfinite(thickness) || thickness <= 0.0f ||
            IsLineOutside(start, end)) {
            return;
        }
        draw_->AddLine(
            ImVec2(start.x, start.y), ImVec2(end.x, end.y),
            ToImColor(color), thickness);
    }

    void DrawLine(const Vec3& start,
                  const Vec3& end,
                  std::uint32_t color,
                  float thickness = 1.5f) const noexcept {
        Vec2 screenStart = {};
        Vec2 screenEnd = {};
        if (!WorldToScreen(start, screenStart) ||
            !WorldToScreen(end, screenEnd)) {
            return;
        }
        DrawLine(screenStart, screenEnd, color, thickness);
    }

    void DrawCircle(const Vec2& position,
                    float radius,
                    float thickness,
                    std::uint32_t color,
                    int segments = 32) const noexcept {
        if (!IsActive() || !position.IsValid() ||
            !std::isfinite(radius) || radius <= 0.0f ||
            !std::isfinite(thickness) || thickness <= 0.0f ||
            IsCircleOutside(position, radius)) {
            return;
        }
        draw_->AddCircle(
            ImVec2(position.x, position.y), radius, ToImColor(color),
            ClampSegments(segments), thickness);
    }

    void DrawCircle(const Vec3& center,
                    float radius,
                    std::uint32_t color,
                    float thickness = 1.5f,
                    int segments = 32) const noexcept {
        if (!IsActive() || !projectionReady_ || !center.IsValid() ||
            !std::isfinite(radius) || radius <= 0.0f ||
            !std::isfinite(thickness) || thickness <= 0.0f) {
            return;
        }

        const int count = ClampSegments(segments);
        std::array<ImVec2, kMaxCircleSegments> points{};
        std::array<bool, kMaxCircleSegments> projected{};
        int projectedCount = 0;
        float minX = 0.0f;
        float maxX = 0.0f;
        float minY = 0.0f;
        float maxY = 0.0f;
        const auto& unit = UnitCircle();

        for (int i = 0; i < count; ++i) {
            const std::size_t unitIndex =
                static_cast<std::size_t>(
                    (i * kMaxCircleSegments) / count);
            const Vec3 sample{
                center.x + unit[unitIndex].x * radius,
                center.y,
                center.z + unit[unitIndex].y * radius
            };
            Vec2 screen = {};
            // Use the shared projection path so viewport origin is applied in
            // exactly the same way as every other world primitive.
            if (!WorldToScreen(sample, screen)) {
                continue;
            }

            const std::size_t index = static_cast<std::size_t>(i);
            points[index] = ImVec2(screen.x, screen.y);
            projected[index] = true;
            ++projectedCount;
            if (projectedCount == 1) {
                minX = maxX = screen.x;
                minY = maxY = screen.y;
            } else {
                minX = (std::min)(minX, screen.x);
                maxX = (std::max)(maxX, screen.x);
                minY = (std::min)(minY, screen.y);
                maxY = (std::max)(maxY, screen.y);
            }
        }

        if (projectedCount < 2 ||
            IsBoundsOutside(minX, maxX, minY, maxY)) {
            return;
        }

        if (projectedCount == count) {
            draw_->AddPolyline(
                points.data(), count, ToImColor(color),
                ImDrawFlags_Closed, thickness);
            return;
        }

        // A point can fail projection when part of the circle crosses behind
        // the camera. Never connect across that gap; doing so creates the
        // large diagonal/irregular polygon that the old renderer produced.
        int firstGap = 0;
        while (firstGap < count &&
               projected[static_cast<std::size_t>(firstGap)]) {
            ++firstGap;
        }
        const int start = (firstGap + 1) % count;
        std::array<ImVec2, kMaxCircleSegments> run{};
        int runCount = 0;
        for (int step = 0; step < count; ++step) {
            const int index = (start + step) % count;
            if (projected[static_cast<std::size_t>(index)]) {
                run[static_cast<std::size_t>(runCount++)] =
                    points[static_cast<std::size_t>(index)];
                continue;
            }
            if (runCount >= 2) {
                draw_->AddPolyline(
                    run.data(), runCount, ToImColor(color),
                    ImDrawFlags_None, thickness);
            }
            runCount = 0;
        }
        if (runCount >= 2) {
            draw_->AddPolyline(
                run.data(), runCount, ToImColor(color),
                ImDrawFlags_None, thickness);
        }
    }

    void DrawCircleFilled(const Vec2& position,
                          float radius,
                          std::uint32_t color,
                          int segments = 16) const noexcept {
        if (!IsActive() || !position.IsValid() ||
            !std::isfinite(radius) || radius <= 0.0f ||
            IsCircleOutside(position, radius)) {
            return;
        }
        draw_->AddCircleFilled(
            ImVec2(position.x, position.y), radius, ToImColor(color),
            ClampSegments(segments));
    }
    void DrawRectFilled(const Vec2& min,
                        const Vec2& max,
                        std::uint32_t color,
                        float rounding = 0.0f) const noexcept {
        if (!IsActive() || !min.IsValid() || !max.IsValid() ||
            max.x < min.x || max.y < min.y ||
            !std::isfinite(rounding) || rounding < 0.0f ||
            IsBoundsOutside(min.x, max.x, min.y, max.y)) {
            return;
        }
        draw_->AddRectFilled(
            ImVec2(min.x, min.y), ImVec2(max.x, max.y),
            ToImColor(color), rounding);
    }

    void DrawText(float x,
                  float y,
                  std::uint32_t color,
                  const char* text) const noexcept {
        if (!IsActive() || !text || !text[0] ||
            !std::isfinite(x) || !std::isfinite(y)) {
            return;
        }
        draw_->AddText(ImVec2(x, y), ToImColor(color), text);
    }

    void DrawText(const Vec2& position,
                  const char* text,
                  std::uint32_t color = 0xFFFFFFFFu,
                  bool centered = false) const noexcept {
        if (!position.IsValid() || !text || !text[0]) {
            return;
        }
        ImVec2 screen(position.x, position.y);
        if (centered) {
            const ImVec2 size = ImGui::CalcTextSize(text);
            screen.x -= size.x * 0.5f;
            screen.y -= size.y * 0.5f;
        }
        DrawText(screen.x, screen.y, color, text);
    }
    void DrawTextSmall(const Vec2& position,
                       const char* text,
                       std::uint32_t color = 0xFFFFFFFFu,
                       bool centered = false,
                       float fontSize = 11.0f) const noexcept {
        if (!IsActive() || !position.IsValid() || !text || !text[0] ||
            !std::isfinite(fontSize) || fontSize <= 0.0f) {
            return;
        }
        const ImFont* font = ImGui::GetFont();
        if (!font) {
            return;
        }
        ImVec2 screen(position.x, position.y);
        if (centered) {
            const ImVec2 size = font->CalcTextSizeA(
                fontSize, (std::numeric_limits<float>::max)(), 0.0f, text);
            screen.x -= size.x * 0.5f;
            screen.y -= size.y * 0.5f;
        }
        draw_->AddText(
            font, fontSize, screen, ToImColor(color), text);
    }

    void DrawText(const Vec3& world,
                  const char* text,
                  std::uint32_t color = 0xFFFFFFFFu,
                  bool centered = false) const noexcept {
        Vec2 screen = {};
        if (WorldToScreen(world, screen)) {
            DrawText(screen, text, color, centered);
        }
    }

    bool DrawIcon(const Vec2& center,
                  ImTextureID texture,
                  float size,
                  std::uint32_t tint = 0xFFFFFFFFu) const noexcept {
        if (!IsActive() || !texture || !center.IsValid() ||
            !std::isfinite(size) || size <= 0.0f ||
            IsCircleOutside(center, size * 0.5f)) {
            return false;
        }
        const float half = size * 0.5f;
        draw_->AddImage(
            texture,
            ImVec2(center.x - half, center.y - half),
            ImVec2(center.x + half, center.y + half),
            ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
            ToImColor(tint));
        return true;
    }

    bool DrawIcon(const Vec3& world,
                  ImTextureID texture,
                  float size,
                  std::uint32_t tint = 0xFFFFFFFFu) const noexcept {
        Vec2 screen = {};
        return WorldToScreen(world, screen) &&
               DrawIcon(screen, texture, size, tint);
    }

private:
    static constexpr int kMaxCircleSegments = 128;
    static constexpr float kScreenPadding = 128.0f;
    static constexpr std::uint32_t kPerformanceMinimapRefreshFrames = 10u;

    static bool SameViewport(const Vec2& left,
                             const Vec2& right) noexcept {
        return std::fabs(left.x - right.x) <= 0.5f &&
               std::fabs(left.y - right.y) <= 0.5f;
    }

    static int ClampSegments(int segments) noexcept {
        return (std::max)(8, (std::min)(segments, kMaxCircleSegments));
    }

    static ImU32 ToImColor(std::uint32_t argb) noexcept {
        return IM_COL32(
            static_cast<int>((argb >> 16) & 0xFF),
            static_cast<int>((argb >> 8) & 0xFF),
            static_cast<int>(argb & 0xFF),
            static_cast<int>((argb >> 24) & 0xFF));
    }

    static const std::array<Vec2, kMaxCircleSegments>& UnitCircle() noexcept {
        static const std::array<Vec2, kMaxCircleSegments> points = [] {
            std::array<Vec2, kMaxCircleSegments> result{};
            constexpr float kPi = 3.14159265358979323846f;
            for (int i = 0; i < kMaxCircleSegments; ++i) {
                const float angle =
                    (2.0f * kPi * static_cast<float>(i)) /
                    static_cast<float>(kMaxCircleSegments);
                result[static_cast<std::size_t>(i)] = {
                    std::cos(angle), std::sin(angle)
                };
            }
            return result;
        }();
        return points;
    }

    bool IsBoundsOutside(float minX,
                         float maxX,
                         float minY,
                         float maxY) const noexcept {
        const float left = viewportOrigin_.x;
        const float top = viewportOrigin_.y;
        const float right = left + rendererSize_.x;
        const float bottom = top + rendererSize_.y;
        return maxX < left - kScreenPadding ||
               minX > right + kScreenPadding ||
               maxY < top - kScreenPadding ||
               minY > bottom + kScreenPadding;
    }
    bool IsCircleOutside(const Vec2& center, float radius) const noexcept {
        const float left = viewportOrigin_.x;
        const float top = viewportOrigin_.y;
        const float right = left + rendererSize_.x;
        const float bottom = top + rendererSize_.y;
        return center.x + radius < left - kScreenPadding ||
               center.x - radius > right + kScreenPadding ||
               center.y + radius < top - kScreenPadding ||
               center.y - radius > bottom + kScreenPadding;
    }

    bool IsLineOutside(const Vec2& start, const Vec2& end) const noexcept {
        const float minX = (std::min)(start.x, end.x);
        const float maxX = (std::max)(start.x, end.x);
        const float minY = (std::min)(start.y, end.y);
        const float maxY = (std::max)(start.y, end.y);
        return IsBoundsOutside(minX, maxX, minY, maxY);
    }

    ImDrawList* draw_ = nullptr;
    Vec2 rendererSize_{};
    Vec2 viewportOrigin_{};
    Vec2 cachedRendererSize_{};
    Vec2 cachedViewportOrigin_{};
    CoreView::Matrix4x4 viewProjection_{};
    bool active_ = false;
    bool projectionReady_ = false;
    std::uint64_t frameSerial_ = 0;
    std::uint32_t minimapRefreshFrames_ = 1;
    mutable CoreMap::TacticalMapState tacticalMap_{};
    mutable std::uint64_t tacticalMapFrame_ = 0;
    mutable bool minimapReady_ = false;
};

} // namespace NightSharp::Companion
