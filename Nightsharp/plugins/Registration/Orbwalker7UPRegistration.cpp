#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../Core/Orbwalker7UP/Orbwalker7UP.h"

namespace Plugins::Registration {

void RegisterOrbwalker7UP(PluginManager& manager) {
    manager.Register<Orbwalker7UPPlugin>();
}

} // namespace Plugins::Registration
