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
static char g_logPath[MAX_PATH] = {0};
static DWORD g_lastError = 0;
static bool g_loadAttempted = false;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void DebugLog(const char* msg) {
    HANDLE hFile = CreateFileA(g_logPath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
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

static void DebugLogInt(const char* prefix, int val) {
    char buf[256];
    wsprintfA(buf, "%s%d", prefix, val);
    DebugLog(buf);
}

static void DebugLogHex(const char* prefix, DWORD val) {
    char buf[256];
    wsprintfA(buf, "%s0x%08X", prefix, val);
    DebugLog(buf);
}

static void DebugLogPtr(const char* prefix, void* ptr) {
    char buf[256];
    wsprintfA(buf, "%s%p", prefix, ptr);
    DebugLog(buf);
}

static void InitLogPath() {
    if (GetModuleFileNameA(g_hModule, g_logPath, MAX_PATH)) {
        char* lastSlash = g_logPath;
        for (char* p = g_logPath; *p; p++) {
            if (*p == '\\' || *p == '/') lastSlash = p;
        }
        if (lastSlash != g_logPath) {
            *(lastSlash + 1) = 0;
            lstrcatA(g_logPath, "core_debug.txt");
        }
    } else {
        lstrcpyA(g_logPath, "C:\\core_debug.txt");
    }

    DeleteFileA(g_logPath);
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
    DebugLog("!!! CRASH DETECTED !!!");
    DebugLogHex("Exception Code: ", pExceptionInfo->ExceptionRecord->ExceptionCode);
    DebugLogPtr("Exception Address: ", pExceptionInfo->ExceptionRecord->ExceptionAddress);
    DebugLogPtr("RIP: ", (void*)pExceptionInfo->ContextRecord->Rip);
    DebugLogPtr("RSP: ", (void*)pExceptionInfo->ContextRecord->Rsp);
    DebugLogPtr("RBP: ", (void*)pExceptionInfo->ContextRecord->Rbp);

    HMODULE hMod = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)pExceptionInfo->ExceptionRecord->ExceptionAddress, &hMod)) {
        char modName[MAX_PATH];
        GetModuleFileNameA(hMod, modName, MAX_PATH);
        DebugLog("Crash in module: ");
        DebugLog(modName);

        MODULEINFO modInfo;
        if (GetModuleInformation(GetCurrentProcess(), hMod, &modInfo, sizeof(modInfo))) {
            DebugLogPtr("Module base: ", modInfo.lpBaseOfDll);
            DebugLogPtr("Crash offset: ", (void*)((DWORD_PTR)pExceptionInfo->ExceptionRecord->ExceptionAddress - (DWORD_PTR)modInfo.lpBaseOfDll));
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

bool LoadLogicDll() {
    g_loadAttempted = true;
    DebugLog("=== LoadLogicDll START ===");
    DebugLog(g_logicPath);

    DWORD attrs = GetFileAttributesA(g_logicPath);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        DebugLog("ERROR: logic.dll file not found!");
        g_lastError = ERROR_FILE_NOT_FOUND;
        return false;
    }
    DebugLogHex("File attributes: ", attrs);

    DebugLog("Calling LoadLibraryA...");

    __try {
        g_hLogic = LoadLibraryA(g_logicPath);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DebugLogHex("EXCEPTION in LoadLibraryA! Code: ", GetExceptionCode());
        g_lastError = GetExceptionCode();
        return false;
    }

    if (!g_hLogic) {
        g_lastError = GetLastError();
        DebugLogInt("LoadLibraryA failed, error: ", g_lastError);

        DWORD flags = DONT_RESOLVE_DLL_REFERENCES;
        HMODULE hTest = LoadLibraryExA(g_logicPath, NULL, flags);
        if (hTest) {
            DebugLog("LoadLibraryEx with DONT_RESOLVE_DLL_REFERENCES succeeded - dependency issue!");
            FreeLibrary(hTest);
        } else {
            DebugLog("LoadLibraryEx also failed - file or format issue");
        }
        return false;
    }
    DebugLogPtr("LoadLibraryA success, handle: ", g_hLogic);

    DebugLog("Getting exports...");

    __try {
        g_pfnLogicInit = (Logic_Init_t)GetProcAddress(g_hLogic, "Logic_Init");
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DebugLog("EXCEPTION getting Logic_Init!");
    }
    DebugLogPtr("Logic_Init: ", g_pfnLogicInit);

    __try {
        g_pfnLogicShutdown = (Logic_Shutdown_t)GetProcAddress(g_hLogic, "Logic_Shutdown");
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DebugLog("EXCEPTION getting Logic_Shutdown!");
    }
    DebugLogPtr("Logic_Shutdown: ", g_pfnLogicShutdown);

    __try {
        g_pfnLogicRender = (Logic_Render_t)GetProcAddress(g_hLogic, "Logic_Render");
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DebugLog("EXCEPTION getting Logic_Render!");
    }
    DebugLogPtr("Logic_Render: ", g_pfnLogicRender);

    __try {
        g_pfnLogicWndProc = (Logic_WndProc_t)GetProcAddress(g_hLogic, "Logic_WndProc");
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DebugLog("EXCEPTION getting Logic_WndProc!");
    }
    DebugLogPtr("Logic_WndProc: ", g_pfnLogicWndProc);

    __try {
        g_pfnLogicWantCaptureMouse = (Logic_WantCaptureMouse_t)GetProcAddress(g_hLogic, "Logic_WantCaptureMouse");
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DebugLog("EXCEPTION getting Logic_WantCaptureMouse!");
    }
    DebugLogPtr("Logic_WantCaptureMouse: ", g_pfnLogicWantCaptureMouse);

    bool result = g_pfnLogicInit && g_pfnLogicShutdown && g_pfnLogicRender;
    DebugLogInt("LoadLogicDll result: ", result ? 1 : 0);
    DebugLog("=== LoadLogicDll END ===");

    return result;
}

void UnloadLogicDll() {
    DebugLog("UnloadLogicDll called");
    if (g_pfnLogicShutdown) {
        __try {
            g_pfnLogicShutdown();
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            DebugLog("EXCEPTION in Logic_Shutdown!");
        }
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
    DebugLog("ReloadLogicDll called");
    UnloadLogicDll();
    Sleep(100);
    if (LoadLogicDll()) {
        if (g_pfnLogicInit) {
            DebugLog("Calling Logic_Init after reload");
            __try {
                g_pfnLogicInit(g_pDevice, g_pContext, g_hWnd, ImGui::GetCurrentContext());
                DebugLog("Logic_Init after reload done");
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                DebugLogHex("EXCEPTION in Logic_Init! Code: ", GetExceptionCode());
            }
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
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            DebugLog("EXCEPTION in Logic_WndProc!");
        }
    }

    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
        return true;
    }

    return CallWindowProcA(g_oWndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!g_init) {
        DebugLog("=== hkPresent FIRST CALL ===");

        __try {
            HRESULT hr = pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pDevice);
            if (FAILED(hr)) {
                DebugLogHex("GetDevice failed: ", hr);
                return g_oPresent(pSwapChain, SyncInterval, Flags);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            DebugLog("EXCEPTION in GetDevice!");
            return g_oPresent(pSwapChain, SyncInterval, Flags);
        }

        DebugLogPtr("Got D3D11 Device: ", g_pDevice);
        g_pDevice->GetImmediateContext(&g_pContext);
        DebugLogPtr("Got Context: ", g_pContext);

        DXGI_SWAP_CHAIN_DESC sd;
        pSwapChain->GetDesc(&sd);
        g_hWnd = sd.OutputWindow;
        DebugLogPtr("Got HWND: ", g_hWnd);

        ID3D11Texture2D* pBackBuffer = nullptr;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
        g_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
        pBackBuffer->Release();
        DebugLog("Created RenderTargetView");

        g_oWndProc = (WNDPROC)SetWindowLongPtrA(g_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
        DebugLogPtr("Original WndProc: ", g_oWndProc);

        DebugLog("Creating ImGui context...");
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
        DebugLog("ImGui context created");

        DebugLog("Initializing ImGui Win32...");
        ImGui_ImplWin32_Init(g_hWnd);
        DebugLog("ImGui Win32 initialized");

        DebugLog("Initializing ImGui DX11...");
        ImGui_ImplDX11_Init(g_pDevice, g_pContext);
        DebugLog("ImGui DX11 initialized");

        DebugLog("=== Loading logic.dll ===");
        if (LoadLogicDll()) {
            if (g_pfnLogicInit) {
                DebugLog("Calling Logic_Init...");
                DebugLogPtr("  Device: ", g_pDevice);
                DebugLogPtr("  Context: ", g_pContext);
                DebugLogPtr("  HWND: ", g_hWnd);
                DebugLogPtr("  ImGuiContext: ", ImGui::GetCurrentContext());

                __try {
                    bool initResult = g_pfnLogicInit(g_pDevice, g_pContext, g_hWnd, ImGui::GetCurrentContext());
                    DebugLogInt("Logic_Init returned: ", initResult ? 1 : 0);
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                    DebugLogHex("!!! EXCEPTION in Logic_Init! Code: ", GetExceptionCode());
                }

                DebugLog("Logic_Init call completed");
            }
        } else {
            DebugLog("LoadLogicDll failed!");
        }

        g_init = true;
        DebugLog("=== Initialization complete ===");
    }

    if (g_requestReload) {
        g_requestReload = false;
        ReloadLogicDll();
    }

    __try {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DebugLog("EXCEPTION in ImGui NewFrame!");
    }

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Core", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::Button("Reload Logic DLL", ImVec2(150, 30))) {
            g_requestReload = true;
        }

        ImGui::Text("Logic: %s", g_hLogic ? "Loaded" : "Not Loaded");
        ImGui::Text("Path: %s", g_logicPath);
        ImGui::Text("Log: %s", g_logPath);
        if (g_loadAttempted && !g_hLogic) {
            ImGui::Text("Error: %lu (0x%08X)", g_lastError, g_lastError);
        }
        ImGui::End();
    }

    if (g_pfnLogicRender) {
        __try {
            g_pfnLogicRender();
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            DebugLog("EXCEPTION in Logic_Render!");
            g_pfnLogicRender = nullptr;
        }
    }

    __try {
        ImGui::Render();
        g_pContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DebugLog("EXCEPTION in ImGui Render!");
    }

    return g_oPresent(pSwapChain, SyncInterval, Flags);
}

DWORD WINAPI UnloadThread(LPVOID lpParam) {
    while (true) {
        if (GetAsyncKeyState(VK_END) & 1) {
            break;
        }
        Sleep(100);
    }

    DebugLog("Unload requested");
    UnloadLogicDll();

    if (g_oWndProc && g_hWnd) {
        SetWindowLongPtrA(g_hWnd, GWLP_WNDPROC, (LONG_PTR)g_oWndProc);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (g_pRenderTargetView) g_pRenderTargetView->Release();

    kiero::shutdown();
    DebugLog("Unload complete");

    FreeLibraryAndExitThread(g_hModule, 0);
    return 0;
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    DebugLog("========================================");
    DebugLog("=== LOADER MAIN THREAD STARTED ===");
    DebugLog("========================================");

    SetUnhandledExceptionFilter(CrashHandler);

    FindLogicDll();
    DebugLog("Logic DLL path:");
    DebugLog(g_logicPath);

    Sleep(1000);
    DebugLog("Starting kiero hook...");

    bool hooked = false;
    int attempts = 0;
    while (!hooked && attempts < 100) {
        __try {
            auto status = kiero::init(kiero::RenderType::D3D11);
            DebugLogInt("kiero::init attempt, status: ", (int)status);
            if (status == kiero::Status::Success) {
                kiero::bind(8, (void**)&g_oPresent, hkPresent);
                DebugLog("Hook installed successfully");
                hooked = true;
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            DebugLog("EXCEPTION in kiero::init!");
        }
        Sleep(100);
        attempts++;
    }

    if (!hooked) {
        DebugLog("Failed to hook after 100 attempts");
        return 1;
    }

    CreateThread(nullptr, 0, UnloadThread, nullptr, 0, nullptr);
    DebugLog("MainThread complete, waiting for Present hook");

    return 0;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        InitLogPath();
        DebugLog("========================================");
        DebugLog("=== DLL ATTACH ===");
        DebugLogPtr("Module handle: ", hModule);
        DebugLog("Log file: ");
        DebugLog(g_logPath);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
