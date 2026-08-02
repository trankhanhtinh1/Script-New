#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../ZDEvade/ZDEvade.h"

namespace Plugins::Registration {

void RegisterZDEvade(PluginManager& manager) {
    manager.Register<ZDEvadePlugin>();
}

} // namespace Plugins::Registration
