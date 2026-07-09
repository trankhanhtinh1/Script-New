#pragma once

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::AIO7UP::Xerath {

using SDK::Core::Utils::AutoAttack;

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

inline Spell Q{ SpellSlot::Q, 750.0f };
inline Spell W{ SpellSlot::W, 950.0f };
inline Spell E{ SpellSlot::E, 1050.0f };
inline Spell R{ SpellSlot::R, 4990.0f };

inline int WallCastT = 0;
inline Vec2 YasuoWallCastedPos;
inline uintptr_t YasuoWall = 0;
inline bool Loaded = false;
inline DWORD LastUpdateTick = 0;
inline DWORD LastBackgroundTick = 0;

static AIHeroClient Player() { return ObjectManager::Player(); }

static bool Bool(Menu* menu, const char* key, bool fallback = true) {
    if (!menu) return fallback;
    const auto* item = menu->Get<MenuBool>(key);
    return item ? item->Value : fallback;
}

static int Slider(Menu* menu, const char* key, int fallback = 0) {
    if (!menu) return fallback;
    const auto* item = menu->Get<MenuSlider>(key);
    return item ? item->Value : fallback;
}

static bool Key(Menu* menu, const char* key, bool fallback = false) {
    if (!menu) return fallback;
    const auto* item = menu->Get<MenuKeyBind>(key);
    return item ? item->Active : fallback;
}

static bool ShouldRunNow(DWORD& lastTick, DWORD intervalMs) {
    const DWORD now = GetTickCount();
    if (lastTick != 0 && now - lastTick < intervalMs) {
        return false;
    }

    lastTick = now;
    return true;
}

static void RemoveKeyPermashow(Menu* menu, const char* key) {
    if (auto* item = menu ? menu->Get<MenuKeyBind>(key) : nullptr) {
        item->RemovePermashow();
    }
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

static HitChance QHitchance() {
    return Bool(Misc, "qslowcast") ? HitChance::VeryHigh : HitChance::High;
}

static HitChance RHitchance() {
    return Bool(Misc, "rslowcast") ? HitChance::VeryHigh : HitChance::High;
}

// Forward declarations (theo thu tu C#)
static void OnInterrupterSpell(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args);
static void OnProcessSpellCast(const SDK::Events::ProcessSpellEventArgs& args);
static void Gapcloser_OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args);
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void Combo();
static void AutoR();
static void SemiAutomatic();
static double QDamage(const AIBaseClient& target);
static double WDamage(const AIBaseClient& target);
static double EDamage(const AIBaseClient& target);
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
    JungleClearMenu->Add(new MenuSlider("ManaLC", "Min Mana JungleClear [Q]", 40, 0, 100));

    Misc = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    Misc->Add(new MenuBool("qslowcast", "Slow Q Cast(high hitchance)"));
    Misc->Add(new MenuBool("rslowcast", "Slow R Cast(high hitchance)"));
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
    Semi->Add(new MenuKeyBind("WKey", "Semi W Key", SDK::Keys::W, KeyBindType::Press))->Permashow();
    Semi->Add(new MenuKeyBind("EKey", "Semi E Key", SDK::Keys::E, KeyBindType::Press))->Permashow();

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
    if (!player.IsValid()) return;
    if (Loaded) return;

    Q = Spell(SpellSlot::Q, 750.0f);
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
    Events::hook.OnGapCloser += &Gapcloser_OnGapcloser;
    Events::hook.OnInterruptableSpell += &OnInterrupterSpell;
    Events::hook.OnProcessSpell += &OnProcessSpellCast;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>7UP - Xerath loaded</font>");
}

// === OnInterrupterSpell ===
static void OnInterrupterSpell(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) return;
    if (Game::IsChatOpen() || Game::IsShopOpen()) return;

    if (Bool(Misc, "eantigapcloser") && E.IsReady() &&
        static_cast<int>(args.DangerLevel) >= static_cast<int>(SDK::DangerLevel::Medium)) {
        const auto sender = AIHeroClient(args.Sender);
        if (sender.IsValid() && sender.DistanceToPlayer() < E.Range) {
            const auto pred = E.GetPrediction(sender);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                E.Cast(pred.GetCastPosition());
            }
        }
    }
}

// === OnProcessSpellCast ===
static void OnProcessSpellCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (Events::IsLocalPlayer(args.Sender)) {
        const std::string name(args.SpellName);
        if (name == "SyndraQ")
            Q.LastCastAttemptT = static_cast<int>(GetTickCount());
        if (name == "SyndraW" || name == "syndrawcast")
            W.LastCastAttemptT = static_cast<int>(GetTickCount());
        if (name == "SyndraE" || name == "syndrae5")
            E.LastCastAttemptT = static_cast<int>(GetTickCount());
    }

    if (args.Sender.IsValid() &&
        args.Sender.Team == static_cast<uint32_t>(Player().Team()) &&
        std::string(args.SpellName) == "YasuoWMovingWall") {
        WallCastT = static_cast<int>(GetTickCount());
        YasuoWallCastedPos = Vec2(args.Sender.Position.x, args.Sender.Position.z);
    }
}

// === Gapcloser_OnGapcloser ===
static void Gapcloser_OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) return;
    if (Game::IsChatOpen() || Game::IsShopOpen()) return;

    if (Bool(Misc, "eantigapcloser") && E.IsReady()) {
        const auto sender = AIHeroClient(args.Sender);
        if (sender.IsValid() && args.End.Distance2D(player.Position()) < 250.0f) {
            const auto pred = E.GetPrediction(sender);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                E.Cast(pred.GetCastPosition());
            }
        }
    }
}

// === Game_OnUpdate ===
static void Game_OnUpdate(const GameUpdateEventArgs&) {
    if (!ShouldRunNow(LastUpdateTick, 40)) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) return;
    if (Game::IsChatOpen() || Game::IsShopOpen()) return;
    if (player.Spellbook().IsWindingUp()) return;

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

    if (ShouldRunNow(LastBackgroundTick, 90)) {
        KillSteal();
        AutoR();
    }
    SemiAutomatic();
}

// === Combo ===
static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) return;

    if (!Q.IsCharging()) {
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
                const auto target = GetTarget(static_cast<float>(Q.ChargedMaxRange), DamageType::Magical);
                if (ValidHeroTarget(target, static_cast<float>(Q.ChargedMaxRange))) {
                    if (!W.IsReady() || target.DistanceToPlayer() > 850.0f) {
                        const auto pred = Q.GetPrediction(target);
                        if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                            Q.StartCharging(pred.GetCastPosition());
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

// === AutoR ===
static void AutoR() {
    const auto player = Player();
    if (!player.IsValid()) return;

    if (!player.HasBuff("XerathLocusOfPower2") || Q.IsCharging()) return;

    AIHeroClient target;

    if (Bool(Ulti, "NearMouse") && Slider(Ulti, "MouseZone", 600) > 0) {
        const float mouseZone = static_cast<float>(Slider(Ulti, "MouseZone", 600));
        auto* selector = SDK::TargetSelector::Instance();
        if (selector) {
            const auto targets = selector->GetTargets(R.Range, DamageType::Magical);
            for (const auto& t : targets) {
                if (t.Position().Distance(Game::CursorPos()) <= mouseZone) {
                    target = t;
                    break;
                }
            }
        }
    } else {
        target = GetTarget(R.Range, DamageType::Magical);
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

// === SemiAutomatic ===
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

// === QDamage ===
// C# uses DamageType.Physical (original bug kept 1-1)
static double QDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0;
    static constexpr float qBase[] = { 0.0f, 70.0f, 110.0f, 150.0f, 190.0f, 230.0f };
    const int level = std::clamp(Q.Level(), 1, 5);
    const float raw = qBase[level] + 0.848f * player.AP();
    return player.CalculatePhysicalDamage(target, raw);
}

// === WDamage ===
static double WDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0;
    static constexpr float wBase[] = { 0.0f, 60.0f, 95.0f, 130.0f, 165.0f, 200.0f };
    const int level = std::clamp(W.Level(), 1, 5);
    const float raw = wBase[level] + 0.58f * player.AP();
    return player.CalculatePhysicalDamage(target, raw);
}

// === EDamage ===
// C# bug: uses R.Level instead of E.Level — kept 1-1
static double EDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0;
    static constexpr float eBase[] = { 0.0f, 80.0f, 110.0f, 140.0f, 170.0f, 200.0f };
    const int level = std::clamp(R.Level(), 1, 5);
    const float raw = eBase[level] + 0.448f * player.AP();
    return player.CalculatePhysicalDamage(target, raw);
}

// === KillSteal ===
static void KillSteal() {
    const auto player = Player();
    if (!player.IsValid()) return;
    if (player.HasBuff("XerathLocusOfPower2") || Q.IsCharging()) return;

    const bool ksQ = Bool(KillStealMenu, "KsQ") && Q.IsReady();
    const bool ksW = Bool(KillStealMenu, "KsW") && W.IsReady();
    const bool ksE = Bool(KillStealMenu, "KsE") && E.IsReady();
    if (!ksQ && !ksW && !ksE) {
        return;
    }

    for (const auto& target : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(target)) continue;
        if (target.HasBuff("JudicatorIntervention") || target.HasBuff("FioraW") ||
            target.HasBuff("kindredrnodeathbuff") || target.HasBuff("UndyingRage") ||
            target.HasBuff("ChronoShift")) continue;

        if (ksQ &&
            ValidHeroTarget(target, static_cast<float>(Q.ChargedMaxRange))) {
            if (target.Health() + target.AllShield() <= QDamage(target)) {
                const auto pred = Q.GetPrediction(target);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    Q.StartCharging(pred.GetCastPosition());
                }
            }
        }

        if (ksW && ValidHeroTarget(target, W.Range)) {
            if (target.Health() + target.AllShield() <= WDamage(target)) {
                const auto pred = W.GetPrediction(target);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    W.Cast(pred.GetCastPosition());
                }
            }
        }

        if (ksE && ValidHeroTarget(target, 500.0f)) {
            if (target.Health() + target.AllShield() <= EDamage(target)) {
                E.Cast();
            }
        }
    }
}

// === Harass ===
static void Harass() {
    const auto player = Player();
    if (!player.IsValid()) return;
    if (player.ManaPercent() < static_cast<float>(Slider(HarassMenu, "Mana", 50))) return;

    if (Bool(HarassMenu, "HarassQ") && Q.IsReady() && !Q.IsCharging()) {
        const auto target = GetTarget(static_cast<float>(Q.ChargedMaxRange), DamageType::Magical);
        if (ValidHeroTarget(target, static_cast<float>(Q.ChargedMaxRange))) {
            const auto pred = Q.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                Q.StartCharging(pred.GetCastPosition());
            }
        }
    }

    if (Q.IsCharging()) {
        const auto target = GetTarget(Q.Range, DamageType::Magical);
        if (ValidHeroTarget(target, Q.Range)) {
            const auto pred = Q.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, QHitchance())) {
                Q.ShootChargedSpell(pred.GetCastPosition());
            }
        }
    }

    if (Bool(HarassMenu, "HarassW") && W.IsReady()) {
        const auto target = GetTarget(W.Range, DamageType::Magical);
        if (ValidHeroTarget(target, W.Range)) {
            const auto pred = W.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                W.Cast(pred.GetCastPosition());
            }
        }
    }

    if (Bool(HarassMenu, "HarassE") && E.IsReady()) {
        const auto target = GetTarget(E.Range, DamageType::Magical);
        if (ValidHeroTarget(target, E.Range)) {
            const auto pred = E.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                E.Cast(pred.GetCastPosition());
            }
        }
    }
}

// === LaneClear ===
static void LaneClear() {
    const auto player = Player();
    if (!player.IsValid()) return;
    if (player.ManaPercent() < static_cast<float>(Slider(LaneClearMenu, "ManaLC", 60))) return;

    if (Bool(LaneClearMenu, "LaneW") && W.IsReady()) {
        std::vector<AIBaseClient> minions;
        for (const auto& m : GameObjects::EnemyMinions()) {
            if (ValidTarget(m, W.Range) && m.IsMinion()) {
                minions.emplace_back(m.Handle());
            }
        }
        if (!minions.empty()) {
            const auto wFarmLocation = W.GetCircularFarmLocation(minions);
            if (wFarmLocation.MinionsHit >= Slider(LaneClearMenu, "MinW", 3)) {
                W.Cast(Vector3(wFarmLocation.Position.x, 0.0f, wFarmLocation.Position.y));
            }
        }
    }

    if (Bool(LaneClearMenu, "LaneQ") && Q.IsReady() && !Q.IsCharging()) {
        std::vector<AIBaseClient> minions;
        for (const auto& m : GameObjects::EnemyMinions()) {
            if (ValidTarget(m, static_cast<float>(Q.ChargedMaxRange)) && m.IsMinion()) {
                minions.emplace_back(m.Handle());
            }
        }
        if (!minions.empty()) {
            const auto qFarmLocation = Q.GetLineFarmLocation(minions);
            if (qFarmLocation.MinionsHit >= Slider(LaneClearMenu, "MinQ", 3)) {
                Q.StartCharging(Vector3(qFarmLocation.Position.x, 0.0f, qFarmLocation.Position.y));
            }
        }
    }

    if (Q.IsCharging()) {
        std::vector<AIBaseClient> minions;
        for (const auto& m : GameObjects::EnemyMinions()) {
            if (ValidTarget(m, Q.Range) && m.IsMinion()) {
                minions.emplace_back(m.Handle());
            }
        }
        if (!minions.empty()) {
            const auto qFarmLocation = Q.GetLineFarmLocation(minions);
            if (qFarmLocation.MinionsHit >= Slider(LaneClearMenu, "MinQ", 3)) {
                Q.ShootChargedSpell(Vector3(qFarmLocation.Position.x, 0.0f, qFarmLocation.Position.y));
            }
        }
    }
}

// === JungleClear ===
static void JungleClear() {
    const auto player = Player();
    if (!player.IsValid()) return;

    AIMinionClient bestMob;
    float bestMaxHealth = 0.0f;
    for (const auto& m : GameObjects::Jungle()) {
        if (!ValidTarget(m, W.Range)) continue;
        if (m.GetJungleType() == JungleType::Unknown) continue;
        if (m.MaxHealth() > bestMaxHealth) {
            bestMaxHealth = m.MaxHealth();
            bestMob = m;
        }
    }

    if (!bestMob.IsValid()) return;

    if (Bool(JungleClearMenu, "QJungle") && Q.IsReady() && !Q.IsCharging() &&
        player.ManaPercent() >= static_cast<float>(Slider(JungleClearMenu, "ManaLC", 40))) {
        Q.StartCharging(bestMob.Position());
    }

    if (Q.IsCharging() && Bool(JungleClearMenu, "QJungle")) {
        Q.ShootChargedSpell(bestMob.Position());
    }

    if (Bool(JungleClearMenu, "WJungle") && W.IsReady()) {
        W.Cast(bestMob.Position());
    }

    if (Bool(JungleClearMenu, "EJungle") && E.IsReady()) {
        E.Cast(bestMob.Position());
    }
}

// === OnUnload ===
static void OnUnload() {
    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnGapCloser -= &Gapcloser_OnGapcloser;
    Events::hook.OnInterruptableSpell -= &OnInterrupterSpell;
    Events::hook.OnProcessSpell -= &OnProcessSpellCast;
    RemoveKeyPermashow(Ulti, "RKey");
    RemoveKeyPermashow(Semi, "WKey");
    RemoveKeyPermashow(Semi, "EKey");
    Loaded = false;
}

} // namespace Plugins::AIO7UP::Xerath
