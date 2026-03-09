#pragma once
#include "../IPlugin.h"
#include "sdk/UI/MenuUI.h"
#include "sdk/GameObjects/GameObject.h"
#include "sdk/GameObjects/GameObjects.h"
#include "sdk/Game.h"
#include "sdk/UI/Drawing.h"
#include "sdk/Wrappers/Damages/DamageCalc.h"
#include "sdk/GameObjects/BuffManager.h"
#include "sdk/Wrappers/TargetSelector/TargetSelector.h"
#include "sdk/Enums.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
// TargetSelectorPlugin - Aligned with NewTargetSelector.cs
// Modes: Smart AD/AP, Lowest Health, Most Priority
// ============================================================================

namespace Plugins {

    class TargetSelectorPlugin : public IPlugin {
    public:
        const char* GetName() const override { return "Target Selector"; }
        const char* GetAuthor() const override { return "NightSharp"; }
        PluginCategory GetCategory() const override { return PluginCategory::CorePlugin; }

        void OnLoad() override {
            m_menu = SDK::MenuUI::Menu::Create("TargetSelector", "Target Selector");

            // Register as custom selector provider.
            SDK::TargetSelector::CustomTargetSelector = [this](float range, SDK::DamageType dmgType) {
                return this->GetTarget(range, dmgType);
            };

            std::string playerName;
            if (SDK::GameObjects::Player.IsValid()) {
                playerName = SDK::GameObjects::Player.GetChampionName();
            }
            m_modeKey = playerName.empty() ? "TSMode" : ("TSMode_" + playerName);

            // Priority menu.
            m_priorityMenu = m_menu->AddSubMenu("Priority", "Priority");

            // Drawings menu (same as NewTargetSelector.cs).
            auto drawMenu = m_menu->AddSubMenu("Drawings", "Drawings");
            drawMenu->Add<SDK::MenuUI::MenuColor>("SelectColor", "^ Draw Color", 1.0f, 0.0f, 0.0f, 1.0f);
            drawMenu->Add<SDK::MenuUI::MenuBool>("DrawSelect", "Draw Selected Target", true);
            drawMenu->Add<SDK::MenuUI::MenuBool>("LightSelect", "HighLight Selected Target", true);

            // Main options.
            m_menu->Add<SDK::MenuUI::MenuBool>("ForceSelectTarget", "Force on Select Target", true);
            m_menu->Add<SDK::MenuUI::MenuBool>("OnlySelectTarget", "Only Attack Select Target", false);
            m_menu->Add<SDK::MenuUI::MenuList>(
                m_modeKey,
                "TS Mode",
                std::vector<std::string>{ "Smart AD/AP", "Lowest Health", "Most Priority" },
                0);

            EnsurePrioritySliders();
        }

        void OnUnload() override {
            SDK::TargetSelector::CustomTargetSelector = nullptr;
            SDK::TargetSelector::ClearForcedTarget();
            SDK::MenuUI::Menu::Remove("TargetSelector");
            m_menu.reset();
            m_priorityMenu.reset();
            m_selectedTarget = SDK::GameObject();
        }

        void OnUpdate() override {
            if (!m_menu) {
                return;
            }

            EnsurePrioritySliders();

            // Keep SDK target-selector mode synced with this plugin menu.
            if (auto* modeList = m_menu->Get<SDK::MenuUI::MenuList>(m_modeKey)) {
                static const SDK::TargetSelector::Mode modeMap[] = {
                    SDK::TargetSelector::Mode::AutoPriority,
                    SDK::TargetSelector::Mode::LowestHP,
                    SDK::TargetSelector::Mode::Priority
                };
                int idx = std::clamp(modeList->Index, 0, 2);
                SDK::TargetSelector::CurrentMode = modeMap[idx];
            }

            auto* only = m_menu->Get<SDK::MenuUI::MenuBool>("OnlySelectTarget");
            SDK::TargetSelector::OnlyAttackSelected = (only && only->Enabled && m_selectedTarget.IsValid());

            SDK::TargetSelector::Update();
            HandleClickSelect();
        }

        void OnRender() override {
            if (!m_menu) {
                return;
            }

            auto* drawMenu = m_menu->GetSubMenu("Drawings");
            if (!drawMenu) {
                return;
            }

            auto* drawSelect = drawMenu->Get<SDK::MenuUI::MenuBool>("DrawSelect");
            if (drawSelect && drawSelect->Enabled && m_selectedTarget.IsValid() && m_selectedTarget.IsAlive()) {
                auto* color = drawMenu->Get<SDK::MenuUI::MenuColor>("SelectColor");
                ImU32 col = color ? color->GetImU32() : IM_COL32(255, 0, 0, 255);
                SDK::Drawing::DrawCircle(m_selectedTarget.GetPosition(),
                    m_selectedTarget.GetBoundingRadius(), col, 3.0f);

                auto* light = drawMenu->Get<SDK::MenuUI::MenuBool>("LightSelect");
                if (light && light->Enabled) {
                    float t = SDK::Game::GetTime();
                    float pulse = 6.0f + 6.0f * (0.5f + 0.5f * sinf(t * 6.0f));
                    SDK::Drawing::DrawCircle(m_selectedTarget.GetPosition(),
                        m_selectedTarget.GetBoundingRadius() + pulse,
                        IM_COL32(180, 80, 255, 190), 2.0f);
                }
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

        SDK::GameObject GetTarget(float range, SDK::DamageType damageType = SDK::DamageType::Physical) {
            if (!m_menu) return SDK::GameObject();

            auto* only = m_menu->Get<SDK::MenuUI::MenuBool>("OnlySelectTarget");
            if (only && only->Enabled && m_selectedTarget.IsValid()) {
                if (IsValidTarget(m_selectedTarget, range)) {
                    return m_selectedTarget;
                }
                return SDK::GameObject();
            }

            auto targets = GetValidTargets(range);
            if (targets.empty()) {
                return SDK::GameObject();
            }

            if (m_selectedTarget.IsValid() && m_selectedTarget.IsAlive()) {
                auto* force = m_menu->Get<SDK::MenuUI::MenuBool>("ForceSelectTarget");
                if (force && force->Enabled) {
                    for (auto& t : targets) {
                        if (t.address == m_selectedTarget.address) {
                            return m_selectedTarget;
                        }
                    }
                }
            }

            return SelectByMode(targets, damageType);
        }

        SDK::GameObject GetTarget(const std::vector<SDK::GameObject>& possibleTargets,
                                  SDK::DamageType damageType = SDK::DamageType::Physical) {
            std::vector<SDK::GameObject> valid;
            for (auto& t : possibleTargets) {
                if (IsValidTarget(t, 99999.0f)) {
                    valid.push_back(t);
                }
            }
            if (valid.empty()) {
                return SDK::GameObject();
            }

            if (m_selectedTarget.IsValid() && m_selectedTarget.IsAlive()) {
                auto* force = m_menu ? m_menu->Get<SDK::MenuUI::MenuBool>("ForceSelectTarget") : nullptr;
                if (force && force->Enabled) {
                    for (auto& t : valid) {
                        if (t.address == m_selectedTarget.address) {
                            return m_selectedTarget;
                        }
                    }
                }
            }

            return SelectByMode(valid, damageType);
        }

        std::vector<SDK::GameObject> GetTargets(float range, SDK::DamageType damageType = SDK::DamageType::Physical) {
            std::vector<SDK::GameObject> targets;
            if (!m_menu) {
                return targets;
            }

            auto* only = m_menu->Get<SDK::MenuUI::MenuBool>("OnlySelectTarget");
            if (only && only->Enabled && m_selectedTarget.IsValid() && IsValidTarget(m_selectedTarget, range)) {
                return { m_selectedTarget };
            }

            targets = GetValidTargets(range);
            SortByMode(targets, damageType);

            if (m_selectedTarget.IsValid() && m_selectedTarget.IsAlive()) {
                auto* force = m_menu->Get<SDK::MenuUI::MenuBool>("ForceSelectTarget");
                if (force && force->Enabled) {
                    for (auto it = targets.begin(); it != targets.end(); ++it) {
                        if (it->address == m_selectedTarget.address) {
                            SDK::GameObject selected = *it;
                            targets.erase(it);
                            targets.insert(targets.begin(), selected);
                            break;
                        }
                    }
                }
            }

            return targets;
        }

        SDK::GameObject GetSelectedTarget() const { return m_selectedTarget; }

        void ClearSelectedTarget() {
            m_selectedTarget = SDK::GameObject();
            SDK::TargetSelector::ClearForcedTarget();
        }

        int GetPriority(const SDK::GameObject& target) {
            if (!target.IsValid()) {
                return 0;
            }
            std::string name = target.GetChampionName();
            std::string key = "TS_" + name;
            if (m_priorityMenu) {
                if (auto* slider = m_priorityMenu->Get<SDK::MenuUI::MenuSlider>(key)) {
                    return slider->Value;
                }
            }
            return GetDefaultPriority(name);
        }

    private:
        std::shared_ptr<SDK::MenuUI::Menu> m_menu;
        std::shared_ptr<SDK::MenuUI::Menu> m_priorityMenu;
        SDK::GameObject m_selectedTarget;
        std::string m_modeKey = "TSMode";

        void EnsurePrioritySliders() {
            if (!m_priorityMenu) {
                return;
            }

            for (auto& enemy : SDK::GameObjects::EnemyHeroes) {
                if (!enemy.IsValid()) {
                    continue;
                }

                const std::string name = enemy.GetChampionName();
                if (name.empty()) {
                    continue;
                }

                const std::string key = "TS_" + name;
                if (!m_priorityMenu->Get<SDK::MenuUI::MenuSlider>(key)) {
                    m_priorityMenu->Add<SDK::MenuUI::MenuSlider>(
                        key,
                        name,
                        GetDefaultPriority(name),
                        1,
                        5);
                }
            }
        }

        void HandleClickSelect() {
            static bool wasClickDown = false;
            bool clickDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            if (clickDown && !wasClickDown) {
                Vec3 clickPos = SDK::Game::GetMouseWorldPos();
                if (clickPos.IsZero()) {
                    wasClickDown = clickDown;
                    return;
                }

                SDK::GameObject nearest;
                float nearestDist = 300.0f;

                for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                    if (!hero.IsValid() || !hero.IsAlive() || !hero.IsVisible()) {
                        continue;
                    }

                    float dist = hero.GetPosition().Distance2D(clickPos);
                    if (dist < nearestDist) {
                        nearestDist = dist;
                        nearest = hero;
                    }
                }

                if (nearest.IsValid()) {
                    m_selectedTarget = nearest;
                    SDK::TargetSelector::SetForcedTarget(nearest);
                } else {
                    m_selectedTarget = SDK::GameObject();
                    SDK::TargetSelector::ClearForcedTarget();
                }
            }

            wasClickDown = clickDown;

            if (m_selectedTarget.IsValid()) {
                if (!m_selectedTarget.IsAlive() || !m_selectedTarget.IsVisible()) {
                    m_selectedTarget = SDK::GameObject();
                    SDK::TargetSelector::ClearForcedTarget();
                }
            }
        }

        bool IsValidTarget(const SDK::GameObject& target, float range) {
            if (!target.IsValid() || !target.IsAlive() || !target.IsVisible() || !target.IsTargetable()) {
                return false;
            }

            if (target.IsZombie()) {
                return false;
            }

            SDK::BuffManager buffs(target.address);
            static const char* invulnBuffs[] = {
                "KayleR", "TryndamereR", "kindaborroweytime",
                "ChronoShift", "UndyingRage", nullptr
            };
            for (const char** p = invulnBuffs; *p; ++p) {
                if (buffs.HasBuff(*p)) {
                    return false;
                }
            }

            if (range > 0.0f) {
                float dist = SDK::GameObjects::Player.DistanceTo(target);
                if (dist > range + target.GetBoundingRadius()) {
                    return false;
                }
            }

            return true;
        }

        std::vector<SDK::GameObject> GetValidTargets(float range) {
            std::vector<SDK::GameObject> result;

            bool isAzir = false;
            auto& player = SDK::GameObjects::Player;
            if (player.IsValid()) {
                std::string champName = player.GetChampionName();
                isAzir = (_stricmp(champName.c_str(), "Azir") == 0);
            }

            for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive() || !hero.IsVisible() || !hero.IsTargetable()) {
                    continue;
                }
                if (hero.IsZombie()) {
                    continue;
                }

                bool inRange = player.DistanceTo(hero) <= range + hero.GetBoundingRadius();

                // Azir soldier range compatibility.
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

                if (inRange) {
                    result.push_back(hero);
                }
            }

            return result;
        }

        float GetSmartScore(const SDK::GameObject& target, SDK::DamageType damageType) {
            auto& player = SDK::GameObjects::Player;

            float ehp = (damageType == SDK::DamageType::Magical)
                ? target.GetEffectiveHealthAP()
                : target.GetEffectiveHealthAD();

            if (ehp <= 0.0f) {
                ehp = 1.0f;
            }

            int prio = GetPriority(target);
            float prioWeight = 0.5f + (float)prio * 0.5f;
            float distPenalty = player.DistanceTo(target) / 1000.0f * 30.0f;

            float aaDmg = SDK::DamageCalc::GetAutoAttackDamage(player, target, false, true);
            float killBonus = 0.0f;
            if (aaDmg > 0.0f) {
                float attacksToKill = target.GetHealth() / aaDmg;
                if (attacksToKill <= 3.0f) {
                    killBonus = -200.0f * (3.0f - attacksToKill);
                }
            }

            return (ehp / prioWeight) + distPenalty + killBonus;
        }

        SDK::GameObject SelectByMode(std::vector<SDK::GameObject>& targets, SDK::DamageType damageType) {
            if (targets.empty()) {
                return SDK::GameObject();
            }

            int mode = 0;
            if (m_menu) {
                if (auto* modeList = m_menu->Get<SDK::MenuUI::MenuList>(m_modeKey)) {
                    mode = modeList->Index;
                }
            }
            mode = std::clamp(mode, 0, 2);

            switch (mode) {
            case 0: // Smart AD/AP
                return *std::min_element(targets.begin(), targets.end(),
                    [this, damageType](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return GetSmartScore(a, damageType) < GetSmartScore(b, damageType);
                    });
            case 1: // Lowest health
                return *std::min_element(targets.begin(), targets.end(),
                    [](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return a.GetHealth() < b.GetHealth();
                    });
            case 2: // Most priority
                return *std::min_element(targets.begin(), targets.end(),
                    [this](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return GetPriority(a) > GetPriority(b);
                    });
            default:
                return targets.front();
            }
        }

        void SortByMode(std::vector<SDK::GameObject>& targets, SDK::DamageType damageType) {
            int mode = 0;
            if (m_menu) {
                if (auto* modeList = m_menu->Get<SDK::MenuUI::MenuList>(m_modeKey)) {
                    mode = modeList->Index;
                }
            }
            mode = std::clamp(mode, 0, 2);

            switch (mode) {
            case 0:
                std::sort(targets.begin(), targets.end(),
                    [this, damageType](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return GetSmartScore(a, damageType) < GetSmartScore(b, damageType);
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
            default:
                break;
            }
        }

        static int GetDefaultPriority(const std::string& name) {
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

            static const char* highPrio[] = {
                "Akali","Diana","Ekko","FiddleSticks","Fiora","Fizz","Heimerdinger",
                "Jayce","Kassadin","Kayle","KhaZix","Lissandra","Mordekaiser","Nidalee",
                "Riven","Senna","Shaco","Vladimir","Yasuo","Zilean","Camille","Kayn",
                "Yone","Viego","Gwen","Akshan","Belveth", nullptr
            };

            static const char* medPrio[] = {
                "Aatrox","Darius","Elise","Evelynn","Galio","Gangplank","Gragas",
                "Irelia","Jax","LeeSin","Maokai","Morgana","Nocturne","Pantheon",
                "Poppy","Pyke","Rengar","Rumble","Ryze","Sett","Swain","Trundle",
                "Tryndamere","Udyr","Urgot","Vi","XinZhao","RekSai","Illaoi","Kled",
                "Lillia","Vex","Renata", nullptr
            };

            static const char* lowPrio[] = {
                "Alistar","Amumu","Bard","Blitzcrank","Braum","Chogath","DrMundo",
                "Garen","Gnar","Hecarim","Janna","JarvanIV","Leona","Lulu","Malphite",
                "Nami","Nasus","Nautilus","Nunu","Olaf","Rammus","Renekton","Sejuani",
                "Shen","Shyvana","Singed","Sion","Skarner","Sona","Taric","TahmKench",
                "Thresh","Volibear","Warwick","MonkeyKing","Yorick","Yuumi","Zac","Zyra",
                "Ornn","Rakan","Ivern","Rell","KSante","Milio", nullptr
            };

            for (const char** p = maxPrio; *p; ++p)
                if (_stricmp(name.c_str(), *p) == 0) return 5;
            for (const char** p = highPrio; *p; ++p)
                if (_stricmp(name.c_str(), *p) == 0) return 4;
            for (const char** p = medPrio; *p; ++p)
                if (_stricmp(name.c_str(), *p) == 0) return 3;
            for (const char** p = lowPrio; *p; ++p)
                if (_stricmp(name.c_str(), *p) == 0) return 2;

            return 1;
        }
    };

} // namespace Plugins
