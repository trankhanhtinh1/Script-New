#pragma once
#include "../IPlugin.h"
#include "../../sdk/MenuUI.h"
#include "../../sdk/GameObject.h"
#include "../../sdk/GameObjects.h"
#include "../../sdk/Game.h"
#include "../../sdk/Drawing.h"
#include "../../sdk/Enums.h"
#include "../../core/Globals.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// ============================================================================
// TargetSelectorPlugin — Ported from ImpulseAIO NewTargetSelector.cs
// Core plugin: intelligent target selection with priority system
// ============================================================================

namespace Plugins {

    class TargetSelectorPlugin : public IPlugin {
    public:
        const char* GetName() const override { return "Target Selector"; }
        const char* GetAuthor() const override { return "NightSharp"; }
        PluginCategory GetCategory() const override { return PluginCategory::Utility; }

        // ====================================================================
        // Lifecycle
        // ====================================================================
        void OnLoad() override {
            m_menu = SDK::MenuUI::Menu::Create("TargetSelector", "Target Selector");

            // Priority sub-menu (per-enemy hero)
            m_priorityMenu = m_menu->AddSubMenu("Priority", "Priority");
            // Priorities will be populated on first update when enemies are known

            // Drawing sub-menu
            auto drawMenu = m_menu->AddSubMenu("Drawings", "Drawings");
            drawMenu->Add<SDK::MenuUI::MenuBool>("DrawSelect", "Draw Selected Target", true);
            drawMenu->Add<SDK::MenuUI::MenuColor>("SelectColor", "Select Circle Color", 1.0f, 0.0f, 0.0f, 1.0f);

            // Main options
            m_menu->Add<SDK::MenuUI::MenuBool>("ForceSelectTarget", "Force on Select Target", true);
            m_menu->Add<SDK::MenuUI::MenuBool>("OnlySelectTarget", "Only Attack Select Target", false);
            m_menu->Add<SDK::MenuUI::MenuList>("TSMode", "TS Mode",
                std::vector<std::string>{"Smart AD/AP", "Lowest Health", "Most Priority"}, 0);
        }

        void OnUnload() override {
            SDK::MenuUI::Menu::Remove("TargetSelector");
            m_menu.reset();
            m_priorityMenu.reset();
        }

        // ====================================================================
        // Update: handle click-to-select + populate priorities
        // ====================================================================
        void OnUpdate() override {
            if (!m_menu) return;

            // Populate priority menu for enemy heroes (once)
            if (!m_prioritiesPopulated && !SDK::GameObjects::EnemyHeroes.empty()) {
                for (auto& enemy : SDK::GameObjects::EnemyHeroes) {
                    if (!enemy.IsValid()) continue;
                    std::string name = enemy.GetChampionName();
                    if (name.empty()) continue;

                    std::string key = "TS_" + name;
                    if (m_priorityMenu && !m_priorityMenu->Get<SDK::MenuUI::MenuSlider>(key)) {
                        int prio = GetDefaultPriority(name);
                        m_priorityMenu->Add<SDK::MenuUI::MenuSlider>(key, name, prio, 1, 5);
                    }
                }
                m_prioritiesPopulated = true;
            }

            // Handle click-to-select (left click near enemy)
            HandleClickSelect();
        }

        // ====================================================================
        // Drawing
        // ====================================================================
        void OnRender() override {
            if (!m_menu) return;

            auto* drawMenu = m_menu->GetSubMenu("Drawings");
            if (!drawMenu) return;

            auto* drawSelect = drawMenu->Get<SDK::MenuUI::MenuBool>("DrawSelect");
            if (!drawSelect || !drawSelect->Enabled) return;

            if (!m_selectedTarget.IsValid() || !m_selectedTarget.IsAlive()) return;

            auto* color = drawMenu->Get<SDK::MenuUI::MenuColor>("SelectColor");
            ImU32 col = color ? color->GetImU32() : IM_COL32(255, 0, 0, 255);

            SDK::Drawing::DrawCircle(m_selectedTarget.GetPosition(),
                m_selectedTarget.GetBoundingRadius() + 20.0f, col, 3.0f);
        }

        // ====================================================================
        // Menu
        // ====================================================================
        void OnMenu() override {
            if (m_menu) m_menu->Draw();
        }

        // ====================================================================
        // Public API — for other plugins and scripts
        // ====================================================================

        // Get single best target in range
        SDK::GameObject GetTarget(float range, SDK::DamageType damageType = SDK::DamageType::Physical) {
            auto targets = GetValidTargets(range);
            if (targets.empty()) return SDK::GameObject();

            // Check forced selected target
            if (m_selectedTarget.IsValid() && m_selectedTarget.IsAlive()) {
                auto* force = m_menu ? m_menu->Get<SDK::MenuUI::MenuBool>("ForceSelectTarget") : nullptr;
                if (force && force->Enabled) {
                    for (auto& t : targets) {
                        if (t.address == m_selectedTarget.address)
                            return m_selectedTarget;
                    }
                }
                auto* only = m_menu ? m_menu->Get<SDK::MenuUI::MenuBool>("OnlySelectTarget") : nullptr;
                if (only && only->Enabled && IsValidTarget(m_selectedTarget, 99999.0f))
                    return m_selectedTarget;
            }

            return SelectByMode(targets, damageType);
        }

        // Get single best target from a pre-filtered list
        SDK::GameObject GetTarget(const std::vector<SDK::GameObject>& possibleTargets,
                                   SDK::DamageType damageType = SDK::DamageType::Physical) {
            std::vector<SDK::GameObject> valid;
            for (auto& t : possibleTargets) {
                if (IsValidTarget(t, 99999.0f)) valid.push_back(t);
            }
            if (valid.empty()) return SDK::GameObject();

            // Force select
            if (m_selectedTarget.IsValid() && m_selectedTarget.IsAlive()) {
                auto* force = m_menu ? m_menu->Get<SDK::MenuUI::MenuBool>("ForceSelectTarget") : nullptr;
                if (force && force->Enabled) {
                    for (auto& t : valid) {
                        if (t.address == m_selectedTarget.address)
                            return m_selectedTarget;
                    }
                }
            }

            return SelectByMode(valid, damageType);
        }

        // Get multiple targets sorted by priority
        std::vector<SDK::GameObject> GetTargets(float range, SDK::DamageType damageType = SDK::DamageType::Physical) {
            auto targets = GetValidTargets(range);
            SortByMode(targets, damageType);

            // Move selected target to front if forced
            if (m_selectedTarget.IsValid()) {
                auto* force = m_menu ? m_menu->Get<SDK::MenuUI::MenuBool>("ForceSelectTarget") : nullptr;
                if (force && force->Enabled) {
                    for (auto it = targets.begin(); it != targets.end(); ++it) {
                        if (it->address == m_selectedTarget.address) {
                            SDK::GameObject sel = *it;
                            targets.erase(it);
                            targets.insert(targets.begin(), sel);
                            break;
                        }
                    }
                }
            }
            return targets;
        }

        SDK::GameObject GetSelectedTarget() const { return m_selectedTarget; }
        void ClearSelectedTarget() { m_selectedTarget = SDK::GameObject(); }

        int GetPriority(const SDK::GameObject& target) {
            if (!target.IsValid()) return 0;
            std::string name = target.GetChampionName();
            std::string key = "TS_" + name;
            if (m_priorityMenu) {
                auto* slider = m_priorityMenu->Get<SDK::MenuUI::MenuSlider>(key);
                if (slider) return slider->Value;
            }
            return GetDefaultPriority(name);
        }

    private:
        std::shared_ptr<SDK::MenuUI::Menu> m_menu;
        std::shared_ptr<SDK::MenuUI::Menu> m_priorityMenu;
        SDK::GameObject m_selectedTarget;
        bool m_prioritiesPopulated = false;

        // ====================================================================
        // Click-to-select handling
        // ====================================================================
        void HandleClickSelect() {
            // Check left mouse click
            static bool wasClickDown = false;
            bool clickDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            if (clickDown && !wasClickDown) {
                Vec3 cursorPos = SDK::Game::GetMouseWorldPos();
                if (cursorPos.IsZero()) { wasClickDown = clickDown; return; }

                SDK::GameObject nearest;
                float nearestDist = 300.0f; // click tolerance

                for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                    if (!hero.IsAlive() || !hero.IsVisible()) continue;
                    float dist = hero.GetPosition().Distance2D(cursorPos);
                    if (dist < nearestDist) {
                        nearestDist = dist;
                        nearest = hero;
                    }
                }

                if (nearest.IsValid())
                    m_selectedTarget = nearest;
                else
                    m_selectedTarget = SDK::GameObject(); // deselect
            }
            wasClickDown = clickDown;
        }

        // ====================================================================
        // Valid target checks
        // ====================================================================
        bool IsValidTarget(const SDK::GameObject& target, float range) {
            if (!target.IsValid()) return false;
            if (!target.IsAlive()) return false;
            if (!target.IsVisible()) return false;
            if (!target.IsTargetable()) return false;
            if (range > 0.0f) {
                float dist = SDK::GameObjects::Player.DistanceTo(target);
                if (dist > range + target.GetBoundingRadius()) return false;
            }
            return true;
        }

        std::vector<SDK::GameObject> GetValidTargets(float range) {
            std::vector<SDK::GameObject> result;
            for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                if (IsValidTarget(hero, range))
                    result.push_back(hero);
            }
            return result;
        }

        // ====================================================================
        // Mode-based selection (ported from NewTargetSelector)
        // ====================================================================
        SDK::GameObject SelectByMode(std::vector<SDK::GameObject>& targets, SDK::DamageType damageType) {
            int mode = 0;
            if (m_menu) {
                auto* modeList = m_menu->Get<SDK::MenuUI::MenuList>("TSMode");
                if (modeList) mode = modeList->Index;
            }

            switch (mode) {
            case 0: // Smart AD/AP — lowest effective health based on damage type
                return *std::min_element(targets.begin(), targets.end(),
                    [damageType](const SDK::GameObject& a, const SDK::GameObject& b) {
                        float ehA = (damageType == SDK::DamageType::Magical) ? a.GetEffectiveHealthAP() : a.GetEffectiveHealthAD();
                        float ehB = (damageType == SDK::DamageType::Magical) ? b.GetEffectiveHealthAP() : b.GetEffectiveHealthAD();
                        return ehA < ehB;
                    });
            case 1: // Lowest Health
                return *std::min_element(targets.begin(), targets.end(),
                    [](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return a.GetHealth() < b.GetHealth();
                    });
            case 2: // Most Priority
                return *std::min_element(targets.begin(), targets.end(),
                    [this](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return GetPriority(a) > GetPriority(b);
                    });
            default:
                return targets.empty() ? SDK::GameObject() : targets[0];
            }
        }

        void SortByMode(std::vector<SDK::GameObject>& targets, SDK::DamageType damageType) {
            int mode = 0;
            if (m_menu) {
                auto* modeList = m_menu->Get<SDK::MenuUI::MenuList>("TSMode");
                if (modeList) mode = modeList->Index;
            }

            switch (mode) {
            case 0:
                std::sort(targets.begin(), targets.end(),
                    [damageType](const SDK::GameObject& a, const SDK::GameObject& b) {
                        float ehA = (damageType == SDK::DamageType::Magical) ? a.GetEffectiveHealthAP() : a.GetEffectiveHealthAD();
                        float ehB = (damageType == SDK::DamageType::Magical) ? b.GetEffectiveHealthAP() : b.GetEffectiveHealthAD();
                        return ehA < ehB;
                    });
                break;
            case 1:
                std::sort(targets.begin(), targets.end(),
                    [](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return a.GetHealth() < b.GetHealth();
                    });
                break;
            case 2:
                std::sort(targets.begin(), targets.end(),
                    [this](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return GetPriority(a) > GetPriority(b);
                    });
                break;
            }
        }

        // ====================================================================
        // Default champion priorities (ported from NewTargetSelector.cs)
        // ====================================================================
        static int GetDefaultPriority(const std::string& name) {
            // Max Priority (5) — ADC / Mage / Assassin
            static const char* maxPrio[] = {
                "Ahri","Aphelios","Anivia","Annie","Ashe","Azir","Brand","Caitlyn",
                "Cassiopeia","Corki","Draven","Ezreal","Graves","Jinx","Kalista",
                "Kaisa","Karma","Karthus","Katarina","Kennen","KogMaw","Kindred",
                "Leblanc","Lucian","Lux","Malzahar","MasterYi","MissFortune","Neeko",
                "Orianna","Quinn","Sivir","Sylas","Syndra","Talon","Teemo","Tristana",
                "TwistedFate","Twitch","Varus","Vayne","Veigar","Velkoz","Viktor",
                "Xerath","Zed","Ziggs","Jhin","Soraka","AurelionSol","Taliyah",
                "Qiyana","Zoe","Xayah","Samira","Zeri","Nilah","Smolder", nullptr
            };
            // High Priority (4)
            static const char* highPrio[] = {
                "Akali","Diana","Ekko","FiddleSticks","Fiora","Fizz","Heimerdinger",
                "Jayce","Kassadin","Kayle","KhaZix","Lissandra","Mordekaiser","Nidalee",
                "Riven","Senna","Shaco","Vladimir","Yasuo","Zilean","Camille","Kayn",
                "Yone","Viego","Gwen","Akshan","Belveth", nullptr
            };
            // Medium Priority (3)
            static const char* medPrio[] = {
                "Aatrox","Darius","Elise","Evelynn","Galio","Gangplank","Gragas",
                "Irelia","Jax","LeeSin","Maokai","Morgana","Nocturne","Pantheon",
                "Poppy","Pyke","Rengar","Rumble","Ryze","Sett","Swain","Trundle",
                "Tryndamere","Udyr","Urgot","Vi","XinZhao","RekSai","Illaoi","Kled",
                "Lillia","Vex","Renata", nullptr
            };
            // Low Priority (2)
            static const char* lowPrio[] = {
                "Alistar","Amumu","Bard","Blitzcrank","Braum","Chogath","DrMundo",
                "Garen","Gnar","Hecarim","Janna","JarvanIV","Leona","Lulu","Malphite",
                "Nami","Nasus","Nautilus","Nunu","Olaf","Rammus","Renekton","Sejuani",
                "Shen","Shyvana","Singed","Sion","Skarner","Sona","Taric","TahmKench",
                "Thresh","Volibear","Warwick","MonkeyKing","Yorick","Yuumi","Zac","Zyra",
                "Ornn","Rakan","Ivern","Rell","KSante","Milio", nullptr
            };

            for (const char** p = maxPrio; *p; p++)
                if (_stricmp(name.c_str(), *p) == 0) return 5;
            for (const char** p = highPrio; *p; p++)
                if (_stricmp(name.c_str(), *p) == 0) return 4;
            for (const char** p = medPrio; *p; p++)
                if (_stricmp(name.c_str(), *p) == 0) return 3;
            for (const char** p = lowPrio; *p; p++)
                if (_stricmp(name.c_str(), *p) == 0) return 2;
            return 1; // Unknown = lowest
        }
    };

} // namespace Plugins
