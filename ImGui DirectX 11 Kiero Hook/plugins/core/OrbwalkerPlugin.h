#pragma once
#include "../IPlugin.h"
#include "../../sdk/MenuUI.h"
#include "../../sdk/GameObject.h"
#include "../../sdk/GameObjects.h"
#include "../../sdk/Game.h"
#include "../../sdk/SpellBook.h"
#include "../../sdk/BuffManager.h"
#include "../../sdk/Drawing.h"
#include "../../sdk/Orbwalker.h"
#include "../../sdk/Enums.h"
#include "../../core/Globals.h"
#include "../../core/Offsets.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// ============================================================================
// OrbwalkerPlugin — Ported from ImpulseAIO NewOrbwalker.cs (EnsoulSharp SDK)
// Core plugin: handles attack + move cycle with proper timing
// ============================================================================

namespace Plugins {

    class OrbwalkerPlugin : public IPlugin {
    public:
        const char* GetName() const override { return "Orbwalker"; }
        const char* GetAuthor() const override { return "NightSharp"; }
        PluginCategory GetCategory() const override { return PluginCategory::Orbwalker; }

        // ====================================================================
        // Lifecycle
        // ====================================================================
        void OnLoad() override {
            // Create E#-style menu
            m_menu = SDK::MenuUI::Menu::Create("Orbwalker", "Orbwalker");

            // Attackable sub-menu
            auto attackable = m_menu->AddSubMenu("Attackable", "Attackable Unit");
            attackable->Add<SDK::MenuUI::MenuBool>("Barrels", "Barrels", true);
            attackable->Add<SDK::MenuUI::MenuBool>("JunglePlant", "Jungle Plant", false);
            attackable->Add<SDK::MenuUI::MenuBool>("SpecialMinions", "Pets", true);
            attackable->Add<SDK::MenuUI::MenuBool>("Wards", "Wards", true);

            // Prioritize sub-menu
            auto prioritize = m_menu->AddSubMenu("Prioritize", "Prioritize");
            prioritize->Add<SDK::MenuUI::MenuBool>("FarmOverHarass", "Farm Over Harass", true);
            prioritize->Add<SDK::MenuUI::MenuBool>("SpecialMinion", "Special Minion", false);
            prioritize->Add<SDK::MenuUI::MenuBool>("SmallJungle", "Small Jungle", false);
            prioritize->Add<SDK::MenuUI::MenuBool>("Turret", "Turret", true);

            // Orbwalker settings sub-menu
            auto orbSettings = m_menu->AddSubMenu("Settings", "Orbwalker Settings");
            orbSettings->Add<SDK::MenuUI::MenuSlider>("ExtraHold", "Extra Hold Position", 50, 0, 250);
            orbSettings->Add<SDK::MenuUI::MenuBool>("MoveRandom", "Randomize Movement", false);
            orbSettings->Add<SDK::MenuUI::MenuSlider>("WindupDelay", "Extra Windup Delay", 60, 0, 250);
            orbSettings->Add<SDK::MenuUI::MenuBool>("LimitAttack", "Don't Kite if AS > 2.5", false);

            // Farm sub-menu
            auto farm = m_menu->AddSubMenu("Farm", "Farm");
            farm->Add<SDK::MenuUI::MenuSlider>("FarmDelay", "Farm Delay", 30, 0, 200);

            // Drawing sub-menu
            auto draw = m_menu->AddSubMenu("Drawing", "Drawing");
            draw->Add<SDK::MenuUI::MenuBool>("DrawAttackRange", "Draw Attack Range", true);
            draw->Add<SDK::MenuUI::MenuBool>("DrawHoldPosition", "Draw Hold Position", false);
            draw->Add<SDK::MenuUI::MenuBool>("DrawKillableMinion", "Draw Killable Minion", false);

            // Keybinds
            m_menu->Add<SDK::MenuUI::MenuKeyBind>("Combo", "Combo", VK_SPACE, SDK::MenuUI::KeyBindType::Press);
            m_menu->Add<SDK::MenuUI::MenuKeyBind>("Harass", "Harass", 'C', SDK::MenuUI::KeyBindType::Press);
            m_menu->Add<SDK::MenuUI::MenuKeyBind>("LaneClear", "LaneClear", 'V', SDK::MenuUI::KeyBindType::Press);
            m_menu->Add<SDK::MenuUI::MenuKeyBind>("LastHit", "LastHit", 'X', SDK::MenuUI::KeyBindType::Press);
            m_menu->Add<SDK::MenuUI::MenuKeyBind>("Flee", "Flee", 'Z', SDK::MenuUI::KeyBindType::Press);
        }

        void OnUnload() override {
            SDK::MenuUI::Menu::Remove("Orbwalker");
            m_menu.reset();
        }

        // ====================================================================
        // Per-frame logic
        // ====================================================================
        void OnUpdate() override {
            if (!m_menu) return;
            m_menu->UpdateKeyBinds();

            auto& player = SDK::GameObjects::Player;
            if (!player.IsValid() || !player.IsAlive()) return;
            if (!SDK::Game::ShouldProcessInput()) return;

            m_activeMode = GetActiveMode();
            if (m_activeMode == SDK::OrbwalkingMode::None) return;

            SDK::GameObject target = GetTarget();
            Orbwalk(target);
        }

        // ====================================================================
        // Drawing
        // ====================================================================
        void OnRender() override {
            if (!m_menu) return;
            auto& player = SDK::GameObjects::Player;
            if (!player.IsValid() || !player.IsAlive()) return;

            auto* drawMenu = m_menu->GetSubMenu("Drawing");
            if (!drawMenu) return;

            // Draw attack range
            auto* drawRange = drawMenu->Get<SDK::MenuUI::MenuBool>("DrawAttackRange");
            if (drawRange && drawRange->Enabled) {
                SDK::Drawing::DrawCircle(player.GetPosition(),
                    player.GetRealAttackRange(), IM_COL32(200, 150, 200, 150), 1.5f);
            }

            // Draw hold position
            auto* drawHold = drawMenu->Get<SDK::MenuUI::MenuBool>("DrawHoldPosition");
            if (drawHold && drawHold->Enabled) {
                auto* settings = m_menu->GetSubMenu("Settings");
                int holdDist = settings ? settings->Get<SDK::MenuUI::MenuSlider>("ExtraHold")->Value : 50;
                SDK::Drawing::DrawCircle(player.GetPosition(),
                    player.GetBoundingRadius() + (float)holdDist, IM_COL32(128, 0, 200, 100), 1.0f);
            }

            // Draw killable minion
            auto* drawKill = drawMenu->Get<SDK::MenuUI::MenuBool>("DrawKillableMinion");
            if (drawKill && drawKill->Enabled) {
                for (auto& minion : SDK::GameObjects::EnemyMinions) {
                    if (!minion.IsAlive() || !minion.IsVisible()) continue;
                    float dist = player.DistanceTo(minion);
                    if (dist > player.GetRealAttackRange() * 2.0f) continue;

                    float hp = minion.GetHealth();
                    float dmg = player.CalcPhysicalDamage(minion);
                    if (hp <= dmg) {
                        SDK::Drawing::DrawCircle(minion.GetPosition(),
                            minion.GetBoundingRadius() * 2.0f, IM_COL32(0, 255, 0, 200), 2.0f);
                    }
                }
            }

            // Draw active mode indicator
            if (m_activeMode != SDK::OrbwalkingMode::None) {
                ImDrawList* dl = ImGui::GetBackgroundDrawList();
                if (dl) {
                    const char* modeNames[] = { "", "Combo", "Harass", "LastHit", "LaneClear", "Flee" };
                    int idx = (int)m_activeMode;
                    if (idx >= 1 && idx <= 5) {
                        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
                        ImVec2 textSize = ImGui::CalcTextSize(modeNames[idx]);
                        dl->AddText(ImVec2(displaySize.x / 2 - textSize.x / 2, displaySize.y - 80),
                            IM_COL32(100, 200, 100, 200), modeNames[idx]);
                    }
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
        // Public API (for scripts)
        // ====================================================================
        SDK::OrbwalkingMode GetActiveMode() const {
            if (!m_menu) return SDK::OrbwalkingMode::None;

            auto* combo = m_menu->Get<SDK::MenuUI::MenuKeyBind>("Combo");
            if (combo && combo->Active) return SDK::OrbwalkingMode::Combo;

            auto* harass = m_menu->Get<SDK::MenuUI::MenuKeyBind>("Harass");
            if (harass && harass->Active) return SDK::OrbwalkingMode::Hybrid;

            auto* laneclear = m_menu->Get<SDK::MenuUI::MenuKeyBind>("LaneClear");
            if (laneclear && laneclear->Active) return SDK::OrbwalkingMode::LaneClear;

            auto* lasthit = m_menu->Get<SDK::MenuUI::MenuKeyBind>("LastHit");
            if (lasthit && lasthit->Active) return SDK::OrbwalkingMode::LastHit;

            auto* flee = m_menu->Get<SDK::MenuUI::MenuKeyBind>("Flee");
            if (flee && flee->Active) return SDK::OrbwalkingMode::Flee;

            return SDK::OrbwalkingMode::None;
        }

        bool CanAttack(float extraWindup = 0.0f) const {
            auto& player = SDK::GameObjects::Player;
            if (!player.IsValid()) return false;

            float now = SDK::Game::GetTime();
            float delay = player.GetAttackDelay();

            // Check buffs that prevent attacking
            SDK::BuffManager buffs(player.address);
            if (buffs.HasBuff("tahmkenchwhasdevouredtarget")) return false;
            if (buffs.HasBuffOfType(SDK::BuffType::Fear)) return false;
            if (buffs.HasBuffOfType(SDK::BuffType::Polymorph)) return false;

            float ping = SDK::Game::GetPing();
            return (now * 1000.0f + ping / 2.0f + 25.0f) >= (m_lastAutoAttackTick + delay * 1000.0f + extraWindup);
        }

        bool CanMove(float extraWindup = 0.0f) const {
            auto& player = SDK::GameObjects::Player;
            if (!player.IsValid()) return false;

            float now = SDK::Game::GetTime();
            float windup = player.GetAttackWindup();
            float ping = SDK::Game::GetPing();

            auto* settings = m_menu ? m_menu->GetSubMenu("Settings") : nullptr;
            float windupDelay = settings ? (float)(settings->Get<SDK::MenuUI::MenuSlider>("WindupDelay")->Value) : 60.0f;

            return (now * 1000.0f + ping / 2.0f) >= (m_lastAutoAttackTick + windup * 1000.0f + windupDelay + extraWindup);
        }

        void ResetAutoAttackTimer() {
            m_lastAutoAttackTick = 0.0f;
        }

        SDK::OrbwalkingMode ActiveMode() const { return m_activeMode; }

    private:
        // ====================================================================
        // State
        // ====================================================================
        std::shared_ptr<SDK::MenuUI::Menu> m_menu;
        SDK::OrbwalkingMode m_activeMode = SDK::OrbwalkingMode::None;
        float m_lastAutoAttackTick = 0.0f;
        float m_lastMoveTick = 0.0f;
        int m_autoAttackCounter = 0;

        // ====================================================================
        // Target selection (ported from NewOrbwalker.GetTarget)
        // ====================================================================
        SDK::GameObject GetTarget() {
            auto& player = SDK::GameObjects::Player;
            if (!player.IsValid()) return SDK::GameObject();

            if (m_activeMode == SDK::OrbwalkingMode::None || m_activeMode == SDK::OrbwalkingMode::Flee)
                return SDK::GameObject();

            float range = player.GetRealAttackRange();

            // Priority 1: Farm over harass check
            bool farmOverHarass = true;
            if (auto* p = m_menu->GetSubMenu("Prioritize"))
                if (auto* foh = p->Get<SDK::MenuUI::MenuBool>("FarmOverHarass"))
                    farmOverHarass = foh->Enabled;

            // If not farm-over-harass in harass/laneclear: prioritize heroes
            if ((m_activeMode == SDK::OrbwalkingMode::Hybrid ||
                 m_activeMode == SDK::OrbwalkingMode::LaneClear) && !farmOverHarass) {
                auto hero = GetBestHeroTarget(range);
                if (hero.IsValid()) return hero;
            }

            // Priority 2: Last-hittable minion (not in combo)
            if (m_activeMode != SDK::OrbwalkingMode::Combo) {
                auto lhMinion = GetLastHitMinion(range);
                if (lhMinion.IsValid()) return lhMinion;
            }

            // Priority 3: Hero target
            if (m_activeMode != SDK::OrbwalkingMode::LastHit) {
                auto hero = GetBestHeroTarget(range);
                if (hero.IsValid()) return hero;
            }

            // Priority 4: Jungle monsters (in laneclear/harass/lasthit)
            if (m_activeMode == SDK::OrbwalkingMode::LaneClear ||
                m_activeMode == SDK::OrbwalkingMode::Hybrid ||
                m_activeMode == SDK::OrbwalkingMode::LastHit) {
                auto jungle = GetBestJungleTarget(range);
                if (jungle.IsValid()) return jungle;
            }

            // Priority 5: Any minion (laneclear push)
            if (m_activeMode == SDK::OrbwalkingMode::LaneClear) {
                auto pushMinion = GetPushMinion(range);
                if (pushMinion.IsValid()) return pushMinion;
            }

            return SDK::GameObject();
        }

        SDK::GameObject GetBestHeroTarget(float range) {
            auto& player = SDK::GameObjects::Player;
            SDK::GameObject best;
            float bestHP = 999999.0f;

            for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                if (!hero.IsAlive() || !hero.IsVisible()) continue;
                if (!hero.IsTargetable()) continue;
                if (!player.IsInAttackRange(hero)) continue;

                float hp = hero.GetHealth();
                if (hp < bestHP) {
                    bestHP = hp;
                    best = hero;
                }
            }
            return best;
        }

        SDK::GameObject GetLastHitMinion(float range) {
            auto& player = SDK::GameObjects::Player;
            SDK::GameObject best;
            float lowestHP = 999999.0f;

            for (auto& minion : SDK::GameObjects::EnemyMinions) {
                if (!minion.IsAlive() || !minion.IsVisible()) continue;
                if (!player.IsInAttackRange(minion)) continue;

                float hp = minion.GetHealth();
                float dmg = player.CalcPhysicalDamage(minion);
                if (hp <= dmg && hp < lowestHP) {
                    lowestHP = hp;
                    best = minion;
                }
            }
            return best;
        }

        SDK::GameObject GetBestJungleTarget(float range) {
            auto& player = SDK::GameObjects::Player;
            SDK::GameObject best;
            float bestHP = 0.0f;

            bool smallFirst = false;
            if (auto* p = m_menu->GetSubMenu("Prioritize"))
                if (auto* sj = p->Get<SDK::MenuUI::MenuBool>("SmallJungle"))
                    smallFirst = sj->Enabled;

            for (auto& mob : SDK::GameObjects::JungleMinions) {
                if (!mob.IsAlive() || !mob.IsVisible()) continue;
                if (!player.IsInAttackRange(mob)) continue;

                float hp = mob.GetMaxHealth();
                if (smallFirst) {
                    // Prefer smaller mobs
                    if (!best.IsValid() || hp < bestHP) {
                        bestHP = hp;
                        best = mob;
                    }
                } else {
                    // Prefer bigger mobs
                    if (hp > bestHP) {
                        bestHP = hp;
                        best = mob;
                    }
                }
            }
            return best;
        }

        SDK::GameObject GetPushMinion(float range) {
            auto& player = SDK::GameObjects::Player;
            SDK::GameObject best;
            float bestHP = 0.0f;

            for (auto& minion : SDK::GameObjects::EnemyMinions) {
                if (!minion.IsAlive() || !minion.IsVisible()) continue;
                if (!player.IsInAttackRange(minion)) continue;

                float hp = minion.GetHealth();
                if (hp > bestHP) {
                    bestHP = hp;
                    best = minion;
                }
            }
            return best;
        }

        // ====================================================================
        // Orbwalk — Attack + Move cycle
        // ====================================================================
        void Orbwalk(SDK::GameObject& target) {
            auto& player = SDK::GameObjects::Player;
            if (!player.IsValid()) return;

            float now = SDK::Game::GetTime();
            float nowMs = now * 1000.0f;

            // Attack if possible
            if (CanAttack() && target.IsValid() && player.IsInAttackRange(target)) {
                Attack(target);
                return;
            }

            // Move if possible
            if (CanMove()) {
                Vec3 mousePos = SDK::Game::GetMouseWorldPos();

                auto* settings = m_menu ? m_menu->GetSubMenu("Settings") : nullptr;
                float holdDist = 50.0f;
                if (settings) {
                    auto* hold = settings->Get<SDK::MenuUI::MenuSlider>("ExtraHold");
                    if (hold) holdDist = (float)hold->Value;
                }

                // Hold position check
                float dist = player.GetPosition().Distance2D(mousePos);
                if (dist < (std::max)(30.0f, holdDist + player.GetBoundingRadius())) {
                    return;
                }

                // Throttle move commands
                float ping = SDK::Game::GetPing();
                float moveInterval = 70.0f + (std::min)(60.0f, ping);
                if (nowMs - m_lastMoveTick < moveInterval) return;

                MoveTo(mousePos);
            }
        }

        void Attack(SDK::GameObject& target) {
            SDK::Orbwalker::IssueOrder(3, target.GetPosition(), &target);
            float now = SDK::Game::GetTime();
            float ping = SDK::Game::GetPing();
            m_lastAutoAttackTick = now * 1000.0f - ping / 2.0f;
            m_autoAttackCounter++;
        }

        void MoveTo(Vec3 pos) {
            SDK::Orbwalker::IssueOrder(2, pos);
            m_lastMoveTick = SDK::Game::GetTime() * 1000.0f;
        }

    };

} // namespace Plugins
