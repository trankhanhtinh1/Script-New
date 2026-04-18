#include "Overlay.h"

#include "../core/CoreValidation.h"
#include "../core/CrashTelemetry.h"
#include "../core/CoreRuntime.h"
#include "../crt_shim.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx11.h"
#include "../imgui/imgui_impl_win32.h"
#include "../menu/NightSharpMenu.h"
#include "../menu/MenuConfig.h"
#include "../menu/MenuPersistence.h"
#include "../menu/PluginHostBridge.h"
#include "../menu/PluginRegistry.h"
#include "../plugins/PluginBootstrap.h"
#include "../plugins/PluginManager.h"

#include "../menu/MenuUI.h"
#include "../sdk/SDK.h"

#include <d3d11.h>
#include <dxgi.h>

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
    if (g_bMenuVisible || NightSharpMenu::debugWindowEnabled) {
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

void MoveOverlayToTarget() {
    if (!g_hGameWindow || !g_hOverlay) {
        return;
    }

    RECT rc = {};
    GetWindowRect(g_hGameWindow, &rc);
    SetWindowPos(g_hOverlay, HWND_TOPMOST,
        rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void SetClickThrough(bool through) {
    if (!g_hOverlay) return;
    LONG_PTR cur = GetWindowLongPtrW(g_hOverlay, GWL_EXSTYLE);
    LONG_PTR desired = through ? (cur | WS_EX_TRANSPARENT) : (cur & ~WS_EX_TRANSPARENT);
    if (desired == cur) return;
    SetWindowLongPtrW(g_hOverlay, GWL_EXSTYLE, desired);
    SetWindowPos(g_hOverlay, nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void UpdateClickThroughState() {
    if (!g_bMenuVisible && !NightSharpMenu::debugWindowEnabled) {
        SetClickThrough(true);
        return;
    }
    if (ImGui::GetIO().WantCaptureMouse) {
        SetClickThrough(false);
        return;
    }
    if (g_bMenuVisible && g_hOverlay) {
        POINT pt = {};
        GetCursorPos(&pt);
        ScreenToClient(g_hOverlay, &pt);
        const float cx = (float)pt.x;
        const float cy = (float)pt.y;
        for (int i = 0; i < NightSharpMenu::menuPanelCount; i++) {
            auto& p = NightSharpMenu::menuPanels[i];
            if (cx >= p.x && cx <= p.x + p.w && cy >= p.y && cy <= p.y + p.h) {
                SetClickThrough(false);
                return;
            }
        }
    }
    SetClickThrough(true);
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

    g_hOverlay = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        overlayClassName,
        L"NightSharp Overlay",
        WS_POPUP,
        gameRect.left,
        gameRect.top,
        gameRect.right - gameRect.left,
        gameRect.bottom - gameRect.top,
        nullptr,
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

    {
        HANDLE hFont = CreateFileA("C:\\Windows\\Fonts\\msyh.ttc",
            GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFont != INVALID_HANDLE_VALUE) {
            DWORD fileSize = GetFileSize(hFont, nullptr);
            if (fileSize > 0 && fileSize != INVALID_FILE_SIZE) {
                void* fontData = ImGui::MemAlloc(fileSize);
                if (fontData) {
                    DWORD bytesRead = 0;
                    if (ReadFile(hFont, fontData, fileSize, &bytesRead, nullptr) && bytesRead == fileSize) {
                        ImFontConfig mergeCfg;
                        mergeCfg.MergeMode = true;
                        mergeCfg.PixelSnapH = true;
                        mergeCfg.FontDataOwnedByAtlas = true;
                        io.Fonts->AddFontFromMemoryTTF(fontData, (int)fileSize, 14.0f, &mergeCfg,
                            io.Fonts->GetGlyphRangesChineseFull());
                    } else {
                        ImGui::MemFree(fontData);
                    }
                }
            }
            CloseHandle(hFont);
        }
    }

    ImGui_ImplWin32_Init(g_hOverlay);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

    WriteStage("[NightSharp] STAGE 5: ImGui initialized\r\n");

    CrashTelemetry::SetStage("Overlay::Run::InitMenuPlugins");
    ShowWindow(g_hOverlay, SW_SHOWNOACTIVATE);
    UpdateWindow(g_hOverlay);
    MoveOverlayToTarget();

    SDK::MenuManager::Init();
    NightSharpMenu::LoadGlobals();
    NightSharpMenu::showMenu = true;
    SDK::MenuManager::SetMenuVisible(true);
    SetClickThrough(true);

    CrashTelemetry::SetStage("Overlay::Run::InitCore");
    const bool coreInitOk = CoreRuntime::Initialize();
    WriteStage(coreInitOk
        ? "[NightSharp] STAGE 7: CoreRuntime initialized\r\n"
        : "[NightSharp] STAGE 7: CoreRuntime init incomplete\r\n");

    CrashTelemetry::SetStage("Overlay::Run::InitSDK");
    bool sdkInitFault = false;
    __try {
        SDK::Bootstrap::Init(nullptr);
    } __except(CrashTelemetry::ReportAndHandle("Overlay::Run::InitSDK", GetExceptionInformation())) {
        sdkInitFault = true;
    }
    if (sdkInitFault) {
        WriteStageFmt(
            "[NightSharp] STAGE 8: FAILED - SDK bootstrap crashed at %s\r\n",
            CrashTelemetry::g_stage ? CrashTelemetry::g_stage : "unknown");
        SDK::Bootstrap::Shutdown();
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

    CrashTelemetry::SetStage("Overlay::Run::PluginBootstrap::EnsureRegistered");
    WriteStage("[NightSharp] STAGE 8.1: PluginBootstrap::EnsureRegistered begin\r\n");
    Plugins::PluginBootstrap::EnsureRegistered();
    WriteStage("[NightSharp] STAGE 8.1: PluginBootstrap::EnsureRegistered done\r\n");

    CrashTelemetry::SetStage("Overlay::Run::PluginRegistry::LoadConfig");
    WriteStage("[NightSharp] STAGE 8.2: PluginRegistry::LoadConfig begin\r\n");
    PluginRegistry::LoadConfig();
    WriteStage("[NightSharp] STAGE 8.2: PluginRegistry::LoadConfig done\r\n");

    CrashTelemetry::SetStage("Overlay::Run::PluginHostBridge::WireHostAPI");
    WriteStage("[NightSharp] STAGE 8.3: PluginHostBridge::WireHostAPI begin\r\n");
    PluginHostBridge::WireHostAPI();
    WriteStage("[NightSharp] STAGE 8.3: PluginHostBridge::WireHostAPI done\r\n");

    WriteStage("[NightSharp] STAGE 8a: Plugin substrate registered (post-SDK)\r\n");

    // Load plugins AFTER SDK init — plugins need SDK menus to exist for override
    CrashTelemetry::SetStage("Overlay::Run::PluginManager::LoadAuto");
    Plugins::PluginManager::Get().LoadAuto();
    WriteStage("[NightSharp] STAGE 8b: Plugins loaded (post-SDK)\r\n");

    CrashTelemetry::SetStage("Overlay::PreLoadAll");
    __try {
        MenuPersistence::LoadAll();
    }
    __except (CrashTelemetry::ReportAndHandle("Overlay::LoadAll", GetExceptionInformation())) {
        WriteStage("[NightSharp] STAGE 8c: FAILED (LoadAll crash)\r\n");
    }
    CrashTelemetry::SetStage("Overlay::PostLoadAll");
    WriteStage("[NightSharp] STAGE 8c: MenuPersistence::LoadAll done\r\n");

    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    int frameCount = 0;

    while (!g_bShutdown) {
        bool frameFault = false;
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

            if (g_bShutdown) {
                __leave;
            }

            CrashTelemetry::SetStage("Overlay::Frame::CheckGameWindow");
            traceFrameStage("CheckGameWindow");
            if (!IsGameWindowAlive()) {
                WriteStage("[NightSharp] RENDER LOOP: Game window gone, breaking\r\n");
                InterlockedExchange(&g_bShutdown, 1);
                __leave;
            }

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
            CrashTelemetry::SetStage("Overlay::Frame::BeginWrite");
            traceFrameStage("BeginWrite");
            CoreRuntime::BeginWritePhase();
            CrashTelemetry::SetStage("Overlay::Frame::SDKUpdate");
            traceFrameStage("SDKUpdate");
            SDK::Bootstrap::Update();

            CrashTelemetry::SetStage("Overlay::Frame::ImGuiBackends");
            traceFrameStage("ImGuiBackends");
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();

            if (g_bMenuVisible || NightSharpMenu::debugWindowEnabled) {
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

            CrashTelemetry::SetStage("Overlay::Frame::PluginsUpdate");
            traceFrameStage("PluginsUpdate");
            Plugins::PluginManager::Get().OnUpdate();

            CrashTelemetry::SetStage("Overlay::Frame::MenuRender");
            traceFrameStage("MenuRender");
            NightSharpMenu::Render();
            NightSharpMenu::RenderDebugWindow();
            UpdateClickThroughState();
            CrashTelemetry::SetStage("Overlay::Frame::PluginsRender");
            traceFrameStage("PluginsRender");
            Plugins::PluginManager::Get().OnRender();

            CrashTelemetry::SetStage("Overlay::Frame::SDKRender");
            traceFrameStage("SDKRender");
            SDK::Bootstrap::Render();
            CrashTelemetry::SetStage("Overlay::Frame::EndWrite");
            traceFrameStage("EndWrite");
            CoreRuntime::EndWritePhase();

            CrashTelemetry::SetStage("Overlay::Frame::ImGuiRender");
            traceFrameStage("ImGuiRender");
            ImGui::EndFrame();
            ImGui::Render();

            CrashTelemetry::SetStage("Overlay::Frame::D3DPresent");
            traceFrameStage("D3DPresent");
            g_pd3dContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
            g_pd3dContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            g_pSwapChain->Present(1, 0);

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
            __try { WriteStage("[NightSharp] RENDER LOOP: frame exception, breaking\r\n"); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
            break;
        }
    }

    CrashTelemetry::SetStage("Overlay::Run::Cleanup");
    WriteStage("[NightSharp] CLEANUP: Begin\r\n");

    CrashTelemetry::SetStage("Overlay::Cleanup::UnloadAll");
    WriteStage("[NightSharp] CLEANUP 1: UnloadAll::Begin\r\n");
    __try {
        Plugins::PluginManager::Get().UnloadAll();
        WriteStage("[NightSharp] CLEANUP 1: UnloadAll::OK\r\n");
    } __except (CrashTelemetry::ReportAndHandle("Cleanup::UnloadAll", GetExceptionInformation())) {
        WriteStage("[NightSharp] CLEANUP 1: UnloadAll::SEH_CAUGHT\r\n");
    }

    CrashTelemetry::SetStage("Overlay::Cleanup::SDKShutdown");
    WriteStage("[NightSharp] CLEANUP 2: SDK::Shutdown::Begin\r\n");
    __try {
        SDK::Bootstrap::Shutdown();
        WriteStage("[NightSharp] CLEANUP 2: SDK::Shutdown::OK\r\n");
    } __except (CrashTelemetry::ReportAndHandle("Cleanup::SDKShutdown", GetExceptionInformation())) {
        WriteStage("[NightSharp] CLEANUP 2: SDK::Shutdown::SEH_CAUGHT\r\n");
    }

    CrashTelemetry::SetStage("Overlay::Cleanup::PluginManagerClear");
    WriteStage("[NightSharp] CLEANUP 2a: PluginManager::ClearAll::Begin\r\n");
    __try {
        Plugins::PluginManager::Get().ClearAll();
        WriteStage("[NightSharp] CLEANUP 2a: PluginManager::ClearAll::OK\r\n");
    } __except (CrashTelemetry::ReportAndHandle("Cleanup::PluginManagerClear", GetExceptionInformation())) {
        WriteStage("[NightSharp] CLEANUP 2a: PluginManager::ClearAll::SEH_CAUGHT\r\n");
    }

    CrashTelemetry::SetStage("Overlay::Cleanup::PluginRegistryClear");
    WriteStage("[NightSharp] CLEANUP 2b: PluginRegistry::Clear::Begin\r\n");
    __try {
        PluginRegistry::Clear();
        WriteStage("[NightSharp] CLEANUP 2b: PluginRegistry::Clear::OK\r\n");
    } __except (CrashTelemetry::ReportAndHandle("Cleanup::PluginRegistryClear", GetExceptionInformation())) {
        WriteStage("[NightSharp] CLEANUP 2b: PluginRegistry::Clear::SEH_CAUGHT\r\n");
    }

    CrashTelemetry::SetStage("Overlay::Cleanup::CoreRuntimeShutdown");
    WriteStage("[NightSharp] CLEANUP 3: CoreRuntime::Shutdown::Begin\r\n");
    __try {
        CoreRuntime::Shutdown();
        WriteStage("[NightSharp] CLEANUP 3: CoreRuntime::Shutdown::OK\r\n");
    } __except (CrashTelemetry::ReportAndHandle("Cleanup::CoreRuntimeShutdown", GetExceptionInformation())) {
        WriteStage("[NightSharp] CLEANUP 3: CoreRuntime::Shutdown::SEH_CAUGHT\r\n");
    }

    CrashTelemetry::SetStage("Overlay::Cleanup::MenuPersistenceSave");
    __try {
        MenuPersistence::SaveAll();
        WriteStage("[NightSharp] CLEANUP 3b: MenuPersistence::SaveAll::OK\r\n");
    } __except (CrashTelemetry::ReportAndHandle("Cleanup::MenuPersistenceSave", GetExceptionInformation())) {
        WriteStage("[NightSharp] CLEANUP 3b: MenuPersistence::SaveAll::SEH_CAUGHT\r\n");
    }

    CrashTelemetry::SetStage("Overlay::Cleanup::MenuManagerShutdown");
    WriteStage("[NightSharp] CLEANUP 4: MenuManager::Shutdown::Begin\r\n");
    __try {
        SDK::MenuManager::Shutdown();
        WriteStage("[NightSharp] CLEANUP 4: MenuManager::Shutdown::OK\r\n");
    } __except (CrashTelemetry::ReportAndHandle("Cleanup::MenuManagerShutdown", GetExceptionInformation())) {
        WriteStage("[NightSharp] CLEANUP 4: MenuManager::Shutdown::SEH_CAUGHT\r\n");
    }

    CrashTelemetry::SetStage("Overlay::Cleanup::ImGuiDX11Shutdown");
    WriteStage("[NightSharp] CLEANUP 5: ImGui_ImplDX11_Shutdown::Begin\r\n");
    __try {
        ImGui_ImplDX11_Shutdown();
        WriteStage("[NightSharp] CLEANUP 5: ImGui_ImplDX11_Shutdown::OK\r\n");
    } __except (CrashTelemetry::ReportAndHandle("Cleanup::ImGuiDX11Shutdown", GetExceptionInformation())) {
        WriteStage("[NightSharp] CLEANUP 5: ImGui_ImplDX11_Shutdown::SEH_CAUGHT\r\n");
    }

    CrashTelemetry::SetStage("Overlay::Cleanup::ImGuiWin32Shutdown");
    WriteStage("[NightSharp] CLEANUP 6: ImGui_ImplWin32_Shutdown::Begin\r\n");
    __try {
        ImGui_ImplWin32_Shutdown();
        WriteStage("[NightSharp] CLEANUP 6: ImGui_ImplWin32_Shutdown::OK\r\n");
    } __except (CrashTelemetry::ReportAndHandle("Cleanup::ImGuiWin32Shutdown", GetExceptionInformation())) {
        WriteStage("[NightSharp] CLEANUP 6: ImGui_ImplWin32_Shutdown::SEH_CAUGHT\r\n");
    }

    CrashTelemetry::SetStage("Overlay::Cleanup::ImGuiDestroyContext");
    WriteStage("[NightSharp] CLEANUP 7: ImGui::DestroyContext::Begin\r\n");
    __try {
        ImGui::DestroyContext();
        WriteStage("[NightSharp] CLEANUP 7: ImGui::DestroyContext::OK\r\n");
    } __except (CrashTelemetry::ReportAndHandle("Cleanup::ImGuiDestroyContext", GetExceptionInformation())) {
        WriteStage("[NightSharp] CLEANUP 7: ImGui::DestroyContext::SEH_CAUGHT\r\n");
    }

    CrashTelemetry::SetStage("Overlay::Cleanup::CleanupDeviceD3D11");
    WriteStage("[NightSharp] CLEANUP 8: CleanupDeviceD3D11::Begin\r\n");
    __try {
        CleanupDeviceD3D11();
        WriteStage("[NightSharp] CLEANUP 8: CleanupDeviceD3D11::OK\r\n");
    } __except (CrashTelemetry::ReportAndHandle("Cleanup::CleanupDeviceD3D11", GetExceptionInformation())) {
        WriteStage("[NightSharp] CLEANUP 8: CleanupDeviceD3D11::SEH_CAUGHT\r\n");
    }

    CrashTelemetry::SetStage("Overlay::Cleanup::DestroyWindow");
    WriteStage("[NightSharp] CLEANUP 9: DestroyWindow::Begin\r\n");
    __try {
        if (g_hOverlay) {
            DestroyWindow(g_hOverlay);
            g_hOverlay = nullptr;
        }
        WriteStage("[NightSharp] CLEANUP 9: DestroyWindow::OK\r\n");
    } __except (CrashTelemetry::ReportAndHandle("Cleanup::DestroyWindow", GetExceptionInformation())) {
        WriteStage("[NightSharp] CLEANUP 9: DestroyWindow::SEH_CAUGHT\r\n");
    }

    CrashTelemetry::SetStage("Overlay::Cleanup::UnregisterClass");
    WriteStage("[NightSharp] CLEANUP 10: UnregisterClass::Begin\r\n");
    __try {
        if (classRegistered) {
            UnregisterClassW(overlayClassName, wc.hInstance);
        }
        WriteStage("[NightSharp] CLEANUP 10: UnregisterClass::OK\r\n");
    } __except (CrashTelemetry::ReportAndHandle("Cleanup::UnregisterClass", GetExceptionInformation())) {
        WriteStage("[NightSharp] CLEANUP 10: UnregisterClass::SEH_CAUGHT\r\n");
    }

    CrashTelemetry::SetStage("Overlay::Cleanup::NsHeapDestroy");
    WriteStage("[NightSharp] CLEANUP 11: NsHeapDestroy::Begin\r\n");
    __try {
        NsHeapDestroy();
        WriteStage("[NightSharp] CLEANUP 11: NsHeapDestroy::OK\r\n");
    } __except (CrashTelemetry::ReportAndHandle("Cleanup::NsHeapDestroy", GetExceptionInformation())) {
        WriteStage("[NightSharp] CLEANUP 11: NsHeapDestroy::SEH_CAUGHT\r\n");
    }

    InterlockedExchange(&g_bRunning, 0);
    WriteStage("[NightSharp] CLEANUP: Complete\r\n");
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
