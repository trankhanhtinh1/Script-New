#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../AwarenessActivator/AwarenessActivatorPlugin.h"

namespace Plugins::Registration {

void RegisterAwarenessActivator(PluginManager& manager) {
    manager.Register<NightSharp::Companion::AwarenessActivatorPlugin>();
}

} // namespace Plugins::Registration
