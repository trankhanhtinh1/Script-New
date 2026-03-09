#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include "kiero/kiero.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

// Core
#include "core/Vector.h"
#include "core/Offsets.h"
#include "core/LeagueObfuscation.h"
#include "core/Globals.h"
#include "sdk/Utils/DebugConsole.h"

// Menu
#include "menu/MenuConfig.h"
#include "menu/Menu.h"

typedef HRESULT(__stdcall* Present) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef uintptr_t PTR;
