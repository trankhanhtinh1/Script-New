#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../Utility/DeveloperToolsPluginOld.h"

namespace Plugins::Registration {

void RegisterDeveloperTools(PluginManager& manager) {
    manager.Register<DeveloperToolsPluginOld>();
}

} // namespace Plugins::Registration
