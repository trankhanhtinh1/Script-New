#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../EzEvade/EzEvadePlugin.h"

namespace Plugins::Registration {

void RegisterEzEvade(PluginManager& manager) {
    manager.Register<EzEvadePlugin>();
}

} // namespace Plugins::Registration
