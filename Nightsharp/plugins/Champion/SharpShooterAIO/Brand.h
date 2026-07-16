#pragma once

// ============================================================================
// SharpShooter AIO — Brand
// Port từ CSharpFiles_2/Brand.cs (ToxicAio) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Syndra.h.
//
// Kỹ năng:
//   Q Sear            — skillshot line 1040, delay 0.25, width 120, speed 1600, collision.
//   W Pillar of Flame — skillshot circle 900, delay 0.25, radius 260.
//   E Conflagration   — targeted 625.
//   R Pyroclasm       — targeted 750.
//   Passive Blaze     — buff "brandablaze" (Ablaze). Q lên target Ablaze thì stun.
//
// Ghi chú port (giữ 1-1 với C#):
//   * OnGameUpdate: Combo(W,Q,R) / LaneClear(Laneclear+Jungle) / skind + Killsteal.
//   * OnBeforeAA: Combo → LogicE.
//   * OnGapCloser: E/W/Q theo buff brandablaze.
//   * LogicQ: 6 nhánh (3 pred + 3 non-pred), giữ nguyên thứ tự & điều kiện.
//   * ComboDamage draw: Kill / Combo+2AA / Unkillable.
//   * Killsteal Q/W/E/R theo QDamage/WDamage/EDamage/RDamage + GetIncomingDamage.
//   * Damage tables recheck wiki (leagueoflegends.com, patch 2026-07): số C# lệch
//     nên cập nhật theo wiki, ghi comment nguồn. Giữ cấu trúc hàm 1-1.
//   * MISSING API: Me.SetSkin/SkinId (skind), OktwCommon.GetIncomingDamage,
//     MenuSliderButton skin — xem missapi.md.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <vector>

namespace Plugins::SharpAIO::Brand {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* menuQ = nullptr;
inline Menu* menuW = nullptr;
inline Menu* menuE = nullptr;
inline Menu* menuR = nullptr;
inline Menu* menuP = nullptr;
inline Menu* menuL = nullptr;
inline Menu* menuK = nullptr;
inline Menu* menuM = nullptr;
inline Menu* menuD = nullptr;

inline Spell Q{ SpellSlot::Q, 1040.0f };
inline Spell W{ SpellSlot::W, 900.0f };
inline Spell E{ SpellSlot::E, 625.0f };
inline Spell R{ SpellSlot::R, 750.0f };

inline bool Loaded = false;
inline SpellSlot igniteSlot = SpellSlot::Unknown;
inline HitChance hitchance = HitChance::High;

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

// C#: OktwCommon.GetIncomingDamage — không có trong SDK. Trả 0 (ước lượng thấp
// hơn 1 chút khi có nhiều nguồn damage bay tới). Xem missapi.md.
static float GetIncomingDamage(const AIBaseClient&) {
    return 0.0f;
}

// C#: comb(menuP, "Pred") → hitchance theo index. Giữ default High.
static void SetHitchanceFromPred() {
    switch (ListIndex(menuP, "Pred", 2)) {
    case 0: hitchance = HitChance::Low; break;
    case 1: hitchance = HitChance::Medium; break;
    case 2: hitchance = HitChance::High; break;
    case 3: hitchance = HitChance::VeryHigh; break;
    default: hitchance = HitChance::High; break;
    }
}

// ── Damage tính tay ──
// C# gốc dùng bảng cũ; recheck wiki (leagueoflegends.com/Brand, 2026-07):
//   Q Sear            (MAGIC): 70/100/130/160/190 + 65% AP
//   W Pillar of Flame (MAGIC): 75/120/165/210/255 + 70% AP
//   E Conflagration   (MAGIC): 55/80/105/130/155 + 60% AP
//   R Pyroclasm single(MAGIC): 300/525/750 + 90% AP (per-bounce 100/175/250 +30%)
// CDragon/wiki latest: BrandQ..R BaseDamage + AP ratio.
static double QDamage(const AIHeroClient& Qtarget) {
    const auto player = Player();
    if (!ValidUnit(Qtarget)) {
        return 0.0;
    }
    const int qLevel = Q.Level();
    if (qLevel <= 0) {
        return 0.0;
    }
    static const float base[6] = { 0.0f, 70.0f, 100.0f, 130.0f, 160.0f, 190.0f };
    const double raw = base[qLevel] + 0.65 * player.AP();
    return player.CalculateMagicDamage(AIBaseClient(Qtarget.Handle()), static_cast<float>(raw));
}

static double WDamage(const AIHeroClient& Wtarget) {
    const auto player = Player();
    if (!ValidUnit(Wtarget)) {
        return 0.0;
    }
    const int wLevel = W.Level();
    if (wLevel <= 0) {
        return 0.0;
    }
    static const float base[6] = { 0.0f, 75.0f, 120.0f, 165.0f, 210.0f, 255.0f };
    const double raw = base[wLevel] + 0.70 * player.AP();
    return player.CalculateMagicDamage(AIBaseClient(Wtarget.Handle()), static_cast<float>(raw));
}

static double EDamage(const AIHeroClient& Etarget) {
    const auto player = Player();
    if (!ValidUnit(Etarget)) {
        return 0.0;
    }
    const int eLevel = E.Level();
    if (eLevel <= 0) {
        return 0.0;
    }
    static const float base[6] = { 0.0f, 55.0f, 80.0f, 105.0f, 130.0f, 155.0f };
    const double raw = base[eLevel] + 0.60 * player.AP();
    return player.CalculateMagicDamage(AIBaseClient(Etarget.Handle()), static_cast<float>(raw));
}

static double RDamage(const AIHeroClient& Rtarget) {
    const auto player = Player();
    if (!ValidUnit(Rtarget)) {
        return 0.0;
    }
    const int rLevel = R.Level();
    if (rLevel <= 0) {
        return 0.0;
    }
    static const float base[4] = { 0.0f, 300.0f, 525.0f, 750.0f };
    const double raw = base[rLevel] + 0.90 * player.AP();
    return player.CalculateMagicDamage(AIBaseClient(Rtarget.Handle()), static_cast<float>(raw));
}

// C#: ComboDamage — Q+E+R (+ ignite nếu có). Ignite damage: MISSING API
// GetSummonerSpellDamage → bỏ term ignite (xem missapi.md).
static float ComboDamage(const AIBaseClient& enemy) {
    const auto player = Player();
    if (!ValidUnit(enemy)) {
        return 0.0f;
    }
    double damage = 0.0;
    if (Q.IsReady()) {
        damage += player.GetSpellDamage(enemy, SpellSlot::Q);
    }
    if (E.IsReady()) {
        damage += player.GetSpellDamage(enemy, SpellSlot::E);
    }
    if (R.IsReady()) {
        damage += player.GetSpellDamage(enemy, SpellSlot::R);
    }
    return static_cast<float>(damage);
}

// Forward declarations — đúng thứ tự file C#.
static void OnGameUpdate(const GameUpdateEventArgs& args);
static void skind();
static void OnBeforeAA(OrbwalkingActionArgs& args);
static void OnGapCloser(const GapCloserEventArgs& args);
static void OnDraw();
static void LogicQ();
static void LogicW();
static void LogicE();
static void LogicR();
static void Jungle();
static void Laneclear();
static void Killsteal();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Brand", true);

    menuQ = MenuRoot->AddSubMenu(new Menu("Qsettings", "Q settings"));
    menuQ->Add(new MenuBool("UseQ", "Use Q in Combo"));
    menuQ->Add(new MenuBool("PQ", "Use Q only if it can stun"));

    menuW = MenuRoot->AddSubMenu(new Menu("Wsettings", "W settings"));
    menuW->Add(new MenuBool("UseW", "use W in Combo"));

    menuE = MenuRoot->AddSubMenu(new Menu("Esettings", "E settings"));
    menuE->Add(new MenuBool("UseE", "use E in Combo"));

    menuR = MenuRoot->AddSubMenu(new Menu("Rsettings", "R settings"));
    menuR->Add(new MenuBool("UseR", "use R in Combo"));

    menuP = MenuRoot->AddSubMenu(new Menu("Psettings", "Pred settings"));
    menuP->Add(new MenuBool("QPred", "Enable Q Prediction"));
    menuP->Add(new MenuBool("WPred", "Enable W Prediction"));
    menuP->Add(new MenuList("Pred", "Prediction hitchance",
        std::vector<std::string>{ "Low", "Medium", "High", "Very High" }, 2));

    menuL = MenuRoot->AddSubMenu(new Menu("Clear", "Clear settings"));
    menuL->Add(new MenuBool("LcW", "use W to LaneClear"));
    menuL->Add(new MenuBool("LcE", "use E to LaneClear"));
    menuL->Add(new MenuBool("JcQ", "use Q to Jungleclear"));
    menuL->Add(new MenuBool("JcW", "use W to Jungleclear"));
    menuL->Add(new MenuBool("JcE", "use E to Jungleclear"));

    menuK = MenuRoot->AddSubMenu(new Menu("Killsteal", "Killsteal settings"));
    menuK->Add(new MenuBool("KsQ", "use Q to Killsteal"));
    menuK->Add(new MenuBool("KsW", "use W to Killsteal"));
    menuK->Add(new MenuBool("KsE", "use E to Killsteal"));
    menuK->Add(new MenuBool("KsR", "use R to Killsteal"));

    menuM = MenuRoot->AddSubMenu(new Menu("Misc", "Misc settings"));
    menuM->Add(new MenuBool("Ag", "AntiGapCloser"));
    // MISSING API: MenuSliderButton "Skin" + Me.SetSkin — SDK không expose skin
    // changer. Bỏ (chỉ cosmetic). Xem missapi.md.

    menuD = MenuRoot->AddSubMenu(new Menu("Draw", "Draw settings"));
    menuD->Add(new MenuBool("drawQ", "Q Range  (White)", true));
    menuD->Add(new MenuBool("drawW", "W Range  (White)", true));
    menuD->Add(new MenuBool("drawE", "E Range (White)", true));
    menuD->Add(new MenuBool("drawR", "R Range  (Red)", true));
    menuD->Add(new MenuBool("drawD", "Draw Combo Damage", true));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 1040.0f);
    Q.SetSkillshot(0.25f, 120.0f, 1600.0f, true, SpellType::Line);
    Q.DamageType = DamageType::Magical;

    W = Spell(SpellSlot::W, 900.0f);
    W.SetSkillshot(0.25f, 260.0f, FLT_MAX, false, SpellType::Circle);
    W.DamageType = DamageType::Magical;

    E = Spell(SpellSlot::E, 625.0f);
    E.SetTargetted(0.25f, FLT_MAX);
    E.DamageType = DamageType::Magical;

    R = Spell(SpellSlot::R, 750.0f);
    R.SetTargetted(0.25f, FLT_MAX);
    R.DamageType = DamageType::Magical;

    igniteSlot = player.GetSpellSlot("SummonerDot");

    BuildMenu();

    Events::hook.OnGameUpdate += &OnGameUpdate;
    Drawing::OnDraw += &OnDraw;
    Orbwalker::OnBeforeAttack += &OnBeforeAA;
    Events::hook.OnGapCloser += &OnGapCloser;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Brand loaded</font>");
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
        LogicW();
        LogicQ();
        LogicR();
    }

    if (Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear) {
        Laneclear();
        Jungle();
    }

    if (Orbwalker::ActiveMode() == OrbwalkingMode::LastHit) {
    }

    if (Orbwalker::ActiveMode() == OrbwalkingMode::Harass) {
    }
    skind();
    Killsteal();
}

static void skind() {
    // MISSING API: Me.SkinId / Me.SetSkin — SDK không expose. Bỏ (cosmetic).
    // Xem missapi.md.
}

static void OnBeforeAA(OrbwalkingActionArgs&) {
    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo) {
        LogicE();
    }
}

static void OnGapCloser(const GapCloserEventArgs& args) {
    const auto target = AIHeroClient(args.Sender);
    if (!target.IsValid()) {
        return;
    }
    if (Bool(menuM, "Ag", false) && !target.HasBuff("brandablaze") && E.IsReady()) {
        E.CastOnUnit(AIBaseClient(target.Handle()));
    } else if (Bool(menuM, "Ag", false) && !target.HasBuff("brandablaze") && !E.IsReady()) {
        W.Cast(target.Position());
    } else if (Bool(menuM, "Ag", false) && target.HasBuff("brandablaze")) {
        const auto pred = Q.GetPrediction(AIBaseClient(target.Handle()));
        Q.Cast(pred.GetCastPosition());
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    // C# gốc comment out các vòng range (drawQ/W/E/R). Giữ nguyên (không vẽ range).

    if (Bool(menuD, "drawD", true)) {
        for (const auto& enemyVisible : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemyVisible)) {
                continue;
            }
            const auto base = AIBaseClient(enemyVisible.Handle());
            Vector2 screen;
            if (!Drawing::WorldToScreen(enemyVisible.Position(), screen)) {
                continue;
            }
            if (ComboDamage(base) > enemyVisible.Health()) {
                Drawing::DrawText(screen.x + 50.0f, screen.y - 40.0f, 0xFFFF0000u, "Combo=Kill");
            } else if (ComboDamage(base) + player.GetAutoAttackDamage(base, true) * 2.0f > enemyVisible.Health()) {
                Drawing::DrawText(screen.x + 50.0f, screen.y - 40.0f, 0xFFFFA500u, "Combo + 2 AA = Kill");
            } else {
                Drawing::DrawText(screen.x + 50.0f, screen.y - 40.0f, 0xFF00FF00u, "Unkillable with combo + 2 AA");
            }
        }
    }
}

static void LogicQ() {
    const auto qtarget = TSGetTarget(Q.Range, DamageType::Magical);
    const bool useQ = Bool(menuQ, "UseQ", false);
    const bool pq = Bool(menuQ, "PQ", false);
    const bool qpred = Bool(menuP, "QPred", false);
    if (!qtarget.IsValid()) {
        return;
    }
    const auto input = Q.GetPrediction(AIBaseClient(qtarget.Handle()));

    SetHitchanceFromPred();

    if (Q.IsReady() && ValidHeroTarget(qtarget, Q.Range) && HitchanceAtLeast(input.Hitchance, hitchance) && useQ && pq && qtarget.HasBuff("brandablaze") && qpred) {
        Q.Cast(input.GetCastPosition());
    } else if (Q.IsReady() && ValidHeroTarget(qtarget, Q.Range) && HitchanceAtLeast(input.Hitchance, hitchance) && useQ &&
               !pq && !qtarget.HasBuff("brandablaze") && qpred) {
        Q.Cast(input.GetCastPosition());
    } else if ((Q.IsReady() && ValidHeroTarget(qtarget, Q.Range) && HitchanceAtLeast(input.Hitchance, hitchance) && useQ && Q.GetDamage(AIBaseClient(qtarget.Handle())) > qtarget.Health()) && qpred) {
        Q.Cast(input.GetCastPosition());
    } else if (Q.IsReady() && ValidHeroTarget(qtarget, Q.Range) && useQ && pq && qtarget.HasBuff("brandablaze") && !qpred) {
        Q.CastOnUnit(AIBaseClient(qtarget.Handle()));
    } else if (Q.IsReady() && ValidHeroTarget(qtarget, Q.Range) && useQ &&
               !pq && !qtarget.HasBuff("brandablaze") && !qpred) {
        Q.CastOnUnit(AIBaseClient(qtarget.Handle()));
    } else if ((Q.IsReady() && ValidHeroTarget(qtarget, Q.Range) && useQ && Q.GetDamage(AIBaseClient(qtarget.Handle())) > qtarget.Health()) && !qpred) {
        Q.CastOnUnit(AIBaseClient(qtarget.Handle()));
    }
}

static void LogicW() {
    const bool useW = Bool(menuW, "UseW", false);
    const auto wtarget = TSGetTarget(W.Range, DamageType::Magical);
    const bool wpred = Bool(menuP, "WPred", false);
    if (!wtarget.IsValid()) {
        return;
    }
    const auto input = W.GetPrediction(AIBaseClient(wtarget.Handle()));

    SetHitchanceFromPred();

    if (W.IsReady() && ValidHeroTarget(wtarget, W.Range) && HitchanceAtLeast(input.Hitchance, hitchance) && useW && wpred) {
        W.Cast(input.GetCastPosition());
    } else if (W.IsReady() && ValidHeroTarget(wtarget, W.Range) && useW && !wpred) {
        W.Cast(wtarget.Position());
    }
}

static void LogicE() {
    const bool useE = Bool(menuE, "UseE", false);
    const auto etarget = TSGetTarget(E.Range, DamageType::Magical);

    if (E.IsReady() && ValidHeroTarget(etarget, E.Range) && useE) {
        E.CastOnUnit(AIBaseClient(etarget.Handle()));
    }
}

static void LogicR() {
    const auto player = Player();
    const auto rtarget = TSGetTarget(R.Range, DamageType::Magical);
    const bool useR = Bool(menuR, "UseR", false);
    if (!rtarget.IsValid()) {
        return;
    }

    if (R.IsReady() && ValidHeroTarget(rtarget, R.Range) && player.CountEnemyHeroesInRange(750.0f) >= 2 && useR) {
        R.CastOnUnit(AIBaseClient(rtarget.Handle()));
    } else if (R.IsReady() && ValidHeroTarget(rtarget, R.Range) && player.CountEnemyHeroesInRange(750.0f) >= 1 &&
               player.HealthPercent() <= 50.0f && rtarget.HealthPercent() >= 50.0f) {
        R.CastOnUnit(AIBaseClient(rtarget.Handle()));
    }
}

static void Jungle() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const bool JcQq = Bool(menuL, "JcQ", false);
    const bool JcWw = Bool(menuL, "JcW", false);
    const bool JcEe = Bool(menuL, "JcE", false);

    auto mobs = GameObjects::Jungle();
    mobs.erase(
        std::remove_if(mobs.begin(), mobs.end(),
            [](const AIMinionClient& m) { return !ValidTarget(m, E.Range); }),
        mobs.end());
    std::sort(mobs.begin(), mobs.end(),
        [](const AIMinionClient& a, const AIMinionClient& b) { return a.MaxHealth() < b.MaxHealth(); });

    if (!mobs.empty()) {
        const auto& mob = mobs[0];
        if (JcQq && Q.IsReady() && player.Distance(mob.Position()) < Q.Range) {
            Q.Cast(mob.Position());
        }
        if (JcWw && W.IsReady() && player.Distance(mob.Position()) < W.Range) {
            W.Cast(mob.Position());
        }
        if (JcEe && E.IsReady() && player.Distance(mob.Position()) < E.Range) {
            E.CastOnUnit(AIBaseClient(mob.Handle()));
        }
    }
}

static void Laneclear() {
    const bool lcw = Bool(menuL, "LcW", false);
    if (lcw && W.IsReady()) {
        std::vector<AIBaseClient> minions;
        for (const auto& m : GameObjects::EnemyMinions()) {
            if (ValidTarget(m, W.Range) && m.IsMinion()) {
                minions.push_back(AIBaseClient(m.Handle()));
            }
        }
        if (!minions.empty()) {
            const auto wFarmLocation = W.GetCircularFarmLocation(minions);
            if (wFarmLocation.Position.IsValid()) {
                W.Cast(Vector3::From2D(wFarmLocation.Position));
                return;
            }
        }
    }

    const bool lce = Bool(menuL, "LcE", false);
    if (lce && E.IsReady()) {
        AIMinionClient minion;
        for (const auto& m : GameObjects::EnemyMinions()) {
            if (ValidTarget(m, E.Range)) {
                minion = m;
                break;
            }
        }
        if (!minion.IsValid()) {
            return;
        }
        E.CastOnUnit(AIBaseClient(minion.Handle()));
    }
}

static void Killsteal() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const bool ksQ = Bool(menuK, "KsQ", false);
    const bool ksW = Bool(menuK, "KsW", false);
    const bool ksE = Bool(menuK, "KsE", false);
    const bool ksR = Bool(menuK, "KsR", false);
    const auto Qtarget = TSGetTarget(Q.Range, DamageType::Magical);
    const auto Wtarget = TSGetTarget(W.Range, DamageType::Magical);
    const auto Etarget = TSGetTarget(E.Range, DamageType::Magical);
    const auto Rtarget = TSGetTarget(R.Range, DamageType::Magical);

    if (!Qtarget.IsValid()) return;
    if (Qtarget.IsInvulnerable()) return;
    if (!Wtarget.IsValid()) return;
    if (Wtarget.IsInvulnerable()) return;
    if (!Etarget.IsValid()) return;
    if (Etarget.IsInvulnerable()) return;
    if (!Rtarget.IsValid()) return;
    if (Rtarget.IsInvulnerable()) return;

    if (!(player.Distance(Qtarget.Position()) <= Q.Range) ||
        !(QDamage(Qtarget) >= Qtarget.Health() + GetIncomingDamage(AIBaseClient(Qtarget.Handle())))) return;
    if (Q.IsReady() && ksQ) Q.CastOnUnit(AIBaseClient(Qtarget.Handle()));

    if (!(player.Distance(Wtarget.Position()) <= W.Range) ||
        !(WDamage(Wtarget) >= Wtarget.Health() + GetIncomingDamage(AIBaseClient(Wtarget.Handle())))) return;
    if (W.IsReady() && ksW) W.Cast(Wtarget.Position());

    if (!(player.Distance(Etarget.Position()) <= E.Range) ||
        !(EDamage(Qtarget) >= Etarget.Health() + GetIncomingDamage(AIBaseClient(Etarget.Handle())))) return;
    if (E.IsReady() && ksE) E.CastOnUnit(AIBaseClient(Etarget.Handle()));

    if (!(player.Distance(Rtarget.Position()) <= R.Range) ||
        !(RDamage(Rtarget) >= Rtarget.Health() + GetIncomingDamage(AIBaseClient(Rtarget.Handle())))) return;
    if (R.IsReady() && ksR) R.CastOnUnit(AIBaseClient(Rtarget.Handle()));
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &OnGameUpdate;
    Drawing::OnDraw -= &OnDraw;
    Orbwalker::OnBeforeAttack -= &OnBeforeAA;
    Events::hook.OnGapCloser -= &OnGapCloser;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Brand
