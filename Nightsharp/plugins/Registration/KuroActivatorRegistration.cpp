#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../KuroActivator/KuroActivatorPlugin.h"

namespace Plugins::Registration {

void RegisterKuroActivator(PluginManager& manager) {
    manager.Register<Plugins::KuroActivator::KuroActivatorPlugin>();
}

} // namespace Plugins::Registration