#include "Overlay.h"

#include "OverlayStatus.h"
#include "../crt_shim.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx11.h"
#include "../imgui/imgui_impl_win32.h"
#include "../menu/MenuConfig.h"
#include "../menu/NightSharpMenu.h"
#include "../Plugins/PluginBootstrap.h"
#include "../Plugins/PluginManager.h"
#include "../Core/CoreRuntime.h"
#include "../CrashReporter.h"
#include "../DebugLog.h"
#include "../FpsDropDebug.h"
#include "../SDK/Lifecycle.h"
#include "../SDK/UI/Icons.h"

#include <d3d11.h>
#include <dcomp.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <windowsx.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dcomp.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dContext = nullptr;
IDXGISwapChain1* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
IDCompositionDevice* g_pDcompDevice = nullptr;
IDCompositionTarget* g_pDcompTarget = nullptr;
IDCompositionVisual* g_pDcompVisual = nullptr;

HWND g_hOverlay = nullptr;
HWND g_hTargetWindow = nullptr;
UINT g_ResizeW = 0;
UINT g_ResizeH = 0;
bool g_antiCaptureEnabled = false;

volatile LONG g_bRunning = 0;
volatile LONG g_bShutdown = 0;
volatile LONG g_bMenuVisible = 1;
volatile LONG g_bMenuReady = 0;

const wchar_t* OVERLAY_CLASS_BASE = L"NightSharpOverlay";
constexpr DWORD kTargetOverlayFrameMs = 16; // ~60 Hz — halves per-frame CPU vs 125 Hz
constexpr DWORD kGameReadyPollMs = 500;
constexpr int kGameReadyMaxPolls = 240;
constexpr DWORD kMenuStartDelayAfterLocalPlayerMs = 3000;

template <typename T>
void SafeRelease(T*& ptr) {
    if (ptr) {
        ptr->Release();
        ptr = nullptr;
    }
}

void Log(const char* msg) {
    NightSharpDebug::WriteRaw(msg);
}

void LogLastError(const char* prefix) {
    NightSharpDebug::Logf("%s (gle=%lu)", prefix, GetLastError());
}

bool IsShutdownRequested() {
    return InterlockedCompareExchange(&g_bShutdown, 0, 0) != 0;
}

bool ReadLocalPlayerSafe(uintptr_t& outLocalPlayer) {
    outLocalPlayer = 0;
    __try {
        if (CoreRuntime::EnsureInitialized()) {
            (void)CoreRuntime::RefreshReadState();
            const auto& ctx = CoreRuntime::GetContext();
            outLocalPlayer = ctx.localPlayer;
            return (ctx.statusMask & CoreRuntime::Status_RuntimeObjectsReady) != 0 &&
                   outLocalPlayer != 0;
        }
    } __except (1) {
        outLocalPlayer = 0;
    }
    return false;
}

bool WaitForGameReady() {
    NightSharpDebug::Phase("overlay-wait-game-ready");
    Log("[NightSharp] Waiting for first localPlayer read, then delaying menu/plugins 3.0s\n");

    bool sawLocalPlayer = false;
    DWORD firstLocalPlayerTick = 0;
    for (int i = 0; i < kGameReadyMaxPolls; ++i) {
        if (IsShutdownRequested()) {
            return false;
        }
        if (GetAsyncKeyState(VK_END) & 1) {
            InterlockedExchange(&g_bShutdown, 1);
            return false;
        }
        if (g_hTargetWindow && !IsWindow(g_hTargetWindow)) {
            InterlockedExchange(&g_bShutdown, 1);
            return false;
        }

        uintptr_t localPlayer = 0;
        if (ReadLocalPlayerSafe(localPlayer)) {
            if (!sawLocalPlayer) {
                sawLocalPlayer = true;
                firstLocalPlayerTick = GetTickCount();
                char buffer[160] = {};
                wsprintfA(
                    buffer,
                    "[NightSharp] First localPlayer read; delaying menu/plugins 3.0s\n");
                Log(buffer);
            }

            const DWORD elapsedMs = GetTickCount() - firstLocalPlayerTick;

            if (elapsedMs >= kMenuStartDelayAfterLocalPlayerMs) {
                char buffer[192] = {};
                wsprintfA(
                    buffer,
                    "[NightSharp] LocalPlayer ready, elapsed=%d.%02d. Loading menu/plugins/hooks\n",
                    static_cast<int>(elapsedMs / 1000),
                    static_cast<int>((elapsedMs % 1000) / 10));
                Log(buffer);
                NightSharpDebug::Phase("overlay-game-ready");
                return true;
            }
        }

        if (i > 0 && (i % 20) == 0) {
            char buffer[160] = {};
            if (sawLocalPlayer) {
                const DWORD elapsedMs = GetTickCount() - firstLocalPlayerTick;
                wsprintfA(
                    buffer,
                    "[NightSharp] Still delaying menu/plugins, elapsed=%d.%02d\n",
                    static_cast<int>(elapsedMs / 1000),
                    static_cast<int>((elapsedMs % 1000) / 10));
            } else {
                wsprintfA(
                    buffer,
                    "[NightSharp] Still waiting for first localPlayer read (%d sec)\n",
                    static_cast<int>((i * kGameReadyPollMs) / 1000));
            }
            Log(buffer);
        }
        Sleep(kGameReadyPollMs);
    }

    Log("[NightSharp] LocalPlayer wait timed out, continuing startup\n");
    return !IsShutdownRequested();
}

bool CreateRenderTarget() {
    if (!g_pSwapChain || !g_pd3dDevice) {
        return false;
    }

    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr) || !backBuffer) {
        return false;
    }

    hr = g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_pRenderTargetView);
    backBuffer->Release();
    return SUCCEEDED(hr) && g_pRenderTargetView != nullptr;
}

void CleanupRenderTarget() {
    SafeRelease(g_pRenderTargetView);
}

void CleanupDeviceD3D11() {
    CleanupRenderTarget();
    SafeRelease(g_pDcompVisual);
    SafeRelease(g_pDcompTarget);
    SafeRelease(g_pDcompDevice);
    SafeRelease(g_pSwapChain);
    SafeRelease(g_pd3dContext);
    SafeRelease(g_pd3dDevice);
}

bool CreateDeviceD3D11(HWND hWnd) {
    CleanupDeviceD3D11();

    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        levels,
        2,
        D3D11_SDK_VERSION,
        &g_pd3dDevice,
        &featureLevel,
        &g_pd3dContext);

    if (FAILED(hr) || !g_pd3dDevice || !g_pd3dContext) {
        CleanupDeviceD3D11();
        return false;
    }

    IDXGIDevice* dxgiDevice = nullptr;
    IDXGIAdapter* adapter = nullptr;
    IDXGIFactory2* factory = nullptr;

    hr = g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    if (FAILED(hr) || !dxgiDevice) {
        CleanupDeviceD3D11();
        return false;
    }

    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr) || !adapter) {
        SafeRelease(dxgiDevice);
        CleanupDeviceD3D11();
        return false;
    }

    hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    SafeRelease(adapter);
    if (FAILED(hr) || !factory) {
        SafeRelease(dxgiDevice);
        CleanupDeviceD3D11();
        return false;
    }

    RECT rc = {};
    GetClientRect(hWnd, &rc);
    UINT width = static_cast<UINT>(rc.right - rc.left);
    UINT height = static_cast<UINT>(rc.bottom - rc.top);
    if (width == 0) {
        width = 1;
    }
    if (height == 0) {
        height = 1;
    }

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    desc.Scaling = DXGI_SCALING_STRETCH;

    hr = factory->CreateSwapChainForComposition(g_pd3dDevice, &desc, nullptr, &g_pSwapChain);
    SafeRelease(factory);
    if (FAILED(hr) || !g_pSwapChain) {
        SafeRelease(dxgiDevice);
        CleanupDeviceD3D11();
        return false;
    }

    hr = DCompositionCreateDevice(dxgiDevice, IID_PPV_ARGS(&g_pDcompDevice));
    SafeRelease(dxgiDevice);
    if (FAILED(hr) || !g_pDcompDevice) {
        CleanupDeviceD3D11();
        return false;
    }

    hr = g_pDcompDevice->CreateTargetForHwnd(hWnd, TRUE, &g_pDcompTarget);
    if (FAILED(hr) || !g_pDcompTarget) {
        CleanupDeviceD3D11();
        return false;
    }

    hr = g_pDcompDevice->CreateVisual(&g_pDcompVisual);
    if (FAILED(hr) || !g_pDcompVisual) {
        CleanupDeviceD3D11();
        return false;
    }

    hr = g_pDcompVisual->SetContent(g_pSwapChain);
    if (FAILED(hr)) {
        CleanupDeviceD3D11();
        return false;
    }

    hr = g_pDcompTarget->SetRoot(g_pDcompVisual);
    if (FAILED(hr)) {
        CleanupDeviceD3D11();
        return false;
    }

    hr = g_pDcompDevice->Commit();
    if (FAILED(hr)) {
        CleanupDeviceD3D11();
        return false;
    }

    if (!CreateRenderTarget()) {
        CleanupDeviceD3D11();
        return false;
    }

    return true;
}

HWND FindProcessWindow() {
    struct EnumData {
        DWORD pid;
        HWND result;
    } data = { GetCurrentProcessId(), nullptr };

    EnumWindows([](HWND hWnd, LPARAM lParam) -> BOOL {
        auto* d = reinterpret_cast<EnumData*>(lParam);
        DWORD pid = 0;
        GetWindowThreadProcessId(hWnd, &pid);
        if (pid != d->pid) {
            return TRUE;
        }
        if (!IsWindowVisible(hWnd)) {
            return TRUE;
        }
        if ((GetWindowLongPtrW(hWnd, GWL_STYLE) & WS_CHILD) != 0) {
            return TRUE;
        }

        RECT rc = {};
        GetWindowRect(hWnd, &rc);
        if ((rc.right - rc.left) <= 0 || (rc.bottom - rc.top) <= 0) {
            return TRUE;
        }

        d->result = hWnd;
        return FALSE;
    }, reinterpret_cast<LPARAM>(&data));

    return data.result;
}

void GetOverlayRect(RECT* outRect) {
    if (!outRect) {
        return;
    }

    if (g_hTargetWindow && IsWindow(g_hTargetWindow)) {
        GetWindowRect(g_hTargetWindow, outRect);
        return;
    }

    outRect->left = 0;
    outRect->top = 0;
    outRect->right = GetSystemMetrics(SM_CXSCREEN);
    outRect->bottom = GetSystemMetrics(SM_CYSCREEN);
}

void MoveOverlayToTarget() {
    if (!g_hOverlay) {
        return;
    }

    const DWORD now = GetTickCount();
    static DWORD s_lastRectCheckTick = 0;
    if (now - s_lastRectCheckTick < 100) {
        return;
    }
    s_lastRectCheckTick = now;

    RECT rc = {};
    GetOverlayRect(&rc);

    static RECT s_lastRect = {};
    static DWORD s_lastSetWindowPosTick = 0;
    const bool sameRect = EqualRect(&rc, &s_lastRect) != FALSE;
    if (sameRect && now - s_lastSetWindowPosTick < 500) {
        return;
    }

    HWND zOrder = g_hTargetWindow ? HWND_TOP : HWND_TOPMOST;
    SetWindowPos(
        g_hOverlay,
        zOrder,
        rc.left,
        rc.top,
        rc.right - rc.left,
        rc.bottom - rc.top,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

    s_lastRect = rc;
    s_lastSetWindowPosTick = now;
}

bool IsClientPointOverMenu(POINT pt) {
    return NightSharpMenu::IsPointInside(static_cast<float>(pt.x), static_cast<float>(pt.y));
}

bool IsScreenPointOverMenu(POINT pt) {
    if (!g_hOverlay) {
        return false;
    }

    ScreenToClient(g_hOverlay, &pt);
    return IsClientPointOverMenu(pt);
}

void SetClickThrough(bool through) {
    if (!g_hOverlay) {
        return;
    }

    LONG_PTR current = GetWindowLongPtrW(g_hOverlay, GWL_EXSTYLE);
    LONG_PTR desired = through ? (current | WS_EX_TRANSPARENT) : (current & ~WS_EX_TRANSPARENT);
    if (desired == current) {
        return;
    }

    SetWindowLongPtrW(g_hOverlay, GWL_EXSTYLE, desired);
    SetWindowPos(
        g_hOverlay,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void SetAntiCapture(bool enabled) {
    if (!g_hOverlay || enabled == g_antiCaptureEnabled) {
        return;
    }

    constexpr DWORD kWdaNone = 0x00000000u;
    constexpr DWORD kWdaExcludeFromCapture = 0x00000011u;
    const DWORD affinity = enabled ? kWdaExcludeFromCapture : kWdaNone;

    if (SetWindowDisplayAffinity(g_hOverlay, affinity)) {
        g_antiCaptureEnabled = enabled;
        NightSharpDebug::Logf(
            "[Overlay] SetWindowDisplayAffinity affinity=0x%08lX enabled=%d",
            affinity,
            enabled ? 1 : 0);
        return;
    }

    static DWORD s_lastFailureTick = 0;
    const DWORD now = GetTickCount();
    if (now - s_lastFailureTick > 5000) {
        s_lastFailureTick = now;
        NightSharpDebug::Logf(
            "[Overlay] SetWindowDisplayAffinity failed affinity=0x%08lX gle=%lu",
            affinity,
            GetLastError());
    }
}

void UpdateClickThroughFromMenuBounds() {
    if (Config::OverlayInput::clickThrough) {
        SetClickThrough(true);
        return;
    }

    if (g_bMenuVisible == 0) {
        SetClickThrough(true);
        return;
    }

    POINT cursorPt = {};
    GetCursorPos(&cursorPt);
    SetClickThrough(!IsScreenPointOverMenu(cursorPt));
}

void ToggleMenuVisible() {
    if (InterlockedCompareExchange(&g_bMenuReady, 0, 0) == 0) {
        return;
    }

    LONG visible = !g_bMenuVisible;
    InterlockedExchange(&g_bMenuVisible, visible);
    NightSharpMenu::showMenu = (visible != 0);
    SetClickThrough(Config::OverlayInput::clickThrough || visible == 0);
}

void SyncImGuiMouse() {
    if (!g_hOverlay) {
        return;
    }

    POINT mousePos = {};
    GetCursorPos(&mousePos);
    ScreenToClient(g_hOverlay, &mousePos);

    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
    const bool acceptMenuInput = g_bMenuVisible != 0;
    io.MouseDown[0] = acceptMenuInput && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    io.MouseDown[1] = acceptMenuInput && (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
}

LRESULT WINAPI OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCHITTEST) {
        if (Config::OverlayInput::clickThrough) {
            return HTTRANSPARENT;
        }

        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        return IsScreenPointOverMenu(pt) ? HTCLIENT : HTTRANSPARENT;
    }

    if (g_bMenuVisible && !Config::OverlayInput::clickThrough) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
            return TRUE;
        }
    }

    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            g_ResizeW = static_cast<UINT>(LOWORD(lParam));
            g_ResizeH = static_cast<UINT>(HIWORD(lParam));
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

} // namespace

void Overlay::Run() {
    NightSharpDebug::Phase("overlay-run-start");
    InterlockedExchange(&g_bRunning, 1);
    InterlockedExchange(&g_bShutdown, 0);
    InterlockedExchange(&g_bMenuReady, 0);
    InterlockedExchange(&g_bMenuVisible, 0);
    NightSharpMenu::showMenu = false;
    SDK::Lifecycle::ResetShutdownState();

    Log("[NightSharp] Overlay::Run entered\n");

    NightSharpDebug::Phase("overlay-find-window");
    g_hTargetWindow = FindProcessWindow();

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
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    bool classRegistered = false;
    NightSharpDebug::Phase("overlay-register-class");
    SetLastError(ERROR_SUCCESS);
    if (!RegisterClassExW(&wc)) {
        NightSharpDebug::Phase("overlay-register-class-failed");
        LogLastError("[NightSharp] RegisterClassExW failed");
        InterlockedExchange(&g_bRunning, 0);
        return;
    }
    classRegistered = true;

    RECT overlayRect = {};
    GetOverlayRect(&overlayRect);

    const DWORD exStyle =
        WS_EX_LAYERED |
        WS_EX_TRANSPARENT |
        WS_EX_NOACTIVATE |
        WS_EX_TOOLWINDOW |
        (g_hTargetWindow ? 0 : WS_EX_TOPMOST);

    NightSharpDebug::Phase("overlay-create-window");
    g_hOverlay = CreateWindowExW(
        exStyle,
        overlayClassName,
        L"NightSharp Overlay",
        WS_POPUP,
        overlayRect.left,
        overlayRect.top,
        overlayRect.right - overlayRect.left,
        overlayRect.bottom - overlayRect.top,
        g_hTargetWindow,
        nullptr,
        wc.hInstance,
        nullptr);

    if (!g_hOverlay) {
        NightSharpDebug::Phase("overlay-create-window-failed");
        LogLastError("[NightSharp] CreateWindowExW failed");
        if (classRegistered) {
            UnregisterClassW(overlayClassName, wc.hInstance);
        }
        InterlockedExchange(&g_bRunning, 0);
        return;
    }

    NightSharpDebug::Phase("overlay-create-d3d11");
    if (!CreateDeviceD3D11(g_hOverlay)) {
        NightSharpDebug::Phase("overlay-create-d3d11-failed");
        Log("[NightSharp] D3D11 + DirectComposition init failed\n");
        DestroyWindow(g_hOverlay);
        g_hOverlay = nullptr;
        if (classRegistered) {
            UnregisterClassW(overlayClassName, wc.hInstance);
        }
        InterlockedExchange(&g_bRunning, 0);
        return;
    }

    NightSharpDebug::Phase("overlay-imgui-init");
    NsHeapInit();
    ImGui::SetAllocatorFunctions(NsImGuiAlloc, NsImGuiFree, nullptr);
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.Fonts->AddFontDefault();

    ImGui_ImplWin32_Init(g_hOverlay);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);
    SDK::UI::Icons::SetDevice(g_pd3dDevice, g_pd3dContext);

    if (WaitForGameReady()) {
        InterlockedExchange(&g_bMenuVisible, 1);
        SDK::Lifecycle::ResetShutdownState();
        NightSharpDebug::Phase("plugin-bootstrap");
        SDK::Events::SetDeliveryEnabled(false);
        __try {
            Plugins::PluginBootstrap::EnsureRegistered();
            SDK::Events::SetDeliveryEnabled(true);
            NightSharpMenu::showMenu = true;
            InterlockedExchange(&g_bMenuReady, 1);
        }
        __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                      "Overlay::PluginBootstrap::EnsureRegistered",
                      GetExceptionInformation())) {
            SDK::Events::SetDeliveryEnabled(false);
            __try { Plugins::PluginBootstrap::Shutdown(); } __except (1) {}
            __try { SDK::Events::Reset(); } __except (1) {}
            NightSharpDebug::Logf("[NightSharp] Plugin bootstrap crashed; requesting shutdown");
            InterlockedExchange(&g_bShutdown, 1);
        }
    }

    if (!IsShutdownRequested()) {
        ShowWindow(g_hOverlay, SW_SHOWNOACTIVATE);
        UpdateWindow(g_hOverlay);
        MoveOverlayToTarget();
        SetClickThrough(Config::OverlayInput::clickThrough);
    }

    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    while (!g_bShutdown) {
        NightSharpDebug::SetPhase("overlay-frame");
        const DWORD frameStart = GetTickCount();
        NightSharpPerf::BeginFrame();

        MSG msg = {};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                InterlockedExchange(&g_bShutdown, 1);
            }
        }

        if (g_bShutdown) {
            break;
        }

        if (g_hTargetWindow && !IsWindow(g_hTargetWindow)) {
            InterlockedExchange(&g_bShutdown, 1);
            break;
        }

        if (g_ResizeW != 0 && g_ResizeH != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeW, g_ResizeH, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeW = 0;
            g_ResizeH = 0;
            CreateRenderTarget();
        }

        MoveOverlayToTarget();
        SetAntiCapture(Config::StreamProtection::bypassObs);

        if ((GetAsyncKeyState(VK_F1) & 1) &&
            InterlockedCompareExchange(&g_bMenuReady, 0, 0) != 0) {
            ToggleMenuVisible();
        }
        if (GetAsyncKeyState(VK_END) & 1) {
            InterlockedExchange(&g_bShutdown, 1);
            break;
        }
        NightSharpPerf::ToggleHotkeys();

        UpdateClickThroughFromMenuBounds();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        SyncImGuiMouse();
        ImGui::NewFrame();

        // Refresh game state once per frame before any plugin accesses it.
        // EnsureReady() in CoreObjectManager no longer calls RefreshReadState
        // on every manager lookup, so this single tick is the only refresh.
        auto perfStart = NightSharpPerf::Now();
        CoreRuntime::TickRead();
        NightSharpPerf::AddPhase(
            "CoreRuntime::TickRead",
            NightSharpPerf::MsSince(perfStart));

        perfStart = NightSharpPerf::Now();
        Plugins::PluginManager::Get().OnUpdate();
        NightSharpPerf::AddPhase(
            "PluginManager::OnUpdate",
            NightSharpPerf::MsSince(perfStart));

        perfStart = NightSharpPerf::Now();
        Plugins::PluginManager::Get().OnRender();
        NightSharpPerf::AddPhase(
            "PluginManager::OnRender",
            NightSharpPerf::MsSince(perfStart));

        perfStart = NightSharpPerf::Now();
        if (InterlockedCompareExchange(&g_bMenuReady, 0, 0) != 0) {
            __try {
                NightSharpMenu::Render();
            }
            __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                          "Overlay::NightSharpMenu::Render",
                          GetExceptionInformation())) {
                NightSharpDebug::Logf("[NightSharp] Menu render crashed; requesting shutdown");
                InterlockedExchange(&g_bShutdown, 1);
            }
        }
        NightSharpPerf::AddPhase(
            "NightSharpMenu::Render",
            NightSharpPerf::MsSince(perfStart));

        NightSharpPerf::RenderOverlay();
        OverlayStatus::Render(OverlayStatus::Mode::External);

        ImGui::EndFrame();
        ImGui::Render();

        g_pd3dContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
        g_pd3dContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        perfStart = NightSharpPerf::Now();
        g_pSwapChain->Present(0, 0);
        NightSharpPerf::AddPhase("Present", NightSharpPerf::MsSince(perfStart));

        const DWORD frameElapsed = GetTickCount() - frameStart;
        if (frameElapsed < kTargetOverlayFrameMs) {
            perfStart = NightSharpPerf::Now();
            Sleep(kTargetOverlayFrameMs - frameElapsed);
            NightSharpPerf::AddPhase("Sleep", NightSharpPerf::MsSince(perfStart));
        }
        NightSharpPerf::EndFrame();
    }

    NightSharpDebug::Phase("overlay-shutdown-plugins");
    SDK::Events::SetDeliveryEnabled(false);
    __try {
        Plugins::PluginBootstrap::Shutdown();
    }
    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                  "Overlay::PluginBootstrap::Shutdown",
                  GetExceptionInformation())) {
        NightSharpDebug::Logf("[NightSharp] Plugin shutdown crashed");
    }

    NightSharpDebug::Phase("overlay-sdk-shutdown");
    __try {
        SDK::Lifecycle::Shutdown();
    }
    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                  "Overlay::SDK::Lifecycle::Shutdown",
                  GetExceptionInformation())) {
        NightSharpDebug::Logf("[NightSharp] SDK shutdown crashed");
    }

    NightSharpDebug::Phase("overlay-imgui-shutdown");
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
    g_hTargetWindow = nullptr;

    InterlockedExchange(&g_bRunning, 0);
    NightSharpDebug::Phase("overlay-cleanup-complete");
    Log("[NightSharp] Overlay cleanup complete\n");
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
