#pragma once

namespace Plugins {

class PluginManager;

namespace Registration {

void RegisterAwarenessActivator(PluginManager& manager);
void RegisterKuroActivator(PluginManager& manager);
void RegisterEzrealCastSpell(PluginManager& manager);
void RegisterJaxCastSpell(PluginManager& manager);
void RegisterXerathSemiNewCastSpell(PluginManager& manager);
void RegisterXerathCastSpell(PluginManager& manager);
void RegisterOrbwalker7UP(PluginManager& manager);
void RegisterKuroTargetSelector(PluginManager& manager);
void RegisterPlayerEventFilter(PluginManager& manager);
void RegisterSpellTrackingDebug(PluginManager& manager);
void RegisterObjectDefinitionDraw(PluginManager& manager);
void RegisterDeveloperTools(PluginManager& manager);
void RegisterEzrealMissileLifecycle(PluginManager& manager);
void RegisterAIO7UP(PluginManager& manager);
void RegisterSharpShooterAIO(PluginManager& manager);
void RegisterZiblldev9898(PluginManager& manager);
void RegisterEzEvade(PluginManager& manager);
void RegisterKuroEvade(PluginManager& manager);
void RegisterZDEvade(PluginManager& manager);
void RegisterKuroAIO(PluginManager& manager);

} // namespace Registration
} // namespace Plugins
