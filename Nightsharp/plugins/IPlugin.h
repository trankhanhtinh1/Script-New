#pragma once

#include "../sdk/Core/Objects.h"
#include "../sdk/Wrappers/Orbwalking/OrbwalkerBase.h"
#include "../sdk/Events/AntiGapcloser.h"
#include "../sdk/Events/BuffTracker.h"

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

        // ── Plugin Identity ──
        virtual const char* GetName() const = 0;
        virtual const char* GetInternalId() const = 0;
        virtual const char* GetAuthor() const { return "NightSharp"; }
        virtual PluginCategory GetCategory() const { return PluginCategory::Core; }
        virtual bool AutoLoadByDefault() const { return false; }
        virtual bool CanLoad() const { return true; }

        // ── Lifecycle ──
        virtual void OnLoad() {}
        virtual void OnUnload() {}
        virtual void OnUpdate() {}
        virtual void OnRender() {}
        virtual void OnMenu() {}

        // ── Event Callbacks (override these in champion scripts) ──
        virtual void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {}
        virtual void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {}
        virtual void OnGapcloser(const SDK::AIHeroClient& sender, const SDK::AntiGapcloser::GapcloserArgs& args) {}
        virtual void OnBuffGain(const SDK::AIBaseClient& sender, const SDK::Events::BuffEventArgs& args) {}
        virtual void OnBuffLose(const SDK::AIBaseClient& sender, const SDK::Events::BuffEventArgs& args) {}

        virtual SDK::MenuUI::Menu* GetMenuRoot() { return nullptr; }

        // ── Convenience: Player shortcut (like C# ObjectManager.Player) ──
        SDK::AIHeroClient Player() const { return SDK::ObjectManager::Player(); }

        // ── State ──
        bool IsLoaded() const { return m_loaded; }
        bool IsEnabled() const { return m_enabled; }
        void SetEnabled(bool value) { m_enabled = value; }
        int GetRegistryIndex() const { return m_registryIndex; }

        // ── Auto event registration (called by PluginManager after OnLoad) ──
        void RegisterEvents() {
            s_activePlugin = this;
            SDK::Orbwalker::Instance().OnBeforeAttack(S_OnBeforeAttack);
            SDK::Orbwalker::Instance().OnAfterAttack(S_OnAfterAttack);
            SDK::AntiGapcloser::OnGapcloser(S_OnGapcloser);
            SDK::Events::BuffTracker::OnBuffGain([](const SDK::AIBaseClient& sender, const SDK::Events::BuffEventArgs& args) {
                if (s_activePlugin) s_activePlugin->OnBuffGain(sender, args);
            });
            SDK::Events::BuffTracker::OnBuffLose([](const SDK::AIBaseClient& sender, const SDK::Events::BuffEventArgs& args) {
                if (s_activePlugin) s_activePlugin->OnBuffLose(sender, args);
            });
        }

    private:
        friend class PluginManager;
        bool m_loaded = false;
        bool m_enabled = true;
        int m_registryIndex = -1;

        // Static dispatch trampolines
        static inline IPlugin* s_activePlugin = nullptr;

        static void S_OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
            if (s_activePlugin && args.Type == SDK::OrbwalkingType::BeforeAttack)
                s_activePlugin->OnBeforeAttack(args);
        }
        static void S_OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
            if (s_activePlugin && args.Type == SDK::OrbwalkingType::AfterAttack)
                s_activePlugin->OnAfterAttack(args);
        }
        static void S_OnGapcloser(const SDK::AIHeroClient& sender, const SDK::AntiGapcloser::GapcloserArgs& args) {
            if (s_activePlugin) s_activePlugin->OnGapcloser(sender, args);
        }
    };

} // namespace Plugins
