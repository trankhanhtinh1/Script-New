#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include "kiero/kiero.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <Psapi.h>

#include <vector>
#include <map>
#include <variant>
#include <TlHelp32.h>
#include <directxmath.h>
#include <winternl.h>
#include <string>
#include <sstream>
#include <fstream>
#include <cctype>
#include <format>

#include <dxgi.h>
#include <d3d11.h>
#include <d3d10.h>
#include <d3d9.h>



#include "kiero/kiero.h"
#include "kiero/minhook/include/MinHook.h"

typedef HRESULT(__stdcall* Present) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef uintptr_t PTR;

#include "Console.h"