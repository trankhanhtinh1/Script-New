#pragma once
// ============================================================================
// CursorUtils.h — Cursor Utility (EnsoulSharp SDK Port)
// ============================================================================
// Port of EnsoulSharp.SDK/Core/Utils/Cursor.cs
// Provides:
//   - Screen position of cursor (from WndProc WM_MOUSEMOVE)
//   - Game world position (from Game::GetMouseWorldPos)
//   - IsOverHUD — detect if cursor is over game HUD elements
//   - Fake click visualization for Orbwalker
// ============================================================================

#include "core/Vector.h"
#include "Game.h"
#include "Drawing.h"

#include <cmath>
#include <Windows.h>

namespace SDK {

class CursorUtils {
public:

    // ---- Update from WndProc (call in WndProc hook) ----
    // msg = WM_MOUSEMOVE (0x0200) → update screen position
    static void OnWndProc(UINT msg, LPARAM lParam) {
        if (msg == 0x0200) { // WM_MOUSEMOVE
            s_screenX = (short)(lParam & 0xFFFF);
            s_screenY = (short)((lParam >> 16) & 0xFFFF);
        }
        else if (msg == 0x0201) { // WM_LBUTTONDOWN
            s_lastClickTime = Game::GetTime();
            s_lastClickScreenPos = Vec2((float)s_screenX, (float)s_screenY);
            s_lastClickWorldPos = Game::GetMouseWorldPos();
        }
    }

    // ---- Current cursor screen position (pixels) ----
    static Vec2 GetScreenPosition() {
        return Vec2((float)s_screenX, (float)s_screenY);
    }

    // ---- Current cursor game world position ----
    static Vec3 GetWorldPosition() {
        return Game::GetMouseWorldPos();
    }

    // ---- Game cursor projected to screen (via W2S) ----
    static Vec2 GetGameScreenPosition() {
        Vec3 worldPos = Game::GetMouseWorldPos();
        Vec2 screenPos;
        if (Drawing::WorldToScreen(worldPos, screenPos))
            return screenPos;
        return Vec2((float)s_screenX, (float)s_screenY);
    }

    // ---- Is cursor over HUD? ----
    // Compares real cursor screen pos with game cursor W2S pos.
    // If they differ significantly, cursor is over a HUD element.
    static bool IsOverHUD() {
        Vec2 realPos = GetScreenPosition();
        Vec2 gamePos = GetGameScreenPosition();

        float dx = fabsf(realPos.x - gamePos.x);
        float dy = fabsf(realPos.y - gamePos.y);

        // Threshold: if more than 10px apart, cursor is over HUD
        return (dx > 10.0f || dy > 10.0f);
    }

    // ---- Last click info ----
    static float GetLastClickTime() { return s_lastClickTime; }
    static Vec2  GetLastClickScreenPos() { return s_lastClickScreenPos; }
    static Vec3  GetLastClickWorldPos() { return s_lastClickWorldPos; }

    // ---- Fake Click Visualization ----
    // Call in render loop to draw a shrinking circle at the last click position
    static void DrawFakeClick(float duration = 0.3f, unsigned int color = 0xFF00FF00) {
        float now = Game::GetTime();
        float elapsed = now - s_lastClickTime;
        if (elapsed > duration || elapsed < 0.0f) return;

        float progress = elapsed / duration; // 0 → 1
        float radius = 30.0f * (1.0f - progress);
        float alpha = 1.0f - progress;

        Vec2 screenPos;
        if (Drawing::WorldToScreen(s_lastClickWorldPos, screenPos)) {
            // Decompose color and apply fading alpha
            unsigned int r = (color >> 16) & 0xFF;
            unsigned int g = (color >> 8) & 0xFF;
            unsigned int b = color & 0xFF;
            unsigned int a = (unsigned int)(alpha * 255.0f);

            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            if (dl) {
                dl->AddCircle(
                    ImVec2(screenPos.x, screenPos.y),
                    radius,
                    IM_COL32(r, g, b, a),
                    12, 2.0f
                );
            }
        }
    }

    // ---- Move command visualization ----
    // Draw a green click at a specific world position (for Orbwalker)
    static void ShowMoveClick(const Vec3& worldPos, unsigned int color = 0xFF00FF00) {
        s_lastClickTime = Game::GetTime();
        s_lastClickWorldPos = worldPos;

        Vec2 sp;
        if (Drawing::WorldToScreen(worldPos, sp))
            s_lastClickScreenPos = sp;
    }

    // ---- Attack command visualization ----
    // Draw a red click at target position (for Orbwalker)
    static void ShowAttackClick(const Vec3& worldPos) {
        ShowMoveClick(worldPos, 0xFFFF0000);
    }

private:
    static inline int s_screenX = 0;
    static inline int s_screenY = 0;
    static inline float s_lastClickTime = 0.0f;
    static inline Vec2 s_lastClickScreenPos;
    static inline Vec3 s_lastClickWorldPos;
};

} // namespace SDK
