#pragma once

// ═══════════════════════════════════════════════════════
// NightSharp Xerath Plugin — Full port from Xerath.cs (7UPAIO)
// Logic: 100% giữ nguyên bản gốc, thứ tự logic giữ nguyên
// ═══════════════════════════════════════════════════════

#include "../IPlugin.h"
#include "menu/MenuUI.h"
#include "sdk/Core/Game.h"
#include "sdk/SDK.h"
#include "sdk/Wrappers/Damages/Damage.h"
#include "sdk/Wrappers/Orbwalking/Orbwalker.h"
#include "sdk/Wrappers/Spells/Spell.h"
#include "sdk/Wrappers/TargetSelector/TargetSelector.h"
#include "sdk/Events/SpellCastTracker.h"

#include <algorithm>
#include <cfloat>
#include <string>

namespace Plugins {

using namespace SDK;
using namespace SDK::MenuUI;

class XerathPlugin : public IPlugin {
public:
  // ── IPlugin identity ──
  const char *GetName()       const override { return "Xerath"; }
  const char *GetInternalId() const override { return "champion_xerath"; }
  const char *GetAuthor()     const override { return "7UP / NightSharp"; }
  PluginCategory GetCategory() const override { return PluginCategory::Champion; }
  bool AutoLoadByDefault()    const override { return false; }

  bool CanLoad() const override {
    return Player().IsValid() && Player().CharacterName() == "Xerath";
  }

  // ── Spells (Xerath.cs lines 23-26) ──
  Spell Q, W, E, R;

  // ══ State (Xerath.cs lines 27-29) ══
  int m_wallCastT = 0;
  Vector3 m_yasuoWallCastedPos = {};

  // ════════════════════════════════════════════════
  // OnLoad — Spell Init + Menu  (Xerath.cs lines 30-106)
  // ════════════════════════════════════════════════
  void OnLoad() override {
    if (m_menu) return;

    // Spell Definitions (Xerath.cs lines 34-45)
    Q = Spell(SpellSlot::Q, 750.0f);
    Q.SetSkillshot(0.55f, 65.0f, FLT_MAX, false, SpellType::Line);
    Q.SetCharged("XerathArcanopulseChargeUp", "XerathArcanopulseChargeUp", 750.0f, 1550.0f, 1500);

    W = Spell(SpellSlot::W, 950.0f);
    W.SetSkillshot(0.65f, 110.0f, FLT_MAX, false, SpellType::Circle);

    E = Spell(SpellSlot::E, 1050.0f);
    E.SetSkillshot(0.25f, 55.0f, 1400.0f, true, SpellType::Line);

    R = Spell(SpellSlot::R, 4990.0f);
    R.SetSkillshot(0.70f, 110.0f, FLT_MAX, false, SpellType::Circle);

    // Menu (Xerath.cs lines 48-100)
    m_menu = Menu::Create("XerathRoot", "[NightSharp] Xerath");

    // ── Combo Menu (Xerath.cs lines 49-53) ──
    auto *comboMenu = m_menu->AddSubMenu("ComboMenu", "Combo Settings");
    comboMenu->Add<MenuBool>("ComboQ", "Use Q", true);
    comboMenu->Add<MenuBool>("ComboW", "Use W", true);
    comboMenu->Add<MenuBool>("ComboE", "Use E", true);

    // ── Harass Menu (Xerath.cs lines 54-58) ──
    auto *harassMenu = m_menu->AddSubMenu("HarassMenu", "Harass Settings");
    harassMenu->Add<MenuBool>("HarassQ", "Use Q", true);
    harassMenu->Add<MenuBool>("HarassW", "Use W", true);
    harassMenu->Add<MenuBool>("HarassE", "Use E", true);
    harassMenu->Add<MenuSlider>("Mana", "Min Mana Harass", 50, 0, 100);

    // ── LaneClear Menu (Xerath.cs lines 60-66) ──
    auto *laneClearMenu = m_menu->AddSubMenu("LaneClearMenu", "LaneClear Settings");
    laneClearMenu->Add<MenuBool>("LaneQ", "Use Q", true);
    laneClearMenu->Add<MenuSlider>("MinQ", "Hit Minions LaneClear Q", 3, 1, 6);
    laneClearMenu->Add<MenuBool>("LaneW", "Use W", true);
    laneClearMenu->Add<MenuSlider>("MinW", "Hit Minions LaneClear W", 3, 1, 6);
    laneClearMenu->Add<MenuSlider>("ManaLC", "Min Mana LaneClear", 60, 0, 100);

    // ── JungleClear Menu (Xerath.cs lines 67-72) ──
    auto *jungleMenu = m_menu->AddSubMenu("JungleClearMenu", "JungleClear Settings");
    jungleMenu->Add<MenuBool>("QJungle", "Use Q JungleClear", true);
    jungleMenu->Add<MenuBool>("WJungle", "Use W JungleClear", true);
    jungleMenu->Add<MenuBool>("EJungle", "Use E JungleClear", true);
    jungleMenu->Add<MenuSlider>("ManaJG", "Min Mana JungleClear", 40, 0, 100);

    // ── Misc Menu (Xerath.cs lines 73-78) ──
    auto *miscMenu = m_menu->AddSubMenu("Misc", "Misc Settings");
    miscMenu->Add<MenuBool>("qslowcast", "Slow Q Cast (high hitchance)", false);
    miscMenu->Add<MenuBool>("rslowcast", "Slow R Cast (high hitchance)", false);
    miscMenu->Add<MenuBool>("eantigapcloser", "Use E AntiGapcloser", true);
    miscMenu->Add<MenuBool>("einterrupt", "Use E Interrupt Spell", true);

    // ── KillSteal Menu (Xerath.cs lines 79-83) ──
    auto *ksMenu = m_menu->AddSubMenu("KillStealMenu", "KillSteal Settings");
    ksMenu->Add<MenuBool>("KsQ", "Use Q KillSteal", true);
    ksMenu->Add<MenuBool>("KsW", "Use W KillSteal", true);
    ksMenu->Add<MenuBool>("KsE", "Use E KillSteal", true);

    // ── R Settings Menu (Xerath.cs lines 84-88) ──
    auto *ultiMenu = m_menu->AddSubMenu("Ulti", "R Settings");
    ultiMenu->Add<MenuKeyBind>("RKey", "R Key", 'T', KeyBindType::Press);
    ultiMenu->Add<MenuBool>("NearMouse", "Near Mouse", true);
    ultiMenu->Add<MenuSlider>("MouseZone", "Mouse Zone", 600, 0, 1200);

    // ── Semi Key Menu (Xerath.cs lines 89-92) ──
    auto *semiMenu = m_menu->AddSubMenu("Semi", "Semi Key");
    semiMenu->Add<MenuKeyBind>("WKey", "Semi W Key", 'W', KeyBindType::Press);
    semiMenu->Add<MenuKeyBind>("EKey", "Semi E Key", 'E', KeyBindType::Press);
  }

  void OnUnload() override {
    if (!m_menu) return;
    Menu::Remove("XerathRoot");
    m_menu = nullptr;
  }

  Menu *GetMenuRoot() override { return m_menu; }

  // ════════════════════════════════════════════════
  // OnGapcloser (Xerath.cs lines 184-205)
  // ════════════════════════════════════════════════
  void OnGapcloser(const AIHeroClient& sender, const AntiGapcloser::GapcloserArgs& args) override {
    if (!m_menu) return;
    if (Player().IsDead() || Player().IsRecalling()) return;

    auto *miscMenu = m_menu->GetSubMenu("Misc");
    if (!miscMenu) return;

    if (miscMenu->GetBoolValue("eantigapcloser", true) && E.IsReady() &&
        args.EndPosition.Distance(Player().Position()) < 250.0f) {
      auto pred = E.GetPrediction(sender);
      if (pred.Hitchance >= HitChance::High) {
        E.Cast(pred.CastPosition);
      }
    }
  }

  // ════════════════════════════════════════════════
  // OnProcessSpellCast (Xerath.cs lines 158-181)
  // Tracks Yasuo wall + last cast times
  // ════════════════════════════════════════════════
  void OnProcessSpellCast(const AIBaseClient& sender,
      const Events::SpellCast::ProcessSpellCastEventArgs& args) override {
    if (!m_menu) return;

    // Xerath.cs lines 161-169: Track last cast time (note: C# code references Syndra names — bug in C# code)
    // We keep the same behavior for consistency
    if (sender.IsValid() && sender.IsMe()) {
      if (args.SpellName == "XerathArcanopulseChargeUp")
        Q.LastCastAttemptTime = Game::TickCount();
      if (args.SpellName == "XerathArcaneBarrage2")
        W.LastCastAttemptTime = Game::TickCount();
      if (args.SpellName == "XerathMageSpear")
        E.LastCastAttemptTime = Game::TickCount();
    }

    // Xerath.cs lines 176-180: Track Yasuo wall
    if (sender.IsValid() && sender.Team() == Player().Team() && args.SpellName == "YasuoWMovingWall") {
      m_wallCastT = Game::TickCount();
      m_yasuoWallCastedPos = sender.Position();
    }
  }

  // ════════════════════════════════════════════════
  // OnUpdate — Main loop (Xerath.cs lines 216-238)
  // ════════════════════════════════════════════════
  void OnUpdate() override {
    if (!Player().IsValid() || !m_menu) return;
    if (Player().IsDead() || Player().IsRecalling() || Player().IsWindingUp()) return;

    auto mode = Orbwalker::GetMode();

    switch (mode) {
      case OrbwalkerMode::Combo:
        Combo();
        return; // Xerath.cs line 226: return after combo
      case OrbwalkerMode::Harass:
        Harass();
        break;
      case OrbwalkerMode::Clear:
        LaneClear();
        JungleClear();
        break;
      default:
        break;
    }

    // Always run these (Xerath.cs lines 235-237)
    KillSteal();
    AutoR();
    SemiAutomatic();
  }

private:
  Menu *m_menu = nullptr;

  // ════════════════════════════════════════════════
  // Hitchance helpers (Xerath.cs lines 108-110)
  // ════════════════════════════════════════════════
  HitChance QHitchance() const {
    auto *miscMenu = m_menu->GetSubMenu("Misc");
    return (miscMenu && miscMenu->GetBoolValue("qslowcast", false)) ? HitChance::VeryHigh : HitChance::High;
  }

  HitChance RHitchance() const {
    auto *miscMenu = m_menu->GetSubMenu("Misc");
    return (miscMenu && miscMenu->GetBoolValue("rslowcast", false)) ? HitChance::VeryHigh : HitChance::High;
  }

  // ════════════════════════════════════════════════
  // Damage calculations (Xerath.cs lines 354-371)
  // ════════════════════════════════════════════════
  float QDamage(const AIBaseClient& target) const {
    int level = Player().GetSpellBook().GetSpell(SpellSlot::Q).Level();
    static const float baseDmg[] = { 0, 75, 115, 155, 195, 235 };
    float raw = baseDmg[std::clamp(level, 0, 5)] + 0.9f * Player().FlatMagicDamageMod();
    return Player().CalculateMagicDamage(target, raw);
  }

  float WDamage(const AIBaseClient& target) const {
    int level = Player().GetSpellBook().GetSpell(SpellSlot::W).Level();
    static const float baseDmg[] = { 0, 50, 85, 120, 155, 190 };
    float raw = baseDmg[std::clamp(level, 0, 5)] + 0.65f * Player().FlatMagicDamageMod();
    return Player().CalculateMagicDamage(target, raw);
  }

  float EDamage(const AIBaseClient& target) const {
    // Fixed 2026-04-17: C# original used R.Level for E damage which capped the
    // value at rank 3 (Xerath R has only 3 ranks). E has 5 ranks and the
    // baseDmg[] table is sized accordingly, so index with E.Level.
    int level = Player().GetSpellBook().GetSpell(SpellSlot::E).Level();
    static const float baseDmg[] = { 0, 70, 100, 130, 160, 190 };
    float raw = baseDmg[std::clamp(level, 0, 5)] + 0.45f * Player().FlatMagicDamageMod();
    return Player().CalculateMagicDamage(target, raw);
  }

  // ════════════════════════════════════════════════
  // Combo (Xerath.cs lines 239-305)
  // ════════════════════════════════════════════════
  void Combo() {
    auto *comboMenu = m_menu->GetSubMenu("ComboMenu");
    if (!comboMenu) return;

    if (!Q.IsCharging()) {
      // Not charging Q — cast W, E, then start Q charge
      // Ult check (Xerath.cs line 244)
      if (!Player().HasBuff("XerathLocusOfPower2")) {

        // W combo (Xerath.cs lines 246-257)
        if (comboMenu->GetBoolValue("ComboW", true) && W.IsReady()) {
          auto target = TargetSelector::GetTarget(W.Range, SDK::DamageType::Magical);
          if (target.IsValid() && target.IsValidTarget(W.Range)) {
            auto pred = W.GetPrediction(target);
            if (pred.Hitchance >= HitChance::VeryHigh) {
              W.Cast(pred.CastPosition);
            }
          }
        }

        // E combo (Xerath.cs lines 259-270)
        if (comboMenu->GetBoolValue("ComboE", true) && E.IsReady()) {
          auto target = TargetSelector::GetTarget(E.Range, SDK::DamageType::Magical);
          if (target.IsValid() && target.IsValidTarget(E.Range)) {
            auto pred = E.GetPrediction(target);
            if (pred.Hitchance >= HitChance::VeryHigh) {
              E.Cast(pred.CastPosition);
            }
          }
        }

        // Q combo — start charging (Xerath.cs lines 272-287)
        if (comboMenu->GetBoolValue("ComboQ", true) && Q.IsReady()) {
          auto target = TargetSelector::GetTarget(Q.ChargedMaxRange, SDK::DamageType::Magical);
          if (target.IsValid() && target.IsValidTarget(Q.ChargedMaxRange)) {
            // slow buff = more hitchance || target too far for W (Xerath.cs line 278)
            if (!W.IsReady() || target.DistanceToPlayer() > 850.0f) {
              auto pred = Q.GetPrediction(target);
              if (pred.Hitchance >= HitChance::High) {
                Q.StartCharging();
              }
            }
          }
        }
      }
    } else {
      // Q is charging — shoot when ready (Xerath.cs lines 290-304)
      if (comboMenu->GetBoolValue("ComboQ", true) && Q.IsReady() && Q.IsCharging()) {
        auto target = TargetSelector::GetTarget(Q.GetRange(), SDK::DamageType::Magical);
        if (target.IsValid() && target.IsValidTarget(Q.GetRange())) {
          auto pred = Q.GetPrediction(target);
          if (pred.Hitchance >= QHitchance()) {
            Q.ShootChargedSpell(pred.CastPosition);
          }
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // AutoR (Xerath.cs lines 307-332)
  // ════════════════════════════════════════════════
  void AutoR() {
    if (!Player().HasBuff("XerathLocusOfPower2") || Q.IsCharging()) return;

    auto *ultiMenu = m_menu->GetSubMenu("Ulti");
    if (!ultiMenu) return;

    // Find target (Xerath.cs line 313)
    auto target = TargetSelector::GetTarget(R.Range, SDK::DamageType::Magical);

    // Near Mouse filter (Xerath.cs lines 315-319)
    if (ultiMenu->GetBoolValue("NearMouse", true) && ultiMenu->GetSliderValue("MouseZone", 600) > 0) {
      float mouseZone = static_cast<float>(ultiMenu->GetSliderValue("MouseZone", 600));
      Vector3 cursorPos = Game::CursorPos();

      // Find closest valid target near mouse
      AIHeroClient bestTarget;
      float bestDist = FLT_MAX;
      for (const auto& enemy : ObjectManager::EnemyHeroes()) {
        if (!enemy.IsValid() || !enemy.IsValidTarget(R.Range)) continue;
        float distToMouse = enemy.Position().Distance(cursorPos);
        if (distToMouse <= mouseZone && distToMouse < bestDist) {
          bestDist = distToMouse;
          bestTarget = enemy;
        }
      }
      if (bestTarget.IsValid()) {
        target = bestTarget;
      } else {
        return; // No valid target near mouse
      }
    }

    // R cast (Xerath.cs lines 320-329)
    if (target.IsValid() && target.IsValidTarget(R.Range)) {
      if (ultiMenu->GetKeyBindValue("RKey")) {
        auto pred = R.GetPrediction(target);
        if (pred.Hitchance >= RHitchance()) {
          R.Cast(pred.CastPosition);
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // SemiAutomatic (Xerath.cs lines 333-352)
  // ════════════════════════════════════════════════
  void SemiAutomatic() {
    auto *semiMenu = m_menu->GetSubMenu("Semi");
    if (!semiMenu) return;

    // Semi W (Xerath.cs lines 335-342)
    if (semiMenu->GetKeyBindValue("WKey") && W.IsReady()) {
      auto target = TargetSelector::GetTarget(W.Range, SDK::DamageType::Magical);
      if (target.IsValid() && target.IsValidTarget(W.Range)) {
        W.CastPredicted(target, HitChance::High);
      }
    }

    // Semi E (Xerath.cs lines 344-351)
    if (semiMenu->GetKeyBindValue("EKey") && E.IsReady()) {
      auto target = TargetSelector::GetTarget(E.Range, SDK::DamageType::Magical);
      if (target.IsValid() && target.IsValidTarget(E.Range)) {
        E.CastPredicted(target, HitChance::High);
      }
    }
  }

  // ════════════════════════════════════════════════
  // KillSteal (Xerath.cs lines 373-442)
  // ════════════════════════════════════════════════
  void KillSteal() {
    if (Player().HasBuff("XerathLocusOfPower2") || Q.IsCharging()) return;

    auto *ksMenu = m_menu->GetSubMenu("KillStealMenu");
    if (!ksMenu) return;

    bool ksQ = ksMenu->GetBoolValue("KsQ", true);
    bool ksW = ksMenu->GetBoolValue("KsW", true);
    bool ksE = ksMenu->GetBoolValue("KsE", true);

    for (const auto& target : ObjectManager::EnemyHeroes()) {
      if (!target.IsValidTarget(W.Range)) continue;
      // Invulnerability checks (Xerath.cs line 382)
      // Invulnerability buffs (HasBuff uses lstrcmpiA — case-insensitive but
      // whitespace/exact-token sensitive). Real internal names have no spaces.
      if (target.HasBuff("JudicatorIntervention") ||  // Kayle R
          target.HasBuff("KindredRNoDeathBuff") ||    // Kindred R (Lamb's Respite)
          target.HasBuff("UndyingRage")) continue;    // Tryndamere R (fixed: was "Undying Rage")

      float effectiveHP = target.Health() + target.AllShield();

      // Q KS (Xerath.cs lines 384-420)
      if (ksQ && Q.IsReady()) {
        if (Player().Distance(target) > 150.0f) {
          if (effectiveHP <= QDamage(target)) {
            // Not charging → start charge (Xerath.cs lines 392-404)
            if (!Q.IsCharging()) {
              auto t1 = TargetSelector::GetTarget(Q.ChargedMaxRange, SDK::DamageType::Magical);
              if (t1.IsValid() && t1.IsValidTarget(Q.ChargedMaxRange)) {
                if (!W.IsReady() || t1.DistanceToPlayer() > 850.0f) {
                  auto pred = Q.GetPrediction(t1);
                  if (pred.Hitchance >= HitChance::High) {
                    Q.StartCharging();
                  }
                }
              }
            } else {
              // Already charging → shoot (Xerath.cs lines 407-416)
              auto t2 = TargetSelector::GetTarget(Q.GetRange(), SDK::DamageType::Magical);
              if (t2.IsValid() && t2.IsValidTarget(Q.GetRange())) {
                auto pred = Q.GetPrediction(t2);
                if (pred.Hitchance >= QHitchance()) {
                  Q.ShootChargedSpell(pred.CastPosition);
                }
              }
            }
          }
        }
      }

      // W KS (Xerath.cs lines 421-430)
      if (ksW && W.IsReady()) {
        if (effectiveHP <= WDamage(target)) {
          W.CastPredicted(target, HitChance::High);
        }
      }

      // E KS (Xerath.cs lines 431-440)
      if (ksE && E.IsReady() && target.IsValidTarget(500.0f)) {
        if (effectiveHP <= EDamage(target)) {
          E.CastPredicted(target, HitChance::High);
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // Harass (Xerath.cs lines 444-513)
  // ════════════════════════════════════════════════
  void Harass() {
    auto *harassMenu = m_menu->GetSubMenu("HarassMenu");
    if (!harassMenu) return;

    if (!Q.IsCharging()) {
      // Ult + mana check (Xerath.cs lines 449-450)
      if (!Player().HasBuff("XerathLocusOfPower2") &&
          Player().ManaPercent() >= static_cast<float>(harassMenu->GetSliderValue("Mana", 50))) {

        // W harass (Xerath.cs lines 452-464)
        if (harassMenu->GetBoolValue("HarassW", true) && W.IsReady()) {
          auto target = TargetSelector::GetTarget(W.Range, SDK::DamageType::Magical);
          if (target.IsValid() && target.IsValidTarget(W.Range)) {
            auto pred = W.GetPrediction(target);
            if (pred.Hitchance >= HitChance::VeryHigh) {
              W.Cast(pred.CastPosition);
              return;
            }
          }
        }

        // E harass (Xerath.cs lines 466-478)
        if (harassMenu->GetBoolValue("HarassE", true) && E.IsReady()) {
          auto target = TargetSelector::GetTarget(E.Range, SDK::DamageType::Magical);
          if (target.IsValid() && target.IsValidTarget(E.Range)) {
            auto pred = E.GetPrediction(target);
            if (pred.Hitchance >= HitChance::VeryHigh) {
              E.Cast(pred.CastPosition);
              return;
            }
          }
        }

        // Q harass — start charging (Xerath.cs lines 480-495)
        if (harassMenu->GetBoolValue("HarassQ", true) && Q.IsReady()) {
          auto target = TargetSelector::GetTarget(Q.ChargedMaxRange, SDK::DamageType::Magical);
          if (target.IsValid() && target.IsValidTarget(Q.ChargedMaxRange)) {
            if (!W.IsReady() || target.DistanceToPlayer() > 850.0f) {
              auto pred = Q.GetPrediction(target);
              if (pred.Hitchance >= HitChance::High) {
                Q.StartCharging();
              }
            }
          }
        }
      }
    } else {
      // Q is charging — ignore mana, shoot (Xerath.cs lines 498-513)
      if (harassMenu->GetBoolValue("HarassQ", true) && Q.IsReady() && Q.IsCharging()) {
        auto target = TargetSelector::GetTarget(Q.GetRange(), SDK::DamageType::Magical);
        if (target.IsValid() && target.IsValidTarget(Q.GetRange())) {
          auto pred = Q.GetPrediction(target);
          if (pred.Hitchance >= QHitchance()) {
            Q.ShootChargedSpell(pred.CastPosition);
          }
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // LaneClear (Xerath.cs lines 515-573)
  // ════════════════════════════════════════════════
  void LaneClear() {
    auto *lcMenu = m_menu->GetSubMenu("LaneClearMenu");
    if (!lcMenu) return;

    if (!Q.IsCharging()) {
      // Ult + mana check (Xerath.cs lines 520-521)
      if (!Player().HasBuff("XerathLocusOfPower2") &&
          Player().ManaPercent() >= static_cast<float>(lcMenu->GetSliderValue("ManaLC", 60))) {

        // W lane clear (Xerath.cs lines 523-537)
        if (lcMenu->GetBoolValue("LaneW", true) && W.IsReady()) {
          int minW = lcMenu->GetSliderValue("MinW", 3);
          auto farmResult = GetCircularFarmLocation(W.Range, W.Width);
          if (farmResult.second >= minW) {
            W.Cast(farmResult.first);
            return;
          }
        }

        // Q lane clear — start charging (Xerath.cs lines 539-552)
        if (lcMenu->GetBoolValue("LaneQ", true) && Q.IsReady()) {
          int minQ = lcMenu->GetSliderValue("MinQ", 3);
          auto farmResult = GetLineFarmLocation(Q.ChargedMaxRange, Q.Width);
          if (farmResult.second >= minQ) {
            Q.StartCharging();
          }
        }
      }
    } else {
      // Q is charging — ignore mana, shoot (Xerath.cs lines 555-572)
      if (lcMenu->GetBoolValue("LaneQ", true) && Q.IsReady() && Q.IsCharging()) {
        int minQ = lcMenu->GetSliderValue("MinQ", 3);
        auto farmResult = GetLineFarmLocation(Q.GetRange(), Q.Width);
        if (farmResult.second >= minQ) {
          Q.ShootChargedSpell(farmResult.first);
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // JungleClear (Xerath.cs lines 575-649)
  // ════════════════════════════════════════════════
  void JungleClear() {
    auto *jgMenu = m_menu->GetSubMenu("JungleClearMenu");
    if (!jgMenu) return;

    if (!Q.IsCharging()) {
      // Ult + mana check (Xerath.cs lines 580-581)
      if (!Player().HasBuff("XerathLocusOfPower2") &&
          Player().ManaPercent() >= static_cast<float>(jgMenu->GetSliderValue("ManaJG", 40))) {

        // W jungle (Xerath.cs lines 583-598)
        if (jgMenu->GetBoolValue("WJungle", true) && W.IsReady()) {
          auto mob = GetHighestHPJungleMob(W.Range);
          if (mob.IsValid() && mob.IsValidTarget(W.Range)) {
            auto pred = W.GetPrediction(mob);
            if (pred.Hitchance >= HitChance::High) {
              W.Cast(pred.CastPosition);
              return;
            }
          }
        }

        // E jungle (Xerath.cs lines 600-615)
        if (jgMenu->GetBoolValue("EJungle", true) && E.IsReady()) {
          auto mob = GetHighestHPJungleMob(E.Range);
          if (mob.IsValid() && mob.IsValidTarget(E.Range)) {
            auto pred = E.GetPrediction(mob);
            if (pred.Hitchance >= HitChance::High) {
              E.Cast(pred.CastPosition);
              return;
            }
          }
        }

        // Q jungle — start charging (Xerath.cs lines 617-627)
        if (jgMenu->GetBoolValue("QJungle", true) && Q.IsReady()) {
          auto mob = GetHighestHPJungleMob(Q.ChargedMaxRange);
          if (mob.IsValid() && mob.IsValidTarget(Q.ChargedMaxRange)) {
            Q.StartCharging();
          }
        }
      }
    } else {
      // Q is charging — shoot at jungle (Xerath.cs lines 630-648)
      if (jgMenu->GetBoolValue("QJungle", true) && Q.IsReady() && Q.IsCharging()) {
        auto mob = GetHighestHPJungleMob(Q.ChargedMaxRange);
        if (mob.IsValid() && mob.IsValidTarget(Q.GetRange())) {
          auto pred = Q.GetPrediction(mob);
          if (pred.Hitchance >= HitChance::High) {
            Q.ShootChargedSpell(pred.CastPosition);
          }
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // Helper: Get highest HP jungle mob in range
  // (Xerath.cs uses GameObjects.Jungle.OrderByDescending(x => x.MaxHealth).FirstOrDefault())
  // ════════════════════════════════════════════════
  AIMinionClient GetHighestHPJungleMob(float range) const {
    AIMinionClient bestMob;
    float bestHP = 0.0f;
    for (const auto& mob : ObjectManager::JungleMinions()) {
      if (!mob.IsValid() || !mob.IsValidTarget(range)) continue;
      if (mob.MaxHealth() > bestHP) {
        bestHP = mob.MaxHealth();
        bestMob = mob;
      }
    }
    return bestMob;
  }

  // ════════════════════════════════════════════════
  // Helper: GetCircularFarmLocation
  // (Xerath.cs: W.GetCircularFarmLocation(minions))
  // Returns {bestPosition, minionsHit}
  // ════════════════════════════════════════════════
  std::pair<Vector3, int> GetCircularFarmLocation(float range, float radius) const {
    Vector3 bestPos = {};
    int bestCount = 0;

    auto minions = ObjectManager::EnemyMinions();

    for (const auto& minion : minions) {
      if (!minion.IsValid() || !minion.IsValidTarget(range)) continue;

      int count = 0;
      for (const auto& other : minions) {
        if (!other.IsValid() || !other.IsValidTarget(range)) continue;
        if (other.Position().Distance(minion.Position()) <= radius) {
          count++;
        }
      }

      if (count > bestCount) {
        bestCount = count;
        bestPos = minion.Position();
      }
    }

    return { bestPos, bestCount };
  }

  // ════════════════════════════════════════════════
  // Helper: GetLineFarmLocation
  // (Xerath.cs: Q.GetLineFarmLocation(minions))
  // Returns {bestPosition, minionsHit}
  // ════════════════════════════════════════════════
  std::pair<Vector3, int> GetLineFarmLocation(float range, float width) const {
    Vector3 bestPos = {};
    int bestCount = 0;
    auto playerPos = Player().Position();

    auto minions = ObjectManager::EnemyMinions();

    for (const auto& minion : minions) {
      if (!minion.IsValid() || !minion.IsValidTarget(range)) continue;

      Vector3 endPos = minion.Position();
      Vector3 dir = endPos - playerPos;
      float len = dir.Length2D();
      if (len < 1.0f) continue;

      // Normalize direction
      float invLen = 1.0f / len;
      Vector3 normDir = { dir.x * invLen, dir.y, dir.z * invLen };

      int count = 0;
      for (const auto& other : minions) {
        if (!other.IsValid() || !other.IsValidTarget(range)) continue;

        // Project minion onto the line
        Vector3 toMinion = other.Position() - playerPos;
        float proj = toMinion.x * normDir.x + toMinion.z * normDir.z;
        if (proj < 0 || proj > range) continue;

        // Perpendicular distance
        float perpX = toMinion.x - proj * normDir.x;
        float perpZ = toMinion.z - proj * normDir.z;
        float perpDist = sqrtf(perpX * perpX + perpZ * perpZ);

        if (perpDist <= width * 0.5f) {
          count++;
        }
      }

      if (count > bestCount) {
        bestCount = count;
        bestPos = endPos;
      }
    }

    return { bestPos, bestCount };
  }
};

} // namespace Plugins
