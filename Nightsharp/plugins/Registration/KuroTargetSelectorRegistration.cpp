#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../Core/KuroTargetSelector/KuroTargetSelectorPlugin.h"

namespace Plugins::Registration {

void RegisterKuroTargetSelector(PluginManager& manager) {
    manager.Register<KuroTargetSelector::KuroTargetSelectorPlugin>();
}

} // namespace Plugins::Registration
