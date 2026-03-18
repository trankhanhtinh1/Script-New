#pragma once
// ============================================================================
// CursorUtils.h — Cursor Utility (EnsoulSharp SDK Port)
// ============================================================================
// Features:
//   1. FakeClick — uses game's native CursorMoveTo/CursorMoveToRed VFX
//   2. FakeCursor — draws game cursor on overlay for livestream protection
//   3. Screen/World position helpers
//   4. IsOverHUD detection
//
// IDA Analysis:
//   CursorSystem global = HudInstance (0x1DA1628) — same pointer
//   CursorSystem+48 = green click object (CursorMoveTo particle)
//   CursorSystem+56 = red click object (CursorMoveToRed particle)
//   ShowClick vtable[1] signature:
//     void ShowClick(obj, float height, float worldX, float worldZ, float a5, float a6)
//   height > 0 = show, height <= 0 = hide
// ============================================================================

#include "core/Vector.h"
#include "core/Offsets.h"
#include "core/Globals.h"
#include "Game.h"
#include "Drawing.h"

#include <cmath>
#include <Windows.h>

namespace SDK {

// ============================================================================
// Native FakeClick — calls game's actual cursor VFX system
// ============================================================================
// IDA-verified vtable signatures (from binary analysis):
//   Green (CursorMoveTo)    vtable[1]: void(obj, height, worldX, worldZ, alphaS, alphaE) — 6 params
//   Red   (CursorMoveToRed) vtable[1]: void(obj, worldX, worldZ) — 3 params, auto-sets active+time
// ============================================================================
class NativeClick {
public:
    enum ClickType : int {
        Move   = 0,    // Green cursor (CursorMoveTo)
        Attack = 1,    // Red cursor (CursorMoveToRed)
    };

    // Show green click at world position (CursorMoveTo particle)
    static bool ShowMoveClick(const Vec3& worldPos) {
        uintptr_t base = Globals::base;
        if (!base) return false;

        uintptr_t cursorSystemPtr = *(uintptr_t*)(base + Offset::Global::HudInstance);
        if (!cursorSystemPtr) return false;

        // Green click object at CursorSystem + 48
        uintptr_t clickObj = *(uintptr_t*)(cursorSystemPtr + 48);
        if (!clickObj) return false;

        uintptr_t vtable = *(uintptr_t*)clickObj;
        if (!vtable) return false;

        // Green vtable[1]: void __fastcall (obj, float height, float worldX, float worldZ, float a5, float a6)
        // height > 0 = show, height <= 0 = hide
        typedef void(__fastcall *GreenClickFn)(uintptr_t, float, float, float, float, float);
        GreenClickFn fn = (GreenClickFn)(*(uintptr_t*)(vtable + 8));
        if (!fn) return false;

        fn(clickObj, 50.0f, worldPos.x, worldPos.z, 0.0f, 0.0f);
        return true;
    }

    // Show red click at world position (CursorMoveToRed particle)
    static bool ShowAttackClick(const Vec3& worldPos) {
        uintptr_t base = Globals::base;
        if (!base) return false;

        uintptr_t cursorSystemPtr = *(uintptr_t*)(base + Offset::Global::HudInstance);
        if (!cursorSystemPtr) return false;

        // Red click object at CursorSystem + 56
        uintptr_t clickObj = *(uintptr_t*)(cursorSystemPtr + 56);
        if (!clickObj) return false;

        uintptr_t vtable = *(uintptr_t*)clickObj;
        if (!vtable) return false;

        // Red vtable[1]: void __fastcall (obj, float worldX, float worldZ)
        // Auto-sets active=1, gameTime, scale
        typedef void(__fastcall *RedClickFn)(uintptr_t, float, float);
        RedClickFn fn = (RedClickFn)(*(uintptr_t*)(vtable + 8));
        if (!fn) return false;

        fn(clickObj, worldPos.x, worldPos.z);
        return true;
    }

    // Hide green click indicator
    static void HideMoveClick() {
        uintptr_t base = Globals::base;
        if (!base) return;
        uintptr_t cursorSystemPtr = *(uintptr_t*)(base + Offset::Global::HudInstance);
        if (!cursorSystemPtr) return;
        uintptr_t clickObj = *(uintptr_t*)(cursorSystemPtr + 48);
        if (!clickObj) return;
        uintptr_t vtable = *(uintptr_t*)clickObj;
        if (!vtable) return;
        typedef void(__fastcall *GreenClickFn)(uintptr_t, float, float, float, float, float);
        GreenClickFn fn = (GreenClickFn)(*(uintptr_t*)(vtable + 8));
        if (!fn) return;
        // height <= 0 = hide
        fn(clickObj, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    }
};

// ============================================================================
// FakeCursor — Draw game cursor on overlay for livestream protection
// ============================================================================
// Strategy:
//   - Read game's mouse position from memory (CursorInstance)
//   - Draw a cursor image on overlay at that screen position
//   - Game cursor can be hidden via SetCursor(NULL) or ClipCursor
//   - Viewer sees normal cursor on stream, but real cursor is elsewhere
//
// Usage:
//   - Call FakeCursor::OnRender() in your ImGui render loop
//   - Call FakeCursor::SetEnabled(true) to activate
//   - The overlay cursor follows the game's internal mouse position
//   - Real mouse is invisible (handled by SetCursor hook)
class FakeCursor {
public:
    static inline bool Enabled = false;
    static inline bool HideRealCursor = false;  // If true, hide the hardware cursor

    static void SetEnabled(bool enabled) {
        Enabled = enabled;
        if (!enabled && HideRealCursor) {
            // Restore real cursor
            ShowCursor(TRUE);
            HideRealCursor = false;
        }
    }

    // Call in render loop — draws fake cursor on overlay
    static void OnRender() {
        if (!Enabled) return;

        // Get game's internal mouse screen position
        Vec2 mouseScreen = GetGameMouseScreenPos();
        if (mouseScreen.x <= 0.0f && mouseScreen.y <= 0.0f) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        if (!dl) return;

        // Draw a cursor-like triangle (simple but effective)
        // Main cursor body (white fill + black outline)
        ImVec2 base(mouseScreen.x, mouseScreen.y);
        
        // Standard cursor shape: triangle pointing down-right
        ImVec2 p1(base.x, base.y);             // Tip
        ImVec2 p2(base.x, base.y + 18.0f);     // Bottom-left
        ImVec2 p3(base.x + 12.0f, base.y + 14.0f); // Bottom-right

        // Black outline
        dl->AddTriangleFilled(p1, p2, p3, IM_COL32(0, 0, 0, 255));
        // White fill (slightly smaller)
        ImVec2 p1i(base.x + 1.5f, base.y + 2.0f);
        ImVec2 p2i(base.x + 1.5f, base.y + 16.0f);
        ImVec2 p3i(base.x + 10.5f, base.y + 13.0f);
        dl->AddTriangleFilled(p1i, p2i, p3i, IM_COL32(255, 255, 255, 255));

        // Cursor tail/stem
        ImVec2 t1(base.x + 3.0f, base.y + 14.0f);
        ImVec2 t2(base.x + 5.0f, base.y + 12.0f);
        ImVec2 t3(base.x + 8.0f, base.y + 20.0f);
        ImVec2 t4(base.x + 6.0f, base.y + 21.0f);
        dl->AddQuadFilled(t1, t2, t3, t4, IM_COL32(0, 0, 0, 255));
        ImVec2 t1i(base.x + 3.5f, base.y + 14.5f);
        ImVec2 t2i(base.x + 4.8f, base.y + 12.8f);
        ImVec2 t3i(base.x + 7.5f, base.y + 19.5f);
        ImVec2 t4i(base.x + 6.2f, base.y + 20.2f);
        dl->AddQuadFilled(t1i, t2i, t3i, t4i, IM_COL32(255, 255, 255, 255));
    }

    // Hide the real hardware cursor (call once when enabling)
    static void HideHardwareCursor() {
        if (!HideRealCursor) {
            // Hide by calling ShowCursor(FALSE) until count < 0
            while (ShowCursor(FALSE) >= 0) {}
            HideRealCursor = true;
        }
    }

    // Restore real hardware cursor
    static void ShowHardwareCursor() {
        if (HideRealCursor) {
            while (ShowCursor(TRUE) < 0) {}
            HideRealCursor = false;
        }
    }

    // Get game's internal mouse screen position from memory
    static Vec2 GetGameMouseScreenPos() {
        uintptr_t base = Globals::base;
        if (!base) return Vec2(0, 0);

        // Read from MouseScreenVec2 global (screen coordinates)
        uintptr_t mousePtr = *(uintptr_t*)(base + Offset::Global::MouseScreenVec2);
        if (!mousePtr) return Vec2(0, 0);

        // Mouse screen X = mousePtr + 0xC, Y = mousePtr + 0x10
        float mx = *(float*)(mousePtr + 0xC);
        float my = *(float*)(mousePtr + 0x10);
        return Vec2(mx, my);
    }
};

// ============================================================================
// CursorUtils — Combined utility (backward compatible)
// ============================================================================
class CursorUtils {
public:

    // ---- Update from WndProc (call in WndProc hook) ----
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
    static bool IsOverHUD() {
        Vec2 realPos = GetScreenPosition();
        Vec2 gamePos = GetGameScreenPosition();
        float dx = fabsf(realPos.x - gamePos.x);
        float dy = fabsf(realPos.y - gamePos.y);
        return (dx > 10.0f || dy > 10.0f);
    }

    // ---- Last click info ----
    static float GetLastClickTime() { return s_lastClickTime; }
    static Vec2  GetLastClickScreenPos() { return s_lastClickScreenPos; }
    static Vec3  GetLastClickWorldPos() { return s_lastClickWorldPos; }

    // ================================================================
    // FakeClick — Native game click (recommended)
    // ================================================================
    
    // Show green click at world position (game's native VFX)
    static bool ShowMoveClick(const Vec3& worldPos) {
        s_lastClickTime = Game::GetTime();
        s_lastClickWorldPos = worldPos;
        return NativeClick::ShowMoveClick(worldPos);
    }

    // Show red click at world position (game's native VFX)
    static bool ShowAttackClick(const Vec3& worldPos) {
        s_lastClickTime = Game::GetTime();
        s_lastClickWorldPos = worldPos;
        return NativeClick::ShowAttackClick(worldPos);
    }

    // ================================================================
    // FakeClick — Overlay fallback (if native doesn't work)
    // ================================================================

    // Draw overlay fake click circle (ImGui-based fallback)
    static void DrawFakeClick(float duration = 0.3f, unsigned int color = 0xFF00FF00) {
        float now = Game::GetTime();
        float elapsed = now - s_lastClickTime;
        if (elapsed > duration || elapsed < 0.0f) return;

        float progress = elapsed / duration;
        float radius = 30.0f * (1.0f - progress);
        float alpha = 1.0f - progress;

        Vec2 screenPos;
        if (Drawing::WorldToScreen(s_lastClickWorldPos, screenPos)) {
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

private:
    static inline int s_screenX = 0;
    static inline int s_screenY = 0;
    static inline float s_lastClickTime = 0.0f;
    static inline Vec2 s_lastClickScreenPos;
    static inline Vec3 s_lastClickWorldPos;
};

} // namespace SDK
