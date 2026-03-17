#pragma once
/*
 * ============================================================================
 *  OverlayRenderer.h — D3D11 device + swapchain for overlay window
 *
 *  Creates its OWN D3D11 device (NOT the game's!) for ImGui rendering.
 *  This means NO vtable hooks on the game's SwapChain.
 * ============================================================================
 */

#include <d3d11.h>
#include <dxgi.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

namespace Overlay {

class Renderer {
public:
    ID3D11Device*           m_device    = nullptr;
    ID3D11DeviceContext*    m_context   = nullptr;
    IDXGISwapChain*         m_swapchain = nullptr;
    ID3D11RenderTargetView* m_rtv       = nullptr;
    bool                    m_imguiInit = false;

    // ================================================================
    // Initialize D3D11 device + swapchain for overlay window
    // ================================================================
    bool Init(HWND overlayHwnd) {
        RECT rect;
        GetClientRect(overlayHwnd, &rect);

        DXGI_SWAP_CHAIN_DESC scd = {};
        scd.BufferCount       = 1;
        scd.BufferDesc.Width  = rect.right - rect.left;
        scd.BufferDesc.Height = rect.bottom - rect.top;
        scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // allows alpha
        scd.BufferDesc.RefreshRate.Numerator   = 0;
        scd.BufferDesc.RefreshRate.Denominator = 1;
        scd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.OutputWindow = overlayHwnd;
        scd.SampleDesc.Count   = 1;
        scd.SampleDesc.Quality = 0;
        scd.Windowed     = TRUE;
        scd.SwapEffect   = DXGI_SWAP_EFFECT_DISCARD;
        scd.Flags        = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        UINT createFlags = 0;
        #ifdef _DEBUG
        createFlags |= D3D11_CREATE_DEVICE_DEBUG;
        #endif

        D3D_FEATURE_LEVEL featureLevel;
        D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createFlags,
            featureLevels, 1,
            D3D11_SDK_VERSION,
            &scd,
            &m_swapchain,
            &m_device,
            &featureLevel,
            &m_context
        );

        if (FAILED(hr)) return false;

        return CreateRTV();
    }

    // ================================================================
    // Initialize ImGui with our D3D11 device
    // ================================================================
    bool InitImGui(HWND overlayHwnd) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.IniFilename = nullptr; // Don't save imgui.ini (stealth)

        ImGui_ImplWin32_Init(overlayHwnd);
        ImGui_ImplDX11_Init(m_device, m_context);

        m_imguiInit = true;
        return true;
    }

    // ================================================================
    // Prepare frame (update display size + timing, but NOT NewFrame yet)
    // After calling this, update io.MousePos/MouseDown manually,
    // then call CommitFrame().
    // ================================================================
    void PrepareFrame() {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        // DO NOT call ImGui::NewFrame() here — caller inserts manual input first
    }

    // ================================================================
    // Commit frame (call ImGui::NewFrame — processes all IO state)
    // ================================================================
    void CommitFrame() {
        ImGui::NewFrame();
    }

    // ================================================================
    // End render frame + Present
    // ================================================================
    void EndFrame() {
        ImGui::Render();

        // Clear with transparent black (alpha = 0)
        const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_context->OMSetRenderTargets(1, &m_rtv, nullptr);
        m_context->ClearRenderTargetView(m_rtv, clearColor);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        m_swapchain->Present(1, 0); // VSync on (1) to save CPU
    }

    // ================================================================
    // Handle resize
    // ================================================================
    void Resize(UINT width, UINT height) {
        if (!m_swapchain || width == 0 || height == 0) return;

        // Release old RTV
        if (m_rtv) { m_rtv->Release(); m_rtv = nullptr; }

        // Resize buffers
        m_swapchain->ResizeBuffers(0, width, height,
                                    DXGI_FORMAT_UNKNOWN, 0);

        // Recreate RTV
        CreateRTV();
    }

    // ================================================================
    // Shutdown
    // ================================================================
    void Shutdown() {
        if (m_imguiInit) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            m_imguiInit = false;
        }
        if (m_rtv)       { m_rtv->Release();       m_rtv = nullptr; }
        if (m_swapchain) { m_swapchain->Release();  m_swapchain = nullptr; }
        if (m_context)   { m_context->Release();    m_context = nullptr; }
        if (m_device)    { m_device->Release();     m_device = nullptr; }
    }

private:
    bool CreateRTV() {
        ID3D11Texture2D* backBuffer = nullptr;
        HRESULT hr = m_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                             (LPVOID*)&backBuffer);
        if (FAILED(hr)) return false;

        hr = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_rtv);
        backBuffer->Release();
        return SUCCEEDED(hr);
    }
};

} // namespace Overlay
