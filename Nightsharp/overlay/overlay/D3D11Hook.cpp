#include "D3D11Hook.h"
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
#include "../CrashReporter.h"
#include "../DebugLog.h"
#include "../FpsDropDebug.h"
#include "../SDK/Core/Game.h"
#include "../SDK/Events/Events.h"
#include "../SDK/UI/UI.h"

#include <d3d11.h>
#include <dxgi.h>
#include <cstdlib>
#include <mutex>
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
static std::once_flag   g_initDevice;
static bool             g_active           = false;
static volatile LONG    g_shutdown         = 0;
// Set to 1 once PluginBootstrap::EnsureRegistered() has returned (success or crash).
// Render() gates all SDK/plugin calls on this flag to prevent the bootstrap race.
static volatile LONG    g_bootstrapDone    = 0;

// -----------------------------------------------------------------------
// Pattern scanning helpers
// -----------------------------------------------------------------------
static uint8_t* FindPattern(const wchar_t* module, const char* sig) {
    const auto mod = GetModuleHandleW(module);
    if (!mod) return nullptr;

    // Convert signature string to bytes (-1 = wildcard)
    auto pattern_to_bytes = [](const char* pattern) {
        struct { int32_t bytes[128]; int len; } result = {};
        const char* p = pattern;
        while (*p && result.len < 128) {
            if (*p == ' ') { ++p; continue; }
            if (*p == '?') {
                ++p; if (*p == '?') ++p;
                result.bytes[result.len++] = -1;
                continue;
            }
            result.bytes[result.len++] = strtoul(p, const_cast<char**>(&p), 16);
        }
        return result;
    };

    auto pb = pattern_to_bytes(sig);

    const auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(mod);
    const auto nt  = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uint8_t*>(mod) + dos->e_lfanew);
    const auto section = IMAGE_FIRST_SECTION(nt);

    const auto scanStart = reinterpret_cast<uint8_t*>(mod) + section->VirtualAddress;
    const auto scanSize  = section->SizeOfRawData;

    MEMORY_BASIC_INFORMATION mbi = {};
    const uint8_t* nextCheck = nullptr;

    for (size_t i = 0; i < scanSize - pb.len; ++i) {
        bool found = true;
        for (int j = 0; j < pb.len; ++j) {
            const auto addr = scanStart + i + j;
            if (addr >= nextCheck) {
                if (!VirtualQuery(addr, &mbi, sizeof(mbi))) break;
                if (mbi.Protect == PAGE_NOACCESS) {
                    i += reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize -
                         (reinterpret_cast<uintptr_t>(scanStart) + i);
                    found = false;
                    break;
                }
                nextCheck = static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
            }
            if (pb.bytes[j] != -1 && scanStart[i + j] != pb.bytes[j]) {
                found = false;
                break;
            }
        }
        if (found) return &scanStart[i];
    }
    return nullptr;
}

static uintptr_t ResolveRelativeCall(uint8_t* addr) {
    if (!addr || addr[0] != 0xE8) return 0;
    return reinterpret_cast<uintptr_t>(addr) + *reinterpret_cast<int32_t*>(addr + 1) + 5;
}

// -----------------------------------------------------------------------
// Swapchain acquisition via MaterialRegistry (R3nzSkin-style)
// -----------------------------------------------------------------------
static IDXGISwapChain* FindSwapChain() {
    // Pattern: "E8 ? ? ? ? 8B 57 34 45 33 C9"
    // CALL Riot__Renderer__MaterialRegistry__GetSingletonPtr; MOV EDX,[RDI+34]; XOR ECX,ECX
    constexpr const char* funcSig =
        "E8 ? ? ? ? 8B 57 34 45 33 C9";
    auto funcAddr = FindPattern(nullptr, funcSig);
    if (!funcAddr) {
        NightSharpDebug::Logf("[D3D11Hook] MaterialRegistry singleton pattern not found");
        return nullptr;
    }

    auto singletonFunc = ResolveRelativeCall(funcAddr);
    if (!singletonFunc) {
        NightSharpDebug::Logf("[D3D11Hook] MaterialRegistry singleton resolve failed");
        return nullptr;
    }

    auto materialRegistry = reinterpret_cast<uintptr_t(__fastcall*)()>(singletonFunc)();
    if (!materialRegistry) {
        NightSharpDebug::Logf("[D3D11Hook] MaterialRegistry singleton returned null");
        return nullptr;
    }

    // Pattern: "48 8D BB ? ? ? ? C6 83 ? ? ? ? ? 0F 84"
    // LEA RDI, [RBX + field_offset]; MOV byte [RBX + other_offset], 0; JE ...
    constexpr const char* fieldSig =
        "48 8D BB ? ? ? ? C6 83 ? ? ? ? ? 0F 84";
    auto fieldAddr = FindPattern(nullptr, fieldSig);
    if (!fieldAddr) {
        NightSharpDebug::Logf("[D3D11Hook] SwapChain field pattern not found");
        return nullptr;
    }

    // The field offset is the 32-bit displacement in the LEA instruction
    // LEA RDI, [RBX + disp32] → bytes: 48 8D BB disp32
    int32_t swapChainOffset = *reinterpret_cast<int32_t*>(fieldAddr + 3);

    auto swapChain = *reinterpret_cast<IDXGISwapChain**>(materialRegistry + swapChainOffset);
    if (!swapChain) {
        NightSharpDebug::Logf("[D3D11Hook] SwapChain pointer is null");
        return nullptr;
    }

    NightSharpDebug::Logf("[D3D11Hook] Found swapchain at registry+0x%X = %p",
                          swapChainOffset, (void*)swapChain);
    return swapChain;
}

static bool ResolveSwapChainLayoutCached(bool noisy) {
    if (g_swapChainLayoutResolved) {
        return true;
    }

    constexpr const char* funcSig =
        "E8 ? ? ? ? 8B 57 34 45 33 C9";
    uint8_t* funcAddr = FindPattern(nullptr, funcSig);
    if (!funcAddr) {
        if (noisy) {
            NightSharpDebug::Logf("[D3D11Hook] MaterialRegistry singleton pattern not found");
        }
        return false;
    }

    const uintptr_t singletonFunc = ResolveRelativeCall(funcAddr);
    if (!singletonFunc) {
        if (noisy) {
            NightSharpDebug::Logf("[D3D11Hook] MaterialRegistry singleton resolve failed");
        }
        return false;
    }

    constexpr const char* fieldSig =
        "48 8D BB ? ? ? ? C6 83 ? ? ? ? ? 0F 84";
    uint8_t* fieldAddr = FindPattern(nullptr, fieldSig);
    if (!fieldAddr) {
        if (noisy) {
            NightSharpDebug::Logf("[D3D11Hook] SwapChain field pattern not found");
        }
        return false;
    }

    g_materialRegistrySingletonFn = singletonFunc;
    g_swapChainOffset = *reinterpret_cast<int32_t*>(fieldAddr + 3);
    g_swapChainLayoutResolved = true;
    NightSharpDebug::Logf("[D3D11Hook] Swapchain layout resolved singleton=0x%p offset=0x%X",
                          reinterpret_cast<void*>(g_materialRegistrySingletonFn),
                          g_swapChainOffset);
    return true;
}

static IDXGISwapChain* FindSwapChainCached(bool noisy = true) {
    if (!ResolveSwapChainLayoutCached(noisy)) {
        return nullptr;
    }

    uintptr_t materialRegistry = 0;
    __try {
        materialRegistry =
            reinterpret_cast<uintptr_t(__fastcall*)()>(g_materialRegistrySingletonFn)();
    }
    __except (1) {
        if (noisy) {
            NightSharpDebug::Logf("[D3D11Hook] MaterialRegistry singleton call faulted");
        }
        return nullptr;
    }

    if (!materialRegistry) {
        if (noisy) {
            NightSharpDebug::Logf("[D3D11Hook] MaterialRegistry singleton returned null");
        }
        return nullptr;
    }

    IDXGISwapChain* swapChain = nullptr;
    __try {
        swapChain =
            *reinterpret_cast<IDXGISwapChain**>(materialRegistry + g_swapChainOffset);
    }
    __except (1) {
        if (noisy) {
            NightSharpDebug::Logf("[D3D11Hook] SwapChain field read faulted registry=0x%p offset=0x%X",
                                  reinterpret_cast<void*>(materialRegistry),
                                  g_swapChainOffset);
        }
        return nullptr;
    }

    if (!swapChain) {
        if (noisy) {
            NightSharpDebug::Logf("[D3D11Hook] SwapChain pointer is null registry=0x%p offset=0x%X",
                                  reinterpret_cast<void*>(materialRegistry),
                                  g_swapChainOffset);
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
            NightSharpDebug::Logf("[D3D11Hook] SwapChain GetDesc faulted ptr=%p",
                                  static_cast<void*>(swapChain));
        }
        return nullptr;
    }

    if (FAILED(hr)) {
        if (noisy) {
            NightSharpDebug::Logf("[D3D11Hook] SwapChain GetDesc failed hr=0x%08lX ptr=%p",
                                  static_cast<unsigned long>(hr),
                                  static_cast<void*>(swapChain));
        }
        return nullptr;
    }

    NightSharpDebug::Logf("[D3D11Hook] Found swapchain at registry+0x%X = %p",
                          g_swapChainOffset,
                          static_cast<void*>(swapChain));
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

        IDXGISwapChain* swapChain = FindSwapChainCached(i == 0);
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
    // Let ImGui process input first
    if (ImGui::GetCurrentContext() &&
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return TRUE;
    }

    // Handle keyboard shortcuts
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_F1) {
            if (InterlockedCompareExchange(&g_menuReady, 0, 0) == 0) {
                return TRUE;
            }
            NightSharpMenu::showMenu = !NightSharpMenu::showMenu;
            return TRUE;
        }
        if (wParam == VK_END) {
            InterlockedExchange(&g_shutdown, 1);
            return TRUE;
        }
    }

    // Dispatch to SDK WndProc subscribers (cursor, target selector, chat, plugins, etc.)
    SDK::Game::DispatchWndProc(hWnd, msg, wParam, lParam);

    // Dispatch key events to MenuKeyBind state machines (updates Active field)
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYUP) {
        const bool down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
        SDK::UI::MenuManager::Instance().DispatchKey(static_cast<int>(wParam), down);
    }

    // Forward to original game WndProc
    return CallWindowProcW(g_originalWndProc, hWnd, msg, wParam, lParam);
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
static void InitImGui(IDXGISwapChain* swapChain) {
    NightSharpDebug::Phase("d3d11hook-imgui-init");

    NsHeapInit();
    ImGui::SetAllocatorFunctions(NsImGuiAlloc, NsImGuiFree, nullptr);
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.Fonts->AddFontDefault();

    g_pSwapChain = swapChain;
    g_pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_pd3dDevice));
    g_pd3dDevice->GetImmediateContext(&g_pd3dContext);

    if (!CreateRenderTargetInternal()) {
        NightSharpDebug::Logf("[D3D11Hook] Failed to create render target");
        return;
    }

    ImGui_ImplWin32_Init(g_gameHwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);
    ImGui_ImplDX11_CreateDeviceObjects();

    NightSharpDebug::Logf("[D3D11Hook] ImGui initialized with game device");
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

static void Render() {
    if (g_shutdown)
        return;

    NightSharpPerf::BeginFrame();

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Guard: do not touch game memory or plugins until PluginBootstrap has fully
    // completed on the install thread. Without this, TickRead/OnUpdate can dereference
    // SDK singletons that haven't been initialized yet, causing ACCESS_VIOLATION.
    if (g_bootstrapDone) {
        // Read game state once per frame
        {
            NightSharpPerf::ScopedTimer timer("CoreRuntime::TickRead");
            CoreRuntime::TickRead();
        }

        // Plugin update + render
        {
            NightSharpPerf::ScopedTimer timer("PluginManager::OnUpdate");
            Plugins::PluginManager::Get().OnUpdate();
        }
        {
            NightSharpPerf::ScopedTimer timer("PluginManager::OnRender");
            Plugins::PluginManager::Get().OnRender();
        }

        // Menu + PermaShow render
        {
            NightSharpPerf::ScopedTimer timer("NightSharpMenu::Render");
            RenderMenuSafe();
        }
    }

    ImGui::EndFrame();
    ImGui::Render();

    g_pd3dContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    NightSharpPerf::DumpSections();
    NightSharpPerf::EndFrame();
}

// -----------------------------------------------------------------------
// VTable hooks
// -----------------------------------------------------------------------
struct DxgiPresent {
    static long WINAPI Hooked(IDXGISwapChain* pSwapChain, UINT syncInterval, UINT flags) {
        std::call_once(g_initDevice, [&]() {
            InitImGui(pSwapChain);
        });
        Render();
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
        CleanupRenderTargetInternal();
        auto hr = m_original(pSwapChain, bufferCount, width, height, newFormat, swapChainFlags);
        CreateRenderTargetInternal();
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

    __try {
        Plugins::PluginBootstrap::EnsureRegistered();
        ok = true;
    }
    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                  "D3D11Hook::PluginBootstrap",
                  GetExceptionInformation())) {
        NightSharpDebug::Logf("[D3D11Hook] Plugin bootstrap crashed");
    }
    // Signal Render() that it is now safe to call TickRead/OnUpdate/OnRender.
    // This runs whether bootstrap succeeded or crashed, so the render thread
    // is never permanently blocked (it will just skip SDK calls if plugins failed).
    InterlockedExchange(&g_bootstrapDone, 1);
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
    InterlockedExchange(&g_menuReady, 0);
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

    // Wait until we can read a sane game time, then delay three in-game
    // seconds before showing the menu/loading plugins. This matches the old
    // startup behavior even when the DLL is injected after gameTime is already
    // greater than 3.0.
    NightSharpDebug::Phase("d3d11hook-wait-game");
    NightSharpDebug::Logf("[D3D11Hook] Waiting for game time read + %.1fs...",
                          kMenuStartDelayAfterGameTimeSeconds);

    bool sawGameTime = false;
    float firstGameTime = 0.0f;
    for (int i = 0; i < kGameReadyMaxPolls; ++i) {
        if (g_shutdown) {
            Uninstall();
            return true;
        }

        float gameTime = 0.0f;
        if (ReadGameTimeForStartup(gameTime)) {
            if (!sawGameTime) {
                sawGameTime = true;
                firstGameTime = gameTime;
                NightSharpDebug::Logf("[D3D11Hook] First sane game time read %.2fs; delaying menu/plugins %.1fs",
                                      gameTime,
                                      kMenuStartDelayAfterGameTimeSeconds);
            }

            float elapsed = gameTime - firstGameTime;
            if (elapsed < 0.0f) {
                firstGameTime = gameTime;
                elapsed = 0.0f;
            }

            if (elapsed >= kMenuStartDelayAfterGameTimeSeconds) {
                NightSharpDebug::Logf("[D3D11Hook] Game ready at %.2fs (elapsed %.2fs), bootstrapping plugins",
                                      gameTime,
                                      elapsed);
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
            if (sawGameTime) {
                NightSharpDebug::Logf("[D3D11Hook] Still delaying menu/plugins: gameTime=%.2fs first=%.2fs",
                                      gameTime,
                                      firstGameTime);
            } else {
                NightSharpDebug::Logf("[D3D11Hook] Still waiting for first sane game time read (%d sec)",
                                      static_cast<int>((i * kGameReadyPollMs) / 1000));
            }
        }
        Sleep(kGameReadyPollMs);
    }

    NightSharpDebug::Logf("[D3D11Hook] Game time wait timed out, bootstrapping anyway");
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
    if (!g_active) return;
    g_active = false;
    InterlockedExchange(&g_menuReady, 0);
    NightSharpMenu::showMenu = false;

    NightSharpDebug::Phase("d3d11hook-uninstall");
    NightSharpDebug::Logf("[D3D11Hook] Uninstalling...");

    ShutdownPluginsSafe();
    ShutdownCoreEventsSafe();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupRenderTargetInternal();
    UnhookWndProc();

    if (g_vmtHook) {
        delete g_vmtHook;
        g_vmtHook = nullptr;
    }

    SafeRelease(g_pd3dContext);
    SafeRelease(g_pd3dDevice);
    g_pSwapChain = nullptr;

    NightSharpDebug::Logf("[D3D11Hook] Uninstall complete");
}

bool IsActive() {
    return g_active;
}

void RequestShutdown() {
    InterlockedExchange(&g_shutdown, 1);
}

bool IsShutdownRequested() {
    return g_shutdown != 0;
}

} // namespace D3D11Hook
