#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIJaxGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Jax {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MaximumBuffCount;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::SpellEnabled;

inline Menu* TacticsMenu = nullptr;
inline Menu* LeapMenu = nullptr;
inline Menu* CounterStrikeMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline PassiveLedger Passive = {};
inline CounterStrikeState CounterStrike = {};
inline int CompletedAttackSequence = 0;
inline int LastAfterAttackTargetId = 0;
inline int LastAfterAttackTick = 0;
inline int LastLocalAutoTargetId = 0;
inline int LastLocalAutoTick = 0;
inline int IncomingBasicAttackUntil = 0;
inline int IncomingBasicAttackSourceId = 0;
inline int IncomingBurstUntil = 0;
inline int PlayerOverrideUntil = 0;
inline int QCastTick = 0;
inline int WCastTick = 0;
inline int ECastTick = 0;
inline int RCastTick = 0;
inline int PendingWTargetId = 0;
inline int PendingWUntil = 0;
inline int RExpireTick = 0;
inline bool RActive = false;
inline Vector3 LastLeapEndpoint = {};

using ControllerHelpers::Now;

using ControllerHelpers::Ready;

inline bool Throttle(int index, int delayMs) {
    const int tick = index == 0 ? QCastTick : index == 1 ? WCastTick :
        index == 2 ? ECastTick : RCastTick;
    return Now() - tick >= delayMs;
}

inline bool TargetCannotBeDamaged(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           target.HasBuff("JaxCounterStrike") || target.HasBuff("FioraW") ||
           target.HasBuff("SivirE") || target.HasBuff("NocturneShroudofDarkness") ||
           target.HasBuff("MorganaE") || target.HasBuff("BlackShield") ||
           target.HasBuff("VladimirSanguinePool") || target.HasBuff("FizzEIcon") ||
           target.HasBuff("KayleR") || target.HasBuff("kindredrnodeathbuff");
}

inline int SpellRank(int index) {
    return index >= 0 && index < 4 && Engine::RuntimeSpells[index]
        ? std::max(0, Engine::RuntimeSpells[index]->Level()) : 0;
}

inline float QDamage(const AIHeroClient& target) {
    static constexpr std::array<float, 6> base{ 0.0f, 65.0f, 105.0f, 145.0f, 185.0f, 225.0f };
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = base[std::clamp(SpellRank(0), 0, 5)] +
        player.BonusAttackDamage() + 0.60f * player.AP();
    return player.CalculatePhysicalDamage(target, raw);
}

inline float WDamage(const AIHeroClient& target) {
    static constexpr std::array<float, 6> base{ 0.0f, 40.0f, 75.0f, 110.0f, 145.0f, 180.0f };
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = base[std::clamp(SpellRank(1), 0, 5)] + 0.60f * player.AP();
    return player.CalculateMagicDamage(target, raw);
}

inline float EDamage(const AIHeroClient& target) {
    static constexpr std::array<float, 6> base{ 0.0f, 40.0f, 55.0f, 70.0f, 85.0f, 100.0f };
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = (base[std::clamp(SpellRank(2), 0, 5)] +
        0.035f * target.MaxHealth()) *
        CounterStrikeDamageMultiplier(CounterStrike.DodgedBasicAttacks);
    return player.CalculatePhysicalDamage(target, raw);
}

using ControllerHelpers::Lethal;

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    Passive = NormalizePassive(Passive, now);
    const int liveStacks = MaximumBuffCount(
        player, { "JaxPassiveHaste", "jaxpassivehaste", "JaxPassiveStacks" });
    if (liveStacks > 0) {
        Passive.Stacks = std::clamp(liveStacks, 0, kMaximumPassiveStacks);
        Passive.ExpiresAt = std::max(Passive.ExpiresAt, now + 350);
    }

    const bool liveCounterStrike = player.HasBuff("JaxCounterStrike") ||
        player.HasBuff("JaxE") || player.HasBuff("jaxcounterstrike");
    if (liveCounterStrike) {
        if (!CounterStrike.Active) CounterStrike = { true, now, 0 };
    } else if (CounterStrike.Active &&
               now - CounterStrike.StartedAt >= kEMaximumDurationMs) {
        CounterStrike = {};
    }

    RActive = player.HasBuff("JaxRelentlessAssault") ||
        player.HasBuff("JaxR") || (RActive && now < RExpireTick);
    if (RExpireTick > 0 && now >= RExpireTick) {
        RActive = false;
        RExpireTick = 0;
    }
    if (PendingWUntil > 0 && now > PendingWUntil) {
        PendingWTargetId = PendingWUntil = 0;
    }
}

inline LeapContext BuildLeapContext(const AIHeroClient& target,
                                    bool fleeing,
                                    bool lethal) {
    LeapContext context{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + 35.0f)) return context;
    const Vector3 endpoint = target.Position();
    context.TargetValid = true;
    context.EndpointValid = endpoint.IsValid() && !endpoint.IsZero();
    context.EndpointWall = !context.EndpointValid || SDK::NavMesh::IsWall(endpoint);
    context.UnderEnemyTurret = context.EndpointValid && Engine::UnderEnemyTurret(endpoint);
    context.StartedUnderEnemyTurret = Engine::UnderEnemyTurret(player.Position());
    context.EnemyTarget = true;
    context.Lethal = lethal;
    context.Fleeing = fleeing;
    context.RetreatProgress = !fleeing ||
        endpoint.Distance2D(Game::CursorPos()) + 45.0f <
            player.Position().Distance2D(Game::CursorPos());
    context.NearbyEnemies = context.EndpointValid
        ? Engine::CountEnemiesAt(endpoint, 550.0f) : 99;
    context.MaximumEnemies = Slider(LeapMenu, "MaxLeapEnemies", 2);
    return context;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool fleeing = false) {
    if (!Ready(0, mode) || !Throttle(0, 70) ||
        !Engine::ValidEnemy(target, kQRange + 35.0f) || TargetCannotBeDamaged(target)) {
        return false;
    }
    const bool lethal = Lethal(target, QDamage(target));
    const LeapContext safety = BuildLeapContext(target, fleeing, lethal);
    if (!LeapEndpointSafe(safety) || (Orbwalker::IsWindingUp() && !lethal)) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    QCastTick = Now();
    LastLeapEndpoint = target.Position();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool allowLethal = true) {
    if (!Ready(1, mode) || !Throttle(1, 45) ||
        !Engine::ValidEnemy(target) || !InAutoAttackRange(target, 45.0f) ||
        TargetCannotBeDamaged(target)) {
        return false;
    }
    const bool lethal = allowLethal && Lethal(target, WDamage(target));
    WResetContext reset{};
    reset.Ready = true;
    reset.AttackCompleted = LastAfterAttackTick > 0;
    reset.ExactTarget = LastAfterAttackTargetId == static_cast<int>(target.NetworkId());
    reset.AttackWindingUp = Orbwalker::IsWindingUp();
    reset.Lethal = lethal;
    reset.MillisecondsSinceAttack = Now() - LastAfterAttackTick;
    if (!MayResetWithW(reset)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    WCastTick = Now();
    PendingWTargetId = PendingWUntil = 0;
    return true;
}

inline bool CastEStart(const AIHeroClient& target, Mode mode, bool defensive) {
    if (CounterStrike.Active || !Ready(2, mode) || !Throttle(2, 70)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    EStartContext context{};
    context.Ready = true;
    context.IncomingBasicAttack = IncomingBasicAttackUntil >= Now();
    context.Defensive = defensive;
    context.ComboCommit = mode == Mode::Combo || mode == Mode::Harass;
    context.TargetInRadius = Engine::ValidEnemy(target, kERadius + target.BoundingRadius());
    context.AttackWindingUp = Orbwalker::IsWindingUp();
    if (!ShouldStartCounterStrike(context)) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    ECastTick = Now();
    CounterStrike = { true, ECastTick, 0 };
    return true;
}

inline bool CastERecast(const AIHeroClient& target, Mode mode,
                        bool fleeing = false, bool interrupt = false) {
    if (!CounterStrike.Active || !Ready(2, mode) || !Throttle(2, 70)) return false;
    ERecastContext context{};
    context.Active = true;
    context.ElapsedMs = Now() - CounterStrike.StartedAt;
    context.TargetInRadius = Engine::ValidEnemy(target, kERadius + target.BoundingRadius());
    context.TargetEscaping = Engine::ValidEnemy(target) &&
        GameObjects::Player().Position().Distance2D(target.Position()) >= kERadius - 35.0f;
    context.Interrupt = interrupt;
    context.Lethal = Engine::ValidEnemy(target) && Lethal(target, EDamage(target));
    context.Fleeing = fleeing;
    context.DodgedBasicAttacks = CounterStrike.DodgedBasicAttacks;
    if (!ShouldRecastCounterStrike(context)) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    ECastTick = Now();
    CounterStrike = {};
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool defensive) {
    if (!Ready(3, mode) || !Throttle(3, 110)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), 700.0f);
    const int allies = Engine::CountAlliesAt(player.Position(), 800.0f);
    RContext context{};
    context.Ready = true;
    context.AlreadyActive = RActive;
    context.TargetValid = Engine::ValidEnemy(target, 800.0f);
    context.Duel = enemies == 1;
    context.TargetCommitted = Engine::ValidEnemy(target, 450.0f) || CounterStrike.Active;
    context.TargetDangerous = Engine::ValidEnemy(target) &&
        (target.HealthPercent() >= player.HealthPercent() ||
         target.TotalAttackDamage() > player.TotalAttackDamage());
    context.IncomingBurst = defensive || IncomingBurstUntil >= Now();
    context.PlayerHealthPercent = player.HealthPercent();
    context.DefensiveHealthPercent = static_cast<float>(
        Slider(UltimateMenu, "RDefensiveHP", 48));
    context.NearbyEnemies = enemies;
    context.NearbyAllies = allies;
    if (!MayActivateGrandmaster(context)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    RCastTick = Now();
    RExpireTick = RCastTick + kRDurationMs;
    RActive = true;
    return true;
}

inline bool TryDefensive(const AIHeroClient& threat, Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool incomingAuto = IncomingBasicAttackUntil >= Now();
    const bool emergency = incomingAuto || IncomingBurstUntil >= Now() ||
        player.HealthPercent() <= Slider(TacticsMenu, "EmergencyHP", 32);
    if (!emergency) return false;
    if (CounterStrike.Active && CastERecast(threat, mode, mode == Mode::Flee)) return true;
    if (CastEStart(threat, mode, true)) return true;
    return CastR(threat, mode, true);
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (InAutoAttackRange(target, 45.0f) && Lethal(target, WDamage(target)) &&
        CastW(target, mode)) return true;
    if (mode != Mode::Automatic && Lethal(target, QDamage(target)) && CastQ(target, mode)) {
        return true;
    }
    if (CounterStrike.Active && Lethal(target, EDamage(target)) &&
        CastERecast(target, mode)) return true;
    return false;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CounterStrike.Active && CastERecast(target, Mode::Combo)) return true;
    if (CastR(target, Mode::Combo, false)) return true;
    const auto player = GameObjects::Player();
    if (player.IsValid() && player.Position().Distance2D(target.Position()) >
        ControllerHelpers::AutoAttackRange(target) + 35.0f && CastQ(target, Mode::Combo)) {
        return true;
    }
    if (CastEStart(target, Mode::Combo, false)) return true;
    if (CastW(target, Mode::Combo)) return true;
    return false;
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CounterStrike.Active && CastERecast(target, Mode::Harass)) return true;
    if (CastW(target, Mode::Harass)) return true;
    const auto player = GameObjects::Player();
    if (player.IsValid() && player.HealthPercent() >= 60.0f &&
        Passive.Stacks >= Slider(TacticsMenu, "HarassPassiveStacks", 3) &&
        player.Position().Distance2D(target.Position()) >
            ControllerHelpers::AutoAttackRange(target) + 40.0f) {
        return CastQ(target, Mode::Harass);
    }
    return false;
}

inline bool TryFlee(const AIHeroClient& threat) {
    if (CounterStrike.Active && CastERecast(threat, Mode::Flee, true)) return true;
    if (CastEStart(threat, Mode::Flee, true)) return true;
    if (Engine::ValidEnemy(threat) && CastQ(threat, Mode::Flee, true)) return true;
    return CastR(threat, Mode::Flee, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (PlayerOverrideUntil > Now()) return true;
    AIHeroClient target = selected;
    if (!Engine::ValidEnemy(target)) target = Engine::SelectTarget(kQRange + 100.0f);
    const AIHeroClient threat = NearestEnemyToPlayer(target, 950.0f);

    if (mode == Mode::Flee) {
        (void)TryFlee(threat);
        return true;
    }
    if (TryDefensive(threat, mode == Mode::None ? Mode::Automatic : mode)) return true;
    if (TryKillSecure(target, mode)) return true;

    switch (mode) {
    case Mode::Combo: (void)TryCombo(target); break;
    case Mode::Harass: (void)TryHarass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)Engine::TryFarm(mode); break;
    case Mode::Automatic: {
        AutomaticContext context{};
        context.ManualOwnership = PlayerOverrideUntil > Now();
        context.Defensive = IncomingBurstUntil >= Now();
        context.IncomingBasicAttack = IncomingBasicAttackUntil >= Now();
        context.KillSecure = Engine::ValidEnemy(target) &&
            (Lethal(target, WDamage(target)) ||
             (CounterStrike.Active && Lethal(target, EDamage(target))));
        if (AutomaticAllowed(context)) {
            if (!TryDefensive(threat, Mode::Automatic))
                (void)TryKillSecure(target, Mode::Automatic);
        }
        break;
    }
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !args.Sender.IsValid()) return;
    const int now = Now();
    if (!IsLocalPlayer(args.Sender)) {
        const bool targetsPlayer = args.TargetNetworkId ==
            static_cast<std::uint32_t>(player.NetworkId());
        // const bool turretAttack = args.Sender.Type ==
        // ::Core::Objects::ObjectType::AITurretClient;
        const bool turretAttack = false;
        if (args.IsAutoAttack && targetsPlayer) {
            IncomingBasicAttackUntil = now + 650;
            IncomingBasicAttackSourceId = static_cast<int>(args.Sender.NetworkId);
            if (CounterStrikeAvoidsBasicAttack(
                    CounterStrike.Active, true, true, turretAttack)) {
                CounterStrike.DodgedBasicAttacks = std::min(
                    5, CounterStrike.DodgedBasicAttacks + 1);
            }
        }
        const auto analysis = ControllerHelpers::AnalyzeEnemyCast(
            args, 220.0f, 110.0f, 250, 280, 260, 1500, 450);
        if (analysis.Valid && analysis.TargetsPlayer &&
            (analysis.LikelyHardCrowdControl || !args.IsAutoAttack)) {
            IncomingBurstUntil = now + 700;
        }
        return;
    }

    if (args.IsAutoAttack) return;
    const int slot = args.Slot;
    const bool owned = slot >= 0 && slot < 4 && Engine::WasControllerCast(slot);
    if (!owned) PlayerOverrideUntil = now +
        Slider(TacticsMenu, "ManualOwnershipMs", 560);
    if (slot == 0) {
        QCastTick = now;
        if (args.EndPosition.IsValid() && !args.EndPosition.IsZero())
            LastLeapEndpoint = args.EndPosition;
    } else if (slot == 1) {
        WCastTick = now;
    } else if (slot == 2) {
        ECastTick = now;
        if (Engine::TextContains(args.SpellName, "Attack") ||
            Engine::TextContains(args.SpellName, "Recast")) {
            CounterStrike = {};
        } else if (!CounterStrike.Active) {
            CounterStrike = { true, now, 0 };
        }
    } else if (slot == 3) {
        RCastTick = now;
        RActive = true;
        RExpireTick = now + kRDurationMs;
    }
}

inline bool JaxBuff(const char* name, const char* token) {
    return Engine::TextContains(name, token);
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (JaxBuff(args.BuffName, "JaxPassive")) {
        if (args.Count > 0) Passive.Stacks = std::clamp(args.Count, 0, kMaximumPassiveStacks);
        Passive.ExpiresAt = now + ControllerHelpers::RemainingMilliseconds(
            args.EndTime, kPassiveDurationMs, 100, 4000);
    }
    if (JaxBuff(args.BuffName, "CounterStrike") || JaxBuff(args.BuffName, "JaxE")) {
        if (!CounterStrike.Active) CounterStrike = { true, now, 0 };
    }
    if (JaxBuff(args.BuffName, "RelentlessAssault") || JaxBuff(args.BuffName, "JaxR")) {
        RActive = true;
        RExpireTick = now + ControllerHelpers::RemainingMilliseconds(
            args.EndTime, kRDurationMs, 500, 10000);
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (JaxBuff(args.BuffName, "JaxPassive")) Passive = {};
    if (JaxBuff(args.BuffName, "CounterStrike") || JaxBuff(args.BuffName, "JaxE"))
        CounterStrike = {};
    if (JaxBuff(args.BuffName, "RelentlessAssault") || JaxBuff(args.BuffName, "JaxR")) {
        RActive = false;
        RExpireTick = 0;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(args, LastAfterAttackTargetId, LastAfterAttackTick)) return;
    Passive = ObservePassiveAttack(Passive, LastAfterAttackTick);
    ++CompletedAttackSequence;
    PendingWTargetId = LastAfterAttackTargetId;
    PendingWUntil = LastAfterAttackTick + 360;
}

inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawRanges", false)) return;
    Drawing::DrawCircle(player.Position(), CounterStrike.Active ? kERadius : kQRange,
        CounterStrike.Active ? 0xFFE8B34Fu : 0xFF8E62D9u, 1.8f, 40);
    if (!LastLeapEndpoint.IsZero())
        Drawing::DrawLine(player.Position(), LastLeapEndpoint, 0xFFB58AE8u, 2.0f);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("JaxOneTrick", "Jax duel mechanics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    TacticsMenu->Add(new MenuSlider("EmergencyHP", "Emergency E/R threshold", 32, 10, 70));
    TacticsMenu->Add(new MenuSlider("HarassPassiveStacks", "Passive stacks before leap harass", 3, 0, 8));
    LeapMenu = TacticsMenu->AddSubMenu(new Menu("LeapStrike", "Leap endpoint safety"));
    LeapMenu->Add(new MenuSlider("MaxLeapEnemies", "Maximum enemies at Q endpoint", 2, 1, 5));
    CounterStrikeMenu = TacticsMenu->AddSubMenu(new Menu("CounterStrike", "Counter Strike timing"));
    CounterStrikeMenu->Add(new MenuSeparator("DodgeRule", "Hold E for dodge value; recast for stun/escape"));
    UltimateMenu = TacticsMenu->AddSubMenu(new Menu("Grandmaster", "Defensive and duel R"));
    UltimateMenu->Add(new MenuSlider("RDefensiveHP", "R defensive HP threshold", 48, 15, 85));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("JaxFarm", "Orbwalker-owned farm"));
    FarmMenu->Add(new MenuSeparator("FarmRule", "Shared farm path preserves AA ownership"));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("JaxCoach", "Mechanic visualization"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/E range and leap route", false));
}

inline void OnLoad() {
    Passive = {};
    CounterStrike = {};
    CompletedAttackSequence = 0;
    LastAfterAttackTargetId = LastAfterAttackTick = 0;
    LastLocalAutoTargetId = LastLocalAutoTick = 0;
    IncomingBasicAttackUntil = IncomingBasicAttackSourceId = IncomingBurstUntil = 0;
    PlayerOverrideUntil = 0;
    QCastTick = WCastTick = ECastTick = RCastTick = 0;
    PendingWTargetId = PendingWUntil = 0;
    RExpireTick = 0;
    RActive = false;
    LastLeapEndpoint = {};
    ReconcileState();
}

inline void OnUnload() {
    TacticsMenu = LeapMenu = CounterStrikeMenu = UltimateMenu = FarmMenu = CoachMenu = nullptr;
    Passive = {};
    CounterStrike = {};
    RActive = false;
}

inline constexpr const char* Scenarios[] = {
    "Pin Jax to Riot 26.15 and CommunityDragon 16.15 Summoner's Rift mechanics",
    "Track up to eight Relentless Assault passive stacks",
    "Refresh the passive expiry on each completed basic attack",
    "Reconcile passive stack count from live buff telemetry",
    "Expire stale passive stacks when events are missed",
    "Track every third completed attack for Grandmaster passive cadence",
    "Use Leap Strike's 700 targeted reach without fake skillshot prediction",
    "Reject Q when the target endpoint is invalid or a wall",
    "Reject a new enemy-turret Q endpoint unless the target is lethal",
    "Reject crowded Q endpoints above the configured enemy limit",
    "Require Q to make cursor-relative retreat progress in Flee",
    "Preserve the current AA windup before a nonlethal Q",
    "Respect selected target before the shared orbwalker target fallback",
    "Use W only after an exact-target completed attack or for lethal damage",
    "Preserve an in-progress basic attack instead of clipping it with W",
    "Keep AA-W-AA reset timing inside the observed reset window",
    "Track Counter Strike start and recast state from events and polling",
    "Start E reactively for a basic attack directed at Jax",
    "Do not claim Counter Strike dodges turret attacks",
    "Count observed basic attacks avoided during active E",
    "Scale E damage only to the five-dodge cap",
    "Respect the one-second minimum E recast lock",
    "Hold E when the target remains contained and more dodge value is available",
    "Recast E when the target exits the stun radius",
    "Recast E for lethal damage, interruption, flee or expiry",
    "Use the 375 Counter Strike radius for containment decisions",
    "Do not activate R while its defensive buff is already active",
    "Activate R for real incoming burst or emergency health",
    "Activate R when Jax is materially outnumbered",
    "Activate R in a committed dangerous duel",
    "Hold R in a healthy low-pressure ordinary one-on-one",
    "Combo gates R before committing to a dangerous duel",
    "Combo uses safe Q entry and starts E without clipping an attack",
    "Combo gives W priority only in its valid reset window",
    "Harass requires passive preparation before an optional leap",
    "Harass never spends R as an unsolicited trade tool",
    "LaneClear remains on the shared orbwalker-owned farm path",
    "Jungle remains on the shared farm path while W/E profile semantics apply",
    "LastHit does not spend Q or E on an unsolicited champion route",
    "Flee prioritizes E dodge and stun timing before any leap",
    "Flee Q must improve the cursor-relative retreat route",
    "Automatic mode may answer incoming basic attacks and burst",
    "Automatic mode may use in-range W or active-E lethal damage",
    "Automatic mode never starts an unsolicited Q engage",
    "Track manual Q/W/E/R casts and yield the owned decision loop",
    "Keep manual spell state visible to event and polling reconciliation",
    "Never automate Flash, Ignite, Smite, item actives or movement orders",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Jax;
    controller.ControllerId = "champion.kuroaio.ai.jax.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIJax.md";
    controller.ImplementationSummary =
        "Eight-stack passive reconciliation, safe targeted Q endpoints, AA-W-AA reset "
        "ownership, attack-dodging Counter Strike timing, and defensive/duel-gated R.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastLocalAutoTargetId, &LastLocalAutoTick>;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Jax
