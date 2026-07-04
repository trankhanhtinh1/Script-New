#include "Overlay.h"

#include "MissionInfoOverlay.h"
#include "OverlayManager.h"
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
#include "../Core/CoreSkinChanger.h"
#include "../Core/CoreZoomHack.h"
#include "../CrashReporter.h"
#include "../DebugLog.h"
#include "../FpsDropDebug.h"
#include "../InternalDebugConsole.h"
#include "../SDK/Data/EmbeddedAssets.h"
#include "../SDK/Lifecycle.h"
#include "../SDK/Data/DragonSoulData.h"
#include "../SDK/UI/Drawing.h"
#include "../SDK/UI/Icons.h"

#include <cstdio>
#include <cstdint>
#include <d3d11.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <windowsx.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dcomp.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace MissionInfoOverlay {

namespace {

std::uint8_t FirstSetMissionBit(std::uint8_t mask) {
    for (std::uint8_t result = 1; result < 8; ++result) {
        if ((mask & static_cast<std::uint8_t>(1u << result)) != 0) {
            return result;
        }
    }
    return 0;
}

uintptr_t ReadMissionInfoAddress() {
    uintptr_t missionInfo = CoreRuntime::g_ctx.missionInfo;
    if (Globals::IsValidPtr(missionInfo)) {
        return missionInfo;
    }

    const uintptr_t global = CoreRuntime::g_ctx.missionInfoGlobal;
    return Globals::IsValidPtr(global)
        ? Globals::Read<uintptr_t>(global)
        : 0;
}

void DrawTextWithShadowSized(ImDrawList* draw,
                             const ImVec2& pos,
                             float fontSize,
                             ImU32 color,
                             const char* text) {
    if (!draw || !text || !*text) {
        return;
    }

    ImFont* font = ImGui::GetFont();
    const ImU32 shadow = IM_COL32(0, 0, 0, 230);
    draw->AddText(font, fontSize, ImVec2(pos.x + 2.0f, pos.y + 2.0f), shadow, text);
    draw->AddText(font, fontSize, ImVec2(pos.x - 1.0f, pos.y + 1.0f), shadow, text);
    draw->AddText(font, fontSize, pos, color, text);
}

void EnsureDragonSoulIconsLoaded(const SDK::Data::DragonSoulData::DragonSoulEntry* entry) {
    if (!entry || entry->IconKey.empty()) {
        return;
    }

    const std::string key(entry->IconKey);
    if (SDK::UI::Icons::HasIcon(key)) {
        return;
    }

    for (const auto& asset : SDK::Data::EmbeddedAssets::kDragonSoulIcons) {
        if (asset.Key == entry->IconKey) {
            SDK::UI::Icons::LoadIconFromBytes(
                std::string(asset.Key),
                asset.Data,
                static_cast<int>(asset.Size));
            return;
        }
    }
}

std::uint8_t ReadSelectedElementalTerrainSafe() {
    std::uint8_t terrainId = 0;
    __try {
        const uintptr_t missionInfo = ReadMissionInfoAddress();
        if (Globals::IsValidPtr(missionInfo)) {
            const std::uint8_t mask = Globals::Read<std::uint8_t>(
                missionInfo + Offset::MissionInfo::SelectedElementalTerrain);
            terrainId = FirstSetMissionBit(mask);
        }
    }
    __except (1) {
        terrainId = 0;
    }
    return terrainId;
}

} // namespace

void RenderDragonSoulName() {
    if (!ImGui::GetCurrentContext()) {
        return;
    }

    const std::uint8_t terrainId = ReadSelectedElementalTerrainSafe();

    const auto* soulData = SDK::Data::DragonSoulData::FindByTerrainId(terrainId);
    EnsureDragonSoulIconsLoaded(soulData);

    const char* soulName = "Unknown";
    if (soulData && !soulData->Name.empty()) {
        soulName = soulData->Name.data();
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    if (!draw) {
        return;
    }

    constexpr float kX = 10.0f;
    constexpr float kY = 10.0f;
    constexpr float kIconSize = 34.0f;
    constexpr float kFontSize = 22.0f;
    constexpr float kGap = 8.0f;

    float textX = kX;
    if (soulData && !soulData->IconKey.empty()) {
        const std::string iconKey(soulData->IconKey);
        ImTextureID texture = SDK::UI::Icons::GetIcon(iconKey);
        if (texture && texture != SDK::UI::Icons::GetPlaceholder()) {
            draw->AddImage(
                texture,
                ImVec2(kX, kY),
                ImVec2(kX + kIconSize, kY + kIconSize),
                ImVec2(0.0f, 0.0f),
                ImVec2(1.0f, 1.0f),
                IM_COL32(255, 255, 255, 255));
            textX = kX + kIconSize + kGap;
        }
    }

    char text[64] = {};
    std::snprintf(text, sizeof(text), "DragonSoulName: %s", soulName);
    DrawTextWithShadowSized(
        draw,
        ImVec2(textX, kY + 4.0f),
        kFontSize,
        IM_COL32(255, 220, 80, 255),
        text);
}

} // namespace MissionInfoOverlay

namespace {

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
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
volatile LONG g_bOverlayShown = 0;

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

bool IsTargetForeground();

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

void TickCoreMemoryHacks() {
    __try {
        CoreZoomHack::Tick(Config::ZoomHack::enabled, Config::ZoomHack::maxZoom);
    }
    __except (1) {
    }

    __try {
        CoreSkinChanger::Tick(Config::SkinChanger::enabled, Config::SkinChanger::skinId);
    }
    __except (1) {
    }
}

void RestoreCoreMemoryHacks() {
    __try {
        CoreSkinChanger::Tick(false, Config::SkinChanger::skinId);
    }
    __except (1) {
    }

    __try {
        CoreZoomHack::Tick(false, Config::ZoomHack::maxZoom);
    }
    __except (1) {
    }
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
        const bool targetForeground = IsTargetForeground();
        if (targetForeground && (GetAsyncKeyState(VK_END) & 1)) {
            NightSharpDebug::Logf("[Overlay] VK_END pressed while waiting for game readiness");
            OverlayManager::RequestShutdown();
            return false;
        }
        if (targetForeground && (GetAsyncKeyState(VK_F8) & 1)) {
            NightSharpDebug::Logf("[Overlay] VK_F8 pressed while waiting for game readiness");
            OverlayManager::RequestSwitch();
            return false;
        }
        if (g_hTargetWindow && !IsWindow(g_hTargetWindow)) {
            NightSharpDebug::Logf("[Overlay] Target window became invalid while waiting");
            OverlayManager::RequestShutdown();
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

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
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

    if (FAILED(hr) || !g_pSwapChain || !g_pd3dDevice || !g_pd3dContext) {
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
    HWND riotWindow = FindWindowA("RiotWindowClass", nullptr);
    if (riotWindow) {
        DWORD pid = 0;
        GetWindowThreadProcessId(riotWindow, &pid);
        if (pid == GetCurrentProcessId()) {
            return riotWindow;
        }
    }

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

void SetClickThrough(bool through);

bool IsTargetForeground() {
    if (!g_hTargetWindow || !IsWindow(g_hTargetWindow)) {
        return true;
    }

    HWND foreground = GetForegroundWindow();
    if (!foreground) {
        return false;
    }

    if (foreground == g_hTargetWindow || foreground == g_hOverlay) {
        return true;
    }

    if (GetAncestor(foreground, GA_ROOT) == g_hTargetWindow) {
        return true;
    }

    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(foreground, &foregroundPid);
    return foregroundPid == GetCurrentProcessId();
}

void SetOverlayShown(bool shown) {
    if (!g_hOverlay) {
        return;
    }

    const LONG desired = shown ? 1 : 0;
    if (InterlockedExchange(&g_bOverlayShown, desired) == desired) {
        return;
    }

    ShowWindow(g_hOverlay, shown ? SW_SHOWNOACTIVATE : SW_HIDE);
    NightSharpDebug::Logf("[Overlay] %s external overlay",
                          shown ? "Showing" : "Hiding");
}

bool MoveOverlayToTarget() {
    if (!g_hOverlay) {
        return false;
    }

    if ((g_hTargetWindow && IsIconic(g_hTargetWindow)) || !IsTargetForeground()) {
        SetClickThrough(true);
        SetOverlayShown(false);
        return false;
    }

    RECT rc = {};
    GetOverlayRect(&rc);
    SetOverlayShown(true);
    SetWindowPos(g_hOverlay, HWND_TOP,
        rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    return true;
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
    if (!g_hOverlay) return;
    if (enabled == g_antiCaptureEnabled) return;
    const DWORD affinity = enabled ? 0x00000011u : 0x00000000u;
    if (SetWindowDisplayAffinity(g_hOverlay, affinity)) {
        g_antiCaptureEnabled = enabled;
    }
}

void UpdateClickThroughFromMenuBounds() {
    if (!IsTargetForeground()) {
        SetClickThrough(true);
        return;
    }

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
    const bool acceptMenuInput = g_bMenuVisible != 0 && IsTargetForeground();
    io.MouseDown[0] = acceptMenuInput && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    io.MouseDown[1] = acceptMenuInput && (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
}

void RenderStatusOnlyFrame() {
    if (!g_pd3dContext || !g_pRenderTargetView || !g_pSwapChain) {
        return;
    }

    const bool overlayActive = MoveOverlayToTarget();
    SetClickThrough(overlayActive ? Config::OverlayInput::clickThrough : true);
    if (!overlayActive) {
        return;
    }

    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    SyncImGuiMouse();
    ImGui::NewFrame();
    OverlayStatus::Render(OverlayStatus::Mode::External);
    MissionInfoOverlay::RenderDragonSoulName();
    ImGui::EndFrame();
    ImGui::Render();

    g_pd3dContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
    g_pd3dContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_pSwapChain->Present(0, 0);
}

LRESULT WINAPI OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCHITTEST) {
        if (!IsTargetForeground() || Config::OverlayInput::clickThrough) {
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
    InterlockedExchange(&g_bOverlayShown, 0);
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
        WS_EX_TRANSPARENT |
        WS_EX_NOACTIVATE |
        WS_EX_LAYERED |
        WS_EX_TOOLWINDOW;

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

    SetLayeredWindowAttributes(g_hOverlay, 0, 255, LWA_ALPHA);
    const MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(g_hOverlay, &margins);

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
    SetOverlayShown(true);
    UpdateWindow(g_hOverlay);
    MoveOverlayToTarget();
    RenderStatusOnlyFrame();

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
        const bool overlayActive = MoveOverlayToTarget();
        if (overlayActive) {
            UpdateWindow(g_hOverlay);
        }
        SetClickThrough(overlayActive ? Config::OverlayInput::clickThrough : true);
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
                NightSharpDebug::Logf("[Overlay] WM_QUIT received in external message pump");
                InterlockedExchange(&g_bShutdown, 1);
            }
        }

        if (g_bShutdown) {
            break;
        }

        if (g_hTargetWindow && !IsWindow(g_hTargetWindow)) {
            NightSharpDebug::Logf("[Overlay] Target window became invalid in external frame loop");
            OverlayManager::RequestShutdown();
            break;
        }

        if (g_ResizeW != 0 && g_ResizeH != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeW, g_ResizeH, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeW = 0;
            g_ResizeH = 0;
            CreateRenderTarget();
        }

        const bool overlayActive = MoveOverlayToTarget();
        SetAntiCapture(
            Config::StreamProtection::bypassObs &&
            !SDK::Drawing::HasCaptureVisibleContent());

        if (overlayActive) {
            if ((GetAsyncKeyState(VK_F1) & 1) &&
                InterlockedCompareExchange(&g_bMenuReady, 0, 0) != 0) {
                ToggleMenuVisible();
            }
            if (GetAsyncKeyState(VK_F8) & 1) {
                NightSharpDebug::Logf("[Overlay] VK_F8 pressed in external frame loop");
                OverlayManager::RequestSwitch();
                break;
            }
            if (GetAsyncKeyState(VK_END) & 1) {
                NightSharpDebug::Logf("[Overlay] VK_END pressed in external frame loop");
                OverlayManager::RequestShutdown();
                break;
            }
            NightSharpPerf::ToggleHotkeys();
            NightSharpDebug::InternalConsole::ToggleHotkey();
            UpdateClickThroughFromMenuBounds();
        } else {
            SetClickThrough(true);
        }

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
        TickCoreMemoryHacks();
        NightSharpPerf::AddPhase(
            "CoreMemoryHacks::Tick",
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
        SDK::Drawing::DispatchDraw();
        NightSharpPerf::AddPhase(
            "SDK::Drawing::DispatchDraw",
            NightSharpPerf::MsSince(perfStart));

        perfStart = NightSharpPerf::Now();
        SDK::Drawing::DispatchEndScene();
        NightSharpPerf::AddPhase(
            "SDK::Drawing::DispatchEndScene",
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
        NightSharpDebug::InternalConsole::Render();
        OverlayStatus::Render(OverlayStatus::Mode::External);
        MissionInfoOverlay::RenderDragonSoulName();

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

    NightSharpDebug::Logf(
        "[Overlay] external render loop exited localShutdown=%ld switch=%d",
        InterlockedCompareExchange(&g_bShutdown, 0, 0),
        OverlayManager::IsSwitchRequested() ? 1 : 0);

    NightSharpDebug::Phase("overlay-shutdown-plugins");
    RestoreCoreMemoryHacks();
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
    g_antiCaptureEnabled = false;

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
