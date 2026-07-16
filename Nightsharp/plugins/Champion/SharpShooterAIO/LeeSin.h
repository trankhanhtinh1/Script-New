#pragma once

// ============================================================================
// SharpShooter AIO — LeeSin
// Port từ CSharpFiles_2/LeeSin.cs (ToxicAio Reborn) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Syndra.h.
//
// Kỹ năng:
//   Q Sonic Wave / Resonating Strike — Q1 skillshot line 1200 (delay 0.25,
//        width 60, speed 1800, collision). Q2 dash-to-target range 1250.
//        Phân biệt Q1/Q2 qua Q.Instance().Name(): "BlindMonkQOne"/"BlindMonkQTwo".
//   W Safeguard / Iron Will — dash-to-ally 700. Combo: W lên chính mình (Me).
//   E Tempest / Cripple — AoE quanh mình, range 450.
//   R Dragon's Rage — targeted 375, knockback.
//   Insec: keybind → IssueMove về sau target + R + Flash (kick về đội mình).
//
// Ghi chú port (giữ 1-1 với C#):
//   * OnTickUpdate (Game.OnUpdate): Insec keybind + Killsteal.
//   * OnGameUpdate (GameTick): Combo = QRQ2 → Q → E → W → R; LaneClear = Jungle+Lane.
//   * LogicQ: Q1 skillshot theo hitchance; Q2 dash nếu trong tầm (chặn khi target
//     dưới trụ nếu không bật useQTurret).
//   * LogicQRQ2: Q1→R→Q2 execute combo (Q1 hit → R → delay Q2 lao vào).
//   * Insec: flash-R kick. MISSING API DelayAction → cast tuần tự (xem missapi.md).
//   * Damage: E/R tính tay theo wiki (patch 2026-07-09). Q dùng GetSpellDamage
//     (SDK xử lý execute-scaling theo missing HP của Q2).
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::LeeSin {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* PredMenu = nullptr;
inline Menu* KillStealMenu = nullptr;
inline Menu* MiscMenu = nullptr;
inline Menu* ClearMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 1200.0f };
inline Spell Q2{ SpellSlot::Q, 1250.0f };
inline Spell W{ SpellSlot::W, 700.0f };
inline Spell E{ SpellSlot::E, 450.0f };
inline Spell R{ SpellSlot::R, 375.0f };

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

static bool KeyActive(Menu* menu, const char* key) {
    if (!menu) {
        return false;
    }
    const auto* item = menu->Get<MenuKeyBind>(key);
    return item && item->Active;
}

static int ListIndex(Menu* menu, const char* key, int fallback = 0) {
    return menu ? menu->GetListIndex(key, fallback) : fallback;
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

static float RealAutoAttackRange(const AIBaseClient& unit) {
    return AutoAttack::GetRealAutoAttackRange(unit);
}

// C#: comb(menuP,"QPred") → HitChance theo index 0..3.
static HitChance QHitchance() {
    switch (ListIndex(PredMenu, "QPred", 2)) {
    case 0: return HitChance::Low;
    case 1: return HitChance::Medium;
    case 2: return HitChance::High;
    case 3: return HitChance::VeryHigh;
    default: return HitChance::High;
    }
}

// C#: Q.Name == "BlindMonkQOne" / "BlindMonkQTwo".
static bool IsQOne() {
    return Q.Instance().Name() == "BlindMonkQOne";
}

static bool IsQTwo() {
    return Q.Instance().Name() == "BlindMonkQTwo";
}

// C#: Flashslot = Me.GetSpellSlot("SummonerFlash"); Flashslot.IsReady().
static SpellSlot FlashSlot() {
    const auto player = Player();
    return player.IsValid() ? player.GetSpellSlot("SummonerFlash") : SpellSlot::Unknown;
}

static bool FlashReady() {
    const auto player = Player();
    const SpellSlot slot = FlashSlot();
    if (!player.IsValid() || slot == SpellSlot::Unknown) {
        return false;
    }
    return player.Spellbook().CanUseSpell(slot) == CoreSpellBook::State_Ready;
}

// ── Damage tính tay theo wiki (leagueoflegends.com/Lee_Sin, patch 2026-07-09) ──
// E Tempest (PHYSICAL): 35/60/85/110/135 + 90% AD.
// R Dragon's Rage (PHYSICAL): 175/400/625 + 200% bonus AD (bỏ collision bonus).
// Q dùng SDK GetSpellDamage (execute-scaling theo missing HP không tính tay được).
static float EDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const int rank = E.Instance().Level();
    if (rank < 1) {
        return 0.0f;
    }
    static const float base[5] = { 35.0f, 60.0f, 85.0f, 110.0f, 135.0f };
    const float raw = base[rank - 1] + 0.90f * player.AD();
    return player.CalculatePhysicalDamage(target, raw);
}

static float RDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const int rank = R.Instance().Level();
    if (rank < 1) {
        return 0.0f;
    }
    const int idx = (rank - 1 < 3) ? rank - 1 : 2;
    static const float base[3] = { 175.0f, 400.0f, 625.0f };
    const float raw = base[idx] + 2.00f * player.BonusAttackDamage();
    return player.CalculatePhysicalDamage(target, raw);
}

static float GetComboDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    float damage = 0.0f;
    if (Q.IsReady() && IsQOne()) {
        damage += player.GetSpellDamage(target, SpellSlot::Q);
    }
    if (Q.IsReady() && IsQTwo()) {
        damage += player.GetSpellDamage(target, SpellSlot::Q);
    }
    if (W.IsReady()) {
        damage += player.GetSpellDamage(target, SpellSlot::W);
    }
    if (R.IsReady()) {
        damage += RDamage(target);
    }
    return damage;
}

// Forward declarations — đúng thứ tự file C#.
static void OnTickUpdate(const GameUpdateEventArgs& args);
static void OnGameUpdate(const GameUpdateEventArgs& args);
static void LogicQ();
static void LogicW();
static void LogicE();
static void LogicR();
static void LogicQRQ2();
static void Jungle();
static void Laneclear();
static void Killsteal();
static void OnDraw();
static void OnGapCloser(const GapCloserEventArgs& args);
static void Interrupter_OnInterrupterSpell(const Events::InterruptableSpell::InterruptableTargetEventArgs& args);
static void Insec();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - LeeSin", true);

    QMenu = MenuRoot->AddSubMenu(new Menu("Qsettings", "Q settings"));
    QMenu->Add(new MenuBool("useQ", "Use Q in Combo"));
    QMenu->Add(new MenuBool("useQ2", "Use Q2 in Combo"));
    QMenu->Add(new MenuKeyBind("useQTurret", "Use Q2 under Turret", 'T', KeyBindType::Toggle));

    WMenu = MenuRoot->AddSubMenu(new Menu("Wsettings", "W settings"));
    WMenu->Add(new MenuBool("useW", "Use W in Combo"));

    EMenu = MenuRoot->AddSubMenu(new Menu("Esettings", "E settings"));
    EMenu->Add(new MenuBool("useE", "Use E in Combo"));

    RMenu = MenuRoot->AddSubMenu(new Menu("Rsettings", "R settings"));
    RMenu->Add(new MenuBool("useR", "Use R in Combo"));
    RMenu->Add(new MenuBool("try", "Try to use Q-R-Q2"));
    RMenu->Add(new MenuKeyBind("Ins", "Insec", 'G', KeyBindType::Press));
    RMenu->Add(new MenuBool("Gapp", "Gapclose with Q if target is not in Insec Range"));

    PredMenu = MenuRoot->AddSubMenu(new Menu("Pred", "Prediction settings"));
    PredMenu->Add(new MenuList("QPred", "Q Hitchance",
        std::vector<std::string>{ "Low", "Medium", "High", "Very High" }, 2));

    KillStealMenu = MenuRoot->AddSubMenu(new Menu("Killsteal", "Killsteal Settings"));
    KillStealMenu->Add(new MenuBool("KsQ", "use Q to Killsteal"));
    KillStealMenu->Add(new MenuBool("KsQ2", "use Q2 to Killsteal"));
    KillStealMenu->Add(new MenuBool("KsE", "use E to Killsteal"));
    KillStealMenu->Add(new MenuBool("KsR", "use R to Killsteal", false));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc", "Misc Settings"));
    MiscMenu->Add(new MenuBool("AG", "AntiGapcloser"));
    MiscMenu->Add(new MenuBool("Int", "Interrupter"));

    ClearMenu = MenuRoot->AddSubMenu(new Menu("Clear", "Clear settings"));
    ClearMenu->Add(new MenuBool("LcE", "use E to Lane clear"));
    ClearMenu->Add(new MenuBool("JcQ", "use Q to Jungle clear"));
    ClearMenu->Add(new MenuBool("JcW", "use W to Jungle clear"));
    ClearMenu->Add(new MenuBool("JcE", "use E to Jungle clear"));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Draw settings"));
    DrawMenu->Add(new MenuBool("drawQ", "Q Range  (White)"));
    DrawMenu->Add(new MenuBool("drawW", "W Range  (Blue)"));
    DrawMenu->Add(new MenuBool("drawE", "E Range (Green)"));
    DrawMenu->Add(new MenuBool("drawR", "R Range  (Red)"));
    DrawMenu->Add(new MenuBool("drawIn", "Draw Damage Indicator"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 1200.0f);
    Q.SetSkillshot(0.25f, 60.0f, 1800.0f, true, SpellType::Line);
    Q.DamageType = DamageType::Physical;

    Q2 = Spell(SpellSlot::Q, 1250.0f);
    Q2.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, 700.0f);
    W.DamageType = DamageType::Magical;

    E = Spell(SpellSlot::E, 450.0f);
    E.DamageType = DamageType::Physical;

    R = Spell(SpellSlot::R, 375.0f);
    R.DamageType = DamageType::Physical;

    BuildMenu();

    Events::hook.OnGameUpdate += &OnGameUpdate;
    Events::hook.OnUpdate += &OnTickUpdate;
    Drawing::OnDraw += &OnDraw;
    Events::hook.OnInterruptableTarget += &Interrupter_OnInterrupterSpell;
    Events::hook.OnGapCloser += &OnGapCloser;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - LeeSin loaded</font>");
}

// C#: Game.OnUpdate += OnTickUpdate.
static void OnTickUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }

    if (KeyActive(RMenu, "Ins")) {
        CoreControl::IssueMove(Game::CursorPos(), true);
        const auto target = TSGetTarget(Q.Range, DamageType::Physical);
        const bool gapclose = Bool(RMenu, "Gapp");
        if (!target.IsValid()) {
            return;
        }

        if (!Extensions::IsValidTarget(target, RealAutoAttackRange(AIBaseClient(player.Handle())) + 70.0f, true)) {
            if (gapclose) {
                LogicQ();
            }
        }

        Insec();

        if (!FlashReady()) {
            LogicQ();
            LogicE();
        }
    }

    Killsteal();
}

// C#: GameEvent.OnGameTick += OnGameUpdate.
static void OnGameUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        LogicQRQ2();
        LogicQ();
        LogicE();
        LogicW();
        LogicR();
        break;
    case OrbwalkingMode::LaneClear:
        Jungle();
        Laneclear();
        break;
    default:
        break;
    }
}

static void LogicQ() {
    const auto qtarget = TSGetTarget(Q.Range, DamageType::Magical);
    const bool useQ = Bool(QMenu, "useQ");
    const bool useQ2 = Bool(QMenu, "useQ2");
    const bool turret = KeyActive(QMenu, "useQTurret");
    if (!qtarget.IsValid()) {
        return;
    }

    const HitChance hitchance = QHitchance();

    if (Extensions::IsValidTarget(qtarget, Q.Range, true)) {
        if (Q.IsReady() && ValidHeroTarget(qtarget, Q.Range) && useQ && IsQOne()) {
            const auto qpred = Q.GetPrediction(qtarget);
            if (HitchanceAtLeast(qpred.Hitchance, hitchance)) {
                Q.Cast(qpred.GetCastPosition());
            }
        }

        if (!turret && qtarget.IsUnderEnemyTurret()) {
            return;
        }

        if (Q.IsReady() && ValidHeroTarget(qtarget, Q2.Range) && useQ2 && IsQTwo()) {
            Q.Cast();
        }
    }
}

static void LogicW() {
    const auto player = Player();
    const float aaRange = RealAutoAttackRange(AIBaseClient(player.Handle()));
    const auto wtarget = TSGetTarget(aaRange, DamageType::Magical);
    const bool useW = Bool(WMenu, "useW");
    if (!wtarget.IsValid()) {
        return;
    }

    if (Extensions::IsValidTarget(wtarget, aaRange, true)) {
        if (useW && W.IsReady() && ValidHeroTarget(wtarget, aaRange)) {
            W.CastOnUnit(AIBaseClient(player.Handle()));
        }
    }
}

static void LogicE() {
    const auto etarget = TSGetTarget(E.Range, DamageType::Physical);
    const bool useE = Bool(EMenu, "useE");
    if (!etarget.IsValid()) {
        return;
    }

    if (Extensions::IsValidTarget(etarget, E.Range, true)) {
        if (E.IsReady() && useE && ValidHeroTarget(etarget, E.Range)) {
            E.Cast();
        }
    }
}

static void LogicR() {
    const auto rtarget = TSGetTarget(R.Range, DamageType::Physical);
    const bool useR = Bool(RMenu, "useR");
    if (!rtarget.IsValid()) {
        return;
    }

    if (Extensions::IsValidTarget(rtarget, R.Range, true)) {
        if (R.IsReady() && useR && ValidHeroTarget(rtarget, R.Range)) {
            if (rtarget.Health() <= RDamage(AIBaseClient(rtarget.Handle()))) {
                R.CastOnUnit(AIBaseClient(rtarget.Handle()));
            }
        }
    }
}

static void LogicQRQ2() {
    const auto player = Player();
    const auto target = TSGetTarget(Q.Range, DamageType::Physical);
    const bool trying = Bool(RMenu, "try");
    if (!target.IsValid()) {
        return;
    }

    const HitChance hitchance = QHitchance();
    const auto tBase = AIBaseClient(target.Handle());

    if (Extensions::IsValidTarget(target, Q.Range, true)) {
        if (Q.IsReady() && R.IsReady() && ValidHeroTarget(target) && IsQOne() && trying) {
            const auto qpred = Q.GetPrediction(target);
            if (HitchanceAtLeast(qpred.Hitchance, hitchance)) {
                if (player.GetSpellDamage(tBase, SpellSlot::Q) + RDamage(tBase) +
                        player.GetSpellDamage(tBase, SpellSlot::Q) >= target.Health()) {
                    Q.Cast(qpred.GetCastPosition());
                }
            }
        }

        if (Q.IsReady() && R.IsReady() && ValidHeroTarget(target) && IsQTwo() && trying) {
            if (player.GetSpellDamage(tBase, SpellSlot::Q) + RDamage(tBase) +
                    player.GetSpellDamage(tBase, SpellSlot::Q) >= target.Health()) {
                R.CastOnUnit(tBase);
            }
        }

        if (Q.IsReady() && !R.IsReady() && ValidHeroTarget(target) && IsQTwo() && trying) {
            if (player.GetSpellDamage(tBase, SpellSlot::Q) + RDamage(tBase) +
                    player.GetSpellDamage(tBase, SpellSlot::Q) >= target.Health()) {
                if (!Extensions::IsValidTarget(target, RealAutoAttackRange(AIBaseClient(player.Handle())), true)) {
                    // MISSING API: DelayAction.Add(350, () => Q2.Cast()) — không có timed
                    // scheduler; cast Q2 ngay (xem missapi.md). Q2 lao vào target.
                    Q.Cast();
                }
            }
        }
    }
}

static void Jungle() {
    const auto player = Player();
    const bool JcQq = Bool(ClearMenu, "JcQ");
    const bool JcWw = Bool(ClearMenu, "JcW");
    const bool JcEe = Bool(ClearMenu, "JcE");

    auto mobs = GameObjects::Jungle();
    mobs.erase(
        std::remove_if(mobs.begin(), mobs.end(),
            [](const AIMinionClient& mob) { return !ValidTarget(mob, Q.Range); }),
        mobs.end());
    std::sort(mobs.begin(), mobs.end(),
        [](const AIMinionClient& a, const AIMinionClient& b) {
            return a.MaxHealth() < b.MaxHealth();
        });

    if (!mobs.empty()) {
        const auto& mob = mobs[0];
        if (JcQq && Q.IsReady() && player.Distance(mob.Position()) < Q.Range) {
            Q.Cast(mob.Position());
        }
        if (JcWw && W.IsReady() &&
                player.Distance(mob.Position()) < RealAutoAttackRange(AIBaseClient(player.Handle()))) {
            W.CastOnUnit(AIBaseClient(player.Handle()));
        }
        if (JcEe && E.IsReady() && player.Distance(mob.Position()) < E.Range) {
            E.Cast();
        }
    }
}

static void Laneclear() {
    const bool lce = Bool(ClearMenu, "LcE");

    if (lce && E.IsReady()) {
        std::vector<AIBaseClient> minions;
        for (const auto& m : GameObjects::EnemyMinions()) {
            if (ValidTarget(m, E.Range) && m.IsMinion()) {
                minions.push_back(AIBaseClient(m.Handle()));
            }
        }
        if (!minions.empty()) {
            const auto eFarm = E.GetLineFarmLocation(minions);
            if (eFarm.Position.IsValid()) {
                E.Cast();
                return;
            }
        }
    }
}

static void Killsteal() {
    const auto player = Player();
    const bool ksQ = Bool(KillStealMenu, "KsQ");
    const bool ksE = Bool(KillStealMenu, "KsE");
    const bool ksR = Bool(KillStealMenu, "KsR", false);

    for (const auto& target : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(target, Q.Range) ||
                target.HasBuff("JudicatorIntervention") ||
                target.HasBuff("kindredrnodeathbuff") ||
                target.HasBuff("Undying Rage")) {
            continue;
        }
        const auto tBase = AIBaseClient(target.Handle());

        if (ksQ && Q.IsReady() && Extensions::IsValidTarget(target, Q.Range, true) && IsQOne()) {
            if (target.DistanceToPlayer() <= Q.Range) {
                if (target.Health() + target.AllShield() <= player.GetSpellDamage(tBase, SpellSlot::Q)) {
                    const auto qpred = Q.GetPrediction(target);
                    if (HitchanceAtLeast(qpred.Hitchance, HitChance::High)) {
                        Q.Cast(qpred.GetCastPosition());
                    }
                }
            }
        }

        if (ksQ && Q.IsReady() && Extensions::IsValidTarget(target, Q2.Range, true) && IsQTwo()) {
            if (target.DistanceToPlayer() <= Q2.Range) {
                if (target.Health() + target.AllShield() <= player.GetSpellDamage(tBase, SpellSlot::Q)) {
                    Q.Cast();
                }
            }
        }

        if (ksE && E.IsReady() && Extensions::IsValidTarget(target, E.Range, true)) {
            if (target.DistanceToPlayer() <= E.Range) {
                if (target.Health() + target.AllShield() <= player.GetSpellDamage(tBase, SpellSlot::E)) {
                    E.Cast();
                }
            }
        }

        if (ksR && R.IsReady() && Extensions::IsValidTarget(target, R.Range, true)) {
            if (target.DistanceToPlayer() <= R.Range) {
                if (target.Health() + target.AllShield() <= player.GetSpellDamage(tBase, SpellSlot::R)) {
                    R.CastOnUnit(tBase);
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
    if (Bool(DrawMenu, "drawQ", false)) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFFFFFFFu);
    }
    if (Bool(DrawMenu, "drawW", false)) {
        Drawing::DrawCircle(player.Position(), W.Range, 0xFF0000FFu);
    }
    if (Bool(DrawMenu, "drawE", false)) {
        Drawing::DrawCircle(player.Position(), E.Range, 0xFF00FF00u);
    }
    if (Bool(DrawMenu, "drawR", false)) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFFFF0000u);
    }
    if (Bool(DrawMenu, "drawIn", false)) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemy) || enemy.IsDead()) {
                continue;
            }
            const float dmg = GetComboDamage(AIBaseClient(enemy.Handle()));
            char buf[64];
            snprintf(buf, sizeof(buf), "Combo: %.0f", dmg);
            Drawing::DrawText(enemy.Position(), buf, 0xFFFFCC00u, true);
        }
    }
}

static void OnGapCloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "AG")) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (!sender.IsValid() || !sender.IsEnemy()) {
        return;
    }
    const auto player = Player();
    if (player.Distance(args.End) < R.Range) {
        R.CastOnUnit(AIBaseClient(sender.Handle()));
    }
}

static void Interrupter_OnInterrupterSpell(const Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    if (!Bool(MiscMenu, "Int")) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (!sender.IsValid()) {
        return;
    }
    const auto player = Player();
    if (player.Distance(sender.Position()) < R.Range) {
        R.CastOnUnit(AIBaseClient(sender.Handle()));
    }
}

// C#: Insec — flash-R kick. target trong AA range + flash ready + ward chưa dùng
// + không dính Q1 → di chuyển ra sau target rồi R + Flash (kick về đội mình).
static void Insec() {
    const auto player = Player();
    const auto target = TSGetTarget(400.0f, DamageType::Physical);
    if (!target.IsValid()) {
        return;
    }

    if (Extensions::IsValidTarget(target, RealAutoAttackRange(AIBaseClient(player.Handle())) + 70.0f, true) &&
            FlashReady() && !target.HasBuff("BlindMonkQOne")) {
        const auto insectarget = TSGetTarget(400.0f, DamageType::Physical);
        if (!insectarget.IsValid()) {
            return;
        }

        CoreControl::IssueMove(insectarget.Position().Extend(player.Position(), 70.0f), true);
        // MISSING API: DelayAction.Add(100, ...) — không có timed scheduler; R + Flash
        // cast tuần tự ngay (xem missapi.md). Flash tới sau target để kick về mình.
        R.CastOnUnit(AIBaseClient(target.Handle()));
        const SpellSlot flashSlot = FlashSlot();
        if (flashSlot != SpellSlot::Unknown) {
            player.Spellbook().CastSpell(flashSlot, target.Position().Extend(player.Position(), -100.0f));
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &OnGameUpdate;
    Events::hook.OnUpdate -= &OnTickUpdate;
    Drawing::OnDraw -= &OnDraw;
    Events::hook.OnInterruptableTarget -= &Interrupter_OnInterrupterSpell;
    Events::hook.OnGapCloser -= &OnGapCloser;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::LeeSin
