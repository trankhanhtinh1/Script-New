#pragma once

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Port of 7UPAIO/Champion/Xerath.cs. Function order mirrors the C# file.
//
// Spell metadata comes from local GameData 16.14.7945912
// (extracted/Champions/Xerath/data/characters/xerath/xerath.bin, converted with
// ritobin_cli) rather than the C# literals, which are several patches stale.
// Damage is computed from that same source instead of sdk/Data/DamageData.h —
// that table has Xerath E base damage as all zeros, so GetSpellDamage(E) would
// return ratio-only numbers.
//
// Five defects in the C# source are corrected here rather than carried over;
// each is marked `FIX(source)` at the site.
// ─────────────────────────────────────────────────────────────────────────────

namespace Plugins::AIO7UP::Xerath {

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* Misc = nullptr;
inline Menu* KillStealMenu = nullptr;
inline Menu* Ulti = nullptr;
inline Menu* Semi = nullptr;
inline Menu* Draw = nullptr;

// GameData: XerathArcanopulseChargeUp mSpell.castRange = 750.
inline Spell Q{ SpellSlot::Q, 750.0f };
// GameData: XerathArcaneBarrage2 mSpell.castRange = 1000 (C# had 950).
inline Spell W{ SpellSlot::W, 1000.0f };
// GameData: XerathMageSpear mSpell.castRangeDisplayOverride = 1050.
inline Spell E{ SpellSlot::E, 1050.0f };
// GameData: XerathLocusOfPower2 mSpell.castRange = 5000 (C# had 4990).
inline Spell R{ SpellSlot::R, 5000.0f };

inline bool Loaded = false;
// Tracked because the C# tracked them. The wall itself no longer needs a
// champion-side check: Q/W/R pass collision=false to mean "minions do not stop
// this", and the prediction layer now evaluates the projectile-wall group
// independently of that flag, so an enemy Wind Wall already reports as a
// collision through Spell::GetPrediction. Kept as the source had them in case a
// future rule needs the cast timestamp.
inline int WallCastT = 0;
inline Vec2 YasuoWallCastedPos = {};

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

static bool Key(Menu* menu, const char* key, bool fallback = false) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuKeyBind>(key);
    return item ? item->Active : fallback;
}

static void RemoveKeyPermashow(Menu* menu, const char* key) {
    if (auto* item = menu ? menu->Get<MenuKeyBind>(key) : nullptr) {
        item->RemovePermashow();
    }
}

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
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

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static void BuildMenu();
static void OnGameLoad();
static HitChance QHitchance();
static HitChance RHitchance();
static void OnInterrupterSpell(
    const Events::InterruptableSpell::InterruptableTargetEventArgs& args);
static void Obj_AI_Base_OnProcessSpellCast(const ProcessSpellEventArgs& args);
static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args);
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void Combo();
static void AutoR();
static void SemiAutomatic();
static double QDamage(const AIBaseClient& target);
static double WDamage(const AIBaseClient& target);
static double EDamage(const AIBaseClient& target);
static double RDamage(const AIBaseClient& target);
static void KillSteal();
static void Harass();
static void LaneClear();
static void JungleClear();
static void OnUnload();

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
    // FIX(source): Xerath.cs registers this slider as "ManaLC" but JungleClear()
    // reads JungleClearMenu["ManaJG"], so the lookup never resolved and jungle
    // clear was dead. Registered under the key the logic actually asks for.
    JungleClearMenu->Add(new MenuSlider("ManaJG", "Min Mana JungleClear [Q]", 40, 0, 100));

    Misc = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    Misc->Add(new MenuBool("qslowcast", "Slow Q Cast(high hitchance)"));
    Misc->Add(new MenuBool("rslowcast", "Slow R Cast(high hitchance)"));
    // FIX(source): registered lowercase "eantigapcloser" while both readers used
    // Misc["EAntiGapcloser"]; the gapcloser and interrupter paths could never
    // fire. One key spelling now, used by registration and both readers.
    Misc->Add(new MenuBool("eantigapcloser", "Use E AntiGapcloser"));
    Misc->Add(new MenuBool("einterrupt", "Use E Interrupt Spell"));

    KillStealMenu = MenuRoot->AddSubMenu(new Menu("KillSteal Settings", "KillSteal"));
    KillStealMenu->Add(new MenuBool("KsQ", "Use Q KillSteal"));
    KillStealMenu->Add(new MenuBool("KsW", "Use W KillSteal"));
    KillStealMenu->Add(new MenuBool("KsE", "Use E KillSteal"));

    Ulti = MenuRoot->AddSubMenu(new Menu("RSetting", "R Settings"));
    Ulti->Add(new MenuKeyBind("RKey", "R Key", SDK::Keys::T, KeyBindType::Press))->Permashow();
    Ulti->Add(new MenuBool("NearMouse", "Near Mouse"));
    Ulti->Add(new MenuSlider("MouseZone", "Mouse Zone", 600, 0, 1200));

    Semi = MenuRoot->AddSubMenu(new Menu("Semi", "Semi Key"));
    Semi->Add(new MenuKeyBind("WKey", "Semi W Key", SDK::Keys::W, KeyBindType::Press));
    Semi->Add(new MenuKeyBind("EKey", "Semi E Key", SDK::Keys::E, KeyBindType::Press));

    Draw = MenuRoot->AddSubMenu(new Menu("draw", "Drawings"));
    Draw->Add(new MenuBool("drawQ", "Draw Q"));
    Draw->Add(new MenuBool("drawW", "Draw W"));
    Draw->Add(new MenuBool("drawE", "Draw E"));
    Draw->Add(new MenuBool("drawR", "Draw R"));
    Draw->Add(new MenuBool("RMouse", "Draw R Mouse"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (Loaded) {
        return;
    }

    // GameData 16.14: XerathArcanopulse2 mSpell.mCastTime = 0.5 (C# 0.55),
    // XerathArcanopulseChargeUp mSpell.mLineWidth = 100 (C# 65).
    //
    // Minions do not stop Arcanopulse but a Wind Wall does, and the C# expressed
    // only the first half by passing collision=false — which in this SDK switches
    // the whole collision pass off, walls included, so Q fired straight through a
    // wall. Asking for collision with a wall-only object set says both things:
    // ProcessProjectileWalls runs while the hero and minion passes are skipped.
    Q = Spell(SpellSlot::Q, 750.0f);
    Q.SetSkillshot(0.5f, 100.0f, FLT_MAX, true, SpellType::Line);
    Q.SetCollisionObjects(SDK::CollisionableObjects::YasuoWall);
    // GameData 16.14: castRange 750 -> mCastRangeGrowthMax 1700 over
    // mCastRangeGrowthDuration 1.5s. C# used a 1550 maximum.
    Q.SetCharged("XerathArcanopulseChargeUp", "XerathArcanopulseChargeUp", 750, 1700, 1.5f);

    // GameData 16.14: mCastTime 0.25 plus DataValues DamageDelay 0.5. The C#
    // 0.65 sits between the two and is a tuned prediction delay, so it is kept.
    W = Spell(SpellSlot::W, 1000.0f);
    W.SetSkillshot(0.65f, 110.0f, FLT_MAX, false, SpellType::Circle);

    // GameData 16.14: XerathMageSpear mSpell.mLineWidth = 70 (C# 55),
    // mCastTime 0.25. Missile speed kept from the source.
    E = Spell(SpellSlot::E, 1050.0f);
    E.SetSkillshot(0.25f, 70.0f, 1400.0f, true, SpellType::Line);

    // GameData 16.14: XerathLocusOfPower2 DataValues AoESize = 200 (C# 110).
    R = Spell(SpellSlot::R, 5000.0f);
    R.SetSkillshot(0.70f, 200.0f, FLT_MAX, false, SpellType::Circle);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnGapCloser += &Gapcloser_OnGapcloser;
    Events::hook.OnInterruptableSpell += &OnInterrupterSpell;
    Events::hook.OnProcessSpell += &Obj_AI_Base_OnProcessSpellCast;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>7UP - Xerath loaded</font>");
}

static HitChance QHitchance() {
    return Bool(Misc, "qslowcast") ? HitChance::VeryHigh : HitChance::High;
}

static HitChance RHitchance() {
    return Bool(Misc, "rslowcast") ? HitChance::VeryHigh : HitChance::High;
}

static void OnInterrupterSpell(
    const Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    const auto player = Player();
    if (player.IsDead() || player.IsRecalling()) {
        return;
    }

    if (Game::IsChatOpen() || Game::IsShopOpen()) {
        return;
    }

    const AIHeroClient sender(args.Sender);
    // FIX(source): reader key matches the registered "eantigapcloser".
    if (Bool(Misc, "eantigapcloser") && E.IsReady() &&
        static_cast<int>(args.DangerLevel) >= static_cast<int>(SDK::DangerLevel::Medium) &&
        sender.IsValid() && sender.DistanceToPlayer() < E.Range) {
        const auto pred = E.GetPrediction(sender);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
            E.Cast(pred.GetCastPosition());
        }
    }
}

static void Obj_AI_Base_OnProcessSpellCast(const ProcessSpellEventArgs& args) {
    if (!Loaded) {
        return;
    }

    // Last cast time of spells.
    // FIX(source): the C# matched "SyndraQ"/"SyndraW"/"SyndraE" here, left over
    // from a Syndra script — Xerath never casts those, so the timestamps never
    // updated. Matching the real Xerath spell names instead.
    if (Events::IsLocalPlayer(args.Sender)) {
        const std::string spellName = args.SpellName;
        if (spellName == "XerathArcanopulseChargeUp" ||
            spellName == "XerathArcanopulse2") {
            Q.LastCastAttemptT = Variables::TickCount();
        }
        if (spellName == "XerathArcaneBarrage2") {
            W.LastCastAttemptT = Variables::TickCount();
        }
        if (spellName == "XerathMageSpear") {
            E.LastCastAttemptT = Variables::TickCount();
        }
    }

    const AIBaseClient sender(args.Sender.Ptr);
    if (sender.IsValid() && sender.IsAlly() &&
        std::string(args.SpellName) == "YasuoWMovingWall") {
        WallCastT = Variables::TickCount();
        YasuoWallCastedPos = sender.Position().To2D();
    }
}

static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args) {
    const auto player = Player();
    if (player.IsDead() || player.IsRecalling()) {
        return;
    }

    if (Game::IsChatOpen() || Game::IsShopOpen()) {
        return;
    }

    // FIX(source): reader key matches the registered "eantigapcloser".
    if (Bool(Misc, "eantigapcloser") && E.IsReady() &&
        args.End.Distance(player.Position()) < 250.0f) {
        const AIHeroClient sender(args.Sender);
        if (!sender.IsValid()) {
            return;
        }
        const auto pred = E.GetPrediction(sender);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
            E.Cast(pred.GetCastPosition());
        }
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (player.IsDead() ||
        player.IsRecalling() ||
        Game::IsChatOpen() ||
        player.Spellbook().IsWindingUp()) {
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

static void Combo() {
    const auto player = Player();
    if (!Q.IsCharging()) {
        // ult check
        if (!player.HasBuff("XerathLocusOfPower2")) {
            if (Bool(ComboMenu, "ComboW") && W.IsReady()) {
                const auto target = GetTarget(W.Range, DamageType::Magical);
                if (ValidHeroTarget(target, W.Range)) {
                    const auto pred = W.GetPrediction(target);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh)) {
                        W.Cast(pred.GetCastPosition());
                    }
                }
            }

            if (Bool(ComboMenu, "ComboE") && E.IsReady()) {
                const auto target = GetTarget(E.Range, DamageType::Magical);
                if (ValidHeroTarget(target, E.Range)) {
                    const auto pred = E.GetPrediction(target);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh)) {
                        E.Cast(pred.GetCastPosition());
                    }
                }
            }

            if (Bool(ComboMenu, "ComboQ") && Q.IsReady()) {
                const float chargedMax = static_cast<float>(Q.ChargedMaxRange);
                const auto target = GetTarget(chargedMax, DamageType::Magical);
                if (ValidHeroTarget(target, chargedMax)) {
                    // slow buff = more hitchance || target too far and cant be w hit
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
            const float currentRange = Q.CurrentRange();
            const auto target = GetTarget(currentRange, DamageType::Magical);
            if (ValidHeroTarget(target, currentRange)) {
                const auto pred = Q.GetPrediction(target);
                if (HitchanceAtLeast(pred.Hitchance, QHitchance())) {
                    Q.ShootChargedSpell(pred.GetCastPosition());
                }
            }
        }
    }
}

static void AutoR() {
    const auto player = Player();
    if (!player.HasBuff("XerathLocusOfPower2") || Q.IsCharging()) {
        return;
    }

    auto target = GetTarget(R.Range, DamageType::Magical);

    if (Bool(Ulti, "NearMouse") && Slider(Ulti, "MouseZone", 600) > 0) {
        const float zone = static_cast<float>(Slider(Ulti, "MouseZone", 600));
        const Vector3 cursor = Game::CursorPos();
        AIHeroClient nearMouse;
        for (const auto& hero : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(hero, R.Range)) {
                continue;
            }
            if (hero.Position().Distance(cursor) <= zone) {
                nearMouse = hero;
                break;
            }
        }
        target = nearMouse;
    }

    if (ValidHeroTarget(target, R.Range)) {
        if (Key(Ulti, "RKey")) {
            const auto pred = R.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, RHitchance())) {
                R.Cast(pred.GetCastPosition());
            }
        }
    }
}

static void SemiAutomatic() {
    if (Key(Semi, "WKey") && W.IsReady()) {
        const auto target = GetTarget(W.Range, DamageType::Magical);
        if (ValidHeroTarget(target, W.Range)) {
            W.Cast(target);
        }
    }

    if (Key(Semi, "EKey") && E.IsReady()) {
        const auto target = GetTarget(E.Range, DamageType::Magical);
        if (ValidHeroTarget(target, E.Range)) {
            E.Cast(target);
        }
    }
}

// FIX(source): the C# damage helpers used a stale patch table, declared every
// spell as DamageType.Physical on a full-AP champion, and indexed the E table
// with R.Level. All four now come from GameData 16.14.7945912 DataValues +
// mSpellCalculations and go through CalculateMagicDamage.
static double QDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0;
    }

    // XerathArcanopulseChargeUp: BaseDamage + TooltipTotalDamage coefficient 0.9.
    static constexpr float qBase[] = { 35.0f, 75.0f, 115.0f, 155.0f, 195.0f };
    static constexpr float qApRatio = 0.8999999761581421f;
    const int level = std::clamp(Q.Level(), 1, 5);
    const float raw = qBase[level - 1] + qApRatio * player.AP();
    return player.CalculateMagicDamage(target, raw);
}

static double WDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0;
    }

    // XerathArcaneBarrage2: BaseDamage + TotalDamage coefficient 0.65. The centre
    // hit multiplies this by DataValues SweetSpotMultiplier (1.667); the plain
    // value is used here so kill checks stay conservative, as in the source.
    static constexpr float wBase[] = { 15.0f, 50.0f, 85.0f, 120.0f, 155.0f };
    static constexpr float wApRatio = 0.6499999761581421f;
    const int level = std::clamp(W.Level(), 1, 5);
    const float raw = wBase[level - 1] + wApRatio * player.AP();
    return player.CalculateMagicDamage(target, raw);
}

static double EDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0;
    }

    // XerathMageSpear: BaseDamage + TooltipTotalDamage coefficient 0.45.
    static constexpr float eBase[] = { 40.0f, 70.0f, 100.0f, 130.0f, 160.0f };
    static constexpr float eApRatio = 0.44999998807907104f;
    const int level = std::clamp(E.Level(), 1, 5);
    const float raw = eBase[level - 1] + eApRatio * player.AP();
    return player.CalculateMagicDamage(target, raw);
}

// FIX(source): the C# had no R damage helper at all.
static double RDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0;
    }

    // XerathLocusOfPower2: BaseDamage + TooltipTotalDamage coefficient 0.45, per
    // shot. DataValues NumberOfShots is 3/4/5 by rank, so a full ult is this
    // value times the shot count; one shot is returned here.
    static constexpr float rBase[] = { 120.0f, 170.0f, 220.0f };
    static constexpr float rApRatio = 0.44999998807907104f;
    const int level = std::clamp(R.Level(), 1, 3);
    const float raw = rBase[level - 1] + rApRatio * player.AP();
    return player.CalculateMagicDamage(target, raw);
}

static void KillSteal() {
    const auto player = Player();
    if (player.HasBuff("XerathLocusOfPower2") || Q.IsCharging()) {
        return;
    }

    const bool KsQ = Bool(KillStealMenu, "KsQ");
    const bool KsW = Bool(KillStealMenu, "KsW");
    const bool KsE = Bool(KillStealMenu, "KsE");
    for (const auto& target : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(target, W.Range) ||
            target.HasBuff("JudicatorIntervention") ||
            target.HasBuff("kindredrnodeathbuff") ||
            target.HasBuff("Undying Rage")) {
            continue;
        }

        if (KsQ && Q.IsReady()) {
            if (target.IsValid()) {
                if (player.Distance(target) > 150.0f) {
                    if (target.Health() + target.AllShield() <= QDamage(target)) {
                        const float chargedMax = static_cast<float>(Q.ChargedMaxRange);
                        const auto target1 = GetTarget(chargedMax, DamageType::Magical);
                        if (ValidHeroTarget(target1, chargedMax)) {
                            // slow buff = more hitchance || target too far and cant be w hit
                            if (!W.IsReady() || target1.DistanceToPlayer() > 850.0f) {
                                const auto pred = Q.GetPrediction(target1);
                                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                                    Q.StartCharging();
                                }
                            }
                        } else {
                            const float currentRange = Q.CurrentRange();
                            const auto target2 = GetTarget(currentRange, DamageType::Magical);
                            if (ValidHeroTarget(target2, currentRange)) {
                                const auto pred = Q.GetPrediction(target2);
                                if (HitchanceAtLeast(pred.Hitchance, QHitchance())) {
                                    Q.ShootChargedSpell(pred.GetCastPosition());
                                }
                            }
                        }
                    }
                }
            }
        }
        if (KsW && W.IsReady()) {
            if (target.IsValid()) {
                if (target.Health() + target.AllShield() <= WDamage(target)) /*try*/ {
                    W.Cast(target);
                }
            }
        }
        if (KsE && E.IsReady() && ValidHeroTarget(target, 500.0f)) {
            if (target.IsValid()) {
                if (target.Health() + target.AllShield() <= EDamage(target)) {
                    E.Cast();
                }
            }
        }
    }
}

static void Harass() {
    const auto player = Player();
    if (!Q.IsCharging()) {
        // ult + mana check
        if (!player.HasBuff("XerathLocusOfPower2") &&
            player.ManaPercent() >= static_cast<float>(Slider(HarassMenu, "Mana", 50))) {
            if (Bool(HarassMenu, "HarassW") && W.IsReady()) {
                const auto target = GetTarget(W.Range, DamageType::Magical);
                if (ValidHeroTarget(target, W.Range)) {
                    const auto pred = W.GetPrediction(target);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh)) {
                        W.Cast(pred.GetCastPosition());
                        return;
                    }
                }
            }

            if (Bool(HarassMenu, "HarassE") && E.IsReady()) {
                const auto target = GetTarget(E.Range, DamageType::Magical);
                if (ValidHeroTarget(target, E.Range)) {
                    const auto pred = E.GetPrediction(target);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh)) {
                        E.Cast(pred.GetCastPosition());
                        return;
                    }
                }
            }

            if (Bool(HarassMenu, "HarassQ") && Q.IsReady()) {
                const float chargedMax = static_cast<float>(Q.ChargedMaxRange);
                const auto target = GetTarget(chargedMax, DamageType::Magical);
                if (ValidHeroTarget(target, chargedMax)) {
                    // slow buff = more hitchance || target too far and cant be w hit
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
        // ignore mana check when q charge
        if (Bool(HarassMenu, "HarassQ") && Q.IsReady() && Q.IsCharging()) {
            const float currentRange = Q.CurrentRange();
            const auto target = GetTarget(currentRange, DamageType::Magical);
            if (ValidHeroTarget(target, currentRange)) {
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
    if (!Q.IsCharging()) {
        // ult + mana check
        if (!player.HasBuff("XerathLocusOfPower2") &&
            player.ManaPercent() >= static_cast<float>(Slider(LaneClearMenu, "ManaLC", 60))) {
            if (Bool(LaneClearMenu, "LaneW") && W.IsReady()) {
                std::vector<AIBaseClient> minions;
                for (const auto& minion : GameObjects::EnemyMinions()) {
                    if (ValidTarget(minion, W.Range) && minion.IsMinion()) {
                        minions.push_back(minion);
                    }
                }
                if (!minions.empty()) {
                    const auto wFarmLocation = W.GetCircularFarmLocation(minions);
                    if (wFarmLocation.Position.IsValid() &&
                        wFarmLocation.MinionsHit >= Slider(LaneClearMenu, "MinW", 3)) {
                        W.Cast(Vector3::From2D(wFarmLocation.Position));
                        return;
                    }
                }
            }

            if (Bool(LaneClearMenu, "LaneQ") && Q.IsReady()) {
                const float chargedMax = static_cast<float>(Q.ChargedMaxRange);
                std::vector<AIBaseClient> minions;
                for (const auto& minion : GameObjects::EnemyMinions()) {
                    if (ValidTarget(minion, chargedMax) && minion.IsMinion()) {
                        minions.push_back(minion);
                    }
                }
                if (!minions.empty()) {
                    const auto qFarmLocation = Q.GetLineFarmLocation(minions);
                    if (qFarmLocation.Position.IsValid() &&
                        qFarmLocation.MinionsHit >= Slider(LaneClearMenu, "MinQ", 3)) {
                        Q.StartCharging();
                    }
                }
            }
        }
    } else {
        // ignore mana check when q charge
        if (Bool(LaneClearMenu, "LaneQ") && Q.IsReady() && Q.IsCharging()) {
            const float currentRange = Q.CurrentRange();
            std::vector<AIBaseClient> minions;
            for (const auto& minion : GameObjects::EnemyMinions()) {
                if (ValidTarget(minion, currentRange) && minion.IsMinion()) {
                    minions.push_back(minion);
                }
            }
            if (!minions.empty()) {
                const auto qFarmLocation = Q.GetLineFarmLocation(minions);
                if (qFarmLocation.Position.IsValid() &&
                    qFarmLocation.MinionsHit >= Slider(LaneClearMenu, "MinQ", 3)) {
                    Q.ShootChargedSpell(Vector3::From2D(qFarmLocation.Position));
                }
            }
        }
    }
}

static void JungleClear() {
    const auto player = Player();
    if (!Q.IsCharging()) {
        // ult + mana check
        // FIX(source): slider key now resolves (see BuildMenu).
        if (!player.HasBuff("XerathLocusOfPower2") &&
            player.ManaPercent() >= static_cast<float>(Slider(JungleClearMenu, "ManaJG", 40))) {
            if (Bool(JungleClearMenu, "WJungle") && W.IsReady()) {
                AIMinionClient mob;
                float bestHealth = -1.0f;
                for (const auto& candidate : GameObjects::Jungle()) {
                    if (!ValidTarget(candidate, W.Range) ||
                        candidate.GetJungleType() == JungleType::Unknown) {
                        continue;
                    }
                    if (candidate.MaxHealth() > bestHealth) {
                        bestHealth = candidate.MaxHealth();
                        mob = candidate;
                    }
                }

                if (ValidTarget(mob, W.Range)) {
                    const auto pred = W.GetPrediction(mob);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                        W.Cast(pred.GetCastPosition());
                        return;
                    }
                }
            }

            if (Bool(JungleClearMenu, "EJungle") && E.IsReady()) {
                AIMinionClient mob;
                float bestHealth = -1.0f;
                for (const auto& candidate : GameObjects::Jungle()) {
                    if (!ValidTarget(candidate, E.Range) ||
                        candidate.GetJungleType() == JungleType::Unknown) {
                        continue;
                    }
                    if (candidate.MaxHealth() > bestHealth) {
                        bestHealth = candidate.MaxHealth();
                        mob = candidate;
                    }
                }

                if (ValidTarget(mob, E.Range)) {
                    const auto pred = E.GetPrediction(mob);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                        E.Cast(pred.GetCastPosition());
                        return;
                    }
                }
            }

            if (Bool(JungleClearMenu, "QJungle") && Q.IsReady()) {
                const float chargedMax = static_cast<float>(Q.ChargedMaxRange);
                AIMinionClient mob;
                float bestHealth = -1.0f;
                for (const auto& candidate : GameObjects::Jungle()) {
                    if (!ValidTarget(candidate, chargedMax) ||
                        candidate.GetJungleType() == JungleType::Unknown) {
                        continue;
                    }
                    if (candidate.MaxHealth() > bestHealth) {
                        bestHealth = candidate.MaxHealth();
                        mob = candidate;
                    }
                }

                if (ValidTarget(mob, chargedMax)) {
                    Q.StartCharging();
                }
            }
        }
    } else {
        // ignore mana check when q charge
        if (Bool(JungleClearMenu, "QJungle") && Q.IsReady() && Q.IsCharging()) {
            const float chargedMax = static_cast<float>(Q.ChargedMaxRange);
            AIMinionClient mob;
            float bestHealth = -1.0f;
            for (const auto& candidate : GameObjects::Jungle()) {
                if (!ValidTarget(candidate, chargedMax) ||
                    candidate.GetJungleType() == JungleType::Unknown) {
                    continue;
                }
                if (candidate.MaxHealth() > bestHealth) {
                    bestHealth = candidate.MaxHealth();
                    mob = candidate;
                }
            }

            if (ValidTarget(mob, Q.CurrentRange())) {
                const auto pred = Q.GetPrediction(mob);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    Q.ShootChargedSpell(pred.GetCastPosition());
                }
            }
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnGapCloser -= &Gapcloser_OnGapcloser;
    Events::hook.OnInterruptableSpell -= &OnInterrupterSpell;
    Events::hook.OnProcessSpell -= &Obj_AI_Base_OnProcessSpellCast;
    RemoveKeyPermashow(Ulti, "RKey");

    Loaded = false;
}

} // namespace Plugins::AIO7UP::Xerath
