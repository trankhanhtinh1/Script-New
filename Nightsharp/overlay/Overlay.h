#pragma once
/*
 * NightSharp - D3D11 ImGui Overlay
 *
 * Creates a transparent Win32 overlay window, initializes D3D11 +
 * DirectComposition, and renders a minimal ImGui menu.
 */

#include <Windows.h>
#include <cstdint>
#include <d3d11.h>
#include <dcomp.h>
#include <dwmapi.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dcomp.lib")

namespace Overlay {

    // Initialize and run the overlay. Blocks until overlay is closed.
    // Call from a worker thread because this owns the message pump.
    void Run();

    // Signal the overlay to shut down.
    void RequestShutdown();

    // True while the render loop is active.
    bool IsRunning();

    // Toggle menu visibility.
    void ToggleMenu();

    // Current menu visibility.
    bool IsMenuVisible();

} // namespace Overlay
