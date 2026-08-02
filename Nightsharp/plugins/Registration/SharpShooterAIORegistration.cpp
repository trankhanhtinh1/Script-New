#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../Champion/SharpShooterAIO/SharpShooterAIO.h"

namespace Plugins::Registration {

void RegisterSharpShooterAIO(PluginManager& manager) {
    manager.Register<SharpShooterAIOPlugin>();
}

} // namespace Plugins::Registration
