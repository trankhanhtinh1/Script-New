#define _CRT_SECURE_NO_WARNINGS
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <DbgHelp.h>
#include <Psapi.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "kiero/kiero.h"

struct ImGuiContext;
typedef bool (*Logic_Init_t)(ID3D11Device*, ID3D11DeviceContext*, HWND, ImGuiContext*);
typedef void (*Logic_Shutdown_t)();
typedef void (*Logic_Render_t)();
typedef LRESULT (*Logic_WndProc_t)(HWND, UINT, WPARAM, LPARAM);
typedef bool (*Logic_WantCaptureMouse_t)();

static HMODULE g_hModule = nullptr;
static HMODULE g_hLogic = nullptr;
static bool g_requestReload = false;
static bool g_init = false;

static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;
static ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
static HWND g_hWnd = nullptr;
static WNDPROC g_oWndProc = nullptr;

static Logic_Init_t g_pfnLogicInit = nullptr;
static Logic_Shutdown_t g_pfnLogicShutdown = nullptr;
static Logic_Render_t g_pfnLogicRender = nullptr;
static Logic_WndProc_t g_pfnLogicWndProc = nullptr;
static Logic_WantCaptureMouse_t g_pfnLogicWantCaptureMouse = nullptr;

typedef HRESULT(__stdcall* Present)(IDXGISwapChain*, UINT, UINT);
static Present g_oPresent = nullptr;

static char g_logicPath[MAX_PATH] = {0};
static char g_crashPath[MAX_PATH] = {0};
static DWORD g_lastError = 0;
static bool g_loadAttempted = false;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 只在崩潰時寫入 crash.txt
static void WriteCrashLog(const char* msg) {
    HANDLE hFile = CreateFileA(g_crashPath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        SYSTEMTIME st;
        GetLocalTime(&st);
        char timeBuf[64];
        wsprintfA(timeBuf, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        WriteFile(hFile, timeBuf, lstrlenA(timeBuf), &written, NULL);
        WriteFile(hFile, msg, lstrlenA(msg), &written, NULL);
        WriteFile(hFile, "\r\n", 2, &written, NULL);
        CloseHandle(hFile);
    }
}

static void InitCrashPath() {
    if (GetModuleFileNameA(g_hModule, g_crashPath, MAX_PATH)) {
        char* lastSlash = g_crashPath;
        for (char* p = g_crashPath; *p; p++) {
            if (*p == '\\' || *p == '/') lastSlash = p;
        }
        if (lastSlash != g_crashPath) {
            *(lastSlash + 1) = 0;
            lstrcatA(g_crashPath, "crash.txt");
        }
    } else {
        lstrcpyA(g_crashPath, "C:\\crash.txt");
    }
}

static void FindLogicDll() {
    char path[MAX_PATH];

    const char* hardcodedPaths[] = {
        "C:\\zz\\Script-New\\x64\\Release\\logic.dll",
        "D:\\zz\\Script-New\\x64\\Release\\logic.dll",
        nullptr
    };

    for (int i = 0; hardcodedPaths[i]; i++) {
        if (GetFileAttributesA(hardcodedPaths[i]) != INVALID_FILE_ATTRIBUTES) {
            lstrcpyA(g_logicPath, hardcodedPaths[i]);
            return;
        }
    }

    if (GetModuleFileNameA(g_hModule, path, MAX_PATH) && path[0]) {
        char* lastSlash = path;
        for (char* p = path; *p; p++) {
            if (*p == '\\' || *p == '/') lastSlash = p;
        }
        if (lastSlash != path) {
            *(lastSlash + 1) = 0;
            lstrcatA(path, "logic.dll");
            if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
                lstrcpyA(g_logicPath, path);
                return;
            }
        }
    }

    lstrcpyA(g_logicPath, "logic.dll");
}

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* pExceptionInfo) {
    char buf[512];

    WriteCrashLog("=== CRASH DETECTED ===");

    wsprintfA(buf, "Exception Code: 0x%08X", pExceptionInfo->ExceptionRecord->ExceptionCode);
    WriteCrashLog(buf);

    wsprintfA(buf, "Exception Address: %p", pExceptionInfo->ExceptionRecord->ExceptionAddress);
    WriteCrashLog(buf);

    wsprintfA(buf, "RIP: %p", (void*)pExceptionInfo->ContextRecord->Rip);
    WriteCrashLog(buf);

    HMODULE hMod = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)pExceptionInfo->ExceptionRecord->ExceptionAddress, &hMod)) {
        char modName[MAX_PATH];
        GetModuleFileNameA(hMod, modName, MAX_PATH);
        wsprintfA(buf, "Crash Module: %s", modName);
        WriteCrashLog(buf);

        MODULEINFO modInfo;
        if (GetModuleInformation(GetCurrentProcess(), hMod, &modInfo, sizeof(modInfo))) {
            wsprintfA(buf, "Crash Offset: 0x%llX", (DWORD_PTR)pExceptionInfo->ExceptionRecord->ExceptionAddress - (DWORD_PTR)modInfo.lpBaseOfDll);
            WriteCrashLog(buf);
        }
    }

    WriteCrashLog("======================");

    return EXCEPTION_CONTINUE_SEARCH;
}

bool LoadLogicDll() {
    g_loadAttempted = true;

    if (GetFileAttributesA(g_logicPath) == INVALID_FILE_ATTRIBUTES) {
        g_lastError = ERROR_FILE_NOT_FOUND;
        return false;
    }

    __try {
        g_hLogic = LoadLibraryA(g_logicPath);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        g_lastError = GetExceptionCode();
        return false;
    }

    if (!g_hLogic) {
        g_lastError = GetLastError();
        return false;
    }

    __try {
        g_pfnLogicInit = (Logic_Init_t)GetProcAddress(g_hLogic, "Logic_Init");
        g_pfnLogicShutdown = (Logic_Shutdown_t)GetProcAddress(g_hLogic, "Logic_Shutdown");
        g_pfnLogicRender = (Logic_Render_t)GetProcAddress(g_hLogic, "Logic_Render");
        g_pfnLogicWndProc = (Logic_WndProc_t)GetProcAddress(g_hLogic, "Logic_WndProc");
        g_pfnLogicWantCaptureMouse = (Logic_WantCaptureMouse_t)GetProcAddress(g_hLogic, "Logic_WantCaptureMouse");
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    return g_pfnLogicInit && g_pfnLogicShutdown && g_pfnLogicRender;
}

void UnloadLogicDll() {
    if (g_pfnLogicShutdown) {
        __try {
            g_pfnLogicShutdown();
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
    if (g_hLogic) {
        FreeLibrary(g_hLogic);
        g_hLogic = nullptr;
    }
    g_pfnLogicInit = nullptr;
    g_pfnLogicShutdown = nullptr;
    g_pfnLogicRender = nullptr;
    g_pfnLogicWndProc = nullptr;
    g_pfnLogicWantCaptureMouse = nullptr;
}

void ReloadLogicDll() {
    UnloadLogicDll();
    Sleep(100);
    if (LoadLogicDll()) {
        if (g_pfnLogicInit) {
            __try {
                g_pfnLogicInit(g_pDevice, g_pContext, g_hWnd, ImGui::GetCurrentContext());
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (g_pfnLogicWndProc) {
        __try {
            LRESULT result = g_pfnLogicWndProc(hWnd, uMsg, wParam, lParam);
            if (result != 0) {
                return result;
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }

    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
        return true;
    }

    return CallWindowProcA(g_oWndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!g_init) {
        __try {
            HRESULT hr = pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pDevice);
            if (FAILED(hr)) {
                return g_oPresent(pSwapChain, SyncInterval, Flags);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return g_oPresent(pSwapChain, SyncInterval, Flags);
        }

        g_pDevice->GetImmediateContext(&g_pContext);

        DXGI_SWAP_CHAIN_DESC sd;
        pSwapChain->GetDesc(&sd);
        g_hWnd = sd.OutputWindow;

        ID3D11Texture2D* pBackBuffer = nullptr;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
        g_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
        pBackBuffer->Release();

        g_oWndProc = (WNDPROC)SetWindowLongPtrA(g_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;

        ImGui_ImplWin32_Init(g_hWnd);
        ImGui_ImplDX11_Init(g_pDevice, g_pContext);

        if (LoadLogicDll()) {
            if (g_pfnLogicInit) {
                __try {
                    g_pfnLogicInit(g_pDevice, g_pContext, g_hWnd, ImGui::GetCurrentContext());
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
        }

        g_init = true;
    }

    if (g_requestReload) {
        g_requestReload = false;
        ReloadLogicDll();
    }

    __try {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    } __except(EXCEPTION_EXECUTE_HANDLER) {}

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Core", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::Button("Reload Logic DLL", ImVec2(150, 30))) {
            g_requestReload = true;
        }

        ImGui::Text("Logic: %s", g_hLogic ? "Loaded" : "Not Loaded");
        ImGui::Text("Path: %s", g_logicPath);
        if (g_loadAttempted && !g_hLogic) {
            ImGui::Text("Error: %lu (0x%08X)", g_lastError, g_lastError);
        }
        ImGui::End();
    }

    if (g_pfnLogicRender) {
        __try {
            g_pfnLogicRender();
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            g_pfnLogicRender = nullptr;
        }
    }

    __try {
        ImGui::Render();
        g_pContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    } __except(EXCEPTION_EXECUTE_HANDLER) {}

    return g_oPresent(pSwapChain, SyncInterval, Flags);
}

DWORD WINAPI UnloadThread(LPVOID lpParam) {
    while (true) {
        if (GetAsyncKeyState(VK_END) & 1) {
            break;
        }
        Sleep(100);
    }

    UnloadLogicDll();

    if (g_oWndProc && g_hWnd) {
        SetWindowLongPtrA(g_hWnd, GWLP_WNDPROC, (LONG_PTR)g_oWndProc);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (g_pRenderTargetView) g_pRenderTargetView->Release();

    kiero::shutdown();

    FreeLibraryAndExitThread(g_hModule, 0);
    return 0;
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    SetUnhandledExceptionFilter(CrashHandler);

    FindLogicDll();

    Sleep(1000);

    bool hooked = false;
    int attempts = 0;
    while (!hooked && attempts < 100) {
        __try {
            auto status = kiero::init(kiero::RenderType::D3D11);
            if (status == kiero::Status::Success) {
                kiero::bind(8, (void**)&g_oPresent, hkPresent);
                hooked = true;
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
        Sleep(100);
        attempts++;
    }

    if (!hooked) {
        return 1;
    }

    CreateThread(nullptr, 0, UnloadThread, nullptr, 0, nullptr);

    return 0;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        InitCrashPath();
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
