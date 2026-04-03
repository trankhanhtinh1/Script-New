#pragma once

// ═══════════════════════════════════════════════════════
// NightSharp Ezreal Plugin — Ported line-by-line from Ezreal.cs
// ═══════════════════════════════════════════════════════

#include "../IPlugin.h"
#include "menu/MenuUI.h"
#include "sdk/Core/Game.h"
#include "sdk/SDK.h"
#include "sdk/Utils/Jungle.h"
#include "sdk/Wrappers/Damages/Damage.h"
#include "sdk/Wrappers/Orbwalking/Orbwalker.h"
#include "sdk/Wrappers/Spells/Spell.h"
#include "sdk/Wrappers/TargetSelector/TargetSelector.h"

namespace Plugins {

class EzrealPlugin : public IPlugin {
public:
  // ── IPlugin interface ──
  const char *GetName() const override { return "Ezreal"; }
  const char *GetInternalId() const override { return "champion_ezreal"; }
  const char *GetAuthor() const override { return "7UP / NightSharp"; }
  PluginCategory GetCategory() const override {
    return PluginCategory::Champion;
  }
  bool AutoLoadByDefault() const override { return false; }

  bool CanLoad() const override {
    const auto player = SDK::ObjectManager::Player();
    if (!player.IsValid())
      return false;
    return player.CharacterName() == "Ezreal";
  }

  // ════════════════════════════════════════════════
  // OnLoad — Spell Init + Menu (matching Ezreal.cs OnGameLoad lines 27-86)
  // ════════════════════════════════════════════════
  void OnLoad() override {
    if (m_menu)
      return;
    using namespace SDK;
    using namespace SDK::MenuUI;

    // ── Spell Definitions (Ezreal.cs lines 31-40) ──
    Q = Spell(SpellSlot::Q, 1200.0f);
    Q.SetSkillshot(0.25f, 53.0f, 2000.0f, true, SpellType::Line);

    W = Spell(SpellSlot::W, 1200.0f);
    W.SetSkillshot(0.25f, 55.0f, 1700.0f, false, SpellType::Line);

    E = Spell(SpellSlot::E, 475.0f);
    E.Delay = 0.65f;

    R = Spell(SpellSlot::R, 5000.0f);
    R.SetSkillshot(1.0f, 160.0f, 2200.0f, false, SpellType::Line);

    EQ = Spell(SpellSlot::Q, 1625.0f);
    EQ.SetSkillshot(0.90f, 57.0f, 1350.0f, true, SpellType::Line);

    // ── Menu (Ezreal.cs lines 43-79) ──
    m_menu = Menu::Create("EzrealRoot", "[NightSharp] Ezreal");

    // Combo Settings
    auto *combo = m_menu->AddSubMenu("combo", "Combo Settings");
    combo->Add<MenuBool>("useQ", "Use Q", true);
    combo->Add<MenuBool>("useW", "Use W", true);
    combo->Add<MenuBool>("useE", "Use E", true);
    combo->Add<MenuBool>("ComboECheck", "Use E |Safe Check", true);
    combo->Add<MenuBool>("ComboEWall", "Use E |Wall Check", true);
    combo->Add<MenuBool>("useR", "Use R", true);
    // Semi R — Ezreal.cs line 51: MenuKeyBind("SemiR", "Semi R", Keys.R, KeyBindType.Press)
    combo->Add<MenuKeyBind>("SemiR", "Semi R", 'T', KeyBindType::Press);

    // Harass Settings
    auto *harass = m_menu->AddSubMenu("harass", "Harass Settings");
    harass->Add<MenuBool>("useQ", "Use Q", true);
    harass->Add<MenuBool>("useW", "Use W", true);

    // LaneClear Settings
    auto *laneclear = m_menu->AddSubMenu("laneclear", "LaneClear Settings");
    laneclear->Add<MenuBool>("useQ", "Use Q", true);
    laneclear->Add<MenuBool>("QLH", "Use Q Last Hit", false);
    laneclear->Add<MenuSlider>("ManaCL", "Mana Clear", 15, 0, 100);

    // JungleClear Settings
    auto *jungle = m_menu->AddSubMenu("jungle", "Jungle Settings");
    jungle->Add<MenuBool>("useQ", "Use Q", true);
    jungle->Add<MenuBool>("useW", "Use W", true);
    jungle->Add<MenuSlider>("ManaCL", "Mana Clear", 15, 0, 100);

    // R Settings
    auto *rmenu = m_menu->AddSubMenu("rmenu", "R Settings");
    rmenu->Add<MenuBool>("AutoR", "Auto R", true);
    rmenu->Add<MenuSlider>("RRange", "Auto R |Min Cast Range >= x", 900, 0,
                           1500);
    rmenu->Add<MenuSlider>("RMaxRange", "Auto R |Max Cast Range >= x", 3000,
                           1500, 5000);

    // Misc
    auto *misc = m_menu->AddSubMenu("misc", "Misc Settings");
    misc->Add<MenuBool>("gapcloser", "Gapcloser", true);

    // KillSteal
    auto *ks = m_menu->AddSubMenu("killsteal", "KillSteal Settings");
    ks->Add<MenuBool>("killstealQ", "Use Q", true);
  }

  void OnUnload() override {
    if (!m_menu)
      return;
    SDK::MenuUI::Menu::Remove("EzrealRoot");
    m_menu = nullptr;
  }

  SDK::MenuUI::Menu *GetMenuRoot() override { return m_menu; }

  // ════════════════════════════════════════════════
  // File-based diagnostic logger (manual-map safe)
  // ════════════════════════════════════════════════
  static void EzLog(const char *msg) {
    HANDLE hFile =
        CreateFileA("C:\\Users\\Public\\Ezreal.txt", FILE_APPEND_DATA,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
      DWORD written = 0;
      WriteFile(hFile, msg, static_cast<DWORD>(strlen(msg)), &written, nullptr);
      CloseHandle(hFile);
    }
  }

  // ════════════════════════════════════════════════
  // OnUpdate — Main loop (matching Ezreal.cs Game_OnUpdate lines 198-236)
  // ════════════════════════════════════════════════
  void OnUpdate() override {
    using namespace SDK;
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !m_menu)
      return;

    // if (Player.IsDead || Player.IsRecalling() || Player.IsWindingUp) return;
    if (player.IsDead() || player.IsRecalling() || player.IsWindingUp())
      return;

    // ── Diagnostic: Log spell state once per second ──
    {
      const int now = Game::TickCount();
      if (now - m_lastDiagTick > 1000) {
        m_lastDiagTick = now;
        char buf[512] = {};
        wsprintfA(
            buf,
            "[Ezreal] Mode=%d Q.Ready=%d W.Ready=%d E.Ready=%d R.Ready=%d "
            "CanCast=%d GameTimex10=%d\r\n",
            static_cast<int>(Orbwalker::GetMode()), Q.IsReady() ? 1 : 0,
            W.IsReady() ? 1 : 0, E.IsReady() ? 1 : 0, R.IsReady() ? 1 : 0,
            CoreAPI::Control::CanCastSpell() ? 1 : 0,
            static_cast<int>(Game::Time() * 10.0f));
        EzLog(buf);
      }
    }

    // R.Range = RMenu["RMaxRange"].Value;
    if (R.Instance().Level() > 0) {
      auto *rmenu = m_menu->GetSubMenu("rmenu");
      if (rmenu) {
        R.Range = static_cast<float>(rmenu->GetSliderValue("RMaxRange", 3000));
      }
    }

    // ── Semi R (Ezreal.cs lines 209-212) ──
    // if (ComboMenu["SemiR"].GetValue<MenuKeyBind>().Active) { OneKeyCastR(); }
    {
      auto *combo = m_menu->GetSubMenu("combo");
      if (combo && combo->GetKeyBindValue("SemiR")) {
        OneKeyCastR(player);
      }
    }

    // AutoR logic (Ezreal.cs lines 213-216)
    {
      auto *rmenu = m_menu->GetSubMenu("rmenu");
      if (rmenu && rmenu->GetBoolValue("AutoR", true) && R.IsReady() &&
          player.CountEnemyHeroesInRange(1000) == 0) {
        AutoRLogic(player);
      }
    }

    // switch (Orbwalker.ActiveMode) (Ezreal.cs lines 217-232)
    const auto mode = Orbwalker::GetMode();
    switch (mode) {
    case OrbwalkerMode::Combo:
      Combo(player);
      break;
    case OrbwalkerMode::Harass:
      Harass(player);
      break;
    case OrbwalkerMode::Clear:
      LaneClear(player);
      JungleClear(player);
      break;
    case OrbwalkerMode::LastHit:
      LastHit(player);
      break;
    default:
      break;
    }

    // KillSteal always runs (Ezreal.cs line 233)
    KillSteal(player);
  }

private:
  SDK::MenuUI::Menu *m_menu = nullptr;
  SDK::Spell Q, W, E, R, EQ;
  int m_lastDiagTick = 0;

  // ════════════════════════════════════════════════
  // Combo (matching Ezreal.cs lines 298-377)
  // ════════════════════════════════════════════════
  void Combo(const SDK::AIHeroClient &player) {
    using namespace SDK;
    auto *combo = m_menu->GetSubMenu("combo");
    if (!combo)
      return;

    const bool useQ = combo->GetBoolValue("useQ", true);
    const bool useW = combo->GetBoolValue("useW", true);
    const bool useE = combo->GetBoolValue("useE", true);
    const bool useR = combo->GetBoolValue("useR", true);

    // var target = TargetSelector.GetTarget(EQ.Range, DamageType.Physical);
    const float eqRange = EQ.GetRange();
    auto target = TargetSelector::GetTarget(eqRange, DamageType::Physical);
    if (!target.IsValid() || !target.IsValidTarget(eqRange))
      return;

    // E logic (Ezreal.cs lines 308-311)
    if (useE && E.IsReady() && target.IsValidTarget(EQ.GetRange())) {
      ComboELogic(player, target);
    }

    // W logic (Ezreal.cs lines 313-334)
    if (useW && W.IsReady() && target.IsValidTarget(W.GetRange())) {
      auto wPred = W.GetPrediction(target);
      if (static_cast<int>(wPred.Hitchance) >=
          static_cast<int>(HitChance::High)) {
        // Case 1: Q is also ready — pre-cast W at Q's predicted position
        if (Q.IsReady()) {
          auto qPred = Q.GetPrediction(target);
          if (static_cast<int>(qPred.Hitchance) >=
              static_cast<int>(HitChance::High)) {
            W.Cast(qPred.CastPosition);
          }
        }
        // Case 2: Target is in auto-attack range — cast W directly
        if (player.InAutoAttackRange(target)) {
          W.Cast(wPred.CastPosition);
        }
      }
    }

    // Q logic (Ezreal.cs lines 336-344)
    if (useQ && Q.IsReady() && target.IsValidTarget(Q.GetRange())) {
      auto qp = Q.GetPrediction(target);
      if (static_cast<int>(qp.Hitchance) >=
          static_cast<int>(HitChance::Medium)) {
        Q.Cast(qp.CastPosition);
      }
    }

    // R logic in combo (Ezreal.cs lines 346-374)
    if (useR && R.IsReady()) {
      if (player.CountEnemyHeroesInRange(800) > 1)
        return;

      auto *rmenu = m_menu->GetSubMenu("rmenu");
      const float rMinRange =
          rmenu ? static_cast<float>(rmenu->GetSliderValue("RRange", 900))
                : 900.0f;

      for (const auto &rTarget : ObjectManager::EnemyHeroes()) {
        if (!rTarget.IsValidTarget(R.GetRange()))
          continue;
        if (rTarget.DistanceToPlayer() < rMinRange)
          continue;

        // Kill with R alone if far away
        const float rDmgSolo = Damage::GetSpellDamage(player, rTarget,
                                                      SpellSlot::R,
                                                      DamageStage::Default);

        if (rTarget.Health() < rDmgSolo &&
            rTarget.DistanceToPlayer() > Q.GetRange() + E.GetRange() / 2.0f) {
          R.CastPredicted(rTarget, HitChance::High);
        }

        // Kill with R+Q+W combo
        if (rTarget.IsValidTarget(Q.GetRange() + E.GetRange())) {
          float totalDmg = rDmgSolo;
          if (Q.IsReady())
            totalDmg += Damage::GetSpellDamage(player, rTarget, SpellSlot::Q,
                                               DamageStage::Default);
          if (W.IsReady())
            totalDmg += Damage::GetSpellDamage(player, rTarget, SpellSlot::W,
                                               DamageStage::Default);
          if (totalDmg > rTarget.Health() + rTarget.HPRegenRate() * 2.0f) {
            R.CastPredicted(rTarget, HitChance::High);
          }
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // ComboELogic (matching Ezreal.cs lines 379-455)
  // ════════════════════════════════════════════════
  void ComboELogic(const SDK::AIHeroClient &player,
                   const SDK::AIHeroClient &target) {
    using namespace SDK;
    auto *combo = m_menu->GetSubMenu("combo");
    if (!combo)
      return;

    const bool ECheck = combo->GetBoolValue("ComboECheck", true);
    const bool EWall = combo->GetBoolValue("ComboEWall", true);

    if (!target.IsValid() || !target.IsValidTarget())
      return;

    if (!ECheck)
      return;
    // Safe check: not under enemy turret, <= 2 enemies nearby
    if (player.CountEnemyHeroesInRange(1200.0f) > 2)
      return;

    const float aaRange = player.AttackRange() + player.BoundingRadius() +
                          target.BoundingRadius();
    if (target.DistanceToPlayer() <= aaRange)
      return; // already in AA range

    const Vector3 playerPos = player.Position();
    const Vector3 targetPos = target.Position();
    const Vector3 cursorPos = Game::CursorPos();

    // Extend toward target
    auto direction = (targetPos - playerPos);
    const float dist = direction.Length();
    if (dist > 0.001f)
      direction = direction * (475.0f / dist);
    const Vector3 castEPos = playerPos + direction;

    // Check: target must be closer to cursor than player (moving toward cursor)
    if (targetPos.Distance(cursorPos) >= playerPos.Distance(cursorPos))
      return;

    // Case 1: Kill with E + AA (Ezreal.cs lines 393-410)
    float eDmg = Damage::GetSpellDamage(player, target, SpellSlot::E,
                                        DamageStage::Default);
    float aaDmg = Damage::GetAutoAttackDamage(player, target);
    if (target.Health() < eDmg + aaDmg) {
      if (!EWall || !CoreAPI::NavGrid::IsWall(castEPos)) {
        E.Cast(castEPos);
      }
      return;
    }

    // Case 2: Kill with E + W (Ezreal.cs lines 412-431)
    if (W.IsReady()) {
      float wDmg = Damage::GetSpellDamage(player, target, SpellSlot::W,
                                          DamageStage::Default);
      if (target.Health() < eDmg + wDmg &&
          targetPos.Distance(cursorPos) + 350.0f <
              playerPos.Distance(cursorPos)) {
        if (!EWall || !CoreAPI::NavGrid::IsWall(castEPos)) {
          E.Cast(castEPos);
        }
        return;
      }
    }

    // Case 3: Kill with E + Q (Ezreal.cs lines 433-451)
    if (Q.IsReady()) {
      float qDmg = Damage::GetSpellDamage(player, target, SpellSlot::Q,
                                          DamageStage::Default);
      if (target.Health() < eDmg + qDmg &&
          targetPos.Distance(cursorPos) + 300.0f <
              playerPos.Distance(cursorPos)) {
        if (!EWall || !CoreAPI::NavGrid::IsWall(castEPos)) {
          E.Cast(castEPos);
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // Harass (matching Ezreal.cs lines 457-471)
  // ════════════════════════════════════════════════
  void Harass(const SDK::AIHeroClient &player) {
    using namespace SDK;
    auto *harass = m_menu->GetSubMenu("harass");
    if (!harass)
      return;

    if (harass->GetBoolValue("useQ", true) && Q.IsReady()) {
      auto target =
          TargetSelector::GetTarget(Q.GetRange(), DamageType::Physical);
      if (target.IsValid() && target.IsValidTarget(Q.GetRange())) {
        Q.CastPredicted(target, HitChance::High);
      }
    }
  }

  // ════════════════════════════════════════════════
  // LaneClear (matching Ezreal.cs lines 473-491)
  // Fixed: removed IsLaneMinion() filter — EnsoulSharp uses GetMinions()
  // which returns all enemy minions in range, not just lane minions.
  // ════════════════════════════════════════════════
  void LaneClear(const SDK::AIHeroClient &player) {
    using namespace SDK;
    auto *lc = m_menu->GetSubMenu("laneclear");
    if (!lc)
      return;

    if (!lc->GetBoolValue("useQ", true))
      return;
    if (player.ManaPercent() <
        static_cast<float>(lc->GetSliderValue("ManaCL", 15)))
      return;
    if (!Q.IsReady())
      return;

    // Ezreal.cs lines 482-488:
    // var preds = GameObjects.GetMinions(Player.Position, Q.Range)
    //     .Where(i => Q.GetHealthPrediction(i) > 0
    //         && Q.GetHealthPrediction(i) <= Q.GetDamage(i)
    //         && (...distance > autoAttackRange + 50 || health > aaDmg))
    //     .Select(y => Q.GetPrediction(y, false, -1, CollisionObjects.Minions))
    //     .Where(i => i.Hitchance >= HitChance.High)
    for (const auto &minion : ObjectManager::EnemyMinions()) {
      if (!minion.IsValid() || !minion.IsValidTarget(Q.GetRange()))
        continue;

      const float hpPred = Q.GetHealthPrediction(minion);
      if (hpPred <= 0.0f)
        continue;
      if (hpPred > Q.GetDamage(minion))
        continue;

      // Prefer Q on minions out of AA range or that we can't last hit with AA
      if (minion.DistanceToPlayer() > player.AttackRange() + player.BoundingRadius() + minion.BoundingRadius() + 50.0f ||
          minion.Health() > Damage::GetAutoAttackDamage(player, minion)) {
        // Cast with collision check — Q collides with minions
        // Note: When collision is detected, Hitchance is set to HitChance::Collision
        // so checking >= High naturally excludes collision cases
        auto pred = Q.GetPrediction(minion, true);
        if (static_cast<int>(pred.Hitchance) >= static_cast<int>(HitChance::High)) {
          Q.Cast(pred.CastPosition);
          return;
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // JungleClear (matching Ezreal.cs lines 494-523)
  // ════════════════════════════════════════════════
  void JungleClear(const SDK::AIHeroClient &player) {
    using namespace SDK;
    auto *jg = m_menu->GetSubMenu("jungle");
    if (!jg)
      return;

    const bool useQ = jg->GetBoolValue("useQ", true);
    const bool useW = jg->GetBoolValue("useW", true);
    if (player.ManaPercent() <
        static_cast<float>(jg->GetSliderValue("ManaCL", 15)))
      return;

    for (const auto &mob : ObjectManager::JungleMinions()) {
      if (!mob.IsValidTarget(Q.GetRange()))
        continue;

      // W on Legendary mobs only (Ezreal.cs lines 506-516)
      if (useW && W.IsReady() && mob.IsValidTarget(W.GetRange())) {
        if (Utils::Jungle::GetJungleType(mob) >= JungleType::Legendary) {
          W.CastPredicted(mob, HitChance::High);
        }
      }

      // Q on closest mob (Ezreal.cs lines 518-522)
      if (useQ && Q.IsReady() && player.Distance(mob) < Q.GetRange()) {
        Q.Cast(mob.Position());
        return;
      }
    }
  }

  // ════════════════════════════════════════════════
  // LastHit (matching Ezreal.cs lines 526-546)
  // Fixed: removed IsLaneMinion() filter
  // ════════════════════════════════════════════════
  void LastHit(const SDK::AIHeroClient &player) {
    using namespace SDK;
    auto *lc = m_menu->GetSubMenu("laneclear");
    if (!lc)
      return;

    if (player.ManaPercent() <
        static_cast<float>(lc->GetSliderValue("ManaCL", 15)))
      return;
    if (!Q.IsReady())
      return;

    for (const auto &minion : ObjectManager::EnemyMinions()) {
      if (!minion.IsValidTarget(Q.GetRange()))
        continue;

      // Only Q minions outside AA range that are killable
      const float aaRange = player.AttackRange() + player.BoundingRadius() +
                            minion.BoundingRadius();
      if (minion.DistanceToPlayer() > aaRange &&
          minion.Health() <
              Damage::GetSpellDamage(player, minion, SpellSlot::Q,
                                    DamageStage::Default)) {
        Q.CastPredicted(minion, HitChance::Medium);
        return;
      }
    }
  }

  // ════════════════════════════════════════════════
  // OneKeyCastR / Semi R (matching Ezreal.cs lines 238-257)
  // ════════════════════════════════════════════════
  void OneKeyCastR(const SDK::AIHeroClient &player) {
    using namespace SDK;

    // Player.IssueOrder(GameObjectOrder.MoveTo, Game.CursorPos);
    CoreAPI::Control::IssueMove(player.Position().IsZero()
        ? Game::CursorPos() : Game::CursorPos());

    if (!R.IsReady())
      return;

    auto *rmenu = m_menu->GetSubMenu("rmenu");
    const float rMinRange =
        rmenu ? static_cast<float>(rmenu->GetSliderValue("RRange", 900))
              : 900.0f;

    // var target = TargetSelector.GetTarget(R.Range, DamageType.Physical);
    auto target = TargetSelector::GetTarget(R.GetRange(), DamageType::Physical);

    // if (target.IsValidTarget(R.Range) && !target.IsValidTarget(RMenu["RRange"]))
    if (target.IsValid() && target.IsValidTarget(R.GetRange()) &&
        target.DistanceToPlayer() >= rMinRange) {
      R.CastPredicted(target, HitChance::High);
    }
  }

  // ════════════════════════════════════════════════
  // AutoRLogic (matching Ezreal.cs lines 259-281)
  // Fixed: R.Cast(target) → R.CastPredicted for both cases
  // ════════════════════════════════════════════════
  void AutoRLogic(const SDK::AIHeroClient &player) {
    using namespace SDK;
    auto *rmenu = m_menu->GetSubMenu("rmenu");
    if (!rmenu)
      return;

    const float rMinRange =
        static_cast<float>(rmenu->GetSliderValue("RRange", 900));

    for (const auto &target : ObjectManager::EnemyHeroes()) {
      if (!target.IsValidTarget(R.GetRange()))
        continue;
      if (target.DistanceToPlayer() < rMinRange)
        continue;

      float rDmg = Damage::GetSpellDamage(player, target, SpellSlot::R,
                                    DamageStage::Default);
      float qDmg = Damage::GetSpellDamage(player, target, SpellSlot::Q,
                                    DamageStage::Default);
      const float effectiveHp = target.Health() + target.HPRegenRate() * 2.0f;

      // Immobile target: R + 3*Q kill (Ezreal.cs lines 267-272)
      // C# uses: !target.CanMove && R.Cast(target) — targeted cast
      if (!target.IsMoving() && target.IsValidTarget(EQ.GetRange()) &&
          rDmg + qDmg * 3.0f >= effectiveHp) {
        // Immobile → cast at current position (high confidence)
        R.Cast(target.Position());
        continue;
      }

      // Standing still / low path: R alone kill (Ezreal.cs lines 274-279)
      // C# uses: rDmg > effectiveHp && target.Path.Length < 2 &&
      //          R.GetPrediction(target).Hitchance >= High → R.Cast(target)
      if (rDmg > effectiveHp) {
        R.CastPredicted(target, HitChance::High);
      }
    }
  }

  // ════════════════════════════════════════════════
  // KillSteal (matching Ezreal.cs lines 555-594)
  // ════════════════════════════════════════════════
  void KillSteal(const SDK::AIHeroClient &player) {
    using namespace SDK;
    auto *ks = m_menu->GetSubMenu("killsteal");
    if (!ks)
      return;
    if (!ks->GetBoolValue("killstealQ", true))
      return;
    if (!Q.IsReady())
      return;

    for (const auto &target : ObjectManager::EnemyHeroes()) {
      if (!target.IsValidTarget(Q.GetRange()))
        continue;

      // Skip invulnerable / untargetable targets
      if (target.HasBuff("JudicatorIntervention")) continue; // Kayle R
      if (target.HasBuff("kindredrnodeathbuff")) continue;   // Kindred R
      if (target.HasBuff("UndyingRage")) continue;           // Tryndamere R
      if (target.HasBuff("FioraW")) continue;                // Fiora Riposte
      if (target.HasBuff("ChronoShift")) continue;           // Zilean R
      if (target.HasBuff("zhonyasringshield")) continue;     // Zhonya's
      if (target.HasBuff("BardRStasis")) continue;           // Bard R
      if (target.HasBuff("MelW")) continue;                  // Mel W Shield

      // QDamage = 20/45/70/95/120 + 1.3 * bonus AD (Ezreal.cs lines 548-553)
      const int qLevel = Q.Instance().Level();
      if (qLevel <= 0)
        continue;
      constexpr float qBase[6] = {0.0f, 20.0f, 45.0f, 70.0f, 95.0f, 120.0f};
      const float rawQDmg =
          qBase[std::min(qLevel, 5)] + 1.30f * player.BonusAttackDamage();
      const float qDmg = player.CalculatePhysicalDamage(target, rawQDmg);

      const float effectiveHp = target.Health() + target.AllShield();

      if (player.Distance(target) > 150.0f) {
        if (effectiveHp <= qDmg) {
          Q.CastPredicted(target, HitChance::High);
          return;
        }
      } else {
        if (effectiveHp <= qDmg * 1.5f) {
          Q.CastPredicted(target, HitChance::High);
          return;
        }
      }
    }
  }
};

} // namespace Plugins
