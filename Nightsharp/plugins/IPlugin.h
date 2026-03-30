#pragma once

namespace SDK {
    namespace MenuUI {
        class Menu;
    }
}

namespace Plugins {

    enum class PluginCategory {
        Core = 0,
        Champion,
        Utility,
        Misc
    };

    class IPlugin {
    public:
        virtual ~IPlugin() = default;

        virtual const char* GetName() const = 0;
        virtual const char* GetInternalId() const = 0;
        virtual const char* GetAuthor() const { return "NightSharp"; }
        virtual PluginCategory GetCategory() const { return PluginCategory::Core; }
        virtual bool AutoLoadByDefault() const { return false; }
        virtual bool CanLoad() const { return true; }

        virtual void OnLoad() {}
        virtual void OnUnload() {}
        virtual void OnUpdate() {}
        virtual void OnRender() {}
        virtual void OnMenu() {}

        virtual SDK::MenuUI::Menu* GetMenuRoot() { return nullptr; }

        bool IsLoaded() const { return m_loaded; }
        bool IsEnabled() const { return m_enabled; }
        void SetEnabled(bool value) { m_enabled = value; }
        int GetRegistryIndex() const { return m_registryIndex; }

    private:
        friend class PluginManager;
        bool m_loaded = false;
        bool m_enabled = true;
        int m_registryIndex = -1;
    };

} // namespace Plugins
