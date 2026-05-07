#pragma once

// ============================================================================
// IPlugin.h - Phase 3.1 (Apr 26/2026): full port from old source.
// Provides plugin lifecycle interface + auto event-dispatch wiring.
// Plugins override OnBeforeAttack/OnAfterAttack/OnGapcloser/OnBuffGain/
// OnProcessSpellCast and they get fanned out from a single static trampoline
// per event registered with the SDK.
// ============================================================================

#include "../sdk/GameObjects/ObjectManager.h"
#include "../sdk/Wrappers/Orbwalking/Orbwalker.h"
#include "../sdk/Events/AntiGapcloser.h"
#include "../sdk/Events/BuffTracker.h"
#include "../sdk/Events/SpellCastTracker.h"

#include <string>

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
        virtual const char* GetChampionName() const { return nullptr; }
        virtual bool AutoLoadByDefault() const { return false; }
        virtual bool CanLoad() const {
            const char* championName = GetChampionName();
            if (!championName || !championName[0]) return true;

            const auto player = SDK::ObjectManager::Player();
            if (!player.IsValid()) return false;

            const std::string currentChampion = player.CharacterName();
            return currentChampion == championName;
        }

        // ── Lifecycle ──
        virtual void OnLoad()   {}
        virtual void OnUnload() {}
        virtual void OnUpdate() {}
        virtual void OnRender() {}
        virtual void OnMenu()   {}

        // ── Event callbacks (override in champion / utility plugins) ──
        virtual void OnBeforeAttack(SDK::OrbwalkingActionArgs& /*args*/) {}
        virtual void OnAfterAttack (SDK::OrbwalkingActionArgs& /*args*/) {}
        virtual void OnGapcloser(const SDK::AIHeroClient& /*sender*/,
                                 const SDK::AntiGapcloser::GapcloserArgs& /*args*/) {}
        // Phase 3.1 (Apr 26/2026): only OnBuffUpdate is delivered by the
        // game's BuffManager dispatcher. The legacy OnBuffGain/OnBuffLose
        // pair was misleading - OnBuffGain was just an alias of OnBuffUpdate
        // and OnBuffLose was never fired. Use IsActive(gameTime) inside
        // OnBuffUpdate to detect transitions if you need gain/lose semantics.
        virtual void OnBuffUpdate(const SDK::AIBaseClient& /*sender*/,
                                  const SDK::Events::BuffEventArgs& /*args*/) {}
        virtual void OnProcessSpellCast(const SDK::AIBaseClient& /*sender*/,
                                        const SDK::Events::SpellCast::ProcessSpellCastEventArgs& /*args*/) {}

        virtual SDK::MenuUI::Menu* GetMenuRoot() { return nullptr; }

        // ── Convenience: Player shortcut (matches C# ObjectManager.Player) ──
        SDK::AIHeroClient Player() const { return SDK::ObjectManager::Player(); }

        // ── State ──
        bool IsLoaded()  const { return m_loaded; }
        bool IsEnabled() const { return m_enabled; }
        void SetEnabled(bool value) { m_enabled = value; }
        int  GetRegistryIndex() const { return m_registryIndex; }

        // ── Auto event registration (called by PluginManager after OnLoad) ──
        // Adds this plugin to the global dispatch list and (once globally)
        // registers static trampolines with the SDK that fan out to every
        // registered plugin. Idempotent.
        void RegisterEvents() {
            // 1) Add this plugin to dispatch list (no duplicates).
            bool alreadyRegistered = false;
            for (int i = 0; i < s_pluginCount; ++i) {
                if (s_plugins[i] == this) { alreadyRegistered = true; break; }
            }
            if (!alreadyRegistered && s_pluginCount < kMaxPlugins) {
                s_plugins[s_pluginCount++] = this;
            }

            // 2) Wire static trampolines exactly ONCE process-wide.
            if (!s_handlersRegistered) {
                s_handlersRegistered = true;
                SDK::Orbwalker::OnBeforeAttack(&S_OnBeforeAttack);
                SDK::Orbwalker::OnAfterAttack (&S_OnAfterAttack);
                SDK::AntiGapcloser::OnGapcloser(&S_OnGapcloser);
                SDK::Events::BuffTracker::OnBuffUpdate(
                    [](const SDK::AIBaseClient& sender, const SDK::Events::BuffEventArgs& args) {
                        for (int i = 0; i < s_pluginCount; ++i) {
                            if (s_plugins[i] && s_plugins[i]->m_loaded && s_plugins[i]->m_enabled)
                                s_plugins[i]->OnBuffUpdate(sender, args);
                        }
                    });
                SDK::Events::SpellCast::AddOnProcessSpellCast(&S_OnProcessSpellCast);
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
        int  m_registryIndex = -1;

        // ── Multi-plugin dispatch array ──
        static constexpr int kMaxPlugins = 16;
        static inline IPlugin* s_plugins[kMaxPlugins] = {};
        static inline int      s_pluginCount = 0;
        static inline bool     s_handlersRegistered = false;

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
        static void S_OnGapcloser(const SDK::AIHeroClient& sender,
                                  const SDK::AntiGapcloser::GapcloserArgs& args) {
            for (int i = 0; i < s_pluginCount; ++i) {
                if (s_plugins[i] && s_plugins[i]->m_loaded && s_plugins[i]->m_enabled)
                    s_plugins[i]->OnGapcloser(sender, args);
            }
        }
        static void S_OnProcessSpellCast(const SDK::AIBaseClient& sender,
                                         const SDK::Events::SpellCast::ProcessSpellCastEventArgs& args) {
            for (int i = 0; i < s_pluginCount; ++i) {
                if (s_plugins[i] && s_plugins[i]->m_loaded && s_plugins[i]->m_enabled)
                    s_plugins[i]->OnProcessSpellCast(sender, args);
            }
        }
    };

} // namespace Plugins
