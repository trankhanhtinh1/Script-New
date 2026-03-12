#pragma once
#include "../IPlugin.h"
#include "sdk/UI/MenuUI.h"
#include "sdk/GameObjects/GameObject.h"
#include "sdk/GameObjects/GameObjects.h"
#include "sdk/GameObjects/Missile.h"
#include "sdk/Game.h"
#include "sdk/GameObjects/SpellBook.h"
#include "sdk/GameObjects/BuffManager.h"
#include "sdk/UI/Drawing.h"
#include "sdk/Wrappers/Orbwalking/Orbwalker.h"
#include "sdk/Wrappers/Damages/DamageCalc.h"
#include "sdk/Wrappers/Orbwalking/HealthPrediction.h"
#include "sdk/Wrappers/TargetSelector/TargetSelector.h"
#include "sdk/Utils/MinionUtils.h"
#include "sdk/Events/TurretAggro.h"
#include "sdk/GameObjects/AiManager.h"
#include "sdk/Enums.h"
#include "core/Globals.h"
#include "core/Offsets.h"
#include "plugins/utility/evade/EvadeCore.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>
#include <unordered_map>

// ============================================================================
// OrbwalkerPlugin advanced Ported from ImpulseAIO NewOrbwalker.cs (EnsoulSharp SDK)
// Core plugin: handles attack + move cycle with proper timing
// ============================================================================

namespace Plugins {

	class OrbwalkerPlugin : public IPlugin {
	public:
		const char* GetName() const override { return "Orbwalker 2.0"; }
		const char* GetAuthor() const override { return "NightSharp"; }
		PluginCategory GetCategory() const override { return PluginCategory::CorePlugin; }

		// ====================================================================
		// Lifecycle
		// ====================================================================
		void OnLoad() override {
			// Create E#-style menu
			m_menu = SDK::MenuUI::Menu::Create("Orbwalker", "Orbwalker 2.0");

			// Attackable sub-menu
			auto attackable = m_menu->AddSubMenu("Attackable", "Attackable Unit");
			attackable->Add<SDK::MenuUI::MenuBool>("Barrels", "GP Barrels", true);
			attackable->Add<SDK::MenuUI::MenuBool>("JunglePlant", "Jungle Plant", false);
			attackable->Add<SDK::MenuUI::MenuBool>("SpecialMinions", "Pets", true);
			attackable->Add<SDK::MenuUI::MenuBool>("Wards", "Wards", true);
			attackable->Add<SDK::MenuUI::MenuBool>("Inhibitor", "Inhibitor", true);
			attackable->Add<SDK::MenuUI::MenuBool>("Nexus", "Nexus", true);

			// Prioritize sub-menu
			auto prioritize = m_menu->AddSubMenu("Prioritize", "Prioritize");
			prioritize->Add<SDK::MenuUI::MenuBool>("FarmOverHarass", "Farm Over Harass", true);
			prioritize->Add<SDK::MenuUI::MenuBool>("SpecialMinion", "Special Minion (Barrels/Wards)", false);
			prioritize->Add<SDK::MenuUI::MenuBool>("SmallJungle", "Small Jungle", false);
			prioritize->Add<SDK::MenuUI::MenuBool>("Turret", "Turret", true);

			// Orbwalker settings sub-menu
			auto orbSettings = m_menu->AddSubMenu("Settings", "Orbwalker Settings");
			orbSettings->Add<SDK::MenuUI::MenuSlider>("ExtraHold", "Extra Hold Position", 50, 0, 250);
			orbSettings->Add<SDK::MenuUI::MenuBool>("MoveRandom", "Randomize Movement", false);
			orbSettings->Add<SDK::MenuUI::MenuSlider>("WindupDelay", "Extra Windup Delay (ms)", 30, 0, 250);
			orbSettings->Add<SDK::MenuUI::MenuBool>("LimitAttack", "Don't Kite if AS > 2.5", false);
			orbSettings->Add<SDK::MenuUI::MenuBool>("MissileCheck", "Use Missile Checks", true);
			orbSettings->Add<SDK::MenuUI::MenuBool>("CalcItemDamage", "Calculate Item Damage", true);
			orbSettings->Add<SDK::MenuUI::MenuBool>("YasuoWallCheck", "Yasuo WindWall Check", true);
			orbSettings->Add<SDK::MenuUI::MenuBool>("HighOrb", "High Frequency Walk", false);
			orbSettings->Add<SDK::MenuUI::MenuBool>("CalculateRunaway", "Calculate Runaway Distance", false);
			orbSettings->Add<SDK::MenuUI::MenuSlider>("MaxMoveDistance", "Max Move Distance", 0, 0, 1500);
			orbSettings->Add<SDK::MenuUI::MenuSlider>("MoveDelay", "Move Delay (ms)", 50, 0, 500);

			// Farm sub-menu
			auto farm = m_menu->AddSubMenu("Farm", "Farm");
			farm->Add<SDK::MenuUI::MenuSlider>("FarmDelay", "Farm Delay", 30, 0, 200);
			farm->Add<SDK::MenuUI::MenuSlider>("FastFarmDelay", "Fast Farm Delay", 0, 0, 1000);
			farm->Add<SDK::MenuUI::MenuList>("TurretFarm", "Turret Farm",
				std::vector<std::string>{"Enabled", "Off"}, 0);
			farm->Add<SDK::MenuUI::MenuSlider>("TurretFarmMaxLevel", "Turret Farm Max Level", 18, 1, 18);
			farm->Add<SDK::MenuUI::MenuBool>("ShouldWait", "Wait for Last Hit", true);

			// Misc sub-menu
			auto misc = m_menu->AddSubMenu("Misc", "Misc");
			misc->Add<SDK::MenuUI::MenuBool>("DrawChaseRange", "Draw Chase Range", false);
			misc->Add<SDK::MenuUI::MenuBool>("ShowFakeClick", "Show Fake Click", false);
			misc->Add<SDK::MenuUI::MenuSlider>("ForceChaseRange", "Force Chase Extra Range", 0, 0, 500);
			misc->Add<SDK::MenuUI::MenuKeyBind>("FindKey", "Force Chase Key", 'F', SDK::MenuUI::KeyBindType::Press);

			// Drawing sub-menu
			auto draw = m_menu->AddSubMenu("Drawing", "Drawing");
			draw->Add<SDK::MenuUI::MenuBool>("DrawAttackRange", "Draw Attack Range", true);
			draw->Add<SDK::MenuUI::MenuBool>("DrawHoldPosition", "Draw Hold Position", false);
			draw->Add<SDK::MenuUI::MenuBool>("DrawKillableMinion", "Draw Killable Minion", false);
			draw->Add<SDK::MenuUI::MenuBool>("DrawActiveMode", "Draw Active Mode", true);
			draw->Add<SDK::MenuUI::MenuBool>("DrawNonKillable", "Draw Non-Killable Minion", false);
			draw->Add<SDK::MenuUI::MenuColor>("RangeColor", "Attack Range Color", 0.8f, 0.6f, 0.8f, 0.6f);
			draw->Add<SDK::MenuUI::MenuColor>("KillableColor", "Killable Minion Color", 0.0f, 1.0f, 0.0f, 0.8f);
			draw->Add<SDK::MenuUI::MenuColor>("NonKillableColor", "Non-Killable Color", 1.0f, 0.3f, 0.0f, 0.8f);

			// Keybinds
			m_menu->Add<SDK::MenuUI::MenuSeparator>("sep_keys", "--- Keybinds ---");
			m_menu->Add<SDK::MenuUI::MenuKeyBind>("Combo", "Combo", VK_SPACE, SDK::MenuUI::KeyBindType::Press);
			m_menu->Add<SDK::MenuUI::MenuKeyBind>("Harass", "Harass", 'C', SDK::MenuUI::KeyBindType::Press);
			m_menu->Add<SDK::MenuUI::MenuKeyBind>("LaneClear", "LaneClear", 'V', SDK::MenuUI::KeyBindType::Press);
			m_menu->Add<SDK::MenuUI::MenuKeyBind>("LastHit", "LastHit", 'X', SDK::MenuUI::KeyBindType::Press);
			m_menu->Add<SDK::MenuUI::MenuKeyBind>("Flee", "Flee", 'Z', SDK::MenuUI::KeyBindType::Press);
			m_menu->Add<SDK::MenuUI::MenuKeyBind>("FastLaneClear", "Fast LaneClear", 'A', SDK::MenuUI::KeyBindType::Press);
			m_menu->Add<SDK::MenuUI::MenuKeyBind>("ComboNoMove", "Combo (No Move)", 0, SDK::MenuUI::KeyBindType::Press);
		}

		void OnUnload() override {
			SDK::MenuUI::Menu::Remove("Orbwalker");
			m_menu.reset();
			m_laneClearCachedTarget = SDK::GameObject();
			m_laneClearCachedHasValue = false;
			m_laneClearCacheExpireTime = 0.0f;
			m_laneClearCacheOrigin = Vec3(0, 0, 0);
			m_laneClearCacheRange = 0.0f;
		}

		// ====================================================================
		// Per-frame logic
		// ====================================================================
		void OnUpdate() override {
			if (!m_menu) return;

			auto& player = SDK::GameObjects::Player;
			if (!player.IsValid() || !player.IsAlive()) return;
			if (!SDK::Game::ShouldProcessInput()) return;

			m_activeMode = GetActiveMode();
		SDK::Orbwalker::ActiveMode = m_activeMode; // Sync to SDK so champion scripts see the mode
			m_turretFarmWait = false;
			if (m_activeMode != SDK::OrbwalkingMode::LaneClear) {
				m_laneClearCachedTarget = SDK::GameObject();
				m_laneClearCachedHasValue = false;
				m_laneClearCacheExpireTime = 0.0f;
			}
			if (m_activeMode == SDK::OrbwalkingMode::None) return;

			// Update ForceChase state
			m_forceChaseActive = false;
			if (m_activeMode == SDK::OrbwalkingMode::Combo) {
				auto* misc = m_menu->GetSubMenu("Misc");
				if (misc) {
					auto* findKey = misc->Get<SDK::MenuUI::MenuKeyBind>("FindKey");
					auto* chaseRange = misc->Get<SDK::MenuUI::MenuSlider>("ForceChaseRange");
					if (findKey && findKey->Active && chaseRange && chaseRange->Value > 0) {
						m_forceChaseActive = true;
						m_forceChaseExtraRange = (float)chaseRange->Value;
					}
				}
			}

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
				auto* rangeCol = drawMenu->Get<SDK::MenuUI::MenuColor>("RangeColor");
				ImU32 col = rangeCol ? rangeCol->GetImU32() : IM_COL32(200, 150, 200, 150);
				SDK::Drawing::DrawCircle(player.GetPosition(),
					player.GetRealAttackRange(), col, 1.5f);
			}

			// Draw hold position
			auto* drawHold = drawMenu->Get<SDK::MenuUI::MenuBool>("DrawHoldPosition");
			if (drawHold && drawHold->Enabled) {
				auto* settings = m_menu->GetSubMenu("Settings");
				int holdDist = settings ? settings->Get<SDK::MenuUI::MenuSlider>("ExtraHold")->Value : 50;
				SDK::Drawing::DrawCircle(player.GetPosition(),
					player.GetBoundingRadius() + (float)holdDist, IM_COL32(128, 0, 200, 100), 1.0f);
			}

			// Draw killable minion â€” uses HealthPrediction for accuracy
			auto* drawKill = drawMenu->Get<SDK::MenuUI::MenuBool>("DrawKillableMinion");
			if (drawKill && drawKill->Enabled) {
				auto* killCol = drawMenu->Get<SDK::MenuUI::MenuColor>("KillableColor");
				ImU32 killColor = killCol ? killCol->GetImU32() : IM_COL32(0, 255, 0, 200);
				bool calcItems = true;
				if (auto* settings = m_menu ? m_menu->GetSubMenu("Settings") : nullptr)
					if (auto* ci = settings->Get<SDK::MenuUI::MenuBool>("CalcItemDamage"))
						calcItems = ci->Enabled;
				for (auto& minion : SDK::GameObjects::EnemyMinions) {
					if (!minion.IsAlive() || !minion.IsVisible()) continue;
					float dist = player.DistanceTo(minion);
					if (dist > player.GetRealAttackRange() * 2.0f) continue;

					float dmg = SDK::DamageCalc::GetAutoAttackDamage(player, minion, false, calcItems);
					float impactMs = GetAttackImpactTimeMs(minion);
					float predictedHP = SDK::HealthPrediction::GetPrediction(minion, impactMs);
					if (predictedHP > 0.0f && predictedHP <= dmg) {
						SDK::Drawing::DrawCircle(minion.GetPosition(),
							minion.GetBoundingRadius() * 2.0f, killColor, 2.0f);
					}
				}
			}

			// Draw non-killable minions (minions that will die before AA lands)
			auto* drawNonKill = drawMenu->Get<SDK::MenuUI::MenuBool>("DrawNonKillable");
			if (drawNonKill && drawNonKill->Enabled) {
				auto* nkCol = drawMenu->Get<SDK::MenuUI::MenuColor>("NonKillableColor");
				ImU32 nkColor = nkCol ? nkCol->GetImU32() : IM_COL32(255, 75, 0, 200);
				bool calcItems = true;
				if (auto* settings2 = m_menu ? m_menu->GetSubMenu("Settings") : nullptr)
					if (auto* ci2 = settings2->Get<SDK::MenuUI::MenuBool>("CalcItemDamage"))
						calcItems = ci2->Enabled;
				for (auto& minion : SDK::GameObjects::EnemyMinions) {
					if (!minion.IsAlive() || !minion.IsVisible()) continue;
					if (!player.IsInAttackRange(minion)) continue;

					float dmg = SDK::DamageCalc::GetAutoAttackDamage(player, minion, false, calcItems);
					float impactMs = GetAttackImpactTimeMs(minion);
					float predictedHP = SDK::HealthPrediction::GetPrediction(minion, impactMs);

					if (predictedHP <= 0.0f && minion.GetHealth() > 0.0f && minion.GetHealth() <= dmg) {
						SDK::Drawing::DrawCircle(minion.GetPosition(),
							minion.GetBoundingRadius() * 2.0f, nkColor, 2.0f);
					}
				}
			}

			// Draw chase range (ForceChase visualization) â€” rainbow when active
			auto* miscMenu = m_menu->GetSubMenu("Misc");
			if (miscMenu) {
				auto* drawChase = miscMenu->Get<SDK::MenuUI::MenuBool>("DrawChaseRange");
				auto* chaseRange = miscMenu->Get<SDK::MenuUI::MenuSlider>("ForceChaseRange");
				if (drawChase && drawChase->Enabled && chaseRange && chaseRange->Value > 0) {
					float totalRange = player.GetRealAttackRange() + (float)chaseRange->Value;
					if (m_forceChaseActive) {
						// Rainbow circle when ForceChase is active
						float t = fmodf(SDK::Game::GetTime() * 2.0f, 1.0f);
						int r = (int)(sinf(t * 6.2832f) * 127 + 128);
						int g = (int)(sinf(t * 6.2832f + 2.094f) * 127 + 128);
						int b = (int)(sinf(t * 6.2832f + 4.189f) * 127 + 128);
						SDK::Drawing::DrawCircle(player.GetPosition(), totalRange,
							IM_COL32(r, g, b, 200), 2.5f);
					}
					else {
						SDK::Drawing::DrawCircle(player.GetPosition(), totalRange,
							IM_COL32(100, 200, 255, 120), 1.0f);
					}
				}
			}

			// Draw active mode indicator
			auto* drawActiveMode = drawMenu->Get<SDK::MenuUI::MenuBool>("DrawActiveMode");
			if (drawActiveMode && drawActiveMode->Enabled && m_activeMode != SDK::OrbwalkingMode::None) {
				ImDrawList* dl = ImGui::GetBackgroundDrawList();
				if (dl) {
					const char* modeNames[] = { "", "Combo", "Harass", "LastHit", "LaneClear", "Flee" };
					ImU32 modeColors[] = { 0, IM_COL32(255, 100, 100, 220), IM_COL32(255, 200, 100, 220),
						IM_COL32(100, 255, 100, 220), IM_COL32(100, 200, 255, 220), IM_COL32(200, 200, 200, 220) };
					int idx = (int)m_activeMode;
					if (idx >= 1 && idx <= 5) {
						ImVec2 displaySize = ImGui::GetIO().DisplaySize;
						ImVec2 textSize = ImGui::CalcTextSize(modeNames[idx]);
						float x = displaySize.x / 2 - textSize.x / 2;
						float y = displaySize.y - 80;
						// Shadow
						dl->AddText(ImVec2(x + 1, y + 1), IM_COL32(0, 0, 0, 180), modeNames[idx]);
						dl->AddText(ImVec2(x, y), modeColors[idx], modeNames[idx]);
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

			// ComboNoMove returns Combo mode but we track the no-move flag separately
			auto* comboNoMove = m_menu->Get<SDK::MenuUI::MenuKeyBind>("ComboNoMove");
			if (comboNoMove && comboNoMove->Active) return SDK::OrbwalkingMode::Combo;

			auto* harass = m_menu->Get<SDK::MenuUI::MenuKeyBind>("Harass");
			if (harass && harass->Active) return SDK::OrbwalkingMode::Hybrid;

			auto* fastLC = m_menu->Get<SDK::MenuUI::MenuKeyBind>("FastLaneClear");
			if (fastLC && fastLC->Active) return SDK::OrbwalkingMode::LaneClear;

			auto* laneclear = m_menu->Get<SDK::MenuUI::MenuKeyBind>("LaneClear");
			if (laneclear && laneclear->Active) return SDK::OrbwalkingMode::LaneClear;

			auto* lasthit = m_menu->Get<SDK::MenuUI::MenuKeyBind>("LastHit");
			if (lasthit && lasthit->Active) return SDK::OrbwalkingMode::LastHit;

			auto* flee = m_menu->Get<SDK::MenuUI::MenuKeyBind>("Flee");
			if (flee && flee->Active) return SDK::OrbwalkingMode::Flee;

			return SDK::OrbwalkingMode::None;
		}

		bool IsComboNoMove() const {
			if (!m_menu) return false;
			auto* comboNoMove = m_menu->Get<SDK::MenuUI::MenuKeyBind>("ComboNoMove");
			return comboNoMove && comboNoMove->Active;
		}

		bool IsFastLaneClear() const {
			if (!m_menu) return false;
			auto* fastLC = m_menu->Get<SDK::MenuUI::MenuKeyBind>("FastLaneClear");
			return fastLC && fastLC->Active;
		}

		bool IsItemDamageCalculationEnabled() const {
			if (!m_menu) return true;
			if (auto* settings = m_menu->GetSubMenu("Settings")) {
				if (auto* ci = settings->Get<SDK::MenuUI::MenuBool>("CalcItemDamage")) {
					return ci->Enabled;
				}
			}
			return true;
		}

		float GetFarmDelayMs() const {
			if (!m_menu) return 30.0f;
			if (auto* farm = m_menu->GetSubMenu("Farm")) {
				if (auto* fd = farm->Get<SDK::MenuUI::MenuSlider>("FarmDelay")) {
					return (float)fd->Value;
				}
			}
			return 30.0f;
		}

		float GetFastFarmDelayMs() const {
			if (!m_menu) return 220.0f;
			if (auto* farm = m_menu->GetSubMenu("Farm")) {
				if (auto* ffd = farm->Get<SDK::MenuUI::MenuSlider>("FastFarmDelay")) {
					return (float)ffd->Value;
				}
			}
			return 220.0f;
		}

		// ====================================================================
		// Timing â€” Simplified approach matching Script-New-main (proven working)
		// Uses m_lastAttackCommandTime (set when we send attack command)
		// and optionally SDK::Orbwalker::LastAttackTime (set by OnProcessSpellCast event)
		// ====================================================================

		bool CanAttack(float extraWindup = 0.0f) const {
			auto& player = SDK::GameObjects::Player;
			if (!player.IsValid()) return false;

			// Check buffs that prevent attacking
			SDK::BuffManager buffs(player.address);
			if (buffs.HasBuff("tahmkenchwhasdevouredtarget")) return false;
			if (buffs.HasBuffOfType(SDK::BuffType::Fear)) return false;
			if (buffs.HasBuffOfType(SDK::BuffType::Polymorph)) return false;
			if (buffs.HasBuffOfType(SDK::BuffType::Stun)) return false;
			if (buffs.HasBuffOfType(SDK::BuffType::Charm)) return false;
			if (buffs.HasBuffOfType(SDK::BuffType::Suppression)) return false;

			// 1.1 Champion-specific: Jhin reload check
			std::string champName = player.GetChampionName();
			if (_stricmp(champName.c_str(), "Jhin") == 0) {
				if (buffs.HasBuff("JhinPassiveReload")) return false;
			}

			// 1.1 Champion-specific: Graves ammo check
			if (_stricmp(champName.c_str(), "Graves") == 0) {
				if (buffs.HasBuff("GravesBasicAttackAmmo1") == false &&
					buffs.HasBuff("GravesBasicAttackAmmo2") == false) {
					return false; // No ammo
				}
			}

			float now = SDK::Game::GetTime();
			float delay = player.GetAttackDelay();

			// Simple timing: can attack when full attack delay has passed
			// Same formula as Script-New-main (proven working)
			return now >= m_lastAttackCommandTime + delay;
		}

		bool CanMove(float extraWindup = 0.0f) const {
			auto& player = SDK::GameObjects::Player;
			if (!player.IsValid()) return false;

			std::string champName = player.GetChampionName();

			// 1.1 Kalista: can always move after attack
			if (_stricmp(champName.c_str(), "Kalista") == 0) return true;

			// Rengar special: extra windup during leap attack
			if (_stricmp(champName.c_str(), "Rengar") == 0) {
				SDK::BuffManager buffs(player.address);
				if (buffs.HasBuff("RengarR")) {
					extraWindup += 0.05f; // Extra 50ms windup for leap attacks
				}
			}

			float now = SDK::Game::GetTime();
			float windup = player.GetAttackWindup();

			// Missile launched â€” allow movement immediately (ranged champions)
			auto* settings = m_menu ? m_menu->GetSubMenu("Settings") : nullptr;
			bool missileCheck = settings ? settings->Get<SDK::MenuUI::MenuBool>("MissileCheck")->Enabled : true;
			if (SDK::Orbwalker::MissileLaunched && missileCheck) return true;

			// Get windup buffer from menu (in ms, convert to seconds)
			float windupDelayMs = settings ? (float)(settings->Get<SDK::MenuUI::MenuSlider>("WindupDelay")->Value) : 30.0f;
			float windupBuffer = windupDelayMs / 1000.0f;

			// Core timing: can move after windup + buffer
			return now >= m_lastAttackCommandTime + windup + windupBuffer + extraWindup;
		}

		void ResetAutoAttackTimer() {
			auto& player = SDK::GameObjects::Player;
			m_lastAttackCommandTime = 0.0f;
			SDK::Orbwalker::LastAttackTime = 0.0f;
			SDK::Orbwalker::MissileLaunched = false;
			if (!m_attackReadyEventFired && player.IsValid()) {
				SDK::OrbwalkingActionArgs readyArgs;
				readyArgs.Target = m_lastTarget;
				readyArgs.Sender = player;
				readyArgs.Type = SDK::OrbwalkingType::OnAttackReady;
				readyArgs.Process = true;
				SDK::Orbwalker::InvokeAction(readyArgs);
			}
			m_attackReadyEventFired = true;
			m_moveReadyEventFired = true;
		}

		SDK::OrbwalkingMode ActiveMode() const { return m_activeMode; }

		// ====================================================================
		// ShouldWait â€” EnsoulSharp OrbwalkerSelector.ShouldWait
		// Returns true if any minion will be last-hittable soon, so we
		// should NOT push the wave (used in LaneClear to delay attacks)
		// ====================================================================
		bool ShouldWait(float range) {
			return AnalyzeLaneFarm(range).shouldWait;
		}

		// ====================================================================
		// BlockOrders â€” allow scripts to temporarily block attacks/moves
		// ====================================================================
		bool BlockAttack() const { return m_blockAttack; }
		bool BlockMove() const { return m_blockMove; }
		void SetBlockAttack(bool block) { m_blockAttack = block; }
		void SetBlockMove(bool block) { m_blockMove = block; }

	private:
		// ====================================================================
		// State
		// ====================================================================
		std::shared_ptr<SDK::MenuUI::Menu> m_menu;
		SDK::OrbwalkingMode m_activeMode = SDK::OrbwalkingMode::None;
		float m_lastAttackCommandTime = 0.0f;  // Game time when we SENT the attack command
		float m_lastMoveTime = 0.0f;            // Game time (seconds) for move throttling
		int m_autoAttackCounter = 0;
		bool m_attackReadyEventFired = true;
		bool m_afterAttackFired = true;
		bool m_moveReadyEventFired = true;
		bool m_forceChaseActive = false;
		float m_forceChaseExtraRange = 0.0f;
		bool m_blockAttack = false;
		bool m_blockMove = false;
		Vec3 m_lastMoveDir = Vec3(0, 0, 0);     // For angle check
		SDK::GameObject m_lastTarget;             // For TargetSwitch event
		SDK::GameObject m_laneClearCachedTarget;
		float m_laneClearCacheExpireTime = 0.0f;
		Vec3 m_laneClearCacheOrigin = Vec3(0, 0, 0);
		float m_laneClearCacheRange = 0.0f;
		bool m_laneClearCachedHasValue = false;
		bool m_turretFarmWait = false;

		struct TurretShotInfo {
			SDK::GameObject turret;
			bool isTargeted = false;
			int shotsBeforeImpact = 0;
			int shotsBeforeNextImpact = 0;
			float shotDamage = 0.0f;
		};

		struct PendingTurretShot {
			int casterNetId = 0;
			float arrivalMs = FLT_MAX;
		};

		using PendingTurretShotMap = std::unordered_map<int, std::vector<PendingTurretShot>>;

		struct LaneFarmEntry {
			SDK::GameObject minion;
			float currentHp = 0.0f;
			float damage = 0.0f;
			float impactHp = 0.0f;
			float nextImpactHp = 0.0f;
			int aggroCount = 0;
			bool isSiege = false;
			bool hasIncomingMissile = false;
		};

		struct LaneFarmAnalysis {
			bool shouldWait = false;
			SDK::GameObject lastHitTarget;
			SDK::GameObject pushTarget;
		};

		// ====================================================================
		// 1.2 GetProjectileSpeed â€” AA missile speed per champion
		// Source: EnsoulSharp NewOrbwalker.cs GetProjectileSpeed()
		// ====================================================================
		static float GetProjectileSpeed(const SDK::GameObject& unit) {
			// Melee = instant (max float)
			if (unit.IsMelee()) return FLT_MAX;

			std::string name = unit.GetChampionName();
			if (name.empty()) return 2000.0f; // default ranged

			// Champion-specific missile speeds (from EnsoulSharp)
			static const std::unordered_map<std::string, float> speedMap = {
				// Buff-dependent (return max to let buff check handle it)
				{"Jinx", 2750.0f},    // Default; with JinxQ rockets = 2000
				{"Kayle", 2000.0f},   // With range upgrade 2250
				{"Viktor", 2300.0f},  // ViktorPowerTransferReturn max
				{"Neeko", 1500.0f},   // neekowpassiveready max (melee transform)

				// Form-dependent
				{"Jayce", 2500.0f},   // Ranged form
				{"Nidalee", 1750.0f}, // Ranged form
				{"Elise", 1600.0f},   // Human form

				// Special
				{"Ivern", 1600.0f},   // ivernwpassive passive range
				{"Poppy", 1600.0f},   // poppypassivebuff ranged AA
				{"Thresh", 1800.0f},
				{"Rakan", 1800.0f},

				// Aphelios â€” weapon-dependent (average value)
				{"Aphelios", 2100.0f},

				// ADCs with known speeds
				{"Caitlyn", 2500.0f},
				{"Ezreal", 2000.0f},
				{"Ashe", 2000.0f},
				{"Varus", 2000.0f},
				{"KogMaw", 1800.0f},
				{"Twitch", 2500.0f},
				{"Tristana", 2250.0f},
				{"Lucian", 2800.0f},
				{"Vayne", 2000.0f},
				{"Draven", 1600.0f},
				{"Jhin", 2600.0f},
				{"MissFortune", 2000.0f},
				{"Kalista", 2400.0f},
				{"Sivir", 1750.0f},
				{"Xayah", 2075.0f},
				{"Kaisa", 2000.0f},
				{"Senna", 20000.0f}, // Basically instant
				{"Samira", 2600.0f},
				{"Zeri", 2600.0f},
				{"Nilah", FLT_MAX},   // Melee-like
				{"Smolder", 2500.0f},

				// Mages/ranged with known speeds
				{"Teemo", 1500.0f},
				{"Azir", FLT_MAX},   // Soldier attacks (instant for orbwalker purposes)
				{"Orianna", 1450.0f},
				{"Syndra", 1800.0f},
				{"Lux", 1600.0f},
				{"Ahri", 1750.0f},
				{"Annie", 1500.0f},
				{"Brand", 1600.0f},
				{"Cassiopeia", 1500.0f},
				{"Velkoz", 1600.0f},
				{"Xerath", 2050.0f},
				{"Ziggs", 1500.0f},
				{"Zyra", 1700.0f},
				{"Lulu", 1450.0f},
				{"Nami", 1500.0f},
				{"Sona", 1500.0f},
				{"Soraka", 1500.0f},
				{"Janna", 1600.0f},
				{"Yuumi", 1500.0f},
				{"Seraphine", 1500.0f},
				{"Heimerdinger", 1500.0f},
				{"Kennen", 1600.0f},
				{"Quinn", 2000.0f},
				{"Kindred", 2000.0f},
				{"Graves", FLT_MAX}, // Shotgun = melee-like (no missile)
			};

			auto it = speedMap.find(name);
			if (it != speedMap.end())
				return it->second;

			// Primary fallback: SDK spell-data read.
			float sdkSpeed = unit.GetBasicAttackMissileSpeed();
			if (sdkSpeed > 100.0f && sdkSpeed < 10000.0f)
				return sdkSpeed;

			return 2000.0f;
		}

		// ====================================================================
		// 1.4 Jax CounterStrike check â€” skip attack if target has this buff
		// ====================================================================
		static bool HasJaxCounterStrike(const SDK::GameObject& target) {
			if (!target.IsValid() || !target.IsHero()) return false;
			SDK::BuffManager buffs(target.address);
			return buffs.HasBuff("JaxCounterStrike");
		}

		// ====================================================================
		// Target selection (ported from NewOrbwalker.GetTarget)
		// Enhanced: 1.10 ForceChase, 1.4 Jax check, 1.14 SpecialMinion
		// ====================================================================
		SDK::GameObject GetTarget() {
			auto& player = SDK::GameObjects::Player;
			if (!player.IsValid()) return SDK::GameObject();

			if (m_activeMode == SDK::OrbwalkingMode::None || m_activeMode == SDK::OrbwalkingMode::Flee)
				return SDK::GameObject();

			float range = player.GetRealAttackRange();
			const bool useLaneClearCache = (m_activeMode == SDK::OrbwalkingMode::LaneClear && !CanAttack());
			const float now = SDK::Game::GetTime();

			auto finalizeTarget = [&](const SDK::GameObject& target) -> SDK::GameObject {
				if (useLaneClearCache) {
					m_laneClearCachedTarget = target;
					m_laneClearCachedHasValue = target.IsValid();
					m_laneClearCacheExpireTime = now + 0.025f;
					m_laneClearCacheOrigin = player.GetPosition();
					m_laneClearCacheRange = range;
				}
				return target;
				};

			if (useLaneClearCache && now < m_laneClearCacheExpireTime) {
				const bool sameOrigin = m_laneClearCacheOrigin.IsValid() &&
					player.GetPosition().Distance2D(m_laneClearCacheOrigin) <= 35.0f;
				const bool sameRange = fabsf(range - m_laneClearCacheRange) <= 1.0f;
				if (!sameOrigin || !sameRange) {
					m_laneClearCachedHasValue = false;
					m_laneClearCacheExpireTime = 0.0f;
				}
				else if (m_laneClearCachedHasValue &&
					m_laneClearCachedTarget.IsValid() &&
					m_laneClearCachedTarget.IsAlive() &&
					player.DistanceTo(m_laneClearCachedTarget) <= range + m_laneClearCachedTarget.GetBoundingRadius()) {
					return m_laneClearCachedTarget;
				}

				if (sameOrigin && sameRange && !m_laneClearCachedHasValue) {
					return SDK::GameObject();
				}
			}

			// 1.10 ForceChase extend attack range when key held
			if (m_forceChaseActive)
				range += m_forceChaseExtraRange;

			// Priority 0: Special minions (GP barrels, wards, plants, pets) [1.14]
			bool prioritizeSpecial = false;
			if (auto* p = m_menu->GetSubMenu("Prioritize"))
				if (auto* sm = p->Get<SDK::MenuUI::MenuBool>("SpecialMinion"))
					prioritizeSpecial = sm->Enabled;

			if (prioritizeSpecial) {
				auto special = GetSpecialMinion(range);
				if (special.IsValid()) return finalizeTarget(special);
			}

			// Priority 1: Farm over harass check
			bool farmOverHarass = true;
			if (auto* p = m_menu->GetSubMenu("Prioritize"))
				if (auto* foh = p->Get<SDK::MenuUI::MenuBool>("FarmOverHarass"))
					farmOverHarass = foh->Enabled;

			// If not farm-over-harass in harass/laneclear: prioritize heroes
			if ((m_activeMode == SDK::OrbwalkingMode::Hybrid ||
				m_activeMode == SDK::OrbwalkingMode::LaneClear) && !farmOverHarass) {
				auto hero = GetBestHeroTarget(range);
				if (hero.IsValid()) return finalizeTarget(hero);
			}

			LaneFarmAnalysis laneFarmAnalysis;
			const bool needLaneFarmAnalysis = (m_activeMode != SDK::OrbwalkingMode::Combo);
			if (needLaneFarmAnalysis) {
				laneFarmAnalysis = AnalyzeLaneFarm(range);
			}

			if ((m_activeMode == SDK::OrbwalkingMode::LaneClear ||
				m_activeMode == SDK::OrbwalkingMode::LastHit) && CanTurretFarm()) {
				auto turretFarmMinion = GetTurretFarmMinion(range);
				if (turretFarmMinion.IsValid()) return finalizeTarget(turretFarmMinion);
				if (m_turretFarmWait) return finalizeTarget(SDK::GameObject());
			}

			// Priority 2: Last-hittable minion (not in combo)
			if (m_activeMode != SDK::OrbwalkingMode::Combo) {
				auto lhMinion = laneFarmAnalysis.lastHitTarget;
				if (lhMinion.IsValid()) return finalizeTarget(lhMinion);
			}

			// Priority 3: Hero target
			if (m_activeMode != SDK::OrbwalkingMode::LastHit) {
				auto hero = GetBestHeroTarget(range);
				if (hero.IsValid()) return finalizeTarget(hero);
			}

			// Priority 4: Special minions (non-priority mode) â€” in combo
			if (m_activeMode == SDK::OrbwalkingMode::Combo && !prioritizeSpecial) {
				auto special = GetSpecialMinion(range);
				if (special.IsValid()) return finalizeTarget(special);
			}

			// Priority 5: Jungle monsters (in laneclear/harass/lasthit)
			if (m_activeMode == SDK::OrbwalkingMode::LaneClear ||
				m_activeMode == SDK::OrbwalkingMode::Hybrid ||
				m_activeMode == SDK::OrbwalkingMode::LastHit) {
				auto jungle = GetBestJungleTarget(range);
				if (jungle.IsValid()) return finalizeTarget(jungle);
			}

			// Priority 6: Any minion (laneclear push â€” with ShouldWait logic)
			if (m_activeMode == SDK::OrbwalkingMode::LaneClear) {
				SDK::GameObject lcMinion;
				if (!laneFarmAnalysis.shouldWait) {
					lcMinion = laneFarmAnalysis.pushTarget;
				}
				if (lcMinion.IsValid()) return finalizeTarget(lcMinion);
			}

			// Priority 7: Turrets (in laneclear)
			if (m_activeMode == SDK::OrbwalkingMode::LaneClear) {
				bool pTurret = true;
				if (auto* p = m_menu->GetSubMenu("Prioritize"))
					if (auto* t = p->Get<SDK::MenuUI::MenuBool>("Turret"))
						pTurret = t->Enabled;
				if (pTurret) {
					for (auto& turret : SDK::GameObjects::EnemyTurrets) {
						if (!turret.IsAlive()) continue;
						if (player.DistanceTo(turret) <= range + turret.GetBoundingRadius())
							return finalizeTarget(turret);
					}
				}
			}

			// Priority 8: Inhibitors (in laneclear)
			if (m_activeMode == SDK::OrbwalkingMode::LaneClear) {
				bool attackInhib = true;
				auto* attackable = m_menu->GetSubMenu("Attackable");
				if (attackable)
					if (auto* inh = attackable->Get<SDK::MenuUI::MenuBool>("Inhibitor"))
						attackInhib = inh->Enabled;

				if (attackInhib) {
					for (auto& inhib : SDK::GameObjects::EnemyInhibitors) {
						if (!inhib.IsAlive()) continue;
						if (player.DistanceTo(inhib) <= range + inhib.GetBoundingRadius())
							return finalizeTarget(inhib);
					}
				}
			}

			// Priority 9: Nexus (in laneclear)
			if (m_activeMode == SDK::OrbwalkingMode::LaneClear) {
				bool attackNexus = true;
				auto* attackable = m_menu->GetSubMenu("Attackable");
				if (attackable)
					if (auto* nex = attackable->Get<SDK::MenuUI::MenuBool>("Nexus"))
						attackNexus = nex->Enabled;

				if (attackNexus) {
					for (auto& nexus : SDK::GameObjects::EnemyNexus) {
						if (!nexus.IsAlive()) continue;
						if (player.DistanceTo(nexus) <= range + nexus.GetBoundingRadius())
							return finalizeTarget(nexus);
					}
				}
			}

			return finalizeTarget(SDK::GameObject());
		}

		SDK::GameObject GetBestHeroTarget(float range) {
			auto& player = SDK::GameObjects::Player;

			// 1. Check SDK::TargetSelector::ForcedTarget (set by TargetSelectorPlugin click)
			auto forced = SDK::TargetSelector::GetForcedTarget();
			if (forced.IsValid() && forced.IsAlive() && forced.IsVisible()) {
				if (player.DistanceTo(forced) <= range + forced.GetBoundingRadius()) {
					if (!HasJaxCounterStrike(forced))
						return forced;
				}
				// "Only Attack Selected Target"
				if (SDK::TargetSelector::OnlyAttackSelected)
					return SDK::GameObject();
			}

			// "Only Attack Selected Target" with a valid forced target that's
			// dead/invisible â†’ still don't attack other targets
			if (SDK::TargetSelector::OnlyAttackSelected &&
				SDK::TargetSelector::ForcedTarget.IsValid())
				return SDK::GameObject();

			// 2. Use SDK::TargetSelector for smart target selection
			auto target = SDK::TargetSelector::GetTarget(range);

			// 1.4 Jax CounterStrike â€” skip if target has it, try next
			if (target.IsValid() && HasJaxCounterStrike(target)) {
				auto targets = SDK::TargetSelector::GetTargets(range);
				for (auto& t : targets) {
					if (!HasJaxCounterStrike(t))
						return t;
				}
				return SDK::GameObject(); // All targets dodging
			}

			return target;
		}

		// ====================================================================
		// 1.14 GetSpecialMinion â€” GP barrels, enemy wards, jungle plants, pets
		// ====================================================================
		SDK::GameObject GetSpecialMinion(float range) {
			auto& player = SDK::GameObjects::Player;
			if (!m_menu) return SDK::GameObject();
			auto* attackable = m_menu->GetSubMenu("Attackable");
			if (!attackable) return SDK::GameObject();

			SDK::GameObject best;
			float bestDist = FLT_MAX;

			// GP Barrels
			bool attackBarrels = true;
			if (auto* b = attackable->Get<SDK::MenuUI::MenuBool>("Barrels"))
				attackBarrels = b->Enabled;

			if (attackBarrels) {
				for (auto& minion : SDK::GameObjects::EnemyMinions) {
					if (!minion.IsAlive() || !minion.IsVisible()) continue;
					std::string name = minion.GetName();
					if (name.find("gangplankbarrel") == std::string::npos) continue;
					float dist = player.DistanceTo(minion);
					if (dist > range + minion.GetBoundingRadius()) continue;

					// Barrel is last-hittable if HP <= 1 or 2 (based on owner level)
					float hp = minion.GetHealth();
					if (hp <= 2.0f && dist < bestDist) {
						bestDist = dist;
						best = minion;
					}
				}
				// Also check ally minions for own GP barrels
				for (auto& minion : SDK::GameObjects::AllyMinions) {
					if (!minion.IsAlive() || !minion.IsVisible()) continue;
					std::string name = minion.GetName();
					if (name.find("gangplankbarrel") == std::string::npos) continue;
					float dist = player.DistanceTo(minion);
					if (dist > range + minion.GetBoundingRadius()) continue;
					float hp = minion.GetHealth();
					if (hp <= 2.0f && dist < bestDist) {
						bestDist = dist;
						best = minion;
					}
				}
				if (best.IsValid()) return best;
			}

			// Enemy Wards
			bool attackWards = true;
			if (auto* w = attackable->Get<SDK::MenuUI::MenuBool>("Wards"))
				attackWards = w->Enabled;

			if (attackWards) {
				for (auto& ward : SDK::GameObjects::EnemyWards) {
					if (!ward.IsAlive()) continue;
					float dist = player.DistanceTo(ward);
					if (dist > range + ward.GetBoundingRadius()) continue;
					if (dist < bestDist) {
						bestDist = dist;
						best = ward;
					}
				}
				if (best.IsValid()) return best;
			}

			// Jungle Plants
			bool attackPlants = false;
			if (auto* jp = attackable->Get<SDK::MenuUI::MenuBool>("JunglePlant"))
				attackPlants = jp->Enabled;

			if (attackPlants) {
				for (auto& plant : SDK::GameObjects::JunglePlants) {
					if (!plant.IsAlive()) continue;
					float dist = player.DistanceTo(plant);
					if (dist > range + plant.GetBoundingRadius()) continue;
					if (dist < bestDist) {
						bestDist = dist;
						best = plant;
					}
				}
				if (best.IsValid()) return best;
			}

			// Pets
			bool attackPets = true;
			if (auto* sp = attackable->Get<SDK::MenuUI::MenuBool>("SpecialMinions"))
				attackPets = sp->Enabled;

			if (attackPets) {
				for (auto& pet : SDK::GameObjects::Pets) {
					if (!pet.IsAlive()) continue;
					if (pet.GetTeam() == player.GetTeam()) continue; // Don't attack own pets
					float dist = player.DistanceTo(pet);
					if (dist > range + pet.GetBoundingRadius()) continue;
					if (dist < bestDist) {
						bestDist = dist;
						best = pet;
					}
				}
			}

			return best;
		}

		// Calculate AA travel time (cast + projectile) in milliseconds
		float GetAAImpactTimeMs(const SDK::GameObject& target) const {
			auto& player = SDK::GameObjects::Player;
			float windup = player.GetAttackWindup() * 1000.0f;
			float ping = SDK::Game::GetPing() / 2.0f;         // half RTT

			// Projectile travel time for ranged champions
			float projTime = 0.0f;
			float projSpeed = GetProjectileSpeed(player);
			if (projSpeed < FLT_MAX && projSpeed > 0.0f) {
				float dist = player.DistanceTo(target);
				projTime = (dist / projSpeed) * 1000.0f; // ms
			}

			return windup + projTime + ping;
		}

		float GetAttackReadyDelayMs() const {
			auto& player = SDK::GameObjects::Player;
			if (!player.IsValid()) return 0.0f;

			float readyAt = m_lastAttackCommandTime + player.GetAttackDelay();
			float remainingSeconds = (std::max)(0.0f, readyAt - SDK::Game::GetTime());
			return remainingSeconds * 1000.0f;
		}

		float GetAttackImpactTimeMs(const SDK::GameObject& target, float extraDelayMs = 0.0f) const {
			return GetAttackReadyDelayMs() + GetAAImpactTimeMs(target) + extraDelayMs;
		}

		float GetNextAttackReadyTimeMs(float extraDelayMs = 0.0f) const {
			auto& player = SDK::GameObjects::Player;
			if (!player.IsValid()) return 0.0f;
			return GetAttackReadyDelayMs() + player.GetAttackDelay() * 1000.0f + extraDelayMs;
		}

		float GetNextAttackImpactTimeMs(const SDK::GameObject& target, float extraDelayMs = 0.0f) const {
			auto& player = SDK::GameObjects::Player;
			if (!player.IsValid()) return 0.0f;
			return GetAttackImpactTimeMs(target, extraDelayMs) + player.GetAttackDelay() * 1000.0f;
		}

		static float GetFollowUpSafetyBufferHp(const SDK::GameObject& minion, int aggroCount) {
			float bufferHp = 20.0f + (float)(std::min)(aggroCount, 4) * 6.0f;
			if (SDK::MinionUtils::IsSiegeMinion(minion.GetChampionName()) ||
				SDK::MinionUtils::IsSuperMinion(minion.GetChampionName())) {
				bufferHp += 20.0f;
			}
			return bufferHp;
		}

		static float GetPhysicalDamageAfterArmor(float rawDamage, float armor) {
			if (armor >= 0.0f) {
				return rawDamage * (100.0f / (100.0f + armor));
			}
			return rawDamage * (2.0f - (100.0f / (100.0f - armor)));
		}

		float GetTurretShotDamage(const SDK::GameObject& minion) const {
			return (std::max)(1.0f, GetPhysicalDamageAfterArmor(160.0f, minion.GetArmor()));
		}

		static float GetMissileArrivalTimeMs(const SDK::Missile& missile) {
			float speed = missile.IsTurretShot() ? 1200.0f : (missile.IsAutoAttack() ? 1600.0f : 1800.0f);
			if (speed <= 0.0f) return FLT_MAX;
			float dist = missile.GetPosition().Distance2D(missile.GetEndPos());
			return (dist / speed) * 1000.0f;
		}

		static PendingTurretShotMap BuildPendingTurretShotMap() {
			PendingTurretShotMap shotsByTarget;
			auto missiles = SDK::MissileManager::GetMissiles();
			for (auto& missile : missiles) {
				if (!missile.IsValid() || !missile.IsTurretShot()) continue;

				int targetNetId = missile.GetTargetNetId();
				if (targetNetId == 0) continue;

				shotsByTarget[targetNetId].push_back(
					PendingTurretShot{ missile.GetCasterNetId(), GetMissileArrivalTimeMs(missile) });
			}

			return shotsByTarget;
		}

		TurretShotInfo GetTurretShotInfo(const SDK::GameObject& minion,
			float impactTimeMs,
			float nextImpactTimeMs,
			const PendingTurretShotMap& pendingShotsByTarget) const {
			TurretShotInfo info;
			info.shotDamage = GetTurretShotDamage(minion);

			int targetNetId = minion.GetNetId();
			if (targetNetId == 0) return info;

			std::unordered_set<int> pendingTurrets;
			auto pendingIt = pendingShotsByTarget.find(targetNetId);
			if (pendingIt != pendingShotsByTarget.end()) {
				for (const auto& shot : pendingIt->second) {
					info.isTargeted = true;
					if (shot.arrivalMs <= impactTimeMs) info.shotsBeforeImpact++;
					if (shot.arrivalMs <= nextImpactTimeMs) info.shotsBeforeNextImpact++;
					pendingTurrets.insert(shot.casterNetId);
				}
			}

			SDK::GameObject turret = SDK::TurretAggro::GetTurretTargetingUnit(minion);
			if (turret.IsValid() && turret.GetTeam() == SDK::GameObjects::Player.GetTeam()) {
				info.turret = turret;
				info.isTargeted = true;

				if (pendingTurrets.find(turret.GetNetId()) == pendingTurrets.end()) {
					constexpr float turretWindupMs = 125.0f;
					if (turretWindupMs <= impactTimeMs) info.shotsBeforeImpact++;
					if (turretWindupMs <= nextImpactTimeMs) info.shotsBeforeNextImpact++;
				}
			}

			return info;
		}

		SDK::GameObject GetTurretFarmMinion(float range) {
			if (!CanTurretFarm()) return SDK::GameObject();

			auto& player = SDK::GameObjects::Player;
			bool calcItems = IsItemDamageCalculationEnabled();
			float farmDelayMs = GetFarmDelayMs();
			const PendingTurretShotMap pendingShotsByTarget = BuildPendingTurretShotMap();

			SDK::GameObject killTarget;
			SDK::GameObject prepTarget;
			float lowestKillHp = FLT_MAX;
			float bestPrepHp = FLT_MAX;
			bool shouldWait = false;

			for (auto& minion : SDK::GameObjects::EnemyMinions) {
				if (!minion.IsAlive() || !minion.IsVisible()) continue;
				if (!player.IsInAttackRange(minion)) continue;

				float damage = SDK::DamageCalc::GetAutoAttackDamage(player, minion, false, calcItems);
				if (damage <= 0.0f) continue;

				float impactTimeMs = GetAttackImpactTimeMs(minion, farmDelayMs);
				float nextImpactTimeMs = GetNextAttackImpactTimeMs(minion, farmDelayMs);
				TurretShotInfo turretInfo = GetTurretShotInfo(
					minion, impactTimeMs, nextImpactTimeMs, pendingShotsByTarget);
				if (!turretInfo.isTargeted) continue;

				float currentHp = minion.GetHealth();
				float impactHp = SDK::HealthPrediction::GetPrediction(minion, impactTimeMs);
				float nextHp = SDK::HealthPrediction::GetPrediction(minion, nextImpactTimeMs);

				if (impactHp > 0.0f && impactHp <= damage && impactHp < lowestKillHp) {
					lowestKillHp = impactHp;
					killTarget = minion;
					continue;
				}

				if (currentHp > damage && nextHp > 0.0f && nextHp <= damage) {
					shouldWait = true;
					continue;
				}

				float hpAfterPrep = currentHp - damage - turretInfo.shotDamage;
				if (turretInfo.shotsBeforeNextImpact > 0 &&
					hpAfterPrep > 0.0f &&
					hpAfterPrep <= damage &&
					hpAfterPrep < bestPrepHp) {
					bestPrepHp = hpAfterPrep;
					prepTarget = minion;
				}
			}

			if (killTarget.IsValid()) return killTarget;
			if (prepTarget.IsValid()) return prepTarget;

			m_turretFarmWait = shouldWait;
			return SDK::GameObject();
		}

		LaneFarmAnalysis AnalyzeLaneFarm(float range) const {
			LaneFarmAnalysis analysis;
			if (!m_menu) return analysis;

			auto& player = SDK::GameObjects::Player;
			if (!player.IsValid()) return analysis;

			auto* farm = m_menu->GetSubMenu("Farm");
			const bool shouldWaitEnabled = farm &&
				farm->Get<SDK::MenuUI::MenuBool>("ShouldWait") &&
				farm->Get<SDK::MenuUI::MenuBool>("ShouldWait")->Enabled;

			const bool evaluatePush = (m_activeMode == SDK::OrbwalkingMode::LaneClear);
			const bool calcItems = IsItemDamageCalculationEnabled();
			const float farmDelayMs = GetFarmDelayMs();
			const float shouldWaitDelayMs = IsFastLaneClear() ? GetFastFarmDelayMs() : farmDelayMs;
			const bool shouldWaitUsesDefaultNextImpact =
				fabsf(shouldWaitDelayMs - farmDelayMs) <= 0.001f;

			std::vector<LaneFarmEntry> killable;
			std::vector<LaneFarmEntry> entries;
			SDK::GameObject bestPush;
			float bestPushScore = -FLT_MAX;

			for (auto& minion : SDK::GameObjects::EnemyMinions) {
				if (!minion.IsAlive() || !minion.IsVisible()) continue;
				if (!player.IsInAttackRange(minion)) continue;

				LaneFarmEntry entry;
				entry.minion = minion;
				entry.currentHp = minion.GetHealth();
				entry.damage = SDK::DamageCalc::GetAutoAttackDamage(player, minion, false, calcItems);
				if (entry.damage <= 0.0f) continue;

				const float impactTimeMs = GetAttackImpactTimeMs(minion, farmDelayMs);
				const float nextImpactTimeMs = GetNextAttackImpactTimeMs(minion, farmDelayMs);
				entry.impactHp = SDK::HealthPrediction::GetPrediction(minion, impactTimeMs);
				entry.nextImpactHp = SDK::HealthPrediction::GetPrediction(minion, nextImpactTimeMs);

				if (shouldWaitEnabled) {
					float shouldWaitNextImpactHp = entry.nextImpactHp;
					if (!shouldWaitUsesDefaultNextImpact) {
						shouldWaitNextImpactHp = SDK::HealthPrediction::GetPrediction(
							minion, GetNextAttackImpactTimeMs(minion, shouldWaitDelayMs));
					}
					if (entry.currentHp > entry.damage &&
						shouldWaitNextImpactHp > 0.0f &&
						shouldWaitNextImpactHp <= entry.damage) {
						analysis.shouldWait = true;
					}
				}

				if (entry.impactHp > 0.0f && entry.impactHp <= entry.damage) {
					entry.isSiege = SDK::MinionUtils::IsSiegeMinion(minion.GetChampionName()) ||
						SDK::MinionUtils::IsSuperMinion(minion.GetChampionName());
					entry.hasIncomingMissile = HasIncomingMissileToKill(
						minion, entry.currentHp, entry.impactHp, impactTimeMs);
					killable.push_back(entry);
				}
				entries.push_back(entry);
			}

			if (evaluatePush && !entries.empty()) {
				std::vector<int> aggroCounts(entries.size(), 0);
				for (auto& allyMinion : SDK::GameObjects::AllyMinions) {
					if (!allyMinion.IsValid() || !allyMinion.IsAlive()) continue;

					for (size_t i = 0; i < entries.size(); ++i) {
						if (entries[i].minion.DistanceTo(allyMinion) <= 500.0f) {
							++aggroCounts[i];
						}
					}
				}

				for (size_t i = 0; i < entries.size(); ++i) {
					auto& entry = entries[i];
					if (entry.impactHp <= 0.0f) continue;

					entry.aggroCount = aggroCounts[i];
					const float followUpSafetyHp = GetFollowUpSafetyBufferHp(entry.minion, entry.aggroCount);

					if (entry.impactHp <= entry.damage) continue;
					if (entry.nextImpactHp > 0.0f &&
						entry.nextImpactHp <= entry.damage + followUpSafetyHp) {
						continue;
					}

					float score = entry.impactHp;
					if (entry.aggroCount > 0) {
						score = 10000.0f - entry.impactHp + (float)entry.aggroCount * 10.0f;
					}

					if (score > bestPushScore) {
						bestPushScore = score;
						bestPush = entry.minion;
					}
				}
			}

			LaneFarmEntry* bestImportant = nullptr;
			for (auto& entry : killable) {
				if (!bestImportant) {
					bestImportant = &entry;
					continue;
				}
				if (entry.isSiege && !bestImportant->isSiege) {
					bestImportant = &entry;
					continue;
				}
				if (!entry.isSiege && bestImportant->isSiege) {
					continue;
				}
				if (entry.impactHp < bestImportant->impactHp) {
					bestImportant = &entry;
				}
			}

			if (bestImportant) {
				analysis.lastHitTarget = bestImportant->minion;

				for (const auto& entry : killable) {
					if (!entry.hasIncomingMissile) continue;
					if (entry.isSiege) continue;
					if (entry.minion.GetNetId() == bestImportant->minion.GetNetId()) continue;
					if (bestImportant->nextImpactHp > 0.0f &&
						bestImportant->nextImpactHp <= bestImportant->damage) {
						analysis.lastHitTarget = entry.minion;
						break;
					}
				}
			}

			if (!analysis.shouldWait) {
				analysis.pushTarget = bestPush;
			}

			return analysis;
		}

		// ================================================================
		// HasIncomingMissile — check if a minion has an in-flight missile
		// ================================================================
		bool HasIncomingMissileToKill(const SDK::GameObject& minion,
			float currentHP,
			float hpAtImpact,
			float impactTimeMs) const {

			if (currentHP <= 0.0f) return false;

			if (hpAtImpact <= 0.0f) return true;

			int incomingCount = SDK::HealthPrediction::GetIncomingAttackCount(minion, impactTimeMs);
			if (incomingCount <= 0) return false;

			float incomingDmg = currentHP - hpAtImpact;

			return (incomingDmg >= currentHP * 0.8f);
		}

		SDK::GameObject GetLastHitMinion(float range) {
			return AnalyzeLaneFarm(range).lastHitTarget;
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
				}
				else {
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
			return AnalyzeLaneFarm(range).pushTarget;
		}

		// ====================================================================
		// 1.5 Turret farm advanced turret farming logic
		// Ported from NewOrbwalker.cs CanTurretFarm()
		// ====================================================================
		bool CanTurretFarm() {
			if (!m_menu) return false;
			auto* farm = m_menu->GetSubMenu("Farm");
			if (!farm) return false;

			auto* tfEnabled = farm->Get<SDK::MenuUI::MenuList>("TurretFarm");
			if (!tfEnabled || tfEnabled->Index != 0) return false; // "Off"

			auto& player = SDK::GameObjects::Player;

			auto* tfMaxLevel = farm->Get<SDK::MenuUI::MenuSlider>("TurretFarmMaxLevel");
			if (tfMaxLevel && player.GetLevel() > tfMaxLevel->Value) return false;

			// Check if we're actually under an ally turret
			if (!SDK::GameObjects::IsUnderAllyTurret(player.GetPosition()))
				return false;

			return true;
		}

		// Get the ally turret that is attacking a minion
		SDK::GameObject GetTurretAggro(const SDK::GameObject& minion) {
			// Find closest ally turret that is attacking this minion
			for (auto& turret : SDK::GameObjects::AllyTurrets) {
				if (!turret.IsAlive()) continue;
				float dist = turret.GetPosition().Distance2D(minion.GetPosition());
				if (dist <= 875.0f) { // turret attack range
					return turret;
				}
			}
			return SDK::GameObject();
		}

		// Predict minion HP after N turret shots
		float PredictMinionHPAfterTurretShots(const SDK::GameObject& minion, int turretShots) {
			// Approximate turret damage
			float turretDmg = 200.0f; // Base turret damage (varies, but good average)
			float minionHP = minion.GetHealth();
			float minionArmor = minion.GetArmor();

			// Effective turret damage against minion (physical)
			float armorMod = 100.0f / (100.0f + minionArmor);
			float effectiveDmg = turretDmg * armorMod;

			return minionHP - (effectiveDmg * (float)turretShots);
		}

		// ====================================================================
		// Orbwalk Attack + Move cycle (full EnsoulSharp port)
		// Features: BlockOrders, OnAction events, ShouldWait, angle check,
		//           max distance, movement delay, NonKillableMinion, turret farm
		// ====================================================================
		void Orbwalk(SDK::GameObject& target) {
			auto& player = SDK::GameObjects::Player;
			if (!player.IsValid()) return;

			float now = SDK::Game::GetTime();
			// ---- AFTER ATTACK EVENT (fires when CanMove becomes true after attack) ----
			if (!m_afterAttackFired &&
				m_lastAttackCommandTime > 0.0f &&
				CanMove() &&
				m_lastTarget.IsValid()) {
				SDK::OrbwalkingActionArgs afterArgs;
				afterArgs.Target = m_lastTarget;
				afterArgs.Sender = player;
				afterArgs.Type = SDK::OrbwalkingType::AfterAttack;
				afterArgs.Process = true;
				SDK::Orbwalker::InvokeAction(afterArgs);
				// Fire dedicated AfterAttack callbacks (champion scripts like Ezreal subscribe here)
				SDK::Orbwalker::InvokeAfterAttack(afterArgs);
				m_afterAttackFired = true;
			}
			if (!m_attackReadyEventFired &&
				m_lastAttackCommandTime > 0.0f &&
				CanAttack()) {
				SDK::OrbwalkingActionArgs readyArgs;
				readyArgs.Target = m_lastTarget;
				readyArgs.Sender = player;
				readyArgs.Type = SDK::OrbwalkingType::OnAttackReady;
				readyArgs.Process = true;
				SDK::Orbwalker::InvokeAction(readyArgs);
				m_attackReadyEventFired = true;
			}
			if (!m_moveReadyEventFired && !m_blockMove && CanMove()) {
				auto* settings = m_menu ? m_menu->GetSubMenu("Settings") : nullptr;
				float moveDelay = 0.05f;
				if (settings) {
					if (auto* md = settings->Get<SDK::MenuUI::MenuSlider>("MoveDelay"))
						moveDelay = (float)md->Value / 1000.0f;
				}
				if (now >= m_lastMoveTime + moveDelay) {
					SDK::OrbwalkingActionArgs readyArgs;
					readyArgs.Position = player.GetPosition();
					readyArgs.Sender = player;
					readyArgs.Type = SDK::OrbwalkingType::OnMoveReady;
					readyArgs.Process = true;
					SDK::Orbwalker::InvokeAction(readyArgs);
					m_moveReadyEventFired = true;
				}
			}

			// ---- EVADE CHECK: Cancel attack if dodge is urgent ----
			{
				auto& evade = ::Evade::EvadeCore::Instance();
				if (evade.GetState() == ::Evade::EvadeState::Dodging) {
					// Evade is actively dodging — skip attack entirely
					// Do NOT return here; fall through to let move phase
					// be handled (but move phase also skips — evade handles movement)
					return; // Let evade control everything
				}
				if (evade.ShouldCancelAttack(now)) {
					// Urgent dodge needed — skip attack to dodge instead
					return;
				}
			}

			// ---- ATTACK PHASE ----
			if (CanAttack() && !m_blockAttack && target.IsValid() && player.IsInAttackRange(target)) {
				// Fire BeforeAttack event — scripts can cancel by setting Process = false
				SDK::OrbwalkingActionArgs beforeArgs;
				beforeArgs.Target = target;
				beforeArgs.Sender = player;
				beforeArgs.Type = SDK::OrbwalkingType::BeforeAttack;
				beforeArgs.Process = true;
				SDK::Orbwalker::InvokeAction(beforeArgs);
				// Also fire dedicated BeforeAttack callbacks (champion scripts subscribe here)
				if (beforeArgs.Process) {
					SDK::Orbwalker::InvokeBeforeAttack(beforeArgs);
				}

				if (beforeArgs.Process) {
					// TargetSwitch event
					if (m_lastTarget.IsValid() && m_lastTarget.GetNetId() != target.GetNetId()) {
						SDK::OrbwalkingActionArgs switchArgs;
						switchArgs.Target = target;
						switchArgs.Sender = player;
						switchArgs.Type = SDK::OrbwalkingType::TargetSwitch;
						switchArgs.Process = true;
						SDK::Orbwalker::InvokeAction(switchArgs);
					}

					Attack(target);

					// Fire OnAttack event
					SDK::OrbwalkingActionArgs onArgs;
					onArgs.Target = target;
					onArgs.Sender = player;
					onArgs.Type = SDK::OrbwalkingType::OnAttack;
					onArgs.Process = true;
					SDK::Orbwalker::InvokeAction(onArgs);

					m_lastTarget = target;
					return; // Attack issued — don't move this frame
				}
			}

			// ---- MOVE PHASE ----
			if (!CanMove()) return;
			if (m_blockMove) return;

			// LimitAttack: skip kiting if AS > 2.5
			bool limitAttack = false;
			auto* settings = m_menu ? m_menu->GetSubMenu("Settings") : nullptr;
			if (settings)
				if (auto* la = settings->Get<SDK::MenuUI::MenuBool>("LimitAttack"))
					limitAttack = la->Enabled;

			if (limitAttack) {
				float delay = player.GetAttackDelay();
				if (delay < 0.3846f && m_autoAttackCounter % 3 != 0)
					return; // Skip movement to maintain high AS kiting
			}

			// ComboNoMove mode: don't move, only attack
			if (IsComboNoMove()) return;

			// NonKillableMinion detection (in LastHit/LaneClear modes)
			if (m_activeMode == SDK::OrbwalkingMode::LastHit ||
				m_activeMode == SDK::OrbwalkingMode::LaneClear) {
				DetectNonKillableMinions();
			}

			// Movement delay from menu (in ms, convert to seconds)
			float moveDelay = 0.05f; // default 50ms
			bool moveRandom = false;
			float holdDist = 50.0f;
			int maxMoveDist = 0;
			bool highOrb = false;

			if (settings) {
				if (auto* md = settings->Get<SDK::MenuUI::MenuSlider>("MoveDelay"))
					moveDelay = (float)md->Value / 1000.0f;
				if (auto* rnd = settings->Get<SDK::MenuUI::MenuBool>("MoveRandom"))
					moveRandom = rnd->Enabled;
				if (auto* hold = settings->Get<SDK::MenuUI::MenuSlider>("ExtraHold"))
					holdDist = (float)hold->Value;
				if (auto* mmd = settings->Get<SDK::MenuUI::MenuSlider>("MaxMoveDistance"))
					maxMoveDist = mmd->Value;
				if (auto* ho = settings->Get<SDK::MenuUI::MenuBool>("HighOrb"))
					highOrb = ho->Enabled;
			}

			// High Frequency Walk: reduce delay
			if (highOrb) moveDelay = (std::min)(moveDelay, 0.02f);

			// Movement delay throttle
			if (now < m_lastMoveTime + moveDelay) return;

			Vec3 mousePos = SDK::Game::GetMouseWorldPos();
			Vec3 playerPos = player.GetPosition();

			// Hold position check
			float distToCursor = playerPos.Distance2D(mousePos);
			if (distToCursor < (std::max)(30.0f, holdDist + player.GetBoundingRadius()))
				return;

			// Calculate final move position
			Vec3 finalPos = mousePos;

			// Max move distance: limit the distance we move towards cursor
			if (maxMoveDist > 0 && distToCursor > (float)maxMoveDist) {
				Vec3 dir = Vec3(mousePos.x - playerPos.x, 0, mousePos.z - playerPos.z);
				float dirLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
				if (dirLen > 1.0f) {
					dir = Vec3(dir.x / dirLen, 0, dir.z / dirLen);
					finalPos = Vec3(playerPos.x + dir.x * (float)maxMoveDist,
						mousePos.y,
						playerPos.z + dir.z * (float)maxMoveDist);
				}
			}

			// Randomize movement: slight random offset to make pattern less predictable
			if (moveRandom && distToCursor > 100.0f) {
				Vec3 moveDir = Vec3(finalPos.x - playerPos.x, 0, finalPos.z - playerPos.z);
				float moveDirLen = std::sqrt(moveDir.x * moveDir.x + moveDir.z * moveDir.z);
				if (moveDirLen > 1.0f) {
					float rndFactor = 0.6f + (float)(rand() % 40) / 100.0f;
					float rndDist = rndFactor * 400.0f;
					Vec3 dir = Vec3(moveDir.x / moveDirLen, 0, moveDir.z / moveDirLen);
					finalPos = Vec3(playerPos.x + dir.x * rndDist,
						mousePos.y,
						playerPos.z + dir.z * rndDist);
				}
			}

			// Angle check: don't issue movement if new direction is very close to current direction
			// This prevents micro-jittering from too many redundant move commands
			Vec3 newDir = Vec3(finalPos.x - playerPos.x, 0, finalPos.z - playerPos.z);
			float newDirLen = std::sqrt(newDir.x * newDir.x + newDir.z * newDir.z);
			if (newDirLen > 1.0f && m_lastMoveDir.x != 0.0f) {
				newDir = Vec3(newDir.x / newDirLen, 0, newDir.z / newDirLen);
				float dot = newDir.x * m_lastMoveDir.x + newDir.z * m_lastMoveDir.z;
				// If angle < ~5 degrees and we moved recently, skip
				if (dot > 0.996f && (now - m_lastMoveTime) < 0.15f) {
					return;
				}
			}

			// Fire Movement event
			SDK::OrbwalkingActionArgs moveArgs;
			moveArgs.Position = finalPos;
			moveArgs.Sender = player;
			moveArgs.Type = SDK::OrbwalkingType::Movement;
			moveArgs.Process = true;
			SDK::Orbwalker::InvokeAction(moveArgs);

			if (!moveArgs.Process) return; // Script cancelled movement

			// Use possibly modified position from event handler
			finalPos = moveArgs.Position;

			// ---- EVADE CHECK: Block movement into skillshots ----
			{
				auto& evade = ::Evade::EvadeCore::Instance();
				Vec2 movePos2D(finalPos.x, finalPos.z);
				if (evade.ShouldBlockMovement(movePos2D, now)) {
					// Save the blocked command so evade can resume it later
					evade.SaveBlockedCommand(movePos2D);
					return; // Don't walk into skillshot
				}
			}

			MoveTo(finalPos);
		}

		void Attack(SDK::GameObject& target) {
			SDK::Orbwalker::IssueOrder(3, target.GetPosition(), &target);
			float now = SDK::Game::GetTime();
			m_lastAttackCommandTime = now;
			SDK::Orbwalker::LastAttackTime = now;
			SDK::Orbwalker::MissileLaunched = false;
			m_attackReadyEventFired = false;
			m_afterAttackFired = false;
			m_autoAttackCounter++;
			SDK::Orbwalker::AutoAttackCounter++;
		}

		void MoveTo(Vec3 pos) {
			// Save direction for angle check
			Vec3 playerPos = SDK::GameObjects::Player.GetPosition();
			Vec3 dir = Vec3(pos.x - playerPos.x, 0, pos.z - playerPos.z);
			float dirLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
			if (dirLen > 1.0f)
				m_lastMoveDir = Vec3(dir.x / dirLen, 0, dir.z / dirLen);

			SDK::Orbwalker::IssueOrder(2, pos);
			m_lastMoveTime = SDK::Game::GetTime();
			m_moveReadyEventFired = false;
		}

		// ====================================================================
		// NonKillableMinion detection (EnsoulSharp OrbwalkerSelector)
		// Fires OnAction with NonKillableMinion type for each minion that
		// will die before our AA reaches it
		// ====================================================================
		void DetectNonKillableMinions() {
			auto& player = SDK::GameObjects::Player;
			bool calcItems = true;
			if (auto* settings = m_menu ? m_menu->GetSubMenu("Settings") : nullptr)
				if (auto* ci = settings->Get<SDK::MenuUI::MenuBool>("CalcItemDamage"))
					calcItems = ci->Enabled;

			for (auto& minion : SDK::GameObjects::EnemyMinions) {
				if (!minion.IsAlive() || !minion.IsVisible()) continue;
				if (!player.IsInAttackRange(minion)) continue;

				float dmg = SDK::DamageCalc::GetAutoAttackDamage(player, minion, false, calcItems);
				float impactMs = GetAttackImpactTimeMs(minion);
				float predictedHP = SDK::HealthPrediction::GetPrediction(minion, impactMs);

				// Minion will die before our AA reaches it (unkillable)
				if (predictedHP <= 0.0f) {
					// Check if it was killable now but won't be when AA lands
					float currentHP = minion.GetHealth();
					if (currentHP > 0.0f && currentHP <= dmg) {
						SDK::OrbwalkingActionArgs nkArgs;
						nkArgs.Target = minion;
						nkArgs.Sender = player;
						nkArgs.Type = SDK::OrbwalkingType::NonKillableMinion;
						nkArgs.Process = true;
						SDK::Orbwalker::InvokeAction(nkArgs);
					}
				}
			}
		}

		// ====================================================================
		// GetLaneClearMinion advanced returns a minion to attack in LaneClear mode
		// Uses ShouldWait logic: if a minion will be last-hittable soon, wait
		// ====================================================================
		SDK::GameObject GetLaneClearMinion(float range) {
			LaneFarmAnalysis analysis = AnalyzeLaneFarm(range);
			if (analysis.shouldWait) return SDK::GameObject();
			return analysis.pushTarget;
		}

	};

} // namespace Plugins

