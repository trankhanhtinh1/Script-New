#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../Champion/EzrealMissileLifecyclePlugin.h"

namespace Plugins::Registration {

void RegisterEzrealMissileLifecycle(PluginManager& manager) {
    manager.Register<EzrealMissileLifecyclePlugin>();
}

} // namespace Plugins::Registration
