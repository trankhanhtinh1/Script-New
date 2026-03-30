#pragma once

#include "../IPlugin.h"
#include "menu/MenuUI.h"

namespace Plugins {

    class RenderTestPlugin : public IPlugin {
    public:
        const char* GetName() const override { return "PluginSandbox"; }
        const char* GetInternalId() const override { return "plugin_sandbox"; }
        const char* GetAuthor() const override { return "NightSharp"; }
        PluginCategory GetCategory() const override { return PluginCategory::Misc; }
        bool AutoLoadByDefault() const override { return true; }

        void OnLoad() override {
            if (m_menu) {
                return;
            }

            m_menu = SDK::MenuUI::Menu::Create("PluginSandboxRoot", "Plugin Sandbox");
            m_menu->Add<SDK::MenuUI::MenuBool>("enabled", "Enabled", true);
            m_menu->Add<SDK::MenuUI::MenuBool>("menuOnly", "Menu-only runtime", true);
            m_menu->Add<SDK::MenuUI::MenuSlider>("revision", "Rewrite Revision", 1, 1, 100);
            m_menu->Add<SDK::MenuUI::MenuList>("stage", "Rewrite Stage",
                std::vector<std::string>{ "Skeleton", "Hooks", "Runtime", "Validation" }, 0);
        }

        void OnUnload() override {
            if (!m_menu) {
                return;
            }

            SDK::MenuUI::Menu::Remove("PluginSandboxRoot");
            m_menu = nullptr;
        }

        SDK::MenuUI::Menu* GetMenuRoot() override {
            return m_menu;
        }

    private:
        SDK::MenuUI::Menu* m_menu = nullptr;
    };

} // namespace Plugins
