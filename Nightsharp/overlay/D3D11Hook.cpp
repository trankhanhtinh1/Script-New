#include "D3D11Hook.h"
#include "MissionInfoOverlay.h"
#include "OverlayStatus.h"
#include "OverlayManager.h"
#include "VmtHook.h"

#include "../crt_shim.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx11.h"
#include "../imgui/imgui_impl_win32.h"
#include "../menu/MenuConfig.h"
#include "../menu/NightSharpMenu.h"
#include "../Plugins/PluginBootstrap.h"
#include "../Plugins/PluginManager.h"
#include "../Core/CoreRuntime.h"
#include "../Core/CoreEvents.h"
#include "../Core/CoreSkinChanger.h"
#include "../Core/CoreZoomHack.h"
#include "../CrashReporter.h"
#include "../CrashTrace.h"
#include "../DebugLog.h"
#include "../FpsDropDebug.h"
#include "../SDK/Lifecycle.h"
#include "../SDK/Core/Game.h"
#include "../SDK/Events/Events.h"
#include "../SDK/UI/Icons.h"
#include "../SDK/UI/UI.h"
#include "../SDK/UI/Drawing.h"

#include <d3d11.h>
#include <dxgi.h>
#include <cstdlib>
#include <new>

template <typename T>
static inline void SafeRelease(T*& ptr) {
    if (ptr) { ptr->Release(); ptr = nullptr; }
}

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace D3D11Hook {

// -----------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------
ID3D11Device*           g_pd3dDevice       = nullptr;
ID3D11DeviceContext*    g_pd3dContext       = nullptr;
IDXGISwapChain*         g_pSwapChain       = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

static VmtHook*         g_vmtHook          = nullptr;
static HWND             g_gameHwnd         = nullptr;
static WNDPROC          g_originalWndProc  = nullptr;
static bool             g_active           = false;
static volatile LONG    g_initDeviceState  = 0; // 0=not started, 1=initializing, 2=done
static volatile LONG    g_shutdown         = 0;
static volatile LONG    g_uninstalling     = 0;
static volatile LONG    g_hookCalls        = 0;
static volatile LONG    g_menuReady        = 0;
static volatile LONG    g_imguiContextReady = 0;
static volatile LONG    g_imguiBackendReady = 0;
static volatile LONG    g_zoomTickFaultLogged = 0;
static volatile LONG    g_skinTickFaultLogged = 0;
static volatile LONG    g_zoomRestoreFaultLogged = 0;
static volatile LONG    g_skinRestoreFaultLogged = 0;
// Set to 1 once PluginBootstrap::EnsureRegistered() has returned (success or crash).
// Render() gates all SDK/plugin calls on this flag to prevent the bootstrap race.
static volatile LONG    g_bootstrapDone    = 0;

static constexpr DWORD kMenuStartDelayAfterLocalPlayerMs = 3000;
static constexpr DWORD kGameReadyPollMs = 500;
static constexpr int kGameReadyMaxPolls = 240;
static constexpr DWORD kSwapChainPollMs = 500;
static constexpr int kSwapChainMaxPolls = 240;

static bool ReadLocalPlayerForStartup(uintptr_t& outLocalPlayer) {
    outLocalPlayer = 0;
    __try {
        if (!CoreRuntime::EnsureInitialized()) {
            return false;
        }
        (void)CoreRuntime::RefreshReadState();
        const auto& ctx = CoreRuntime::GetContext();
        outLocalPlayer = ctx.localPlayer;
        if ((ctx.statusMask & CoreRuntime::Status_RuntimeObjectsReady) == 0) {
            return false;
        }
    }
    __except (1) {
        outLocalPlayer = 0;
        return false;
    }
    return outLocalPlayer != 0;
}

struct HookCallScope {
    HookCallScope() {
        InterlockedIncrement(&g_hookCalls);
    }

    ~HookCallScope() {
        InterlockedDecrement(&g_hookCalls);
    }
};

static bool IsUnloading() {
    return InterlockedCompareExchange(&g_shutdown, 0, 0) != 0 ||
           InterlockedCompareExchange(&g_uninstalling, 0, 0) != 0;
}

static void WaitForHookCallsToDrain(DWORD timeoutMs = 2000) {
    const DWORD start = GetTickCount();
    while (InterlockedCompareExchange(&g_hookCalls, 0, 0) != 0 &&
           GetTickCount() - start < timeoutMs) {
        Sleep(1);
    }
}

// -----------------------------------------------------------------------
// Swapchain acquisition via the renderer runtime object
// -----------------------------------------------------------------------
static IDXGISwapChain* FindSwapChainFromRenderer(bool noisy = true) {
    if (!CoreRuntime::EnsureInitialized() || !CoreRuntime::RefreshReadState()) {
        if (noisy) {
            NightSharpDebug::Logf("[D3D11Hook] CoreRuntime is not ready for renderer lookup");
        }
        return nullptr;
    }

    const auto& ctx = CoreRuntime::GetContext();
    const uintptr_t renderer = ctx.renderer;
    const uintptr_t swapChainSlot = renderer + Offset::D3D::SwapChain;
    if (!Globals::IsValidPtr(renderer) ||
        !Globals::IsReadablePtr(swapChainSlot, sizeof(uintptr_t))) {
        if (noisy) {
            NightSharpDebug::Logf(
                "[D3D11Hook] Renderer is not ready global=%p renderer=%p",
                reinterpret_cast<void*>(ctx.rendererGlobal),
                reinterpret_cast<void*>(renderer));
        }
        return nullptr;
    }

    auto* swapChain = reinterpret_cast<IDXGISwapChain*>(
        Globals::ReadPtr(swapChainSlot));
    if (!swapChain ||
        !Globals::IsReadablePtr(reinterpret_cast<uintptr_t>(swapChain), sizeof(uintptr_t))) {
        if (noisy) {
            NightSharpDebug::Logf(
                "[D3D11Hook] Renderer swapchain is not ready renderer=%p slot=+0x%X ptr=%p",
                reinterpret_cast<void*>(renderer),
                static_cast<unsigned>(Offset::D3D::SwapChain),
                static_cast<void*>(swapChain));
        }
        return nullptr;
    }

    const uintptr_t vtable = Globals::ReadPtr(reinterpret_cast<uintptr_t>(swapChain));
    constexpr size_t kRequiredVtableEntries = 14;
    if (!Globals::IsReadablePtr(vtable, sizeof(uintptr_t) * kRequiredVtableEntries)) {
        if (noisy) {
            NightSharpDebug::Logf(
                "[D3D11Hook] Renderer swapchain has an invalid vtable ptr=%p vtable=%p",
                static_cast<void*>(swapChain),
                reinterpret_cast<void*>(vtable));
        }
        return nullptr;
    }

    const uintptr_t presentFn = Globals::ReadPtr(
        vtable + Offset::D3D::PresentVtableOffset);
    const uintptr_t resizeBuffersFn = Globals::ReadPtr(
        vtable + Offset::D3D::ResizeBuffersVtableOffset);
    if (!Globals::IsExecutablePtr(presentFn) ||
        !Globals::IsExecutablePtr(resizeBuffersFn)) {
        if (noisy) {
            NightSharpDebug::Logf(
                "[D3D11Hook] Renderer swapchain methods are invalid present=%p resize=%p",
                reinterpret_cast<void*>(presentFn),
                reinterpret_cast<void*>(resizeBuffersFn));
        }
        return nullptr;
    }

    DXGI_SWAP_CHAIN_DESC desc = {};
    HRESULT hr = E_FAIL;
    __try {
        hr = swapChain->GetDesc(&desc);
    }
    __except (1) {
        if (noisy) {
            NightSharpDebug::Logf("[D3D11Hook] Renderer swapchain GetDesc faulted ptr=%p",
                                  static_cast<void*>(swapChain));
        }
        return nullptr;
    }

    if (FAILED(hr) || !desc.OutputWindow || !IsWindow(desc.OutputWindow)) {
        if (noisy) {
            NightSharpDebug::Logf(
                "[D3D11Hook] Renderer swapchain GetDesc invalid hr=0x%08lX hwnd=%p ptr=%p",
                static_cast<unsigned long>(hr),
                desc.OutputWindow,
                static_cast<void*>(swapChain));
        }
        return nullptr;
    }

    NightSharpDebug::Logf(
        "[D3D11Hook] Found swapchain via renderer=%p slot=+0x%X ptr=%p hwnd=%p",
        reinterpret_cast<void*>(renderer),
        static_cast<unsigned>(Offset::D3D::SwapChain),
        static_cast<void*>(swapChain),
        desc.OutputWindow);
    return swapChain;
}

static IDXGISwapChain* WaitForSwapChain() {
    NightSharpDebug::Phase("d3d11hook-wait-swapchain");
    NightSharpDebug::Logf("[D3D11Hook] Waiting for game swapchain...");

    for (int i = 0; i < kSwapChainMaxPolls; ++i) {
        if (g_shutdown) {
            return nullptr;
        }
        if (g_gameHwnd && !IsWindow(g_gameHwnd)) {
            InterlockedExchange(&g_shutdown, 1);
            return nullptr;
        }

        IDXGISwapChain* swapChain = FindSwapChainFromRenderer(i == 0);
        if (swapChain) {
            if (i > 0) {
                NightSharpDebug::Logf("[D3D11Hook] Swapchain became ready after %d sec",
                                      static_cast<int>((i * kSwapChainPollMs) / 1000));
            }
            return swapChain;
        }

        if (i > 0 && (i % 20) == 0) {
            NightSharpDebug::Logf("[D3D11Hook] Still waiting for swapchain (%d sec)",
                                  static_cast<int>((i * kSwapChainPollMs) / 1000));
        }
        Sleep(kSwapChainPollMs);
    }

    NightSharpDebug::Logf("[D3D11Hook] Swapchain wait timed out");
    return nullptr;
}

// -----------------------------------------------------------------------
// Render target
// -----------------------------------------------------------------------
static bool CreateRenderTargetInternal() {
    if (!g_pSwapChain || !g_pd3dDevice) return false;

    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr) || !backBuffer) return false;

    hr = g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_pRenderTargetView);
    backBuffer->Release();
    return SUCCEEDED(hr) && g_pRenderTargetView != nullptr;
}

static void CleanupRenderTargetInternal() {
    if (g_pRenderTargetView) {
        g_pRenderTargetView->Release();
        g_pRenderTargetView = nullptr;
    }
}

// -----------------------------------------------------------------------
// WndProc hook
// -----------------------------------------------------------------------
static LRESULT WINAPI WndProcHook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (IsUnloading()) {
        return g_originalWndProc
            ? CallWindowProcW(g_originalWndProc, hWnd, msg, wParam, lParam)
            : DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    // Track window focus from the message stream so IsGameFocused() never has to
    // poll GetForegroundWindow() (a USER32 call that intermittently blocks for
    // several ms) on the per-tick Key()/ShouldProcessInput() hot path.
    if (msg == WM_ACTIVATEAPP) {
        ::CoreGame::SetWindowFocused(wParam != FALSE);
        if (wParam == FALSE) {
            NightSharpMenu::ResetMouseInputCapture();
            NightSharpMenu::EnsoulSharpTheme::CancelRootDrag();
        }
    } else if (msg == WM_CANCELMODE || msg == WM_KILLFOCUS) {
        NightSharpMenu::ResetMouseInputCapture();
        NightSharpMenu::EnsoulSharpTheme::CancelRootDrag();
    }

    if (NightSharpMenu::showMenu &&
        SDK::UI::MenuManager::Instance().DispatchCapturedInput(msg, wParam, lParam)) {
        return TRUE;
    }

    // Raw mouse input bypasses the normal WM_*BUTTON messages on some game
    // input paths.  Consume only mouse packets owned by the visible menu and
    // still let DefWindowProc perform the required foreground-input cleanup.
    if (msg == WM_INPUT &&
        NightSharpMenu::ShouldCaptureRawMouseInput(hWnd, lParam)) {
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    // Let ImGui process input first
    if (ImGui::GetCurrentContext() &&
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return TRUE;
    }

    // The EnsoulSharp renderer uses the foreground draw list instead of ImGui
    // widgets, so WantCaptureMouse is not sufficient.  Consume pointer input
    // over one of its actual panels to keep the same click-through behavior as
    // the external overlay and prevent a menu click from reaching the game.
    if (NightSharpMenu::showMenu && msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) {
        POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
            ScreenToClient(hWnd, &point);
        }
        if (NightSharpMenu::ShouldCaptureMouseMessage(
                msg,
                wParam,
                static_cast<float>(point.x),
                static_cast<float>(point.y))) {
            return TRUE;
        }
    }

    // Handle keyboard shortcuts
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_F1) {
            if (InterlockedCompareExchange(&g_menuReady, 0, 0) == 0) {
                return TRUE;
            }
            NightSharpMenu::showMenu = !NightSharpMenu::showMenu;
            if (!NightSharpMenu::showMenu) {
                NightSharpMenu::ResetMouseInputCapture();
                NightSharpMenu::EnsoulSharpTheme::CancelRootDrag();
            }
            return TRUE;
        }
        if (wParam == VK_F8) {
            NightSharpDebug::Logf("[D3D11Hook] VK_F8 received in internal WndProc");
            OverlayManager::RequestSwitch();
            return TRUE;
        }
        if (wParam == VK_END) {
            NightSharpDebug::Logf("[D3D11Hook] VK_END received in internal WndProc");
            OverlayManager::RequestShutdown();
            return TRUE;
        }
    }

    // Dispatch to SDK WndProc subscribers (cursor, target selector, chat, plugins, etc.)
    const bool processInput =
        SDK::Game::DispatchWndProc(hWnd, msg, wParam, lParam);

    // Block keybind activation while the chat box is open — typing must not
    // toggle keybinds (Fly Hack / Auto W) or hold the combo key.
    SDK::UI::g_KeybindInputBlocked = SDK::Game::IsChatOpen();

    // Dispatch key/mouse events to MenuKeyBind state machines.
    SDK::UI::MenuManager::Instance().DispatchInput(msg, wParam, lParam);

    if (!processInput) return TRUE;
    // Forward to original game WndProc
    return g_originalWndProc
        ? CallWindowProcW(g_originalWndProc, hWnd, msg, wParam, lParam)
        : DefWindowProcW(hWnd, msg, wParam, lParam);
}

static bool HookWndProc(HWND hWnd) {
    if (!hWnd) return false;

    // Get current WndProc (the game's)
    LONG_PTR current = GetWindowLongPtrW(hWnd, GWLP_WNDPROC);
    if (!current) return false;

    // Set our hook as the new WndProc
    LONG_PTR result = SetWindowLongPtrW(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProcHook));
    if (!result) return false;

    g_originalWndProc = reinterpret_cast<WNDPROC>(current);
    g_gameHwnd = hWnd;

    // Seed focus state once (injection happens in the active game) so IsGameFocused()
    // uses the message-driven flag immediately; WM_ACTIVATEAPP keeps it accurate.
    {
        DWORD pid = 0;
        const HWND fg = GetForegroundWindow();
        if (fg) {
            GetWindowThreadProcessId(fg, &pid);
        }
        ::CoreGame::SetWindowFocused(fg == hWnd || pid == GetCurrentProcessId());
    }

    NightSharpDebug::Logf("[D3D11Hook] WndProc hooked: original=0x%p new=0x%p",
                          (void*)current, (void*)WndProcHook);
    return true;
}

static void UnhookWndProc() {
    if (g_gameHwnd && g_originalWndProc) {
        SetWindowLongPtrW(g_gameHwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWndProc));
        g_originalWndProc = nullptr;
    }
}

// -----------------------------------------------------------------------
// ImGui initialization
// -----------------------------------------------------------------------
static void CleanupFailedImGuiInit(bool win32Started, bool dx11Started) {
    if (dx11Started) {
        ImGui_ImplDX11_Shutdown();
    }
    if (win32Started) {
        ImGui_ImplWin32_Shutdown();
    }

    CleanupRenderTargetInternal();
    SafeRelease(g_pd3dContext);
    SafeRelease(g_pd3dDevice);
    g_pSwapChain = nullptr;

    if (InterlockedCompareExchange(&g_imguiContextReady, 0, 0) != 0 &&
        ImGui::GetCurrentContext()) {
        ImGui::DestroyContext();
        InterlockedExchange(&g_imguiContextReady, 0);
    }
    InterlockedExchange(&g_imguiBackendReady, 0);
}

static bool InitImGui(IDXGISwapChain* swapChain) {
    NightSharpDebug::Phase("d3d11hook-imgui-init");
    if (!swapChain || !g_gameHwnd || !IsWindow(g_gameHwnd)) {
        NightSharpDebug::Logf("[D3D11Hook] ImGui init skipped: invalid swapchain/window");
        return false;
    }

    NsHeapInit();
    ImGui::SetAllocatorFunctions(NsImGuiAlloc, NsImGuiFree, nullptr);
    ImGui::CreateContext();
    InterlockedExchange(&g_imguiContextReady, 1);
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    ImFontConfig menuFontConfig;
    menuFontConfig.OversampleH = 3;
    menuFontConfig.OversampleV = 2;
    menuFontConfig.PixelSnapH = true;
    menuFontConfig.RasterizerMultiply = 1.1f;
    io.Fonts->AddFontDefault(&menuFontConfig);
    NightSharpMenu::EnsoulSharpTheme::LoadFonts(io, NightSharpMenu::EnsoulSharpTheme::FontSize);

    ImFontConfig permaFontConfig;
    permaFontConfig.OversampleH = 3;
    permaFontConfig.OversampleV = 2;
    permaFontConfig.PixelSnapH = true;
    ImFont* permaFont = io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\tahoma.ttf",
        16.0f,
        &permaFontConfig,
        NightSharpMenu::EnsoulSharpTheme::GlyphRanges);
    SDK::UI::PermaShow::SetFont(
        permaFont ? permaFont : NightSharpMenu::EnsoulSharpTheme::Font());

    g_pSwapChain = swapChain;
    HRESULT hr = g_pSwapChain->GetDevice(
        __uuidof(ID3D11Device),
        reinterpret_cast<void**>(&g_pd3dDevice));
    if (FAILED(hr) || !g_pd3dDevice) {
        NightSharpDebug::Logf("[D3D11Hook] ImGui init failed: GetDevice hr=0x%08lX",
                              static_cast<unsigned long>(hr));
        CleanupFailedImGuiInit(false, false);
        return false;
    }

    g_pd3dDevice->GetImmediateContext(&g_pd3dContext);
    if (!g_pd3dContext) {
        NightSharpDebug::Logf("[D3D11Hook] ImGui init failed: null immediate context");
        CleanupFailedImGuiInit(false, false);
        return false;
    }

    if (!CreateRenderTargetInternal()) {
        NightSharpDebug::Logf("[D3D11Hook] Failed to create render target");
        CleanupFailedImGuiInit(false, false);
        return false;
    }

    bool win32Started = false;
    if (!ImGui_ImplWin32_Init(g_gameHwnd)) {
        NightSharpDebug::Logf("[D3D11Hook] ImGui Win32 backend init failed");
        CleanupFailedImGuiInit(false, false);
        return false;
    }
    win32Started = true;

    if (!ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext)) {
        NightSharpDebug::Logf("[D3D11Hook] ImGui DX11 backend init failed");
        CleanupFailedImGuiInit(win32Started, false);
        return false;
    }

    ImGui_ImplDX11_CreateDeviceObjects();
    SDK::UI::Icons::SetDevice(g_pd3dDevice, g_pd3dContext);
    InterlockedExchange(&g_imguiBackendReady, 1);

    NightSharpDebug::Logf("[D3D11Hook] ImGui initialized with game device");
    return true;
}

static void EnsureImGuiInitialized(IDXGISwapChain* swapChain) {
    const LONG state = InterlockedCompareExchange(&g_initDeviceState, 1, 0);
    if (state != 0) {
        return;
    }

    const bool backendReady =
        !IsUnloading() &&
        InitImGui(swapChain) &&
        InterlockedCompareExchange(&g_imguiBackendReady, 0, 0) != 0;
    InterlockedExchange(&g_initDeviceState, backendReady ? 2 : 0);
}

// -----------------------------------------------------------------------
// Rendering
// -----------------------------------------------------------------------
static void RenderMenuSafe() {
    __try {
        NightSharpMenu::Render();
    }
    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                  "D3D11Hook::NightSharpMenu::Render",
                  GetExceptionInformation())) {
        NightSharpDebug::Logf("[D3D11Hook] Menu render crashed");
    }
}

static LONG LogOnceAndContinue(const char* stage,
                               volatile LONG* flag,
                               EXCEPTION_POINTERS* exceptionPointers) {
    if (flag && InterlockedCompareExchange(flag, 1, 0) == 0) {
        NightSharpDebug::LogException(stage, exceptionPointers);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

static void TickCoreMemoryHacks() {
    __try {
        CoreZoomHack::Tick(Config::ZoomHack::enabled, Config::ZoomHack::maxZoom);
    }
    __except (LogOnceAndContinue(
                  "D3D11Hook::CoreZoomHack::Tick",
                  &g_zoomTickFaultLogged,
                  GetExceptionInformation())) {
    }

    __try {
        CoreSkinChanger::Tick(Config::SkinChanger::enabled, Config::SkinChanger::skinId);
    }
    __except (LogOnceAndContinue(
                  "D3D11Hook::CoreSkinChanger::Tick",
                  &g_skinTickFaultLogged,
                  GetExceptionInformation())) {
    }
}

static void RestoreCoreMemoryHacks() {
    __try {
        CoreSkinChanger::Tick(false, Config::SkinChanger::skinId);
    }
    __except (LogOnceAndContinue(
                  "D3D11Hook::CoreSkinChanger::Restore",
                  &g_skinRestoreFaultLogged,
                  GetExceptionInformation())) {
    }

    __try {
        CoreZoomHack::Tick(false, Config::ZoomHack::maxZoom);
    }
    __except (LogOnceAndContinue(
                  "D3D11Hook::CoreZoomHack::Restore",
                  &g_zoomRestoreFaultLogged,
                  GetExceptionInformation())) {
    }
}

static void RenderStatusOverlaysSafe(OverlayStatus::Mode mode) {
    __try {
        OverlayStatus::Render(mode);
    }
    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                  "D3D11Hook::OverlayStatus::Render",
                  GetExceptionInformation())) {
        NightSharpDebug::Logf("[D3D11Hook] Overlay status render crashed");
    }

    __try {
        MissionInfoOverlay::RenderDragonSoulName();
    }
    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                  "D3D11Hook::MissionInfoOverlay::RenderDragonSoulName",
                  GetExceptionInformation())) {
        NightSharpDebug::Logf("[D3D11Hook] Mission info overlay render crashed");
    }
}

static void Render() {
    if (IsUnloading() ||
        InterlockedCompareExchange(&g_imguiBackendReady, 0, 0) == 0 ||
        !g_pd3dContext ||
        !g_pRenderTargetView)
        return;

    NightSharpDebug::SetPhase("d3d11hook-render-begin");
    NightSharpDebug::CrashTrace::Record(
        nscrash::TraceTag::D3DUpdate,
        g_bootstrapDone ? 1u : 0u);
    NightSharpPerf::BeginFrame();

    NightSharpDebug::SetPhase("d3d11hook-imgui-newframe");
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Guard: do not touch game memory or plugins until PluginBootstrap has fully
    // completed on the install thread. Without this, TickRead/OnUpdate can dereference
    // SDK singletons that haven't been initialized yet, causing ACCESS_VIOLATION.
    if (g_bootstrapDone) {
        // Read game state once per frame
        {
            NightSharpDebug::SetPhase("d3d11hook-render-tickread");
            NightSharpPerf::ScopedTimer timer("CoreRuntime::TickRead");
            CoreRuntime::TickRead();
        }
        {
            NightSharpDebug::SetPhase("d3d11hook-render-memory-hacks");
            NightSharpPerf::ScopedTimer timer("CoreMemoryHacks::Tick");
            TickCoreMemoryHacks();
        }

        // Plugin update + render
        {
            NightSharpDebug::SetPhase("d3d11hook-render-plugin-update");
            NightSharpPerf::ScopedTimer timer("PluginManager::OnUpdate");
            Plugins::PluginManager::Get().OnUpdate();
        }
        {
            NightSharpDebug::SetPhase("d3d11hook-render-plugin-render");
            NightSharpPerf::ScopedTimer timer("PluginManager::OnRender");
            Plugins::PluginManager::Get().OnRender();
        }

        // SDK Drawing handlers (TargetSelector, Orbwalker, Core Render objects)
        {
            NightSharpDebug::SetPhase("d3d11hook-render-drawing-dispatch");
            NightSharpPerf::ScopedTimer timer("SDK::Drawing::DispatchDraw");
            SDK::Drawing::DispatchDraw();
        }
        {
            NightSharpDebug::SetPhase("d3d11hook-render-endscene-dispatch");
            NightSharpPerf::ScopedTimer timer("SDK::Drawing::DispatchEndScene");
            SDK::Drawing::DispatchEndScene();
        }

        // Menu + PermaShow render
        {
            NightSharpDebug::SetPhase("d3d11hook-render-menu");
            NightSharpPerf::ScopedTimer timer("NightSharpMenu::Render");
            RenderMenuSafe();
        }
    }

    NightSharpDebug::SetPhase("d3d11hook-render-status-overlays");
    RenderStatusOverlaysSafe(OverlayStatus::Mode::Internal);

    NightSharpDebug::SetPhase("d3d11hook-imgui-render");
    ImGui::EndFrame();
    ImGui::Render();

    NightSharpDebug::SetPhase("d3d11hook-dx11-submit");
    g_pd3dContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    NightSharpPerf::EndFrame();
    NightSharpDebug::SetPhase("d3d11hook-render-idle");
}

static void PresentFrameSafe(IDXGISwapChain* pSwapChain) {
    __try {
        NightSharpDebug::SetPhase("d3d11hook-present-frame");
        EnsureImGuiInitialized(pSwapChain);
        Render();
    }
    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                  "D3D11Hook::PresentFrame",
                  GetExceptionInformation())) {
        NightSharpDebug::Logf("[D3D11Hook] Present frame crashed; disabling internal overlay");
        InterlockedExchange(&g_shutdown, 1);
    }
}

// -----------------------------------------------------------------------
// VTable hooks
// -----------------------------------------------------------------------
struct DxgiPresent {
    static long WINAPI Hooked(IDXGISwapChain* pSwapChain, UINT syncInterval, UINT flags) {
        HookCallScope scope;
        NightSharpDebug::CrashTrace::Record(
            nscrash::TraceTag::D3DPresent,
            syncInterval,
            flags);
        if (!IsUnloading()) {
            PresentFrameSafe(pSwapChain);
        }
        NightSharpPerf::ScopedTimer timer("Present");
        return m_original(pSwapChain, syncInterval, flags);
    }
    static decltype(&Hooked) m_original;
};
decltype(DxgiPresent::m_original) DxgiPresent::m_original;

struct DxgiResizeBuffers {
    static long WINAPI Hooked(IDXGISwapChain* pSwapChain,
                              UINT bufferCount, UINT width, UINT height,
                              DXGI_FORMAT newFormat, UINT swapChainFlags)
{
        HookCallScope scope;
        if (IsUnloading()) {
            return m_original(pSwapChain, bufferCount, width, height, newFormat, swapChainFlags);
        }

        CleanupRenderTargetInternal();
        auto hr = m_original(pSwapChain, bufferCount, width, height, newFormat, swapChainFlags);
        if (!IsUnloading()) {
            CreateRenderTargetInternal();
        }
        return hr;
    }
    static decltype(&Hooked) m_original;
};
decltype(DxgiResizeBuffers::m_original) DxgiResizeBuffers::m_original;

// -----------------------------------------------------------------------
// SEH-safe helpers (no C++ objects with destructors in these)
// -----------------------------------------------------------------------
static bool BootstrapPluginsSafe() {
    bool ok = false;
    SDK::Events::SetDeliveryEnabled(false);
    InterlockedExchange(&g_bootstrapDone, 0);

    __try {
        Plugins::PluginBootstrap::EnsureRegistered();
        ok = true;
    }
    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                  "D3D11Hook::PluginBootstrap",
                  GetExceptionInformation())) {
        NightSharpDebug::Logf("[D3D11Hook] Plugin bootstrap crashed");
    }

    if (ok) {
        SDK::Events::SetDeliveryEnabled(true);
        // Signal Render() that it is now safe to call TickRead/OnUpdate/OnRender.
        InterlockedExchange(&g_bootstrapDone, 1);
        return true;
    }

    SDK::Events::SetDeliveryEnabled(false);
    __try {
        Plugins::PluginBootstrap::Shutdown();
    }
    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                  "D3D11Hook::PluginBootstrap::CleanupAfterCrash",
                  GetExceptionInformation())) {
    }
    __try {
        SDK::Events::Reset();
    }
    __except (1) {
    }
    return false;
}

static void ShutdownPluginsSafe() {
    SDK::Events::SetDeliveryEnabled(false);
    __try {
        Plugins::PluginBootstrap::Shutdown();
    }
    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                  "D3D11Hook::PluginBootstrap::Shutdown",
                  GetExceptionInformation())) {
    }
}

static void ShutdownCoreEventsSafe() {
    __try {
        Core::Events::Shutdown();
    }
    __except (1) {
    }
}

static HWND FindGameWindow() {
    struct { DWORD pid; HWND hwnd; } data = { GetCurrentProcessId(), nullptr };
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* d = reinterpret_cast<decltype(data)*>(lParam);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != d->pid) return TRUE;
        if (!IsWindowVisible(hwnd)) return TRUE;
        if (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) return TRUE;
        RECT rc = {};
        GetWindowRect(hwnd, &rc);
        if ((rc.right - rc.left) <= 0 || (rc.bottom - rc.top) <= 0) return TRUE;
        d->hwnd = hwnd;
        return FALSE;
    }, reinterpret_cast<LPARAM>(&data));
    return data.hwnd;
}

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------
bool Install() {
    if (g_active) return true;

    NightSharpDebug::Phase("d3d11hook-install");
    NightSharpDebug::Logf("[D3D11Hook] Installing D3D11 swapchain hooks...");
    InterlockedExchange(&g_initDeviceState, 0);
    InterlockedExchange(&g_shutdown, 0);
    InterlockedExchange(&g_uninstalling, 0);
    InterlockedExchange(&g_hookCalls, 0);
    InterlockedExchange(&g_menuReady, 0);
    InterlockedExchange(&g_imguiContextReady, 0);
    InterlockedExchange(&g_imguiBackendReady, 0);
    InterlockedExchange(&g_bootstrapDone, 0);
    NightSharpMenu::showMenu = false;

    // Retry FindGameWindow: the game window may not be visible/owned by the render
    // thread yet when the DLL is injected. Retry up to 10 times with 100ms gaps.
    HWND gameHwnd = nullptr;
    for (int retry = 0; retry < 10 && !gameHwnd; ++retry) {
        gameHwnd = FindGameWindow();
        if (!gameHwnd) {
            NightSharpDebug::Logf("[D3D11Hook] Game window not found, retry %d/10...", retry + 1);
            Sleep(100);
        }
    }
    if (!gameHwnd) {
        NightSharpDebug::Logf("[D3D11Hook] Could not find game window after retries");
        return false;
    }

    // Retry HookWndProc: SetWindowLongPtrW returns 0 (fails) when the window's
    // message pump isn't fully owned by its thread yet. This is a timing issue
    // at injection time and resolves within a few hundred milliseconds.
    bool wndHooked = false;
    for (int retry = 0; retry < 10 && !wndHooked; ++retry) {
        wndHooked = HookWndProc(gameHwnd);
        if (!wndHooked) {
            NightSharpDebug::Logf("[D3D11Hook] WndProc hook failed, retry %d/10...", retry + 1);
            Sleep(100);
        }
    }
    if (!wndHooked) {
        NightSharpDebug::Logf("[D3D11Hook] Failed to hook WndProc after retries");
        return false;
    }

    IDXGISwapChain* swapChain = WaitForSwapChain();
    if (!swapChain) {
        NightSharpDebug::Logf("[D3D11Hook] Failed to find swapchain after wait, unhooking WndProc");
        UnhookWndProc();
        return g_shutdown != 0;
    }

    g_vmtHook = new (std::nothrow) VmtHook(swapChain);
    if (!g_vmtHook) {
        NightSharpDebug::Logf("[D3D11Hook] Failed to allocate VmtHook");
        UnhookWndProc();
        return false;
    }

    DxgiPresent::m_original = g_vmtHook->Hook<decltype(DxgiPresent::m_original)>(8, &DxgiPresent::Hooked);
    DxgiResizeBuffers::m_original = g_vmtHook->Hook<decltype(DxgiResizeBuffers::m_original)>(13, &DxgiResizeBuffers::Hooked);

    CoreRuntime::EnsureInitialized();

    g_active = true;
    NightSharpDebug::Phase("d3d11hook-installed");
    NightSharpDebug::Logf("[D3D11Hook] D3D11 swapchain hooks installed (Present=8, ResizeBuffers=13)");

    // Wait until local player is readable, then delay three wall-clock seconds
    // before showing the menu/loading plugins.
    NightSharpDebug::Phase("d3d11hook-wait-game");
    NightSharpDebug::Logf("[D3D11Hook] Waiting for localPlayer read + %.1fs...",
                          static_cast<double>(kMenuStartDelayAfterLocalPlayerMs) / 1000.0);

    bool sawLocalPlayer = false;
    DWORD firstLocalPlayerTick = 0;
    uintptr_t firstLocalPlayer = 0;
    for (int i = 0; i < kGameReadyMaxPolls; ++i) {
        if (g_shutdown) {
            Uninstall();
            return true;
        }

        uintptr_t localPlayer = 0;
        if (ReadLocalPlayerForStartup(localPlayer)) {
            if (!sawLocalPlayer) {
                sawLocalPlayer = true;
                firstLocalPlayerTick = GetTickCount();
                firstLocalPlayer = localPlayer;
                NightSharpDebug::Logf("[D3D11Hook] First localPlayer read %p; delaying menu/plugins %.1fs",
                                      reinterpret_cast<void*>(localPlayer),
                                      static_cast<double>(kMenuStartDelayAfterLocalPlayerMs) / 1000.0);
            }

            const DWORD elapsedMs = GetTickCount() - firstLocalPlayerTick;

            if (elapsedMs >= kMenuStartDelayAfterLocalPlayerMs) {
                NightSharpDebug::Logf("[D3D11Hook] LocalPlayer ready %p (elapsed %.2fs), bootstrapping plugins",
                                      reinterpret_cast<void*>(localPlayer),
                                      static_cast<double>(elapsedMs) / 1000.0);
                NightSharpDebug::Phase("d3d11hook-plugins");
                if (!BootstrapPluginsSafe()) {
                    NightSharpDebug::Logf("[D3D11Hook] Plugin bootstrap failed; shutting down hook");
                    InterlockedExchange(&g_shutdown, 1);
                    Uninstall();
                    return false;
                }
                NightSharpMenu::showMenu = true;
                InterlockedExchange(&g_menuReady, 1);
                while (!g_shutdown) {
                    Sleep(250);
                }
                Uninstall();
                return true;
            }
        }

        if (i > 0 && (i % 20) == 0) {
            if (sawLocalPlayer) {
                const DWORD elapsedMs = GetTickCount() - firstLocalPlayerTick;
                NightSharpDebug::Logf("[D3D11Hook] Still delaying menu/plugins: localPlayer=%p first=%p elapsed=%.2fs",
                                      reinterpret_cast<void*>(localPlayer),
                                      reinterpret_cast<void*>(firstLocalPlayer),
                                      static_cast<double>(elapsedMs) / 1000.0);
            } else {
                NightSharpDebug::Logf("[D3D11Hook] Still waiting for first localPlayer read (%d sec)",
                                      static_cast<int>((i * kGameReadyPollMs) / 1000));
            }
        }
        Sleep(kGameReadyPollMs);
    }

    NightSharpDebug::Logf("[D3D11Hook] LocalPlayer wait timed out, bootstrapping anyway");
    NightSharpDebug::Phase("d3d11hook-plugins-timeout");
    if (!BootstrapPluginsSafe()) {
        NightSharpDebug::Logf("[D3D11Hook] Plugin bootstrap failed after timeout; shutting down hook");
        InterlockedExchange(&g_shutdown, 1);
        Uninstall();
        return false;
    }
    NightSharpMenu::showMenu = true;
    InterlockedExchange(&g_menuReady, 1);

    while (!g_shutdown) {
        Sleep(250);
    }
    Uninstall();
    return true;
}

void Uninstall() {
    InterlockedExchange(&g_shutdown, 1);
    if (InterlockedCompareExchange(&g_uninstalling, 1, 0) != 0) {
        return;
    }

    const bool wasActive = g_active;
    g_active = false;
    InterlockedExchange(&g_menuReady, 0);
    InterlockedExchange(&g_bootstrapDone, 0);
    NightSharpMenu::showMenu = false;

    NightSharpDebug::Phase("d3d11hook-uninstall");
    NightSharpDebug::Logf("[D3D11Hook] Uninstalling...");

    if (g_vmtHook) {
        g_vmtHook->Unhook();
    }
    WaitForHookCallsToDrain();

    UnhookWndProc();

    RestoreCoreMemoryHacks();
    ShutdownPluginsSafe();
    __try {
        SDK::Lifecycle::Shutdown();
    }
    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                  "D3D11Hook::SDK::Lifecycle::Shutdown",
                  GetExceptionInformation())) {
    }
    ShutdownCoreEventsSafe();

    if (InterlockedCompareExchange(&g_imguiBackendReady, 0, 0) != 0) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        InterlockedExchange(&g_imguiBackendReady, 0);
    }
    if (InterlockedCompareExchange(&g_imguiContextReady, 0, 0) != 0 &&
        ImGui::GetCurrentContext()) {
        ImGui::DestroyContext();
        InterlockedExchange(&g_imguiContextReady, 0);
    }

    CleanupRenderTargetInternal();

    if (g_vmtHook) {
        delete g_vmtHook;
        g_vmtHook = nullptr;
    }

    SafeRelease(g_pd3dContext);
    SafeRelease(g_pd3dDevice);
    g_pSwapChain = nullptr;
    InterlockedExchange(&g_initDeviceState, 0);

    InterlockedExchange(&g_uninstalling, 0);
    NightSharpDebug::Logf("[D3D11Hook] Uninstall complete active=%d", wasActive ? 1 : 0);
}

bool IsActive() {
    return g_active;
}

void RequestShutdown() {
    NightSharpDebug::Logf("[D3D11Hook] RequestShutdown");
    InterlockedExchange(&g_shutdown, 1);
}

bool IsShutdownRequested() {
    return g_shutdown != 0;
}

} // namespace D3D11Hook
