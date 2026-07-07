#pragma once

// ============================================================================
// 7UPAIO — Kalista (full rewrite, 1-1 port of 7UPAIO/Champion/Kalista.cs)
// ----------------------------------------------------------------------------
// Logic mirrors Kalista.cs one-to-one. Intentional deviations (see the list at
// the bottom of this file) are limited to:
//   D1. Clear() Q-on-jungle now models Ezreal.h::JungleClear (priority sort +
//       visible/plant/pet filter + prediction) instead of the raw Large/Legendary
//       loop in the .cs.
//   D2. Combo() orbwalk-minion no longer requires an enemy hero nearby: while
//       holding the combo key it force-attacks the nearest minion / jungle
//       monster / plant to the cursor.
//   D3. Clear() E-on-jungle iterates ALL jungle monsters and casts E when
//       killable, using a *0.5 damage threshold for Epic/Legendary (detected via
//       IsEpicJungleMob, same test as Ezreal.h::JungleClear W).
//   D4. Combo() E keeps the .cs "harassPlus" behaviour: if a rend-killable marked
//       minion exists while the marked hero target is in E range, cast E so the
//       stacks are not wasted and the enemy still eats damage + slow.
//   D5. RendCache::Update() is pumped from Game_OnUpdate (the EnsoulSharp SDK
//       maintains this cache internally; the port must refresh it manually).
//   D6. The .cs Game_OnUpdate IssueOrder replay is rate-limited and sends at
//       most one order per tick; the raw C# loop can disconnect NightSharp when
//       holding Space/V/C/X.
// SDK usage follows the working Ezreal.h reference to avoid mismatched APIs.
// ============================================================================

#include "Common.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace Plugins::AIO7UP::Kalista {

using namespace Common;

static float GetEDamage(const AIBaseClient& target);

// ============================================================================
// Local helpers (jungle Epic/Legendary detection — mirror of Ezreal.h)
// ============================================================================
static bool ContainsInsensitive(const std::string& value, const char* needle) {
    if (value.empty() || !needle || !needle[0]) {
        return false;
    }
    std::string lower = value;
    std::string search = needle;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    std::transform(search.begin(), search.end(), search.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lower.find(search) != std::string::npos;
}

// Same Epic/Legendary test Ezreal.h::JungleClear uses for its W cast.
static bool IsEpicJungleMob(const AIMinionClient& minion) {
    const JungleType type = minion.GetJungleType();
    if (type == JungleType::Legendary || type == JungleType::Epic) {
        return true;
    }
    const std::string name = minion.CharacterName();
    return ContainsInsensitive(name, "dragon") ||
           ContainsInsensitive(name, "baron") ||
           ContainsInsensitive(name, "riftherald") ||
           ContainsInsensitive(name, "voidgrub") ||
           ContainsInsensitive(name, "atakhan") ||
           ContainsInsensitive(name, "sentinel");
}

static int JunglePriority(const AIMinionClient& minion) {
    const JungleType type = minion.GetJungleType();
    if (type == JungleType::Legendary) return 5000;
    if (type == JungleType::Epic) return 4000;
    if (IsEpicJungleMob(minion)) return 4000;
    if (type == JungleType::Large) return 3000;
    if (type == JungleType::Small) return 1000;
    return 0;
}

// ============================================================================
// RendCache — port of EnsoulSharp RendCache helper for Kalista E
// ============================================================================
namespace RendCache {

inline std::vector<AIBaseClient> RendMinions;

inline void Update() {
    RendMinions.clear();
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (minion.IsValid() && minion.HasBuff("kalistaexpungemarker")) {
            RendMinions.emplace_back(minion.Handle());
        }
    }
    for (const auto& hero : GameObjects::EnemyHeroes()) {
        if (hero.IsValid() && hero.HasBuff("kalistaexpungemarker")) {
            RendMinions.emplace_back(hero.Handle());
        }
    }
    for (const auto& mob : GameObjects::Jungle()) {
        if (mob.IsValid() && mob.HasBuff("kalistaexpungemarker")) {
            RendMinions.emplace_back(mob.Handle());
        }
    }
}

inline bool IsUnitRendKillable(const AIBaseClient& unit) {
    if (!unit.IsValid() || !unit.HasBuff("kalistaexpungemarker")) {
        return false;
    }
    return GetEDamage(unit) > unit.Health();
}

} // namespace RendCache

// ============================================================================
// Globals
// ============================================================================
inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* Eset = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* HarrassMenu = nullptr;
inline Menu* Rset = nullptr;
inline Menu* Misc = nullptr;
inline Menu* KsMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 1200.0f };
inline Spell NonCollisionQ{ SpellSlot::Q, 1140.0f };
inline Spell W{ SpellSlot::W, 5000.0f };
inline Spell E{ SpellSlot::E, 1000.0f };
inline Spell R{ SpellSlot::R, 1200.0f };
inline Spell Ignite{ SpellSlot::Unknown, 600.0f };

inline bool Loaded = false;
inline int LastForcusTime = 0;
inline int lastETime = 0;
inline int LastAATick = 0;
inline int LastManualMoveOrderTick = 0;
inline int LastManualAttackOrderTick = 0;
inline int LastAutoWCastTick = 0;

// EnsoulSharp EDamage arrays (Kalista.cs).
static constexpr float EBaseDamage[] = { 0.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f };
static constexpr float EStackBaseDamage[] = { 0.0f, 10.0f, 14.0f, 19.0f, 25.0f, 32.0f };
static constexpr float EStackMultiplierDamage[] = { 0.0f, 0.198f, 0.23748f, 0.27498f, 0.31248f, 0.34988f };
static constexpr int kOracleLensItemId = 3364;
static constexpr int kControlWardItemId = 2055;

// Incoming damage tracking for soulbound ally (Kalista.cs).
inline std::map<float, float> IncomingDamageToSoulboundAlly;
inline std::map<float, float> InstantDamageOnSoulboundAlly;

static float AllIncomingDamageToSoulbound() {
    float total = 0.0f;
    for (const auto& entry : IncomingDamageToSoulboundAlly) total += entry.second;
    for (const auto& entry : InstantDamageOnSoulboundAlly) total += entry.second;
    return total;
}

// ============================================================================
// Forward declarations
// ============================================================================
static void BuildMenu();
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void OnAfterAttack(OrbwalkingActionArgs& args);
static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args);
static void OnEndScene();
static void OnNonKillableMinion(OrbwalkingActionArgs& args);
static void OnPlayAnimation(const Events::PlayAnimationEventArgs& args);
static void OnUnload();

static void Combo();
static void Harass();
static void Clear();
static void Routine();
static void Killsteal();
static void LogicE();
static void RLogic();
static void FlyHack();
static void ReplayOrbwalkerOrders();

static float GetEDamage(const AIBaseClient& target);
static float QDamageCalc(const AIBaseClient& target);

static bool HasHardCcOrSlow(const AIBaseClient& target) {
    return SDK::HasBuffOfType(target, SDK::BuffType::Asleep) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Charm) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Fear) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Knockup) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Slow) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Stun);
}

// target.IsUnKillable() (Kalista.cs extension).
static bool IsUnKillable(const AIBaseClient& target) {
    if (!target.IsValid() || target.IsDead() || target.Health() <= 0.0f) {
        return true;
    }
    if (target.HasBuff("KindredRNoDeathBuff")) return true;

    const float gameTime = CoreBuffs::ResolveGameTime();
    const auto buffEndTime = [&target](const char* name) {
        const auto buff = CoreBuffs::FindByName(target.Address(), name);
        return buff.IsValid() ? buff.GetEndTime() : 0.0f;
    };

    if (target.HasBuff("UndyingRage") &&
        buffEndTime("UndyingRage") - gameTime > 0.3f &&
        target.Health() <= target.MaxHealth() * 0.10f) {
        return true;
    }
    if (target.HasBuff("JudicatorIntervention")) return true;
    if (target.HasBuff("ChronoShift") &&
        buffEndTime("ChronoShift") - gameTime > 0.3f &&
        target.Health() <= target.MaxHealth() * 0.10f) {
        return true;
    }
    if (target.HasBuff("VladimirSanguinePool")) return true;
    if (target.HasBuff("ShroudofDarkness")) return true;
    if (target.HasBuff("SivirShield")) return true;
    if (target.HasBuff("itemmagekillerveil")) return true;
    return target.HasBuff("FioraW");
}

// ============================================================================
// Damage calculations (Kalista.cs)
// ============================================================================
static float GetEDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    const int eLevel = E.Level();
    if (eLevel < 0 || eLevel > 5) return 0.0f;

    const float eBaseDamage = EBaseDamage[eLevel] + 0.6f * player.TotalAttackDamage();
    const float eStackDamage = EStackBaseDamage[eLevel] +
                               EStackMultiplierDamage[eLevel] * player.TotalAttackDamage();
    const int eStacksOnTarget = target.GetBuffCount("kalistaexpungemarker");
    if (eStacksOnTarget == 0) {
        return 0.0f;
    }

    float total = eBaseDamage + eStackDamage * static_cast<float>(eStacksOnTarget - 1);
    if (target.Type() == ::Core::Objects::ObjectType::AIMinionClient) {
        const AIMinionClient minion(target.Handle());
        if ((minion.GetJungleType() & JungleType::Legendary) != JungleType::Unknown) {
            total /= 2.0f;
        }
    }
    return player.CalculatePhysicalDamage(target, total);
}

static float QDamageCalc(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const int qLevel = Q.Level();
    static const float baseDamage[] = { 0, 20, 85, 150, 215, 280, 280 };
    if (qLevel < 0 || qLevel > 6) return 0.0f;
    const float qBaseDamage = baseDamage[qLevel] + 1.0f * player.TotalAttackDamage();
    return player.CalculatePhysicalDamage(target, qBaseDamage);
}

// E.GetKalistaRealDamage(x, tolerEnabled, tolerValue) (Kalista.cs extension).
static float GetKalistaRealDamage(const AIBaseClient& target, bool useToler, float tolerValue) {
    float damage = GetEDamage(target);
    if (useToler) damage += tolerValue;
    if (target.HasBuff("FerociousHowl")) damage *= 0.7f;
    if (Player().HasBuff("summonerexhaust")) damage *= 0.4f;
    return damage;
}

// ============================================================================
// Menu (Kalista.cs BuildMenu — 1-1)
// ============================================================================
static void BuildMenu() {
    MenuRoot = new Menu("champion.7upaio", "7UP - Kalista", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo"));
    ComboMenu->Add(new MenuKeyBind("FlyHack", "Fly Hack", SDK::Keys::T, KeyBindType::Toggle));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("disQ", "Block on high aa speed"));
    ComboMenu->Add(new MenuBool("useE", "Use E"));
    ComboMenu->Add(new MenuBool("disE1", "Block on Debuff"));
    ComboMenu->Add(new MenuBool("disE2", "Limit usage"));
    ComboMenu->Add(new MenuBool("orbminion", "Orbwalker Minion"));

    Eset = MenuRoot->AddSubMenu(new Menu("Eset", "Eset"));
    {
        const char* eModeOptions[] = { "Only Combo", "Always", "Disable" };
        Eset->Add(new MenuList("EMode", "Use E Mode", eModeOptions, 3, 1));
    }
    Eset->Add(new MenuBool("harassPlus", "Auto Kill Minion && Any Enemy Have E Buff"));

    Rset = MenuRoot->AddSubMenu(new Menu("Rset", "Rset"));
    Menu* WowCombo = Rset->AddSubMenu(new Menu("WowCombo", "WowCombo"));
    WowCombo->Add(new MenuBool("Balista", "Balista"));
    WowCombo->Add(new MenuBool("Salista", "Salista"));
    WowCombo->Add(new MenuBool("Talista", "Talista"));
    Rset->Add(new MenuBool("kaliusersaveally", "Use R to save Soulbound"));
    Rset->Add(new MenuBool("userengage", "Use R to engage"));

    HarrassMenu = MenuRoot->AddSubMenu(new Menu("Harass", "Harass"));
    HarrassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarrassMenu->Add(new MenuBool("QMinion", "Use Q on Minion"));
    HarrassMenu->Add(new MenuBool("useE", "Use E"));
    HarrassMenu->Add(new MenuBool("disE1", "Block on Debuff"));
    HarrassMenu->Add(new MenuBool("disE2", "Limit usage"));
    HarrassMenu->Add(new MenuSlider("Mana", "Mana", 50, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q for jungle"));
    LaneClearMenu->Add(new MenuBool("useE", "Use E"));
    LaneClearMenu->Add(new MenuSlider("MinE", "Use Kill min minion Count", 2, 1, 5));
    LaneClearMenu->Add(new MenuSlider("Mana", "Don't Lane/Jung if Mana <= X%", 40, 0, 100));

    Misc = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    Misc->Add(new MenuBool("misc-prevent-e", "Prevent E on Spellshields & Invulnerable"));
    Misc->Add(new MenuBool("misc-dying-e", "E before dying"));
    Misc->Add(new MenuSlider("misc-dying-e-pro", "E dying on %", 10, 1, 50));
    Misc->Add(new MenuBool("misc-leaving-e", "E when leaving range"));
    Misc->Add(new MenuSlider("misc-leaving-e-pro", "E leaving stacks", 5, 1, 10));
    Misc->Add(new MenuKeyBind("misc-ward-trick", "Auto W", SDK::Keys::G, KeyBindType::Toggle, true));
    Misc->Add(new MenuBool("Forcus", "Forcus Attack"));
    Misc->Add(new MenuSliderButton("EToler", "Enabled E Toler DMG", 0, -100, 110, true));

    KsMenu = MenuRoot->AddSubMenu(new Menu("KS Settings", "KS"));
    KsMenu->Add(new MenuBool("KSQ", "Use Q KS"));
    KsMenu->Add(new MenuBool("KSE", "Use E KS"));
    KsMenu->Add(new MenuBool("KSEJG", "Use E KS JG"));

    MenuRoot->Attach();
}

// ============================================================================
// OnBeforeAttack — Forcus (Kalista.cs)
// ============================================================================
static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    const auto player = Player();
    if (!Bool(Misc, "Forcus") || !player.IsValid() || !args.Target.IsValid()) {
        return;
    }
    if (!SDK::CanMove(player)) {
        return;
    }
    const AIBaseClient targetBase(args.Target.Handle());
    if (targetBase.IsDead() || targetBase.Health() <= 0.0f) {
        return;
    }

    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo ||
        Orbwalker::ActiveMode() == OrbwalkingMode::Harass) {
        for (const auto& target : GameObjects::EnemyHeroes()) {
            if (target.IsDead() || !target.IsValid()) continue;
            const float aaRange = AutoAttack::GetRealAutoAttackRange(target);
            if (AutoAttack::InAutoAttackRange(target) &&
                target.HasBuff("kalistacoopstrikemarkally")) {
                if (!target.IsDead() && ValidHeroTarget(target, aaRange)) {
                    Orbwalker::ForceTarget(target);
                    LastForcusTime = SDK::Variables::TickCount();
                }
            }
        }
    } else if (Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear) {
        for (const auto& target : GameObjects::EnemyMinions()) {
            if (target.IsDead() || !target.IsValid()) continue;
            const float aaRange = AutoAttack::GetRealAutoAttackRange(target);
            if (AutoAttack::InAutoAttackRange(target) &&
                target.HasBuff("kalistacoopstrikemarkally")) {
                if (!target.IsDead() && ValidTarget(target, aaRange)) {
                    Orbwalker::ForceTarget(target);
                    LastForcusTime = SDK::Variables::TickCount();
                }
            }
        }
    }
}

// ============================================================================
// OnAfterAttack — Q on hero (combo/harass) + Q on jungle (Kalista.cs)
// ============================================================================
static void OnAfterAttack(OrbwalkingActionArgs& args) {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    const bool comboQ = Bool(ComboMenu, "useQ");
    const bool harassQ = Bool(HarrassMenu, "useQ");
    const bool jungleQ = Bool(LaneClearMenu, "useQ");
    const int manaSlider = Slider(HarrassMenu, "Mana", 50);

    Orbwalker::ForceTarget(AttackableUnit());

    const AIBaseClient targetBase(args.Target.Handle());
    if (targetBase.IsDead() || targetBase.Health() <= 0.0f || player.IsDead() || !Q.IsReady()) {
        return;
    }

    if (targetBase.IsHero()) {
        const auto target = AIHeroClient(targetBase.Handle());
        if (!target.IsValid() || target.IsDead() || !ValidHeroTarget(target, Q.Range)) {
            return;
        }
        if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo) {
            if (comboQ) {
                const auto qPred = Q.GetPrediction(target);
                if (HitchanceAtLeast(qPred.Hitchance, HitChance::High)) {
                    CastPosition(Q, qPred.GetCastPosition(), target);
                }
            }
        } else if (Orbwalker::ActiveMode() == OrbwalkingMode::Harass ||
                   (Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear &&
                    player.ManaPercent() < static_cast<float>(manaSlider))) {
            if (harassQ) {
                const auto qPred = Q.GetPrediction(target);
                if (HitchanceAtLeast(qPred.Hitchance, HitChance::High)) {
                    CastPosition(Q, qPred.GetCastPosition(), target);
                }
            }
        }
    } else if (targetBase.Type() == ::Core::Objects::ObjectType::AIMinionClient) {
        if (Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear &&
            player.ManaPercent() < static_cast<float>(manaSlider)) {
            const auto mob = AIMinionClient(targetBase.Handle());
            if (mob.IsValid() && ValidTarget(mob, Q.Range) &&
                mob.GetJungleType() != JungleType::Unknown) {
                if (jungleQ) {
                    CastPosition(Q, mob.Position(), mob);
                }
            }
        }
    }
}

// ============================================================================
// OnProcessSpellCast — anti-stealth + AA-reset (Kalista.cs)
// ============================================================================
static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args) {
    const auto player = Player();

    if (ContainsInsensitive(std::string(args.SpellName ? args.SpellName : ""), "talonshadow")) {
        if (SDK::Items::HasItem(player, kOracleLensItemId) &&
            SDK::Items::CanUseItem(player, kOracleLensItemId)) {
            SDK::Items::UseItem(player, kOracleLensItemId);
        } else if (SDK::Items::HasItem(player, kControlWardItemId)) {
            SDK::Items::UseItem(player, kControlWardItemId);
        }
    }

    if (Events::IsLocalPlayer(args.Sender)) {
        SDK::Core::Utils::DelayAction::Add(100, []() {
            Orbwalker::ResetAutoAttackTimer();
        });
    }
}

// ============================================================================
// Manual order replay for Kalista passive orbwalking
// ============================================================================
static void ReplayOrbwalkerOrders() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    const OrbwalkingMode mode = Orbwalker::ActiveMode();
    if (mode == OrbwalkingMode::None || mode == OrbwalkingMode::Flee) {
        return;
    }

    const int now = SDK::Variables::TickCount();
    constexpr int kMoveThrottleMs = 90;
    constexpr int kAttackThrottleMs = 140;

    const auto issueMove = [&]() {
        if (now - LastManualMoveOrderTick >= kMoveThrottleMs) {
            if (SDK::IssueOrder(player, SDK::GameObjectOrder::MoveTo, Game::CursorPos())) {
                LastManualMoveOrderTick = now;
            }
        }
    };

    const auto orbTarget = Orbwalker::GetTarget();
    if (!orbTarget.IsValid()) {
        issueMove();
        return;
    }

    const AIBaseClient target(orbTarget.Handle());
    if (!target.IsValid() || target.IsDead() || target.Health() <= 0.0f) {
        issueMove();
        return;
    }

    const int lastAaTick = Orbwalker::LastAutoAttackTick();
    const int attackReadyTick = lastAaTick +
        static_cast<int>(SDK::AttackDelay(player) * 1000.0f) - 180;

    if (now >= attackReadyTick &&
        now - LastManualAttackOrderTick >= kAttackThrottleMs) {
        if (SDK::IssueOrder(player, SDK::GameObjectOrder::AttackUnit, target)) {
            LastManualAttackOrderTick = now;
        }
        return;
    }

    if (now >= lastAaTick + 1) {
        issueMove();
    }
}

// ============================================================================
// Game_OnUpdate
// ============================================================================
static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling() ||
        Game::IsChatOpen() || player.Spellbook().IsWindingUp()) {
        return;
    }

    ReplayOrbwalkerOrders();
    RendCache::Update();   // D5: port maintains the rend cache manually.

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        Harass();
        break;
    case OrbwalkingMode::LaneClear:
        Clear();
        break;
    default:
        break;
    }

    Routine();
    Killsteal();
    FlyHack();
    LogicE();
    RLogic();
}

// ============================================================================
// Combo (Kalista.cs)
// ============================================================================
static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    const bool useQ = Bool(ComboMenu, "useQ");
    const bool disQ = Bool(ComboMenu, "disQ");
    const bool useE = Bool(ComboMenu, "useE");
    const bool disE1 = Bool(ComboMenu, "disE1");
    const bool disE2 = Bool(ComboMenu, "disE2");
    const bool harassPlus = Bool(Eset, "harassPlus");

    const auto target = GetTarget(Q.Range, DamageType::Physical);

    if (ValidHeroTarget(target, Q.Range)) {
        if (useQ && Q.IsReady()) {
            if (disQ) {
                if (SDK::AttackSpeed(player) < 1.98f) {
                    const auto qPred = Q.GetPrediction(target, false, 0);
                    if (HitchanceAtLeast(qPred.Hitchance, HitChance::High)) {
                        CastPosition(Q, qPred.GetUnitPosition(), target);
                    }
                }
            } else {
                const auto qPred = Q.GetPrediction(target, false, 0);
                if (HitchanceAtLeast(qPred.Hitchance, HitChance::High)) {
                    CastPosition(Q, qPred.GetUnitPosition(), target);
                }
            }
        }

        if (useE && E.IsReady()) {
            if (ValidHeroTarget(target, E.Range) &&
                SDK::Variables::TickCount() - lastETime > 500 + Game::Ping()) {
                // Kill check.
                if (target.Health() < GetEDamage(target) && !IsUnKillable(target)) {
                    E.Cast();
                }

                // D4: harassPlus — a rend-killable marked minion + marked hero in
                // E range => cast E so the stacks are spent on a guaranteed minion
                // kill while the enemy still takes damage + slow.
                if (harassPlus &&
                    target.DistanceToPlayer() > player.AttackRange() + player.BoundingRadius() + 100.0f &&
                    ValidHeroTarget(target, E.Range)) {
                    AIMinionClient eKillMinion;
                    for (const auto& minion : GameObjects::EnemyMinions()) {
                        if (!ValidTarget(minion, player.AttackRange() + player.BoundingRadius() + minion.BoundingRadius())) {
                            continue;
                        }
                        if (!minion.HasBuff("kalistaexpungemarker")) continue;
                        if (minion.DistanceToPlayer() > E.Range) continue;
                        if (minion.Health() < GetEDamage(minion)) {
                            eKillMinion = minion;
                            break;
                        }
                    }
                    if (eKillMinion.IsValid() && ValidTarget(eKillMinion, E.Range) &&
                        ValidHeroTarget(target, E.Range)) {
                        E.Cast();
                    }
                }
            }

            // disE1 / disE2 gate (Kalista.cs).
            for (const auto& t : GameObjects::EnemyHeroes()) {
                if (!ValidHeroTarget(t, E.Range) || !t.HasBuff("kalistaexpungemarker")) {
                    continue;
                }
                if (disE1) {
                    if (HasHardCcOrSlow(t)) continue;

                    bool minionKillable = false;
                    for (const auto& m : GameObjects::EnemyMinions()) {
                        if (ValidTarget(m, E.Range) && m.HasBuff("kalistaexpungemarker") &&
                            m.Health() <= GetEDamage(m)) {
                            minionKillable = true;
                            break;
                        }
                    }
                    if (minionKillable) {
                        if (disE2) {
                            if (SDK::Variables::TickCount() - E.LastCastAttemptT > 2500) {
                                E.Cast();
                            }
                        } else {
                            E.Cast();
                        }
                    }
                }
            }
        }
    }

    // D2: Orbwalker-minion — while holding the combo key keep auto-attacking the
    // nearest minion / jungle monster / plant to the cursor. Unlike Kalista.cs
    // this does NOT require an enemy hero to be near.
    if (Bool(ComboMenu, "orbminion")) {
        AIMinionClient attackUnit;
        float bestDist = FLT_MAX;
        const auto consider = [&](const AIMinionClient& unit) {
            if (!ValidTarget(unit) || unit.IsDead()) return;
            if (!AutoAttack::InAutoAttackRange(unit)) return;
            const float dist = unit.Position().Distance2D(Game::CursorPos());
            if (dist < bestDist) {
                attackUnit = unit;
                bestDist = dist;
            }
        };
        for (const auto& minion : GameObjects::EnemyMinions()) consider(minion);
        for (const auto& mob : GameObjects::Jungle()) consider(mob);
        for (const auto& plant : GameObjects::Plants()) consider(plant);

        if (attackUnit.IsValid() && !attackUnit.IsDead()) {
            Orbwalker::ForceTarget(attackUnit);
        } else {
            Orbwalker::ForceTarget(AttackableUnit());
        }
    }
}

// ============================================================================
// Harass (Kalista.cs)
// ============================================================================
static void Harass() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    const int mana = Slider(HarrassMenu, "Mana", 50);
    const bool useQ = Bool(HarrassMenu, "useQ");
    const bool useQMinion = Bool(HarrassMenu, "QMinion");
    const bool useE = Bool(HarrassMenu, "useE");
    const bool disE1 = Bool(HarrassMenu, "disE1");
    const bool disE2 = Bool(HarrassMenu, "disE2");

    if (player.ManaPercent() < static_cast<float>(mana)) {
        return;
    }

    const auto target = GetTarget(Q.Range, DamageType::Physical);
    if (!ValidHeroTarget(target, Q.Range)) {
        return;
    }

    if (useQ && Q.IsReady()) {
        const auto qPred = Q.GetPrediction(target, false, 0);
        if (HitchanceAtLeast(qPred.Hitchance, HitChance::High)) {
            CastPosition(Q, qPred.GetUnitPosition(), target);
        } else if (useQMinion) {
            const auto& col = qPred.CollisionObjects;
            if (!col.empty()) {
                for (const auto& colObj : col) {
                    if (ValidTarget(colObj, Q.Range) && colObj.Health() < Q.GetDamage(colObj)) {
                        CastPosition(Q, qPred.GetUnitPosition(), target);
                        break;
                    }
                }
            }
        }
    }

    if (useE && E.IsReady()) {
        for (const auto& t : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(t, E.Range) || !t.HasBuff("kalistaexpungemarker")) {
                continue;
            }
            if (disE1) {
                if (HasHardCcOrSlow(t)) continue;

                bool minionKillable = false;
                for (const auto& m : GameObjects::EnemyMinions()) {
                    if (ValidTarget(m, E.Range) && m.HasBuff("kalistaexpungemarker") &&
                        m.Health() <= GetEDamage(m)) {
                        minionKillable = true;
                        break;
                    }
                }
                if (minionKillable) {
                    if (disE2) {
                        if (SDK::Variables::TickCount() - E.LastCastAttemptT > 2500) {
                            E.Cast();
                        }
                    } else {
                        E.Cast();
                    }
                }
            }
        }
    }
}

// ============================================================================
// Clear — LaneClear + JungleClear (Kalista.cs, with D1 + D3)
// ============================================================================
static void Clear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    const int waveMana = Slider(LaneClearMenu, "Mana", 40);
    const bool useE = Bool(LaneClearMenu, "useE");
    const int minE = Slider(LaneClearMenu, "MinE", 2);
    const bool qJungle = Bool(LaneClearMenu, "useQ");

    if (player.ManaPercent() < static_cast<float>(waveMana)) {
        return;
    }

    // E on lane minions (>= MinE killable) — Kalista.cs.
    if (useE && E.IsReady()) {
        int killCount = 0;
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (ValidTarget(minion, E.Range) && minion.HasBuff("kalistaexpungemarker") &&
                minion.Health() < GetEDamage(minion)) {
                ++killCount;
            }
        }
        if (killCount >= minE) {
            E.Cast();
        }
    }

    // D1: Q on jungle — modelled on Ezreal.h::JungleClear (priority sort +
    // visible/plant/pet/clone filter + prediction) instead of the raw
    // Large||Legendary loop in Kalista.cs.
    if (qJungle && Q.IsReady()) {
        auto mobs = GameObjects::Jungle();
        mobs.erase(
            std::remove_if(mobs.begin(), mobs.end(), [](const AIMinionClient& mob) {
                return !ValidTarget(mob, Q.Range) || !mob.IsVisible() ||
                       mob.IsPlant() || mob.IsPet() || mob.IsClone();
            }),
            mobs.end());
        std::sort(mobs.begin(), mobs.end(),
                  [](const AIMinionClient& a, const AIMinionClient& b) {
                      const int pa = JunglePriority(a);
                      const int pb = JunglePriority(b);
                      if (pa != pb) return pa > pb;
                      if (std::fabs(a.MaxHealth() - b.MaxHealth()) > 1.0f) {
                          return a.MaxHealth() > b.MaxHealth();
                      }
                      return a.DistanceToPlayer() < b.DistanceToPlayer();
                  });
        for (const auto& mob : mobs) {
            if (!ValidTarget(mob, Q.Range)) continue;
            const auto qPred = Q.GetPrediction(mob);
            if (HitchanceAtLeast(qPred.Hitchance, HitChance::Medium)) {
                CastPosition(Q, qPred.GetUnitPosition(), mob);
                break;
            }
        }
    }

    // D3: E on jungle — every jungle monster; kill threshold is GetEDamage, with
    // an extra *0.5 for Epic/Legendary (IsEpicJungleMob, same test as Ezreal W).
    if (useE && E.IsReady()) {
        for (const auto& mob : GameObjects::Jungle()) {
            if (!ValidTarget(mob, E.Range) || !mob.HasBuff("kalistaexpungemarker")) {
                continue;
            }
            const float mult = IsEpicJungleMob(mob) ? 0.5f : 1.0f;
            if (mob.Health() < GetEDamage(mob) * mult) {
                E.Cast();
                break;
            }
        }
    }
}

// ============================================================================
// FlyHack (Kalista.cs)
// ============================================================================
static void FlyHack() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (!Key(ComboMenu, "FlyHack")) {
        return;
    }
    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo && SDK::AttackSpeed(player) > 2.0f) {
        const auto orbTarget = Orbwalker::GetTarget();
        if (orbTarget.IsValid()) {
            const AIBaseClient target(orbTarget.Handle());
            if (!target.IsValid()) return;
            if (SDK::Variables::TickCount() - LastAATick <= 10 + Game::Ping()) {
                SDK::IssueOrder(player, SDK::GameObjectOrder::MoveTo, Game::CursorPos());
            }
            if (SDK::Variables::TickCount() - LastAATick >= Game::Ping()) {
                SDK::IssueOrder(player, SDK::GameObjectOrder::AttackUnit, target);
                LastAATick = SDK::Variables::TickCount();
            }
        }
    }
}

// ============================================================================
// LogicE (Kalista.cs)
// ============================================================================
static void LogicE() {
    if (!E.IsReady()) {
        return;
    }
    const int eMode = ListIndex(Eset, "EMode", 1);
    const bool harassPlus = Bool(Eset, "harassPlus");

    if ((eMode == 0 && Orbwalker::ActiveMode() == OrbwalkingMode::Combo) || eMode == 1) {
        for (const auto& hero : GameObjects::EnemyHeroes()) {
            if (RendCache::IsUnitRendKillable(hero)) {
                E.Cast();
                break;
            }
        }
        if (harassPlus) {
            bool heroInRange = false;
            for (const auto& hero : GameObjects::EnemyHeroes()) {
                if (E.IsInRange(hero)) {
                    heroInRange = true;
                    break;
                }
            }
            if (heroInRange) {
                for (const auto& minion : GameObjects::EnemyMinions()) {
                    if (E.IsInRange(minion) && RendCache::IsUnitRendKillable(minion)) {
                        E.Cast();
                        break;
                    }
                }
            }
        }
    }
}

// ============================================================================
// RLogic (Kalista.cs)
// ============================================================================
static void RLogic() {
    const auto player = Player();
    if (!player.IsValid() || !R.IsReady()) {
        return;
    }

    const bool useRAllySaver = Bool(Rset, "kaliusersaveally");
    const bool balistaBool = Bool(Rset, "Balista");
    const bool talistaBool = Bool(Rset, "Talista");
    const bool salistaBool = Bool(Rset, "Salista");
    const bool useREngage = Bool(Rset, "userengage");

    AIHeroClient ally;
    for (const auto& a : GameObjects::AllyHeroes()) {
        if (a.IsMe() || a.IsDead()) continue;
        if (a.HasBuff("kalistacoopstrikeally")) {
            ally = a;
            break;
        }
    }

    if (ally.IsValid() && ally.IsVisible() && ally.DistanceToPlayer() <= R.Range) {
        if ((useRAllySaver &&
             player.CountEnemyHeroesInRange(R.Range) > 0 &&
             ally.CountEnemyHeroesInRange(R.Range) > 0 &&
             ally.HealthPercent() <= 30.0f) ||
            AllIncomingDamageToSoulbound() > player.Health()) {
            R.Cast();
        }

        for (auto it = IncomingDamageToSoulboundAlly.begin(); it != IncomingDamageToSoulboundAlly.end();) {
            if (it->first < Game::Time()) {
                it = IncomingDamageToSoulboundAlly.erase(it);
            } else {
                ++it;
            }
        }

        if (balistaBool && ally.CharacterName() == "Blitzcrank") {
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (!enemy.IsDead() && enemy.IsValid() && enemy.HasBuff("rocketgrab")) {
                    R.Cast();
                    break;
                }
            }
        }
        if (talistaBool && ally.CharacterName() == "TahmKench") {
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (!enemy.IsDead() && enemy.IsValid() && enemy.HasBuff("tahmkenchwdevoured")) {
                    R.Cast();
                    break;
                }
            }
        }
        if (salistaBool && ally.CharacterName() == "Skarner") {
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (!enemy.IsDead() && enemy.IsValid() && enemy.HasBuff("skarnerimpale")) {
                    R.Cast();
                    break;
                }
            }
        }
    }

    if (useREngage) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemy, 1000.0f)) continue;
            if (!Extensions::IsFacing(enemy, player)) continue;
            const auto waypoints = enemy.Path();
            if (!waypoints.empty()) {
                const Vector3 lastWaypoint = waypoints.back();
                if (lastWaypoint.Distance(player.ServerPosition()) < 400.0f) {
                    R.Cast();
                    break;
                }
            }
        }
    }
}

// ============================================================================
// Routine — KS jungle E, E leaving, E dying, ward-trick Auto W (Kalista.cs)
// ============================================================================
static void Routine() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // KS Dragon/Baron/other jungle with E (Kalista.cs Routine).
    if (Bool(KsMenu, "KSEJG") && E.IsReady()) {
        for (const auto& mob : GameObjects::Jungle()) {
            if (!ValidTarget(mob, E.Range)) continue;

            const std::string name = mob.CharacterName();
            if (name.find("Baron") != std::string::npos) {
                if (player.HasBuff("barontarget")) {
                    if (mob.Health() < GetEDamage(mob) * 0.5f) E.Cast();
                } else {
                    if (mob.Health() < GetEDamage(mob)) E.Cast();
                }
            } else if (name.find("Dragon") != std::string::npos) {
                if (player.HasBuff("barontarget")) {
                    const int stackCount = player.GetBuffCount("s5test_dragonslayerbuff");
                    const float reduction = 1.0f - (0.07f * static_cast<float>(stackCount));
                    if (mob.Health() < GetEDamage(mob) * reduction) E.Cast();
                } else {
                    if (mob.Health() < GetEDamage(mob)) E.Cast();
                }
            } else if (name.find("Mini") == std::string::npos &&
                       name.find("Dragon") == std::string::npos &&
                       name.find("Baron") == std::string::npos) {
                if (mob.Health() < GetEDamage(mob)) E.Cast();
            }
            break; // Kalista.cs: FirstOrDefault jungle mob only.
        }
    }

    // E when leaving range (Kalista.cs).
    if (Bool(Misc, "misc-leaving-e") && E.IsReady()) {
        const auto target = GetTarget(E.Range, DamageType::Physical);
        if (target.IsValid() && !target.IsDead()) {
            const int stackCount = target.GetBuffCount("kalistaexpungemarker");
            if (stackCount >= Slider(Misc, "misc-leaving-e-pro", 5)) {
                if (ValidHeroTarget(target) &&
                    target.DistanceToPlayer() > E.Range - 50.0f) {
                    if (Bool(Misc, "misc-prevent-e")) {
                        if (!SDK::HasBuffOfType(target, SDK::BuffType::SpellShield) ||
                            !SDK::HasBuffOfType(target, SDK::BuffType::Invulnerability)) {
                            E.Cast();
                        }
                    } else {
                        E.Cast();
                    }
                }
            }
        }
    }

    // E before dying (Kalista.cs).
    if (Bool(Misc, "misc-dying-e") && E.IsReady()) {
        if (player.HealthPercent() <= static_cast<float>(Slider(Misc, "misc-dying-e-pro", 10))) {
            E.Cast();
        }
    }

    // Ward trick — Auto W to Baron/Dragon (Kalista.cs). Default-on for scout checks,
    // but throttled and disabled while enemies are nearby so it does not spam casts.
    const auto* wardTrick = Misc ? Misc->Get<MenuKeyBind>("misc-ward-trick") : nullptr;
    if (wardTrick && wardTrick->Active && W.IsReady() &&
        player.CountEnemyHeroesInRange(1500.0f) == 0 &&
        SDK::Variables::TickCount() - LastAutoWCastTick >= 1000) {
        const Vector3 drakePos(9796.0f, 4432.0f, -71.0f);
        const Vector3 baronPos(5064.0f, 10568.0f, -71.0f);
        if (W.Range >= player.Distance(baronPos)) {
            if (CastPosition(W, baronPos)) {
                LastAutoWCastTick = SDK::Variables::TickCount();
            }
        } else if (W.Range >= player.Distance(drakePos)) {
            if (CastPosition(W, drakePos)) {
                LastAutoWCastTick = SDK::Variables::TickCount();
            }
        }
    }
}

// ============================================================================
// Killsteal — KSQ, KSE (Kalista.cs)
// ============================================================================
static void Killsteal() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Bool(KsMenu, "KSQ") && Q.IsReady()) {
        for (const auto& target : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(target, Q.Range)) continue;
            if (target.Health() < QDamageCalc(target) && !target.IsInvulnerable()) {
                const auto qPred = Q.GetPrediction(target, false, 0);
                if (HitchanceAtLeast(qPred.Hitchance, HitChance::High)) {
                    CastPosition(Q, qPred.GetUnitPosition(), target);
                }
            }
        }
    }

    if (Bool(KsMenu, "KSE") && E.IsReady()) {
        const bool tolerEnabled = SliderButtonEnabled(Misc, "EToler");
        const int tolerValue = SliderButtonValue(Misc, "EToler");
        for (const auto& target : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(target, E.Range)) continue;
            if (target.Health() < GetKalistaRealDamage(target, tolerEnabled, static_cast<float>(tolerValue)) &&
                !IsUnKillable(target)) {
                E.Cast();
                break;
            }
        }
    }
}

// ============================================================================
// OnEndScene — E damage indicator (Kalista.cs)
// ============================================================================
static void OnEndScene() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || Game::IsChatOpen()) {
        return;
    }
    if (!E.IsReady()) {
        return;
    }

    for (const auto& target : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(target) ||
            !target.IsVisibleOnScreen() ||
            !target.HasBuff("kalistaexpungemarker")) {
            continue;
        }
        const float dmg = GetEDamage(target);
        if (dmg <= 0.0f) continue;

        const Vec2 barPos = Drawing::HpBarScreenPos(target);
        if (!barPos.IsValid() || target.MaxHealth() <= 0.0f) continue;

        const float xPos = barPos.x - 45.0f;
        float yPos = barPos.y - 19.0f;
        if (target.CharacterName() == "Annie") {
            yPos += 2.0f;
        }
        const float remainHealth = target.Health() - dmg;
        const float x1 = xPos + (target.Health() / target.MaxHealth() * 104.0f);
        const float x2 = xPos + ((remainHealth > 0.0f ? remainHealth : 0.0f) / target.MaxHealth() * 103.4f);
        Drawing::DrawLine(x1, yPos, x2, yPos, 11.0f, 0xFFFF9300u);
    }
}

// ============================================================================
// OnNonKillableMinion — E lasthit assist (Kalista.cs)
// ============================================================================
static void OnNonKillableMinion(OrbwalkingActionArgs& args) {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (!Bool(LaneClearMenu, "useE") || !E.IsReady()) {
        return;
    }
    if (player.HasBuff("summonerexhaust")) {
        return;
    }
    if (player.Mana() - 40.0f < 40.0f) {
        return;
    }

    const AIBaseClient unit(args.Target.Handle());
    if (ValidTarget(unit) && RendCache::IsUnitRendKillable(unit)) {
        if (Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear ||
            Orbwalker::ActiveMode() == OrbwalkingMode::Harass ||
            Orbwalker::ActiveMode() == OrbwalkingMode::Combo) {
            E.Cast();
        }
    }
}

// ============================================================================
// OnPlayAnimation — Spell3 dance easter egg (Kalista.cs)
// ============================================================================
static void OnPlayAnimation(const Events::PlayAnimationEventArgs& args) {
    if (Events::IsLocalPlayer(args.Sender)) {
        if (strcmp(args.Animation, "Spell3") == 0) {
            Game::SendEmote(EmoteId::Dance);
            if (args.Process) {
                *args.Process = false;
            }
        }
    }
}

// ============================================================================
// OnGameLoad (Kalista.cs)
// ============================================================================
static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 1200.0f);
    Q.SetSkillshot(0.35f, 40.0f, 2400.0f, true, SpellType::Line);

    NonCollisionQ = Spell(SpellSlot::Q, 1140.0f);
    NonCollisionQ.SetSkillshot(0.25f, 40.0f, 2400.0f, false, SpellType::Line);

    W = Spell(SpellSlot::W, 5000.0f);
    E = Spell(SpellSlot::E, 1000.0f);
    R = Spell(SpellSlot::R, 1200.0f);
    R.SetSkillshot(0.50f, 1500.0f, FLT_MAX, false, SpellType::Circle);

    Ignite = Spell(player.GetSpellSlot("summonerdot"), 600.0f);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpellCast;
    Drawing::OnEndScene += &OnEndScene;
    Orbwalker::OnAfterAttack += &OnAfterAttack;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Orbwalker::OnNonKillableMinion += &OnNonKillableMinion;
    Events::hook.OnPlayAnimation += &OnPlayAnimation;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>7UP - Kalista loaded</font>");
}

// ============================================================================
// OnUnload
// ============================================================================
static void OnUnload() {
    if (!Loaded) {
        return;
    }
    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpellCast;
    Drawing::OnEndScene -= &OnEndScene;
    Orbwalker::OnAfterAttack -= &OnAfterAttack;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Orbwalker::OnNonKillableMinion -= &OnNonKillableMinion;
    Events::hook.OnPlayAnimation -= &OnPlayAnimation;

    Loaded = false;
}

} // namespace Plugins::AIO7UP::Kalista
