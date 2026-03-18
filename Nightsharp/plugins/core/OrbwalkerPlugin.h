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
#include "sdk/Utils/CursorUtils.h"
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
#include "sdk/Utils/DebugConsole.h"

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
			orbSettings->Add<SDK::MenuUI::MenuSlider>("WindupDelay", "Extra Windup Delay (ms)", 60, 0, 250);
			orbSettings->Add<SDK::MenuUI::MenuBool>("LimitAttack", "Don't Kite if AS > 2.5", false);
			orbSettings->Add<SDK::MenuUI::MenuBool>("MissileCheck", "Use Missile Checks", true);
			orbSettings->Add<SDK::MenuUI::MenuBool>("CalcItemDamage", "Calculate Item Damage", false);
			orbSettings->Add<SDK::MenuUI::MenuBool>("YasuoWallCheck", "Yasuo WindWall Check", true);
			orbSettings->Add<SDK::MenuUI::MenuBool>("HighOrb", "High Frequency Walk", false);
			orbSettings->Add<SDK::MenuUI::MenuBool>("CalculateRunaway", "Calculate Runaway Distance", true);
			orbSettings->Add<SDK::MenuUI::MenuBool>("SupportMode", "Support Mode", false);

			// Farm sub-menu
			auto farm = m_menu->AddSubMenu("Farm", "Farm");
			farm->Add<SDK::MenuUI::MenuSlider>("FarmDelay", "Farm Delay", 30, 0, 200);
			farm->Add<SDK::MenuUI::MenuSlider>("FastFarmDelay", "Fast Farm Delay", 220, 0, 1000);
			farm->Add<SDK::MenuUI::MenuList>("TurretFarm", "Turret Farm",
				std::vector<std::string>{"Enabled", "Off"}, 0);
			farm->Add<SDK::MenuUI::MenuSlider>("TurretFarmMaxLevel", "Turret Farm Max Level", 18, 1, 18);
			farm->Add<SDK::MenuUI::MenuBool>("ShouldWait", "Wait for Last Hit", true);


			// Drawing sub-menu
			auto draw = m_menu->AddSubMenu("Drawing", "Drawing");
			draw->Add<SDK::MenuUI::MenuBool>("DrawAttackRange", "Draw Attack Range", true);
			draw->Add<SDK::MenuUI::MenuBool>("DrawHoldPosition", "Draw Hold Position", false);
			draw->Add<SDK::MenuUI::MenuBool>("DrawKillableMinion", "Draw Killable Minion", false);
			draw->Add<SDK::MenuUI::MenuBool>("ShowFakeClick", "Show Fake Click", true);
			draw->Add<SDK::MenuUI::MenuBool>("FakeCursor", "Fake Cursor (Livestream)", false);

			// Keybinds
			m_menu->Add<SDK::MenuUI::MenuSeparator>("sep_keys", "--- Keybinds ---");
			m_menu->Add<SDK::MenuUI::MenuKeyBind>("Combo", "Combo", VK_SPACE, SDK::MenuUI::KeyBindType::Press);
			m_menu->Add<SDK::MenuUI::MenuKeyBind>("Harass", "Harass", 'C', SDK::MenuUI::KeyBindType::Press);
			m_menu->Add<SDK::MenuUI::MenuKeyBind>("LaneClear", "LaneClear", 'V', SDK::MenuUI::KeyBindType::Press);
			m_menu->Add<SDK::MenuUI::MenuKeyBind>("LastHit", "LastHit", 'X', SDK::MenuUI::KeyBindType::Press);
			m_menu->Add<SDK::MenuUI::MenuKeyBind>("Flee", "Flee", 'Z', SDK::MenuUI::KeyBindType::Press);
			m_menu->Add<SDK::MenuUI::MenuKeyBind>("FastLaneClear", "Fast LaneClear", 'A', SDK::MenuUI::KeyBindType::Press);
			m_menu->Add<SDK::MenuUI::MenuKeyBind>("ComboNoMove", "Combo (No Move)", 0, SDK::MenuUI::KeyBindType::Press);

			// Cache champion flags for performance (NewOrbwalker.cs constructor)
			std::string myChamp = SDK::GameObjects::Player.GetChampionName();
			m_isAphelios = (_stricmp(myChamp.c_str(), "Aphelios") == 0);
			m_isGraves = (_stricmp(myChamp.c_str(), "Graves") == 0);
			m_isJhin = (_stricmp(myChamp.c_str(), "Jhin") == 0);
			m_isKalista = (_stricmp(myChamp.c_str(), "Kalista") == 0);
			m_isRengar = (_stricmp(myChamp.c_str(), "Rengar") == 0);
			m_isSett = (_stricmp(myChamp.c_str(), "Sett") == 0);

			// Check enemy team for special interactions
			for (auto& hero : SDK::GameObjects::EnemyHeroes) {
				if (!hero.IsValid()) continue;
				std::string name = hero.GetChampionName();
				if (_stricmp(name.c_str(), "Jax") == 0) m_jaxInGame = true;
				if (_stricmp(name.c_str(), "Gangplank") == 0) m_gangplankInGame = true;
			}
			// Check all heroes for TahmKench
			for (auto& hero : SDK::GameObjects::AllHeroes) {
				if (!hero.IsValid()) continue;
				std::string name = hero.GetChampionName();
				if (_stricmp(name.c_str(), "TahmKench") == 0 && hero.GetNetId() != SDK::GameObjects::Player.GetNetId()) {
					m_tahmKenchInGame = true;
				}
			}
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

			// === DEBUG ===
			static int s_plugDbg = 0;
			int nowT = SDK::Game::GetTickCount();
			if (nowT - s_plugDbg > 1000 && m_activeMode != SDK::OrbwalkingMode::None) {
				s_plugDbg = nowT;
				const char* mn[] = {"None","Combo","LaneClear","Hybrid","LastHit","Flee"};
				int mi = (int)m_activeMode; if(mi<0||mi>5) mi=0;
				DEBUG_LOG_TAG("PLUG", "OnUpdate: mode=%s canAtk=%d canMv=%d", mn[mi], CanAttack(), CanMove());
			}

			if (m_activeMode == SDK::OrbwalkingMode::None) return;

			SDK::GameObject target = GetTarget();

			if (nowT - s_plugDbg < 50 && target.IsValid()) {
				DEBUG_LOG_TAG("PLUG", "  target=0x%llX hp=%.0f inRange=%d",
					(unsigned long long)target.address, target.GetHealth(), player.IsInAttackRange(target));
			}

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
				// PaleVioletRed matching C# NewOrbwalker.cs
				SDK::Drawing::DrawCircle(player.GetPosition(),
					player.GetRealAttackRange(), IM_COL32(219, 112, 147, 180), 1.5f);
			}

			// Draw hold position
			auto* drawHold = drawMenu->Get<SDK::MenuUI::MenuBool>("DrawHoldPosition");
			if (drawHold && drawHold->Enabled) {
				auto* settings = m_menu->GetSubMenu("Settings");
				int holdDist = settings ? settings->Get<SDK::MenuUI::MenuSlider>("ExtraHold")->Value : 50;
				SDK::Drawing::DrawCircle(player.GetPosition(),
					player.GetBoundingRadius() + (float)holdDist, IM_COL32(128, 0, 200, 100), 1.0f);
			}

			// Draw killable minion — uses HealthPrediction for accuracy
			auto* drawKill = drawMenu->Get<SDK::MenuUI::MenuBool>("DrawKillableMinion");
			if (drawKill && drawKill->Enabled) {
				bool calcItems = IsItemDamageCalculationEnabled();
				for (auto& minion : SDK::GameObjects::EnemyMinions) {
					if (!minion.IsAlive() || !minion.IsVisible()) continue;
					float dist = player.DistanceTo(minion);
					if (dist > player.GetRealAttackRange() * 2.0f) continue;

					float dmg = SDK::DamageCalc::GetAutoAttackDamage(player, minion, false, calcItems);
					if (minion.GetHealth() > 0.0f && minion.GetHealth() < dmg) {
						// Green circle matching C# (0, 255, 0, 255)
						SDK::Drawing::DrawCircle(minion.GetPosition(),
							minion.GetBoundingRadius() * 2.0f, IM_COL32(0, 255, 0, 200), 2.0f);
					}
				}
			}

			// Native click indicators (green move / red attack) are triggered at the
			// actual IssueOrder call sites (Move → ShowMoveClick, Attack → ShowAttackClick)
			// using game VFX particles, NOT overlay drawing. No overlay fallback needed.

			// Fake Cursor for livestream protection
			auto* fakeCursorMenu = drawMenu->Get<SDK::MenuUI::MenuBool>("FakeCursor");
			if (fakeCursorMenu) {
				SDK::FakeCursor::SetEnabled(fakeCursorMenu->Enabled);
				if (fakeCursorMenu->Enabled) {
					SDK::FakeCursor::OnRender();
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
		// Timing — TickCount-based matching NewOrbwalker.cs exactly
		// ====================================================================

		bool CanAttack(float extraWindup = 0.0f) const {
			auto& player = SDK::GameObjects::Player;
			if (!player.IsValid()) return false;

			// Pause ticks
			int tickNow = SDK::Game::GetTickCount();
			if (m_allPauseTick > 0 && m_allPauseTick - tickNow > 0) return false;
			if (m_attackPauseTick > 0 && m_attackPauseTick - tickNow > 0) return false;

			// CC checks
			SDK::BuffManager buffs(player.address);
			if (m_tahmKenchInGame && buffs.HasBuff("tahmkenchwhasdevouredtarget")) return false;
			if (buffs.HasBuffOfType(SDK::BuffType::Fear)) return false;
			if (buffs.HasBuffOfType(SDK::BuffType::Polymorph)) return false;

			// Blindingdart (non-Kalista)
			if (!m_isKalista && buffs.HasBuff("blindingdart")) return false;

			// Rengar Q bypass
			if (m_isRengar && (buffs.HasBuff("RengarQ") || buffs.HasBuff("RengarQEmp"))) return true;

			// Aphelios preload
			if (m_isAphelios && buffs.HasBuff("apheliospreload")) return false;

			// Jhin reload
			if (m_isJhin && buffs.HasBuff("JhinPassiveReload")) return false;

			// Calculate attack delay in ms
			float num = player.GetAttackDelay() * 1000.0f;

			// Graves special formula
			if (m_isGraves) {
				if (!buffs.HasBuff("gravesbasicattackammo1")) return false;
				num = player.GetAttackDelay() * 1000.0f * 1.0740297f - 716.2381f;
			}
			// Sett passive — faster second punch
			else if (m_isSett && m_nextAttackIsPassive) {
				num = player.GetAttackDelay() * 1000.0f / 8.0f;
			}

			// Core: TickCount + Ping/2 + 25 >= LastAutoAttackTick + delay + extraWindup
			return (float)(tickNow + (int)(SDK::Game::GetPing() / 2.0f) + 25) >=
				(float)m_lastAutoAttackTick + num + extraWindup;
		}

		bool CanMove(float extraWindup = 0.0f, bool disableMissileCheck = false) const {
			auto& player = SDK::GameObjects::Player;
			if (!player.IsValid()) return false;

			// Pause ticks
			int tickNow = SDK::Game::GetTickCount();
			if (m_allPauseTick > 0 && m_allPauseTick - tickNow > 0) return false;
			if (m_movePauseTick > 0 && m_movePauseTick - tickNow > 0) return false;

			// TahmKench devour
			if (m_tahmKenchInGame) {
				SDK::BuffManager buffs(player.address);
				if (buffs.HasBuff("tahmkenchwhasdevouredtarget")) return false;
			}

			// Kalista: can always move
			if (m_isKalista) return true;

			// Missile launched — allow movement immediately
			if (SDK::Orbwalker::MissileLaunched && !disableMissileCheck) {
				auto* settings = m_menu ? m_menu->GetSubMenu("Settings") : nullptr;
				bool missileCheck = settings ? settings->Get<SDK::MenuUI::MenuBool>("MissileCheck")->Enabled : true;
				if (missileCheck) return true;
			}

			// Rengar Q/QEmp: extra 200ms windup
			int rengarExtra = 0;
			if (m_isRengar) {
				SDK::BuffManager buffs(player.address);
				if (buffs.HasBuff("RengarQ") || buffs.HasBuff("RengarQEmp"))
					rengarExtra = 200;
			}

			// GetAttackCastDelay — Sett passive reduces windup
			float castDelay = player.GetAttackWindup();
			if (m_isSett && m_nextAttackIsPassive) {
				castDelay = castDelay - castDelay / 8.0f;
			}

			// Core: TickCount + Ping/2 >= LastAutoAttackTick + CastDelay*1000 + extra
			return (float)(tickNow + (int)(SDK::Game::GetPing() / 2.0f)) >=
				(float)m_lastAutoAttackTick + castDelay * 1000.0f + extraWindup + (float)rengarExtra;
		}

		void ResetAutoAttackTimer() {
			m_allPauseTick = 0;
			m_attackPauseTick = 0;
			m_lastAutoAttackTick = 0;
			m_movePauseTick = 0;
			m_lastAttackCommandTime = 0.0f;
			SDK::Orbwalker::LastAttackTime = 0.0f;
			SDK::Orbwalker::LastAutoAttackTick = 0;
			SDK::Orbwalker::MissileLaunched = false;
			m_attackReadyEventFired = true;
			m_moveReadyEventFired = true;
		}

		SDK::OrbwalkingMode ActiveMode() const { return m_activeMode; }

		// ShouldWait — matching NewOrbwalker.cs
		bool ShouldWait(float range) {
			return AnalyzeLaneFarm(range).shouldWait;
		}

		// Pause time API
		void SetPauseTime(int ms) { m_allPauseTick = SDK::Game::GetTickCount() + ms; }
		void SetAttackPauseTime(int ms) { m_attackPauseTick = SDK::Game::GetTickCount() + ms; }
		void SetMovePauseTime(int ms) { m_movePauseTick = SDK::Game::GetTickCount() + ms; }

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
		int m_lastAutoAttackTick = 0;           // TickCount when attack confirmed (OnDoCast)
		int m_lastLocalAttackTick = 0;          // TickCount when attack command issued
		int m_lastMovementTick = 0;             // TickCount for move throttling
		float m_lastMoveTime = 0.0f;            // Game time (seconds) for move throttling
		int m_autoAttackCounter = 0;
		int m_allPauseTick = 0;
		int m_attackPauseTick = 0;
		int m_movePauseTick = 0;
		bool m_attackReadyEventFired = true;
		bool m_afterAttackFired = true;
		bool m_moveReadyEventFired = true;
	
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
		int m_lastFakeClickTick = 0; // For ShowFakeClick rate limiting

		// Champion flags — cached once at load for performance
		bool m_isAphelios = false;
		bool m_isGraves = false;
		bool m_isJhin = false;
		bool m_isKalista = false;
		bool m_isRengar = false;
		bool m_isSett = false;
		bool m_nextAttackIsPassive = false; // Sett passive tracking
		bool m_jaxInGame = false;
		bool m_gangplankInGame = false;
		bool m_tahmKenchInGame = false;

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
		// CanAttackWithWindWall -- checks if attack is blocked by Yasuo/Samira wall
		// Matches NewOrbwalker.cs CanAttackWithWindWall()
		// ====================================================================
		bool CanAttackWithWindWall(const SDK::GameObject& target) const {
			if (!m_menu) return true;
			auto* settings = m_menu->GetSubMenu("Settings");
			if (!settings) return true;
			auto* ywc = settings->Get<SDK::MenuUI::MenuBool>("YasuoWallCheck");
			if (!ywc || !ywc->Enabled) return true;

			auto& player = SDK::GameObjects::Player;
			if (!player.IsValid() || player.IsMelee()) return true; // Melee ignores windwall

			// Check all enemy minions for windwall objects
			for (auto& minion : SDK::GameObjects::EnemyMinions) {
				if (!minion.IsValid() || !minion.IsAlive()) continue;
				std::string name = minion.GetChampionName();
				// Yasuo windwall or Samira blade whirl
				if (_stricmp(name.c_str(), "yourcut") == 0 ||
					_stricmp(name.c_str(), "yourcut2") == 0 ||
					name.find("windwall") != std::string::npos) {
					// Simple check: if windwall is between player and target
					Vec3 wallPos = minion.GetPosition();
					Vec3 playerPos = player.GetPosition();
					Vec3 targetPos = target.GetPosition();

					// Line-circle intersection: is the wall roughly between us and target?
					float wallRadius = 350.0f; // approx windwall width
					Vec3 dir = targetPos - playerPos;
					Vec3 toWall = wallPos - playerPos;
					float dirLen = sqrtf(dir.x * dir.x + dir.z * dir.z);
					if (dirLen < 1.0f) continue;
					float proj = (toWall.x * dir.x + toWall.z * dir.z) / dirLen;
					if (proj < 0.0f || proj > dirLen) continue; // Wall not between

					// Perpendicular distance from wall center to line
					float perpDist = fabsf(toWall.x * dir.z - toWall.z * dir.x) / dirLen;
					if (perpDist < wallRadius) {
						return false; // Attack would be blocked
					}
				}
			}
			return true;
		}

		// ====================================================================
		// CanOrbObj -- checks if target will still be in range when AA lands
		// Matches NewOrbwalker.cs CanOrbObj() -- uses CalculateRunaway setting
		// ====================================================================
		bool CanOrbObj(const SDK::GameObject& target) const {
			if (!target.IsValid() || !target.IsAlive()) return false;

			auto* settings = m_menu ? m_menu->GetSubMenu("Settings") : nullptr;
			if (!settings) return true;
			auto* calcRunaway = settings->Get<SDK::MenuUI::MenuBool>("CalculateRunaway");
			if (!calcRunaway || !calcRunaway->Enabled) return true;

			auto& player = SDK::GameObjects::Player;
			if (!player.IsValid()) return true;

			// Check if target is moving away and will be out of range when AA lands
			float range = player.GetRealAttackRange();
			float dist = player.DistanceTo(target);
			if (dist > range + 50.0f) return false; // Already out of range

			// Get target movement speed
			float targetMS = target.GetMoveSpeed();
			if (targetMS <= 0.0f) return true; // Not moving

			// Calculate time for AA to reach target
			float windupTime = player.GetAttackWindup(); // seconds
			float projSpeed = GetProjectileSpeed(player);
			float projTime = (projSpeed > 0.0f && projSpeed < FLT_MAX)
				? (std::max)(0.0f, dist / projSpeed) : 0.0f;
			float totalTime = windupTime + projTime;

			// Estimate how far target can move in that time
			float runawayDist = targetMS * totalTime;
			float futureDistWorstCase = dist + runawayDist;

			// If target will be out of range even in worst case, skip it
			// Add 100 buffer to avoid edge cases
			if (futureDistWorstCase > range + 100.0f) return false;

			return true;
		}

		// ====================================================================
		// Target selection (ported from NewOrbwalker.GetTarget)
		// Enhanced: 1.4 Jax check, 1.14 SpecialMinion, WindWall, Runaway
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



			// ---- GP Barrel priority check (NewOrbwalker.cs line 1068-1103) ----
			// Works in ALL modes including Combo
			bool attackBarrels = true;
			auto* attackable = m_menu->GetSubMenu("Attackable");
			if (attackable)
				if (auto* b = attackable->Get<SDK::MenuUI::MenuBool>("Barrels"))
					attackBarrels = b->Enabled;

			if (attackBarrels && m_gangplankInGame) {
				// Search for GP barrels in jungle objects (barrels are classified as jungle)
				for (auto& barrel : SDK::GameObjects::JungleMinions) {
					if (!barrel.IsValid() || !barrel.IsAlive()) continue;
					std::string charName = barrel.GetChampionName();
					if (_stricmp(charName.c_str(), "gangplankbarrel") != 0) continue;
					if (!player.IsInAttackRange(barrel)) continue;
					if (barrel.GetHealth() <= 0.0f) continue;

					// HP <= 1: always attackable (last tick)
					if (barrel.GetHealth() <= 1.0f) {
						return finalizeTarget(barrel);
					}

					// HP == 2 and has gangplankebarrelactive buff: check decay timing
					if (barrel.GetHealth() <= 2.0f) {
						auto buff = barrel.GetBuff("gangplankebarrelactive");
						if (buff.IsActive()) {
							// Get owner (GP) level to determine decay rate
							// Level >= 13: 0.5s, Level >= 7: 1.0s, else 2.0s
							int ownerLevel = 1;
							for (auto& hero : SDK::GameObjects::AllHeroes) {
								if (!hero.IsValid()) continue;
								if (_stricmp(hero.GetChampionName().c_str(), "Gangplank") == 0 && hero.GetLevel() > 0) {
									ownerLevel = hero.GetLevel();
									break;
								}
							}

							double decayRate = (ownerLevel >= 13) ? 0.5 :
								((ownerLevel >= 7) ? 1.0 : 2.0);

							// Calculate when barrel will decay to 1 HP
							double buffStart = (double)buff.GetStartTime();
							double nextDecayTime = buffStart + decayRate * 2.0;
							if (buffStart + decayRate > (double)SDK::Game::GetTime()) {
								nextDecayTime = buffStart + decayRate;
							}

							// Calculate time for our AA to reach the barrel
							float projSpeed = GetProjectileSpeed(player);
							float dist = player.DistanceTo(barrel) - player.GetBoundingRadius();
							float projTimeMs = (projSpeed > 0.0f && projSpeed < FLT_MAX)
								? 1000.0f * (std::max)(0.0f, dist / projSpeed) : 1.0f;
							float impactTimeMs = player.GetAttackWindup() * 1000.0f +
								SDK::Game::GetPing() / 2.0f + projTimeMs;

							// If barrel will decay before our AA lands, attack it
							if (nextDecayTime < (double)(SDK::Game::GetTime() + impactTimeMs / 1000.0f)) {
								return finalizeTarget(barrel);
							}
						}
					}
				}
			}

			// Priority 0: Special minions (wards, plants, pets) [1.14]
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

			// Priority 2: Last-hittable minion (not in combo, not support mode)
			if (m_activeMode != SDK::OrbwalkingMode::Combo && !IsSupportMode()) {
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
					if (!HasJaxCounterStrike(forced) && CanAttackWithWindWall(forced) && CanOrbObj(forced))
						return forced;
				}
				// "Only Attack Selected Target"
				if (SDK::TargetSelector::OnlyAttackSelected)
					return SDK::GameObject();
			}

			// "Only Attack Selected Target" with a valid forced target that's
			// dead/invisible -> still don't attack other targets
			if (SDK::TargetSelector::OnlyAttackSelected &&
				SDK::TargetSelector::ForcedTarget.IsValid())
				return SDK::GameObject();

			// 2. Use SDK::TargetSelector for smart target selection
			auto target = SDK::TargetSelector::GetTarget(range);

			// Filter: Jax CounterStrike, WindWall, Runaway distance
			if (target.IsValid()) {
				if (HasJaxCounterStrike(target) || !CanAttackWithWindWall(target) || !CanOrbObj(target)) {
					auto targets = SDK::TargetSelector::GetTargets(range);
					for (auto& t : targets) {
						if (!HasJaxCounterStrike(t) && CanAttackWithWindWall(t) && CanOrbObj(t))
							return t;
					}
					return SDK::GameObject(); // All targets filtered out
				}
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
			// Use missile's actual speed from RuntimeAPI instead of hardcoded values
			float speed = missile.GetMissileSpeed();
			if (speed <= 0.0f || speed >= FLT_MAX) {
				// Fallback for missiles without speed data
				speed = missile.IsTurretShot() ? 1200.0f : 1800.0f;
			}
			float dist = missile.GetPosition().Distance2D(missile.GetEndPos());
			if (dist <= 0.0f) return 0.0f;
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
				float impactHp = SDK::HealthPrediction::GetPrediction(minion, impactTimeMs, (int)farmDelayMs);
				float nextHp = SDK::HealthPrediction::GetPrediction(minion, nextImpactTimeMs, (int)farmDelayMs);

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

		// IsSupportMode — matching NewOrbwalker.cs (line 566-595)
		bool IsSupportMode() const {
			auto* settings = m_menu ? m_menu->GetSubMenu("Settings") : nullptr;
			if (!settings) return false;
			auto* sm = settings->Get<SDK::MenuUI::MenuBool>("SupportMode");
			if (!sm || !sm->Enabled) return false;

			auto& player = SDK::GameObjects::Player;
			float aaRange = player.GetRealAttackRange();
			float checkRange = (std::max)(1200.0f, aaRange * 2.0f);

			// Check if any ally hero is nearby
			for (auto& ally : SDK::GameObjects::AllyHeroes) {
				if (!ally.IsValid() || !ally.IsAlive()) continue;
				if (ally.GetNetId() == player.GetNetId()) continue;
				if (ally.DistanceTo(player) <= checkRange) return true;
			}
			return false;
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
				entry.impactHp = SDK::HealthPrediction::GetPrediction(minion, impactTimeMs, (int)farmDelayMs);
				entry.nextImpactHp = SDK::HealthPrediction::GetPrediction(minion, nextImpactTimeMs, (int)farmDelayMs);

				if (shouldWaitEnabled) {
					// NewOrbwalker.cs ShouldWait (line 528-530):
					// float num = !FastLne ? AttackDelay*1000*2 : AttackDelay*1000 + FastFarmDelay
					// prediction = GetPrediction(m, (int)num, farmDelay, Simulated)
					// if (prediction < damage) return true
					float shouldWaitTimeMs = IsFastLaneClear()
						? (player.GetAttackDelay() * 1000.0f + GetFastFarmDelayMs())
						: (player.GetAttackDelay() * 1000.0f * 2.0f);
					float shouldWaitPred = SDK::HealthPrediction::GetPrediction(
						minion, shouldWaitTimeMs, (int)farmDelayMs);
					if (shouldWaitPred > 0.0f && shouldWaitPred < entry.damage) {
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

			int tickNow = SDK::Game::GetTickCount();

			// Sett passive timeout (2s) — matching NewOrbwalker.cs
			if (m_isSett && m_nextAttackIsPassive && m_lastAutoAttackTick > 0 &&
				tickNow - m_lastAutoAttackTick > 2000) {
				m_nextAttackIsPassive = false;
			}

			float now = SDK::Game::GetTime();
			// ---- AFTER ATTACK EVENT ----
			if (!m_afterAttackFired &&
				m_lastAutoAttackTick > 0 &&
				CanMove() &&
				m_lastTarget.IsValid()) {
				SDK::OrbwalkingActionArgs afterArgs;
				afterArgs.Target = m_lastTarget;
				afterArgs.Sender = player;
				afterArgs.Type = SDK::OrbwalkingType::AfterAttack;
				afterArgs.Process = true;
				SDK::Orbwalker::InvokeAction(afterArgs);
				SDK::Orbwalker::InvokeAfterAttack(afterArgs);
				m_afterAttackFired = true;
			}
			if (!m_attackReadyEventFired &&
				m_lastAutoAttackTick > 0 &&
				CanAttack()) {
				SDK::OrbwalkingActionArgs readyArgs;
				readyArgs.Target = m_lastTarget;
				readyArgs.Sender = player;
				readyArgs.Type = SDK::OrbwalkingType::OnAttackReady;
				readyArgs.Process = true;
				SDK::Orbwalker::InvokeAction(readyArgs);
				m_attackReadyEventFired = true;
			}

			// ---- EVADE CHECK ----
			{
				auto& evade = ::Evade::EvadeCore::Instance();
				if (evade.GetState() == ::Evade::EvadeState::Dodging) return;
				if (evade.ShouldCancelAttack(now)) return;
			}

			// ---- BlockOrders check (NewOrbwalker.cs line 1420) ----
			if (tickNow - m_lastLocalAttackTick < 70 + (std::min)(60, (int)SDK::Game::GetPing()))
				return;

			// ---- ATTACK PHASE ----
			if (!m_blockAttack && CanAttack() && target.IsValid() && player.IsInAttackRange(target)) {
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
			// Get WindupDelay from menu and pass to CanMove (NewOrbwalker.cs line 1428)
			auto* settings = m_menu ? m_menu->GetSubMenu("Settings") : nullptr;
			float windupDelayMs = settings ? (float)(settings->Get<SDK::MenuUI::MenuSlider>("WindupDelay")->Value) : 60.0f;
			if (!CanMove(windupDelayMs)) return;
			if (m_blockMove) return;

			// LimitAttack: skip kiting if AS > 2.5 (NewOrbwalker.cs line 1434)
			bool limitAttack = false;
			if (settings)
				if (auto* la = settings->Get<SDK::MenuUI::MenuBool>("LimitAttack"))
					limitAttack = la->Enabled;

			if (limitAttack) {
				float delay = player.GetAttackDelay();
				if (delay < 0.3846154f && m_autoAttackCounter % 3 != 0 && !CanMove(500.0f, true))
					return; // Skip movement to maintain high AS kiting
			}

			// ComboNoMove mode: don't move, only attack
			if (IsComboNoMove()) return;

			// NonKillableMinion detection (in LastHit/LaneClear modes)
			if (m_activeMode == SDK::OrbwalkingMode::LastHit ||
				m_activeMode == SDK::OrbwalkingMode::LaneClear) {
				DetectNonKillableMinions();
			}

			// Read menu settings
			bool moveRandom = false;
			float holdDist = 50.0f;
			bool highOrb = false;

			if (settings) {
				if (auto* rnd = settings->Get<SDK::MenuUI::MenuBool>("MoveRandom"))
					moveRandom = rnd->Enabled;
				if (auto* hold = settings->Get<SDK::MenuUI::MenuSlider>("ExtraHold"))
					holdDist = (float)hold->Value;
				if (auto* ho = settings->Get<SDK::MenuUI::MenuBool>("HighOrb"))
					highOrb = ho->Enabled;
			}

			Vec3 mousePos = SDK::Game::GetMouseWorldPos();
			Vec3 playerPos = player.GetPosition();
			Vec3 finalPos = mousePos;

			// ==============================================================
			// Hold position check (NewOrbwalker.cs line 1350-1358)
			// ==============================================================
			float holdThreshold = (std::max)(30.0f, holdDist);
			float distSqToCursor = (playerPos.x - mousePos.x) * (playerPos.x - mousePos.x) +
				(playerPos.z - mousePos.z) * (playerPos.z - mousePos.z);
			if (distSqToCursor < holdThreshold * holdThreshold) {
				// Stop if we have a path
				// TODO: Issue stop order if player.HasPath()
				return;
			}

			// ==============================================================
			// MoveRandom check (NewOrbwalker.cs line 1359-1362)
			// Only randomize when very close to cursor (< 150 units)
			// ==============================================================
			if (moveRandom && distSqToCursor < 150.0f * 150.0f) {
				float distToCursor = std::sqrt(distSqToCursor);
				float rndFactor = 0.6f + (float)(rand() % 40) / 100.0f + 0.2f;
				float extendDist = rndFactor * 400.0f;
				if (distToCursor > 1.0f) {
					Vec3 dir = Vec3((mousePos.x - playerPos.x) / distToCursor, 0,
						(mousePos.z - playerPos.z) / distToCursor);
					finalPos = Vec3(playerPos.x + dir.x * extendDist,
						mousePos.y,
						playerPos.z + dir.z * extendDist);
				}
			}

			int tickNow2 = SDK::Game::GetTickCount();
			int ping = (int)SDK::Game::GetPing();

			// ==============================================================
			// Path angle + rate limit (NewOrbwalker.cs line 1363-1398)
			// ==============================================================
			if (!highOrb) {
				// Compute angle between current path and new path
				float angle = 0.0f;
				Vec3 newDir = Vec3(finalPos.x - playerPos.x, 0, finalPos.z - playerPos.z);
				float newDirLen = std::sqrt(newDir.x * newDir.x + newDir.z * newDir.z);

				if (m_lastMoveDir.x != 0.0f || m_lastMoveDir.z != 0.0f) {
					if (newDirLen > 1.0f) {
						Vec3 normNew = Vec3(newDir.x / newDirLen, 0, newDir.z / newDirLen);
						float dot = normNew.x * m_lastMoveDir.x + normNew.z * m_lastMoveDir.z;
						dot = (std::max)(-1.0f, (std::min)(1.0f, dot));
						angle = acosf(dot) * 57.29578f; // rad to deg
					}
				}

				// Rate limit: match C# logic
				if (tickNow2 - m_lastMovementTick < 70 + (std::min)(60, ping) && angle < 60.0f)
					return;
				if (angle >= 60.0f && tickNow2 - m_lastMovementTick < 60)
					return;
			}
			else {
				// HighOrb mode: simplified delay
				if (tickNow2 - m_lastMovementTick < 50 + (std::min)(60, ping))
					return;
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
					evade.SaveBlockedCommand(movePos2D);
					return;
				}
			}

			// ShowFakeClick — matching NewOrbwalker.cs line 1402-1406
			// Shows green click indicator at the move position
			{
				auto* drawMenu = m_menu ? m_menu->GetSubMenu("Drawing") : nullptr;
				if (drawMenu) {
					auto* fakeClick = drawMenu->Get<SDK::MenuUI::MenuBool>("ShowFakeClick");
					if (fakeClick && fakeClick->Enabled) {
						int tickNow3 = SDK::Game::GetTickCount();
						int ping = (int)SDK::Game::GetPing();
						// Rate limit: 250ms - Ping*10 (matching C#)
						int fakeClickDelay = (std::max)(50, 250 - ping * 10);
						if (tickNow3 - m_lastFakeClickTick > fakeClickDelay) {
							SDK::CursorUtils::ShowMoveClick(finalPos); // Green = native game VFX
							m_lastFakeClickTick = tickNow3;
						}
					}
				}
			}

			MoveTo(finalPos);
		}

		void Attack(SDK::GameObject& target) {
			// ShowFakeClick — red indicator at attack target
			{
				auto* drawMenu = m_menu ? m_menu->GetSubMenu("Drawing") : nullptr;
				if (drawMenu) {
					auto* fakeClick = drawMenu->Get<SDK::MenuUI::MenuBool>("ShowFakeClick");
					if (fakeClick && fakeClick->Enabled) {
						SDK::CursorUtils::ShowAttackClick(target.GetPosition()); // Red = native game VFX
						m_lastFakeClickTick = SDK::Game::GetTickCount();
					}
				}
			}
			SDK::Orbwalker::IssueOrder(3, target.GetPosition(), &target);
			float now = SDK::Game::GetTime();
			int tickNow = SDK::Game::GetTickCount();
			m_lastAttackCommandTime = now;
			SDK::Orbwalker::LastAttackTime = now;
			// Tick-based timing sync (NewOrbwalker.cs OnDoCast line 681)
			m_lastAutoAttackTick = tickNow - (int)(SDK::Game::GetPing() / 2.0f);
			m_lastLocalAttackTick = tickNow;
			SDK::Orbwalker::LastAutoAttackTick = m_lastAutoAttackTick;
			SDK::Orbwalker::LastAutoAttackCommandTick = tickNow;
			SDK::Orbwalker::MissileLaunched = false;
			if (m_isKalista) SDK::Orbwalker::MissileLaunched = false;
			m_lastMovementTick = 0; // Reset movement tick on attack
			m_attackReadyEventFired = false;
			m_afterAttackFired = false;
			m_autoAttackCounter++;
			SDK::Orbwalker::TotalAutoAttacks++;
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
			m_lastMovementTick = SDK::Game::GetTickCount(); // Tick-based for path angle check
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
				float predictedHP = SDK::HealthPrediction::GetPrediction(minion, impactMs, (int)GetFarmDelayMs());

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

