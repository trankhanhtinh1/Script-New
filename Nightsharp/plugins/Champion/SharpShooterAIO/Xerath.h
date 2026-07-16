#pragma once

// ============================================================================
// SharpShooter AIO — Xerath
// Port từ CSharpFiles/Xerath/Xerath.cs (ImpulseAIO) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + charged spell theo XerathSemiPlugin.h
// và SharpShooterAIO/Varus.h (charged Q).
//
// Kỹ năng:
//   Q Arcanopulse   — charged skillshot line (750→1500), delay 0.5, width 70.
//   W Eye of Destruction — circle skillshot 970, delay 0.75, radius 125.
//   E Shocking Orb  — line skillshot 1050, delay 0.25, width 60, speed 1400, collision.
//   R Rite of the Arcane — 3-shot ultimate 5000, delay 0.6, radius 100.
//
// Ghi chú port (giữ 1-1 với C#):
//   * Charged Q: dùng Spell wrapper IsCharging()/StartCharging()/ShootChargedSpell()/
//     CurrentRange()/ChargedMaxRange — khớp _q.IsCharging/_q.Range của C#.
//   * RLogic: R multi-shot state machine (xerathrshots buff), track ChargesRemaining
//     + LastChargePosition/Time qua OnProcessSpell (XerathLocusOfPower2/XerathLocusPulse).
//   * MISSING API: AIHeroClient.GetStunDuration() → không có; bỏ sub-check anti-waste
//     E (vẫn cast E khi hitchance đủ). Xem missapi.md.
//   * R.GetDamage() dùng SDK damage library (skill cho phép giữ GetDamage) — không
//     tự viết bảng damage → không cần đối chiếu wiki.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Xerath {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 750.0f };
inline Spell W{ SpellSlot::W, 970.0f };
inline Spell E{ SpellSlot::E, 1050.0f };
inline Spell R{ SpellSlot::R, 5000.0f };

inline bool Loaded = false;

// R multi-shot state (port từ static field C#).
inline Vector3 LastChargePosition{};
inline int LastChargeTime = 0;
inline int ChargesRemaining = 0;
inline AIHeroClient LastUltTarget;
inline bool TargetWillDie = false;

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

static bool ValidUnit(const AttackableUnit& unit) {
    return unit.IsValid() && !unit.IsDead() && unit.Health() > 0.0f;
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

// C#: MaxCharges — 3 khi R chưa học, 2 + R.Level khi đã học.
static int MaxCharges() {
    if (!R.Instance().Learned()) {
        return 3;
    }
    return 2 + R.Instance().Level();
}

// C#: IsCastingUlt — có buff "xerathrshots".
static bool IsCastingUlt() {
    const auto player = Player();
    return player.IsValid() && player.HasBuff("xerathrshots");
}

// Forward declarations — đúng thứ tự file C#.
static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args);
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void RLogic();
static void Harass();
static void Combo();
static void LaneClear();
static void AntiGapCloser(const GapCloserEventArgs& args);
static void OnDraw();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Xerath", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("CQ", "Use Q"));
    ComboMenu->Add(new MenuBool("CW", "Use W"));
    ComboMenu->Add(new MenuBool("CE", "Use E"));
    ComboMenu->Add(new MenuSlider("CQExtra", "Q Extra Range", 30, 0, 200));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("HQ", "Use Q"));
    HarassMenu->Add(new MenuBool("HW", "Use W"));
    HarassMenu->Add(new MenuBool("HE", "Use E"));
    HarassMenu->Add(new MenuSlider("HQExtra", "Q Extra Range", 200, 0, 200));
    HarassMenu->Add(new MenuSlider("HMana", "Don't harass if mana <= X%", 30, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "LaneClear"));
    LaneClearMenu->Add(new MenuBool("LQ", "Use Q"));
    LaneClearMenu->Add(new MenuBool("LW", "Use W"));
    LaneClearMenu->Add(new MenuSlider("HitQ", "Q Min Hit minion", 3, 1, 10));
    LaneClearMenu->Add(new MenuSlider("HitW", "W Min Hit minion", 3, 1, 10));
    LaneClearMenu->Add(new MenuSlider("LMana", "Don't laneclear if mana <= X%", 30, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("intE", "Use E Interrupt"));
    MiscMenu->Add(new MenuBool("GapE", "Anti GapE"));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw Settings", "Draw"));
    DrawMenu->Add(new MenuBool("DQ", "Draw Q"));
    DrawMenu->Add(new MenuBool("DW", "Draw W"));
    DrawMenu->Add(new MenuBool("DE", "Draw E"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 750.0f);
    Q.SetCharged("XerathArcanopulseChargeUp", "XerathArcanopulseChargeUp", 750, 1500, 1.5f);
    Q.SetSkillshot(0.5f, 70.0f, FLT_MAX, false, SpellType::Line);
    Q.DamageType = DamageType::Magical;

    W = Spell(SpellSlot::W, 970.0f);
    W.SetSkillshot(0.75f, 125.0f, FLT_MAX, false, SpellType::Circle);
    W.DamageType = DamageType::Magical;

    E = Spell(SpellSlot::E, 1050.0f);
    E.SetSkillshot(0.25f, 60.0f, 1400.0f, true, SpellType::Line);
    E.SetCollisionObjects(
        SDK::CollisionableObjects::Minions |
        SDK::CollisionableObjects::YasuoWall);
    E.DamageType = DamageType::Magical;

    R = Spell(SpellSlot::R, 5000.0f);
    R.SetSkillshot(0.6f, 100.0f, FLT_MAX, false, SpellType::Circle);
    R.DamageType = DamageType::Magical;

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpellCast;
    Events::hook.OnGapCloser += &AntiGapCloser;
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Xerath loaded</font>");
}

static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args) {
    if (!Loaded || !Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    const std::string name = args.SpellName;
    // Ult activation.
    if (name == "XerathLocusOfPower2") {
        LastChargePosition = Vector3();
        LastChargeTime = 0;
        ChargesRemaining = MaxCharges();
    }
    // Ult charge usage.
    else if (name == "XerathLocusPulse") {
        LastChargePosition = Vector3::From2D(args.EndPosition.To2D());
        LastChargeTime = static_cast<int>(SDK::Variables::TickCount());
        --ChargesRemaining;
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }

    RLogic();

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        Harass();
        break;
    case OrbwalkingMode::LaneClear:
        LaneClear();
        break;
    default:
        break;
    }
}

static void RLogic() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    Orbwalker::AttackEnabled(!Q.IsCharging() && !IsCastingUlt());
    Orbwalker::MoveEnabled(!IsCastingUlt());

    if (!IsCastingUlt()) {
        return;
    }

    // First-time target (chưa có target hoặc còn đủ charge).
    if (!LastUltTarget.IsValid() || ChargesRemaining >= 3) {
        const auto target = GetTarget(R.Range, DamageType::Magical);
        if (ValidHeroTarget(target)) {
            const auto pred = R.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                R.Cast(pred.GetCastPosition());
                LastUltTarget = target;
                TargetWillDie = target.Health() < R.GetDamage(AIBaseClient(target.Handle()));
            }
        }
    }
    // Tiếp shot.
    else if (ChargesRemaining < 3) {
        if ((!TargetWillDie || static_cast<int>(SDK::Variables::TickCount()) - LastChargeTime > 600) &&
            ValidHeroTarget(LastUltTarget, R.Range)) {
            const auto pred = R.GetPrediction(LastUltTarget);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                R.Cast(pred.GetCastPosition());
                TargetWillDie = LastUltTarget.Health() < R.GetDamage(AIBaseClient(LastUltTarget.Handle()));
            }
        } else {
            const auto target = GetTarget(R.Range, DamageType::Magical);
            if (ValidHeroTarget(target)) {
                const float waitTime = std::max(1500.0f, target.Distance(LastChargePosition)) + 500.0f;
                if (static_cast<float>(static_cast<int>(SDK::Variables::TickCount()) - LastChargeTime) + waitTime < 0.0f) {
                    return;
                }
                const auto pred = R.GetPrediction(target);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    R.Cast(pred.GetCastPosition());
                    LastUltTarget = target;
                    TargetWillDie = target.Health() < R.GetDamage(AIBaseClient(target.Handle()));
                }
            }
        }
    }
}

static void Harass() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Q đang charge — bỏ qua mana check, nhả khi đủ tầm.
    if (Q.IsReady() && Q.IsCharging()) {
        const auto target = GetTarget(static_cast<float>(Q.ChargedMaxRange), DamageType::Magical);
        if (ValidHeroTarget(target)) {
            const auto pred = Q.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                Q.ShootChargedSpell(pred.GetCastPosition());
                return;
            }
        }
    }

    if (Q.IsCharging()) {
        return;
    }

    if (player.ManaPercent() < static_cast<float>(Slider(HarassMenu, "HMana", 30))) {
        return;
    }

    if (W.IsReady() && Bool(HarassMenu, "HW")) {
        const auto wt = GetTarget(W.Range, DamageType::Magical);
        if (ValidHeroTarget(wt)) {
            const auto pred = W.GetPrediction(wt);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                W.Cast(pred.GetCastPosition());
                return;
            }
        }
    }

    if (E.IsReady() && Bool(HarassMenu, "HE")) {
        const auto et = GetTarget(E.Range, DamageType::Magical);
        if (ValidHeroTarget(et)) {
            const auto pred = E.GetPrediction(et);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High) && pred.CollisionObjects.empty()) {
                E.Cast(pred.GetCastPosition());
                return;
            }
        }
    }

    // Q chargeup.
    if (Q.IsReady() && Bool(HarassMenu, "HQ") && !Q.IsCharging()) {
        const auto target = GetTarget(static_cast<float>(Q.ChargedMaxRange), DamageType::Magical);
        if (ValidHeroTarget(target)) {
            const auto pred = Q.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                Q.StartCharging();
            }
        }
    }
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (!Q.IsCharging()) {
        if (Bool(ComboMenu, "CW") && W.IsReady()) {
            const auto wt = GetTarget(W.Range, DamageType::Magical);
            if (ValidHeroTarget(wt)) {
                const auto pred = W.GetPrediction(wt);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    W.Cast(pred.GetCastPosition());
                    return;
                }
            }
        }

        if (Bool(ComboMenu, "CE") && E.IsReady()) {
            const auto et = GetTarget(E.Range, DamageType::Magical);
            if (ValidHeroTarget(et)) {
                const auto pred = E.GetPrediction(et);
                // MISSING API: GetStunDuration() — bỏ sub-check anti-waste (xem missapi.md).
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High) && pred.CollisionObjects.empty()) {
                    E.Cast(pred.GetCastPosition());
                    return;
                }
            }
        }
    }

    if (Bool(ComboMenu, "CQ") && Q.IsReady()) {
        const auto target = GetTarget(static_cast<float>(Q.ChargedMaxRange), DamageType::Magical);
        if (ValidHeroTarget(target)) {
            const auto pred = Q.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                if (!Q.IsCharging()) {
                    Q.StartCharging();
                    return;
                }
                Q.ShootChargedSpell(pred.GetCastPosition());
                return;
            }
        }
    }
}

static void LaneClear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    std::vector<AIBaseClient> minions;
    for (const auto& m : GameObjects::EnemyMinions()) {
        if (ValidUnit(m) && m.DistanceToPlayer() <= static_cast<float>(Q.ChargedMaxRange)) {
            minions.push_back(AIBaseClient(m.Handle()));
        }
    }
    if (minions.empty()) {
        return;
    }
    const int minHitQ = Slider(LaneClearMenu, "HitQ", 3);
    const int minHitW = Slider(LaneClearMenu, "HitW", 3);

    // Q đang charge — bỏ qua mana check.
    if (Q.IsReady() && Bool(LaneClearMenu, "LQ") && Q.IsCharging()) {
        const auto farm = Q.GetLineFarmLocation(minions);
        if (farm.MinionsHit >= minHitQ) {
            if (Q.ShootChargedSpell(Vector3::From2D(farm.Position))) {
                return;
            }
        }
    }

    if (Q.IsCharging()) {
        return;
    }

    if (static_cast<float>(Slider(LaneClearMenu, "LMana", 30)) > player.ManaPercent()) {
        return;
    }

    if (Q.IsReady() && Bool(LaneClearMenu, "LQ")) {
        if (static_cast<int>(minions.size()) >= minHitQ) {
            const auto farm = Q.GetLineFarmLocation(minions);
            if (farm.MinionsHit >= minHitQ) {
                Q.StartCharging();
                return;
            }
        }
    }

    if (W.IsReady() && Bool(LaneClearMenu, "LW")) {
        if (static_cast<int>(minions.size()) >= minHitW) {
            const auto farm = W.GetCircularFarmLocation(minions);
            if (farm.MinionsHit >= minHitW) {
                if (W.Cast(Vector3::From2D(farm.Position))) {
                    return;
                }
            }
        }
    }
}

static void AntiGapCloser(const GapCloserEventArgs& args) {
    const auto sender = AIHeroClient(args.Sender);
    if (!sender.IsValid() || !sender.IsEnemy()) {
        return;
    }
    // Anti-gap E.
    if (Bool(MiscMenu, "GapE") && E.IsReady() && sender.IsDashing() && ValidHeroTarget(sender, E.Range)) {
        const auto pred = E.GetPrediction(sender);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::High) && pred.CollisionObjects.empty()) {
            E.Cast(pred.GetCastPosition());
            return;
        }
    }
    // Interrupt E.
    if (Bool(MiscMenu, "intE") && E.IsReady() && ValidHeroTarget(sender, E.Range) &&
        Extensions::IsCastingInterruptableSpell(sender, false)) {
        const auto pred = E.GetPrediction(sender);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::High) && pred.CollisionObjects.empty()) {
            E.Cast(pred.GetCastPosition());
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "DQ", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), static_cast<float>(Q.ChargedMaxRange), 0xFFFFA500u);
    }
    if (Bool(DrawMenu, "DW", false) && W.IsReady()) {
        Drawing::DrawCircle(player.Position(), W.Range, 0xFFFFFFFFu);
    }
    if (Bool(DrawMenu, "DE", false) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), E.Range, 0xFFFF0000u);
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpellCast;
    Events::hook.OnGapCloser -= &AntiGapCloser;
    Drawing::OnDraw -= &OnDraw;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Xerath
