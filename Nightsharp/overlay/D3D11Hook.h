#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdint>

namespace D3D11Hook {

    // Install hooks on the game's IDXGISwapChain.
    // Returns true on success.
    bool Install();

    // Remove hooks and clean up.
    void Uninstall();

    // True while hooks are active.
    bool IsActive();

    // D3D11 objects obtained from the game's swapchain
    extern ID3D11Device*           g_pd3dDevice;
    extern ID3D11DeviceContext*    g_pd3dContext;
    extern IDXGISwapChain*         g_pSwapChain;
    extern ID3D11RenderTargetView* g_pRenderTargetView;

} // namespace D3D11Hook
