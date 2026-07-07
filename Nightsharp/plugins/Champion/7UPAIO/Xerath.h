#pragma once

#include "Common.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::AIO7UP::Xerath {

using namespace Common;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;
inline Menu* KillStealMenu = nullptr;
inline Menu* UltiMenu = nullptr;
inline Menu* SemiMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 1550.0f };
inline Spell W{ SpellSlot::W, 950.0f };
inline Spell E{ SpellSlot::E, 1050.0f };
inline Spell R{ SpellSlot::R, 4990.0f };

inline bool Loaded = false;
inline DWORD LastUpdateTick = 0;

static HitChance QHitchance() {
    return Bool(MiscMenu, "qslowcast") ? HitChance::VeryHigh : HitChance::High;
}

static HitChance RHitchance() {
    return Bool(MiscMenu, "rslowcast") ? HitChance::VeryHigh : HitChance::High;
}

static void BuildMenu();
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnGapcloser(const GapCloserEventArgs& args);
static void OnInterruptableSpell(const Events::InterruptableSpell::InterruptableTargetEventArgs& args);
static void OnProcessSpell(const Events::ProcessSpellEventArgs& args);
static void OnUnload();

static void Combo();
static void Harass();
static void LaneClear();
static void JungleClear();
static void KillSteal();
static void AutoR();
static void SemiAutomatic();

static float QDamage(const AIBaseClient& target);
static float WDamage(const AIBaseClient& target);
static float EDamage(const AIBaseClient& target);

static void BuildMenu() {
    MenuRoot = new Menu("champion.7upaio", "7UP - Xerath", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("ComboQ", "Use Q"));
    ComboMenu->Add(new MenuBool("ComboW", "Use W"));
    ComboMenu->Add(new MenuBool("ComboE", "Use E"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("HarassQ", "Use Q"));
    HarassMenu->Add(new MenuBool("HarassW", "Use W"));
    HarassMenu->Add(new MenuBool("HarassE", "Use E"));
    HarassMenu->Add(new MenuSlider("Mana", "Min Mana Harass", 50, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "LaneClear"));
    LaneClearMenu->Add(new MenuBool("LaneQ", "Use Q"));
    LaneClearMenu->Add(new MenuSlider("MinQ", "Hit Minions LaneClear", 3, 1, 6));
    LaneClearMenu->Add(new MenuBool("LaneW", "Use W"));
    LaneClearMenu->Add(new MenuSlider("MinW", "Hit Minions LaneClear", 3, 1, 6));
    LaneClearMenu->Add(new MenuSlider("ManaLC", "Min Mana LaneClear", 60, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("JungleClear Settings", "JungleClear"));
    JungleClearMenu->Add(new MenuBool("QJungle", "Use Q JungleClear"));
    JungleClearMenu->Add(new MenuBool("WJungle", "Use W JungleClear"));
    JungleClearMenu->Add(new MenuBool("EJungle", "Use E JungleClear"));
    JungleClearMenu->Add(new MenuSlider("ManaJG", "Min Mana JungleClear [Q]", 40, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("qslowcast", "Slow Q Cast (high hitchance)"));
    MiscMenu->Add(new MenuBool("rslowcast", "Slow R Cast (high hitchance)"));
    MiscMenu->Add(new MenuBool("eantigapcloser", "Use E AntiGapcloser"));
    MiscMenu->Add(new MenuBool("einterrupt", "Use E Interrupt Spell"));

    KillStealMenu = MenuRoot->AddSubMenu(new Menu("KillSteal Settings", "KillSteal"));
    KillStealMenu->Add(new MenuBool("KsQ", "Use Q KillSteal"));
    KillStealMenu->Add(new MenuBool("KsW", "Use W KillSteal"));
    KillStealMenu->Add(new MenuBool("KsE", "Use E KillSteal"));

    UltiMenu = MenuRoot->AddSubMenu(new Menu("RSetting", "R Settings"));
    UltiMenu->Add(new MenuKeyBind("RKey", "R Key", SDK::Keys::T, KeyBindType::Press));
    UltiMenu->Add(new MenuBool("NearMouse", "Near Mouse"));
    UltiMenu->Add(new MenuSlider("MouseZone", "Mouse Zone", 600, 0, 1200));

    SemiMenu = MenuRoot->AddSubMenu(new Menu("Semi", "Semi Key"));
    SemiMenu->Add(new MenuKeyBind("WKey", "Semi W Key", SDK::Keys::W, KeyBindType::Press));
    SemiMenu->Add(new MenuKeyBind("EKey", "Semi E Key", SDK::Keys::E, KeyBindType::Press));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Drawing", "Drawings"));
    DrawMenu->Add(new MenuBool("drawQ", "Draw Q"));
    DrawMenu->Add(new MenuBool("drawW", "Draw W"));
    DrawMenu->Add(new MenuBool("drawE", "Draw E"));
    DrawMenu->Add(new MenuBool("drawR", "Draw R"));
    DrawMenu->Add(new MenuBool("RMouse", "Draw R Mouse"));

    MenuRoot->Attach();
}

static float QDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    static const float baseDamage[] = { 0, 70, 110, 150, 190, 230 };
    const int level = Q.Level();
    if (level < 0 || level > 5) return 0.0f;
    const float raw = baseDamage[level] + 0.848f * player.AP();
    return player.CalculateMagicDamage(target, raw);
}

static float WDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    static const float baseDamage[] = { 0, 60, 95, 130, 165, 200 };
    const int level = W.Level();
    if (level < 0 || level > 5) return 0.0f;
    const float raw = baseDamage[level] + 0.58f * player.AP();
    return player.CalculateMagicDamage(target, raw);
}

static float EDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    static const float baseDamage[] = { 0, 80, 110, 140, 170, 200 };
    const int level = E.Level();
    if (level < 0 || level > 5) return 0.0f;
    const float raw = baseDamage[level] + 0.448f * player.AP();
    return player.CalculateMagicDamage(target, raw);
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (!Q.IsCharging()) {
        if (!player.HasBuff("XerathLocusOfPower2")) {
            if (Bool(ComboMenu, "ComboW") && W.IsReady()) {
                const auto target = GetTarget(W.Range, DamageType::Magical);
                if (ValidHeroTarget(target, W.Range)) {
                    const auto pred = W.GetPrediction(target);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh)) {
                        CastPosition(W, pred.GetCastPosition(), target);
                    }
                }
            }

            if (Bool(ComboMenu, "ComboE") && E.IsReady()) {
                const auto target = GetTarget(E.Range, DamageType::Magical);
                if (ValidHeroTarget(target, E.Range)) {
                    const auto pred = E.GetPrediction(target);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh)) {
                        CastPosition(E, pred.GetCastPosition(), target);
                    }
                }
            }

            if (Bool(ComboMenu, "ComboQ") && Q.IsReady()) {
                const auto target = GetTarget(static_cast<float>(Q.ChargedMaxRange), DamageType::Magical);
                if (ValidHeroTarget(target, static_cast<float>(Q.ChargedMaxRange))) {
                    if (!W.IsReady() || target.DistanceToPlayer() > 850.0f) {
                        const auto pred = Q.GetPrediction(target);
                        if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                            Q.StartCharging();
                        }
                    }
                }
            }
        }
    } else {
        if (Bool(ComboMenu, "ComboQ") && Q.IsReady() && Q.IsCharging()) {
            const auto target = GetTarget(Q.Range, DamageType::Magical);
            if (ValidHeroTarget(target, Q.Range)) {
                const auto pred = Q.GetPrediction(target);
                if (HitchanceAtLeast(pred.Hitchance, QHitchance())) {
                    Q.ShootChargedSpell(pred.GetCastPosition());
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

    if (!Q.IsCharging()) {
        if (!player.HasBuff("XerathLocusOfPower2") &&
            player.ManaPercent() >= static_cast<float>(Slider(HarassMenu, "Mana", 50))) {
            if (Bool(HarassMenu, "HarassW") && W.IsReady()) {
                const auto target = GetTarget(W.Range, DamageType::Magical);
                if (ValidHeroTarget(target, W.Range)) {
                    const auto pred = W.GetPrediction(target);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh)) {
                        CastPosition(W, pred.GetCastPosition(), target);
                        return;
                    }
                }
            }

            if (Bool(HarassMenu, "HarassE") && E.IsReady()) {
                const auto target = GetTarget(E.Range, DamageType::Magical);
                if (ValidHeroTarget(target, E.Range)) {
                    const auto pred = E.GetPrediction(target);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh)) {
                        CastPosition(E, pred.GetCastPosition(), target);
                        return;
                    }
                }
            }

            if (Bool(HarassMenu, "HarassQ") && Q.IsReady()) {
                const auto target = GetTarget(static_cast<float>(Q.ChargedMaxRange), DamageType::Magical);
                if (ValidHeroTarget(target, static_cast<float>(Q.ChargedMaxRange))) {
                    if (!W.IsReady() || target.DistanceToPlayer() > 850.0f) {
                        const auto pred = Q.GetPrediction(target);
                        if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                            Q.StartCharging();
                        }
                    }
                }
            }
        }
    } else {
        if (Bool(HarassMenu, "HarassQ") && Q.IsReady() && Q.IsCharging()) {
            const auto target = GetTarget(Q.Range, DamageType::Magical);
            if (ValidHeroTarget(target, Q.Range)) {
                const auto pred = Q.GetPrediction(target);
                if (HitchanceAtLeast(pred.Hitchance, QHitchance())) {
                    Q.ShootChargedSpell(pred.GetCastPosition());
                }
            }
        }
    }
}

static void LaneClear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (!Q.IsCharging()) {
        if (!player.HasBuff("XerathLocusOfPower2") &&
            player.ManaPercent() >= static_cast<float>(Slider(LaneClearMenu, "ManaLC", 60))) {
            if (Bool(LaneClearMenu, "LaneW") && W.IsReady()) {
                std::vector<AIBaseClient> minions;
                for (const auto& minion : GameObjects::EnemyMinions()) {
                    if (ValidTarget(minion, W.Range) && minion.IsMinion()) {
                        minions.emplace_back(minion.Handle());
                    }
                }

                if (!minions.empty()) {
                    const auto farmLoc = W.GetCircularFarmLocation(minions);
                    if (farmLoc.Position.IsValid() &&
                        farmLoc.MinionsHit >= Slider(LaneClearMenu, "MinW", 3)) {
                        W.Cast(farmLoc.Position);
                        return;
                    }
                }
            }

            if (Bool(LaneClearMenu, "LaneQ") && Q.IsReady()) {
                std::vector<AIBaseClient> minions;
                for (const auto& minion : GameObjects::EnemyMinions()) {
                    if (ValidTarget(minion, static_cast<float>(Q.ChargedMaxRange)) && minion.IsMinion()) {
                        minions.emplace_back(minion.Handle());
                    }
                }

                if (!minions.empty()) {
                    const auto farmLoc = Q.GetLineFarmLocation(minions);
                    if (farmLoc.Position.IsValid() &&
                        farmLoc.MinionsHit >= Slider(LaneClearMenu, "MinQ", 3)) {
                        Q.StartCharging();
                    }
                }
            }
        }
    } else {
        if (Bool(LaneClearMenu, "LaneQ") && Q.IsReady() && Q.IsCharging()) {
            std::vector<AIBaseClient> minions;
            for (const auto& minion : GameObjects::EnemyMinions()) {
                if (ValidTarget(minion, Q.Range) && minion.IsMinion()) {
                    minions.emplace_back(minion.Handle());
                }
            }

            if (!minions.empty()) {
                const auto farmLoc = Q.GetLineFarmLocation(minions);
                if (farmLoc.Position.IsValid() &&
                    farmLoc.MinionsHit >= Slider(LaneClearMenu, "MinQ", 3)) {
                    Q.ShootChargedSpell(farmLoc.Position);
                }
            }
        }
    }
}

static void JungleClear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (!Q.IsCharging()) {
        if (!player.HasBuff("XerathLocusOfPower2") &&
            player.ManaPercent() >= static_cast<float>(Slider(JungleClearMenu, "ManaJG", 40))) {
            if (Bool(JungleClearMenu, "WJungle") && W.IsReady()) {
                AIMinionClient bestMob;
                float bestMaxHealth = 0.0f;
                for (const auto& mob : GameObjects::Jungle()) {
                    if (!ValidTarget(mob, W.Range) || mob.GetJungleType() == JungleType::Unknown) {
                        continue;
                    }
                    if (mob.MaxHealth() > bestMaxHealth) {
                        bestMob = mob;
                        bestMaxHealth = mob.MaxHealth();
                    }
                }

                if (bestMob.IsValid() && ValidTarget(bestMob, W.Range)) {
                    const auto pred = W.GetPrediction(bestMob);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                        CastPosition(W, pred.GetCastPosition(), bestMob);
                        return;
                    }
                }
            }

            if (Bool(JungleClearMenu, "EJungle") && E.IsReady()) {
                AIMinionClient bestMob;
                float bestMaxHealth = 0.0f;
                for (const auto& mob : GameObjects::Jungle()) {
                    if (!ValidTarget(mob, E.Range) || mob.GetJungleType() == JungleType::Unknown) {
                        continue;
                    }
                    if (mob.MaxHealth() > bestMaxHealth) {
                        bestMob = mob;
                        bestMaxHealth = mob.MaxHealth();
                    }
                }

                if (bestMob.IsValid() && ValidTarget(bestMob, E.Range)) {
                    const auto pred = E.GetPrediction(bestMob);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                        CastPosition(E, pred.GetCastPosition(), bestMob);
                        return;
                    }
                }
            }

            if (Bool(JungleClearMenu, "QJungle") && Q.IsReady()) {
                AIMinionClient bestMob;
                float bestMaxHealth = 0.0f;
                for (const auto& mob : GameObjects::Jungle()) {
                    if (!ValidTarget(mob, static_cast<float>(Q.ChargedMaxRange)) || mob.GetJungleType() == JungleType::Unknown) {
                        continue;
                    }
                    if (mob.MaxHealth() > bestMaxHealth) {
                        bestMob = mob;
                        bestMaxHealth = mob.MaxHealth();
                    }
                }

                if (bestMob.IsValid() && ValidTarget(bestMob, static_cast<float>(Q.ChargedMaxRange))) {
                    Q.StartCharging();
                }
            }
        }
    } else {
        if (Bool(JungleClearMenu, "QJungle") && Q.IsReady() && Q.IsCharging()) {
            AIMinionClient bestMob;
            float bestMaxHealth = 0.0f;
            for (const auto& mob : GameObjects::Jungle()) {
                if (!ValidTarget(mob, static_cast<float>(Q.ChargedMaxRange)) || mob.GetJungleType() == JungleType::Unknown) {
                    continue;
                }
                if (mob.MaxHealth() > bestMaxHealth) {
                    bestMob = mob;
                    bestMaxHealth = mob.MaxHealth();
                }
            }

            if (bestMob.IsValid() && ValidTarget(bestMob, Q.Range)) {
                const auto pred = Q.GetPrediction(bestMob);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    Q.ShootChargedSpell(pred.GetCastPosition());
                }
            }
        }
    }
}

static void KillSteal() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (player.HasBuff("XerathLocusOfPower2") || Q.IsCharging()) {
        return;
    }

    const bool ksQ = Bool(KillStealMenu, "KsQ");
    const bool ksW = Bool(KillStealMenu, "KsW");
    const bool ksE = Bool(KillStealMenu, "KsE");

    for (const auto& target : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(target, W.Range) ||
            target.HasBuff("JudicatorIntervention") ||
            target.HasBuff("kindredrnodeathbuff") ||
            target.HasBuff("Undying Rage") ||
            target.HasBuff("UndyingRage")) {
            continue;
        }

        if (ksQ && Q.IsReady() && target.DistanceToPlayer() > 150.0f) {
            if (target.Health() + target.AllShield() <= QDamage(target)) {
                const auto target1 = GetTarget(static_cast<float>(Q.ChargedMaxRange), DamageType::Magical);
                if (ValidHeroTarget(target1, static_cast<float>(Q.ChargedMaxRange))) {
                    if (!W.IsReady() || target1.DistanceToPlayer() > 850.0f) {
                        const auto pred = Q.GetPrediction(target1);
                        if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                            Q.StartCharging();
                        }
                    }
                } else {
                    const auto target2 = GetTarget(Q.Range, DamageType::Magical);
                    if (ValidHeroTarget(target2, Q.Range)) {
                        const auto pred = Q.GetPrediction(target2);
                        if (HitchanceAtLeast(pred.Hitchance, QHitchance())) {
                            Q.ShootChargedSpell(pred.GetCastPosition());
                        }
                    }
                }
            }
        }

        if (ksW && W.IsReady()) {
            if (target.Health() + target.AllShield() <= WDamage(target)) {
                CastPosition(W, target.Position(), target);
            }
        }

        if (ksE && E.IsReady() && target.DistanceToPlayer() <= 500.0f) {
            if (target.Health() + target.AllShield() <= EDamage(target)) {
                E.Cast();
            }
        }
    }
}

static void AutoR() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (!player.HasBuff("XerathLocusOfPower2") || Q.IsCharging()) {
        return;
    }

    auto target = GetTarget(R.Range, DamageType::Magical);

    if (Bool(UltiMenu, "NearMouse") && Slider(UltiMenu, "MouseZone", 0) > 0) {
        const float mouseZone = static_cast<float>(Slider(UltiMenu, "MouseZone", 600));
        AIHeroClient best;
        float bestDist = mouseZone;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemy, R.Range)) {
                continue;
            }
            const float dist = enemy.Position().Distance2D(Game::CursorPos());
            if (dist <= bestDist) {
                best = enemy;
                bestDist = dist;
            }
        }
        if (best.IsValid()) {
            target = best;
        }
    }

    if (ValidHeroTarget(target, R.Range)) {
        if (Key(UltiMenu, "RKey")) {
            const auto pred = R.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, RHitchance())) {
                CastPosition(R, pred.GetCastPosition(), target);
            }
        }
    }
}

static void SemiAutomatic() {
    if (Key(SemiMenu, "WKey") && W.IsReady()) {
        const auto target = GetTarget(W.Range, DamageType::Magical);
        if (ValidHeroTarget(target, W.Range)) {
            CastPosition(W, target.Position(), target);
        }
    }

    if (Key(SemiMenu, "EKey") && E.IsReady()) {
        const auto target = GetTarget(E.Range, DamageType::Magical);
        if (ValidHeroTarget(target, E.Range)) {
            CastPosition(E, target.Position(), target);
        }
    }
}

static void OnGapcloser(const GapCloserEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }

    if (Game::IsChatOpen() || Game::IsShopOpen()) {
        return;
    }

    if (Bool(MiscMenu, "eantigapcloser") && E.IsReady()) {
        const Vector3 endPos = args.End;
        if (Extensions::DistanceToPlayer(endPos) < 250.0f) {
            const auto sender = AIHeroClient(args.Sender);
            if (ValidHeroTarget(sender, E.Range)) {
                const auto pred = E.GetPrediction(sender);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    CastPosition(E, pred.GetCastPosition(), sender);
                }
            }
        }
    }
}

static void OnInterruptableSpell(const Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }

    if (Game::IsChatOpen() || Game::IsShopOpen()) {
        return;
    }

    if (Bool(MiscMenu, "einterrupt") && E.IsReady() &&
        args.DangerLevel >= DangerLevel::Medium) {
        const auto sender = AIHeroClient(args.Sender);
        if (ValidHeroTarget(sender, E.Range)) {
            const auto pred = E.GetPrediction(sender);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                CastPosition(E, pred.GetCastPosition(), sender);
            }
        }
    }
}

static void OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    // TODO SDK: The C# code tracks YasuoWall position via OnProcessSpellCast.
    // The NightSharp SDK has OnProcessSpell event but the args structure
    // differs slightly. The Yasuo wall tracking is a minor feature and
    // can be added later if needed.
    (void)args;
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    if (!ShouldRunNow(LastUpdateTick, 40)) {
        return;
    }

    const auto player = Player();
    const bool qCharging = Q.IsCharging();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling() ||
        Game::IsChatOpen() || (player.Spellbook().IsWindingUp() && !qCharging)) {
        return;
    }

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        return;
    case OrbwalkingMode::Harass:
        Harass();
        break;
    case OrbwalkingMode::LaneClear:
        LaneClear();
        JungleClear();
        break;
    default:
        break;
    }

    KillSteal();
    AutoR();
    SemiAutomatic();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 1550.0f);
    Q.SetSkillshot(0.55f, 65.0f, FLT_MAX, false, SpellType::Line);
    Q.SetCharged("XerathArcanopulseChargeUp", "XerathArcanopulseChargeUp", 750, 1550, 1.5f);

    W = Spell(SpellSlot::W, 950.0f);
    W.SetSkillshot(0.65f, 110.0f, FLT_MAX, false, SpellType::Circle);

    E = Spell(SpellSlot::E, 1050.0f);
    E.SetSkillshot(0.25f, 55.0f, 1400.0f, true, SpellType::Line);

    R = Spell(SpellSlot::R, 4990.0f);
    R.SetSkillshot(0.70f, 110.0f, FLT_MAX, false, SpellType::Circle);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnGapCloser += &OnGapcloser;
    Events::hook.OnInterruptableSpell += &OnInterruptableSpell;
    Events::hook.OnProcessSpell += &OnProcessSpell;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>7UP - Xerath loaded</font>");
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnGapCloser -= &OnGapcloser;
    Events::hook.OnInterruptableSpell -= &OnInterruptableSpell;
    Events::hook.OnProcessSpell -= &OnProcessSpell;

    Loaded = false;
}

} // namespace Plugins::AIO7UP::Xerath
