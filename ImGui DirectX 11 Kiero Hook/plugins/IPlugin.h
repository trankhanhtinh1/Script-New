#pragma once
#include <string>

// ============================================================================
// IPlugin — Base interface for all loadable plugins/modules
// Reference: ElUtilitySuite/IPlugin.cs + YamiPortAIO architecture
// ============================================================================

namespace Plugins {

    // Plugin categories (for menu grouping)
    enum class PluginCategory {
        CorePlugin,     // Core systems: Orbwalker, TargetSelector, Prediction, etc.
        Champion,       // Champion-specific scripts (Ezreal, Jinx, etc.)
        Utility,        // Utility systems: awareness, evade, helpers
        Misc,           // Dev/test/other modules

        // Backward-compatible aliases
        Orbwalker = CorePlugin,
        Evade = Utility,
        Other = Misc
    };

    // ========================================================================
    // IPlugin — Every plugin must implement this interface
    // ========================================================================
    class IPlugin {
    public:
        virtual ~IPlugin() = default;

        // Identity
        virtual const char* GetName() const = 0;
        virtual const char* GetAuthor() const = 0;
        virtual PluginCategory GetCategory() const = 0;

        // Lifecycle
        virtual void OnLoad() {}          // Called when plugin is loaded
        virtual void OnUnload() {}        // Called when plugin is unloaded

        // Per-frame callbacks (only called if loaded)
        virtual void OnUpdate() {}        // Game logic (before rendering)
        virtual void OnRender() {}        // Drawing/overlay (during ImGui frame)

        // Menu
        virtual void OnMenu() {}          // Draw plugin-specific ImGui menu items

        // State
        bool IsLoaded() const { return m_loaded; }
        bool IsEnabled() const { return m_enabled; }
        void SetEnabled(bool v) { m_enabled = v; }

    private:
        friend class PluginManager;
        bool m_loaded = false;
        bool m_enabled = true;
    };

} // namespace Plugins
