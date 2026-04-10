#pragma once

#include "../sdk/Core/Objects.h"
#include "../sdk/Wrappers/Orbwalking/OrbwalkerBase.h"
#include "../sdk/Events/AntiGapcloser.h"
#include "../sdk/Events/BuffTracker.h"
#include "../sdk/Events/SpellCastTracker.h"

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
        virtual void OnProcessSpellCast(const SDK::AIBaseClient& sender, const SDK::Events::SpellCast::ProcessSpellCastEventArgs& args) {}

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
            // Add this plugin to the dispatch list (no duplicates)
            bool alreadyRegistered = false;
            for (int i = 0; i < s_pluginCount; ++i) {
                if (s_plugins[i] == this) { alreadyRegistered = true; break; }
            }
            if (!alreadyRegistered && s_pluginCount < kMaxPlugins) {
                s_plugins[s_pluginCount++] = this;
            }

            // Register static trampolines ONCE (prevents duplicate handler entries)
            if (!s_handlersRegistered) {
                s_handlersRegistered = true;
                SDK::Orbwalker::Instance().OnBeforeAttack(S_OnBeforeAttack);
                SDK::Orbwalker::Instance().OnAfterAttack(S_OnAfterAttack);
                SDK::AntiGapcloser::OnGapcloser(S_OnGapcloser);
                SDK::Events::BuffTracker::OnBuffGain([](const SDK::AIBaseClient& sender, const SDK::Events::BuffEventArgs& args) {
                    for (int i = 0; i < s_pluginCount; ++i) {
                        if (s_plugins[i] && s_plugins[i]->m_loaded && s_plugins[i]->m_enabled)
                            s_plugins[i]->OnBuffGain(sender, args);
                    }
                });
                SDK::Events::BuffTracker::OnBuffLose([](const SDK::AIBaseClient& sender, const SDK::Events::BuffEventArgs& args) {
                    for (int i = 0; i < s_pluginCount; ++i) {
                        if (s_plugins[i] && s_plugins[i]->m_loaded && s_plugins[i]->m_enabled)
                            s_plugins[i]->OnBuffLose(sender, args);
                    }
                });
                SDK::Events::SpellCast::AddOnProcessSpellCast(S_OnProcessSpellCast);
            }
        }

        // ── Remove plugin from dispatch list (called on Unload) ──
        void UnregisterEvents() {
            for (int i = 0; i < s_pluginCount; ++i) {
                if (s_plugins[i] == this) {
                    for (int j = i; j < s_pluginCount - 1; ++j)
                        s_plugins[j] = s_plugins[j + 1];
                    s_plugins[--s_pluginCount] = nullptr;
                    break;
                }
            }
        }

    private:
        friend class PluginManager;
        bool m_loaded = false;
        bool m_enabled = true;
        int m_registryIndex = -1;

        // ── Multi-plugin dispatch array ──
        static constexpr int kMaxPlugins = 16;
        static inline IPlugin* s_plugins[kMaxPlugins] = {};
        static inline int s_pluginCount = 0;
        static inline bool s_handlersRegistered = false;

        static void S_OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
            if (args.Type != SDK::OrbwalkingType::BeforeAttack) return;
            for (int i = 0; i < s_pluginCount; ++i) {
                if (s_plugins[i] && s_plugins[i]->m_loaded && s_plugins[i]->m_enabled)
                    s_plugins[i]->OnBeforeAttack(args);
            }
        }
        static void S_OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
            if (args.Type != SDK::OrbwalkingType::AfterAttack) return;
            for (int i = 0; i < s_pluginCount; ++i) {
                if (s_plugins[i] && s_plugins[i]->m_loaded && s_plugins[i]->m_enabled)
                    s_plugins[i]->OnAfterAttack(args);
            }
        }
        static void S_OnGapcloser(const SDK::AIHeroClient& sender, const SDK::AntiGapcloser::GapcloserArgs& args) {
            for (int i = 0; i < s_pluginCount; ++i) {
                if (s_plugins[i] && s_plugins[i]->m_loaded && s_plugins[i]->m_enabled)
                    s_plugins[i]->OnGapcloser(sender, args);
            }
        }
        static void S_OnProcessSpellCast(const SDK::AIBaseClient& sender, const SDK::Events::SpellCast::ProcessSpellCastEventArgs& args) {
            for (int i = 0; i < s_pluginCount; ++i) {
                if (s_plugins[i] && s_plugins[i]->m_loaded && s_plugins[i]->m_enabled)
                    s_plugins[i]->OnProcessSpellCast(sender, args);
            }
        }
    };

} // namespace Plugins
