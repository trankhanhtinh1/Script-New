#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../Core/PlayerEventFilterPlugin.h"

namespace Plugins::Registration {

void RegisterPlayerEventFilter(PluginManager& manager) {
    manager.Register<PlayerEventFilterPlugin>();
}

} // namespace Plugins::Registration
