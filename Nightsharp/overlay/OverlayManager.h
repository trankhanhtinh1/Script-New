#pragma once

namespace OverlayManager {

enum class Mode {
    Internal = 0,
    External = 1,
};

// Runs the active overlay mode. Blocks until the mode exits or a switch restarts it.
void Run();

// Hotkey path: request the current mode to stop, then restart as the other mode.
void RequestSwitch();

// Final shutdown path used by dllmain/unload.
void RequestShutdown();

// Shutdown plus immediate internal unhook for detach cleanup.
void ShutdownCurrent();

Mode CurrentMode();
const char* CurrentModeName();
bool IsSwitchRequested();

} // namespace OverlayManager
