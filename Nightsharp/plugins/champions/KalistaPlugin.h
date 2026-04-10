#pragma once

// ═══════════════════════════════════════════════════════
// NightSharp Kalista Plugin — Full port from Kalista.cs (7UPAIO)
// Logic: 100% giữ nguyên bản gốc, thứ tự logic giữ nguyên
// ═══════════════════════════════════════════════════════

#include "../IPlugin.h"
#include "../PluginSdkCompat.h"
#include "menu/MenuUI.h"
#include "sdk/Core/Game.h"
#include "sdk/SDK.h"
#include "sdk/Extensions/Unit.h"
#include "sdk/Wrappers/Damages/Damage.h"
#include "sdk/Utils/Jungle.h"
#include "sdk/Wrappers/Orbwalking/Orbwalker.h"
#include "sdk/Wrappers/Spells/Spell.h"
#include "sdk/Wrappers/TargetSelector/TargetSelector.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace Plugins {

using namespace SDK;
using namespace SDK::MenuUI;

class KalistaPlugin : public IPlugin {
public:
  const char *GetName()       const override { return "Kalista"; }
  const char *GetInternalId() const override { return "champion_kalista"; }
  const char *GetAuthor()     const override { return "7UP / NightSharp"; }
  PluginCategory GetCategory() const override { return PluginCategory::Champion; }
  bool AutoLoadByDefault()    const override { return false; }

  bool CanLoad() const override {
    return Player().IsValid() && Player().CharacterName() == "Kalista";
  }

  Spell Q, W, E, R;

  // Rend damage tables
  static constexpr float EBaseDamage[]            = { 0, 5, 15, 25, 35, 45 };
  static constexpr float EStackBaseDamage[]       = { 0, 7, 14, 21, 28, 35 };
  static constexpr float EStackMultiplierDamage[] = { 0, 0.20f, 0.275f, 0.35f, 0.425f, 0.50f };
  static constexpr float EBaseAttackDamageRatio   = 0.70f;
  static constexpr float EBaseAbilityPowerRatio   = 0.65f;
  static constexpr float EStackAbilityPowerRatio  = 0.50f;

  // State
  int m_AATime = 0;
  float m_lastAATick = 0;
  int m_lastETime = 0;
  int m_lastForcusTime = 0;
  std::map<float, float> m_incomingDmgToSoulbound;
  std::map<float, float> m_instantDmgOnSoulbound;

  void OnLoad() override {
    if (m_menu) return;

    // Spell definitions (Kalista.cs lines 38-46)
    Q = Spell(SpellSlot::Q, 1200.0f);
    W = Spell(SpellSlot::W, 5000.0f);
    E = Spell(SpellSlot::E, 1000.0f);
    R = Spell(SpellSlot::R, 1200.0f);

    Q.SetSkillshot(0.35f, 40.0f, 2400.0f, true, SpellType::Line);
    R.SetSkillshot(0.50f, 1500.0f, FLT_MAX, false, SpellType::Circle);

    m_menu = Menu::Create("KalistaRoot", "[NightSharp] Kalista");

    // ComboMenu (Kalista.cs lines 51-64)
    auto *comboMenu = m_menu->AddSubMenu("Combo", "Combo");
    comboMenu->Add<MenuKeyBind>("FlyHack", "Fly Hack", 'T', KeyBindType::Toggle);
    comboMenu->Add<MenuBool>("useQ", "Use Q", true);
    comboMenu->Add<MenuBool>("disQ", "Block on high aa speed", true);
    comboMenu->Add<MenuBool>("useE", "Use E", true);
    comboMenu->Add<MenuBool>("disE1", "Block on Debuff", true);
    comboMenu->Add<MenuBool>("disE2", "Limit usage", true);
    comboMenu->Add<MenuBool>("orbminion", "Orbwalker Minion", true);

    // Eset (Kalista.cs lines 65-68)
    auto *eSet = m_menu->AddSubMenu("Eset", "E Settings");
    eSet->Add<MenuList>("EMode", "Use E Mode",
      std::vector<std::string>{"Only Combo", "Always", "Disable"}, 1);
    eSet->Add<MenuBool>("harassPlus", "Auto Kill Minion && Any Enemy Have E Buff", true);

    // Rset (Kalista.cs lines 69-77)
    auto *rSet = m_menu->AddSubMenu("Rset", "R Settings");
    auto *wowCombo = rSet->AddSubMenu("WowCombo", "WowCombo");
    wowCombo->Add<MenuBool>("Balista", "Balista", true);
    wowCombo->Add<MenuBool>("Salista", "Salista", true);
    wowCombo->Add<MenuBool>("Talista", "Talista", true);
    rSet->Add<MenuBool>("kaliusersaveally", "Use R to save Soulbound", true);
    rSet->Add<MenuBool>("userengage", "Use R to engage", true);

    // Harass (Kalista.cs lines 78-84)
    auto *harassMenu = m_menu->AddSubMenu("Harass", "Harass");
    harassMenu->Add<MenuBool>("useQ", "Use Q", true);
    harassMenu->Add<MenuBool>("QMinion", "Use Q on Minion", true);
    harassMenu->Add<MenuBool>("useE", "Use E", true);
    harassMenu->Add<MenuBool>("disE1", "Block on Debuff", true);
    harassMenu->Add<MenuBool>("disE2", "Limit usage", true);
    harassMenu->Add<MenuSlider>("Mana", "Mana", 50, 0, 100);

    // LaneClear (Kalista.cs lines 85-92)
    auto *lcMenu = m_menu->AddSubMenu("LaneClear", "Lane Clear");
    lcMenu->Add<MenuBool>("useQ", "Use Q for jungle", true);
    lcMenu->Add<MenuBool>("useE", "Use E", true);
    lcMenu->Add<MenuSlider>("MinE", "Use Kill min minion Count", 2, 1, 5);
    lcMenu->Add<MenuSlider>("Mana", "Don't Lane/Jung if Mana <= X%", 40, 0, 100);

    // Misc (Kalista.cs lines 93-103)
    auto *miscMenu = m_menu->AddSubMenu("Misc", "Misc");
    miscMenu->Add<MenuBool>("misc-prevent-e", "Prevent E on Spellshields & Invulnerable", true);
    miscMenu->Add<MenuBool>("misc-dying-e", "E before dying", true);
    miscMenu->Add<MenuSlider>("misc-dying-e-pro", "E dying on %", 10, 1, 50);
    miscMenu->Add<MenuBool>("misc-leaving-e", "E when leaving range", true);
    miscMenu->Add<MenuSlider>("misc-leaving-e-pro", "E leaving stacks", 5, 1, 10);
    miscMenu->Add<MenuKeyBind>("misc-ward-trick", "Auto W", 'G', KeyBindType::Toggle);
    miscMenu->Add<MenuBool>("Forcus", "Forcus Attack", true);
    miscMenu->Add<MenuSlider>("EToler", "E Toler DMG", 0, -100, 110);

    // KS (Kalista.cs lines 105-109)
    auto *ksMenu = m_menu->AddSubMenu("KS", "KS Settings");
    ksMenu->Add<MenuBool>("KSQ", "Use Q KS", true);
    ksMenu->Add<MenuBool>("KSE", "Use E KS", true);
    ksMenu->Add<MenuBool>("KSEJG", "Use E KS JG", true);
  }

  void OnUnload() override {
    if (!m_menu) return;
    Menu::Remove("KalistaRoot");
    m_menu = nullptr;
  }

  Menu *GetMenuRoot() override { return m_menu; }

  // ════════════════════════════════════════════════
  // OnBeforeAttack (Kalista.cs lines 203-243)
  // ════════════════════════════════════════════════
  void OnBeforeAttack(OrbwalkingActionArgs& args) override {
    if (!m_menu) return;
    auto *miscMenu = m_menu->GetSubMenu("Misc");
    if (!miscMenu || !miscMenu->GetBoolValue("Forcus", true)) return;

    auto mode = Orbwalker::GetMode();
    if (mode == OrbwalkerMode::Combo || mode == OrbwalkerMode::Harass) {
      // Force target: enemy with W passive mark (Kalista.cs lines 210-223)
      for (const auto& target : ObjectManager::EnemyHeroes()) {
        if (!target.IsValid() || target.IsDead()) continue;
        if (!target.IsValidTarget(Player().AttackRange() + Player().BoundingRadius() + target.BoundingRadius()))
          continue;
        if (target.HasBuff("kalistacoopstrikemarkally")) {
          Orbwalker::ForceTarget(target);
          m_lastForcusTime = Game::TickCount();
          break;
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // OnAfterAttack (Kalista.cs lines 244-310)
  // ════════════════════════════════════════════════
  void OnAfterAttack(OrbwalkingActionArgs& args) override {
    if (!m_menu) return;
    m_AATime = Game::TickCount();
    Orbwalker::ClearForcedTarget();

    if (!Q.IsReady()) return;

    auto *comboMenu = m_menu->GetSubMenu("Combo");
    auto *harassMenu = m_menu->GetSubMenu("Harass");
    auto *lcMenu = m_menu->GetSubMenu("LaneClear");
    auto mode = Orbwalker::GetMode();

    if (mode == OrbwalkerMode::Combo) {
      if (comboMenu && comboMenu->GetBoolValue("useQ", true)) {
        auto target = TargetSelector::GetTarget(Q.Range, SDK::DamageType::Physical);
        if (target.IsValid() && target.IsValidTarget(Q.Range)) {
          auto pred = Q.GetPrediction(target);
          if (pred.Hitchance >= HitChance::High) {
            Q.Cast(pred.CastPosition);
          }
        }
      }
    } else if (mode == OrbwalkerMode::Harass) {
      int mana = harassMenu ? harassMenu->GetSliderValue("Mana", 50) : 50;
      if (Player().ManaPercent() >= static_cast<float>(mana) &&
          harassMenu && harassMenu->GetBoolValue("useQ", true)) {
        auto target = TargetSelector::GetTarget(Q.Range, SDK::DamageType::Physical);
        if (target.IsValid() && target.IsValidTarget(Q.Range)) {
          auto pred = Q.GetPrediction(target);
          if (pred.Hitchance >= HitChance::High) {
            Q.Cast(pred.CastPosition);
          }
        }
      }
    } else if (mode == OrbwalkerMode::Clear) {
      // Jungle Q (Kalista.cs lines 293-309)
      if (lcMenu && lcMenu->GetBoolValue("useQ", true)) {
        for (const auto& mob : ObjectManager::JungleMinions()) {
          if (!mob.IsValid() || !mob.IsValidTarget(Q.Range)) continue;
          Q.Cast(mob);
          break;
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // OnUpdate (Kalista.cs lines 365-405)
  // ════════════════════════════════════════════════
  void OnUpdate() override {
    if (!Player().IsValid() || !m_menu) return;
    if (Player().IsDead() || Player().IsRecalling() || Player().IsWindingUp()) return;

    auto mode = Orbwalker::GetMode();

    switch (mode) {
      case OrbwalkerMode::Combo:  ComboMode(); break;
      case OrbwalkerMode::Harass: HarassMode(); break;
      case OrbwalkerMode::Clear:  ClearMode(); break;
      default: break;
    }

    Routine();
    Killsteal();
    FlyHack();
    LogicE();
    RLogic();
  }

private:
  Menu *m_menu = nullptr;

  bool IsEpicMonsterForRend(const AIBaseClient& target) const {
    if (!target.IsMinion()) return false;
    return Utils::Jungle::GetJungleType(AIMinionClient(target.Ref())) >= JungleType::Legendary;
  }

  // ════════════════════════════════════════════════
  // EDamage (Kalista.cs lines 181-200)
  // ════════════════════════════════════════════════
  float EDamage(const AIBaseClient& target) const {
    int eLevel = Player().GetSpellBook().GetSpell(SpellSlot::E).Level();
    if (eLevel < 1 || eLevel > 5) return 0;

    int stacks = target.GetBuffCount("kalistaexpungemarker");
    if (stacks <= 0) return 0;

    const float attackDamage = Player().TotalAttackDamage();
    const float abilityPower = Player().TotalMagicalDamage();
    const float eBaseDmg = EBaseDamage[eLevel] +
                           EBaseAttackDamageRatio * attackDamage +
                           EBaseAbilityPowerRatio * abilityPower;
    const float eStackDmg = EStackBaseDamage[eLevel] +
                            EStackMultiplierDamage[eLevel] * attackDamage +
                            EStackAbilityPowerRatio * abilityPower;
    float total = eBaseDmg + eStackDmg * static_cast<float>(stacks - 1);
    if (IsEpicMonsterForRend(target)) {
      total *= 0.5f;
    }
    return Player().CalculatePhysicalDamage(target, total);
  }

  // GetEDamage (Kalista.cs lines 1115-1134) — same as EDamage
  float GetEDamage(const AIBaseClient& target) const {
    return EDamage(target);
  }

  bool CastRend() {
    if (!E.IsReady()) return false;
    const bool casted = E.Cast();
    if (casted) {
      m_lastETime = Game::TickCount();
    }
    return casted;
  }

  // ════════════════════════════════════════════════
  // GetRealDamage (Kalista.cs lines 938-951)
  // ════════════════════════════════════════════════
  float GetRealDamage(const AIBaseClient& target) const {
    float dmg = EDamage(target);
    if (target.HasBuff("FerociousHowl")) return dmg * 0.7f;
    if (Player().HasBuff("summonerexhaust")) return dmg * 0.4f;
    return dmg;
  }

  // HasRendBuff (Kalista.cs lines 955-958)
  bool HasRendBuff(const AIBaseClient& target, float range) const {
    return target.IsValidTarget(range) && target.HasBuff("kalistaexpungemarker");
  }

  // ════════════════════════════════════════════════
  // QDamage (Kalista.cs lines 1070-1076)
  // ════════════════════════════════════════════════
  float QDamage(const AIBaseClient& target) const {
    int qLevel = Player().GetSpellBook().GetSpell(SpellSlot::Q).Level();
    if (qLevel < 1) return 0;
    float baseDmg[] = { 0, 10, 75, 140, 205, 270 };
    float qBase = baseDmg[qLevel] + 1.05f * Player().TotalAttackDamage();
    return Player().CalculatePhysicalDamage(target, qBase);
  }

  // ════════════════════════════════════════════════
  // ComboMode (Kalista.cs lines 407-558)
  // ════════════════════════════════════════════════
  void ComboMode() {
    auto *comboMenu = m_menu->GetSubMenu("Combo");
    auto *eSet = m_menu->GetSubMenu("Eset");
    if (!comboMenu) return;

    bool useQ = comboMenu->GetBoolValue("useQ", true);
    bool disQ = comboMenu->GetBoolValue("disQ", true);
    bool useE = comboMenu->GetBoolValue("useE", true);
    bool disE1 = comboMenu->GetBoolValue("disE1", true);
    bool disE2 = comboMenu->GetBoolValue("disE2", true);
    bool orbminion = comboMenu->GetBoolValue("orbminion", true);
    bool harassPlus = eSet ? eSet->GetBoolValue("harassPlus", true) : true;

    auto target = TargetSelector::GetTarget(Q.Range, SDK::DamageType::Physical);
    if (!target.IsValid() || !target.IsValidTarget(Q.Range)) return;

    // Q combo (Kalista.cs lines 453-474)
    if (useQ && Q.IsReady()) {
      if (disQ) {
        if (Player().AttackSpeedMod() < 1.98f) {
          auto pred = Q.GetPrediction(target);
          if (pred.Hitchance >= HitChance::High) Q.Cast(pred.CastPosition);
        }
      } else {
        auto pred = Q.GetPrediction(target);
        if (pred.Hitchance >= HitChance::High) Q.Cast(pred.CastPosition);
      }
    }

    // E combo (Kalista.cs lines 476-537)
    if (useE && E.IsReady() && target.IsValidTarget(E.Range)) {
      if (Game::TickCount() - m_lastETime > 500 + Game::Ping()) {
        // E kill target
        if (target.Health() < GetEDamage(target) && !target.IsInvulnerable()) {
          CastRend();
        }
        // Harass plus: kill minion while enemy in E range
        if (harassPlus &&
            target.DistanceToPlayer() > Player().AttackRange() + Player().BoundingRadius() + 100.0f &&
            target.IsValidTarget(E.Range)) {
          for (const auto& m : ObjectManager::EnemyMinions()) {
            if (!m.IsValid() || !m.IsValidTarget(E.Range)) continue;
            if (m.HasBuff("kalistaexpungemarker") && m.Health() < EDamage(m)) {
              CastRend();
              break;
            }
          }
        }
      }

      // E on debuffed enemies — block on CC (Kalista.cs lines 509-536)
      for (const auto& t : ObjectManager::EnemyHeroes()) {
        if (!t.IsValid() || !t.IsValidTarget(E.Range)) continue;
        if (!t.HasBuff("kalistaexpungemarker")) continue;

        if (disE1) {
          if (Compat::HasMovementLock(t)) {
            continue;
          }

          // Kill minion with E marks while enemy has marks
          bool canKillMinion = false;
          for (const auto& m : ObjectManager::EnemyMinions()) {
            if (m.IsValid() && m.IsValidTarget(E.Range) && m.HasBuff("kalistaexpungemarker") &&
                m.Health() <= GetEDamage(m)) {
              canKillMinion = true;
              break;
            }
          }

          if (canKillMinion) {
            if (disE2) {
              if (Game::TickCount() - m_lastETime > 2500) CastRend();
            } else {
              CastRend();
            }
          }
        }
      }
    }

    // Orbwalker Minion — gap close (Kalista.cs lines 538-557)
    if (orbminion) {
      bool noHeroInAA = true;
      bool heroNearby = false;
      for (const auto& x : ObjectManager::EnemyHeroes()) {
        if (!x.IsValid() || x.IsDead()) continue;
        float aaRange = Player().AttackRange() + Player().BoundingRadius() + x.BoundingRadius();
        if (x.IsValidTarget(aaRange)) { noHeroInAA = false; }
        if (x.IsValidTarget(1000.0f)) { heroNearby = true; }
      }

      if (noHeroInAA && heroNearby) {
        AIMinionClient bestMinion;
        float bestDist = FLT_MAX;
        for (const auto& m : ObjectManager::EnemyMinions()) {
          if (!m.IsValid() || m.IsDead()) continue;
          if (!m.IsValidTarget(Player().AttackRange() + Player().BoundingRadius() + m.BoundingRadius()))
            continue;
          float d = m.Position().Distance(Game::CursorPos());
          if (d < bestDist) { bestDist = d; bestMinion = m; }
        }
        if (bestMinion.IsValid()) {
          Orbwalker::ForceTarget(bestMinion);
        }
      } else {
        Orbwalker::ClearForcedTarget();
      }
    }
  }

  // ════════════════════════════════════════════════
  // HarassMode (Kalista.cs lines 560-631)
  // ════════════════════════════════════════════════
  void HarassMode() {
    auto *harassMenu = m_menu->GetSubMenu("Harass");
    if (!harassMenu) return;

    int mana = harassMenu->GetSliderValue("Mana", 50);
    if (Player().ManaPercent() < static_cast<float>(mana)) return;

    bool useQ = harassMenu->GetBoolValue("useQ", true);
    bool useE = harassMenu->GetBoolValue("useE", true);
    bool disE1 = harassMenu->GetBoolValue("disE1", true);
    bool disE2 = harassMenu->GetBoolValue("disE2", true);

    auto target = TargetSelector::GetTarget(Q.Range, SDK::DamageType::Physical);
    if (!target.IsValid() || !target.IsValidTarget(Q.Range)) return;

    // Q harass (Kalista.cs lines 579-598)
    if (useQ && Q.IsReady()) {
      auto pred = Q.GetPrediction(target);
      if (pred.Hitchance >= HitChance::High) {
        Q.Cast(pred.CastPosition);
      }
    }

    // E harass — same logic as combo E (Kalista.cs lines 601-630)
    if (useE && E.IsReady()) {
      for (const auto& t : ObjectManager::EnemyHeroes()) {
        if (!t.IsValid() || !t.IsValidTarget(E.Range)) continue;
        if (!t.HasBuff("kalistaexpungemarker")) continue;

        if (disE1) {
          if (Compat::HasMovementLock(t)) continue;

          bool canKillMinion = false;
          for (const auto& m : ObjectManager::EnemyMinions()) {
            if (m.IsValid() && m.IsValidTarget(E.Range) && m.HasBuff("kalistaexpungemarker") &&
                m.Health() <= GetEDamage(m)) {
              canKillMinion = true;
              break;
            }
          }

          if (canKillMinion) {
            if (disE2) {
              if (Game::TickCount() - m_lastETime > 2500) CastRend();
            } else {
              CastRend();
            }
          }
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // ClearMode (Kalista.cs lines 633-782)
  // ════════════════════════════════════════════════
  void ClearMode() {
    auto *lcMenu = m_menu->GetSubMenu("LaneClear");
    if (!lcMenu) return;

    int waveMana = lcMenu->GetSliderValue("Mana", 40);
    if (Player().ManaPercent() < static_cast<float>(waveMana)) return;

    bool useE = lcMenu->GetBoolValue("useE", true);
    bool useQ = lcMenu->GetBoolValue("useQ", true);
    int minE = lcMenu->GetSliderValue("MinE", 2);

    // E lane clear — kill minions with E marks (Kalista.cs lines 740-748)
    if (useE && E.IsReady()) {
      int killCount = 0;
      for (const auto& m : ObjectManager::EnemyMinions()) {
        if (!m.IsValid() || !m.IsValidTarget(E.Range)) continue;
        if (m.HasBuff("kalistaexpungemarker") && m.Health() < GetEDamage(m))
          killCount++;
      }
      if (killCount >= minE) CastRend();
    }

    // Q jungle (Kalista.cs lines 750-763)
    if (useQ && Q.IsReady()) {
      for (const auto& mob : ObjectManager::JungleMinions()) {
        if (!mob.IsValid() || !mob.IsValidTarget(Q.Range)) continue;
        // Only large+ mobs
        if (mob.MaxHealth() > 1000.0f) {
          auto pred = Q.GetPrediction(mob);
          if (pred.Hitchance >= HitChance::Medium) Q.Cast(pred.CastPosition);
          break;
        }
      }
    }

    // E jungle (Kalista.cs lines 765-781)
    if (useE && E.IsReady()) {
      for (const auto& mob : ObjectManager::JungleMinions()) {
        if (!mob.IsValid() || !mob.IsValidTarget(E.Range)) continue;
        if (!mob.HasBuff("kalistaexpungemarker")) continue;

        if (mob.MaxHealth() > 3000.0f) {
          // Large/Legendary — kill at 50% E damage
          if (mob.Health() < GetEDamage(mob) * 0.5f) { CastRend(); break; }
        } else if (mob.MaxHealth() > 1000.0f) {
          // Medium — kill at 50% E damage
          if (mob.Health() < GetEDamage(mob) * 0.5f) { CastRend(); break; }
        } else {
          // Small — full E damage
          if (mob.Health() < GetEDamage(mob)) { CastRend(); break; }
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // FlyHack (Kalista.cs lines 804-825)
  // ════════════════════════════════════════════════
  void FlyHack() {
    auto *comboMenu = m_menu->GetSubMenu("Combo");
    if (!comboMenu || !comboMenu->GetKeyBindValue("FlyHack")) return;

    if (Orbwalker::GetMode() == OrbwalkerMode::Combo && Player().AttackSpeedMod() > 2.0f) {
      const float aaRange = Player().AttackRange() + Player().BoundingRadius() + 100.0f;
      auto target = OrbwalkerSelector::GetTarget(Player(), Orbwalker::GetMode(), aaRange, Orbwalker::GetMenu());
      if (target.IsValid()) {
        if (Game::TickCount() - static_cast<int>(m_lastAATick) <= 10 + Game::Ping()) {
          Player().IssueOrder(GameObjectOrder::MoveTo, Game::CursorPos());
        }
        if (Game::TickCount() - static_cast<int>(m_lastAATick) >= Game::Ping()) {
          Player().IssueOrder(GameObjectOrder::AttackUnit, target);
          m_lastAATick = static_cast<float>(Game::TickCount());
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // LogicE (Kalista.cs lines 826-848)
  // ════════════════════════════════════════════════
  void LogicE() {
    auto *eSet = m_menu->GetSubMenu("Eset");
    if (!eSet) return;

    int eMode = eSet->GetListIndex("EMode", 1);
    bool harassPlus = eSet->GetBoolValue("harassPlus", true);
    if (!E.IsReady()) return;

    // Kill any enemy hero with E (Kalista.cs lines 833-838)
    if ((eMode == 0 && Orbwalker::GetMode() == OrbwalkerMode::Combo) || eMode == 1) {
      for (const auto& o : ObjectManager::EnemyHeroes()) {
        if (!o.IsValid() || !o.IsValidTarget(E.Range)) continue;
        if (o.HasBuff("kalistaexpungemarker") && o.Health() < GetRealDamage(o)) {
          CastRend();
          return;
        }
      }
      // Harass plus (Kalista.cs lines 839-845)
      if (harassPlus) {
        bool enemyInRange = false;
        bool minionKillable = false;
        for (const auto& o : ObjectManager::EnemyHeroes()) {
          if (o.IsValid() && o.IsValidTarget(E.Range)) { enemyInRange = true; break; }
        }
        for (const auto& m : ObjectManager::EnemyMinions()) {
          if (m.IsValid() && m.IsValidTarget(E.Range) && m.HasBuff("kalistaexpungemarker") &&
              m.Health() < GetRealDamage(m)) {
            minionKillable = true;
            break;
          }
        }
        if (enemyInRange && minionKillable) CastRend();
      }
    }
  }

  // ════════════════════════════════════════════════
  // RLogic (Kalista.cs lines 850-927)
  // ════════════════════════════════════════════════
  void RLogic() {
    auto *rSet = m_menu->GetSubMenu("Rset");
    if (!rSet || !R.IsReady()) return;

    bool saveAlly = rSet->GetBoolValue("kaliusersaveally", true);
    bool engage = rSet->GetBoolValue("userengage", true);

    auto *wowCombo = rSet->GetSubMenu("WowCombo");
    bool balista = wowCombo ? wowCombo->GetBoolValue("Balista", true) : true;
    bool talista = wowCombo ? wowCombo->GetBoolValue("Talista", true) : true;
    bool salista = wowCombo ? wowCombo->GetBoolValue("Salista", true) : true;

    // Find soulbound ally (Kalista.cs lines 857-858)
    AIHeroClient ally;
    for (const auto& a : ObjectManager::AllyHeroes()) {
      if (a.IsValid() && !a.IsMe() && !a.IsDead() && a.HasBuff("kalistacoopstrikeally")) {
        ally = a;
        break;
      }
    }

    if (ally.IsValid() && ally.IsVisible() && ally.DistanceToPlayer() <= R.Range) {
      // Save ally (Kalista.cs lines 862-865)
      if (saveAlly && Player().CountEnemyHeroesInRange(R.Range) > 0 &&
          ally.CountEnemyHeroesInRange(R.Range) > 0 && ally.HealthPercent() <= 30.0f) {
        R.Cast();
      }

      // Balista combo (Kalista.cs lines 881-891)
      if (balista && ally.CharacterName() == "Blitzcrank") {
        for (const auto& x : ObjectManager::EnemyHeroes()) {
          if (!x.IsValid() || x.IsDead() || !x.IsValidTarget()) continue;
          if (x.HasBuff("rocketgrab")) { R.Cast(); break; }
        }
      }

      // Talista combo (Kalista.cs lines 892-902)
      if (talista && ally.CharacterName() == "TahmKench") {
        for (const auto& x : ObjectManager::EnemyHeroes()) {
          if (!x.IsValid() || x.IsDead() || !x.IsValidTarget()) continue;
          if (x.HasBuff("tahmkenchwdevoured")) { R.Cast(); break; }
        }
      }

      // Salista combo (Kalista.cs lines 903-913)
      if (salista && ally.CharacterName() == "Skarner") {
        for (const auto& x : ObjectManager::EnemyHeroes()) {
          if (!x.IsValid() || x.IsDead() || !x.IsValidTarget()) continue;
          if (x.HasBuff("skarnerimpale")) { R.Cast(); break; }
        }
      }
    }

    // Engage with R (Kalista.cs lines 916-926)
    if (engage) {
      for (const auto& enemy : ObjectManager::EnemyHeroes()) {
        if (!enemy.IsValid() || !enemy.IsValidTarget(1000.0f)) continue;
        if (!SDK::Extensions::IsFacing(enemy, Player())) continue;
        // Check if enemy walking toward us
        float distToPlayer = enemy.Position().Distance(Player().Position());
        if (distToPlayer < 400.0f) {
          R.Cast();
          break;
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // Routine (Kalista.cs lines 960-1069)
  // ════════════════════════════════════════════════
  void Routine() {
    auto *ksMenu = m_menu->GetSubMenu("KS");
    auto *miscMenu = m_menu->GetSubMenu("Misc");
    if (!ksMenu || !miscMenu) return;

    // E KS jungle (Kalista.cs lines 963-1012)
    if (ksMenu->GetBoolValue("KSEJG", true) && E.IsReady()) {
      for (const auto& mob : ObjectManager::JungleMinions()) {
        if (!mob.IsValid() || !mob.IsValidTarget(E.Range)) continue;
        std::string mobName = mob.CharacterName();

        if (mobName.find("Baron") != std::string::npos) {
          float eDmg = EDamage(mob);
          if (Player().HasBuff("barontarget")) {
            if (mob.Health() < eDmg * 0.5f) CastRend();
          } else {
            if (mob.Health() < eDmg) CastRend();
          }
        } else if (mobName.find("Dragon") != std::string::npos) {
          float eDmg = EDamage(mob);
          if (Player().HasBuff("barontarget")) {
            float dragonReduction = 0.07f * Player().GetBuffCount("s5test_dragonslayerbuff");
            if (mob.Health() < eDmg * (1.0f - dragonReduction)) CastRend();
          } else {
            if (mob.Health() < eDmg) CastRend();
          }
        } else if (mobName.find("Mini") == std::string::npos) {
          if (mob.Health() < EDamage(mob)) CastRend();
        }
      }
    }

    // E leaving range (Kalista.cs lines 1014-1042)
    if (miscMenu->GetBoolValue("misc-leaving-e", true) && E.IsReady()) {
      auto enemy = TargetSelector::GetTarget(E.Range, SDK::DamageType::Physical);
      if (enemy.IsValid() && !enemy.IsDead()) {
        int reqStacks = miscMenu->GetSliderValue("misc-leaving-e-pro", 5);
        int stacks = enemy.GetBuffCount("kalistaexpungemarker");
        if (stacks >= reqStacks && enemy.IsValidTarget() &&
            enemy.Distance(Player()) > E.Range - 50.0f) {
          if (miscMenu->GetBoolValue("misc-prevent-e", true)) {
            if (!Compat::IsProtectedFromSpell(enemy, SDK::DamageType::Physical, GetEDamage(enemy))) {
              CastRend();
            }
          } else {
            CastRend();
          }
        }
      }
    }

    // E before dying (Kalista.cs lines 1044-1051)
    if (miscMenu->GetBoolValue("misc-dying-e", true)) {
      int dyingPct = miscMenu->GetSliderValue("misc-dying-e-pro", 10);
      if (Player().HealthPercent() <= static_cast<float>(dyingPct)) {
        CastRend();
      }
    }

    // Auto W trick (Kalista.cs lines 1053-1068)
    if (miscMenu->GetKeyBindValue("misc-ward-trick") && W.IsReady()) {
      Vector3 drakePos = { 9866.0f, -71.0f, 4414.0f };
      Vector3 baronPos = { 5007.0f, -71.0f, 10471.0f };

      if (W.Range >= Player().Distance(baronPos)) {
        W.Cast(baronPos);
      } else if (W.Range >= Player().Distance(drakePos)) {
        W.Cast(drakePos);
      }
    }
  }

  // ════════════════════════════════════════════════
  // Killsteal (Kalista.cs lines 1077-1113)
  // ════════════════════════════════════════════════
  void Killsteal() {
    auto *ksMenu = m_menu->GetSubMenu("KS");
    if (!ksMenu) return;

    // Q KS (Kalista.cs lines 1079-1092)
    if (ksMenu->GetBoolValue("KSQ", true) && Q.IsReady()) {
      for (const auto& target : ObjectManager::EnemyHeroes()) {
        if (!target.IsValid() || !target.IsValidTarget(Q.Range) || target.IsInvulnerable()) continue;
        if (target.Health() < QDamage(target)) {
          auto pred = Q.GetPrediction(target);
          if (pred.Hitchance >= HitChance::High) {
            Q.Cast(pred.CastPosition);
            break;
          }
        }
      }
    }

    // E KS (Kalista.cs lines 1094-1112)
    if (ksMenu->GetBoolValue("KSE", true) && E.IsReady()) {
      for (const auto& target : ObjectManager::EnemyHeroes()) {
        if (!target.IsValid() || !target.IsValidTarget(E.Range) || target.IsInvulnerable()) continue;
        if (target.HasBuff("kalistaexpungemarker") && target.Health() < GetEDamage(target)) {
          CastRend();
          break;
        }
      }
    }
  }
};

} // namespace Plugins
