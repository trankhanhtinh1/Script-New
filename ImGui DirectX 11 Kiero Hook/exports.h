#pragma once
#include <d3d11.h>
#include <Windows.h>

struct ImGuiContext;

extern "C" {
    __declspec(dllexport) bool Logic_Init(ID3D11Device* device, ID3D11DeviceContext* context, HWND hwnd, ImGuiContext* imguiContext);
    __declspec(dllexport) void Logic_Shutdown();
    __declspec(dllexport) void Logic_Render();
    __declspec(dllexport) LRESULT Logic_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    __declspec(dllexport) bool Logic_WantCaptureMouse();
}
