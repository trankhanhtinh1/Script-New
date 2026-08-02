#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../Champion/EzrealSemiPlugin.h"
#include "../Champion/JaxSemiPlugin.h"
#include "../Champion/XerathSemiNewCastSpell.h"
#include "../Champion/XerathSemiPlugin.h"

namespace Plugins::Registration {

void RegisterEzrealCastSpell(PluginManager& manager) {
    manager.Register<EzrealSemiPlugin>();
}

void RegisterJaxCastSpell(PluginManager& manager) {
    manager.Register<JaxSemiPlugin>();
}

void RegisterXerathSemiNewCastSpell(PluginManager& manager) {
    manager.Register<XerathSemiNewCastSpell>();
}

void RegisterXerathCastSpell(PluginManager& manager) {
    manager.Register<XerathSemiPlugin>();
}

} // namespace Plugins::Registration
