#pragma once
/*
 * ============================================================================
 *  OverlayWindow.h — Transparent overlay window for NightSharp
 *
 *  Creates a transparent, topmost window that sits on top of the game.
 *  Uses DWM composition for per-pixel alpha transparency.
 *  Supports click-through toggle for menu interaction.
 * ============================================================================
 */

#include <Windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

namespace Overlay {

// Forward declare WndProc (defined after class)
LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class Window {
public:
    HWND    m_hwnd       = nullptr;
    HWND    m_targetHwnd = nullptr;
    bool    m_running    = true;
    UINT    m_resizeW    = 0;
    UINT    m_resizeH    = 0;

    // ================================================================
    // Find the game window (we're injected — same PID)
    // ================================================================
    bool FindTarget() {
        struct FindData { DWORD pid; HWND result; };
        FindData data = { GetCurrentProcessId(), nullptr };

        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            auto* d = (FindData*)lParam;
            DWORD pid;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid != d->pid) return TRUE;

            // Must be visible, not minimized, has size
            if (!IsWindowVisible(hwnd)) return TRUE;

            RECT r;
            GetClientRect(hwnd, &r);
            if (r.right - r.left < 100 || r.bottom - r.top < 100) return TRUE;

            // Skip tool windows (our overlay will have WS_EX_TOOLWINDOW)
            LONG exStyle = GetWindowLongA(hwnd, GWL_EXSTYLE);
            if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

            d->result = hwnd;
            return FALSE;
        }, (LPARAM)&data);

        m_targetHwnd = data.result;
        return m_targetHwnd != nullptr;
    }

    // ================================================================
    // Create the transparent overlay window
    // ================================================================
    bool Create() {
        WNDCLASSEXA wc = {};
        wc.cbSize        = sizeof(WNDCLASSEXA);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = OverlayWndProc;
        wc.hInstance      = GetModuleHandleA(nullptr);
        wc.hCursor        = LoadCursorA(nullptr, IDC_ARROW);
        wc.hbrBackground  = (HBRUSH)CreateSolidBrush(RGB(0, 0, 0));
        wc.lpszClassName  = "NSOvl";

        if (!RegisterClassExA(&wc)) return false;

        // Get target window bounds
        RECT targetRect = {};
        if (m_targetHwnd) {
            GetWindowRect(m_targetHwnd, &targetRect);
        } else {
            targetRect.right  = GetSystemMetrics(SM_CXSCREEN);
            targetRect.bottom = GetSystemMetrics(SM_CYSCREEN);
        }

        int w = targetRect.right - targetRect.left;
        int h = targetRect.bottom - targetRect.top;

        m_hwnd = CreateWindowExA(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE |
            WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            "NSOvl",
            "",  // No window title (stealth)
            WS_POPUP,
            targetRect.left, targetRect.top, w, h,
            nullptr, nullptr,
            wc.hInstance,
            nullptr
        );

        if (!m_hwnd) return false;

        // Enable per-pixel alpha via DWM
        SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);
        MARGINS margin = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(m_hwnd, &margin);

        ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
        UpdateWindow(m_hwnd);

        return true;
    }

    // ================================================================
    // Follow the game window (position + size sync)
    // ================================================================
    void FollowTarget() {
        if (!m_targetHwnd || !IsWindow(m_targetHwnd)) {
            m_running = false;
            return;
        }

        RECT r;
        GetWindowRect(m_targetHwnd, &r);
        int w = r.right - r.left;
        int h = r.bottom - r.top;

        // Check if game window is minimized
        if (IsIconic(m_targetHwnd)) {
            ShowWindow(m_hwnd, SW_HIDE);
            return;
        }

        // Show overlay if hidden
        if (!IsWindowVisible(m_hwnd)) {
            ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
        }

        // Move overlay to match game window
        SetWindowPos(m_hwnd, HWND_TOPMOST,
                     r.left, r.top, w, h,
                     SWP_NOACTIVATE);
    }

    // ================================================================
    // Toggle click-through (for menu interaction)
    // ================================================================
    void SetClickThrough(bool through) {
        LONG ex = GetWindowLongA(m_hwnd, GWL_EXSTYLE);
        if (through)
            ex |= WS_EX_TRANSPARENT;
        else
            ex &= ~WS_EX_TRANSPARENT;
        SetWindowLongA(m_hwnd, GWL_EXSTYLE, ex);
    }

    // ================================================================
    // Check if game window is focused
    // ================================================================
    bool IsTargetFocused() {
        HWND fg = GetForegroundWindow();
        return (fg == m_targetHwnd || fg == m_hwnd);
    }

    // ================================================================
    // Anti-capture: hide overlay from OBS / screenshots / screen recording
    // Uses SetWindowDisplayAffinity (Windows 10 2004+)
    // WDA_EXCLUDEFROMCAPTURE = 0x11 — window is invisible to capture APIs
    // ================================================================
    void SetAntiCapture(bool enabled) {
        if (!m_hwnd) return;
        if (enabled == m_antiCapture) return; // no change

        // WDA_EXCLUDEFROMCAPTURE = 0x00000011 (Win10 2004+)
        // WDA_NONE = 0x00000000
        const DWORD affinity = enabled ? 0x00000011 : 0x00000000;
        if (SetWindowDisplayAffinity(m_hwnd, affinity)) {
            m_antiCapture = enabled;
        }
    }

    bool m_antiCapture = false;

    // ================================================================
    // Process window messages (non-blocking)
    // ================================================================
    bool PumpMessages() {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            if (msg.message == WM_QUIT) {
                m_running = false;
                return false;
            }
        }
        return true;
    }

    // ================================================================
    // Cleanup
    // ================================================================
    void Destroy() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
        UnregisterClassA("NSOvl", GetModuleHandleA(nullptr));
    }
};

// ================================================================
// Global pointer for WndProc access
// ================================================================
inline Window* g_overlayWindow = nullptr;

} // namespace Overlay

// Forward declare (C++ linkage — matches imgui_impl_win32.cpp)
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

inline LRESULT CALLBACK Overlay::OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (Overlay::g_overlayWindow && wParam != SIZE_MINIMIZED) {
            Overlay::g_overlayWindow->m_resizeW = LOWORD(lParam);
            Overlay::g_overlayWindow->m_resizeH = HIWORD(lParam);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}
