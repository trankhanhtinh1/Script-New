#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../Utility/ObjectDefinitionDrawPlugin.h"

namespace Plugins::Registration {

void RegisterObjectDefinitionDraw(PluginManager& manager) {
    manager.Register<ObjectDefinitionDrawPlugin>();
}

} // namespace Plugins::Registration
