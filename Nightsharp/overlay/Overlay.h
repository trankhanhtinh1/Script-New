#pragma once
/*
 * NightSharp v2.0 — D3D11 Overlay Engine
 *
 * Creates a transparent topmost window with its own D3D11 device.
 * Renders ImGui with CRT-free custom allocator (HeapAlloc).
 * NightSharpMenu 3-panel sidebar drawn each frame.
 * F1 toggles menu visibility / click-through.
 */

#include <Windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <cstdint>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")

namespace Overlay {

    // Initialize and run the overlay. Blocks until overlay is closed.
    // Call from a worker thread — this runs the message pump + render loop.
    void Run();

    // Signal the overlay to shut down (thread-safe).
    void RequestShutdown();

    // True while the render loop is active.
    bool IsRunning();

    // Toggle menu visibility (no-op in raw substrate experiment).
    void ToggleMenu();

    // Check if menu is currently visible (always false in raw substrate).
    bool IsMenuVisible();

} // namespace Overlay
