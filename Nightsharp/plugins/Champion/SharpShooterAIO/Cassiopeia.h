#pragma once

// ============================================================================
// SharpShooter AIO — Cassiopeia
// Port từ CSharpFiles_2/Cassiopeia.cs (ToxicAio Reborn) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Syndra.h.
//
// Kỹ năng (giữ spell setup 1-1 với C#):
//   Q Noxious Blast  — circle 850, delay 0.25, width 30, Magical.
//   W Miasma         — circle 700, delay 0.25, width 25, Magical.
//   E Twin Fang      — targeted 700, delay 0.125, speed 2500, Magical.
//   R Petrifying Gaze— cone 825, delay 0.5, width 40, Magical (stun facing).
//
// Ghi chú port (1-1 với C#):
//   * LogicR/W/Q/E theo đúng thứ tự combo C#.
//   * Qpois toggle (G): chỉ Q khi target chưa dính poison.
//   * Epois toggle (T): chỉ E khi target đã dính poison.
//   * BeforeAA: tắt AA khi combo ở level >= 6 (menu AA).
//   * Interrupter (R), AntiGapcloser (E), OnDash (auto Q).
//   * Killsteal Q/E theo GetKsDamage → GetSpellDamage.
//   * Damage: dùng SDK GetSpellDamage (đã có DamageLibrary) cho draw + killsteal.
//   * MISSING API: HpBarIndicator (draw dmg trên thanh máu) → vẽ text combo dmg.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Cassiopeia {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* PredMenu = nullptr;
inline Menu* KillstealMenu = nullptr;
inline Menu* MiscMenu = nullptr;
inline Menu* ClearMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 850.0f };
inline Spell W{ SpellSlot::W, 700.0f };
inline Spell E{ SpellSlot::E, 700.0f };
inline Spell R{ SpellSlot::R, 825.0f };

inline bool Loaded = false;

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

static int ListIndex(Menu* menu, const char* key, int fallback = 0) {
    return menu ? menu->GetListIndex(key, fallback) : fallback;
}

static bool KeyActive(Menu* menu, const char* key) {
    if (!menu) {
        return false;
    }
    const auto* item = menu->Get<MenuKeyBind>(key);
    return item && item->Active;
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

static AIHeroClient TSGetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

// C#: switch(comb) 0..3 → Low/Medium/High/VeryHigh, default High.
static HitChance HitchanceFromList(Menu* menu, const char* key) {
    switch (ListIndex(menu, key, 2)) {
    case 0: return HitChance::Low;
    case 1: return HitChance::Medium;
    case 2: return HitChance::High;
    case 3: return HitChance::VeryHigh;
    default: return HitChance::High;
    }
}

// C#: GetComboDamage — Q+W+E+R (dùng cho draw).
static float GetComboDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    float damage = 0.0f;
    if (Q.IsReady()) {
        damage += player.GetSpellDamage(target, SpellSlot::Q);
    }
    if (W.IsReady()) {
        damage += player.GetSpellDamage(target, SpellSlot::W);
    }
    if (E.IsReady()) {
        damage += player.GetSpellDamage(target, SpellSlot::E);
    }
    if (R.IsReady()) {
        damage += player.GetSpellDamage(target, SpellSlot::R);
    }
    return damage;
}

// Forward declarations — đúng thứ tự file C#.
static void OnGameUpdate(const GameUpdateEventArgs& args);
static void LogicQ();
static void LogicW();
static void LogicE();
static void LogicR();
static void Jungle();
static void Laneclear();
static void LastHit();
static void Killsteal();
static void OnDraw();
static void GetComboDamageDraw();
static void OnInterrupterSpell(const Events::InterruptableSpell::InterruptableTargetEventArgs& args);
static void OnGapCloser(const GapCloserEventArgs& args);
static void OnDash(const Events::Dash::DashArgs& e);
static void BeforeAA(OrbwalkingActionArgs& args);
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Cassiopeia", true);

    QMenu = MenuRoot->AddSubMenu(new Menu("Qsettings", "Q settings"));
    QMenu->Add(new MenuBool("useQ", "Use Q in Combo", true));
    QMenu->Add(new MenuBool("autoQ", "Auto Q Dashing Target", false));
    QMenu->Add(new MenuKeyBind("Qpois", "Use Q Only When target is not Poisoned", 'G', KeyBindType::Toggle));

    WMenu = MenuRoot->AddSubMenu(new Menu("Wsettings", "W settings"));
    WMenu->Add(new MenuBool("useW", "Use W in Combo", true));
    WMenu->Add(new MenuSlider("WHP", "HP % To use W", 50, 1, 100));

    EMenu = MenuRoot->AddSubMenu(new Menu("Esettings", "E settings"));
    EMenu->Add(new MenuBool("useE", "Use E in Combo", true));
    EMenu->Add(new MenuKeyBind("Epois", "Use E only when target is Poisoned", 'T', KeyBindType::Toggle));

    RMenu = MenuRoot->AddSubMenu(new Menu("Rsettings", "R settings"));
    RMenu->Add(new MenuBool("useR", "Use R in Combo", true));
    RMenu->Add(new MenuSlider("RHP", "HP % To use R", 50, 1, 100));

    PredMenu = MenuRoot->AddSubMenu(new Menu("Pred", "Prediction settings"));
    PredMenu->Add(new MenuList("QPred", "Q Hitchance",
        std::vector<std::string>{ "Low", "Medium", "High", "Very High" }, 2));
    PredMenu->Add(new MenuList("WPred", "W Hitchance",
        std::vector<std::string>{ "Low", "Medium", "High", "Very High" }, 2));
    PredMenu->Add(new MenuList("RPred", "R Hitchance",
        std::vector<std::string>{ "Low", "Medium", "High", "Very High" }, 2));

    KillstealMenu = MenuRoot->AddSubMenu(new Menu("Killsteal", "Killsteal Settings"));
    KillstealMenu->Add(new MenuBool("KsQ", "use Q to Killsteal", true));
    KillstealMenu->Add(new MenuBool("KsE", "use E to Killsteal", true));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc", "Misc settings"));
    MiscMenu->Add(new MenuBool("Int", "Interrupter", true));
    MiscMenu->Add(new MenuBool("AG", "Antigapcloser", true));
    MiscMenu->Add(new MenuBool("AA", "Disable AutoAttacks at Level 6", true));

    ClearMenu = MenuRoot->AddSubMenu(new Menu("Clear", "Clear settings"));
    ClearMenu->Add(new MenuBool("LcQ", "use Q to Lane clear", true));
    ClearMenu->Add(new MenuBool("LcE", "use E to Last Hit", true));
    ClearMenu->Add(new MenuBool("JcQ", "use Q to Jungle clear", true));
    ClearMenu->Add(new MenuBool("JcE", "use E to Jungle clear", true));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Draw settings"));
    DrawMenu->Add(new MenuBool("drawQ", "Q Range  (White)", true));
    DrawMenu->Add(new MenuBool("drawW", "W Range  (Blue)", true));
    DrawMenu->Add(new MenuBool("drawE", "E Range (Green)", true));
    DrawMenu->Add(new MenuBool("drawR", "R Range  (Red)", true));
    DrawMenu->Add(new MenuBool("drawIn", "Draw Damage Indicator", true));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 850.0f);
    Q.SetSkillshot(0.25f, 30.0f, FLT_MAX, false, SpellType::Circle);
    Q.DamageType = DamageType::Magical;

    W = Spell(SpellSlot::W, 700.0f);
    W.SetSkillshot(0.25f, 25.0f, FLT_MAX, false, SpellType::Circle);
    W.DamageType = DamageType::Magical;

    E = Spell(SpellSlot::E, 700.0f);
    E.SetTargetted(0.125f, 2500.0f);
    E.DamageType = DamageType::Magical;

    R = Spell(SpellSlot::R, 825.0f);
    R.SetSkillshot(0.5f, 40.0f, FLT_MAX, false, SpellType::Cone);
    R.DamageType = DamageType::Magical;

    BuildMenu();

    Events::hook.OnGameUpdate += &OnGameUpdate;
    Drawing::OnDraw += &OnDraw;
    Events::hook.OnGapCloser += &OnGapCloser;
    Events::hook.OnInterruptableSpell += &OnInterrupterSpell;
    Events::hook.OnDash += &OnDash;
    Orbwalker::OnBeforeAttack += &BeforeAA;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Cassiopeia loaded</font>");
}

static void OnGameUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }

    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo) {
        LogicR();
        LogicW();
        LogicQ();
        LogicE();
    }

    if (Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear) {
        Laneclear();
        LastHit();
        Jungle();
    }
    Killsteal();
}

static void LogicQ() {
    const auto qtarget = TSGetTarget(Q.Range, DamageType::Magical);
    const bool useQ = Bool(QMenu, "useQ", true);
    const bool qhotkey = KeyActive(QMenu, "Qpois");
    if (!qtarget.IsValid()) {
        return;
    }

    const HitChance hitchance = HitchanceFromList(PredMenu, "QPred");

    if (ValidHeroTarget(qtarget, Q.Range)) {
        if (Q.IsReady() && useQ && qhotkey) {
            if (!qtarget.HasBuff("cassiopeiaqdebuff") && !qtarget.HasBuff("cassiopeiawpoison")) {
                const auto qpred = Q.GetPrediction(AIBaseClient(qtarget.Handle()));
                if (HitchanceAtLeast(qpred.Hitchance, hitchance)) {
                    Q.Cast(qpred.GetCastPosition());
                }
            }
        }
    }

    if (ValidHeroTarget(qtarget, Q.Range)) {
        if (Q.IsReady() && useQ && !qhotkey) {
            const auto qpred = Q.GetPrediction(AIBaseClient(qtarget.Handle()));
            if (HitchanceAtLeast(qpred.Hitchance, hitchance)) {
                Q.Cast(qpred.GetCastPosition());
            }
        }
    }
}

static void LogicW() {
    const auto player = Player();
    const auto wtarget = TSGetTarget(W.Range, DamageType::Magical);
    const bool useW = Bool(WMenu, "useW", true);
    const int whp = Slider(WMenu, "WHP", 50);
    if (!wtarget.IsValid()) {
        return;
    }

    const HitChance hitchance = HitchanceFromList(PredMenu, "WPred");

    // C#: wtarget.InRange(E.Range) — dùng E.Range như bản gốc.
    if (ValidHeroTarget(wtarget, E.Range)) {
        if (useW && W.IsReady() && ValidHeroTarget(wtarget, W.Range)) {
            if (wtarget.HealthPercent() <= static_cast<float>(whp)) {
                const auto wpred = W.GetPrediction(AIBaseClient(wtarget.Handle()));
                if (HitchanceAtLeast(wpred.Hitchance, hitchance)) {
                    W.Cast(wpred.GetCastPosition());
                }
            }
        }
    }
}

static void LogicE() {
    const auto etarget = TSGetTarget(E.Range, DamageType::Magical);
    const bool useE = Bool(EMenu, "useE", true);
    const bool estack = KeyActive(EMenu, "Epois");
    if (!etarget.IsValid()) {
        return;
    }

    if (ValidHeroTarget(etarget, E.Range)) {
        if (E.IsReady() && useE && estack && ValidHeroTarget(etarget, E.Range)) {
            if (etarget.HasBuff("cassiopeiaqdebuff") || etarget.HasBuff("cassiopeiawpoison")) {
                E.CastOnUnit(AIBaseClient(etarget.Handle()));
            }
        } else if (E.IsReady() && useE && !estack && ValidHeroTarget(etarget, E.Range)) {
            E.CastOnUnit(AIBaseClient(etarget.Handle()));
        }
    }
}

static void LogicR() {
    const auto player = Player();
    const auto rtarget = TSGetTarget(R.Range, DamageType::Magical);
    const bool useR = Bool(RMenu, "useR", true);
    const int rhp = Slider(RMenu, "RHP", 50);
    if (!rtarget.IsValid()) {
        return;
    }

    const HitChance hitchance = HitchanceFromList(PredMenu, "RPred");
    const auto rBase = AIBaseClient(rtarget.Handle());

    if (ValidHeroTarget(rtarget, R.Range)) {
        if (R.IsReady() && useR && ValidHeroTarget(rtarget, R.Range) &&
            Extensions::IsFacing(rBase, AIBaseClient(player.Handle())) &&
            rtarget.HealthPercent() <= static_cast<float>(rhp)) {
            const auto rpred = R.GetPrediction(rBase);
            if (HitchanceAtLeast(rpred.Hitchance, hitchance)) {
                R.Cast(rpred.GetCastPosition());
            }
        }
    }
}

static void Jungle() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const bool JcQq = Bool(ClearMenu, "JcQ", true);
    const bool JcEe = Bool(ClearMenu, "JcE", true);

    auto mobs = GameObjects::Jungle();
    mobs.erase(
        std::remove_if(mobs.begin(), mobs.end(),
            [](const AIMinionClient& mob) { return !ValidTarget(mob, Q.Range); }),
        mobs.end());
    std::sort(mobs.begin(), mobs.end(),
        [](const AIMinionClient& a, const AIMinionClient& b) { return a.MaxHealth() < b.MaxHealth(); });

    if (!mobs.empty()) {
        const auto& mob = mobs[0];
        if (JcQq && Q.IsReady() && player.Distance(mob.Position()) < Q.Range) {
            Q.Cast(mob.Position());
        }
        if (JcEe && E.IsReady() && player.Distance(mob.Position()) < E.Range) {
            E.CastOnUnit(AIBaseClient(mob.Handle()));
        }
    }
}

static void Laneclear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const bool lcq = Bool(ClearMenu, "LcQ", true);

    if (lcq && Q.IsReady()) {
        std::vector<AIBaseClient> minions;
        for (const auto& m : GameObjects::EnemyMinions()) {
            if (ValidTarget(m, Q.Range) && m.IsMinion()) {
                minions.push_back(AIBaseClient(m.Handle()));
            }
        }
        if (!minions.empty()) {
            const auto qFarm = Q.GetCircularFarmLocation(minions);
            if (qFarm.Position.IsValid()) {
                Q.Cast(Vector3::From2D(qFarm.Position));
                return;
            }
        }
    }
}

static void LastHit() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (Bool(ClearMenu, "LcE", true)) {
        std::vector<AIMinionClient> allMinions;
        for (const auto& m : GameObjects::EnemyMinions()) {
            if (m.IsMinion() && !m.IsDead()) {
                allMinions.push_back(m);
            }
        }
        std::sort(allMinions.begin(), allMinions.end(),
            [&player](const AIMinionClient& a, const AIMinionClient& b) {
                return a.Distance(player.Position()) < b.Distance(player.Position());
            });

        for (const auto& min : allMinions) {
            if (ValidTarget(min, E.Range) &&
                min.Health() < E.GetDamage(AIBaseClient(min.Handle()))) {
                Orbwalker::ForceTarget(AIBaseClient(min.Handle()));
                E.CastOnUnit(AIBaseClient(min.Handle()));
            }
        }
    }
}

static void Killsteal() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const bool ksQ = Bool(KillstealMenu, "KsQ", true);
    const bool ksE = Bool(KillstealMenu, "KsE", true);

    for (const auto& target : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(target, Q.Range) ||
            target.HasBuff("JudicatorIntervention") ||
            target.HasBuff("kindredrnodeathbuff") ||
            target.HasBuff("Undying Rage")) {
            continue;
        }
        const auto tBase = AIBaseClient(target.Handle());

        if (ksQ && Q.IsReady() && ValidHeroTarget(target, Q.Range)) {
            if (target.DistanceToPlayer() <= Q.Range) {
                if (target.Health() + target.AllShield() <=
                    player.GetSpellDamage(tBase, SpellSlot::Q)) {
                    const auto qpred = Q.GetPrediction(tBase);
                    if (HitchanceAtLeast(qpred.Hitchance, HitChance::High)) {
                        Q.Cast(qpred.GetCastPosition());
                    }
                }
            }
        }

        if (ksE && E.IsReady() && ValidHeroTarget(target, E.Range)) {
            if (target.DistanceToPlayer() <= E.Range) {
                if (target.Health() + target.AllShield() <=
                    player.GetSpellDamage(tBase, SpellSlot::E)) {
                    E.CastOnUnit(tBase);
                }
            }
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "drawQ", true) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFFFFFFFu);
    }
    if (Bool(DrawMenu, "drawW", true) && W.IsReady()) {
        Drawing::DrawCircle(player.Position(), W.Range, 0xFF0000FFu);
    }
    if (Bool(DrawMenu, "drawE", true) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), E.Range, 0xFF00FF00u);
    }
    if (Bool(DrawMenu, "drawR", true) && R.IsReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFFFF0000u);
    }
    GetComboDamageDraw();
}

// C#: DrawingOnEnd + HpBarIndicator.drawDmg — MISSING API (không có overlay thanh
// máu). Thay bằng text combo damage phía trên đầu địch.
static void GetComboDamageDraw() {
    if (!Bool(DrawMenu, "drawIn", true)) {
        return;
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy) || enemy.IsDead()) {
            continue;
        }
        const float dmg = GetComboDamage(AIBaseClient(enemy.Handle()));
        char buf[64];
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "Combo: %.0f", dmg);
        Vector2 screen{};
        if (Drawing::WorldToScreen(enemy.Position(), screen)) {
            Drawing::DrawText(screen.x + 50.0f, screen.y - 40.0f, 0xFFFFCC00u, buf);
        }
    }
}

// C#: Interrupter.OnInterrupterSpell — R vào target đang cast interruptible + facing.
static void OnInterrupterSpell(const Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    if (!Bool(MiscMenu, "Int", true)) {
        return;
    }
    const auto player = Player();
    const auto sender = AIHeroClient(args.Sender);
    if (!sender.IsValid()) {
        return;
    }
    if (player.Distance(sender.PreviousPosition()) < R.Range &&
        Extensions::IsFacing(AIBaseClient(sender.Handle()), AIBaseClient(player.Handle()))) {
        R.Cast(AIBaseClient(sender.Handle()));
    }
}

// C#: AntiGapcloser.OnGapcloser — E vào kẻ lao vào + facing (dùng R.Range như gốc).
static void OnGapCloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "AG", true)) {
        return;
    }
    const auto player = Player();
    const auto sender = AIHeroClient(args.Sender);
    if (!sender.IsValid() || !sender.IsEnemy()) {
        return;
    }
    if (player.Distance(sender.PreviousPosition()) < R.Range &&
        Extensions::IsFacing(AIBaseClient(sender.Handle()), AIBaseClient(player.Handle()))) {
        E.CastOnUnit(AIBaseClient(sender.Handle()));
    }
}

// C#: Dash.OnDash — auto Q lên target đang dash (autoQ).
static void OnDash(const Events::Dash::DashArgs& e) {
    const bool useea = Bool(QMenu, "autoQ", false);
    const auto sender = AIBaseClient(e.Unit);
    if (!Q.IsReady() || !sender.IsValid() || !sender.IsEnemy() || !useea) {
        return;
    }
    const Vector3 endPos = e.EndPos;
    if (endPos.IsValid() && E.IsInRange(endPos)) {
        const auto spred = E.GetPrediction(sender);
        if (HitchanceAtLeast(spred.Hitchance, HitChance::Dash)) {
            Q.Cast(spred.GetCastPosition());
        }
    }
}

// C#: BeforeAA — tắt AA khi combo ở level >= 6.
static void BeforeAA(OrbwalkingActionArgs& args) {
    if (!Loaded) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (Bool(MiscMenu, "AA", true)) {
        if (player.Level() >= 6) {
            if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo) {
                args.Process = false;
            }
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &OnGameUpdate;
    Drawing::OnDraw -= &OnDraw;
    Events::hook.OnGapCloser -= &OnGapCloser;
    Events::hook.OnInterruptableSpell -= &OnInterrupterSpell;
    Events::hook.OnDash -= &OnDash;
    Orbwalker::OnBeforeAttack -= &BeforeAA;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Cassiopeia
