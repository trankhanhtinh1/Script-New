#pragma once

// ═══════════════════════════════════════════════════════
// NightSharp Ezreal Plugin — Simplified C++ (mirrors Ezreal.cs)
// Logic: 100% identical to previous version
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

using namespace SDK;
using namespace SDK::MenuUI;

class EzrealPlugin : public IPlugin {
public:
  // ── IPlugin identity ──
  const char *GetName()       const override { return "Ezreal"; }
  const char *GetInternalId() const override { return "champion_ezreal"; }
  const char *GetAuthor()     const override { return "7UP / NightSharp"; }
  PluginCategory GetCategory() const override { return PluginCategory::Champion; }
  bool AutoLoadByDefault()    const override { return false; }

  bool CanLoad() const override {
    return Player().IsValid() && Player().CharacterName() == "Ezreal";
  }

  // ── Spells (like C#: public static Spell Q, W, E, R, EQ) ──
  Spell Q, W, E, R, EQ;

  // ════════════════════════════════════════════════
  // OnLoad — Spell Init + Menu  (Ezreal.cs lines 27-86)
  // ════════════════════════════════════════════════
  void OnLoad() override {
    if (m_menu) return;

    // Spell Definitions (Ezreal.cs lines 31-40)
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

    // Menu (Ezreal.cs lines 43-79)
    m_menu = Menu::Create("EzrealRoot", "[NightSharp] Ezreal");

    auto *combo = m_menu->AddSubMenu("combo", "Combo Settings");
    combo->Add<MenuBool>("useQ", "Use Q", true);
    combo->Add<MenuBool>("useW", "Use W", true);
    combo->Add<MenuBool>("useE", "Use E", true);
    combo->Add<MenuBool>("ComboECheck", "Use E |Safe Check", true);
    combo->Add<MenuBool>("ComboEWall", "Use E |Wall Check", true);
    combo->Add<MenuBool>("useR", "Use R", true);
    combo->Add<MenuKeyBind>("SemiR", "Semi R", 'T', KeyBindType::Press);

    auto *harass = m_menu->AddSubMenu("harass", "Harass Settings");
    harass->Add<MenuBool>("useQ", "Use Q", true);
    harass->Add<MenuBool>("useW", "Use W", true);

    auto *laneclear = m_menu->AddSubMenu("laneclear", "LaneClear Settings");
    laneclear->Add<MenuBool>("useQ", "Use Q", true);
    laneclear->Add<MenuBool>("QLH", "Use Q Last Hit", false);
    laneclear->Add<MenuSlider>("ManaCL", "Mana Clear", 15, 0, 100);

    auto *jungle = m_menu->AddSubMenu("jungle", "Jungle Settings");
    jungle->Add<MenuBool>("useQ", "Use Q", true);
    jungle->Add<MenuBool>("useW", "Use W", true);
    jungle->Add<MenuSlider>("ManaCL", "Mana Clear", 15, 0, 100);

    auto *rmenu = m_menu->AddSubMenu("rmenu", "R Settings");
    rmenu->Add<MenuBool>("AutoR", "Auto R", true);
    rmenu->Add<MenuSlider>("RRange", "Auto R |Min Cast Range >= x", 900, 0, 1500);
    rmenu->Add<MenuSlider>("RMaxRange", "Auto R |Max Cast Range >= x", 3000, 1500, 5000);

    auto *misc = m_menu->AddSubMenu("misc", "Misc Settings");
    misc->Add<MenuBool>("gapcloser", "Gapcloser", true);
    misc->Add<MenuBool>("antigrab", "Anti-Hook (Blitz/Thresh/Pyke)", true);

    auto *ks = m_menu->AddSubMenu("killsteal", "KillSteal Settings");
    ks->Add<MenuBool>("killstealQ", "Use Q", true);

    // Events are automatically registered by IPlugin::RegisterEvents()
  }

  void OnUnload() override {
    if (!m_menu) return;
    Menu::Remove("EzrealRoot");
    m_menu = nullptr;
  }

  Menu *GetMenuRoot() override { return m_menu; }

  // ════════════════════════════════════════════════
  // OnBeforeAttack — W weaving  (Ezreal.cs lines 88-129)
  // ════════════════════════════════════════════════
  void OnBeforeAttack(OrbwalkingActionArgs& args) override {
    if (!m_menu || !args.Target.IsValid() || args.Target.IsDead()) return;

    if (Orbwalker::GetMode() == OrbwalkerMode::Combo) {
      auto *combo = m_menu->GetSubMenu("combo");
      if (combo && combo->GetBoolValue("useW", true) && W.IsReady()) {
        auto target = TargetSelector::GetTarget(W.Range, DamageType::Physical);
        if (target.IsValid() && target.IsValidTarget(W.Range)) {
          W.CastPredicted(target, HitChance::High);
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // OnAfterAttack — Q weaving  (Ezreal.cs lines 131-177)
  // ════════════════════════════════════════════════
  void OnAfterAttack(OrbwalkingActionArgs& args) override {
    if (!m_menu || !args.Target.IsValid() || args.Target.IsDead()) return;

    const auto mode = Orbwalker::GetMode();

    if (mode == OrbwalkerMode::Combo) {
      auto *combo = m_menu->GetSubMenu("combo");
      if (combo && combo->GetBoolValue("useQ", true) && Q.IsReady()) {
        auto target = TargetSelector::GetTarget(Q.Range, DamageType::Physical);
        if (target.IsValid() && target.IsValidTarget(Q.Range)) {
          Q.CastPredicted(target, HitChance::High);
        }
      }
    } else if (mode == OrbwalkerMode::Harass || mode == OrbwalkerMode::Clear) {
      auto *harass = m_menu->GetSubMenu("harass");
      if (harass && harass->GetBoolValue("useQ", true) && Q.IsReady()) {
        auto target = TargetSelector::GetTarget(Q.Range, DamageType::Physical);
        if (target.IsValid() && target.IsValidTarget(Q.Range)) {
          Q.CastPredicted(target, HitChance::High);
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // OnGapcloser — E escape  (Ezreal.cs lines 179-196)
  // ════════════════════════════════════════════════
  void OnGapcloser(const AIHeroClient& sender, const AntiGapcloser::GapcloserArgs& args) override {
    if (!m_menu) return;
    auto *misc = m_menu->GetSubMenu("misc");
    if (!misc || !misc->GetBoolValue("gapcloser", true)) return;
    if (!E.IsReady() || !sender.IsValid() || !sender.IsValidTarget(E.Range)) return;

    const Vector3 playerPos = Player().Position();
    const Vector3 senderPos = sender.Position();

    // Melee gapcloser (Ezreal.cs lines 184-186)
    if (sender.IsMelee()) {
      if (sender.IsValidTarget(sender.AttackRange() + sender.BoundingRadius() + 100.0f, playerPos)) {
        E.Cast(playerPos + (playerPos - senderPos).Normalized() * E.Range);
        return;
      }
    }

    // Dashing toward us (Ezreal.cs lines 188-191)
    if (sender.IsDashing()) {
      if (args.EndPosition.Distance(playerPos) <= 250.0f ||
          senderPos.Distance(playerPos) <= 300.0f) {
        E.Cast(playerPos + (playerPos - senderPos).Normalized() * E.Range);
        return;
      }
    }

    // Important spell nearby (Ezreal.cs lines 193-195)
    if (senderPos.Distance(playerPos) <= 300.0f) {
      E.Cast(playerPos + (playerPos - senderPos).Normalized() * E.Range);
    }
  }

  // ════════════════════════════════════════════════
  // OnBuffGain — Anti-Hook  (Ezreal.cs lines 284-297)
  // ════════════════════════════════════════════════
  void OnBuffGain(const AIBaseClient& sender, const SDK::Events::BuffEventArgs& args) override {
    if (!m_menu || !sender.IsValid() || !sender.IsMe() || !E.IsReady()) return;

    auto *misc = m_menu->GetSubMenu("misc");
    if (!misc || !misc->GetBoolValue("antigrab", true)) return;

    if (args.Name == "ThreshQ" || args.Name == "rocketgrab2" || args.Name == "PykeQ") {
      const Vector3 playerPos = Player().Position();

      // Find hook caster and E away
      AIHeroClient hookCaster;
      for (const auto& enemy : ObjectManager::EnemyHeroes()) {
        if (!enemy.IsValid()) continue;
        const std::string name = enemy.CharacterName();
        if ((args.Name == "ThreshQ" && name == "Thresh") ||
            (args.Name == "rocketgrab2" && name == "Blitzcrank") ||
            (args.Name == "PykeQ" && name == "Pyke")) {
          hookCaster = enemy;
          break;
        }
      }

      Vector3 escapeDir = hookCaster.IsValid()
          ? (playerPos - hookCaster.Position())
          : (playerPos - Player().Direction());

      float len = escapeDir.Length();
      if (len > 0.001f) escapeDir = escapeDir * (E.Range / len);
      E.Cast(playerPos + escapeDir);
    }
  }

  // ════════════════════════════════════════════════
  // OnUpdate — Main loop  (Ezreal.cs lines 198-236)
  // ════════════════════════════════════════════════
  void OnUpdate() override {
    if (!Player().IsValid() || !m_menu) return;
    if (Player().IsDead() || Player().IsRecalling() || Player().IsWindingUp()) return;

    // R.Range = RMenu["RMaxRange"] (Ezreal.cs line 206)
    if (R.Instance().Level() > 0) {
      auto *rmenu = m_menu->GetSubMenu("rmenu");
      if (rmenu) R.Range = static_cast<float>(rmenu->GetSliderValue("RMaxRange", 3000));
    }

    // Semi R (Ezreal.cs lines 209-212)
    auto *combo = m_menu->GetSubMenu("combo");
    if (combo && combo->GetKeyBindValue("SemiR")) {
      OneKeyCastR();
    }

    // AutoR (Ezreal.cs lines 213-216)
    auto *rmenu = m_menu->GetSubMenu("rmenu");
    if (rmenu && rmenu->GetBoolValue("AutoR", true) && R.IsReady() &&
        Player().CountEnemyHeroesInRange(1000) == 0) {
      AutoRLogic();
    }

    // Mode switch (Ezreal.cs lines 217-232)
    switch (Orbwalker::GetMode()) {
    case OrbwalkerMode::Combo:    Combo();    break;
    case OrbwalkerMode::Harass:   Harass();   break;
    case OrbwalkerMode::Clear:    LaneClear(); JungleClear(); break;
    case OrbwalkerMode::LastHit:  LastHit();  break;
    default: break;
    }

    KillSteal();
  }

private:
  Menu *m_menu = nullptr;

  // ════════════════════════════════════════════════
  // Combo  (Ezreal.cs lines 298-377)
  // ════════════════════════════════════════════════
  void Combo() {
    auto *combo = m_menu->GetSubMenu("combo");
    if (!combo) return;

    const bool useQ = combo->GetBoolValue("useQ", true);
    const bool useW = combo->GetBoolValue("useW", true);
    const bool useE = combo->GetBoolValue("useE", true);
    const bool useR = combo->GetBoolValue("useR", true);

    auto target = TargetSelector::GetTarget(EQ.Range, DamageType::Physical);
    if (!target.IsValid() || !target.IsValidTarget(EQ.Range)) return;

    // E logic
    if (useE && E.IsReady() && target.IsValidTarget(EQ.Range)) {
      ComboELogic(target);
    }

    // W logic (Ezreal.cs lines 313-334)
    if (useW && W.IsReady() && target.IsValidTarget(W.Range)) {
      auto wPred = W.GetPrediction(target);
      if (wPred.Hitchance >= HitChance::High) {
        if (Q.IsReady()) {
          auto qPred = Q.GetPrediction(target);
          if (qPred.Hitchance >= HitChance::High) {
            W.Cast(qPred.CastPosition);
          }
        }
        if (Player().InAutoAttackRange(target)) {
          W.Cast(wPred.CastPosition);
        }
      }
    }

    // Q logic (Ezreal.cs lines 336-344)
    if (useQ && Q.IsReady() && target.IsValidTarget(Q.Range)) {
      auto qp = Q.GetPrediction(target);
      if (qp.Hitchance >= HitChance::Medium) {
        Q.Cast(qp.CastPosition);
      }
    }

    // R logic in combo (Ezreal.cs lines 346-374)
    if (useR && R.IsReady()) {
      if (Player().CountEnemyHeroesInRange(800) > 1) return;

      auto *rmenu = m_menu->GetSubMenu("rmenu");
      const float rMinRange = rmenu ? static_cast<float>(rmenu->GetSliderValue("RRange", 900)) : 900.0f;

      for (const auto &rTarget : ObjectManager::EnemyHeroes()) {
        if (!rTarget.IsValidTarget(R.Range)) continue;
        if (rTarget.DistanceToPlayer() < rMinRange) continue;

        const float rDmg = Damage::GetSpellDamage(Player(), rTarget, SpellSlot::R, DamageStage::Default);

        if (rTarget.Health() < rDmg && rTarget.DistanceToPlayer() > Q.Range + E.Range / 2.0f) {
          R.CastPredicted(rTarget, HitChance::High);
        }

        if (rTarget.IsValidTarget(Q.Range + E.Range)) {
          float totalDmg = rDmg;
          if (Q.IsReady()) totalDmg += Damage::GetSpellDamage(Player(), rTarget, SpellSlot::Q, DamageStage::Default);
          if (W.IsReady()) totalDmg += Damage::GetSpellDamage(Player(), rTarget, SpellSlot::W, DamageStage::Default);
          if (totalDmg > rTarget.Health() + rTarget.HPRegenRate() * 2.0f) {
            R.CastPredicted(rTarget, HitChance::High);
          }
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // ComboELogic  (Ezreal.cs lines 379-455)
  // ════════════════════════════════════════════════
  void ComboELogic(const AIHeroClient &target) {
    auto *combo = m_menu->GetSubMenu("combo");
    if (!combo || !target.IsValid() || !target.IsValidTarget()) return;

    const bool ECheck = combo->GetBoolValue("ComboECheck", true);
    const bool EWall  = combo->GetBoolValue("ComboEWall", true);

    if (!ECheck) return;
    if (Player().CountEnemyHeroesInRange(1200.0f) > 2) return;

    const float aaRange = Player().AttackRange() + Player().BoundingRadius() + target.BoundingRadius();
    if (target.DistanceToPlayer() <= aaRange) return;

    const Vector3 playerPos = Player().Position();
    const Vector3 targetPos = target.Position();
    const Vector3 cursorPos = Game::CursorPos();
    const Vector3 castEPos  = playerPos + (targetPos - playerPos).Normalized() * 475.0f;

    if (targetPos.Distance(cursorPos) >= playerPos.Distance(cursorPos)) return;

    // Kill with E + AA
    float eDmg = Damage::GetSpellDamage(Player(), target, SpellSlot::E, DamageStage::Default);
    float aaDmg = Damage::GetAutoAttackDamage(Player(), target);
    if (target.Health() < eDmg + aaDmg) {
      if (!EWall || !CoreAPI::NavGrid::IsWall(castEPos)) E.Cast(castEPos);
      return;
    }

    // Kill with E + W
    if (W.IsReady()) {
      float wDmg = Damage::GetSpellDamage(Player(), target, SpellSlot::W, DamageStage::Default);
      if (target.Health() < eDmg + wDmg &&
          targetPos.Distance(cursorPos) + 350.0f < playerPos.Distance(cursorPos)) {
        if (!EWall || !CoreAPI::NavGrid::IsWall(castEPos)) E.Cast(castEPos);
        return;
      }
    }

    // Kill with E + Q
    if (Q.IsReady()) {
      float qDmg = Damage::GetSpellDamage(Player(), target, SpellSlot::Q, DamageStage::Default);
      if (target.Health() < eDmg + qDmg &&
          targetPos.Distance(cursorPos) + 300.0f < playerPos.Distance(cursorPos)) {
        if (!EWall || !CoreAPI::NavGrid::IsWall(castEPos)) E.Cast(castEPos);
      }
    }
  }

  // ════════════════════════════════════════════════
  // Harass  (Ezreal.cs lines 457-471)
  // ════════════════════════════════════════════════
  void Harass() {
    auto *harass = m_menu->GetSubMenu("harass");
    if (!harass) return;

    if (harass->GetBoolValue("useQ", true) && Q.IsReady()) {
      auto target = TargetSelector::GetTarget(Q.Range, DamageType::Physical);
      if (target.IsValid() && target.IsValidTarget(Q.Range)) {
        Q.CastPredicted(target, HitChance::High);
      }
    }
  }

  // ════════════════════════════════════════════════
  // LaneClear  (Ezreal.cs lines 473-491)
  // ════════════════════════════════════════════════
  void LaneClear() {
    auto *lc = m_menu->GetSubMenu("laneclear");
    if (!lc || !lc->GetBoolValue("useQ", true)) return;
    if (Player().ManaPercent() < static_cast<float>(lc->GetSliderValue("ManaCL", 15))) return;
    if (!Q.IsReady()) return;

    const float aaRange = Player().AttackRange() + Player().BoundingRadius();
    const bool lastHitOnly = lc->GetBoolValue("QLH", false);

    for (const auto &minion : ObjectManager::EnemyMinions()) {
      if (!minion.IsValid() || !minion.IsValidTarget(Q.Range)) continue;

      const float qDmg = Damage::GetSpellDamage(Player(), minion, SpellSlot::Q, DamageStage::Default);
      if (qDmg <= 0.0f) continue;

      const float minionDist = minion.DistanceToPlayer();
      const bool outOfAARange = minionDist > aaRange + minion.BoundingRadius() + 50.0f;
      const float hp = minion.Health();

      if (lastHitOnly) {
        // Only Q minions that Q can kill AND (out of AA range OR AA can't kill)
        if (hp <= qDmg && (outOfAARange || hp > Player().GetAutoAttackDamage(minion))) {
          Q.Cast(minion.Position());
          return;
        }
      } else {
        // Q any minion that Q can kill
        if (hp <= qDmg) {
          Q.Cast(minion.Position());
          return;
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // JungleClear  (Ezreal.cs lines 494-523)
  // ════════════════════════════════════════════════
  void JungleClear() {
    auto *jg = m_menu->GetSubMenu("jungle");
    if (!jg) return;

    const bool useQ = jg->GetBoolValue("useQ", true);
    const bool useW = jg->GetBoolValue("useW", true);
    if (Player().ManaPercent() < static_cast<float>(jg->GetSliderValue("ManaCL", 15))) return;

    // Get jungle mobs and sort by priority (big first)
    auto mobs = ObjectManager::JungleMinions();
    if (mobs.empty()) return;

    std::sort(mobs.begin(), mobs.end(),
      [](const AIMinionClient& a, const AIMinionClient& b) {
        return Utils::Jungle::GetJungleType(a) > Utils::Jungle::GetJungleType(b);
      });

    // W on Legendary (Dragon/Baron/Herald) — W has no collision
    if (useW && W.IsReady()) {
      for (const auto &mob : mobs) {
        if (!mob.IsValidTarget(W.Range)) continue;
        if (Utils::Jungle::GetJungleType(mob) >= JungleType::Legendary) {
          W.Cast(mob.Position());
          return;
        }
      }
    }

    // Q on biggest mob — use prediction + collision check (Q collides with minions/plants)
    if (useQ && Q.IsReady()) {
      const auto &mob = mobs.front();
      if (mob.IsValidTarget(Q.Range)) {
        auto pred = Q.GetPrediction(mob);
        if (pred.Hitchance >= HitChance::Medium) {
          Q.Cast(pred.CastPosition);
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // LastHit  (Ezreal.cs lines 526-546)
  // ════════════════════════════════════════════════
  void LastHit() {
    auto *lc = m_menu->GetSubMenu("laneclear");
    if (!lc) return;
    if (Player().ManaPercent() < static_cast<float>(lc->GetSliderValue("ManaCL", 15))) return;
    if (!Q.IsReady()) return;

    for (const auto &minion : ObjectManager::EnemyMinions()) {
      if (!minion.IsValidTarget(Q.Range)) continue;

      const float aaRange = Player().AttackRange() + Player().BoundingRadius() + minion.BoundingRadius();
      if (minion.DistanceToPlayer() > aaRange &&
          minion.Health() < Damage::GetSpellDamage(Player(), minion, SpellSlot::Q, DamageStage::Default)) {
        Q.CastPredicted(minion, HitChance::Medium);
        return;
      }
    }
  }

  // ════════════════════════════════════════════════
  // OneKeyCastR / Semi R  (Ezreal.cs lines 238-257)
  // ════════════════════════════════════════════════
  void OneKeyCastR() {
    CoreAPI::Control::IssueMove(Game::CursorPos());
    if (!R.IsReady()) return;

    // Semi R: no range restriction — fire at any valid target
    auto target = TargetSelector::GetTarget(R.Range, DamageType::Physical);
    if (target.IsValid() && target.IsValidTarget(R.Range)) {
      R.CastPredicted(target, HitChance::High);
    }
  }

  // ════════════════════════════════════════════════
  // AutoRLogic  (Ezreal.cs lines 259-281)
  // ════════════════════════════════════════════════
  void AutoRLogic() {
    auto *rmenu = m_menu->GetSubMenu("rmenu");
    if (!rmenu) return;

    const float rMinRange = static_cast<float>(rmenu->GetSliderValue("RRange", 900));

    for (const auto &target : ObjectManager::EnemyHeroes()) {
      if (!target.IsValidTarget(R.Range) || target.DistanceToPlayer() < rMinRange) continue;

      float rDmg = Damage::GetSpellDamage(Player(), target, SpellSlot::R, DamageStage::Default);
      float qDmg = Damage::GetSpellDamage(Player(), target, SpellSlot::Q, DamageStage::Default);
      const float effectiveHp = target.Health() + target.HPRegenRate() * 2.0f;

      // Immobile target: R + 3*Q kill
      if (!target.IsMoving() && target.IsValidTarget(EQ.Range) && rDmg + qDmg * 3.0f >= effectiveHp) {
        R.Cast(target.Position());
        continue;
      }

      // R alone kill
      if (rDmg > effectiveHp) {
        R.CastPredicted(target, HitChance::High);
      }
    }
  }

  // ════════════════════════════════════════════════
  // KillSteal  (Ezreal.cs lines 555-594)
  // ════════════════════════════════════════════════
  void KillSteal() {
    auto *ks = m_menu->GetSubMenu("killsteal");
    if (!ks || !ks->GetBoolValue("killstealQ", true) || !Q.IsReady()) return;

    for (const auto &target : ObjectManager::EnemyHeroes()) {
      if (!target.IsValidTarget(Q.Range)) continue;

      // Skip invulnerable targets
      if (target.HasBuff("JudicatorIntervention") || target.HasBuff("kindredrnodeathbuff") ||
          target.HasBuff("UndyingRage") || target.HasBuff("FioraW") ||
          target.HasBuff("ChronoShift") || target.HasBuff("zhonyasringshield") ||
          target.HasBuff("BardRStasis") || target.HasBuff("MelW")) continue;

      // QDamage = 20/45/70/95/120 + 1.3 * bonus AD
      const int qLevel = Q.Instance().Level();
      if (qLevel <= 0) continue;
      constexpr float qBase[6] = {0.0f, 20.0f, 45.0f, 70.0f, 95.0f, 120.0f};
      const float qDmg = Player().CalculatePhysicalDamage(target,
          qBase[std::min(qLevel, 5)] + 1.30f * Player().BonusAttackDamage());
      const float effectiveHp = target.Health() + target.AllShield();

      if (Player().Distance(target) > 150.0f) {
        if (effectiveHp <= qDmg) { Q.CastPredicted(target, HitChance::High); return; }
      } else {
        if (effectiveHp <= qDmg * 1.5f) { Q.CastPredicted(target, HitChance::High); return; }
      }
    }
  }
};

} // namespace Plugins
