#pragma once
#include "core/Globals.h"
#include "core/Offsets.h"
#include "core/Vector.h"
#include "imgui/imgui.h"
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

    // ====================================================================
    // DrawLine3D — Draw a line with thickness in world space
    // Simulates thickness by drawing multiple parallel lines
    // ====================================================================
    inline void DrawLine3D(const Vec3& start, const Vec3& end,
                           ImU32 color, float thickness = 2.0f) {
        Vec2 s, e;
        if (WorldToScreen(start, s) && WorldToScreen(end, e)) {
            ImDrawList* draw = ImGui::GetBackgroundDrawList();
            if (draw) draw->AddLine(ImVec2(s.x, s.y), ImVec2(e.x, e.y), color, thickness);
        }
    }

    // ====================================================================
    // DrawPolygon — Render a Polygon in world space
    // Uses polygon edges (Points array) and projects each to screen
    // ====================================================================
    inline void DrawPolygon(const std::vector<Vec2>& points, float y,
                            ImU32 borderColor, float thickness = 1.5f,
                            ImU32 fillColor = 0) {
        if (points.size() < 2) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        // Collect screen points
        std::vector<ImVec2> screenPts;
        screenPts.reserve(points.size());
        for (auto& pt : points) {
            Vec2 sp;
            Vec3 wp(pt.x, y, pt.y);
            if (WorldToScreen(wp, sp)) {
                screenPts.push_back(ImVec2(sp.x, sp.y));
            }
        }

        if (screenPts.size() < 2) return;

        // Fill (if requested)
        if ((fillColor & 0xFF000000) != 0 && screenPts.size() >= 3) {
            draw->AddConvexPolyFilled(screenPts.data(), (int)screenPts.size(), fillColor);
        }

        // Border
        for (size_t i = 0; i < screenPts.size(); i++) {
            size_t next = (i + 1) % screenPts.size();
            draw->AddLine(screenPts[i], screenPts[next], borderColor, thickness);
        }
    }

    // Overload: draw from Vec3 points directly
    inline void DrawPolygon3D(const std::vector<Vec3>& points,
                              ImU32 borderColor, float thickness = 1.5f,
                              ImU32 fillColor = 0) {
        if (points.size() < 2) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        std::vector<ImVec2> screenPts;
        screenPts.reserve(points.size());
        for (auto& pt : points) {
            Vec2 sp;
            if (WorldToScreen(pt, sp)) {
                screenPts.push_back(ImVec2(sp.x, sp.y));
            }
        }

        if (screenPts.size() < 2) return;

        if ((fillColor & 0xFF000000) != 0 && screenPts.size() >= 3) {
            draw->AddConvexPolyFilled(screenPts.data(), (int)screenPts.size(), fillColor);
        }

        for (size_t i = 0; i < screenPts.size(); i++) {
            size_t next = (i + 1) % screenPts.size();
            draw->AddLine(screenPts[i], screenPts[next], borderColor, thickness);
        }
    }

    // ====================================================================
    // DrawDamageBar — Show damage overlay on the game's HP bar
    // Draws a colored section on the HP bar to indicate how much damage
    // a combo/spell would deal to the target.
    // ====================================================================
    inline void DrawDamageBar(const Vec3& worldPos, float currentHP, float maxHP,
                               float damage, ImU32 color = IM_COL32(255, 170, 0, 180),
                               float barWidth = 103.0f, float barHeight = 9.0f,
                               float yOffset = -15.0f) {
        Vec2 screen;
        if (!WorldToScreen(worldPos, screen)) return;
        if (maxHP <= 0) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        float hpPct = currentHP / maxHP;
        float dmgPct = damage / maxHP;

        if (hpPct < 0) hpPct = 0;
        if (hpPct > 1) hpPct = 1;
        if (dmgPct < 0) dmgPct = 0;

        // Position: centered at screen pos, slightly above
        float x = screen.x - barWidth / 2.0f;
        float y = screen.y + yOffset;

        float hpEnd = x + barWidth * hpPct;
        float dmgStart = hpEnd - barWidth * dmgPct;
        if (dmgStart < x) dmgStart = x;

        // Draw damage portion
        draw->AddRectFilled(
            ImVec2(dmgStart, y),
            ImVec2(hpEnd, y + barHeight),
            color);

        // Kill indicator (if damage >= currentHP)
        if (damage >= currentHP) {
            const char* killText = "KILL";
            ImVec2 textSize = ImGui::CalcTextSize(killText);
            draw->AddText(
                ImVec2(screen.x - textSize.x / 2.0f, y - textSize.y - 2.0f),
                IM_COL32(255, 50, 50, 255), killText);
        }
    }

    // Overload: use GameObject directly
    inline void DrawDamageBar(const GameObject& target, float damage,
                               ImU32 color = IM_COL32(255, 170, 0, 180)) {
        if (!target.IsValid() || !target.IsAlive()) return;
        Vec3 pos = target.GetPosition();
        pos.y += target.GetBoundingRadius() * 0.5f; // Slightly above
        DrawDamageBar(pos, target.GetHealth(), target.GetMaxHealth(), damage, color);
    }

    // ====================================================================
    // DrawMinimap — Draw on minimap at world position
    // ====================================================================
    inline void DrawMinimapCircle(const Vec3& worldPos, float radius,
                                   ImU32 color, float thickness = 2.0f) {
        // Minimap coords: Map world → minimap screen
        // Map bounds (Summoner's Rift): roughly (-120, -120) to (14870, 14870)
        // Minimap is in bottom-right corner of screen
        const float mapMinX = -120.0f, mapMinZ = -120.0f;
        const float mapMaxX = 14870.0f, mapMaxZ = 14870.0f;

        ImVec2 displaySize = ImGui::GetIO().DisplaySize;

        // Minimap position (bottom-right, approximately 260x260)
        float mmW = 260.0f;
        float mmH = 260.0f;
        float mmX = displaySize.x - mmW - 5.0f;
        float mmY = displaySize.y - mmH - 5.0f;

        // Map world pos → minimap screen pos
        float px = (worldPos.x - mapMinX) / (mapMaxX - mapMinX);
        float pz = 1.0f - (worldPos.z - mapMinZ) / (mapMaxZ - mapMinZ); // Y is inverted

        float screenX = mmX + px * mmW;
        float screenY = mmY + pz * mmH;

        // Scale radius to minimap
        float mmRadius = radius / (mapMaxX - mapMinX) * mmW;
        if (mmRadius < 2.0f) mmRadius = 2.0f;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (draw) {
            draw->AddCircle(ImVec2(screenX, screenY), mmRadius, color, 0, thickness);
        }
    }

    inline void DrawMinimapText(const Vec3& worldPos, const char* text,
                                 ImU32 color = IM_COL32(255, 255, 255, 255)) {
        const float mapMinX = -120.0f, mapMinZ = -120.0f;
        const float mapMaxX = 14870.0f, mapMaxZ = 14870.0f;

        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        float mmW = 260.0f, mmH = 260.0f;
        float mmX = displaySize.x - mmW - 5.0f;
        float mmY = displaySize.y - mmH - 5.0f;

        float px = (worldPos.x - mapMinX) / (mapMaxX - mapMinX);
        float pz = 1.0f - (worldPos.z - mapMinZ) / (mapMaxZ - mapMinZ);

        float screenX = mmX + px * mmW;
        float screenY = mmY + pz * mmH;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (draw) {
            ImVec2 textSize = ImGui::CalcTextSize(text);
            draw->AddText(ImVec2(screenX - textSize.x / 2.0f, screenY - textSize.y / 2.0f),
                         color, text);
        }
    }

    inline void DrawMinimapIcon(const Vec3& worldPos, ImU32 color,
                                 float size = 6.0f) {
        const float mapMinX = -120.0f, mapMinZ = -120.0f;
        const float mapMaxX = 14870.0f, mapMaxZ = 14870.0f;

        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        float mmW = 260.0f, mmH = 260.0f;
        float mmX = displaySize.x - mmW - 5.0f;
        float mmY = displaySize.y - mmH - 5.0f;

        float px = (worldPos.x - mapMinX) / (mapMaxX - mapMinX);
        float pz = 1.0f - (worldPos.z - mapMinZ) / (mapMaxZ - mapMinZ);

        float screenX = mmX + px * mmW;
        float screenY = mmY + pz * mmH;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (draw) {
            draw->AddCircleFilled(ImVec2(screenX, screenY), size, color);
            draw->AddCircle(ImVec2(screenX, screenY), size,
                           IM_COL32(0, 0, 0, 200), 0, 1.0f);
        }
    }

    // ====================================================================
    // DrawRectWorld — Draw a rectangle in world space
    // ====================================================================
    inline void DrawRectWorld(const Vec3& start, const Vec3& end,
                              float width, ImU32 color, float thickness = 1.5f) {
        // Calculate perpendicular vector
        Vec2 dir(end.x - start.x, end.z - start.z);
        float len = dir.Length();
        if (len < 0.001f) return;
        dir = dir * (1.0f / len);
        Vec2 perp(-dir.y, dir.x);
        float halfW = width / 2.0f;

        Vec3 p1(start.x + perp.x * halfW, start.y, start.z + perp.y * halfW);
        Vec3 p2(start.x - perp.x * halfW, start.y, start.z - perp.y * halfW);
        Vec3 p3(end.x - perp.x * halfW, end.y, end.z - perp.y * halfW);
        Vec3 p4(end.x + perp.x * halfW, end.y, end.z + perp.y * halfW);

        DrawLine3D(p1, p2, color, thickness);
        DrawLine3D(p2, p3, color, thickness);
        DrawLine3D(p3, p4, color, thickness);
        DrawLine3D(p4, p1, color, thickness);
    }

    // ====================================================================
    // DrawSector — Draw a cone/sector in world space
    // ====================================================================
    inline void DrawSector(const Vec3& center, float radius, float startAngle,
                            float endAngle, ImU32 color, float thickness = 1.5f,
                            int segments = 20) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        Vec2 screenCenter;
        if (!WorldToScreen(center, screenCenter)) return;

        // Draw the two radius lines
        Vec3 startPt(
            center.x + radius * cosf(startAngle),
            center.y,
            center.z + radius * sinf(startAngle));
        Vec3 endPt(
            center.x + radius * cosf(endAngle),
            center.y,
            center.z + radius * sinf(endAngle));

        DrawLine3D(center, startPt, color, thickness);
        DrawLine3D(center, endPt, color, thickness);

        // Draw the arc
        Vec2 prev;
        bool hasPrev = false;
        float angleRange = endAngle - startAngle;
        for (int i = 0; i <= segments; i++) {
            float angle = startAngle + angleRange * ((float)i / (float)segments);
            Vec3 wp(
                center.x + radius * cosf(angle),
                center.y,
                center.z + radius * sinf(angle));
            Vec2 sp;
            if (WorldToScreen(wp, sp)) {
                if (hasPrev) {
                    draw->AddLine(ImVec2(prev.x, prev.y), ImVec2(sp.x, sp.y),
                                 color, thickness);
                }
                prev = sp;
                hasPrev = true;
            } else {
                hasPrev = false;
            }
        }
    }

    // ====================================================================
    // Screen-space helpers (2D)
    // ====================================================================

    // Draw screen line
    inline void DrawScreenLine(const Vec2& a, const Vec2& b,
                                ImU32 color, float thickness = 1.0f) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (draw) draw->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), color, thickness);
    }

    // Draw screen circle
    inline void DrawScreenCircle(const Vec2& center, float radius,
                                  ImU32 color, float thickness = 1.0f) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (draw) draw->AddCircle(ImVec2(center.x, center.y), radius, color, 0, thickness);
    }

    // Draw screen filled circle
    inline void DrawScreenCircleFilled(const Vec2& center, float radius, ImU32 color) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (draw) draw->AddCircleFilled(ImVec2(center.x, center.y), radius, color);
    }

    // Draw screen rectangle
    inline void DrawScreenRect(const Vec2& topLeft, const Vec2& bottomRight,
                                ImU32 color, float thickness = 1.0f) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (draw) draw->AddRect(ImVec2(topLeft.x, topLeft.y),
                                ImVec2(bottomRight.x, bottomRight.y), color, 0.0f, 0, thickness);
    }

    // Draw screen filled rectangle
    inline void DrawScreenRectFilled(const Vec2& topLeft, const Vec2& bottomRight,
                                      ImU32 color, float rounding = 0.0f) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (draw) draw->AddRectFilled(ImVec2(topLeft.x, topLeft.y),
                                       ImVec2(bottomRight.x, bottomRight.y), color, rounding);
    }

    // Draw screen text
    inline void DrawScreenText(const Vec2& pos, const char* text,
                                ImU32 color = IM_COL32(255, 255, 255, 255)) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (draw) draw->AddText(ImVec2(pos.x, pos.y), color, text);
    }

    // Draw screen text centered
    inline void DrawScreenTextCentered(const Vec2& pos, const char* text,
                                        ImU32 color = IM_COL32(255, 255, 255, 255)) {
        ImVec2 textSize = ImGui::CalcTextSize(text);
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (draw) draw->AddText(ImVec2(pos.x - textSize.x / 2.0f, pos.y - textSize.y / 2.0f),
                                color, text);
    }

} // namespace Drawing
} // namespace SDK
