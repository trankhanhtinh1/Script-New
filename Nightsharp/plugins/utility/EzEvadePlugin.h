#pragma once
#include "../IPlugin.h"
#include "sdk/EzEvade/Program.h"
#include "sdk/EzEvade/Core/Evade.h"
#include "sdk/UI/MenuUI.h"
#include <memory>

namespace Plugins {

    class EzEvadePlugin : public IPlugin {
    public:
        const char* GetName() const override { return "EzEvade"; }
        const char* GetAuthor() const override { return "NightSharp"; }
        PluginCategory GetCategory() const override { return PluginCategory::Utility; }

        void OnLoad() override {
            EzEvade::Program::Main();
            EzEvade::Evade::SetEnabled(true);
            m_menu = EzEvade::Evade::GetMenu();
        }

        void OnUnload() override {
            EzEvade::Evade::SetEnabled(false);
            m_menu.reset();
        }

        void OnRender() override {
            EzEvade::Evade::OnRender();
        }

        void OnUpdate() override {
            if (!EzEvade::Evade::IsEnabled()) {
                EzEvade::Evade::SetEnabled(true);
            }
        }

        void OnMenu() override {
            if (m_menu) {
                m_menu->Draw();
            }
        }

        SDK::MenuUI::Menu* GetMenuRoot() override {
            return m_menu.get();
        }

    private:
        std::shared_ptr<SDK::MenuUI::Menu> m_menu;
    };

} // namespace Plugins
