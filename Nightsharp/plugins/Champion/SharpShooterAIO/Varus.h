#pragma once

// ============================================================================
// SharpShooter AIO — Varus
// Port từ SharpShooterCSHarp/Plugins/Varus.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h. Charged spell theo Spell::SetCharged.
//
// Kỹ năng:
//   Q Piercing Arrow — charged skillshot line. SetCharged(min 250, max 1600,
//                      1.2s). Giữ charge tới khi tầm >= "Q Min Charge" rồi bắn.
//   W Blighted Quiver— on-hit stack (không tự cast; dùng để ưu tiên target có
//                      >=3 stack "varuswdebuff" cho Q/E).
//   E Hail of Arrows — skillshot circle 925, delay 1.0, radius 250.
//   R Chain of Corruption — skillshot line 1200, delay 0.25, width 120 (snare).
//
// Ghi chú port:
//   * IsCharging()/StartCharging()/CurrentRange() từ Spell wrapper. Range charge
//     hiện tại đọc qua Q.CurrentRange() (tương đương _q.Range mid-charge của C#).
//   * Before-attack: chặn auto-attack khi đang charge Q (giữ nhịp charge).
//   * _eLastCastTime gate 1500ms để không spam StartCharging ngay sau khi cast E.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Varus {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 1600.0f };
inline Spell W{ SpellSlot::W, FLT_MAX };
inline Spell E{ SpellSlot::E, 925.0f };
inline Spell R{ SpellSlot::R, 1200.0f };

inline bool Loaded = false;
inline DWORD LastComboEvalTick = 0;
inline DWORD ELastCastTick = 0;

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

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

// Killable rút gọn (loại buff bất tử + so máu/shield vật lý).
static bool IsKillable(const AIBaseClient& target, double damage) {
    if (!ValidUnit(target)) {
        return false;
    }
    if (target.HasBuff("kindredrnodeathbuff") || target.HasBuff("Undying Rage") ||
        target.HasBuff("JudicatorIntervention") || target.HasBuff("BansheesVeil") ||
        target.HasBuff("SivirShield") || target.HasBuff("ShroudofDarkness")) {
        return false;
    }
    return target.Health() + target.PhysicalShield() < damage - 2.0;
}

// Tìm hero có >=3 stack W ("varuswdebuff") trong tầm cho.
static AIHeroClient GetWDebuffTarget(float range) {
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy, range) && enemy.GetBuffCount("varuswdebuff") >= 3) {
            return enemy;
        }
    }
    return AIHeroClient();
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnProcessSpell(const ProcessSpellEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args);
static void Combo();
static void Mixed();
static void Clear();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Varus", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q (charged)"));
    ComboMenu->Add(new MenuSlider("qMinCharge", "Q Min Charge Range", 800, 0, 1600));
    ComboMenu->Add(new MenuBool("useE", "Use E"));
    ComboMenu->Add(new MenuBool("useR", "Use R"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuSlider("qMinCharge", "Q Min Charge Range", 1600, 0, 1600));
    HarassMenu->Add(new MenuBool("useE", "Use E", false));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q", false));
    LaneClearMenu->Add(new MenuBool("useE", "Use E", false));
    LaneClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("useE", "Use E", false));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("gapcloser", "Anti-Gapcloser (R/E)"));
    MiscMenu->Add(new MenuBool("interrupter", "Interrupter (R)"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 1600.0f);
    Q.SetSkillshot(0.25f, 70.0f, 1500.0f, false, SpellType::Line);
    Q.SetCharged("VarusQ", "VarusQ", 250, 1600, 1.2f);
    Q.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, FLT_MAX);

    E = Spell(SpellSlot::E, 925.0f);
    E.SetSkillshot(1.0f, 250.0f, 1750.0f, false, SpellType::Circle);
    E.DamageType = DamageType::Physical;

    R = Spell(SpellSlot::R, 1200.0f);
    R.SetSkillshot(0.25f, 120.0f, 1200.0f, false, SpellType::Line);
    R.DamageType = DamageType::Magical;

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Events::hook.OnGapCloser += &Gapcloser_OnGapcloser;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Varus loaded</font>");
}

static void OnProcessSpell(const ProcessSpellEventArgs& args) {
    if (!Loaded || !Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    if (args.Slot == static_cast<int>(SpellSlot::E)) {
        ELastCastTick = GetTickCount();
    }
}

// Chặn auto-attack khi đang charge Q (giữ nhịp charge, giống args.Process = !IsCharging).
static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (!Loaded) {
        return;
    }
    if (Q.IsCharging()) {
        args.Process = false;
    }
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // ── Q charged ──
    if (Bool(ComboMenu, "useQ") && Q.IsReady()) {
        const int minCharge = Slider(ComboMenu, "qMinCharge", 800);

        // Ưu tiên finish: có hero killable trong tầm Q max.
        AIHeroClient killable;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(enemy, static_cast<float>(Q.ChargedMaxRange)) &&
                IsKillable(enemy, Q.GetDamage(enemy))) {
                const auto pred = Q.GetPrediction(enemy);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    killable = enemy;
                    break;
                }
            }
        }

        if (killable.IsValid()) {
            if (Q.IsCharging()) {
                if (ValidHeroTarget(killable, Q.CurrentRange())) {
                    Q.Cast(killable);
                }
            } else {
                Q.StartCharging();
            }
        } else if (W.Level() > 0) {
            // Có W: ưu tiên hero >=3 stack; nếu không, giữ charge chờ.
            const auto wTarget = GetWDebuffTarget(static_cast<float>(Q.ChargedMaxRange));
            if (wTarget.IsValid()) {
                if (Q.IsCharging()) {
                    if (Q.CurrentRange() >= static_cast<float>(minCharge) &&
                        ValidHeroTarget(wTarget, Q.CurrentRange())) {
                        Q.Cast(wTarget);
                    }
                } else if ((Bool(ComboMenu, "useE") ? !E.IsReady() : true) &&
                           ELastCastTick + 1500 < GetTickCount()) {
                    Q.StartCharging();
                }
            } else if (Q.IsCharging() && Q.CurrentRange() >= static_cast<float>(minCharge)) {
                const auto target = GetTarget(Q.CurrentRange(), DamageType::Physical);
                if (ValidHeroTarget(target, Q.CurrentRange())) {
                    Q.Cast(target);
                }
            }
        } else {
            if (Q.IsCharging()) {
                if (Q.CurrentRange() >= static_cast<float>(minCharge)) {
                    const auto target = GetTarget(Q.CurrentRange(), DamageType::Physical);
                    if (ValidHeroTarget(target, Q.CurrentRange())) {
                        Q.Cast(target);
                    }
                }
            } else if (ValidHeroTarget(GetTarget(static_cast<float>(Q.ChargedMaxRange), DamageType::Physical))) {
                Q.StartCharging();
            }
        }
    }

    // ── E ──
    if (Bool(ComboMenu, "useE") && E.IsReady()) {
        AIHeroClient killable;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(enemy, E.Range) && IsKillable(enemy, E.GetDamage(enemy))) {
                killable = enemy;
                break;
            }
        }
        if (killable.IsValid()) {
            E.Cast(killable);
        } else if (W.Level() > 0) {
            const auto wTarget = GetWDebuffTarget(E.Range);
            if (wTarget.IsValid()) {
                E.Cast(wTarget);
            } else {
                const auto target = GetTarget(E.Range, DamageType::Physical);
                if (ValidHeroTarget(target, E.Range)) {
                    E.CastIfWillHit(AIBaseClient(target.Handle()), 3);
                }
            }
        } else {
            const auto target = GetTarget(E.Range, DamageType::Physical);
            if (ValidHeroTarget(target, E.Range)) {
                E.Cast(target);
            }
        }
    }

    // ── R ──
    if (Bool(ComboMenu, "useR") && R.IsReady()) {
        const auto target = GetTarget(R.Range - 500.0f, DamageType::Magical);
        if (ValidHeroTarget(target, R.Range)) {
            R.Cast(target);
        }
    }
}

static void Mixed() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Bool(HarassMenu, "useQ") && Q.IsReady()) {
        if (Q.IsCharging()) {
            if (Q.CurrentRange() >= static_cast<float>(Q.ChargedMaxRange)) {
                const auto target = GetTarget(Q.CurrentRange(), DamageType::Physical);
                if (ValidHeroTarget(target, Q.CurrentRange())) {
                    Q.Cast(target);
                }
            } else {
                for (const auto& enemy : GameObjects::EnemyHeroes()) {
                    if (ValidHeroTarget(enemy, Q.CurrentRange()) && IsKillable(enemy, Q.GetDamage(enemy))) {
                        Q.Cast(enemy);
                        break;
                    }
                }
            }
        } else if (ManaOkay(Slider(HarassMenu, "Mana", 60)) &&
                   ValidHeroTarget(GetTarget(static_cast<float>(Q.ChargedMaxRange), DamageType::Physical))) {
            Q.StartCharging();
        }
    }

    if (Bool(HarassMenu, "useE", false) && E.IsReady() && ManaOkay(Slider(HarassMenu, "Mana", 60))) {
        const auto target = GetTarget(E.Range, DamageType::Physical);
        if (ValidHeroTarget(target, E.Range)) {
            E.Cast(target);
        }
    }
}

static void Clear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Lane clear Q: charge tới max rồi bắn hàng lính.
    if (Bool(LaneClearMenu, "useQ", false) && Q.IsReady()) {
        auto minions = GameObjects::EnemyLaneMinions();
        if (minions.empty()) {
            minions = GameObjects::EnemyMinions();
        }
        std::vector<AIBaseClient> targets;
        targets.reserve(minions.size());
        for (const auto& minion : minions) {
            if (ValidTarget(minion, static_cast<float>(Q.ChargedMaxRange))) {
                targets.push_back(AIBaseClient(minion.Handle()));
            }
        }
        const auto farm = Q.GetLineFarmLocation(targets);
        if (Q.IsCharging()) {
            if (Q.CurrentRange() >= static_cast<float>(Q.ChargedMaxRange) && farm.MinionsHit >= 1) {
                Q.Cast(Vector3::From2D(farm.Position));
            }
        } else if (farm.MinionsHit >= 4 && ManaOkay(Slider(LaneClearMenu, "Mana", 60))) {
            Q.StartCharging();
        }
    }

    // Lane clear E: bắn cụm lính (>=4).
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
            const auto farm = E.GetCircularFarmLocation(targets);
            if (farm.MinionsHit >= 4) {
                E.Cast(Vector3::From2D(farm.Position));
            }
        }
    }

    // Jungle clear Q: mob máu cao nhất trong tầm.
    if (Bool(JungleClearMenu, "useQ") && Q.IsReady()) {
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
        if (!mobs.empty()) {
            const auto& mob = mobs.front();
            if (Q.IsCharging()) {
                if (Q.CurrentRange() >= static_cast<float>(Q.ChargedMaxRange) && ValidTarget(mob, Q.CurrentRange())) {
                    Q.Cast(mob);
                }
            } else if (ManaOkay(Slider(JungleClearMenu, "Mana", 20))) {
                Q.StartCharging();
            }
        }
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }

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

static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "gapcloser")) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid() || args.End.Distance2D(player.Position()) > 200.0f) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (R.IsReady() && ValidHeroTarget(sender, R.Range)) {
        R.Cast(sender);
    } else if (E.IsReady() && ValidHeroTarget(sender, E.Range)) {
        E.Cast(sender);
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Events::hook.OnGapCloser -= &Gapcloser_OnGapcloser;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Varus
