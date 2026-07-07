#pragma once

// ============================================================================
// SharpShooter AIO — Kog'Maw
// Port từ SharpShooterCSHarp/Plugins/KogMaw.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h.
//
// Kỹ năng:
//   Q Caustic Spittle    — skillshot line 950, delay 0.25, width 70, speed 1650, collision.
//   W Bio-Arcane Barrage — self-buff (tăng tầm đánh), cast trước khi đánh thường.
//                          Range động = 565 + 60 + level*30 + 65.
//   E Void Ooze          — skillshot line 1260, delay 0.5, width 120, speed 1400.
//   R Living Artillery    — skillshot circle, range động = 900 + level*300, delay 1.5,
//                          radius 225. Có cost-stack buff "kogmawlivingartillerycost".
//
// Ghi chú port:
//   * "Keep Mana For W": giữ đủ mana để còn cast W (mana - spellCost >= W.ManaCost).
//   * R Stacks Limit: chỉ tự bắn R khi số stack cost hiện tại < ngưỡng; nếu vượt
//     ngưỡng thì chỉ bắn R để kết liễu (killable).
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::KogMaw {

using SDK::Core::Utils::AutoAttack;

inline const char* const kRCostBuff = "kogmawlivingartillerycost";

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 950.0f };
inline Spell W{ SpellSlot::W, FLT_MAX };
inline Spell E{ SpellSlot::E, 1260.0f };
inline Spell R{ SpellSlot::R, 1000.0f };

inline bool Loaded = false;
inline DWORD LastComboEvalTick = 0;

static AIHeroClient Player() {
    return ObjectManager::Player();
}

static bool Bool(Menu* menu, const char* key, bool fallback = true) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuBool>(key);
    return item ? item->Value : fallback;
}

static int Slider(Menu* menu, const char* key, int fallback = 0) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuSlider>(key);
    return item ? item->Value : fallback;
}

static bool ShouldRunNow(DWORD& lastTick, DWORD intervalMs) {
    const DWORD now = GetTickCount();
    if (lastTick != 0 && now - lastTick < intervalMs) {
        return false;
    }
    lastTick = now;
    return true;
}

static bool ManaOkay(int percent) {
    const auto player = Player();
    return player.IsValid() && player.ManaPercent() >= static_cast<float>(percent);
}

static bool ValidUnit(const AttackableUnit& unit) {
    return unit.IsValid() && !unit.IsDead() && unit.Health() > 0.0f;
}

static bool ValidTarget(const AIBaseClient& unit, float range = FLT_MAX) {
    return ValidUnit(unit) && Extensions::IsValidTarget(unit, range, true);
}

static bool ValidHeroTarget(const AIHeroClient& hero, float range = FLT_MAX) {
    return ValidUnit(hero) && Extensions::IsValidTarget(hero, range, true);
}

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
}

static float HealthRegenRate(const AIBaseClient& unit) {
    return unit.IsValid() ? ::CoreAIHeroClient::HealthRegenRate(unit.Address()) : 0.0f;
}

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static AIHeroClient GetTargetNoCollision(Spell& spell) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTargetNoCollision(&spell) : AIHeroClient();
}

// Rút gọn IsKillableAndValidTarget: loại buff bất tử phổ biến + so máu/hồi/shield.
static bool IsKillable(const AIBaseClient& target, double calculatedDamage, DamageType damageType) {
    if (!ValidUnit(target)) {
        return false;
    }
    if (target.HasBuff("kindredrnodeathbuff") ||
        target.HasBuff("Undying Rage") ||
        target.HasBuff("JudicatorIntervention") ||
        target.HasBuff("BansheesVeil") ||
        target.HasBuff("SivirShield") ||
        target.HasBuff("ShroudofDarkness")) {
        return false;
    }
    const float shield = damageType == DamageType::Physical
        ? target.PhysicalShield()
        : target.MagicalShield();
    return target.Health() + HealthRegenRate(target) + shield < calculatedDamage - 2.0;
}

// Còn đủ mana để cast W sau khi tiêu spellCost? (Keep Mana For W)
static bool KeepManaForW(Menu* menu, Spell& spell) {
    if (!Bool(menu, "keepManaForW")) {
        return true;
    }
    if (W.Level() <= 0) {
        return true;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return true;
    }
    const float spellCost = spell.Instance().ManaCost();
    const float wCost = W.Instance().ManaCost();
    return player.Mana() - spellCost >= wCost;
}

static int RCostStacks() {
    const auto player = Player();
    return player.IsValid() ? player.GetBuffCount(kRCostBuff) : 0;
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void Combo();
static void Mixed();
static void Clear();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Kog'Maw", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useW", "Use W"));
    ComboMenu->Add(new MenuBool("useE", "Use E"));
    ComboMenu->Add(new MenuBool("useR", "Use R"));
    ComboMenu->Add(new MenuSlider("rStacks", "R Stacks Limit", 3, 1, 6));
    ComboMenu->Add(new MenuBool("keepManaForW", "Keep Mana For W"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuBool("useE", "Use E"));
    HarassMenu->Add(new MenuBool("useR", "Use R"));
    HarassMenu->Add(new MenuSlider("rStacks", "R Stacks Limit", 1, 1, 6));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useE", "Use E", false));
    LaneClearMenu->Add(new MenuBool("useR", "Use R", false));
    LaneClearMenu->Add(new MenuSlider("rStacks", "R Stacks Limit", 1, 1, 6));
    LaneClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("useW", "Use W"));
    JungleClearMenu->Add(new MenuBool("useE", "Use E"));
    JungleClearMenu->Add(new MenuBool("useR", "Use R"));
    JungleClearMenu->Add(new MenuSlider("rStacks", "R Stacks Limit", 1, 1, 6));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 950.0f);
    Q.SetSkillshot(0.25f, 70.0f, 1650.0f, true, SpellType::Line);
    Q.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, FLT_MAX);

    E = Spell(SpellSlot::E, 1260.0f);
    E.SetSkillshot(0.5f, 120.0f, 1400.0f, false, SpellType::Line);
    E.DamageType = DamageType::Magical;

    R = Spell(SpellSlot::R, 1000.0f);
    R.SetSkillshot(1.5f, 225.0f, FLT_MAX, false, SpellType::Circle);
    R.DamageType = DamageType::Magical;

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Kog'Maw loaded</font>");
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen() || player.Spellbook().IsWindingUp()) {
        return;
    }

    // Range động theo cấp W/R.
    W.Range = 565.0f + 60.0f + static_cast<float>(W.Level()) * 30.0f + 65.0f;
    R.Range = 900.0f + static_cast<float>(R.Level()) * 300.0f;

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        Mixed();
        break;
    case OrbwalkingMode::LaneClear:
        Clear();
        break;
    default:
        break;
    }
}

static void Combo() {
    if (!ShouldRunNow(LastComboEvalTick, 60)) {
        return;
    }

    if (Bool(ComboMenu, "useQ") && Q.IsReady() && KeepManaForW(ComboMenu, Q)) {
        const auto target = GetTargetNoCollision(Q);
        if (ValidHeroTarget(target, Q.Range)) {
            Q.Cast(target);
        }
    }

    if (Bool(ComboMenu, "useW") && W.IsReady()) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(enemy, W.Range)) {
                W.Cast();
                break;
            }
        }
    }

    if (Bool(ComboMenu, "useE") && E.IsReady() && KeepManaForW(ComboMenu, E)) {
        const auto target = GetTarget(E.Range, DamageType::Magical);
        if (ValidHeroTarget(target, E.Range)) {
            const auto pred = E.GetPrediction(target, true);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                E.Cast(pred.GetCastPosition());
            }
        }
    }

    if (Bool(ComboMenu, "useR") && R.IsReady() && KeepManaForW(ComboMenu, R)) {
        if (RCostStacks() < Slider(ComboMenu, "rStacks", 3)) {
            const auto target = GetTarget(R.Range, DamageType::Magical);
            if (ValidHeroTarget(target, R.Range)) {
                const auto pred = R.GetPrediction(target);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    R.Cast(pred.GetCastPosition());
                }
            }
        } else {
            // Vượt ngưỡng stack: chỉ bắn để kết liễu.
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (!ValidHeroTarget(enemy, R.Range)) {
                    continue;
                }
                if (IsKillable(enemy, R.GetDamage(enemy), DamageType::Magical)) {
                    const auto pred = R.GetPrediction(enemy);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                        R.Cast(pred.GetCastPosition());
                        break;
                    }
                }
            }
        }
    }
}

static void Mixed() {
    const auto player = Player();
    if (!player.IsValid() || !ManaOkay(Slider(HarassMenu, "Mana", 60))) {
        return;
    }

    if (Bool(HarassMenu, "useQ") && Q.IsReady()) {
        const auto target = GetTargetNoCollision(Q);
        if (ValidHeroTarget(target, Q.Range)) {
            Q.Cast(target);
        }
    }

    if (Bool(HarassMenu, "useE") && E.IsReady()) {
        const auto target = GetTarget(E.Range, DamageType::Magical);
        if (ValidHeroTarget(target, E.Range)) {
            const auto pred = E.GetPrediction(target, true);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                E.Cast(pred.GetCastPosition());
            }
        }
    }

    if (Bool(HarassMenu, "useR") && R.IsReady() && RCostStacks() < Slider(HarassMenu, "rStacks", 1)) {
        const auto target = GetTarget(R.Range, DamageType::Magical);
        if (ValidHeroTarget(target, R.Range)) {
            const auto pred = R.GetPrediction(target, true);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                R.Cast(pred.GetCastPosition());
            }
        }
    }
}

static void Clear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Lane clear E: bắn dàn lính trên đường (>=4).
    if (Bool(LaneClearMenu, "useE", false) && E.IsReady() && ManaOkay(Slider(LaneClearMenu, "Mana", 60))) {
        auto minions = GameObjects::EnemyLaneMinions();
        if (minions.empty()) {
            minions = GameObjects::EnemyMinions();
        }
        std::vector<AIBaseClient> targets;
        targets.reserve(minions.size());
        for (const auto& minion : minions) {
            if (ValidTarget(minion, E.Range)) {
                targets.push_back(AIBaseClient(minion.Handle()));
            }
        }
        if (!targets.empty()) {
            const auto farm = E.GetLineFarmLocation(targets);
            if (farm.MinionsHit >= 4) {
                E.Cast(Vector3::From2D(farm.Position));
            }
        }
    }

    // Lane clear R: bắn cụm lính (>=4) khi còn dưới ngưỡng stack.
    if (Bool(LaneClearMenu, "useR", false) && R.IsReady() &&
        ManaOkay(Slider(LaneClearMenu, "Mana", 60)) &&
        RCostStacks() < Slider(LaneClearMenu, "rStacks", 1)) {
        auto minions = GameObjects::EnemyLaneMinions();
        if (minions.empty()) {
            minions = GameObjects::EnemyMinions();
        }
        std::vector<AIBaseClient> targets;
        targets.reserve(minions.size());
        for (const auto& minion : minions) {
            if (ValidTarget(minion, R.Range)) {
                targets.push_back(AIBaseClient(minion.Handle()));
            }
        }
        if (!targets.empty()) {
            const auto farm = R.GetCircularFarmLocation(targets);
            if (farm.MinionsHit >= 4) {
                R.Cast(Vector3::From2D(farm.Position));
            }
        }
    }

    // Jungle clear E / R: mob máu cao nhất trong tầm.
    auto mobs = GameObjects::Jungle();
    mobs.erase(
        std::remove_if(
            mobs.begin(),
            mobs.end(),
            [](const AIMinionClient& mob) {
                return !ValidTarget(mob, 600.0f) || mob.IsPlant() || mob.IsPet();
            }),
        mobs.end());
    std::sort(
        mobs.begin(),
        mobs.end(),
        [](const AIMinionClient& a, const AIMinionClient& b) {
            return a.MaxHealth() > b.MaxHealth();
        });

    if (mobs.empty()) {
        return;
    }
    const auto& mob = mobs.front();

    if (Bool(JungleClearMenu, "useE") && E.IsReady() && ManaOkay(Slider(JungleClearMenu, "Mana", 20)) &&
        ValidTarget(mob, E.Range)) {
        E.Cast(mob.Position());
    }

    if (Bool(JungleClearMenu, "useR") && R.IsReady() && ManaOkay(Slider(JungleClearMenu, "Mana", 20)) &&
        RCostStacks() < Slider(JungleClearMenu, "rStacks", 1) && ValidTarget(mob, R.Range)) {
        R.Cast(mob.Position());
    }
}

// W bật trước khi đánh thường (combo hoặc jungle mob khi laneclear).
// Chặn đánh thường khi đang trong trạng thái W (kogmawicathiansurprise).
static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (!Loaded) {
        return;
    }

    const auto player = Player();
    if (player.IsValid() && player.HasBuff("kogmawicathiansurprise")) {
        args.Process = false;
    }

    if (!W.IsReady()) {
        return;
    }

    const auto targetBase = AIBaseClient(args.Target.Handle());
    if (!ValidUnit(targetBase)) {
        return;
    }

    const OrbwalkingMode mode = Orbwalker::ActiveMode();
    if (mode == OrbwalkingMode::Combo) {
        if (Bool(ComboMenu, "useW") && Extensions::IsValidTarget(targetBase, W.Range, true)) {
            W.Cast();
        }
    } else if (mode == OrbwalkingMode::LaneClear) {
        if (Bool(JungleClearMenu, "useW") && targetBase.IsMinion() &&
            targetBase.Team() == GameObjectTeam::Neutral) {
            W.Cast();
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::KogMaw
