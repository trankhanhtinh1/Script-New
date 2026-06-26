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
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return TRUE;

    // Handle keyboard shortcuts
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_F1) {
            NightSharpMenu::showMenu = !NightSharpMenu::showMenu;
            return TRUE;
        }
        if (wParam == VK_END) {
            InterlockedExchange(&g_shutdown, 1);
            return TRUE;
        }
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
static void Render() {
    if (g_shutdown)
        return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Read game state once per frame
    CoreRuntime::TickRead();

    // Plugin update + render
    Plugins::PluginManager::Get().OnUpdate();
    Plugins::PluginManager::Get().OnRender();

    // Menu + PermaShow render
    __try {
        NightSharpMenu::Render();
    }
    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                  "D3D11Hook::NightSharpMenu::Render",
                  GetExceptionInformation())) {
        NightSharpDebug::Logf("[D3D11Hook] Menu render crashed");
    }

    ImGui::EndFrame();
    ImGui::Render();

    g_pd3dContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
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
static void BootstrapPluginsSafe() {
    __try {
        Plugins::PluginBootstrap::EnsureRegistered();
    }
    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                  "D3D11Hook::PluginBootstrap",
                  GetExceptionInformation())) {
        NightSharpDebug::Logf("[D3D11Hook] Plugin bootstrap crashed");
    }
}

static void ShutdownPluginsSafe() {
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

    HWND gameHwnd = FindGameWindow();
    if (!gameHwnd) {
        NightSharpDebug::Logf("[D3D11Hook] Could not find game window");
        return false;
    }

    if (!HookWndProc(gameHwnd)) {
        NightSharpDebug::Logf("[D3D11Hook] Failed to hook WndProc");
        return false;
    }

    IDXGISwapChain* swapChain = FindSwapChain();
    if (!swapChain) {
        NightSharpDebug::Logf("[D3D11Hook] Failed to find swapchain, unhooking WndProc");
        UnhookWndProc();
        return false;
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

    // Wait for game ready then bootstrap plugins
    NightSharpDebug::Phase("d3d11hook-wait-game");
    NightSharpDebug::Logf("[D3D11Hook] Waiting for game > 3s...");

    for (int i = 0; i < 240; ++i) {
        if (g_shutdown) return true;

        CoreRuntime::RefreshReadState();

        if (CoreRuntime::GetContext().gameTime > 3.0f) {
            NightSharpDebug::Logf("[D3D11Hook] Game ready at %.1fs, bootstrapping plugins",
                                  CoreRuntime::GetContext().gameTime);
            NightSharpDebug::Phase("d3d11hook-plugins");
            BootstrapPluginsSafe();
            return true;
        }
        Sleep(500);
    }

    NightSharpDebug::Logf("[D3D11Hook] Game ready wait timed out, bootstrapping anyway");
    NightSharpDebug::Phase("d3d11hook-plugins-timeout");
    BootstrapPluginsSafe();

    return true;
}

void Uninstall() {
    if (!g_active) return;

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
    g_active = false;

    NightSharpDebug::Logf("[D3D11Hook] Uninstall complete");
}

bool IsActive() {
    return g_active;
}

} // namespace D3D11Hook
