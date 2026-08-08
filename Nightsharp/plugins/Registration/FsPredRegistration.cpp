#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../Core/FsPred/FsPredPlugin.h"

namespace Plugins::Registration {

void RegisterFsPred(PluginManager& manager) {
    manager.Register<FsPredPlugin>();
}

} // namespace Plugins::Registration
