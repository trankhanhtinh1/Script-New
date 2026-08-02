#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../Core/SpellTrackingDebugPlugin.h"

namespace Plugins::Registration {

void RegisterSpellTrackingDebug(PluginManager& manager) {
    manager.Register<SpellTrackingDebugPlugin>();
}

} // namespace Plugins::Registration
