#pragma once
#include "../core/Globals.h"
#include "../core/Offsets.h"
#include "../core/Vector.h"
#include "../imgui/imgui.h"
#include <cmath>

// ============================================================================
// Drawing — World-to-Screen rendering helpers
// Reference: EnsoulSharp Drawing class
// ============================================================================

namespace SDK {
namespace Drawing {

    // ====================================================================
    // World to Screen conversion
    // ====================================================================
    inline bool WorldToScreen(const Vec3& world, Vec2& screen) {
        typedef bool(__cdecl* fnW2S)(Vec3*, Vec3*);
        static uintptr_t fn = Globals::base + Offset::Function::WorldToScreen;

        Vec3 screenPos;
        __try {
            // HudInstance → ViewPort→ W2S
            uintptr_t hud = Globals::Read<uintptr_t>(
                Globals::base + Offset::Global::HudInstance);
            if (!Globals::IsValidPtr(hud)) return false;

            uintptr_t viewport = Globals::Read<uintptr_t>(
                Globals::base + Offset::Global::ViewPort);
            if (!Globals::IsValidPtr(viewport)) return false;

            Vec3 worldCopy = world;
            bool result = ((fnW2S)fn)(&worldCopy, &screenPos);
            if (result) {
                screen.x = screenPos.x;
                screen.y = screenPos.y;
                return true;
            }
        } __except(1) {}
        return false;
    }

    // Alternative W2S using ViewMatrix
    inline bool WorldToScreen2(const Vec3& world, Vec2& screen) {
        __try {
            uintptr_t viewMatAddr = Globals::Read<uintptr_t>(
                Globals::base + Offset::Extra::ViewMatrixInst);
            if (!Globals::IsValidPtr(viewMatAddr)) return false;

            uintptr_t renderer = Globals::Read<uintptr_t>(
                Globals::base + Offset::Global::r3dRenderer);
            if (!Globals::IsValidPtr(renderer)) return false;

            int width  = Globals::Read<int>(renderer + 0x10);
            int height = Globals::Read<int>(renderer + 0x14);

            float matrix[16];
            for (int i = 0; i < 16; i++)
                matrix[i] = Globals::Read<float>(viewMatAddr + i * 4);

            float clipX = world.x * matrix[0] + world.y * matrix[4] + world.z * matrix[8]  + matrix[12];
            float clipY = world.x * matrix[1] + world.y * matrix[5] + world.z * matrix[9]  + matrix[13];
            float clipW = world.x * matrix[3] + world.y * matrix[7] + world.z * matrix[11] + matrix[15];

            if (clipW < 0.1f) return false;

            float ndcX = clipX / clipW;
            float ndcY = clipY / clipW;

            screen.x = (width  / 2.0f) * (1.0f + ndcX);
            screen.y = (height / 2.0f) * (1.0f - ndcY);
            return true;
        } __except(1) { return false; }
    }

    // ====================================================================
    // Drawing primitives (ImGui overlay)
    // ====================================================================

    // Draw circle in world space
    inline void DrawCircle(const Vec3& center, float radius, ImU32 color,
                           float thickness = 1.5f, int segments = 50) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        Vec2 prev;
        bool hasPrev = false;

        for (int i = 0; i <= segments; i++) {
            float angle = (float)i / (float)segments * 2.0f * 3.14159265f;
            Vec3 worldPt;
            worldPt.x = center.x + radius * cosf(angle);
            worldPt.y = center.y;
            worldPt.z = center.z + radius * sinf(angle);

            Vec2 screenPt;
            if (WorldToScreen(worldPt, screenPt)) {
                if (hasPrev) {
                    draw->AddLine(ImVec2(prev.x, prev.y),
                                  ImVec2(screenPt.x, screenPt.y),
                                  color, thickness);
                }
                prev = screenPt;
                hasPrev = true;
            } else {
                hasPrev = false;
            }
        }
    }

    // Draw line in world space
    inline void DrawLine(const Vec3& start, const Vec3& end,
                         ImU32 color, float thickness = 1.5f) {
        Vec2 s, e;
        if (WorldToScreen(start, s) && WorldToScreen(end, e)) {
            ImDrawList* draw = ImGui::GetBackgroundDrawList();
            if (draw) draw->AddLine(ImVec2(s.x, s.y), ImVec2(e.x, e.y), color, thickness);
        }
    }

    // Draw text at world position
    inline void DrawText(const Vec3& worldPos, const char* text,
                         ImU32 color = IM_COL32(255, 255, 255, 255)) {
        Vec2 screen;
        if (WorldToScreen(worldPos, screen)) {
            ImDrawList* draw = ImGui::GetBackgroundDrawList();
            if (draw) draw->AddText(ImVec2(screen.x, screen.y), color, text);
        }
    }

    // Draw text (centered) at world position
    inline void DrawTextCentered(const Vec3& worldPos, const char* text,
                                  ImU32 color = IM_COL32(255, 255, 255, 255)) {
        Vec2 screen;
        if (WorldToScreen(worldPos, screen)) {
            ImVec2 textSize = ImGui::CalcTextSize(text);
            screen.x -= textSize.x / 2.0f;
            ImDrawList* draw = ImGui::GetBackgroundDrawList();
            if (draw) draw->AddText(ImVec2(screen.x, screen.y), color, text);
        }
    }

    // Draw HP bar above object
    inline void DrawHealthBar(const Vec3& worldPos, float health, float maxHealth,
                               float width = 80.0f, float height = 6.0f) {
        Vec2 screen;
        if (!WorldToScreen(worldPos, screen)) return;
        if (maxHealth <= 0) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        float pct = health / maxHealth;
        if (pct < 0) pct = 0;
        if (pct > 1) pct = 1;

        float x = screen.x - width / 2.0f;
        float y = screen.y - 20.0f;

        // Background
        draw->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height),
                           IM_COL32(0, 0, 0, 180));
        // Fill
        ImU32 fillColor = pct > 0.5f ? IM_COL32(0, 200, 0, 255) :
                          pct > 0.25f ? IM_COL32(255, 200, 0, 255) :
                                        IM_COL32(255, 50, 50, 255);
        draw->AddRectFilled(ImVec2(x, y), ImVec2(x + width * pct, y + height), fillColor);
        // Border
        draw->AddRect(ImVec2(x, y), ImVec2(x + width, y + height),
                     IM_COL32(255, 255, 255, 100));
    }

    // ====================================================================
    // Common drawing helpers for game overlay
    // ====================================================================

    // Draw range circle around player
    inline void DrawPlayerRange(float range, ImU32 color = IM_COL32(255, 255, 255, 100)) {
        auto& player = GameObjects::Player;
        if (player.IsValid()) {
            DrawCircle(player.GetPosition(), range, color);
        }
    }

    // Draw attack range circle
    inline void DrawAttackRange(ImU32 color = IM_COL32(0, 255, 0, 80)) {
        auto& player = GameObjects::Player;
        if (player.IsValid()) {
            DrawCircle(player.GetPosition(), player.GetRealAttackRange(), color);
        }
    }

} // namespace Drawing
} // namespace SDK
