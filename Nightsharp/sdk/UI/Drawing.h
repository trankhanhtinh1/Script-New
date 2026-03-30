#pragma once

#include "../../imgui/imgui.h"
#include "../Core/Objects.h"

#include <cmath>

namespace SDK::Drawing {

    inline ImU32 Color(int r, int g, int b, int a = 255) {
        return IM_COL32(r, g, b, a);
    }

    inline ImDrawList* GetDrawList(bool foreground = true) {
        return foreground ? ImGui::GetForegroundDrawList() : ImGui::GetBackgroundDrawList();
    }

    inline bool WorldToScreen(const Vector3& world, Vector2& screen) {
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

    inline void DrawCircle(const Vector3& center, float radius, ImU32 color, float thickness = 1.5f, int segments = 48, bool foreground = true) {
        if (radius <= 0.0f || segments < 8) {
            return;
        }

        auto* draw = GetDrawList(foreground);
        if (!draw) {
            return;
        }

        Vector2 points[128] = {};
        int visible = 0;
        const int cappedSegments = (segments > 127) ? 127 : segments;

        for (int i = 0; i <= cappedSegments; ++i) {
            const float theta = (6.28318530718f * static_cast<float>(i)) / static_cast<float>(cappedSegments);
            const Vector3 point{
                center.x + std::cos(theta) * radius,
                center.y,
                center.z + std::sin(theta) * radius
            };

            Vector2 screen = {};
            if (WorldToScreen(point, screen)) {
                points[visible++] = screen;
            }
        }

        if (visible >= 2) {
            draw->AddPolyline(reinterpret_cast<const ImVec2*>(points), visible, color, false, thickness);
        }
    }

} // namespace SDK::Drawing
