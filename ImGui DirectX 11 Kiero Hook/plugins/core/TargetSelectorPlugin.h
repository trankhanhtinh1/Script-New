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
#include "core/Globals.h"
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
        PluginCategory GetCategory() const override { return PluginCategory::CorePlugin; }

        // ====================================================================
        // Lifecycle
        // ====================================================================
        void OnLoad() override {
            m_menu = SDK::MenuUI::Menu::Create("TargetSelector", "Target Selector");

            // Register this plugin to override SDK TargetSelector
            SDK::TargetSelector::CustomTargetSelector = [this](float range, SDK::DamageType dmgType) {
                return this->GetTarget(range, dmgType);
            };

            // Priority sub-menu (per-enemy hero)
            m_priorityMenu = m_menu->AddSubMenu("Priority", "Priority");
            // Priorities will be populated on first update when enemies are known

            // Weight sub-menu (per-weight item configuration)
            m_weightMenu = m_menu->AddSubMenu("Weights", "Weight Settings");

            // Hero percentage sub-menu (per-hero weight multiplier)
            m_heroPercentMenu = m_menu->AddSubMenu("HeroPercent", "Hero Weight %");

            // Drawing sub-menu
            auto drawMenu = m_menu->AddSubMenu("Drawings", "Drawings");
            drawMenu->Add<SDK::MenuUI::MenuBool>("DrawSelect", "Draw Selected Target", true);
            drawMenu->Add<SDK::MenuUI::MenuBool>("LightSelect", "HighLight Selected Target", true);
            drawMenu->Add<SDK::MenuUI::MenuColor>("SelectColor", "Select Circle Color", 1.0f, 0.0f, 0.0f, 1.0f);
            drawMenu->Add<SDK::MenuUI::MenuBool>("DrawWeightScore", "Draw Weight Scores", false);
            drawMenu->Add<SDK::MenuUI::MenuBool>("DrawBestTarget", "Draw Best Target Circle", true);
            drawMenu->Add<SDK::MenuUI::MenuColor>("BestTargetColor", "Best Target Color", 0.0f, 1.0f, 0.0f, 1.0f);

            // Humanizer sub-menu
            auto humMenu = m_menu->AddSubMenu("Humanizer", "Humanizer");
            humMenu->Add<SDK::MenuUI::MenuSlider>("FowDelay", "FoW Delay (ms)", 0, 0, 1500);

            // Main options
            m_menu->Add<SDK::MenuUI::MenuBool>("ForceSelectTarget", "Force on Select Target", true);
            m_menu->Add<SDK::MenuUI::MenuBool>("OnlySelectTarget", "Only Attack Select Target", false);
            m_menu->Add<SDK::MenuUI::MenuList>("TSMode", "TS Mode",
                std::vector<std::string>{
                    "Smart AD/AP",     // 0
                    "Lowest Health",   // 1
                    "Most Priority",   // 2
                    "Weighted",        // 3
                    "Closest",         // 4
                    "Near Mouse",      // 5
                    "Least Attacks",   // 6
                    "Most AD",         // 7
                    "Most AP"          // 8
                }, 0);

            // Initialize weight items for menu
            SDK::WeightedTargetSelector::Init();
            for (auto& w : SDK::WeightedTargetSelector::Weights) {
                if (!w) continue;
                std::string key = "w_" + w->Name;
                m_weightMenu->Add<SDK::MenuUI::MenuBool>("en_" + w->Name, w->DisplayName + " Enabled", w->Enabled);
                m_weightMenu->Add<SDK::MenuUI::MenuSlider>("wt_" + w->Name, w->DisplayName + " Weight",
                    (int)(w->DefaultWeight * 10.0f), 0, 50);
            }
        }

        void OnUnload() override {
            SDK::TargetSelector::CustomTargetSelector = nullptr;
            SDK::MenuUI::Menu::Remove("TargetSelector");
            m_menu.reset();
            m_priorityMenu.reset();
            m_weightMenu.reset();
            m_heroPercentMenu.reset();
        }

        // ====================================================================
        // Update: handle click-to-select + populate priorities
        // ====================================================================
        void OnUpdate() override {
            if (!m_menu) return;

            // Populate priority menu and hero percentage for enemy heroes (once)
            if (!m_prioritiesPopulated && !SDK::GameObjects::EnemyHeroes.empty()) {
                for (auto& enemy : SDK::GameObjects::EnemyHeroes) {
                    if (!enemy.IsValid()) continue;
                    std::string name = enemy.GetChampionName();
                    if (name.empty()) continue;

                    // Priority slider
                    std::string key = "TS_" + name;
                    if (m_priorityMenu && !m_priorityMenu->Get<SDK::MenuUI::MenuSlider>(key)) {
                        int prio = GetDefaultPriority(name);
                        m_priorityMenu->Add<SDK::MenuUI::MenuSlider>(key, name, prio, 1, 5);
                    }

                    // Hero weight percentage slider (0-200%, default 100%)
                    std::string hpKey = "HP_" + name;
                    if (m_heroPercentMenu && !m_heroPercentMenu->Get<SDK::MenuUI::MenuSlider>(hpKey)) {
                        m_heroPercentMenu->Add<SDK::MenuUI::MenuSlider>(hpKey, name + " %", 100, 0, 200);
                    }
                }
                m_prioritiesPopulated = true;
            }

            // Sync hero percentages to WeightedTargetSelector
            if (m_heroPercentMenu) {
                for (auto& enemy : SDK::GameObjects::EnemyHeroes) {
                    if (!enemy.IsValid()) continue;
                    std::string name = enemy.GetChampionName();
                    std::string hpKey = "HP_" + name;
                    auto* slider = m_heroPercentMenu->Get<SDK::MenuUI::MenuSlider>(hpKey);
                    if (slider)
                        SDK::WeightedTargetSelector::SetHeroPercentage(name, (float)slider->Value / 100.0f);
                }
            }

            // Sync weight settings
            if (m_weightMenu) {
                for (auto& w : SDK::WeightedTargetSelector::Weights) {
                    if (!w) continue;
                    auto* enBool = m_weightMenu->Get<SDK::MenuUI::MenuBool>("en_" + w->Name);
                    if (enBool) w->Enabled = enBool->Enabled;
                    auto* wtSlider = m_weightMenu->Get<SDK::MenuUI::MenuSlider>("wt_" + w->Name);
                    if (wtSlider) w->DefaultWeight = (float)wtSlider->Value / 10.0f;
                }
            }

            // Sync FoW humanizer delay
            if (auto* humMenu = m_menu->GetSubMenu("Humanizer")) {
                auto* fowSlider = humMenu->Get<SDK::MenuUI::MenuSlider>("FowDelay");
                if (fowSlider)
                    SDK::TargetSelectorHumanizer::FowDelay = (float)fowSlider->Value / 1000.0f;
            }

            // Sync TSMode from menu to SDK::TargetSelector::CurrentMode
            // Menu indices: 0=Smart, 1=LowestHP, 2=Priority, 3=Weighted, 4=Closest,
            //               5=NearMouse, 6=LeastAttacks, 7=MostAD, 8=MostAP
            {
                auto* modeList = m_menu->Get<SDK::MenuUI::MenuList>("TSMode");
                if (modeList) {
                    static const SDK::TargetSelector::Mode modeMap[] = {
                        SDK::TargetSelector::Mode::AutoPriority,   // 0: Smart AD/AP
                        SDK::TargetSelector::Mode::LowestHP,       // 1: Lowest Health
                        SDK::TargetSelector::Mode::Priority,       // 2: Most Priority
                        SDK::TargetSelector::Mode::Weighted,       // 3: Weighted
                        SDK::TargetSelector::Mode::Closest,        // 4: Closest
                        SDK::TargetSelector::Mode::NearMouse,      // 5: Near Mouse
                        SDK::TargetSelector::Mode::LeastAttacks,   // 6: Least Attacks
                        SDK::TargetSelector::Mode::MostAD,         // 7: Most AD
                        SDK::TargetSelector::Mode::MostAP,         // 8: Most AP
                    };
                    int idx = modeList->Index;
                    if (idx >= 0 && idx < 9) {
                        SDK::TargetSelector::CurrentMode = modeMap[idx];
                    }
                }
            }

            // Sync "Only Attack Selected Target" flag to SDK::TargetSelector
            auto* only = m_menu->Get<SDK::MenuUI::MenuBool>("OnlySelectTarget");
            SDK::TargetSelector::OnlyAttackSelected = (only && only->Enabled && m_selectedTarget.IsValid());

            // Update TargetSelector FoW tracking
            SDK::TargetSelector::Update();

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

            // Draw selected target circle
            auto* drawSelect = drawMenu->Get<SDK::MenuUI::MenuBool>("DrawSelect");
            if (drawSelect && drawSelect->Enabled && m_selectedTarget.IsValid() && m_selectedTarget.IsAlive()) {
                auto* color = drawMenu->Get<SDK::MenuUI::MenuColor>("SelectColor");
                ImU32 col = color ? color->GetImU32() : IM_COL32(255, 0, 0, 255);
                SDK::Drawing::DrawCircle(m_selectedTarget.GetPosition(),
                    m_selectedTarget.GetBoundingRadius() + 20.0f, col, 3.0f);

                // LightSelect (C# OnRenderMouseOvers analog): animated outer ring highlight.
                auto* light = drawMenu->Get<SDK::MenuUI::MenuBool>("LightSelect");
                if (light && light->Enabled) {
                    float t = SDK::Game::GetTime();
                    float pulse = 8.0f + 6.0f * (0.5f + 0.5f * sinf(t * 6.0f));
                    ImU32 glow = IM_COL32(180, 80, 255, 190);
                    SDK::Drawing::DrawCircle(m_selectedTarget.GetPosition(),
                        m_selectedTarget.GetBoundingRadius() + 24.0f + pulse, glow, 2.0f);
                }
            }

            // Draw best weighted target circle (green circle on the best target)
            auto* drawBest = drawMenu->Get<SDK::MenuUI::MenuBool>("DrawBestTarget");
            if (drawBest && drawBest->Enabled) {
                int bestNetId = SDK::WeightedTargetSelector::BestTargetNetId;
                if (bestNetId > 0) {
                    for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                        if (!hero.IsValid() || !hero.IsAlive()) continue;
                        if (hero.GetNetId() == bestNetId) {
                            auto* bestCol = drawMenu->Get<SDK::MenuUI::MenuColor>("BestTargetColor");
                            ImU32 col = bestCol ? bestCol->GetImU32() : IM_COL32(0, 255, 0, 255);
                            SDK::Drawing::DrawCircle(hero.GetPosition(),
                                hero.GetBoundingRadius() + 30.0f, col, 2.5f);
                            break;
                        }
                    }
                }
            }

            // Draw weight scores above enemy heroes
            auto* drawScores = drawMenu->Get<SDK::MenuUI::MenuBool>("DrawWeightScore");
            if (drawScores && drawScores->Enabled) {
                ImDrawList* dl = ImGui::GetBackgroundDrawList();
                if (!dl) return;

                auto& scores = SDK::WeightedTargetSelector::LastScores;
                for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                    if (!hero.IsValid() || !hero.IsAlive() || !hero.IsVisible()) continue;

                    int netId = hero.GetNetId();
                    auto it = scores.find(netId);
                    if (it == scores.end()) continue;

                    float score = it->second;
                    Vec3 worldPos = hero.GetPosition();
                    worldPos.y += 120.0f; // Above the hero

                    // WorldToScreen
                    Vec2 screenPos;
                    if (!SDK::Drawing::WorldToScreen(worldPos, screenPos)) continue;

                    // Format score text
                    char scoreBuf[64];
                    snprintf(scoreBuf, sizeof(scoreBuf), "%.1f%%", score * 100.0f);

                    // Color based on score: red (high priority) → green (low)
                    int r = (int)(score * 255);
                    int g = (int)((1.0f - score) * 255);
                    ImU32 textCol = IM_COL32(r, g, 50, 255);

                    // Shadow + text
                    ImVec2 textSize = ImGui::CalcTextSize(scoreBuf);
                    float tx = screenPos.x - textSize.x / 2.0f;
                    float ty = screenPos.y - textSize.y / 2.0f;
                    dl->AddText(ImVec2(tx + 1, ty + 1), IM_COL32(0, 0, 0, 200), scoreBuf);
                    dl->AddText(ImVec2(tx, ty), textCol, scoreBuf);
                }
            }
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
        std::shared_ptr<SDK::MenuUI::Menu> m_weightMenu;
        std::shared_ptr<SDK::MenuUI::Menu> m_heroPercentMenu;
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
                float nearestDist = 300.0f; // click tolerance (NewTargetSelector.cs)
                auto& player = SDK::GameObjects::Player;

                for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                    if (!hero.IsAlive() || !hero.IsVisible()) continue;
                    if (player.IsValid() && player.DistanceTo(hero) > 5000.0f) continue;
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
            auto& player = SDK::GameObjects::Player;
            Vec3 mousePos = SDK::Game::GetMouseWorldPos();

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
            case 3: // Weighted
                return SDK::WeightedTargetSelector::GetTarget(targets);
            case 4: // Closest
                return *std::min_element(targets.begin(), targets.end(),
                    [&player](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return player.DistanceTo(a) < player.DistanceTo(b);
                    });
            case 5: // Near Mouse
                return *std::min_element(targets.begin(), targets.end(),
                    [&mousePos](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return a.GetPosition().Distance2D(mousePos) < b.GetPosition().Distance2D(mousePos);
                    });
            case 6: // Least Attacks
                return *std::min_element(targets.begin(), targets.end(),
                    [&player](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return SDK::DamageCalc::GetAutoAttacksToKill(player, a) <
                               SDK::DamageCalc::GetAutoAttacksToKill(player, b);
                    });
            case 7: // Most AD
                return *std::max_element(targets.begin(), targets.end(),
                    [](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return a.GetTotalAD() < b.GetTotalAD();
                    });
            case 8: // Most AP
                return *std::max_element(targets.begin(), targets.end(),
                    [](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return a.GetAP() < b.GetAP();
                    });
            default:
                return *std::min_element(targets.begin(), targets.end(),
                    [this, damageType](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return GetSmartScore(a, damageType) < GetSmartScore(b, damageType);
                    });
            }
        }

        void SortByMode(std::vector<SDK::GameObject>& targets, SDK::DamageType damageType) {
            auto& player = SDK::GameObjects::Player;
            Vec3 mousePos = SDK::Game::GetMouseWorldPos();

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
            case 3: // Weighted
                std::sort(targets.begin(), targets.end(),
                    [](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return SDK::WeightedTargetSelector::CalculateScore(a)
                             < SDK::WeightedTargetSelector::CalculateScore(b);
                    });
                break;
            case 4: // Closest
                std::sort(targets.begin(), targets.end(),
                    [&player](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return player.DistanceTo(a) < player.DistanceTo(b);
                    });
                break;
            case 5: // Near Mouse
                std::sort(targets.begin(), targets.end(),
                    [&mousePos](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return a.GetPosition().Distance2D(mousePos) < b.GetPosition().Distance2D(mousePos);
                    });
                break;
            case 6: // Least Attacks
                std::sort(targets.begin(), targets.end(),
                    [&player](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return SDK::DamageCalc::GetAutoAttacksToKill(player, a) <
                               SDK::DamageCalc::GetAutoAttacksToKill(player, b);
                    });
                break;
            case 7: // Most AD
                std::sort(targets.begin(), targets.end(),
                    [](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return a.GetTotalAD() > b.GetTotalAD();
                    });
                break;
            case 8: // Most AP
                std::sort(targets.begin(), targets.end(),
                    [](const SDK::GameObject& a, const SDK::GameObject& b) {
                        return a.GetAP() > b.GetAP();
                    });
                break;
            default:
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
