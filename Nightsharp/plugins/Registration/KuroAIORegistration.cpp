#include "../PluginRegistrars.h"

#include "../PluginManager.h"
#include "../Core/OrbwalkerKuro/AzirSoldierSupport.h"
#include "../Champion/KuroAIO/KuroAIO.h"

namespace Plugins::Registration {

void RegisterKuroAIO(PluginManager& manager) {
    manager.Register<KuroAIOPlugin>();
}

} // namespace Plugins::Registration
