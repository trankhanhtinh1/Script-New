#pragma once

#include "../../imgui/imgui.h"
#include "../Core/Objects.h"

#include <cmath>

namespace SDK::Drawing {

    struct ViewProjectionFrameCache {
        int Frame = -1;
        bool Valid = false;
        float Matrix[16] = {};
        Vec2 RendererSize = {};
    };

    inline ViewProjectionFrameCache& GetViewProjectionFrameCache() {
        static ViewProjectionFrameCache cache = {};
        const int frame = ImGui::GetFrameCount();
        if (cache.Frame != frame) {
            cache.Frame = frame;
            cache.Valid =
                CoreAPI::View::ReadViewProjection(cache.Matrix) &&
                CoreAPI::View::GetRendererSize(cache.RendererSize);
        }
        return cache;
    }

    inline ImU32 Color(int r, int g, int b, int a = 255) {
        return IM_COL32(r, g, b, a);
    }

    inline ImDrawList* GetDrawList(bool foreground = true) {
        return foreground ? ImGui::GetForegroundDrawList() : ImGui::GetBackgroundDrawList();
    }

    inline bool WorldToScreen(const Vector3& world, Vector2& screen) {
        if (ImGui::GetCurrentContext()) {
            auto& cache = GetViewProjectionFrameCache();
            Vec2 projected = {};
            if (cache.Valid && CoreAPI::View::ProjectWorldToScreen(world, cache.Matrix, cache.RendererSize, projected)) {
                screen = { projected.x, projected.y };
                return screen.IsValid();
            }
        }

        Vec2 out = {};
        if (!CoreAPI::View::WorldToScreen(world, out)) {
            screen = {};
            return false;
        }

        screen = { out.x, out.y };
        return screen.IsValid();
    }

    inline void DrawLine(const Vector2& start, const Vector2& end, ImU32 color, float thickness = 1.5f, bool foreground = true) {
        if (!start.IsValid() || !end.IsValid()) {
            return;
        }

        if (auto* draw = GetDrawList(foreground)) {
            draw->AddLine(ImVec2(start.x, start.y), ImVec2(end.x, end.y), color, thickness);
        }
    }

    inline void DrawLine(const Vector3& start, const Vector3& end, ImU32 color, float thickness = 1.5f, bool foreground = true) {
        Vector2 screenStart = {};
        Vector2 screenEnd = {};
        if (WorldToScreen(start, screenStart) && WorldToScreen(end, screenEnd)) {
            DrawLine(screenStart, screenEnd, color, thickness, foreground);
        }
    }

    inline void DrawText(const Vector2& position, const char* text, ImU32 color = IM_COL32(255, 255, 255, 255), bool centered = false, bool foreground = true) {
        if (!text || !*text || !position.IsValid()) {
            return;
        }

        ImVec2 screen(position.x, position.y);
        if (centered) {
            const ImVec2 textSize = ImGui::CalcTextSize(text);
            screen.x -= textSize.x * 0.5f;
            screen.y -= textSize.y * 0.5f;
        }

        if (auto* draw = GetDrawList(foreground)) {
            draw->AddText(screen, color, text);
        }
    }

    inline void DrawText(const Vector3& world, const char* text, ImU32 color = IM_COL32(255, 255, 255, 255), bool centered = false, bool foreground = true) {
        Vector2 screen = {};
        if (WorldToScreen(world, screen)) {
            DrawText(screen, text, color, centered, foreground);
        }
    }

    inline void DrawCircle(const Vector3& center, float radius, ImU32 color, float thickness = 1.5f, int segments = 32, bool foreground = true) {
        if (radius <= 0.0f || segments < 8) {
            return;
        }

        auto* draw = GetDrawList(foreground);
        if (!draw) {
            return;
        }

        auto& cache = GetViewProjectionFrameCache();

        Vector2 points[128] = {};
        int visible = 0;
        const int cappedSegments = (segments > 127) ? 127 : segments;

        const float step = 6.28318530718f / static_cast<float>(cappedSegments);
        const float stepCos = std::cos(step);
        const float stepSin = std::sin(step);
        float unitX = 1.0f;
        float unitZ = 0.0f;

        for (int i = 0; i <= cappedSegments; ++i) {
            const Vector3 point{
                center.x + unitX * radius,
                center.y,
                center.z + unitZ * radius
            };

            Vector2 screen = {};
            Vec2 projected = {};
            const bool visiblePoint = cache.Valid
                ? CoreAPI::View::ProjectWorldToScreen(point, cache.Matrix, cache.RendererSize, projected)
                : WorldToScreen(point, screen);
            if (visiblePoint) {
                if (cache.Valid) {
                    screen = { projected.x, projected.y };
                }
                points[visible++] = screen;
            }

            const float nextX = unitX * stepCos - unitZ * stepSin;
            const float nextZ = unitX * stepSin + unitZ * stepCos;
            unitX = nextX;
            unitZ = nextZ;
        }

        if (visible >= 2) {
            draw->AddPolyline(reinterpret_cast<const ImVec2*>(points), visible, color, false, thickness);
        }
    }

} // namespace SDK::Drawing
