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
    // ViewProjMatrix cache (updated each frame)
    // Reference: Script-New-main/Render.cpp — matrix is DIRECT memory, not a pointer!
    // ====================================================================
    inline float g_viewProjMatrix[16] = {};
    inline bool  g_matrixReady = false;

    inline void UpdateMatrix() {
        if (!Globals::base) return;
        __try {
            // ViewMatrix at ViewMatrixInst, ProjMatrix at ViewMatrixInst + 0x40
            // These are DIRECT float arrays in memory, NOT pointers!
            float* pView = (float*)(Globals::base + Offset::Extra::ViewMatrixInst);
            float* pProj = (float*)(Globals::base + Offset::Extra::ViewMatrixInst + 0x40);

            // Multiply View * Proj → ViewProj
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    float sum = 0.0f;
                    for (int k = 0; k < 4; k++)
                        sum += pView[i * 4 + k] * pProj[k * 4 + j];
                    g_viewProjMatrix[i * 4 + j] = sum;
                }
            }
            g_matrixReady = true;
        } __except(1) { g_matrixReady = false; }
    }

    // ====================================================================
    // W2S via ViewProjMatrix (most reliable — no game function call)
    // Reference: Script-New-main/Render.cpp WorldToScreen
    // ====================================================================
    inline bool WorldToScreenMatrix(const Vec3& world, Vec2& screen) {
        if (!g_matrixReady) return false;

        float* m = g_viewProjMatrix;
        float w = m[3] * world.x + m[7] * world.y + m[11] * world.z + m[15];

        if (w < 0.01f) return false;

        float x = m[0] * world.x + m[4] * world.y + m[8]  * world.z + m[12];
        float y = m[1] * world.x + m[5] * world.y + m[9]  * world.z + m[13];

        // Use ImGui display size for screen dimensions
        ImVec2 displaySize = ImGui::GetIO().DisplaySize;

        screen.x = (displaySize.x / 2.0f) * (1.0f + x / w);
        screen.y = (displaySize.y / 2.0f) * (1.0f - y / w);
        return true;
    }

    // ====================================================================
    // World to Screen (tries matrix first, fallback to game function)
    // ====================================================================
    inline bool WorldToScreen(const Vec3& world, Vec2& screen) {
        // Try matrix-based first (no game function call)
        if (WorldToScreenMatrix(world, screen))
            return true;

        // Fallback: game function W2S
        if (!Globals::base) return false;
        uintptr_t fnAddr = Globals::base + Offset::Function::WorldToScreen;
        if (!fnAddr) return false;

        typedef bool(__cdecl* fnW2S)(Vec3*, Vec3*);
        Vec3 screenPos;
        Vec3 worldCopy = world;
        __try {
            bool result = ((fnW2S)fnAddr)(&worldCopy, &screenPos);
            if (result) {
                screen.x = screenPos.x;
                screen.y = screenPos.y;
                return true;
            }
        } __except(1) {}
        return false;
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
