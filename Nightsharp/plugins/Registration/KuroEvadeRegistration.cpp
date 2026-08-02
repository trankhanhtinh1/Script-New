#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../KuroEvade/KuroEvadePlugin.h"

namespace Plugins::Registration {

void RegisterKuroEvade(PluginManager& manager) {
    manager.Register<KuroEvadePlugin>();
}

} // namespace Plugins::Registration
