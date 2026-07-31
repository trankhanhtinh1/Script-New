#pragma once
// ============================================================================
// PluginBootstrap.h — Plugin registration entry point
//
// Keeps overlay translation units independent from the plugin registration
// implementation and its heavy include graph.
// ============================================================================

namespace Plugins {
namespace PluginBootstrap {

void EnsureRegistered();
void Shutdown();

} // namespace PluginBootstrap
} // namespace Plugins
