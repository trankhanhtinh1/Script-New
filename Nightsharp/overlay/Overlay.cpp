#include "Overlay.h"

// Phase 2 finalization (Apr 26/2026): legacy_stubs.h retired. Real headers
// pulled directly. SDK::Bootstrap re-enabled May/2026 to register Orbwalker
// and TargetSelector as primary sidebar categories.
#include "../core/CrashTelemetry.h"
#include "../core/CoreAPI.h"
#include "../crt_shim.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx11.h"
#include "../imgui/imgui_impl_win32.h"
#include "../menu/NightSharpMenu.h"
#include "../menu/MenuConfig.h"
#include "../menu/PluginHostBridge.h"
#include "../menu/PluginRegistry.h"
#include "../plugins/PluginBootstrap.h"
#include "../plugins/PluginManager.h"
#include "../menu/MenuUI.h"
#include "../core/CoreEventHook.h"
#include "../core/CoreZoomHack.h"
#include "../core/CoreSkinChanger.h"
#include "../sdk/SDK.h"

#include <d3d11.h>
#include <dxgi.h>

// Telemetry flag-page accessor exported by dllmain.cpp. Offset 18 carries
// the "please shut down" request written by the external injector on
// Ctrl+R; we poll it each frame so a reload initiated from the injector
// triggers the same clean teardown as pressing VK_END in-game.
extern "C" volatile uint8_t* NightSharp_GetFlagPage();

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

HWND g_hOverlay = nullptr;
HWND g_hGameWindow = nullptr;
UINT g_ResizeW = 0;
UINT g_ResizeH = 0;

volatile LONG g_bRunning = 0;
volatile LONG g_bShutdown = 0;
volatile LONG g_bMenuVisible = 1;
bool g_antiCaptureEnabled = false;

const wchar_t* OVERLAY_CLASS_BASE = L"NightSharpOverlay";
constexpr DWORD kTargetOverlayFrameMs = 8; // ~125 FPS cap when the frame is cheap.

void WriteStage(const char* msg) {
    OutputDebugStringA(msg);
    HANDLE hFile = CreateFileA("C:\\Users\\Public\\ns_stage.txt",
        FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hFile, msg, (DWORD)lstrlenA(msg), &written, nullptr);
        CloseHandle(hFile);
    }
}

void WriteStageFmt(const char* format, ...) {
    char buffer[512] = {};
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format ? format : "", args);
    va_end(args);
    WriteStage(buffer);
}

void MarkFramePerf(DWORD& lastTick, DWORD& slowMs, const char*& slowStage, const char* completedStage) {
    const DWORD now = GetTickCount();
    const DWORD elapsed = now - lastTick;
    if (elapsed > slowMs) {
        slowMs = elapsed;
        slowStage = completedStage ? completedStage : "unknown";
    }
    lastTick = now;
}

void LogSlowFrame(int frameCount, DWORD frameStart, DWORD slowMs, const char* slowStage) {
    // Enabled May/2026 to diagnose the overlay-FPS regression. Logs at most
    // once per second and only when the frame exceeded ~30 ms total OR a
    // single stage took >20 ms — so steady-state 60 FPS output stays silent.
    constexpr bool kEnableSlowFrameLog = false;
    if (!kEnableSlowFrameLog) {
        return;
    }

    const DWORD now = GetTickCount();
    const DWORD total = now - frameStart;
    static DWORD s_lastPerfLogTick = 0;
    if ((total < 30 && slowMs < 20) || now - s_lastPerfLogTick < 1000) {
        return;
    }

    s_lastPerfLogTick = now;
    WriteStageFmt(
        "[NightSharp][Perf] slow frame=%d total=%lu ms slowStage=%s slow=%lu ms\r\n",
        frameCount,
        static_cast<unsigned long>(total),
        slowStage ? slowStage : "unknown",
        static_cast<unsigned long>(slowMs));
}

bool CreateRenderTarget() {
    if (!g_pSwapChain || !g_pd3dDevice) {
        return false;
    }

    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (FAILED(hr) || !pBackBuffer) {
        return false;
    }

    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();
    return SUCCEEDED(hr) && g_pRenderTargetView != nullptr;
}

void CleanupRenderTarget() {
    if (g_pRenderTargetView) {
        g_pRenderTargetView->Release();
        g_pRenderTargetView = nullptr;
    }
}

bool CreateDeviceD3D11(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 0;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        levels,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &g_pSwapChain,
        &g_pd3dDevice,
        &featureLevel,
        &g_pd3dContext);

    if (FAILED(hr)) {
        return false;
    }

    return CreateRenderTarget();
}

void CleanupDeviceD3D11() {
    CleanupRenderTarget();
    if (g_pSwapChain) {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dContext) {
        g_pd3dContext->Release();
        g_pd3dContext = nullptr;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

LRESULT WINAPI OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_bMenuVisible) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
            return TRUE;
        }
    }

    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            g_ResizeW = (UINT)LOWORD(lParam);
            g_ResizeH = (UINT)HIWORD(lParam);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

HWND FindGameWindow() {
    HWND hw = FindWindowA("RiotWindowClass", nullptr);
    if (hw) {
        return hw;
    }

    struct EnumData {
        DWORD pid;
        HWND result;
    } data = { GetCurrentProcessId(), nullptr };

    EnumWindows([](HWND hWnd, LPARAM lParam) -> BOOL {
        auto* d = reinterpret_cast<EnumData*>(lParam);
        DWORD pid = 0;
        GetWindowThreadProcessId(hWnd, &pid);
        if (pid != d->pid) return TRUE;
        if (!IsWindowVisible(hWnd)) return TRUE;
        if ((GetWindowLongPtrW(hWnd, GWL_STYLE) & WS_CHILD) != 0) return TRUE;

        RECT rc = {};
        GetClientRect(hWnd, &rc);
        if (rc.right == 0 || rc.bottom == 0) return TRUE;

        d->result = hWnd;
        return FALSE;
    }, (LPARAM)&data);

    return data.result;
}

bool IsGameWindowAlive() {
    if (!g_hGameWindow || !IsWindow(g_hGameWindow)) {
        return false;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(g_hGameWindow, &pid);
    return pid == GetCurrentProcessId();
}

// Re-position the overlay to sit directly on top of the game client area.
//
// IMPORTANT: we use HWND_TOP (not HWND_TOPMOST) because the overlay is
// created as a POPUP window *owned* by the game HWND — see the
// `CreateWindowExW` call below. With that ownership relationship, Windows
// automatically:
//
//   - hides the overlay when the game minimises or is hidden;
//   - drops the overlay behind any window that gets focus in front of the
//     game (so alt-tabbing to the desktop hides the overlay);
//   - re-raises the overlay with the game when the user comes back to it.
//
// Requesting HWND_TOPMOST would break that behaviour — the overlay would
// float over every desktop window (browsers, Discord, etc.) even when the
// LoL client is not focused, which is exactly the "drawing onto the
// desktop" bug the user reported.
void MoveOverlayToTarget() {
    if (!g_hGameWindow || !g_hOverlay) {
        return;
    }

    const DWORD now = GetTickCount();
    static DWORD s_lastRectCheckTick = 0;
    if (now - s_lastRectCheckTick < 100) {
        return;
    }
    s_lastRectCheckTick = now;

    RECT rc = {};
    GetWindowRect(g_hGameWindow, &rc);

    static RECT s_lastRect = {};
    static DWORD s_lastSetWindowPosTick = 0;
    const bool sameRect = EqualRect(&rc, &s_lastRect) != FALSE;
    if (sameRect && now - s_lastSetWindowPosTick < 500) {
        return;
    }

    SetWindowPos(g_hOverlay, HWND_TOP,
        rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    s_lastRect = rc;
    s_lastSetWindowPosTick = now;
}

void SetClickThrough(bool through) {
    if (!g_hOverlay) {
        return;
    }

    LONG_PTR cur = GetWindowLongPtrW(g_hOverlay, GWL_EXSTYLE);
    LONG_PTR desired = through ? (cur | WS_EX_TRANSPARENT) : (cur & ~WS_EX_TRANSPARENT);
    if (desired == cur) {
        return;
    }

    SetWindowLongPtrW(g_hOverlay, GWL_EXSTYLE, desired);
    SetWindowPos(g_hOverlay, nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// Anti-capture: hide overlay from OBS / screenshots / screen recording
// Uses SetWindowDisplayAffinity (Windows 10 2004+)
// WDA_EXCLUDEFROMCAPTURE = 0x11 — window is invisible to capture APIs
void SetAntiCapture(bool enabled) {
    if (!g_hOverlay) return;
    if (enabled == g_antiCaptureEnabled) return;
    const DWORD affinity = enabled ? 0x00000011u : 0x00000000u;
    if (SetWindowDisplayAffinity(g_hOverlay, affinity)) {
        g_antiCaptureEnabled = enabled;
    }
}

void UpdateClickThroughFromMenuBounds() {
    const bool menuVisible = g_bMenuVisible != 0;
    const DWORD now = GetTickCount();
    static DWORD s_lastClickThroughTick = 0;
    static bool s_lastMenuVisible = false;
    if (menuVisible == s_lastMenuVisible && now - s_lastClickThroughTick < 33) {
        return;
    }
    s_lastMenuVisible = menuVisible;
    s_lastClickThroughTick = now;

    if (!menuVisible) {
        SetClickThrough(true);
        return;
    }

    POINT cursorPt = {};
    GetCursorPos(&cursorPt);
    ScreenToClient(g_hOverlay, &cursorPt);

    const float cx = (float)cursorPt.x;
    const float cy = (float)cursorPt.y;
    const bool overMenu =
        (NightSharpMenu::menuBoundsRight > 0.0f) &&
        cx >= NightSharpMenu::menuPosX &&
        cx <= NightSharpMenu::menuBoundsRight &&
        cy >= NightSharpMenu::menuPosY &&
        cy <= NightSharpMenu::menuBoundsBottom;

    SetClickThrough(!overMenu);
}

void ToggleMenuVisible() {
    LONG vis = !g_bMenuVisible;
    InterlockedExchange(&g_bMenuVisible, vis);
    NightSharpMenu::showMenu = (vis != 0);
    SDK::MenuManager::SetMenuVisible(vis != 0);
    SetClickThrough(vis == 0);
}

} // namespace

void Overlay::Run() {
    CrashTelemetry::Install();
    CrashTelemetry::SetStage("Overlay::Run::Enter");
    InterlockedExchange(&g_bRunning, 1);
    InterlockedExchange(&g_bShutdown, 0);

    WriteStage("[NightSharp] STAGE 0: Overlay::Run() entered\r\n");

    CrashTelemetry::SetStage("Overlay::Run::FindGameWindow");
    for (int i = 0; i < 120 && !g_bShutdown; ++i) {
        g_hGameWindow = FindGameWindow();
        if (g_hGameWindow) {
            break;
        }
        Sleep(500);
    }

    if (!g_hGameWindow) {
        WriteStage("[NightSharp] STAGE 1: FAILED - game window not found\r\n");
        InterlockedExchange(&g_bRunning, 0);
        return;
    }

    WriteStage("[NightSharp] STAGE 1: Game window found\r\n");

    CrashTelemetry::SetStage("Overlay::Run::RegisterClass");
    wchar_t overlayClassName[96] = {};
    wsprintfW(
        overlayClassName,
        L"%s_%08X_%p",
        OVERLAY_CLASS_BASE,
        GetCurrentProcessId(),
        OverlayWndProc);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = overlayClassName;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    bool classRegistered = false;

    SetLastError(ERROR_SUCCESS);
    if (!RegisterClassExW(&wc)) {
        const DWORD lastError = GetLastError();
        char buf[256] = {};
        wsprintfA(
            buf,
            "[NightSharp] STAGE 2: FAILED - RegisterClassExW failed (gle=%lu)\r\n",
            lastError);
        WriteStage(buf);
        InterlockedExchange(&g_bRunning, 0);
        return;
    }
    classRegistered = true;

    WriteStage("[NightSharp] STAGE 2: Window class registered\r\n");

    CrashTelemetry::SetStage("Overlay::Run::CreateWindow");
    RECT gameRect = {};
    GetWindowRect(g_hGameWindow, &gameRect);

    // Create the overlay as a POPUP OWNED by the game window.
    //
    // - hWndParent (9th arg) = `g_hGameWindow` makes the overlay's owner the
    //   LoL client. In Win32, a popup with an owner is automatically shown
    //   only when its owner is visible, and is z-ordered just above the
    //   owner. That single parameter is what makes the overlay behave like
    //   an in-game HUD (tab out → overlay disappears) instead of like a
    //   desktop gadget.
    // - We deliberately OMIT `WS_EX_TOPMOST` — it was forcing the overlay
    //   to float over every desktop window, which defeats the "owned by
    //   game" ordering above.
    // - WS_EX_TOOLWINDOW keeps the overlay out of the taskbar / alt-tab UI.
    // - WS_EX_NOACTIVATE + WS_EX_TRANSPARENT + WS_EX_LAYERED give the usual
    //   "draw-only, never-steal-focus, alpha-composited" behaviour.
    g_hOverlay = CreateWindowExW(
        WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        overlayClassName,
        L"NightSharp Overlay",
        WS_POPUP,
        gameRect.left,
        gameRect.top,
        gameRect.right - gameRect.left,
        gameRect.bottom - gameRect.top,
        g_hGameWindow,  // owner — ties the overlay's visibility to the game
        nullptr,
        wc.hInstance,
        nullptr);

    if (!g_hOverlay) {
        const DWORD lastError = GetLastError();
        char buf[256] = {};
        wsprintfA(
            buf,
            "[NightSharp] STAGE 3: FAILED - CreateWindowExW failed (gle=%lu)\r\n",
            lastError);
        WriteStage(buf);
        if (classRegistered) {
            UnregisterClassW(overlayClassName, wc.hInstance);
        }
        InterlockedExchange(&g_bRunning, 0);
        return;
    }

    WriteStage("[NightSharp] STAGE 3: Overlay window created\r\n");

    CrashTelemetry::SetStage("Overlay::Run::InitD3D11");
    SetLayeredWindowAttributes(g_hOverlay, 0, 255, LWA_ALPHA);
    const MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(g_hOverlay, &margins);

    if (!CreateDeviceD3D11(g_hOverlay)) {
        WriteStage("[NightSharp] STAGE 4: FAILED - D3D11 init failed\r\n");
        DestroyWindow(g_hOverlay);
        g_hOverlay = nullptr;
        if (classRegistered) {
            UnregisterClassW(overlayClassName, wc.hInstance);
        }
        InterlockedExchange(&g_bRunning, 0);
        return;
    }

    WriteStage("[NightSharp] STAGE 4: D3D11 device + swap chain created OK\r\n");

    CrashTelemetry::SetStage("Overlay::Run::InitImGui");
    NsHeapInit();
    ImGui::SetAllocatorFunctions(NsImGuiAlloc, NsImGuiFree, nullptr);
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.Fonts->AddFontDefault();

    ImGui_ImplWin32_Init(g_hOverlay);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

    WriteStage("[NightSharp] STAGE 5: ImGui initialized\r\n");

    CrashTelemetry::SetStage("Overlay::Run::InitMenuPlugins");
    ShowWindow(g_hOverlay, SW_SHOWNOACTIVATE);
    UpdateWindow(g_hOverlay);
    MoveOverlayToTarget();

    SDK::MenuManager::Init();
    NightSharpMenu::showMenu = true;
    SDK::MenuManager::SetMenuVisible(true);
    SetClickThrough(false);

    // Install the Shadow-VMT OnProcessSpell hook on a BACKGROUND thread.
    // FindAllDispatchSlots sweeps every writable region in the process, which
    // on a 64-bit LoL can take hundreds of milliseconds — doing it inline
    // would freeze the overlay right after inject. The worker exits as soon
    // as the scan finishes; the menu's "SDK Diagnostics" badge flips from
    // NOT HOOKED → HOOKED the frame it completes.
    CloseHandle(CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
        CoreEventHook::InstallAllHooks();
        return 0;
    }, nullptr, 0, nullptr));

    Plugins::PluginBootstrap::EnsureRegistered();
    PluginRegistry::LoadConfig();

    // Phase 3.1 (Apr 26/2026): NightPackageLoader retired - all plugins are
    // now built-in (no external .night encrypted package format).

    WriteStage("[NightSharp] STAGE 6: Menu + plugin substrate initialized\r\n");

    CrashTelemetry::SetStage("Overlay::Run::InitCore");
    const bool coreInitOk = CoreRuntime::Initialize();
    WriteStage(coreInitOk
        ? "[NightSharp] STAGE 7: CoreRuntime initialized\r\n"
        : "[NightSharp] STAGE 7: CoreRuntime init incomplete\r\n");
    if (coreInitOk) {
        __try { CoreRuntime::TickRead(); } __except (1) {}
    }

    CrashTelemetry::SetStage("Overlay::Run::InitSDK");
    bool sdkInitFault = false;
    __try {
        SDK::Bootstrap::Init(nullptr);
    } __except (1) {
        sdkInitFault = true;
    }
    if (sdkInitFault) {
        WriteStageFmt(
            "[NightSharp] STAGE 8: FAILED - SDK bootstrap crashed at %s\r\n",
            CrashTelemetry::g_stage ? CrashTelemetry::g_stage : "unknown");
        __try { SDK::Bootstrap::Shutdown(); } __except (1) {}
        CoreRuntime::Shutdown();
        SDK::MenuManager::Shutdown();
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        CleanupDeviceD3D11();
        if (g_hOverlay) {
            DestroyWindow(g_hOverlay);
            g_hOverlay = nullptr;
        }
        if (classRegistered) {
            UnregisterClassW(overlayClassName, wc.hInstance);
        }
        NsHeapDestroy();
        InterlockedExchange(&g_bRunning, 0);
        return;
    }
    WriteStage("[NightSharp] STAGE 8: SDK bootstrap initialized\r\n");

    // SDK::Bootstrap registers SDK menu entries after the first config load.
    // Re-apply config now, then load runtime plugins only after Core/SDK state
    // exists so champion checks and event wiring are valid on startup.
    PluginRegistry::LoadConfig();
    Plugins::PluginManager::Get().LoadAuto();
    PluginHostBridge::WireHostAPI();

    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    int frameCount = 0;
    DWORD lastGameAliveCheckTick = 0;
    bool cachedGameAlive = true;

    while (!g_bShutdown) {
        bool frameFault = false;
        const DWORD perfFrameStart = GetTickCount();
        DWORD perfLastTick = perfFrameStart;
        DWORD perfSlowMs = 0;
        const char* perfSlowStage = "FrameStart";
        const auto traceFrameStage = [&](const char* stage) {
            if (frameCount < 2 && stage && *stage) {
                WriteStageFmt("[NightSharp] FRAME %d: %s\r\n", frameCount, stage);
            }
        };
        __try {
            CrashTelemetry::SetStage("Overlay::Frame::PumpMessages");
            traceFrameStage("PumpMessages");
            MSG msg = {};
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
                if (msg.message == WM_QUIT) {
                    InterlockedExchange(&g_bShutdown, 1);
                }
            }
            MarkFramePerf(perfLastTick, perfSlowMs, perfSlowStage, "PumpMessages");

            if (g_bShutdown) {
                __leave;
            }

            CrashTelemetry::SetStage("Overlay::Frame::CheckGameWindow");
            traceFrameStage("CheckGameWindow");
            const DWORD nowAliveCheck = GetTickCount();
            if (nowAliveCheck - lastGameAliveCheckTick >= 500) {
                lastGameAliveCheckTick = nowAliveCheck;
                cachedGameAlive = IsGameWindowAlive();
            }
            if (!cachedGameAlive) {
                WriteStage("[NightSharp] RENDER LOOP: Game window gone, breaking\r\n");
                InterlockedExchange(&g_bShutdown, 1);
                __leave;
            }
            MarkFramePerf(perfLastTick, perfSlowMs, perfSlowStage, "CheckGameWindow");

            CrashTelemetry::SetStage("Overlay::Frame::Resize");
            traceFrameStage("Resize");
            if (g_ResizeW != 0 && g_ResizeH != 0) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, g_ResizeW, g_ResizeH, DXGI_FORMAT_UNKNOWN, 0);
                g_ResizeW = 0;
                g_ResizeH = 0;
                CreateRenderTarget();
            }

            CrashTelemetry::SetStage("Overlay::Frame::MoveOverlay");
            traceFrameStage("MoveOverlay");
            MoveOverlayToTarget();

            // Sync OBS bypass with menu toggle
            SetAntiCapture(Config::StreamProtection::bypassObs);

            CrashTelemetry::SetStage("Overlay::Frame::Hotkeys");
            traceFrameStage("Hotkeys");
            if (GetAsyncKeyState(VK_F1) & 1) {
                ToggleMenuVisible();
            }
            if (GetAsyncKeyState(VK_END) & 1) {
                WriteStage("[NightSharp] RENDER LOOP: VK_END pressed, breaking\r\n");
                InterlockedExchange(&g_bShutdown, 1);
                __leave;
            }

            // Remote unload request: the external injector writes flag[18]=0x01
            // when the user presses Ctrl+R. We drop out of the render loop,
            // run the full cleanup path (uninstall hook + destroy ImGui + free
            // heap) and ONLY THEN mark flag[16]=0x31 so the injector knows
            // it is safe to VirtualFreeEx(remoteBase).
            if (auto* fp = NightSharp_GetFlagPage()) {
                if (fp[18] == 0x01) {
                    WriteStage("[NightSharp] RENDER LOOP: injector unload request (flag[18]=1), breaking\r\n");
                    fp[18] = 0x00;                                 // consume the request
                    InterlockedExchange(&g_bShutdown, 1);
                    __leave;
                }
            }

            CrashTelemetry::SetStage("Overlay::Frame::ClickThrough");
            traceFrameStage("ClickThrough");
            UpdateClickThroughFromMenuBounds();
            MarkFramePerf(perfLastTick, perfSlowMs, perfSlowStage, "WindowState");

            CrashTelemetry::SetStage("Overlay::Frame::TickRead");
            traceFrameStage("TickRead");
            CoreRuntime::TickRead();
            CrashTelemetry::SetStage("Overlay::Frame::Validate");
            traceFrameStage("Validate");
            bool validateFault = false;
            __try {
                CoreValidation::Refresh();
            } __except(CrashTelemetry::ReportAndHandle("Overlay::Frame::Validate", GetExceptionInformation())) {
                validateFault = true;
            }
            if (validateFault) {
                WriteStageFmt(
                    "[NightSharp] FRAME %d: Validate fault at %s\r\n",
                    frameCount,
                    CrashTelemetry::g_stage ? CrashTelemetry::g_stage : "unknown");
                CoreRuntime::g_ctx.validationMask = 0;
            }
            MarkFramePerf(perfLastTick, perfSlowMs, perfSlowStage, "CoreReadValidate");
            CrashTelemetry::SetStage("Overlay::Frame::BeginWrite");
            traceFrameStage("BeginWrite");
            CoreRuntime::BeginWritePhase();
            CrashTelemetry::SetStage("Overlay::Frame::SDKUpdate");
            traceFrameStage("SDKUpdate");
            __try { SDK::Bootstrap::Update(); } __except (1) {}
            MarkFramePerf(perfLastTick, perfSlowMs, perfSlowStage, "SDKUpdate");

            // Heap-only memory hacks (CRC-safe). Run inside write phase.
            CrashTelemetry::SetStage("Overlay::Frame::ZoomHack");
            __try {
                CoreZoomHack::Tick(Config::ZoomHack::enabled, Config::ZoomHack::maxZoom);
            } __except(1) {}

            CrashTelemetry::SetStage("Overlay::Frame::SkinChanger");
            __try {
                CoreSkinChanger::Tick(Config::SkinChanger::enabled, Config::SkinChanger::skinId);
            } __except(1) {}
            MarkFramePerf(perfLastTick, perfSlowMs, perfSlowStage, "CoreHacks");

            CrashTelemetry::SetStage("Overlay::Frame::ImGuiBackends");
            traceFrameStage("ImGuiBackends");
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();

            if (g_bMenuVisible) {
                POINT mp = {};
                GetCursorPos(&mp);
                ScreenToClient(g_hOverlay, &mp);

                ImGuiIO& frameIo = ImGui::GetIO();
                frameIo.MousePos = ImVec2((float)mp.x, (float)mp.y);
                frameIo.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
                frameIo.MouseDown[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            }

            CrashTelemetry::SetStage("Overlay::Frame::ImGuiNewFrame");
            traceFrameStage("ImGuiNewFrame");
            ImGui::NewFrame();
            MarkFramePerf(perfLastTick, perfSlowMs, perfSlowStage, "ImGuiFrame");

            CrashTelemetry::SetStage("Overlay::Frame::PluginsUpdate");
            traceFrameStage("PluginsUpdate");
            Plugins::PluginManager::Get().OnUpdate();
            MarkFramePerf(perfLastTick, perfSlowMs, perfSlowStage, "PluginsUpdate");

            // Drive the event-hook pollers every frame. No-op until the
            // Shadow-VMT hook is installed (via CoreEventHook::InstallAllHooks
            // above or the "Install Hook" button in SDK Diagnostics).
            CrashTelemetry::SetStage("Overlay::Frame::EventHookPoll");
            traceFrameStage("EventHookPoll");
            CoreEventHook::PollAllEvents();
            MarkFramePerf(perfLastTick, perfSlowMs, perfSlowStage, "EventHookPoll");
            CrashTelemetry::SetStage("Overlay::Frame::MenuRender");
            traceFrameStage("MenuRender");
            NightSharpMenu::Render();
            CrashTelemetry::SetStage("Overlay::Frame::PluginsRender");
            traceFrameStage("PluginsRender");
            Plugins::PluginManager::Get().OnRender();
            CrashTelemetry::SetStage("Overlay::Frame::SDKRender");
            traceFrameStage("SDKRender");
            __try { SDK::Bootstrap::Render(); } __except (1) {}
            MarkFramePerf(perfLastTick, perfSlowMs, perfSlowStage, "DrawBuild");
            CrashTelemetry::SetStage("Overlay::Frame::EndWrite");
            traceFrameStage("EndWrite");
            CoreRuntime::EndWritePhase();

            CrashTelemetry::SetStage("Overlay::Frame::ImGuiRender");
            traceFrameStage("ImGuiRender");
            ImGui::EndFrame();
            ImGui::Render();
            MarkFramePerf(perfLastTick, perfSlowMs, perfSlowStage, "ImGuiRender");

            CrashTelemetry::SetStage("Overlay::Frame::D3DPresent");
            traceFrameStage("D3DPresent");
            g_pd3dContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
            g_pd3dContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            // Present without v-sync; the loop below applies a light cap so
            // the overlay can reach 100+ FPS without spinning uncapped.
            g_pSwapChain->Present(0, 0);
            MarkFramePerf(perfLastTick, perfSlowMs, perfSlowStage, "D3DPresent");
            LogSlowFrame(frameCount, perfFrameStart, perfSlowMs, perfSlowStage);

            const DWORD frameElapsed = GetTickCount() - perfFrameStart;
            if (frameElapsed < kTargetOverlayFrameMs) {
                Sleep(kTargetOverlayFrameMs - frameElapsed);
            }

            ++frameCount;
            if (frameCount <= 2) {
                WriteStageFmt("[NightSharp] FRAME %d: COMPLETE\r\n", frameCount - 1);
            }
            if ((frameCount % 300) == 0) {
                char buf[128];
                wsprintfA(buf, "[NightSharp] HEARTBEAT: frame %d rendered\r\n", frameCount);
                WriteStage(buf);
            }
        } __except(CrashTelemetry::ReportAndHandle("Overlay::Frame", GetExceptionInformation())) {
            frameFault = true;
        }

        if (frameFault) {
            WriteStage("[NightSharp] RENDER LOOP: frame exception, breaking\r\n");
            break;
        }
    }

    CrashTelemetry::SetStage("Overlay::Run::Cleanup");

    // IMPORTANT: uninstall the Shadow-VMT hook FIRST. While the shadow
    // dispatch slot still points at our shellcode trampoline, any spell
    // dispatched on another game thread would jump into memory we are
    // about to free — silent crash. Restoring the original vtable pointer
    // makes subsequent dispatches safe before we tear everything else down.
    __try { CoreEventHook::UninstallAll(); } __except (1) {}

    Plugins::PluginManager::Get().UnloadAll();
    __try { SDK::Bootstrap::Shutdown(); } __except (1) {}
    CoreRuntime::Shutdown();
    SDK::MenuManager::Shutdown();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D11();

    if (g_hOverlay) {
        DestroyWindow(g_hOverlay);
        g_hOverlay = nullptr;
    }

    if (classRegistered) {
        UnregisterClassW(overlayClassName, wc.hInstance);
    }
    NsHeapDestroy();

    InterlockedExchange(&g_bRunning, 0);
    WriteStage("[NightSharp] Overlay cleanup complete\r\n");
}

void Overlay::RequestShutdown() {
    InterlockedExchange(&g_bShutdown, 1);
}

bool Overlay::IsRunning() {
    return g_bRunning != 0;
}

void Overlay::ToggleMenu() {
    ToggleMenuVisible();
}

bool Overlay::IsMenuVisible() {
    return g_bMenuVisible != 0;
}
