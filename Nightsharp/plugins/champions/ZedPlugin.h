#pragma once

// ═══════════════════════════════════════════════════════
// NightSharp Zed Plugin — Full port from Zed.cs (7UPAIO)
// Logic: 100% giữ nguyên bản gốc
// ═══════════════════════════════════════════════════════

#include "../IPlugin.h"
#include "../PluginSdkCompat.h"
#include "menu/MenuUI.h"
#include "sdk/Core/Game.h"
#include "sdk/SDK.h"
#include "sdk/Wrappers/Damages/Damage.h"
#include "sdk/Wrappers/Orbwalking/Orbwalker.h"
#include "sdk/Wrappers/Spells/Spell.h"
#include "sdk/Wrappers/TargetSelector/TargetSelector.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins {

using namespace SDK;
using namespace SDK::MenuUI;

class ZedPlugin : public IPlugin {
public:
  const char *GetName()       const override { return "Zed"; }
  const char *GetInternalId() const override { return "champion_zed"; }
  const char *GetAuthor()     const override { return "7UP / NightSharp"; }
  PluginCategory GetCategory() const override { return PluginCategory::Champion; }
  bool AutoLoadByDefault()    const override { return false; }

  bool CanLoad() const override {
    return Player().IsValid() && Player().CharacterName() == "Zed";
  }

  Spell Q, W, E, R;

  // State (Zed.cs lines 20-27)
  Vector3 linepos = {};
  int clockon = 0;
  int countults = 0;
  int countdanger = 0;
  int ticktock = 0;
  Vector3 rpos = {};
  int shadowdelay = 0;
  static constexpr int delayw = 500;

  // DangerDB (Zed.cs lines 596-629)
  static constexpr const char* DangerousList[] = {
    "AhriSeduce", "CurseoftheSadMummy", "InfernalGuardian", "EnchantedCrystalArrow",
    "AzirR", "BrandWildfire", "CassiopeiaPetrifyingGaze", "DariusExecute",
    "DravenRCast", "EvelynnR", "EzrealTrueshotBarrage", "Terrify",
    "GalioIdolOfDurand", "GarenR", "GravesChargeShot", "HecarimUlt",
    "LissandraR", "LuxMaliceCannon", "UFSlash", "AlZaharNetherGrasp",
    "OrianaDetonateCommand", "LeonaSolarFlare", "SejuaniGlacialPrisonStart",
    "SonaCrescendo", "VarusR", "GragasR", "GnarR", "FizzMarinerDoom", "SyndraR"
  };
  static constexpr int DangerousListSize = 29;

  bool IsDangerousSpell(const std::string& name) const {
    for (int i = 0; i < DangerousListSize; i++) {
      if (name.find(DangerousList[i]) != std::string::npos)
        return true;
    }
    return false;
  }

  // Shadow/Ult stage enums (Zed.cs lines 582-594)
  enum class UltCastStage { First, Second, Cooldown };
  enum class ShadowCastStage { First, Second, Cooldown };

  UltCastStage GetUltStage() const {
    if (!R.IsReady()) return UltCastStage::Cooldown;
    // SpellSlot::Name() not available (game stores names in global hash table).
    // Use buff + shadow detection:
    // R Second = ult active (has death mark on someone OR R shadow exists)
    if (Player().HasBuff("zedulttargetmark") || Player().HasBuff("ZedR2") ||
        Player().HasBuff("zedrdeathmark") || GetRShadow().IsValid())
      return UltCastStage::Second;
    return UltCastStage::First;
  }

  ShadowCastStage GetShadowStage() const {
    if (!W.IsReady()) return ShadowCastStage::Cooldown;
    // W Second = shadow already placed (shadow minion exists)
    if (Player().HasBuff("ZedWHandler") || Player().HasBuff("zedwshadowbuff") ||
        Player().HasBuff("ZedW2") || GetWShadow().IsValid())
      return ShadowCastStage::Second;
    return ShadowCastStage::First;
  }

  // Shadow getters (Zed.cs lines 33-49)
  AIMinionClient GetWShadow() const {
    for (const auto& minion : ObjectManager::AllyMinions()) {
      if (minion.IsValid() && minion.IsVisible() &&
          minion.CharacterName() == "Shadow" &&
          minion.Position().Distance(rpos) > 50.0f)
        return minion;
    }
    return AIMinionClient();
  }

  AIMinionClient GetRShadow() const {
    for (const auto& minion : ObjectManager::AllyMinions()) {
      if (minion.IsValid() && minion.IsVisible() &&
          minion.CharacterName() == "Shadow" &&
          minion.Position().Distance(rpos) < 50.0f)
        return minion;
    }
    return AIMinionClient();
  }

  // GetEnemy (Zed.cs lines 29-31)
  AIHeroClient GetEnemy() const {
    auto selected = TargetSelector::GetSelectedTarget();
    if (selected.IsValid()) return selected;
    return TargetSelector::GetTarget(1400.0f, SDK::DamageType::Magical);
  }

  void OnLoad() override {
    if (m_menu) return;

    // Spell definitions (Zed.cs lines 78-82)
    Q = Spell(SpellSlot::Q, 925.0f);
    W = Spell(SpellSlot::W, 700.0f);
    E = Spell(SpellSlot::E, 270.0f);
    R = Spell(SpellSlot::R, 650.0f);

    Q.SetSkillshot(0.25f, 47.0f, 1700.0f, false, SpellType::Line);

    m_menu = Menu::Create("ZedRoot", "[NightSharp] Zed");

    // Combo Menu (Zed.cs lines 91-96)
    auto *comboMenu = m_menu->AddSubMenu("Combo", "Combo");
    comboMenu->Add<MenuBool>("UseWC", "Use W (also gap close)", true);
    comboMenu->Add<MenuBool>("UseIgnitecombo", "Use Ignite(rush for it)", true);
    comboMenu->Add<MenuBool>("UseUlt", "Use Ultimate", true);
    comboMenu->Add<MenuKeyBind>("ActiveCombo", "Combo!", VK_SPACE, KeyBindType::Press);
    comboMenu->Add<MenuKeyBind>("TheLine", "The Line Combo", 'T', KeyBindType::Press);

    // Harass Menu (Zed.cs lines 97-102)
    auto *harassMenu = m_menu->AddSubMenu("Harass", "Harass");
    harassMenu->Add<MenuKeyBind>("longhar", "Long Poke (toggle)", 'U', KeyBindType::Toggle);
    harassMenu->Add<MenuBool>("UseItemsharass", "Use Tiamat/Hydra", true);
    harassMenu->Add<MenuBool>("UseWH", "Use W", true);
    harassMenu->Add<MenuKeyBind>("ActiveHarass", "Harass!", 'C', KeyBindType::Press);

    // Items Menu (Zed.cs lines 103-118)
    auto *itemsMenu = m_menu->AddSubMenu("items", "Items");
    auto *offMenu = itemsMenu->AddSubMenu("Offensive", "Offensive");
    offMenu->Add<MenuBool>("Youmuu", "Use Youmuu's", true);
    offMenu->Add<MenuBool>("Tiamat", "Use Tiamat", true);
    offMenu->Add<MenuBool>("Hydra", "Use Hydra", true);
    offMenu->Add<MenuBool>("Bilge", "Use Bilge", true);
    offMenu->Add<MenuSlider>("BilgeEnemyhp", "If Enemy Hp <", 85, 1, 100);
    offMenu->Add<MenuSlider>("Bilgemyhp", "Or your Hp < ", 85, 1, 100);
    offMenu->Add<MenuBool>("Blade", "Use Blade", true);
    offMenu->Add<MenuSlider>("BladeEnemyhp", "If Enemy Hp <", 85, 1, 100);
    offMenu->Add<MenuSlider>("Blademyhp", "Or Your Hp <", 85, 1, 100);

    auto *defMenu = itemsMenu->AddSubMenu("Deffensive", "Defensive");
    defMenu->Add<MenuBool>("Omen", "Use Randuin Omen", true);
    defMenu->Add<MenuSlider>("Omenenemys", "Randuin if enemys>", 2, 1, 5);

    // Farm Menu (Zed.cs lines 119-140)
    auto *farmMenu = m_menu->AddSubMenu("Farm", "Farm");
    auto *laneMenu = farmMenu->AddSubMenu("LaneFarm", "LaneFarm");
    laneMenu->Add<MenuBool>("UseItemslane", "Use Hydra/Tiamat", true);
    laneMenu->Add<MenuBool>("UseQL", "Q LaneClear", true);
    laneMenu->Add<MenuBool>("UseEL", "E LaneClear", true);
    laneMenu->Add<MenuSlider>("Energylane", "Energy Lane% >", 45, 1, 100);

    auto *lastMenu = farmMenu->AddSubMenu("LastHit", "LastHit");
    lastMenu->Add<MenuBool>("UseQLH", "Q LastHit", true);
    lastMenu->Add<MenuBool>("UseELH", "E LastHit", true);
    lastMenu->Add<MenuSlider>("Energylast", "Energy lasthit% >", 85, 1, 100);

    auto *jgMenu = farmMenu->AddSubMenu("Jungle", "Jungle");
    jgMenu->Add<MenuBool>("UseItemsjungle", "Use Hydra/Tiamat", true);
    jgMenu->Add<MenuBool>("UseQJ", "Q Jungle", true);
    jgMenu->Add<MenuBool>("UseWJ", "W Jungle", true);
    jgMenu->Add<MenuBool>("UseEJ", "E Jungle", true);
    jgMenu->Add<MenuSlider>("Energyjungle", "Energy Jungle% >", 85, 1, 100);

    // Misc Menu (Zed.cs lines 141-152)
    auto *miscMenu = m_menu->AddSubMenu("Misc", "Misc");
    miscMenu->Add<MenuBool>("UseIgnitekill", "Use Ignite KillSteal", true);
    miscMenu->Add<MenuBool>("UseQM", "Use Q KillSteal", true);
    miscMenu->Add<MenuBool>("UseEM", "Use E KillSteal", true);
    miscMenu->Add<MenuBool>("AutoE", "Auto E", true);
    miscMenu->Add<MenuBool>("rdodge", "R Dodge Dangerous", true);

    // Per-enemy dangerous R toggle (Zed.cs lines 147-152)
    for (const auto& e : ObjectManager::EnemyHeroes()) {
      if (!e.IsValid()) continue;
      std::string rName = e.GetSpellBook().GetSpell(SpellSlot::R).Name();
      if (IsDangerousSpell(rName)) {
        std::string name = e.CharacterName();
        miscMenu->Add<MenuBool>(("ds" + name).c_str(), rName.c_str(), true);
      }
    }
  }

  void OnUnload() override {
    if (!m_menu) return;
    Menu::Remove("ZedRoot");
    m_menu = nullptr;
  }

  Menu *GetMenuRoot() override { return m_menu; }

  // ════════════════════════════════════════════════
  // OnSpellCast (Zed.cs lines 171-191)
  // ════════════════════════════════════════════════
  void OnProcessSpellCast(const AIBaseClient& sender,
      const Events::SpellCast::ProcessSpellCastEventArgs& args) override {
    if (!m_menu) return;
    auto *miscMenu = m_menu->GetSubMenu("Misc");

    // Track R cast tick
    if (sender.IsMe() && args.SpellName == "zedult") {
      ticktock = Game::TickCount() + 200;
    }
    // R dodge on dangerous spell
    else if (sender.IsEnemy() && sender.IsHero()) {
      if (miscMenu && miscMenu->GetBoolValue("rdodge", true)) {
        if ((R.IsReady() || Player().GetSpellBook().GetSpell(SpellSlot::R).Name() == "ZedR2")) {
          std::string senderName = sender.CharacterName();
          if (miscMenu->GetBoolValue(("ds" + senderName).c_str(), true)) {
            if (IsDangerousSpell(args.SpellName)) {
              float dist = sender.Distance(Player());
              if (dist < 650.0f || Player().Position().Distance(args.End) <= 250.0f) {
                if (args.SpellName == "SyndraR") {
                  clockon = Game::TickCount() + 150;
                  countdanger++;
                } else {
                  auto target = TargetSelector::GetTarget(640.0f, SDK::DamageType::Magical);
                  if (target.IsValid()) R.Cast(target);
                }
              }
            }
          }
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // OnUpdate (Zed.cs lines 194-217)
  // ════════════════════════════════════════════════
  void OnUpdate() override {
    if (!Player().IsValid() || !m_menu) return;
    if (Player().IsDead() || Player().IsRecalling() || Player().IsWindingUp()) return;

    auto mode = Orbwalker::GetMode();
    auto *comboMenu = m_menu->GetSubMenu("Combo");
    auto *miscMenu = m_menu->GetSubMenu("Misc");

    if (mode == OrbwalkerMode::Combo) {
      Combo(GetEnemy());
    }

    // TheLine (Zed.cs line 197)
    if (comboMenu && comboMenu->GetKeyBindValue("TheLine")) {
      TheLine(GetEnemy());
    }

    if (mode == OrbwalkerMode::Harass) {
      Harass(GetEnemy());
    }

    if (mode == OrbwalkerMode::Clear) {
      Laneclear();
      JungleClear();
    }

    if (mode == OrbwalkerMode::LastHit) {
      LastHitMode();
    }

    // Auto E (Zed.cs line 202)
    if (miscMenu && miscMenu->GetBoolValue("AutoE", true)) {
      CastE();
    }

    // Syndra R delayed dodge (Zed.cs lines 203-207)
    if (Game::TickCount() >= clockon && countdanger > countults) {
      auto target = TargetSelector::GetTarget(640.0f, SDK::DamageType::Magical);
      if (target.IsValid()) R.Cast(target);
      countults++;
    }

    // Track R shadow position (Zed.cs lines 209-214)
    // Simplified: update rpos from any ally shadow when R was just cast
    if (Game::TickCount() < ticktock + 500) {
      for (const auto& minion : ObjectManager::AllyMinions()) {
        if (minion.IsValid() && minion.IsVisible() && minion.CharacterName() == "Shadow") {
          rpos = minion.Position();
          break;
        }
      }
    }

    KillSteal();
  }

private:
  Menu *m_menu = nullptr;

  // ════════════════════════════════════════════════
  // ComboDamage (Zed.cs lines 219-238)
  // ════════════════════════════════════════════════
  float ComboDamage(const AIBaseClient& enemy) const {
    if (!enemy.IsValid()) return 0;
    float damage = 0;

    if (Q.IsReady()) damage += Q.GetDamage(enemy);
    if (W.IsReady() && Q.IsReady())
      damage += Q.GetDamage(enemy) / 2.0f;
    if (E.IsReady()) damage += E.GetDamage(enemy);
    if (R.IsReady()) damage += R.GetDamage(enemy);

    // R passive amplification (Zed.cs lines 235-236)
    int rLvl = Player().GetSpellBook().GetSpell(SpellSlot::R).Level();
    damage += static_cast<float>((rLvl * 0.15 + 0.05) * damage);

    return damage;
  }

  // ════════════════════════════════════════════════
  // Combo (Zed.cs lines 240-277)
  // ════════════════════════════════════════════════
  void Combo(const AIHeroClient& target) {
    if (!target.IsValid()) return;
    auto *comboMenu = m_menu->GetSubMenu("Combo");
    if (!comboMenu) return;

    float overkill = Q.GetDamage(target) +
                     E.GetDamage(target) +
                     Player().GetAutoAttackDamage(target) * 2.0f;

    auto wSpell = Player().GetSpellBook().GetSpell(SpellSlot::W);

    // R check (Zed.cs lines 247-260)
    if (comboMenu->GetBoolValue("UseUlt", true) && GetUltStage() == UltCastStage::First) {
      bool shouldUlt = overkill < target.Health();
      bool wNotReady = !W.IsReady() && wSpell.Cooldown() > 2.0f &&
                       Q.GetDamage(target) < target.Health() &&
                       target.Distance(Player()) > 400.0f;

      if (shouldUlt || wNotReady) {
        // Gap close with W first if too far
        if ((target.Distance(Player()) > 700.0f && target.MoveSpeed() > Player().MoveSpeed()) ||
            target.Distance(Player()) > 800.0f) {
          CastW(target);
          W.Cast();
        }
        R.Cast(target);
        return;
      }
    }

    // Normal combo (Zed.cs lines 261-276)
    // Ignite
    if (comboMenu->GetBoolValue("UseIgnitecombo", true)) {
      if (ComboDamage(target) > target.Health() || target.HasBuff("zedulttargetmark")) {
        Compat::CastIgnite(target);
      }
    }

    // W gap close (Zed.cs lines 267-269)
    if (GetShadowStage() == ShadowCastStage::First && comboMenu->GetBoolValue("UseWC", true) &&
        target.Distance(Player()) > 400.0f && target.Distance(Player()) < 1300.0f) {
      CastW(target);
    }

    // W swap (Zed.cs lines 270-272)
    auto wShadow = GetWShadow();
    if (GetShadowStage() == ShadowCastStage::Second && comboMenu->GetBoolValue("UseWC", true) &&
        wShadow.IsValid() &&
        target.Distance(wShadow.Position()) < target.Distance(Player())) {
      W.Cast();
    }

    CastE();
    CastQ(target);
  }

  // ════════════════════════════════════════════════
  // TheLine (Zed.cs lines 279-310)
  // ════════════════════════════════════════════════
  void TheLine(const AIHeroClient& target) {
    if (!target.IsValid()) {
      Player().IssueOrder(GameObjectOrder::MoveTo, Game::CursorPos());
      return;
    }

    Player().IssueOrder(GameObjectOrder::AttackUnit, target);
    if (!R.IsReady() || target.Distance(Player()) >= 640.0f) return;

    if (GetUltStage() == UltCastStage::First) R.Cast(target);

    linepos = target.Position().Extend(Player().Position(), -500.0f);

    if (GetShadowStage() == ShadowCastStage::First && GetUltStage() == UltCastStage::Second) {
      W.Cast(linepos);
      CastE();
      CastQ(target);

      auto *comboMenu = m_menu->GetSubMenu("Combo");
      if (comboMenu && comboMenu->GetBoolValue("UseIgnitecombo", true)) {
        Compat::CastIgnite(target);
      }
    }

    auto wShadow = GetWShadow();
    if (wShadow.IsValid() && GetUltStage() == UltCastStage::Second &&
        target.Distance(Player()) > 250.0f &&
        target.Distance(wShadow.Position()) < target.Distance(Player())) {
      W.Cast();
    }
  }

  // ════════════════════════════════════════════════
  // Harass (Zed.cs lines 317-341)
  // ════════════════════════════════════════════════
  void Harass(const AIHeroClient& target) {
    if (!target.IsValid()) return;
    auto *harassMenu = m_menu->GetSubMenu("Harass");
    if (!harassMenu) return;

    // Long poke (Zed.cs lines 322-326)
    if (target.IsValidTarget() && harassMenu->GetKeyBindValue("longhar") && W.IsReady() && Q.IsReady() &&
        Player().Mana() > W.Instance().ManaCost() + Q.Instance().ManaCost() &&
        target.Distance(Player()) > 850.0f && target.Distance(Player()) < 1400.0f) {
      CastW(target);
    }

    // Q (Zed.cs lines 327-330)
    auto wShadow = GetWShadow();
    if (target.IsValidTarget() &&
        (GetShadowStage() == ShadowCastStage::Second || GetShadowStage() == ShadowCastStage::Cooldown ||
         !harassMenu->GetBoolValue("UseWH", true)) && Q.IsReady()) {

      bool inRange = target.Distance(Player()) <= 900.0f;
      bool shadowInRange = wShadow.IsValid() && target.Distance(wShadow.Position()) <= 900.0f;
      if (inRange || shadowInRange) CastQ(target);
    }

    // W for close range Q (Zed.cs lines 331-336)
    if (target.IsValidTarget() && W.IsReady() && Q.IsReady() &&
        harassMenu->GetBoolValue("UseWH", true) &&
        Player().Mana() > Q.Instance().ManaCost() + W.Instance().ManaCost()) {
      if (target.Distance(Player()) < 750.0f) CastW(target);
    }

    CastE();
  }

  // ════════════════════════════════════════════════
  // Laneclear (Zed.cs lines 343-375)
  // ════════════════════════════════════════════════
  void Laneclear() {
    auto *laneMenu = m_menu->GetSubMenu("Farm");
    if (!laneMenu) laneMenu = m_menu;
    auto *laneFarm = laneMenu->GetSubMenu("LaneFarm");
    if (!laneFarm) return;

    float energyPct = static_cast<float>(laneFarm->GetSliderValue("Energylane", 45));
    bool enoughEnergy = (Player().Mana() / Player().MaxMana() * 100.0f) >= energyPct;
    bool useQ = laneFarm->GetBoolValue("UseQL", true);
    bool useE = laneFarm->GetBoolValue("UseEL", true);

    if (Q.IsReady() && useQ && enoughEnergy) {
      auto farmResult = GetLineFarmLocation(Q.Range, Q.Width);
      if (farmResult.second >= 3) {
        Q.Cast(farmResult.first);
      } else {
        for (const auto& minion : ObjectManager::EnemyMinions()) {
          if (!minion.IsValid() || !minion.IsValidTarget(Q.Range)) continue;
          if (minion.Health() < 0.75f * Q.GetDamage(minion)) {
            Q.Cast(minion);
            break;
          }
        }
      }
    }

    if (E.IsReady() && useE && enoughEnergy) {
      int minionCount = 0;
      for (const auto& m : ObjectManager::EnemyMinions()) {
        if (m.IsValid() && m.IsValidTarget(E.Range)) minionCount++;
      }
      if (minionCount > 2) {
        E.Cast();
      } else {
        for (const auto& minion : ObjectManager::EnemyMinions()) {
          if (!minion.IsValid() || !minion.IsValidTarget(E.Range)) continue;
          if (minion.Health() < 0.75f * E.GetDamage(minion)) {
            E.Cast();
            break;
          }
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // LastHitMode (Zed.cs lines 377-391)
  // ════════════════════════════════════════════════
  void LastHitMode() {
    auto *laneMenu = m_menu->GetSubMenu("Farm");
    if (!laneMenu) return;
    auto *lastMenu = laneMenu->GetSubMenu("LastHit");
    if (!lastMenu) return;

    float energyPct = static_cast<float>(lastMenu->GetSliderValue("Energylast", 85));
    bool enoughEnergy = (Player().Mana() / Player().MaxMana() * 100.0f) >= energyPct;
    bool useQ = lastMenu->GetBoolValue("UseQLH", true);
    bool useE = lastMenu->GetBoolValue("UseELH", true);

    for (const auto& minion : ObjectManager::EnemyMinions()) {
      if (!minion.IsValid() || !minion.IsValidTarget(Q.Range)) continue;
      if (enoughEnergy && useQ && Q.IsReady() && minion.Distance(Player()) < Q.Range) {
        if (minion.Health() < 0.75f * Q.GetDamage(minion)) {
          Q.Cast(minion);
        }
      }
      if (enoughEnergy && E.IsReady() && useE && minion.Distance(Player()) < E.Range) {
        if (minion.Health() < 0.95f * E.GetDamage(minion)) {
          E.Cast();
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // JungleClear (Zed.cs lines 393-412)
  // ════════════════════════════════════════════════
  void JungleClear() {
    auto *laneMenu = m_menu->GetSubMenu("Farm");
    if (!laneMenu) return;
    auto *jgMenu = laneMenu->GetSubMenu("Jungle");
    if (!jgMenu) return;

    float energyPct = static_cast<float>(jgMenu->GetSliderValue("Energyjungle", 85));
    bool enoughEnergy = (Player().Mana() / Player().MaxMana() * 100.0f) >= energyPct;
    bool useQ = jgMenu->GetBoolValue("UseQJ", true);
    bool useW = jgMenu->GetBoolValue("UseWJ", true);
    bool useE = jgMenu->GetBoolValue("UseEJ", true);

    // Get highest HP mob
    AIMinionClient mob;
    float lowestHP = FLT_MAX;
    for (const auto& m : ObjectManager::JungleMinions()) {
      if (!m.IsValid() || !m.IsValidTarget(Q.Range)) continue;
      if (m.MaxHealth() < lowestHP) { lowestHP = m.MaxHealth(); mob = m; }
    }

    if (!mob.IsValid()) return;

    if (enoughEnergy && W.IsReady() && useW && mob.Distance(Player()) < Q.Range)
      W.Cast(mob.Position());
    if (enoughEnergy && useQ && Q.IsReady() && mob.Distance(Player()) < Q.Range)
      CastQ(mob);
    if (enoughEnergy && E.IsReady() && useE && mob.Distance(Player()) < E.Range)
      E.Cast();
  }

  // ════════════════════════════════════════════════
  // CastW (Zed.cs lines 440-448)
  // ════════════════════════════════════════════════
  void CastW(const AIBaseClient& target) {
    if (delayw >= Game::TickCount() - shadowdelay) return;
    if (GetShadowStage() != ShadowCastStage::First) return;
    if (target.HasBuff("zedulttargetmark") && GetUltStage() == UltCastStage::Cooldown) return;

    Vector3 herew = target.Position().Extend(Player().Position(), -200.0f);
    W.Cast(herew);
    shadowdelay = Game::TickCount();
  }

  // ════════════════════════════════════════════════
  // CastQ (Zed.cs lines 450-466)
  // ════════════════════════════════════════════════
  void CastQ(const AIBaseClient& target) {
    if (!Q.IsReady() || !target.IsValid()) return;

    auto wShadow = GetWShadow();
    if (wShadow.IsValid() && target.Distance(wShadow.Position()) <= 900.0f &&
        target.Distance(Player()) > 450.0f) {
      Q.From = wShadow.Position();
      auto pred = Q.GetPrediction(target);
      if (pred.Hitchance >= HitChance::Medium) Q.Cast(target);
      Q.From = Player().Position();
    } else {
      Q.From = Player().Position();
      auto pred = Q.GetPrediction(target);
      if (pred.CastPosition.Distance(Player().Position()) < 900.0f &&
          pred.Hitchance >= HitChance::Medium) {
        Q.Cast(target);
      }
    }
  }

  // ════════════════════════════════════════════════
  // CastE (Zed.cs lines 468-475)
  // ════════════════════════════════════════════════
  void CastE() {
    if (!E.IsReady()) return;
    auto wShadow = GetWShadow();
    for (const auto& hero : ObjectManager::EnemyHeroes()) {
      if (!hero.IsValid() || !hero.IsValidTarget()) continue;
      bool inPlayerRange = hero.Distance(Player()) <= E.Range;
      bool inShadowRange = wShadow.IsValid() && hero.Distance(wShadow.Position()) <= E.Range;
      if (inPlayerRange || inShadowRange) {
        E.Cast();
        return;
      }
    }
  }

  // ════════════════════════════════════════════════
  // KillSteal (Zed.cs lines 477-526)
  // ════════════════════════════════════════════════
  void KillSteal() {
    auto *miscMenu = m_menu->GetSubMenu("Misc");
    if (!miscMenu) return;

    auto target = TargetSelector::GetTarget(2000.0f, SDK::DamageType::Magical);
    if (!target.IsValid()) return;

    // Q KS (Zed.cs lines 486-517)
    if (target.IsValidTarget() && Q.IsReady() && miscMenu->GetBoolValue("UseQM", true)) {
      float qDmg = Q.GetDamage(target);
      if (qDmg > target.Health()) {
        if (Player().Distance(target) <= Q.Range) {
          Q.Cast(target);
        } else {
          auto wShadow = GetWShadow();
          if (wShadow.IsValid() && wShadow.Distance(target.Position()) <= Q.Range) {
            Q.From = wShadow.Position();
            Q.Cast(target);
            Q.From = Player().Position();
          }
          auto rShadow = GetRShadow();
          if (rShadow.IsValid() && rShadow.Distance(target.Position()) <= Q.Range) {
            Q.From = rShadow.Position();
            Q.Cast(target);
            Q.From = Player().Position();
          }
        }
      }
    }

    // E KS (Zed.cs lines 519-525)
    if (E.IsReady() && miscMenu->GetBoolValue("UseEM", true)) {
      auto t = TargetSelector::GetTarget(E.Range, SDK::DamageType::Magical);
      if (t.IsValid() && t.IsValidTarget()) {
        float eDmg = E.GetDamage(t);
        auto wShadow = GetWShadow();
        bool inRange = Player().Distance(t) <= E.Range;
        bool shadowRange = wShadow.IsValid() && wShadow.Distance(t.Position()) <= E.Range;
        if (eDmg > t.Health() && (inRange || shadowRange)) {
          E.Cast();
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // GetLineFarmLocation helper
  // ════════════════════════════════════════════════
  std::pair<Vector3, int> GetLineFarmLocation(float range, float width) const {
    Vector3 bestPos = {};
    int bestCount = 0;
    auto playerPos = Player().Position();
    auto minions = ObjectManager::EnemyMinions();

    for (const auto& m : minions) {
      if (!m.IsValid() || !m.IsValidTarget(range)) continue;
      Vector3 endPos = m.Position();
      Vector3 dir = endPos - playerPos;
      float len = dir.Length2D();
      if (len < 1.0f) continue;
      float invLen = 1.0f / len;
      Vector3 normDir = {dir.x * invLen, dir.y, dir.z * invLen};

      int count = 0;
      for (const auto& o : minions) {
        if (!o.IsValid() || !o.IsValidTarget(range)) continue;
        Vector3 toM = o.Position() - playerPos;
        float proj = toM.x * normDir.x + toM.z * normDir.z;
        if (proj < 0 || proj > range) continue;
        float perpX = toM.x - proj * normDir.x;
        float perpZ = toM.z - proj * normDir.z;
        if (sqrtf(perpX * perpX + perpZ * perpZ) <= width * 0.5f) count++;
      }
      if (count > bestCount) { bestCount = count; bestPos = endPos; }
    }
    return {bestPos, bestCount};
  }
};

} // namespace Plugins
