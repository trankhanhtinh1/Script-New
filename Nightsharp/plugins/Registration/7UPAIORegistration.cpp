#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../Champion/7UPAIO/7UPAIO.h"

namespace Plugins::Registration {

void RegisterAIO7UP(PluginManager& manager) {
    manager.Register<AIO7UPPlugin>();
}

} // namespace Plugins::Registration
