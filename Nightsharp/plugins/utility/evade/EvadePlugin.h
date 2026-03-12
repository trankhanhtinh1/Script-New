#pragma once
// ============================================================================
// EvadePlugin.h — Main plugin entry point
// Connects EvadeCore + SpellDetector + EvadeDrawer + Menu
// ============================================================================

#include "../../IPlugin.h"
#include "EvadeCore.h"
#include "EvadeDrawer.h"
#include "EvadeMenuBuilder.h"
#include "sdk/UI/MenuUI.h"
#include <memory>

namespace Plugins {

    class EvadePlugin : public IPlugin {
    public:
        const char* GetName() const override { return "Evade"; }
        const char* GetAuthor() const override { return "NightSharp"; }
        PluginCategory GetCategory() const override { return PluginCategory::Utility; }

        void OnLoad() override {
            SDK::MenuUI::Menu::Remove("Evade");
            m_menu = EvadeMenu::BuildEvadeMenu();

            // Initialize core systems
            ::Evade::EvadeCore::Instance().Initialize();
            ::Plugins::Evade::EvadeDrawer::Initialize();

            // Connect SpellDetector menu check function
            ::Evade::SpellDetector::Instance().SetMenuCheckFunction(
                [this](const std::string& spellName, int dangerLevel) -> bool {
                    return IsSpellEnabledInMenu(spellName, dangerLevel);
                });

            // Sync menu config to EvadeCore
            SyncMenuToConfig();
        }

        void OnUnload() override {
            ::Evade::EvadeCore::Instance().Shutdown();
            ::Plugins::Evade::EvadeDrawer::Shutdown();
            SDK::MenuUI::Menu::Remove("Evade");
            m_menu.reset();
        }

        void OnUpdate() override {
            if (!m_menu) return;

            // Sync menu values every tick
            SyncMenuToConfig();

            // Run evade core logic
            float gameTime = SDK::Game::GetTime();
            ::Evade::EvadeCore::Instance().OnUpdate(gameTime);

            // Run drawer update
            ::Plugins::Evade::EvadeDrawer::Update(m_menu.get());
        }

        void OnRender() override {
            ::Plugins::Evade::EvadeDrawer::Draw(m_menu.get());
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

        // Sync menu settings to EvadeCore config
        void SyncMenuToConfig() {
            if (!m_menu) return;
            auto& config = ::Evade::EvadeCore::Instance().GetConfig();

            // Root controls (keybinds read from menu)
            config.Enabled = m_menu->GetBoolValue("EnableEvade", true);
            config.DodgeOnlyDangerous = m_menu->GetBoolValue("DodgeOnlyDangerous", false);
            config.DodgeOnlyCC = m_menu->GetBoolValue("DodgeOnlyCC", false);
            config.DiveTurretMode = m_menu->GetBoolValue("DiveTurretMode", false);
            config.DiveTurretDangerLevel = m_menu->GetSliderValue("DiveTurretDangerLevel", 5);

            // Evade settings
            config.ReactionTimeMs = m_menu->GetSliderValue("ReactionTime", 0);
            config.ExtraEvadeDistance = (float)m_menu->GetSliderValue("ExtraEvadeDistance", 100);
            config.ExtraPingBuffer = (float)m_menu->GetSliderValue("ExtraPingBuffer", 65);
            config.HumanizerDelayMs = m_menu->GetSliderValue("HumanizerDelay", 0);
            config.HigherPrecision = m_menu->GetBoolValue("HigherPrecision", false);

            // Phase 5 additions
            config.DodgeFowSkillshots = m_menu->GetBoolValue("DodgeFowSkillshots", true);
            config.PreventDodgeNearEnemies = m_menu->GetBoolValue("PreventDodgeNearEnemies", true);
            config.MinComfortZone = (float)m_menu->GetSliderValue("MinComfortZone", 550);

            // Phase 7 improvements
            // 7.2 Damage-Aware
            config.DamageAwareDodge = m_menu->GetBoolValue("DamageAwareDodge", false);
            config.DamageAwareDangerThreshold = m_menu->GetSliderValue("DamageAwareDangerThreshold", 3);
            config.DamageSkipHpPercent = (float)m_menu->GetSliderValue("DamageSkipHpPercent", 50) / 100.0f;

            // 7.3 Health-Aware
            config.HealthAwareEvade = m_menu->GetBoolValue("HealthAwareEvade", false);
            config.LowHpThreshold = (float)m_menu->GetSliderValue("LowHpThreshold", 30) / 100.0f;
            config.HighHpThreshold = (float)m_menu->GetSliderValue("HighHpThreshold", 70) / 100.0f;

            // 7.4 Smooth Path
            config.SmoothPath = m_menu->GetBoolValue("SmoothPath", false);
            config.MicroOffsetMax = (float)m_menu->GetSliderValue("MicroOffsetMax", 15);

            // 7.8-7.9
            config.LatencyAdaptive = m_menu->GetBoolValue("LatencyAdaptive", true);
            config.ChampionAwareDodge = m_menu->GetBoolValue("ChampionAwareDodge", false);

            // Wall Awareness
            config.AvoidNearWall = m_menu->GetBoolValue("AvoidNearWall", true);
            config.WallBuffer = (float)m_menu->GetSliderValue("WallBuffer", 65);

            // Sync FoW detection to spell detector
            ::Evade::SpellDetector::Instance().GetConfig().EnableFowDetection = config.DodgeFowSkillshots;
        }

        // Check if a specific spell is enabled in the menu
        bool IsSpellEnabledInMenu(const std::string& spellName, int dangerLevel) const {
            if (!m_menu) return true;
            // Default: all spells enabled. Menu can disable specific ones.
            std::string menuId = EvadeMenu::NormalizeId(spellName);
            return m_menu->GetBoolValue(menuId + "_enabled", true);
        }
    };

} // namespace Plugins
