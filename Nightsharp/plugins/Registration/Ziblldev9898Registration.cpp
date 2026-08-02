#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../Champion/ziblldev9898/ziblldev9898.h"

namespace Plugins::Registration {

void RegisterZiblldev9898(PluginManager& manager) {
    manager.Register<Ziblldev9898Plugin>();
}

} // namespace Plugins::Registration
