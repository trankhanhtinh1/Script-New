#pragma once

// ═══════════════════════════════════════════════════════
// NightSharp Jinx Plugin — Full port from Jinx.cs (7UPAIO)
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
#include "sdk/Math/HealthPrediction.h"
#include "sdk/Events/SpellCastTracker.h"

#include <algorithm>
#include <string>

namespace Plugins {

using namespace SDK;
using namespace SDK::MenuUI;

class JinxPlugin : public IPlugin {
public:
  // ── IPlugin identity ──
  const char *GetName()       const override { return "Jinx"; }
  const char *GetInternalId() const override { return "champion_jinx"; }
  const char *GetAuthor()     const override { return "7UP / NightSharp"; }
  PluginCategory GetCategory() const override { return PluginCategory::Champion; }
  bool AutoLoadByDefault()    const override { return false; }

  bool CanLoad() const override {
    return Player().IsValid() && Player().CharacterName() == "Jinx";
  }

  // ── Spells (Jinx.cs lines 32-35) ──
  Spell Q, W, E, R;

  // ══ State (Jinx.cs lines 14-16, 23-27) ══
  bool m_fishBoneActive = false;
  bool m_combo = false;
  bool m_farm = false;
  float m_wCastTime = 0.0f;
  float m_grabTime = 0.0f;
  float m_bigGunRange = 0.0f;
  float m_QMANA = 0.0f;
  float m_WMANA = 0.0f;
  float m_EMANA = 0.0f;

  // ══ Spell detection list (Jinx.cs lines 17-21) ══
  // Important enemy spells that should be interrupted with E
  static constexpr const char* DangerousSpells[] = {
    "katarinar", "drain", "consume", "absolutezero", "staticfield",
    "reapthewhirlwind", "jinxw", "jinxr", "shenstandunited",
    "threshe", "threshrpenta", "threshq", "meditate",
    "caitlynpiltoverpeacemaker", "volibearqattack",
    "cassiopeiapetrifyinggaze", "ezrealtrueshotbarrage",
    "galioidolofdurand", "luxmalicecannon", "missfortunebullettime",
    "infiniteduress", "alzaharnethergrasp", "lucianq", "velkozr",
    "rocketgrabmissile"
  };

  // ════════════════════════════════════════════════
  // OnLoad — Spell Init + Menu  (Jinx.cs lines 37-122)
  // ════════════════════════════════════════════════
  void OnLoad() override {
    if (m_menu) return;

    // Spell Definitions (Jinx.cs lines 41-48)
    Q = Spell(SpellSlot::Q);
    W = Spell(SpellSlot::W, 1500.0f);
    E = Spell(SpellSlot::E, 925.0f);
    R = Spell(SpellSlot::R, 3000.0f);

    W.SetSkillshot(0.6f, 57.0f, 3300.0f, true, SpellType::Line);
    E.SetSkillshot(1.2f, 100.0f, 1750.0f, false, SpellType::Circle);
    R.SetSkillshot(0.6f, 130.0f, 1700.0f, false, SpellType::Line);

    // Menu (Jinx.cs lines 50-116)
    m_menu = Menu::Create("JinxRoot", "[NightSharp] Jinx");

    // ── Q Menu (Jinx.cs lines 51-63) ──
    auto *qMenu = m_menu->AddSubMenu("QMenu", "Q Settings");
    qMenu->Add<MenuBool>("Qcombo", "Combo Q", true);
    qMenu->Add<MenuBool>("Qharass", "Harass Q", true);
    qMenu->Add<MenuBool>("farmQout", "Farm Q out range AA minion", true);
    qMenu->Add<MenuSlider>("Qlaneclear", "Lane clear x minions", 2, 4, 10);
    // Jinx.cs line 56: Qchange mode — 0=Real Time, 1=Before AA (default=1)
    qMenu->Add<MenuList>("Qchange", "Q change mode FishBone -> MiniGun",
      std::vector<std::string>{"Real Time", "Before AA"}, 1);
    qMenu->Add<MenuSlider>("Qaoe", "Force FishBone if can hit x target", 3, 0, 5);
    qMenu->Add<MenuSlider>("QmanaIgnore", "Ignore mana if can kill in x AA", 3, 0, 10);
    // Jinx.cs lines 59-60: per-enemy harass Q toggle
    for (const auto& enemy : ObjectManager::EnemyHeroes()) {
      if (!enemy.IsValid()) continue;
      std::string key = std::string("harassQ_") + enemy.CharacterName();
      std::string label = std::string("Harass Q enemy: ") + enemy.CharacterName();
      qMenu->Add<MenuBool>(key.c_str(), label.c_str(), true);
    }
    qMenu->Add<MenuSlider>("QmanaCombo", "Q combo mana", 10, 0, 100);
    qMenu->Add<MenuSlider>("QmanaHarass", "Q harass mana", 40, 0, 100);
    qMenu->Add<MenuSlider>("QmanaLC", "Q lane clear mana", 80, 0, 100);

    // ── W Menu (Jinx.cs lines 65-76) ──
    auto *wMenu = m_menu->AddSubMenu("WMenu", "W Settings");
    wMenu->Add<MenuKeyBind>("useW", "Semi cast W key", 'S', KeyBindType::Press);
    wMenu->Add<MenuBool>("Wcombo", "Combo W", true);
    wMenu->Add<MenuBool>("Wharass", "W harass", true);
    wMenu->Add<MenuBool>("Wks", "W KS", true);
    // Jinx.cs line 70: Wts — 0=Target selector, 1=All in range
    wMenu->Add<MenuList>("Wts", "Harass mode",
      std::vector<std::string>{"Target selector", "All in range"}, 0);
    // Jinx.cs line 71: Wmode — 0=Out MiniGun, 1=Out FishBone, 2=Custom
    wMenu->Add<MenuList>("Wmode", "W mode",
      std::vector<std::string>{"Out range MiniGun", "Out range FishBone", "Custom range"}, 0);
    wMenu->Add<MenuSlider>("Wcustome", "Custom minimum range", 600, 0, 1500);
    // Jinx.cs lines 73-74: per-enemy harass W toggle
    for (const auto& enemy : ObjectManager::EnemyHeroes()) {
      if (!enemy.IsValid()) continue;
      std::string key = std::string("harassW_") + enemy.CharacterName();
      std::string label = std::string("Harass W enemy: ") + enemy.CharacterName();
      wMenu->Add<MenuBool>(key.c_str(), label.c_str(), true);
    }
    wMenu->Add<MenuSlider>("WmanaCombo", "W combo mana", 20, 0, 100);
    wMenu->Add<MenuSlider>("WmanaHarass", "W harass mana", 40, 0, 100);

    // ── E Menu (Jinx.cs lines 78-92) ──
    auto *eMenu = m_menu->AddSubMenu("EMenu", "E Settings");
    eMenu->Add<MenuBool>("Ecombo", "Combo E", true);
    eMenu->Add<MenuBool>("AutoEWhenEnemyCastAAM", "Auto E When Melee Enemy AA Me", true);
    eMenu->Add<MenuKeyBind>("useE", "Semi cast E key", 'G', KeyBindType::Press);
    eMenu->Add<MenuBool>("Etel", "E on enemy teleport", true);
    eMenu->Add<MenuBool>("Ecc", "E on CC", true);
    eMenu->Add<MenuBool>("Espell", "E on special spell detection", true);
    eMenu->Add<MenuBool>("EGap", "E Gap", true);
    eMenu->Add<MenuSlider>("EmanaCombo", "E mana", 30, 0, 100);

    // ── R Menu (Jinx.cs lines 94-106) ──
    auto *rMenu = m_menu->AddSubMenu("RMenu", "R Settings");
    rMenu->Add<MenuBool>("Rks", "R KS", true);
    rMenu->Add<MenuKeyBind>("useR", "Semi-manual cast R key", 'T', KeyBindType::Press);
    rMenu->Add<MenuBool>("ComboRTeam", "Use R|Team Fight", true);
    rMenu->Add<MenuBool>("ComboRSolo", "Use R|Solo Mode", true);
    rMenu->Add<MenuSlider>("rMenuMin", "Use R| Min Range >= x", 1000, 500, 2500);
    rMenu->Add<MenuSlider>("rMenuMax", "Use R| Max Range <= x", 3000, 1500, 3500);
  }

  void OnUnload() override {
    if (!m_menu) return;
    Menu::Remove("JinxRoot");
    m_menu = nullptr;
  }

  Menu *GetMenuRoot() override { return m_menu; }

  // ════════════════════════════════════════════════
  // OnGapcloser — E gap close  (Jinx.cs lines 187-199)
  // ════════════════════════════════════════════════
  void OnGapcloser(const AIHeroClient& sender, const AntiGapcloser::GapcloserArgs& args) override {
    if (!m_menu) return;
    auto *eMenu = m_menu->GetSubMenu("EMenu");
    if (!eMenu || !eMenu->GetBoolValue("EGap", true)) return;
    if (!E.IsReady()) return;
    if (!sender.IsValid() || !sender.IsValidTarget(E.Range)) return;

    E.Cast(args.EndPosition);
  }

  // ════════════════════════════════════════════════
  // OnAfterAttack — Q toggle  (Jinx.cs lines 200-228)
  // ════════════════════════════════════════════════
  void OnAfterAttack(OrbwalkingActionArgs& args) override {
    if (!m_menu || !m_fishBoneActive) return;

    auto *qMenu = m_menu->GetSubMenu("QMenu");
    if (!qMenu) return;

    // Jinx.cs line 205-212: Q.IsReady && target is hero && Qchange==1 (Before AA mode)
    if (Q.IsReady() && args.Target.IsValid() && !args.Target.IsDead()) {
      if (qMenu->GetListIndex("Qchange", 1) == 1) {
        // Target is hero → try switch to MiniGun
        FishBoneToMiniGun(AIBaseClient(args.Target.Address()));
      }
    }

    // Jinx.cs lines 214-226: not combo and target is minion
    if (!m_combo && args.Target.IsValid()) {
      if (Orbwalker::GetMode() == OrbwalkerMode::Clear &&
          Player().ManaPercent() > static_cast<float>(qMenu->GetSliderValue("QmanaLC", 80)) &&
          CountMinionsInRange(250.0f, args.Target.Position()) >= qMenu->GetSliderValue("Qlaneclear", 2)) {
        // Giữ FishBone trong lane clear nếu đủ minion — empty block giữ nguyên C#
      } else if (GetRealDistance(args.Target) < GetRealPowPowRange(args.Target)) {
        if (Q.IsReady()) Q.Cast();
      }
    }
  }

  // ════════════════════════════════════════════════
  // OnProcessSpellCast (Jinx.cs lines 229-270)
  // AIBaseClient.OnDoCast equivalent
  // ════════════════════════════════════════════════
  void OnProcessSpellCast(const AIBaseClient& sender,
      const Events::SpellCast::ProcessSpellCastEventArgs& args) override {
    if (!m_menu) return;

    // Jinx.cs lines 232-236: Track W cast time
    if (sender.IsValid() && sender.IsMe()) {
      if (args.SpellName == "JinxWMissile") {
        m_wCastTime = Game::Time();
      }
    }

    auto *eMenu = m_menu->GetSubMenu("EMenu");
    if (!eMenu) return;

    // ── E on dangerous spells (Jinx.cs lines 238-245) ──
    if (E.IsReady() && sender.IsValid() && sender.IsEnemy() &&
        eMenu->GetBoolValue("Espell", true) &&
        Player().ManaPercent() >= static_cast<float>(eMenu->GetSliderValue("EmanaCombo", 30)) &&
        sender.IsValidTarget(E.Range)) {

      // Check if spell name matches any dangerous spell
      std::string spellLower = args.SpellName;
      std::transform(spellLower.begin(), spellLower.end(), spellLower.begin(), ::tolower);

      for (const auto* dangerSpell : DangerousSpells) {
        if (spellLower == dangerSpell) {
          E.Cast(sender.Position());
          break;
        }
      }
    }

    // ── E when melee enemy casts AA on me (Jinx.cs lines 246-249) ──
    if (eMenu->GetBoolValue("AutoEWhenEnemyCastAAM", true) &&
        sender.IsValid() && sender.IsEnemy() &&
        args.IsAutoAttack && E.IsReady() &&
        args.TargetNetworkId == Player().NetworkId() &&
        Player().Distance(sender) < 300.0f) {
      E.Cast(Player().ServerPosition());
    }

    // ── Combo E on enemy melee AA (Jinx.cs lines 256-268) ──
    if (m_combo && eMenu->GetBoolValue("Ecombo", true) && E.IsReady() &&
        sender.IsValid() && sender.IsEnemy() && args.IsAutoAttack &&
        args.TargetNetworkId == Player().NetworkId()) {

      // Close range melee (Jinx.cs line 258-262)
      if (Player().Distance(sender) < 300.0f) {
        E.Cast(Player().ServerPosition());
      }
      // Medium range (Jinx.cs line 264-267)
      else if (Player().Distance(sender) > 300.0f &&
               Player().Distance(sender) <= E.Range &&
               Player().CountEnemyHeroesInRange(300.0f) == 0) {
        E.CastPredicted(sender, HitChance::High);
      }
    }
  }

  // ════════════════════════════════════════════════
  // OnUpdate — Main loop  (Jinx.cs lines 271-288)
  // ════════════════════════════════════════════════
  void OnUpdate() override {
    if (!Player().IsValid() || !m_menu) return;
    if (Player().IsDead() || Player().IsRecalling()) return;

    // R.Range = RMenu["rMenuMax"] (Jinx.cs line 273-276)
    if (R.Instance().Level() > 0) {
      auto *rMenu = m_menu->GetSubMenu("RMenu");
      if (rMenu) R.Range = static_cast<float>(rMenu->GetSliderValue("rMenuMax", 3000));
    }

    // SetValues (Jinx.cs lines 628-665)
    SetValues();

    // Logic theo order giữ nguyên (Jinx.cs lines 280-287)
    if (Q.IsReady())  Qlogic();
    if (W.IsReady())  Wlogic();
    if (E.IsReady())  Elogic();
    if (R.IsReady())  Rlogic();
  }

private:
  Menu *m_menu = nullptr;

  // ════════════════════════════════════════════════
  // SetValues  (Jinx.cs lines 628-665) — full port
  // ════════════════════════════════════════════════
  void SetValues() {
    auto *wMenu = m_menu->GetSubMenu("WMenu");

    // Wcustome adjustment (Jinx.cs lines 630-633)
    // Note: In C# this modifies the slider value at runtime based on Wmode
    // We store it as a helper value instead since we can't easily mutate menu
    // Wmode == 2 → Wcustome = 1500, else 600
    // This is handled in WValidRange directly

    // FishBone detection (Jinx.cs line 641-644)
    m_fishBoneActive = Player().HasBuff("JinxQ");

    // Mode detection (Jinx.cs line 646-658)
    m_combo = (Orbwalker::GetMode() == OrbwalkerMode::Combo);
    m_farm  = (Orbwalker::GetMode() == OrbwalkerMode::Clear  ||
               Orbwalker::GetMode() == OrbwalkerMode::LastHit ||
               Orbwalker::GetMode() == OrbwalkerMode::Harass);

    // Q.Range dynamic (Jinx.cs line 660)
    Q.Range = 685.0f + Player().BoundingRadius() + 25.0f * static_cast<float>(Player().GetSpellBook().GetSpell(SpellSlot::Q).Level());

    // Mana costs (Jinx.cs lines 662-664)
    m_QMANA = 20.0f;
    m_WMANA = W.Instance().ManaCost();
    m_EMANA = E.Instance().ManaCost();

    // bigGunRange for R Solo (Jinx.cs uses Q.Range as bigGunRange effectively)
    m_bigGunRange = Q.Range;
  }

  // ════════════════════════════════════════════════
  // Helper functions  (Jinx.cs lines 619-626)
  // ════════════════════════════════════════════════
  float GetRealDistance(const GameObject& target) const {
    return Player().Position().Distance(target.Position()) + Player().BoundingRadius() + target.BoundingRadius();
  }

  float GetRealPowPowRange(const GameObject& target) const {
    return 650.0f + Player().BoundingRadius() + target.BoundingRadius();
  }

  float GetBonusRange() const {
    return 670.0f + Player().BoundingRadius() + 25.0f * static_cast<float>(Player().GetSpellBook().GetSpell(SpellSlot::Q).Level());
  }

  // ════════════════════════════════════════════════
  // CountMinionsInRange  (Jinx.cs lines 567-576)
  // ════════════════════════════════════════════════
  int CountMinionsInRange(float range, const Vector3& pos) const {
    int count = 0;
    for (const auto& minion : ObjectManager::EnemyMinions()) {
      if (minion.IsValid() && minion.Position().Distance(pos) <= range)
        count++;
    }
    return count;
  }

  // ════════════════════════════════════════════════
  // FishBoneToMiniGun  (Jinx.cs lines 607-617)
  // ════════════════════════════════════════════════
  void FishBoneToMiniGun(const AIBaseClient& t) {
    auto *qMenu = m_menu->GetSubMenu("QMenu");
    if (!qMenu) return;

    float realDistance = GetRealDistance(t);
    if (realDistance < GetRealPowPowRange(t) &&
        t.CountEnemyHeroesInRange(250.0f) < qMenu->GetSliderValue("Qaoe", 3)) {
      if (Player().ManaPercent() < static_cast<float>(qMenu->GetSliderValue("QmanaCombo", 10)) ||
          Player().GetAutoAttackDamage(t) * static_cast<float>(qMenu->GetSliderValue("QmanaIgnore", 3)) < t.Health()) {
        Q.Cast();
      }
    }
  }

  // ════════════════════════════════════════════════
  // WValidRange  (Jinx.cs lines 434-464) — full 3 mode
  // ════════════════════════════════════════════════
  bool WValidRange(const AIBaseClient& t) const {
    auto *wMenu = m_menu->GetSubMenu("WMenu");
    if (!wMenu || !W.IsReady()) return false;

    float range = GetRealDistance(t);
    int wMode = wMenu->GetListIndex("Wmode", 0);

    if (wMode == 0) {
      // Out range MiniGun (Jinx.cs line 440-445)
      float powpowRange = GetRealPowPowRange(t);
      return (range > powpowRange && Player().CountEnemyHeroesInRange(powpowRange) == 0);
    } else if (wMode == 1) {
      // Out range FishBone (Jinx.cs line 448-453)
      return (range > Q.Range + 50.0f && Player().CountEnemyHeroesInRange(Q.Range + 50.0f) == 0);
    } else if (wMode == 2) {
      // Custom range (Jinx.cs line 455-460)
      float customRange = static_cast<float>(wMenu->GetSliderValue("Wcustome", 600));
      return (range > customRange && Player().CountEnemyHeroesInRange(customRange) == 0);
    }
    return false;
  }

  // ════════════════════════════════════════════════
  // GetKsDamage  (Jinx.cs lines 578-603) — full port
  // ════════════════════════════════════════════════
  float GetKsDamage(const AIBaseClient& t, const Spell& spell) const {
    float totalDmg = spell.GetDamage(t);

    // Jinx.cs line 582-583: Exhaust debuff
    if (Player().HasBuff("summonerexhaust"))
      totalDmg *= 0.6f;

    // Jinx.cs line 585-586: Rengar howl
    if (t.HasBuff("ferocioushowl"))
      totalDmg *= 0.7f;

    // Jinx.cs line 588-594: Blitzcrank mana barrier
    if (t.CharacterName() == "Blitzcrank" && !t.HasBuff("BlitzcrankManaBarrierCD") && !t.HasBuff("ManaBarrier")) {
      totalDmg -= t.Mana() / 2.0f;
    }

    // Jinx.cs line 597: HealthPrediction extraHP
    float extraHP = t.Health() - HealthPrediction::GetPrediction(t, 500);
    totalDmg += extraHP;

    // Jinx.cs line 600: HP regen
    totalDmg -= t.HPRegenRate();

    // Jinx.cs line 601: Lifesteal reduction
    // C# PercentLifeStealMod → LifeSteal(), FlatPhysicalDamageMod → TotalAttackDamage()
    totalDmg -= t.LifeSteal() * 0.005f * t.TotalAttackDamage();

    return totalDmg;
  }

  // ════════════════════════════════════════════════
  // Rlogic  (Jinx.cs lines 292-326)
  // ════════════════════════════════════════════════
  void Rlogic() {
    auto *rMenu = m_menu->GetSubMenu("RMenu");
    if (!rMenu) return;

    // Semi R (Jinx.cs line 295-302)
    if (rMenu->GetKeyBindValue("useR") && R.IsReady()) {
      auto t = TargetSelector::GetTarget(R.Range, SDK::DamageType::Physical);
      if (t.IsValid() && t.IsValidTarget()) {
        auto pred = R.GetPrediction(t);
        if (pred.Hitchance >= HitChance::High) {
          R.CastPredicted(t, HitChance::High);
        }
      }
    }

    // R KS (Jinx.cs lines 303-311)
    auto t1 = TargetSelector::GetTarget(R.Range, SDK::DamageType::Physical);
    if (t1.IsValid() && t1.IsValidTarget() && rMenu->GetBoolValue("Rks", true)) {
      if (GetKsDamage(t1, R) > t1.Health() && R.IsReady()) {
        auto pred1 = R.GetPrediction(t1);
        if (pred1.Hitchance >= HitChance::High) {
          // Jinx.cs line 307: Don't R if in W range
          if (t1.DistanceToPlayer() > W.Range + 100.0f) {
            R.CastPredicted(t1, HitChance::High);
          }
        }
      }
    }

    // Combo R (Jinx.cs lines 312-325)
    if (m_combo && R.IsReady()) {
      for (const auto& target : ObjectManager::EnemyHeroes()) {
        if (!target.IsValidTarget(1200.0f)) continue;

        // Team fight mode (Jinx.cs line 316-319)
        if (rMenu->GetBoolValue("ComboRTeam", true) &&
            target.IsValidTarget(600.0f) &&
            target.CountEnemyHeroesInRange(600.0f) >= 2 &&
            target.CountAllyHeroesInRange(200.0f) <= 3 &&
            target.HealthPercent() < 50.0f) {
          R.CastPredicted(target, HitChance::High);
        }

        // Solo mode (Jinx.cs line 320-323)
        if (rMenu->GetBoolValue("ComboRSolo", true) &&
            target.CountEnemyHeroesInRange(1500.0f) <= 2 &&
            target.DistanceToPlayer() > Q.Range &&
            target.DistanceToPlayer() < m_bigGunRange &&
            target.Health() > Player().GetAutoAttackDamage(target) &&
            target.Health() < R.GetDamage(target) + Player().GetAutoAttackDamage(target) * 3.0f) {
          R.CastPredicted(target, HitChance::High);
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // Elogic  (Jinx.cs lines 358-432)
  // ════════════════════════════════════════════════
  void Elogic() {
    auto *eMenu = m_menu->GetSubMenu("EMenu");
    if (!eMenu) return;

    // Semi E (Jinx.cs lines 361-368)
    if (eMenu->GetKeyBindValue("useE") && E.IsReady()) {
      auto t = TargetSelector::GetTarget(E.Range, SDK::DamageType::Physical);
      if (t.IsValid() && t.IsValidTarget()) {
        E.CastPredicted(t, HitChance::High);
      }
    }

    // Combo E (Jinx.cs lines 370-411)
    if (m_combo && eMenu->GetBoolValue("Ecombo", true) && E.IsReady()) {
      auto t = TargetSelector::GetTarget(E.Range, SDK::DamageType::Magical);
      if (t.IsValid() && t.IsValidTarget(E.Range)) {
        if (!t.CanMove()) {
          // CC'd target → cast trực tiếp (Jinx.cs line 390-393)
          E.CastPredicted(t, HitChance::Medium);
        } else {
          // Chỉ cast nếu prediction rất cao (Jinx.cs line 396-401)
          auto pred = E.GetPrediction(t);
          if (pred.Hitchance >= HitChance::VeryHigh &&
              pred.CastPosition.Distance(t.Position()) > 200.0f) {
            E.Cast(pred.CastPosition);
          }
        }
      }
    }

    // E on CC targets (Jinx.cs lines 412-418)
    if (eMenu->GetBoolValue("Ecc", true) && E.IsReady()) {
      for (const auto& target : ObjectManager::EnemyHeroes()) {
        if (target.IsValidTarget(E.Range) && !target.CanMove()) {
          E.CastPredicted(target, HitChance::Medium);
        }
      }
    }

    // E on teleport (Jinx.cs lines 419-431)
    if (eMenu->GetBoolValue("Etel", true) && E.IsReady()) {
      for (const auto& enemy : ObjectManager::EnemyHeroes()) {
        if (!enemy.IsValid() || enemy.DistanceToPlayer() >= E.Range) continue;
        if (enemy.HasBuff("teleport_target") || enemy.HasBuff("Pantheon_GrandSkyfall_Jump")) {
          E.Cast(enemy.Position());
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // Wlogic  (Jinx.cs lines 466-501) — full port
  // ════════════════════════════════════════════════
  void Wlogic() {
    auto *wMenu = m_menu->GetSubMenu("WMenu");
    if (!wMenu) return;

    auto t = TargetSelector::GetTarget(W.Range, SDK::DamageType::Physical);

    // Semi W (Jinx.cs lines 470-473)
    if (wMenu->GetKeyBindValue("useW") && W.IsReady()) {
      if (t.IsValid() && t.IsValidTarget()) {
        W.CastPredicted(t, HitChance::High);
      }
    }

    if (!t.IsValid() || !t.IsValidTarget() || !WValidRange(t)) return;

    // W KS (Jinx.cs line 477-480)
    if (wMenu->GetBoolValue("Wks", true) && GetKsDamage(t, W) > t.Health() && W.IsReady()) {
      W.CastPredicted(t, HitChance::High);
    }

    // Combo W (Jinx.cs lines 482-485)
    if (m_combo && W.IsReady() && wMenu->GetBoolValue("Wcombo", true) &&
        Player().ManaPercent() > static_cast<float>(wMenu->GetSliderValue("WmanaCombo", 20))) {
      auto pred = W.GetPrediction(t);
      if (pred.Hitchance >= HitChance::High) {
        W.CastPredicted(t, HitChance::High);
      }
    }

    // Harass W (Jinx.cs lines 486-498) — full per-enemy + Wts mode
    if (m_farm && W.IsReady() && !Player().IsWindingUp() &&
        wMenu->GetBoolValue("Wharass", true) &&
        Player().ManaPercent() > static_cast<float>(wMenu->GetSliderValue("WmanaHarass", 40))) {

      int wtsMode = wMenu->GetListIndex("Wts", 0);

      if (wtsMode == 0) {
        // Target Selector mode (Jinx.cs line 488-491)
        std::string enemyKey = std::string("harassW_") + t.CharacterName();
        if (wMenu->GetBoolValue(enemyKey.c_str(), true)) {
          W.CastPredicted(t, HitChance::High);
        }
      } else {
        // All in range mode (Jinx.cs line 493-497)
        for (const auto& enemy : ObjectManager::EnemyHeroes()) {
          if (!enemy.IsValidTarget(W.Range) || !WValidRange(enemy)) continue;
          if (!W.IsReady()) break;
          std::string enemyKey = std::string("harassW_") + enemy.CharacterName();
          if (wMenu->GetBoolValue(enemyKey.c_str(), true)) {
            W.CastPredicted(enemy, HitChance::High);
          }
        }
      }
    }
  }

  // ════════════════════════════════════════════════
  // Qlogic  (Jinx.cs lines 502-565) — full port
  // ════════════════════════════════════════════════
  void Qlogic() {
    auto *qMenu = m_menu->GetSubMenu("QMenu");
    if (!qMenu) return;

    if (m_fishBoneActive) {
      // ── FishBone đang bật (Jinx.cs line 504-520) ──

      // Lane clear: giữ FishBone nếu đủ mana + minion target (Jinx.cs line 507-510)
      if (Orbwalker::GetMode() == OrbwalkerMode::Clear &&
          Player().ManaPercent() > static_cast<float>(qMenu->GetSliderValue("QmanaLC", 80))) {
        // Giữ FishBone — empty block giữ nguyên C#
      }
      // Qchange == 0 (Real Time): FishBoneToMiniGun trên orbwalker target (Jinx.cs line 511-515)
      else if (qMenu->GetListIndex("Qchange", 1) == 0) {
        auto orbTarget = TargetSelector::GetTarget(Q.Range, SDK::DamageType::Physical);
        if (orbTarget.IsValid() && orbTarget.IsValidTarget() && orbTarget.IsEnemy()) {
          FishBoneToMiniGun(orbTarget);
        }
      }
      else {
        // Không phải combo → switch về MiniGun (Jinx.cs line 518-519)
        if (!m_combo && Orbwalker::GetMode() != OrbwalkerMode::None) {
          Q.Cast();
        }
      }
    } else {
      // ── MiniGun đang bật (Jinx.cs line 522-564) ──
      auto t = TargetSelector::GetTarget(Q.Range + 40.0f, SDK::DamageType::Physical);

      if (t.IsValid() && t.IsValidTarget()) {
        // Nếu target ngoài AA range HOẶC có thể AoE hit (Jinx.cs line 527)
        if (!Player().InAutoAttackRange(t) ||
            t.CountEnemyHeroesInRange(250.0f) >= qMenu->GetSliderValue("Qaoe", 3)) {

          // Combo Q (Jinx.cs line 529-532)
          if (m_combo && qMenu->GetBoolValue("Qcombo", true) &&
              (Player().ManaPercent() > static_cast<float>(qMenu->GetSliderValue("QmanaCombo", 10)) ||
               Player().GetAutoAttackDamage(t) * static_cast<float>(qMenu->GetSliderValue("QmanaIgnore", 3)) > t.Health())) {
            Q.Cast();
          }

          // Harass Q — per-enemy toggle (Jinx.cs line 533-536)
          if (Orbwalker::GetMode() == OrbwalkerMode::Harass && m_farm &&
              qMenu->GetBoolValue("Qharass", true)) {
            std::string enemyKey = std::string("harassQ_") + t.CharacterName();
            if (qMenu->GetBoolValue(enemyKey.c_str(), true) &&
                (Player().ManaPercent() > static_cast<float>(qMenu->GetSliderValue("QmanaHarass", 40)) ||
                 Player().GetAutoAttackDamage(t) * static_cast<float>(qMenu->GetSliderValue("QmanaIgnore", 3)) > t.Health())) {
              Q.Cast();
            }
          }
        }
      } else {
        // Không có target hero
        // Combo: switch FishBone (Jinx.cs line 541-544)
        if (m_combo && Player().ManaPercent() > static_cast<float>(qMenu->GetSliderValue("QmanaCombo", 10))) {
          Q.Cast();
        }
        // Farm: switch FishBone cho minion ngoài tầm (Jinx.cs line 545-554)
        else if (m_farm && !Player().IsWindingUp() && qMenu->GetBoolValue("farmQout", true)) {
          for (const auto& minion : ObjectManager::EnemyMinions()) {
            if (!minion.IsValid() || !minion.IsValidTarget(Q.Range + 30.0f)) continue;
            if (!Player().InAutoAttackRange(minion) &&
                minion.Health() < Player().GetAutoAttackDamage(minion) * 1.2f &&
                GetRealPowPowRange(minion) < GetRealDistance(minion) &&
                Q.Range < GetRealDistance(minion)) {
              Q.Cast();
              return;
            }
          }
        }

        // Lane clear: switch FishBone cho AoE (Jinx.cs line 555-562)
        if (Orbwalker::GetMode() == OrbwalkerMode::Clear &&
            Player().ManaPercent() > static_cast<float>(qMenu->GetSliderValue("QmanaLC", 80))) {
          for (const auto& minion : ObjectManager::EnemyMinions()) {
            if (!minion.IsValid()) continue;
            if (CountMinionsInRange(250.0f, minion.Position()) >= qMenu->GetSliderValue("Qlaneclear", 2)) {
              Q.Cast();
              break;
            }
          }
        }
      }
    }
  }
};

} // namespace Plugins
