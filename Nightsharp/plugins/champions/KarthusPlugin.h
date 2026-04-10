#pragma once

// ═══════════════════════════════════════════════════════
// NightSharp Karthus Plugin — Full port from Karthus.cs (7UPAIO)
// Logic: 100% giữ nguyên bản gốc, thứ tự logic giữ nguyên
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
#include <cmath>
#include <string>

namespace Plugins {

using namespace SDK;
using namespace SDK::MenuUI;

class KarthusPlugin : public IPlugin {
public:
  const char *GetName()       const override { return "Karthus"; }
  const char *GetInternalId() const override { return "champion_karthus"; }
  const char *GetAuthor()     const override { return "7UP / NightSharp"; }
  PluginCategory GetCategory() const override { return PluginCategory::Champion; }
  bool AutoLoadByDefault()    const override { return false; }

  bool CanLoad() const override {
    return Player().IsValid() && Player().CharacterName() == "Karthus";
  }

  Spell Q, W, E, R;

  static constexpr float SpellQWidth = 160.0f;
  static constexpr float SpellWWidth = 160.0f;

  bool m_comboE = false;

  void OnLoad() override {
    if (m_menu) return;

    // Spell definitions (Karthus.cs lines 41-48)
    Q = Spell(SpellSlot::Q, 875.0f);
    W = Spell(SpellSlot::W, 1000.0f);
    E = Spell(SpellSlot::E, 505.0f);
    R = Spell(SpellSlot::R, 20000.0f);

    Q.SetSkillshot(0.25f, 75.0f, FLT_MAX, false, SpellType::Circle);
    W.SetSkillshot(0.50f, 70.0f, FLT_MAX, false, SpellType::Circle);
    E.SetSkillshot(1.0f, 505.0f, FLT_MAX, false, SpellType::Circle);
    R.SetSkillshot(3.0f, FLT_MAX, FLT_MAX, false, SpellType::Circle);

    m_menu = Menu::Create("KarthusRoot", "[NightSharp] Karthus");

    // Combo Menu (Karthus.cs lines 51-57)
    auto *comboMenu = m_menu->AddSubMenu("ComboMenu", "Combo Settings");
    comboMenu->Add<MenuBool>("comboQ", "Use Q", true);
    comboMenu->Add<MenuBool>("comboW", "Use W", true);
    comboMenu->Add<MenuBool>("comboE", "Use E", true);
    comboMenu->Add<MenuSlider>("comboWPercent", "Use W until Mana %", 10, 0, 100);
    comboMenu->Add<MenuSlider>("comboEPercent", "Use E until Mana %", 15, 0, 100);

    // Harass Menu (Karthus.cs lines 59-63)
    auto *harassMenu = m_menu->AddSubMenu("HarassMenu", "Harass Settings");
    harassMenu->Add<MenuBool>("harassQ", "Use Q", true);
    harassMenu->Add<MenuSlider>("harassQPercent", "Use Q until Mana %", 15, 0, 100);
    harassMenu->Add<MenuBool>("harassQLasthit", "Prioritize Last Hit", true);
    harassMenu->Add<MenuKeyBind>("harassQToggle", "Toggle Q", 'G', KeyBindType::Toggle);

    // LaneClear Menu (Karthus.cs lines 65-70)
    auto *lcMenu = m_menu->AddSubMenu("LaneClearMenu", "LaneClear Settings");
    lcMenu->Add<MenuList>("farmQ", "Use Q",
      std::vector<std::string>{"Last Hit", "Lane Clear", "Both", "No"}, 1);
    lcMenu->Add<MenuBool>("farmE", "Use E in Lane Clear", true);
    lcMenu->Add<MenuBool>("farmAA", "Use AA in Lane Clear", true);
    lcMenu->Add<MenuSlider>("farmQPercent", "Use Q until Mana %", 10, 0, 100);
    lcMenu->Add<MenuSlider>("farmEPercent", "Use E until Mana %", 20, 0, 100);

    // JungleClear Menu (Karthus.cs lines 72-75)
    auto *jgMenu = m_menu->AddSubMenu("JungleClearMenu", "JungleClear Settings");
    jgMenu->Add<MenuBool>("useQ", "Use Q", true);
    jgMenu->Add<MenuBool>("useE", "Use E", true);

    // Misc Menu (Karthus.cs lines 76-80)
    auto *miscMenu = m_menu->AddSubMenu("Misc", "Misc Settings");
    miscMenu->Add<MenuBool>("autoCast", "Auto Combo/LaneClear if dead", false);
    miscMenu->Add<MenuBool>("packetCast", "Packet Cast", false);
  }

  void OnUnload() override {
    if (!m_menu) return;
    Menu::Remove("KarthusRoot");
    m_menu = nullptr;
  }

  Menu *GetMenuRoot() override { return m_menu; }

  // ════════════════════════════════════════════════
  // OnBeforeAttack (Karthus.cs lines 531-544)
  // ════════════════════════════════════════════════
  void OnBeforeAttack(OrbwalkingActionArgs& args) override {
    if (!m_menu) return;
    auto mode = Orbwalker::GetMode();
    auto *lcMenu = m_menu->GetSubMenu("LaneClearMenu");

    if (mode == OrbwalkerMode::Combo) {
      args.Process = !Q.IsReady();
    } else if (mode == OrbwalkerMode::LastHit) {
      bool farmQ = false;
      if (lcMenu) {
        int idx = lcMenu->GetListIndex("farmQ", 1);
        farmQ = (idx == 0 || idx == 2);
      }
      args.Process = !(farmQ && Q.IsReady() &&
        GetManaPercent() >= static_cast<float>(lcMenu ? lcMenu->GetSliderValue("farmQPercent", 10) : 10));
    }
  }

  // ════════════════════════════════════════════════
  // OnUpdate (Karthus.cs lines 89-161)
  // ════════════════════════════════════════════════
  void OnUpdate() override {
    if (!Player().IsValid() || !m_menu) return;
    if (Player().IsDead() || Player().IsRecalling() || Player().IsWindingUp()) return;

    auto *harassMenu = m_menu->GetSubMenu("HarassMenu");
    auto *miscMenu = m_menu->GetSubMenu("Misc");
    auto mode = Orbwalker::GetMode();

    // Toggle Q harass (Karthus.cs lines 110-117)
    if (harassMenu &&
        Player().ManaPercent() >= static_cast<float>(harassMenu->GetSliderValue("harassQPercent", 15))) {
      if (Q.IsReady() && harassMenu->GetKeyBindValue("harassQToggle") &&
          mode != OrbwalkerMode::Combo) {
        CastQ(TargetSelector::GetTarget(Q.Range, SDK::DamageType::Magical));
      }
    }

    switch (mode) {
      case OrbwalkerMode::Combo:
        Combo();
        LogicE();
        return;
      case OrbwalkerMode::Harass:
        Harass();
        break;
      case OrbwalkerMode::Clear:
        LaneClear();
        JungleClear();
        break;
      case OrbwalkerMode::LastHit:
        LastHit();
        break;
      default:
        // Auto cast if dead/passive (Karthus.cs lines 138-142)
        if (miscMenu && miscMenu->GetBoolValue("autoCast", false)) {
          if (IsInPassiveForm()) {
            Combo();
            LaneClear();
          }
        }
        break;
    }

    // Logic EE — turn off E when no mode (Karthus.cs lines 163-169)
    LogicEE();
    // Logic R (Karthus.cs lines 379-411)
    LogicR();
  }

private:
  Menu *m_menu = nullptr;

  // ════════════════════════════════════════════════
  // Helpers (Karthus.cs lines 219-270)
  // ════════════════════════════════════════════════
  float GetManaPercent() const {
    return (Player().Mana() / Player().MaxMana()) * 100.0f;
  }

  bool IsInPassiveForm() const {
    return Compat::IsZombieLike(Player());
  }

  float GetDynamicQWidth(const AIBaseClient& target) const {
    return std::max(30.0f, (1.0f - (Player().Distance(target) / Q.Range)) * Q.Width);
  }

  float GetDynamicWWidth(const AIBaseClient& target) const {
    return std::max(70.0f, (1.0f - (Player().Distance(target) / W.Range)) * SpellWWidth);
  }

  // ════════════════════════════════════════════════
  // CastQ (Karthus.cs lines 219-235)
  // ════════════════════════════════════════════════
  void CastQ(const AIBaseClient& target, int minManaPercent = 0) {
    if (!Q.IsReady() || GetManaPercent() < static_cast<float>(minManaPercent))
      return;
    if (!target.IsValid()) return;
    Q.Width = GetDynamicQWidth(target);
    Q.Cast(target);
  }

  // ════════════════════════════════════════════════
  // CastW (Karthus.cs lines 237-245)
  // ════════════════════════════════════════════════
  void CastW(const AIBaseClient& target, int minManaPercent = 0) {
    if (!W.IsReady() || GetManaPercent() < static_cast<float>(minManaPercent))
      return;
    if (!target.IsValid()) return;
    W.Width = GetDynamicWWidth(target);
    W.Cast(target);
  }

  // ════════════════════════════════════════════════
  // Combo (Karthus.cs lines 337-357)
  // ════════════════════════════════════════════════
  void Combo() {
    auto *comboMenu = m_menu->GetSubMenu("ComboMenu");
    if (!comboMenu) return;

    // W (Karthus.cs lines 341-343)
    if (comboMenu->GetBoolValue("comboW", true)) {
      CastW(TargetSelector::GetTarget(W.Range, SDK::DamageType::Magical),
            comboMenu->GetSliderValue("comboWPercent", 10));
    }

    // Q (Karthus.cs lines 345-353)
    if (comboMenu->GetBoolValue("comboQ", true) && Q.IsReady()) {
      auto target = TargetSelector::GetTarget(Q.Range, SDK::DamageType::Magical);
      if (target.IsValid()) {
        CastQ(target);
      }
    }
  }

  // ════════════════════════════════════════════════
  // LogicE — toggle E on/off (Karthus.cs lines 359-377)
  // ════════════════════════════════════════════════
  void LogicE() {
    auto *comboMenu = m_menu->GetSubMenu("ComboMenu");
    if (!comboMenu) return;

    auto target = TargetSelector::GetTarget(E.Range, SDK::DamageType::Magical);

    if (Player().ManaPercent() > static_cast<float>(comboMenu->GetSliderValue("comboEPercent", 15)) &&
        comboMenu->GetBoolValue("comboE", true)) {
      // Turn on E if enemy in range and E is off
      if (target.IsValidTarget(E.Range) && !Player().HasBuff("KarthusDefileSelf")) {
        E.Cast();
      }
    }

    // Turn off E if no enemies in range and E is on
    if (Player().CountEnemyHeroesInRange(E.Range) == 0 && Player().HasBuff("KarthusDefileSelf")) {
      E.Cast();
    }
  }

  // ════════════════════════════════════════════════
  // LogicEE — turn off E when idle (Karthus.cs lines 163-169)
  // ════════════════════════════════════════════════
  void LogicEE() {
    if (Orbwalker::GetMode() == OrbwalkerMode::None && Player().HasBuff("KarthusDefileSelf")) {
      E.Cast();
    }
  }

  // ════════════════════════════════════════════════
  // LogicR (Karthus.cs lines 379-411)
  // ════════════════════════════════════════════════
  void LogicR() {
    if (!R.IsReady()) return;

    // Find killable target globally
    for (const auto& target : ObjectManager::EnemyHeroes()) {
      if (!target.IsValid() || target.IsDead()) continue;
      if (!target.IsValidTarget()) continue;

      float dmg = R.GetDamage(target);
      float hp = target.Health();

      if (Compat::IsProtectedFromSpell(target, SDK::DamageType::Magical, dmg)) continue;

      // Not in W range + R can kill + no enemies nearby + not under turret + no allies near target
      if (Player().Distance(target) > W.Range + 250.0f &&
          dmg >= hp &&
          Player().CountEnemyHeroesInRange(W.Range) == 0 &&
          target.CountAllyHeroesInRange(W.Range) == 0) {
        R.Cast();
        return;
      }

      // Passive form R (Karthus.cs lines 405-410)
      if (dmg >= hp &&
          Player().HasBuff("KarthusDeathDefiedBuff") &&
          target.CountAllyHeroesInRange(W.Range) == 0) {
        R.Cast();
        return;
      }
    }
  }

  // ════════════════════════════════════════════════
  // Harass (Karthus.cs lines 502-511)
  // ════════════════════════════════════════════════
  void Harass() {
    auto *harassMenu = m_menu->GetSubMenu("HarassMenu");
    if (!harassMenu) return;

    if (harassMenu->GetBoolValue("harassQLasthit", true))
      LastHit();

    if (harassMenu->GetBoolValue("harassQ", true)) {
      CastQ(TargetSelector::GetTarget(Q.Range, SDK::DamageType::Magical),
            harassMenu->GetSliderValue("harassQPercent", 15));
    }
  }

  // ════════════════════════════════════════════════
  // LaneClear (Karthus.cs lines 452-498)
  // ════════════════════════════════════════════════
  void LaneClear() {
    auto *lcMenu = m_menu->GetSubMenu("LaneClearMenu");
    if (!lcMenu) return;

    int farmQIdx = lcMenu->GetListIndex("farmQ", 1);
    bool farmQ = (farmQIdx == 1 || farmQIdx == 2);
    bool farmE = lcMenu->GetBoolValue("farmE", true);

    // Q lane clear (Karthus.cs lines 461-474)
    if (farmQ && Q.IsReady()) {
      Q.Width = SpellQWidth;
      auto farmResult = GetCircularFarmLocation(Q.Range, Q.Width);
      if (farmResult.second >= 1) {
        int minMana = lcMenu->GetSliderValue("farmQPercent", 10);
        if (GetManaPercent() >= static_cast<float>(minMana)) {
          Q.Cast(farmResult.first);
        }
      }
    }

    // E lane clear (Karthus.cs lines 476-498)
    if (!farmE || !E.IsReady() || IsInPassiveForm()) return;
    m_comboE = false;

    int minionCount = 0;
    for (const auto& m : ObjectManager::EnemyMinions()) {
      if (m.IsValid() && m.IsValidTarget(E.Range))
        minionCount++;
    }

    bool enoughMana = GetManaPercent() > static_cast<float>(lcMenu->GetSliderValue("farmEPercent", 20));

    if (enoughMana && minionCount >= 3 && Player().HasBuff("KarthusDefileSelf")) {
      E.Cast();
    }
  }

  // ════════════════════════════════════════════════
  // LastHit (Karthus.cs lines 515-528)
  // ════════════════════════════════════════════════
  void LastHit() {
    auto *lcMenu = m_menu->GetSubMenu("LaneClearMenu");
    if (!lcMenu) return;

    int farmQIdx = lcMenu->GetListIndex("farmQ", 1);
    bool farmQ = (farmQIdx == 0 || farmQIdx == 2);

    if (!farmQ || !Q.IsReady()) return;

    CastQ(TargetSelector::GetTarget(Q.Range, SDK::DamageType::Magical),
          lcMenu->GetSliderValue("farmQPercent", 10));
  }

  // ════════════════════════════════════════════════
  // JungleClear (Karthus.cs lines 414-451)
  // ════════════════════════════════════════════════
  void JungleClear() {
    auto *jgMenu = m_menu->GetSubMenu("JungleClearMenu");
    if (!jgMenu) return;

    auto mobs = ObjectManager::JungleMinions();
    // Filter to range + sort by MaxHealth
    std::vector<AIMinionClient> validMobs;
    for (const auto& m : mobs) {
      if (m.IsValid() && m.IsValidTarget(650.0f))
        validMobs.push_back(m);
    }
    std::sort(validMobs.begin(), validMobs.end(),
      [](const AIMinionClient& a, const AIMinionClient& b) {
        return a.MaxHealth() < b.MaxHealth();
      });

    // Turn off E if no jungle mobs (Karthus.cs lines 419-422)
    if (Player().HasBuff("KarthusDefileSelf") && validMobs.empty()) {
      auto target = TargetSelector::GetTarget(Q.Range, SDK::DamageType::Magical);
      if (!target.IsValid()) {
        E.Cast();
      }
    }

    if (validMobs.empty()) return;

    // Q on farm location (Karthus.cs lines 429-434)
    if (jgMenu->GetBoolValue("useQ", true) && Q.IsReady()) {
      auto farmResult = GetCircularJungleFarmLocation(650.0f, 150.0f);
      if (farmResult.second >= 1) {
        Q.Cast(farmResult.first);
      }
    }

    // Q + E on individual mobs (Karthus.cs lines 437-448)
    for (const auto& mob : validMobs) {
      if (Q.IsReady() && jgMenu->GetBoolValue("useQ", true)) {
        auto pred = Q.GetPrediction(mob);
        Q.Cast(pred.CastPosition);
      }
      if (!Player().HasBuff("KarthusDefileSelf") && mob.IsValidTarget(E.Range) &&
          jgMenu->GetBoolValue("useE", true)) {
        E.Cast();
      }
    }
  }

  // ════════════════════════════════════════════════
  // Helper: Circular farm location for minions
  // ════════════════════════════════════════════════
  std::pair<Vector3, int> GetCircularFarmLocation(float range, float radius) const {
    Vector3 bestPos = {};
    int bestCount = 0;
    for (const auto& m : ObjectManager::EnemyMinions()) {
      if (!m.IsValid() || !m.IsValidTarget(range)) continue;
      int count = 0;
      for (const auto& o : ObjectManager::EnemyMinions()) {
        if (!o.IsValid() || !o.IsValidTarget(range)) continue;
        if (o.Position().Distance(m.Position()) <= radius) count++;
      }
      if (count > bestCount) { bestCount = count; bestPos = m.Position(); }
    }
    return {bestPos, bestCount};
  }

  std::pair<Vector3, int> GetCircularJungleFarmLocation(float range, float radius) const {
    Vector3 bestPos = {};
    int bestCount = 0;
    auto mobs = ObjectManager::JungleMinions();
    for (const auto& m : mobs) {
      if (!m.IsValid() || !m.IsValidTarget(range)) continue;
      int count = 0;
      for (const auto& o : mobs) {
        if (!o.IsValid() || !o.IsValidTarget(range)) continue;
        if (o.Position().Distance(m.Position()) <= radius) count++;
      }
      if (count > bestCount) { bestCount = count; bestPos = m.Position(); }
    }
    return {bestPos, bestCount};
  }
};

} // namespace Plugins
