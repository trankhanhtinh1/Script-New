#pragma once
#include "EvadeMenuData.h"
#include "sdk/UI/MenuUI.h"
#include <memory>
#include <vector>

namespace Plugins::EvadeMenu {

    inline void AddRootOptions(const std::shared_ptr<SDK::MenuUI::Menu>& root) {
        root->Add<SDK::MenuUI::MenuKeyBind>(
            "EnableEvade", "Enable Evade", 'K', SDK::MenuUI::KeyBindType::Toggle, true);
        root->Add<SDK::MenuUI::MenuKeyBind>(
            "DodgeOnlyDangerous", "Dodge Only Dangerous", VK_SPACE, SDK::MenuUI::KeyBindType::Press);
        root->Add<SDK::MenuUI::MenuKeyBind>(
            "DodgeOnlyCC", "Dodge Only CC", 'L', SDK::MenuUI::KeyBindType::Toggle, false);
        root->Add<SDK::MenuUI::MenuKeyBind>(
            "DiveTurretMode", "Dive Turret Mode", 'T', SDK::MenuUI::KeyBindType::Toggle, false)
            ->SetTooltip("Only dodge danger level matching slider while this mode is active.");
        root->Add<SDK::MenuUI::MenuSlider>(
            "DiveTurretDangerLevel", "Dive Turret Min Danger Level", 5, 1, 5)
            ->SetTooltip("Minimum danger level to dodge when Dive Turret Mode is ON.");
    }

    inline void AddEvadeSettings(const std::shared_ptr<SDK::MenuUI::Menu>& root) {
        auto menu = root->AddSubMenu("EvadeSettings", "Evade Settings");

        auto modeList = menu->Add<SDK::MenuUI::MenuList>(
            "EvadeMode", "Evade Mode",
            std::vector<std::string>{ "Fast", "Balanced", "Smoother", "Human", "Max" }, 1);

        auto disableEvadeForKill = menu->Add<SDK::MenuUI::MenuSlider>(
            "DisableEvadeForKill", "Disable Evade for kill if % health enemy", 5, 0, 100);
        auto reactionTime = menu->Add<SDK::MenuUI::MenuSlider>("ReactionTime", "Reaction Time", 50, 0, 300);
        auto extraDistance = menu->Add<SDK::MenuUI::MenuSlider>("ExtraEvadeDistance", "Extra Evade Distance", 35, 0, 150);
        auto evadePrecision = menu->Add<SDK::MenuUI::MenuSlider>("EvadePrecision", "Evade Precision", 50, 8, 100);
        auto maxCalcTime = menu->Add<SDK::MenuUI::MenuSlider>("MaxEvadeCalcTime", "Max Evade Calc Time", 8, 1, 20);
        auto dodgeIfHitIn = menu->Add<SDK::MenuUI::MenuSlider>("DodgeIfHitIn", "Dodge If Hit In", 250, 0, 1000);
        auto humanizerDelay = menu->Add<SDK::MenuUI::MenuSlider>("HumanizerDelay", "Humanizer Delay", 50, 0, 200);
        auto maxSkillshots = menu->Add<SDK::MenuUI::MenuSlider>("MaxSkillshots", "Max Skillshots", 12, 1, 20);
        auto minDangerLevel = menu->Add<SDK::MenuUI::MenuSlider>("MinimumDangerLevel", "Minimum Danger Level", 1, 1, 5);
        auto maxEvadePathDist = menu->Add<SDK::MenuUI::MenuSlider>("MaxEvadePathDistance", "Max Evade Path Distance", 350, 100, 600);
        auto fastEvadeThreshold = menu->Add<SDK::MenuUI::MenuSlider>("FastEvadeThreshold", "Fast Evade Threshold", 2000, 1200, 4000);

        modeList->OnValueChanged([=](const SDK::MenuUI::MenuValueChangedEventArgs& args) {
            auto* list = static_cast<SDK::MenuUI::MenuList*>(args.ChangedItem);
            if (list == nullptr) {
                return;
            }

            switch (list->Index) {
            case 0: // Fast — dodge everything ASAP, minimal delay
                reactionTime->Value = 0;
                extraDistance->Value = 25;
                evadePrecision->Value = 18;
                maxCalcTime->Value = 6;
                dodgeIfHitIn->Value = 320;
                humanizerDelay->Value = 0;
                maxSkillshots->Value = 14;
                minDangerLevel->Value = 1;
                maxEvadePathDist->Value = 380;
                fastEvadeThreshold->Value = 1400;
                disableEvadeForKill->Value = 0;
                break;
            case 1: // Balanced — good for most situations
                reactionTime->Value = 45;
                extraDistance->Value = 35;
                evadePrecision->Value = 40;
                maxCalcTime->Value = 8;
                dodgeIfHitIn->Value = 260;
                humanizerDelay->Value = 35;
                maxSkillshots->Value = 12;
                minDangerLevel->Value = 1;
                maxEvadePathDist->Value = 360;
                fastEvadeThreshold->Value = 1800;
                disableEvadeForKill->Value = 5;
                break;
            case 2: // Smoother — natural looking dodges
                reactionTime->Value = 60;
                extraDistance->Value = 50;
                evadePrecision->Value = 55;
                maxCalcTime->Value = 9;
                dodgeIfHitIn->Value = 230;
                humanizerDelay->Value = 25;
                maxSkillshots->Value = 10;
                minDangerLevel->Value = 1;
                maxEvadePathDist->Value = 380;
                fastEvadeThreshold->Value = 2000;
                disableEvadeForKill->Value = 8;
                break;
            case 3: // Human — realistic delay, skips low danger
                reactionTime->Value = 110;
                extraDistance->Value = 35;
                evadePrecision->Value = 60;
                maxCalcTime->Value = 10;
                dodgeIfHitIn->Value = 170;
                humanizerDelay->Value = 70;
                maxSkillshots->Value = 8;
                minDangerLevel->Value = 2;
                maxEvadePathDist->Value = 380;
                fastEvadeThreshold->Value = 2300;
                disableEvadeForKill->Value = 12;
                break;
            case 4: // Max — dodge EVERYTHING, max performance
                reactionTime->Value = 0;
                extraDistance->Value = 15;
                evadePrecision->Value = 10;
                maxCalcTime->Value = 15;
                dodgeIfHitIn->Value = 900;
                humanizerDelay->Value = 0;
                maxSkillshots->Value = 18;
                minDangerLevel->Value = 1;
                maxEvadePathDist->Value = 450;
                fastEvadeThreshold->Value = 1200;
                disableEvadeForKill->Value = 0;
                break;
            default:
                break;
            }
        });

        // disableEvadeForKill is used in presets above

        // Higher precision option
        menu->Add<SDK::MenuUI::MenuBool>("HigherPrecision", "Higher Precision (more candidates, slower)", false);
    }

    inline void AddEvadeSpellSettings(const std::shared_ptr<SDK::MenuUI::Menu>& root) {
        auto menu = root->AddSubMenu("EvadeSpells", "Evade Spells");
        menu->Add<SDK::MenuUI::MenuBool>("EnableEvadeSpells", "Enable Evade Spells", true);
        menu->Add<SDK::MenuUI::MenuSlider>(
            "UseEvadeSpellsDangerLevel", "Use Evade Spells If Danger >=", 3, 1, 5);

        auto flashMenu = menu->AddSubMenu("Flash", "Flash");
        flashMenu->Add<SDK::MenuUI::MenuBool>("Use", "Use", true);
        flashMenu->Add<SDK::MenuUI::MenuSlider>("DangerLevel", "Danger Level", 4, 1, 5);

        auto championMenu = menu->AddSubMenu("ChampionEvadeSpells", "Champion Evade Spells");
        auto entries = GetChampionEvadeSpellEntries();
        if (entries.empty()) {
            championMenu->Add<SDK::MenuUI::MenuSeparator>("empty", "No evade spell entries for the current champion.");
            return;
        }

        for (const auto& entry : entries) {
            auto spellMenu = championMenu->AddSubMenu(entry.InternalName, entry.DisplayName);
            spellMenu->Add<SDK::MenuUI::MenuBool>("Use", "Use", true);
            spellMenu->Add<SDK::MenuUI::MenuSlider>("DangerLevel", "Danger Level", entry.DefaultDangerLevel, 1, 5);
        }
    }

    inline void AddTrapSettings(const std::shared_ptr<SDK::MenuUI::Menu>& root) {
        auto menu = root->AddSubMenu("TrapSettings", "Trap Settings");
        auto traps = GetEnemyTrapTemplates();
        if (traps.empty()) {
            menu->Add<SDK::MenuUI::MenuSeparator>("empty", "No supported trap champions in the current enemy team.");
            return;
        }

        for (const auto& trap : traps) {
            auto championMenu = menu->AddSubMenu(NormalizeId(trap.Champion), trap.Champion);
            auto spellMenu = championMenu->AddSubMenu(NormalizeId(trap.Slot), trap.Slot);
            spellMenu->Add<SDK::MenuUI::MenuBool>("Enable", "Enable", true);
            spellMenu->Add<SDK::MenuUI::MenuSlider>(
                "DangerLevel", "Danger Level", trap.DefaultDangerLevel, 1, 5);
            spellMenu->Add<SDK::MenuUI::MenuSlider>(
                "ExtraDistance", "Extra Distance", trap.DefaultExtraDistance, 0, 100);
        }
    }

    inline void AddSkillshotSettings(const std::shared_ptr<SDK::MenuUI::Menu>& root) {
        auto menu = root->AddSubMenu("SkillshotSettings", "Skillshot Settings");
        auto groups = GetEnemySkillshotEntries();
        bool hasEntries = false;

        for (const auto& [champion, spells] : groups) {
            if (spells.empty()) {
                continue;
            }

            hasEntries = true;
            auto championMenu = menu->AddSubMenu(NormalizeId(champion), champion);
            for (const auto& spell : spells) {
                auto spellMenu = championMenu->AddSubMenu(spell.InternalName, spell.DisplayName);
                spellMenu->Add<SDK::MenuUI::MenuBool>("Dodge", "Dodge", true);
                spellMenu->Add<SDK::MenuUI::MenuSlider>(
                    "DangerLevel", "Danger Level", spell.DefaultDangerLevel, 1, 5);
                spellMenu->Add<SDK::MenuUI::MenuBool>("Draw", "Draw", spell.DefaultDraw);
                spellMenu->Add<SDK::MenuUI::MenuSlider>("ExtraRadius", "Extra Radius", 0, 0, 100);
            }
        }

        if (!hasEntries) {
            menu->Add<SDK::MenuUI::MenuSeparator>("empty", "No enemy skillshots found for the current game.");
        }
    }

    // ================================================================
    // Phase 5 — Detection Settings (FoW, comfort zone, ping buffer)
    // ================================================================
    inline void AddDetectionSettings(const std::shared_ptr<SDK::MenuUI::Menu>& root) {
        auto menu = root->AddSubMenu("DetectionSettings", "Detection Settings");

        menu->Add<SDK::MenuUI::MenuBool>("DodgeFowSkillshots", "Dodge FoW Skillshots", true)
            ->SetTooltip("Dodge skillshots launched from fog of war.");
        menu->Add<SDK::MenuUI::MenuBool>("PreventDodgeNearEnemies", "Prevent Dodge Near Enemies", true)
            ->SetTooltip("Avoid dodging into close proximity of enemy champions.");
        menu->Add<SDK::MenuUI::MenuSlider>("MinComfortZone", "Min Comfort Zone", 550, 200, 1000)
            ->SetTooltip("Minimum distance to enemies when choosing dodge position.");
        menu->Add<SDK::MenuUI::MenuSlider>("ExtraPingBuffer", "Extra Ping Buffer (ms)", 65, 0, 200)
            ->SetTooltip("Extra buffer added to dodge timing for ping compensation.");
    }

    // ================================================================
    // Phase 7 — Improvement Settings
    // ================================================================
    inline void AddImprovementSettings(const std::shared_ptr<SDK::MenuUI::Menu>& root) {
        auto menu = root->AddSubMenu("Improvements", "Improvements");

        // 7.2 Damage-Aware
        auto dmgMenu = menu->AddSubMenu("DamageAware", "Damage-Aware Dodge");
        dmgMenu->Add<SDK::MenuUI::MenuBool>("DamageAwareDodge", "Enable Damage-Aware Dodge", false)
            ->SetTooltip("Skip dodging low-danger spells when HP is high enough.");
        dmgMenu->Add<SDK::MenuUI::MenuSlider>("DamageAwareDangerThreshold", "Always Dodge If Danger >=", 3, 1, 5)
            ->SetTooltip("Spells with danger >= this are ALWAYS dodged regardless of HP.");
        dmgMenu->Add<SDK::MenuUI::MenuSlider>("DamageSkipHpPercent", "Skip Dodge If HP Above (%)", 50, 10, 100)
            ->SetTooltip("Skip dodging low-danger spells when HP is above this percentage.");

        // 7.3 Health-Aware Evade
        auto healthMenu = menu->AddSubMenu("HealthAware", "Health-Aware Evade");
        healthMenu->Add<SDK::MenuUI::MenuBool>("HealthAwareEvade", "Enable Health-Aware Evade", false)
            ->SetTooltip("Adjust dodge aggressiveness based on current HP.");
        healthMenu->Add<SDK::MenuUI::MenuSlider>("LowHpThreshold", "Low HP Threshold (%)", 30, 5, 50)
            ->SetTooltip("Below this HP% = dodge everything.");
        healthMenu->Add<SDK::MenuUI::MenuSlider>("HighHpThreshold", "High HP Threshold (%)", 70, 40, 100)
            ->SetTooltip("Above this HP% = accept low danger spells.");

        // 7.4 Smooth Path (Anti-Bot)
        menu->Add<SDK::MenuUI::MenuBool>("SmoothPath", "Smooth Path (Anti-Bot)", false)
            ->SetTooltip("Add micro-offsets to dodge point for more human-like movement.");
        menu->Add<SDK::MenuUI::MenuSlider>("MicroOffsetMax", "Micro Offset Max (units)", 15, 5, 30)
            ->SetTooltip("Maximum random offset applied to dodge point.");

        // 7.8 Latency-Adaptive
        menu->Add<SDK::MenuUI::MenuBool>("LatencyAdaptive", "Latency-Adaptive", true)
            ->SetTooltip("Use real-time ping for dodge timing instead of fixed buffer.");

        // 7.9 Champion-Aware
        menu->Add<SDK::MenuUI::MenuBool>("ChampionAwareDodge", "Champion-Aware Dodge", false)
            ->SetTooltip("ADC stays in AA range, mage keeps distance, etc.");

        // Wall Awareness
        auto wallMenu = menu->AddSubMenu("WallAwareness", "Wall Awareness");
        wallMenu->Add<SDK::MenuUI::MenuBool>("AvoidNearWall", "Avoid Dodging Near Walls", true)
            ->SetTooltip("Push dodge point away from walls to maintain escape space.");
        wallMenu->Add<SDK::MenuUI::MenuSlider>("WallBuffer", "Wall Buffer Distance", 65, 20, 150)
            ->SetTooltip("Minimum distance to maintain from walls when dodging.");
    }

    // ================================================================
    // Debug Settings
    // ================================================================
    inline void AddDebugSettings(const std::shared_ptr<SDK::MenuUI::Menu>& root) {
        auto menu = root->AddSubMenu("Debug", "Debug");
        menu->Add<SDK::MenuUI::MenuBool>("DrawDetectionPanel", "Draw Detection Panel", true);
        menu->Add<SDK::MenuUI::MenuBool>("LogDetectionConsole", "Log Detection Console", true);
        menu->Add<SDK::MenuUI::MenuBool>("DrawEvadePosition", "Draw Evade Position", false);
        menu->Add<SDK::MenuUI::MenuBool>("DrawSafePosition", "Draw Safe Position", false);
        menu->Add<SDK::MenuUI::MenuBool>("DrawSkillshotPolygon", "Draw Skillshot Polygon", false);
        menu->Add<SDK::MenuUI::MenuBool>("DrawSkillshotDirection", "Draw Skillshot Direction", false);
        menu->Add<SDK::MenuUI::MenuBool>("DrawDangerLevel", "Draw Danger Level", false);
        menu->Add<SDK::MenuUI::MenuBool>("DrawEvadeState", "Draw Evade State", false);
        menu->Add<SDK::MenuUI::MenuBool>("DrawDodgeResult", "Draw Dodge Result", false);
    }

    inline std::shared_ptr<SDK::MenuUI::Menu> BuildEvadeMenu() {
        auto root = SDK::MenuUI::Menu::Create("Evade", "Evade");
        AddRootOptions(root);
        AddEvadeSettings(root);
        AddDetectionSettings(root);     // Phase 5
        AddEvadeSpellSettings(root);
        AddTrapSettings(root);
        AddSkillshotSettings(root);
        AddImprovementSettings(root);   // Phase 7
        AddDebugSettings(root);
        return root;
    }

} // namespace Plugins::EvadeMenu
