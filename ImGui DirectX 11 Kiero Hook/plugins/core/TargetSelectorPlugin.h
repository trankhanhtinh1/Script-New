#pragma once
#include "../IPlugin.h"
#include "../../sdk/MenuUI.h"
#include "../../sdk/GameObject.h"
#include "../../sdk/GameObjects.h"
#include "../../sdk/Game.h"
#include "../../sdk/Drawing.h"
#include "../../sdk/DamageCalc.h"
#include "../../sdk/BuffManager.h"
#include "../../sdk/TargetSelector.h"
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

            // Sync "Only Attack Selected Target" flag to SDK::TargetSelector
            auto* only = m_menu->Get<SDK::MenuUI::MenuBool>("OnlySelectTarget");
            SDK::TargetSelector::OnlyAttackSelected = (only && only->Enabled && m_selectedTarget.IsValid());

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
            // "Only Attack Selected Target" — strict mode: ONLY attack selected target
            auto* only = m_menu ? m_menu->Get<SDK::MenuUI::MenuBool>("OnlySelectTarget") : nullptr;
            if (only && only->Enabled && m_selectedTarget.IsValid()) {
                if (m_selectedTarget.IsAlive() && m_selectedTarget.IsVisible() && m_selectedTarget.IsTargetable()) {
                    float dist = SDK::GameObjects::Player.DistanceTo(m_selectedTarget);
                    if (dist <= range + m_selectedTarget.GetBoundingRadius())
                        return m_selectedTarget;
                }
                return SDK::GameObject(); // Selected target out of range → attack nothing
            }

            auto targets = GetValidTargets(range);
            if (targets.empty()) return SDK::GameObject();

            // "Force on Selected Target" — prefer selected but fallback to best
            if (m_selectedTarget.IsValid() && m_selectedTarget.IsAlive()) {
                auto* force = m_menu ? m_menu->Get<SDK::MenuUI::MenuBool>("ForceSelectTarget") : nullptr;
                if (force && force->Enabled) {
                    for (auto& t : targets) {
                        if (t.address == m_selectedTarget.address)
                            return m_selectedTarget;
                    }
                }
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
        // 2.4: OnlySelectTarget enforcement — if enabled, only return selected target
        std::vector<SDK::GameObject> GetTargets(float range, SDK::DamageType damageType = SDK::DamageType::Physical) {
            // 2.4: Only Attack Selected Target
            auto* only = m_menu ? m_menu->Get<SDK::MenuUI::MenuBool>("OnlySelectTarget") : nullptr;
            if (only && only->Enabled && m_selectedTarget.IsValid() && m_selectedTarget.IsAlive()) {
                if (IsValidTarget(m_selectedTarget, range)) {
                    return { m_selectedTarget };
                }
                return {}; // Selected target out of range / invalid → return nothing
            }

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

                if (nearest.IsValid()) {
                    m_selectedTarget = nearest;
                    SDK::TargetSelector::SetForcedTarget(nearest); // Sync with SDK
                } else {
                    m_selectedTarget = SDK::GameObject(); // deselect
                    SDK::TargetSelector::ClearForcedTarget();
                }
            }
            wasClickDown = clickDown;

            // Keep ForcedTarget in sync — clear if selected target died or went invisible
            if (m_selectedTarget.IsValid()) {
                if (!m_selectedTarget.IsAlive() || !m_selectedTarget.IsVisible()) {
                    m_selectedTarget = SDK::GameObject();
                    SDK::TargetSelector::ClearForcedTarget();
                }
            }
        }

        // ====================================================================
        // Valid target checks (enhanced with invulnerable/zombie checks)
        // ====================================================================
        bool IsValidTarget(const SDK::GameObject& target, float range) {
            if (!target.IsValid()) return false;
            if (!target.IsAlive()) return false;
            if (!target.IsVisible()) return false;
            if (!target.IsTargetable()) return false;

            // Zombie check (Sion/Karthus/Kog'Maw passive)
            if (target.IsZombie()) return false;

            // Invulnerable check (Kayle R, Tryndamere R, etc.)
            SDK::BuffManager buffs(target.address);
            static const char* invulnBuffs[] = {
                "KayleR", "TryndamereR", "kindaborroweytime",
                "ChronoShift", "UndyingRage", nullptr
            };
            for (const char** p = invulnBuffs; *p; p++)
                if (buffs.HasBuff(*p)) return false;

            if (range > 0.0f) {
                float dist = SDK::GameObjects::Player.DistanceTo(target);
                if (dist > range + target.GetBoundingRadius()) return false;
            }
            return true;
        }

        std::vector<SDK::GameObject> GetValidTargets(float range) {
            std::vector<SDK::GameObject> result;

            // 2.5 Azir: also check if Azir soldiers are in range of target
            bool isAzir = false;
            auto& player = SDK::GameObjects::Player;
            if (player.IsValid()) {
                std::string champName = player.GetChampionName();
                isAzir = (_stricmp(champName.c_str(), "Azir") == 0);
            }

            for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive() || !hero.IsVisible() || !hero.IsTargetable())
                    continue;
                if (hero.IsZombie()) continue;

                float dist = player.DistanceTo(hero);

                // Standard range check
                bool inRange = (dist <= range + hero.GetBoundingRadius());

                // 2.5 Azir soldier extended range:
                // If Azir + soldier within 770 of player + soldier within 350 of target → in range
                if (!inRange && isAzir) {
                    for (auto& soldier : SDK::GameObjects::AzirSoldiers) {
                        if (!soldier.IsValid()) continue;
                        float soldierToPlayer = player.DistanceTo(soldier);
                        float soldierToTarget = soldier.GetPosition().Distance2D(hero.GetPosition());
                        if (soldierToPlayer <= 770.0f && soldierToTarget <= 350.0f) {
                            inRange = true;
                            break;
                        }
                    }
                }

                if (inRange)
                    result.push_back(hero);
            }
            return result;
        }

        // ====================================================================
        // 2.3 Smart AD/AP Scoring (ported from NewTargetSelector.cs)
        // Score = (EHP / priority_weight) — lower is better
        //
        // This combines:
        //   - Effective Health (physical or magical based on source)
        //   - Champion priority (higher prio = lower divisor)
        //   - Distance penalty
        //   - Kill threat bonus (if we can kill them soon)
        // ====================================================================
        float GetSmartScore(const SDK::GameObject& target, SDK::DamageType damageType) {
            auto& player = SDK::GameObjects::Player;
            float hp = target.GetHealth();

            // Effective HP based on damage type
            float ehp;
            if (damageType == SDK::DamageType::Magical)
                ehp = target.GetEffectiveHealthAP();
            else
                ehp = target.GetEffectiveHealthAD();

            if (ehp <= 0) ehp = 1.0f;

            // Priority weight (higher priority = lower EHP score → better target)
            int prio = GetPriority(target);
            float prioWeight = 0.5f + (float)prio * 0.5f; // 1.0 at prio=1, 3.0 at prio=5

            // Distance penalty
            float dist = player.DistanceTo(target);
            float distPenalty = dist / 1000.0f * 30.0f;

            // Kill threat bonus: if we can kill them in 2-3 AAs, strongly prefer
            float aaDmg = SDK::DamageCalc::GetAutoAttackDamage(player, target, false, true);
            float killBonus = 0.0f;
            if (aaDmg > 0 && hp / aaDmg <= 3.0f)
                killBonus = -200.0f * (3.0f - hp / aaDmg); // Negative = lower score = better

            return (ehp / prioWeight) + distPenalty + killBonus;
        }

        // ====================================================================
        // Mode-based selection (ported from NewTargetSelector)
        // Mode 0: Smart AD/AP (2.3) — DamageCalc-based with priority weighting
        // Mode 1: Lowest Health
        // Mode 2: Most Priority
        // ====================================================================
        SDK::GameObject SelectByMode(std::vector<SDK::GameObject>& targets, SDK::DamageType damageType) {
            if (targets.empty()) return SDK::GameObject();

            int mode = 0;
            if (m_menu) {
                auto* modeList = m_menu->Get<SDK::MenuUI::MenuList>("TSMode");
                if (modeList) mode = modeList->Index;
            }

            switch (mode) {
            case 0: // Smart AD/AP — 2.3 scoring
                return *std::min_element(targets.begin(), targets.end(),
                    [this, damageType](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return GetSmartScore(a, damageType) < GetSmartScore(b, damageType);
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
                return targets[0];
            }
        }

        void SortByMode(std::vector<SDK::GameObject>& targets, SDK::DamageType damageType) {
            int mode = 0;
            if (m_menu) {
                auto* modeList = m_menu->Get<SDK::MenuUI::MenuList>("TSMode");
                if (modeList) mode = modeList->Index;
            }

            switch (mode) {
            case 0: // Smart AD/AP
                std::sort(targets.begin(), targets.end(),
                    [this, damageType](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return GetSmartScore(a, damageType) < GetSmartScore(b, damageType);
                    });
                break;
            case 1: // Lowest Health
                std::sort(targets.begin(), targets.end(),
                    [](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return a.GetHealth() < b.GetHealth();
                    });
                break;
            case 2: // Most Priority
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
