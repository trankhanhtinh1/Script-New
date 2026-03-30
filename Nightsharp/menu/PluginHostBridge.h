#pragma once

namespace PluginHostBridge {

    // External plugin host wiring is currently disabled while the old
    // NightSharp core/sdk tree is being re-integrated. Keep the entrypoint
    // available so overlay/bootstrap code still compiles.
    inline void WireHostAPI() {}

} // namespace PluginHostBridge
