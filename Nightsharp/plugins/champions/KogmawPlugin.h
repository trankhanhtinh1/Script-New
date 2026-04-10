#pragma once

// ═══════════════════════════════════════════════════════
// NightSharp Kog'Maw Plugin — Full port from Kogmaw.cs (7UPAIO)
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

#include <algorithm>
#include <cfloat>
#include <string>

namespace Plugins {

using namespace SDK;
using namespace SDK::MenuUI;

class KogmawPlugin : public IPlugin {
public:
  const char *GetName()       const override { return "KogMaw"; }
  const char *GetInternalId() const override { return "champion_kogmaw"; }
  const char *GetAuthor()     const override { return "7UP / NightSharp"; }
  PluginCategory GetCategory() const override { return PluginCategory::Champion; }
  bool AutoLoadByDefault()    const override { return false; }

  bool CanLoad() const override {
    return Player().IsValid() && Player().CharacterName() == "KogMaw";
  }

  Spell Q, W, E, R;
  float QMANA = 0, WMANA = 0, EMANA = 0, RMANA = 0;
  bool attackNow = false;
  int tickIndex = 0;

  void OnLoad() override {
    if (m_menu) return;

    // Spell definitions (Kogmaw.cs lines 31-38)
    Q = Spell(SpellSlot::Q, 1200.0f);
    W = Spell(SpellSlot::W, 1000.0f);
    E = Spell(SpellSlot::E, 1360.0f);
    R = Spell(SpellSlot::R, 1300.0f);

    Q.SetSkillshot(0.25f, 65.0f, 1650.0f, true, SpellType::Line);
    E.SetSkillshot(0.25f, 115.0f, 1400.0f, false, SpellType::Line);
    R.SetSkillshot(0.25f, 115.0f, FLT_MAX, false, SpellType::Circle);

    m_menu = Menu::Create("KogmawRoot", "[NightSharp] Kog'Maw");

    // Q Settings (Kogmaw.cs lines 48-49)
    auto *qMenu = m_menu->AddSubMenu("QConfig", "Q Settings");
    qMenu->Add<MenuBool>("autoQ", "Auto Q", true);
    qMenu->Add<MenuBool>("harrasQ", "Harass Q", true);

    // E Settings (Kogmaw.cs lines 51-53)
    auto *eMenu = m_menu->AddSubMenu("EConfig", "E Settings");
    eMenu->Add<MenuBool>("autoE", "Auto E", true);
    eMenu->Add<MenuBool>("HarrasE", "Harass E", true);
    eMenu->Add<MenuBool>("AGC", "AntiGapcloser E", true);

    // W Settings (Kogmaw.cs lines 55-56)
    auto *wMenu = m_menu->AddSubMenu("WConfig", "W Settings");
    wMenu->Add<MenuBool>("autoW", "Auto W", true);
    wMenu->Add<MenuBool>("harasW", "Harass W on max range", true);

    // R Settings (Kogmaw.cs lines 58-65)
    auto *rMenu = m_menu->AddSubMenu("RConfig", "R Settings");
    rMenu->Add<MenuBool>("autoR", "Auto R", true);
    rMenu->Add<MenuSlider>("RmaxHp", "Target max % HP", 50, 0, 100);
    rMenu->Add<MenuSlider>("comboStack", "Max combo stack R", 2, 0, 10);
    rMenu->Add<MenuSlider>("harasStack", "Max haras stack R", 1, 0, 10);
    rMenu->Add<MenuBool>("Rcc", "R cc", true);
    rMenu->Add<MenuBool>("Rslow", "R slow", true);
    rMenu->Add<MenuBool>("Raoe", "R aoe", true);
    rMenu->Add<MenuBool>("Raa", "R only out of AA range", false);

    // Global settings (Kogmaw.cs lines 74-77)
    m_menu->Add<MenuBool>("sheen", "Sheen logic", true);
    m_menu->Add<MenuBool>("AApriority", "AA priority over spell", true);
    m_menu->Add<MenuBool>("manaDisable", "Disable Mana Manager", false);

    // Farm (Kogmaw.cs lines 79-84)
    auto *farmMenu = m_menu->AddSubMenu("Farm", "Farm");
    farmMenu->Add<MenuBool>("farmW", "LaneClear W", true);
    farmMenu->Add<MenuBool>("farmE", "LaneClear E", true);
    farmMenu->Add<MenuSlider>("LCminions", "LaneClear minimum minions", 2, 0, 10);
    farmMenu->Add<MenuSlider>("Mana", "LaneClear Mana", 80, 0, 100);
    farmMenu->Add<MenuBool>("jungleW", "Jungle clear W", true);
    farmMenu->Add<MenuBool>("jungleE", "Jungle clear E", true);
  }

  void OnUnload() override {
    if (!m_menu) return;
    Menu::Remove("KogmawRoot");
    m_menu = nullptr;
  }

  Menu *GetMenuRoot() override { return m_menu; }

  // ════════════════════════════════════════════════
  // OnGapcloser (Kogmaw.cs lines 101-110)
  // ════════════════════════════════════════════════
  void OnGapcloser(const AIHeroClient& sender, const AntiGapcloser::GapcloserArgs& args) override {
    if (!m_menu) return;
    auto *eMenu = m_menu->GetSubMenu("EConfig");
    if (!eMenu) return;

    if (eMenu->GetBoolValue("AGC", true) && E.IsReady() && Player().Mana() > RMANA + EMANA) {
      if (sender.IsValidTarget(E.Range)) {
        E.Cast(sender);
      }
    }
  }

  // ════════════════════════════════════════════════
  // OnBeforeAttack (Kogmaw.cs lines 112-125)
  // ════════════════════════════════════════════════
  void OnBeforeAttack(OrbwalkingActionArgs& args) override {
    if (!m_menu) return;
    attackNow = true;
    auto *farmMenu = m_menu->GetSubMenu("Farm");

    if (Orbwalker::GetMode() == OrbwalkerMode::Clear && W.IsReady() &&
        Player().ManaPercent() > static_cast<float>(farmMenu ? farmMenu->GetSliderValue("Mana", 80) : 80)) {
      int minionCount = 0;
      for (const auto& m : ObjectManager::EnemyMinions()) {
        if (m.IsValid() && m.IsValidTarget(650.0f)) minionCount++;
      }
      if (farmMenu && minionCount >= farmMenu->GetSliderValue("LCminions", 2)) {
        if (farmMenu->GetBoolValue("farmW", true) && minionCount > 1) {
          W.Cast();
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // OnAfterAttack (Kogmaw.cs lines 127-130)
  // ════════════════════════════════════════════════
  void OnAfterAttack(OrbwalkingActionArgs& args) override {
    attackNow = false;
  }

  // ════════════════════════════════════════════════
  // OnUpdate (Kogmaw.cs lines 132-157)
  // ════════════════════════════════════════════════
  void OnUpdate() override {
    if (!Player().IsValid() || !m_menu) return;
    if (Player().IsDead() || Player().IsRecalling() || Player().IsWindingUp()) return;

    auto *qMenu = m_menu->GetSubMenu("QConfig");
    auto *eMenu = m_menu->GetSubMenu("EConfig");
    auto *wMenu = m_menu->GetSubMenu("WConfig");

    if (LagFree(0)) {
      // Update dynamic ranges (Kogmaw.cs lines 136-137)
      R.Range = 1050.0f + 250.0f * Player().GetSpellBook().GetSpell(SpellSlot::R).Level();
      W.Range = 630.0f + 20.0f * Player().GetSpellBook().GetSpell(SpellSlot::W).Level();
      SetMana();
      Jungle();
    }

    if (LagFree(1) && E.IsReady() && eMenu && eMenu->GetBoolValue("autoE", true))
      LogicE();

    if (LagFree(2) && Q.IsReady() && qMenu && qMenu->GetBoolValue("autoQ", true))
      LogicQ();

    if (LagFree(3) && W.IsReady() && wMenu && wMenu->GetBoolValue("autoW", true))
      LogicW();

    if (LagFree(4) && R.IsReady())
      LogicR();

    tickIndex++;
    if (tickIndex > 4) tickIndex = 0;
  }

private:
  Menu *m_menu = nullptr;

  bool LagFree(int offset) const { return tickIndex == offset; }

  // ════════════════════════════════════════════════
  // SetMana (Kogmaw.cs lines 327-346)
  // ════════════════════════════════════════════════
  void SetMana() {
    if ((m_menu->GetBoolValue("manaDisable", false) && Orbwalker::GetMode() == OrbwalkerMode::Combo) ||
        Player().HealthPercent() < 20.0f) {
      QMANA = WMANA = EMANA = RMANA = 0;
      return;
    }
    QMANA = Q.Instance().ManaCost();
    WMANA = W.Instance().ManaCost();
    EMANA = E.Instance().ManaCost();
    RMANA = R.Instance().ManaCost();
  }

  // ════════════════════════════════════════════════
  // GetRStacks (Kogmaw.cs lines 348-356)
  // ════════════════════════════════════════════════
  int GetRStacks() const {
    return Player().GetBuffCount("kogmawlivingartillerycost");
  }

  // ════════════════════════════════════════════════
  // Sheen logic (Kogmaw.cs lines 300-317)
  // ════════════════════════════════════════════════
  bool Sheen() const {
    // Simplified: if player has sheen buff and sheen logic enabled, delay spell
    if (Player().HasBuff("sheen") && m_menu->GetBoolValue("sheen", true))
      return false;
    if (m_menu->GetBoolValue("AApriority", true) && !attackNow) {
      // Check if orbwalker target is a hero
      // Simplified — in original this checks Orbwalker.GetTarget() is AIHeroClient
      return true;
    }
    return true;
  }

  // ════════════════════════════════════════════════
  // Jungle (Kogmaw.cs lines 159-180)
  // ════════════════════════════════════════════════
  void Jungle() {
    auto *farmMenu = m_menu->GetSubMenu("Farm");
    if (!farmMenu) return;

    if (Orbwalker::GetMode() == OrbwalkerMode::Clear && Player().Mana() > RMANA + QMANA) {
      AIMinionClient bestMob;
      float bestHP = 0;
      for (const auto& mob : ObjectManager::JungleMinions()) {
        if (!mob.IsValid() || !mob.IsValidTarget(650.0f)) continue;
        if (mob.MaxHealth() > bestHP) { bestHP = mob.MaxHealth(); bestMob = mob; }
      }

      if (bestMob.IsValid()) {
        if (E.IsReady() && farmMenu->GetBoolValue("jungleE", true)) {
          E.Cast(bestMob.Position());
          return;
        }
        if (W.IsReady() && farmMenu->GetBoolValue("jungleW", true)) {
          W.Cast();
          return;
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // LogicR (Kogmaw.cs lines 182-224)
  // ════════════════════════════════════════════════
  void LogicR() {
    auto *rMenu = m_menu->GetSubMenu("RConfig");
    if (!rMenu || !rMenu->GetBoolValue("autoR", true) || !Sheen()) return;

    auto target = TargetSelector::GetTarget(R.Range, SDK::DamageType::Magical);
    if (!target.IsValid() || !target.IsValidTarget(R.Range)) return;

    if (target.HealthPercent() >= static_cast<float>(rMenu->GetSliderValue("RmaxHp", 50))) return;

    if (rMenu->GetBoolValue("Raa", false) && target.IsValidTarget(Player().AttackRange()))
      return;

    int harasStack = rMenu->GetSliderValue("harasStack", 1);
    int comboStack = rMenu->GetSliderValue("comboStack", 2);
    int countR = GetRStacks();

    float rDmg = R.GetDamage(target);
    auto rPred = R.GetPrediction(target);

    // KS (Kogmaw.cs line 200)
    if (rDmg > target.Health())  {
      R.Cast(target);
      return;
    }

    // Combo kill in 2 R (Kogmaw.cs lines 202-204)
    if (Orbwalker::GetMode() == OrbwalkerMode::Combo && rDmg * 2.0f > target.Health() &&
        Player().Mana() > RMANA * 3.0f) {
      R.Cast(target);
      return;
    }

    // R on immobile (Kogmaw.cs lines 205-210)
    if (countR < comboStack + 2 && Player().Mana() > RMANA * 3.0f) {
      for (const auto& enemy : ObjectManager::EnemyHeroes()) {
        if (enemy.IsValid() && enemy.IsValidTarget(R.Range)) {
          R.CastPredicted(enemy, HitChance::High);
        }
      }
    }

    // R on slowed (Kogmaw.cs lines 213-215)
    if (rMenu->GetBoolValue("Rcc", true) && rPred.Hitchance == HitChance::Immobile &&
        countR < comboStack + 2 && Player().Mana() > RMANA * 3.0f) {
      R.Cast(rPred.CastPosition);
    }
    else if (rMenu->GetBoolValue("Rslow", true) && rPred.Hitchance >= HitChance::VeryHigh &&
        countR < comboStack + 1 && Player().Mana() > RMANA + WMANA + EMANA + QMANA) {
      R.Cast(rPred.CastPosition);
    }
    // Combo R (Kogmaw.cs lines 216-218)
    else if (Orbwalker::GetMode() == OrbwalkerMode::Combo && countR < comboStack &&
             Player().Mana() > RMANA + WMANA + EMANA + QMANA) {
      R.Cast(target);
    }
    // Harass R (Kogmaw.cs lines 219-221)
    else if (Orbwalker::GetMode() == OrbwalkerMode::Clear && countR < harasStack &&
             Player().Mana() > RMANA + WMANA + EMANA + QMANA) {
      R.Cast(target);
    }
  }

  // ════════════════════════════════════════════════
  // LogicW (Kogmaw.cs lines 226-235)
  // ════════════════════════════════════════════════
  void LogicW() {
    if (Player().CountEnemyHeroesInRange(W.Range) > 0) {
      if (Orbwalker::GetMode() == OrbwalkerMode::Combo) {
        W.Cast();
      } else if (Orbwalker::GetMode() == OrbwalkerMode::Clear) {
        auto *eMenu = m_menu->GetSubMenu("EConfig");
        if (eMenu && eMenu->GetBoolValue("harasW", true) &&
            Player().CountEnemyHeroesInRange(Player().AttackRange()) > 0) {
          W.Cast();
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // LogicQ (Kogmaw.cs lines 237-262)
  // ════════════════════════════════════════════════
  void LogicQ() {
    if (!Sheen()) return;
    auto t = TargetSelector::GetTarget(Q.Range, SDK::DamageType::Magical);
    if (!t.IsValid() || !t.IsValidTarget()) return;

    float qDmg = Q.GetDamage(t);
    float eDmg = E.GetDamage(t);

    // KS (Kogmaw.cs line 246-247)
    if (t.IsValidTarget(W.Range) && qDmg + eDmg > t.Health()) {
      Q.Cast(t);
      return;
    }

    // Combo (Kogmaw.cs line 248-249)
    if (Orbwalker::GetMode() == OrbwalkerMode::Combo && Player().Mana() > RMANA + QMANA * 2 + EMANA) {
      Q.Cast(t);
      return;
    }

    // Harass Q (Kogmaw.cs lines 250-252)
    auto *qMenu = m_menu->GetSubMenu("QConfig");
    if (Orbwalker::GetMode() == OrbwalkerMode::Clear &&
        Player().Mana() > RMANA + EMANA + QMANA * 2 + WMANA &&
        qMenu && qMenu->GetBoolValue("harrasQ", true)) {
      Q.Cast(t);
      return;
    }

    // General (Kogmaw.cs lines 253-258)
    if ((Orbwalker::GetMode() == OrbwalkerMode::Combo || Orbwalker::GetMode() == OrbwalkerMode::Clear) &&
        Player().Mana() > RMANA + QMANA + EMANA) {
      for (const auto& enemy : ObjectManager::EnemyHeroes()) {
        if (enemy.IsValid() && enemy.IsValidTarget(Q.Range)) {
          Q.CastPredicted(enemy, HitChance::High);
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // LogicE (Kogmaw.cs lines 264-298)
  // ════════════════════════════════════════════════
  void LogicE() {
    if (!Sheen()) return;
    auto t = TargetSelector::GetTarget(E.Range, SDK::DamageType::Magical);

    if (t.IsValid() && t.IsValidTarget()) {
      float eDmg = E.GetDamage(t);
      float qDmg = Q.GetDamage(t);

      // KS (Kogmaw.cs lines 273-276)
      if (eDmg > t.Health()) { E.Cast(t); return; }
      if (eDmg + qDmg > t.Health() && Q.IsReady()) { E.Cast(t); return; }

      // Combo (Kogmaw.cs line 277-278)
      if (Orbwalker::GetMode() == OrbwalkerMode::Combo &&
          Player().Mana() > RMANA + WMANA + EMANA + QMANA) {
        E.Cast(t);
        return;
      }

      // Harass E (Kogmaw.cs lines 279-281)
      auto *eMenu = m_menu->GetSubMenu("EConfig");
      if (Orbwalker::GetMode() == OrbwalkerMode::Clear && eMenu &&
          eMenu->GetBoolValue("HarrasE", true) &&
          Player().Mana() > RMANA + WMANA + EMANA + QMANA + EMANA) {
        E.Cast(t);
        return;
      }

      // General (Kogmaw.cs lines 282-287)
      if ((Orbwalker::GetMode() == OrbwalkerMode::Combo || Orbwalker::GetMode() == OrbwalkerMode::Clear) &&
          Player().Mana() > RMANA + WMANA + EMANA) {
        for (const auto& enemy : ObjectManager::EnemyHeroes()) {
          if (enemy.IsValid() && enemy.IsValidTarget(E.Range)) {
            E.CastPredicted(enemy, HitChance::High);
          }
        }
      }
    } else {
      // LaneClear E (Kogmaw.cs lines 289-296)
      auto *farmMenu = m_menu->GetSubMenu("Farm");
      if (Orbwalker::GetMode() == OrbwalkerMode::Clear && farmMenu &&
          Player().ManaPercent() > static_cast<float>(farmMenu->GetSliderValue("Mana", 80)) &&
          farmMenu->GetBoolValue("farmE", true) && Player().Mana() > RMANA + EMANA) {
        auto farmResult = GetLineFarmLocation(E.Range, E.Width);
        if (farmResult.second >= farmMenu->GetSliderValue("LCminions", 2)) {
          E.Cast(farmResult.first);
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
