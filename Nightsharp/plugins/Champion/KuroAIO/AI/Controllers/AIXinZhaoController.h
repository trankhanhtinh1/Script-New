#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIXinZhaoGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::XinZhao {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MaximumBuffCount;
using ControllerHelpers::NameEquals;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Ready;
using ControllerHelpers::RemainingMilliseconds;
using ControllerHelpers::RuntimeNameContains;
using ControllerHelpers::SpellRank;
using ControllerHelpers::SpellEnabled;

enum class Sequence : int {
    None,
    WChallenge,
    ChallengeDash,
    QResetChain,
    QKnockupReady,
    QKnockupLanded,
    CrescentIsolation,
    CrescentDefense,
    EscapeCharge,
    JungleChain,
};

enum class UltimateReason : int {
    None,
    Isolation,
    Lethal,
    OutsideDamage,
    MultiThreat,
    Gapcloser,
    Interrupt,
    Flee,
    Manual,
};

struct WPlan {
    Vector3 Aim = {};
    Vector3 Predicted = {};
    int TargetId = 0;
    int ChampionHits = 0;
    bool SlashHits = false;
    bool Valid = false;
};

struct EPlan {
    Vector3 PredictedTarget = {};
    Vector3 Endpoint = {};
    int TargetId = 0;
    int EnemiesAtEndpoint = 0;
    int AlliesAtEndpoint = 0;
    bool Challenged = false;
    bool CursorAgrees = false;
    bool Safe = false;
    bool Valid = false;
};

struct RPlan {
    IsolationContext Context = {};
    int ChallengedTargetId = 0;
    int SweepHits = 0;
    int Knockbacks = 0;
    float Score = -FLT_MAX;
    bool Safe = false;
    bool Valid = false;
};

inline Menu* TacticsMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;

inline Sequence ActiveSequence = Sequence::None;
inline UltimateReason LastUltimateReason = UltimateReason::None;
inline Mode LastMode = Mode::None;
inline int LastDecisionTargetId = 0;
inline int ManualOwnershipUntil = 0;

inline int PassiveCompletedAttacks = 0;
inline int PassiveLastProcTick = 0;
inline float PassiveLastExpectedHeal = 0.0f;
inline bool QActive = false;
inline int QStrikes = 0;
inline int QCastTick = 0;
inline int QExpireTick = 0;
inline int QTargetId = 0;
inline int LastQKnockupTick = 0;
inline int WCastTick = 0;
inline int WTargetId = 0;
inline Vector3 LastWDirection = {};
inline WPlan LastWPlan = {};
inline int ChallengeTargetId = 0;
inline int ChallengeExpireTick = 0;
inline int ECastTick = 0;
inline int ETargetId = 0;
inline int EExpectedArrivalTick = 0;
inline bool EDashActive = false;
inline EPlan LastEPlan = {};
inline bool RActive = false;
inline int RCastTick = 0;
inline int RExpireTick = 0;
inline RPlan LastRPlan = {};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastRecordedAutoTargetId = 0;
inline int LastRecordedAutoTick = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingOutsideThreats = 0;
inline float IncomingDamage = 0.0f;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;

inline constexpr int kQDurationMs = 5000;
inline constexpr int kChallengeMs = 3000;
inline constexpr int kRDurationMs = 4000;

inline bool CastThrottleReady(int slot, bool reactive = false) {
    return ControllerHelpers::CastThrottleReady(slot, 35, reactive ? 0 : -1);
}

inline bool TargetCannotBeDamaged(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           HasSpellShieldOrImmunity(target) || target.HasBuff("FioraW") ||
           target.HasBuff("VladimirSanguinePool") ||
           target.HasBuff("FizzE") || target.HasBuff("FizzEIcon") ||
           target.HasBuff("EliseSpiderE") || target.HasBuff("BardRStasis") ||
           target.HasBuff("KayleR") || target.HasBuff("kindredrnodeathbuff");
}

inline bool TargetDisplacementImmune(const AIHeroClient& target) {
    static constexpr std::array<const char*, 14> buffs = {
        "OlafRagnarok", "SionR", "MalphiteR", "ViR", "WarwickR",
        "HecarimUlt", "VolibearR", "ShyvanaTransform", "OrnnW", "UdyrE2",
        "KSanteW", "KSanteW_AllOut", "BriarE", "GalioE",
    };
    for (const char* name : buffs) if (target.HasBuff(name)) return true;
    return false;
}

inline bool ChallengeBuffName(const char* name) {
    return NameEquals(name, "XinZhaoWMark") ||
           NameEquals(name, "XinZhaoWDebuff") ||
           NameEquals(name, "XinZhaoChallenge") ||
           NameEquals(name, "XinZhaoRChallenge") ||
           (Engine::TextContains(name, "XinZhao") &&
            Engine::TextContains(name, "Challenge"));
}

inline bool Challenged(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (static_cast<int>(target.NetworkId()) == ChallengeTargetId &&
        ChallengeExpireTick >= Now()) return true;
    return target.HasBuff("XinZhaoWMark") ||
           target.HasBuff("XinZhaoWDebuff") ||
           target.HasBuff("XinZhaoChallenge") ||
           target.HasBuff("XinZhaoRChallenge");
}

inline void SetChallenge(int targetId, int durationMs = kChallengeMs) {
    if (targetId == 0) return;
    ChallengeTargetId = targetId;
    ChallengeExpireTick = Now() + std::max(0, durationMs);
}

inline bool InAttackRange(const AIBaseClient& target, float allowance = 35.0f) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid() &&
           player.Position().Distance2D(target.Position()) <=
               player.AttackRange() + player.BoundingRadius() +
               target.BoundingRadius() + allowance;
}

inline bool CursorConsents(const Vector3& endpoint, float opposition = 220.0f) {
    const auto player = GameObjects::Player();
    const Vector3 cursor = Game::CursorPos();
    if (!player.IsValid() || !cursor.IsValid() || cursor.IsZero() ||
        !endpoint.IsValid()) return false;
    return endpoint.Distance2D(cursor) <=
           player.Position().Distance2D(cursor) + opposition;
}

inline float PassiveExpectedHeal() {
    const auto player = GameObjects::Player();
    return player.IsValid()
        ? PassiveRawHealing(player.Level(), player.MaxHealth(), player.AP())
        : 0.0f;
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    const bool liveQ = player.HasBuff("XinZhaoQ") ||
                       player.HasBuff("xinzhaoq") ||
                       RuntimeNameContains(0, "XinZhaoQ");
    if (liveQ && !QActive) {
        QActive = true;
        QExpireTick = now + kQDurationMs;
    }
    if (QActive && now > QExpireTick) {
        QActive = false;
        QStrikes = QTargetId = 0;
    }
    const int liveQCount = MaximumBuffCount(
        player, { "XinZhaoQ", "xinzhaoq" });
    if (QActive && QStrikes == 0 && liveQCount == 2) QStrikes = 1;

    RActive = player.HasBuff("XinZhaoRRangedImmunity") ||
              player.HasBuff("XenZhaoParry") || player.HasBuff("XinZhaoR") ||
              (RActive && now <= RExpireTick);
    if (RActive && RExpireTick <= now) RExpireTick = now + 250;
    if (RActive && RCastTick > 0 && now > RCastTick + kRDurationMs + 300)
        RActive = false;
    EDashActive = player.IsDashing() && ECastTick > 0 &&
                  now <= EExpectedArrivalTick + 450;
    if (ChallengeExpireTick < now) ChallengeTargetId = ChallengeExpireTick = 0;
    if (IncomingThreatUntil < now) {
        IncomingDamage = 0.0f;
        IncomingOutsideThreats = 0;
    }
    PassiveLastExpectedHeal = PassiveExpectedHeal();
}

inline WPlan BuildWPlan(const AIHeroClient& target) {
    WPlan plan{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetCannotBeDamaged(target)) return plan;
    plan.Predicted = PredictPosition(target, 0.60f);
    const Vector3 direction = Direction2D(player.Position(), plan.Predicted);
    if (direction.IsZero() ||
        !WThrustHits(player.Position(), direction, plan.Predicted,
                     target.BoundingRadius())) return plan;
    plan.Aim = player.Position() + direction * kWRange;
    plan.TargetId = static_cast<int>(target.NetworkId());
    plan.SlashHits = WSlashHits(player.Position(), direction, plan.Predicted,
                                target.BoundingRadius());
    plan.ChampionHits = 1;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) ||
            static_cast<int>(enemy.NetworkId()) == plan.TargetId) continue;
        const Vector3 predicted = PredictPosition(enemy, 0.60f);
        if (WThrustHits(player.Position(), direction, predicted,
                        enemy.BoundingRadius())) ++plan.ChampionHits;
    }
    plan.Valid = true;
    return plan;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (!Ready(1) || !SpellEnabled(1, mode) || !CastThrottleReady(1, reactive) ||
        TargetCannotBeDamaged(target)) return false;
    if (!reactive && Orbwalker::IsWindingUp() &&
        Orbwalker::AttackCastDelayRemaining() > 30) return false;
    const WPlan plan = BuildWPlan(target);
    LastWPlan = plan;
    if (!plan.Valid || !Engine::ControllerCastPosition(1, plan.Aim)) return false;
    WCastTick = Now();
    WTargetId = plan.TargetId;
    LastWDirection = Direction2D(GameObjects::Player().Position(), plan.Aim);
    SetChallenge(plan.TargetId);
    ActiveSequence = Sequence::WChallenge;
    return true;
}

inline EPlan BuildEPlan(const AIBaseClient& target,
                        bool challenged,
                        bool escape = false) {
    EPlan plan{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid() || target.IsDead() ||
        !target.IsTargetable()) return plan;
    plan.TargetId = static_cast<int>(target.NetworkId());
    plan.Challenged = challenged;
    plan.PredictedTarget = target.IsHero()
        ? PredictPosition(AIHeroClient(target.Handle()), 0.15f)
        : target.Position();
    const float distance = player.Position().Distance2D(plan.PredictedTarget);
    if (!ECanReach(distance, player.BoundingRadius(), target.BoundingRadius(),
                   challenged)) return plan;
    plan.Endpoint = EDashEndpoint(player.Position(), plan.PredictedTarget,
                                  player.BoundingRadius(),
                                  target.BoundingRadius(), 15.0f);
    if (!plan.Endpoint.IsValid() || SDK::NavMesh::IsWall(plan.Endpoint))
        return plan;
    plan.EnemiesAtEndpoint = Engine::CountEnemiesAt(plan.Endpoint, 650.0f);
    plan.AlliesAtEndpoint = CountAlliedFollowup(plan.Endpoint, 850.0f);
    plan.CursorAgrees = CursorConsents(plan.Endpoint, escape ? 0.0f : 220.0f);
    plan.Safe = !Engine::UnderEnemyTurret(plan.Endpoint) &&
                !HasReadyDashHazardAt(plan.Endpoint) && plan.CursorAgrees &&
                plan.EnemiesAtEndpoint <= plan.AlliesAtEndpoint +
                    (escape ? 0 : 1);
    plan.Valid = true;
    return plan;
}

inline bool CastE(const AIBaseClient& target,
                  bool challenged,
                  Mode mode,
                  bool escape = false,
                  bool reactive = false) {
    if (!Ready(2) || !SpellEnabled(2, mode) ||
        !CastThrottleReady(2, reactive) || !target.IsValid()) return false;
    const EPlan plan = BuildEPlan(target, challenged, escape);
    LastEPlan = plan;
    if (!plan.Valid || !plan.Safe || !Engine::ControllerCastUnit(2, target))
        return false;
    const auto player = GameObjects::Player();
    const float exposed = EExposedDashDistance(
        player.Position().Distance2D(plan.PredictedTarget),
        player.BoundingRadius(), target.BoundingRadius());
    ECastTick = Now();
    EExpectedArrivalTick = ECastTick +
        static_cast<int>(150.0f + exposed / 1800.0f * 1000.0f);
    ETargetId = plan.TargetId;
    if (target.IsHero()) SetChallenge(plan.TargetId);
    ActiveSequence = escape ? Sequence::EscapeCharge
                            : Sequence::ChallengeDash;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode) {
    return CastE(AIBaseClient(target.Handle()), Challenged(target), mode);
}

inline bool CastQ(Mode mode,
                  const AIBaseClient& intended,
                  bool reactiveReset = false) {
    if (!Ready(0) || !SpellEnabled(0, mode) || QActive ||
        !CastThrottleReady(0, reactiveReset)) return false;
    if (!reactiveReset && Orbwalker::IsWindingUp() &&
        Orbwalker::AttackCastDelayRemaining() > 20) return false;
    if (intended.IsValid() && !InAttackRange(intended, 55.0f)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    QActive = true;
    QStrikes = 0;
    QCastTick = Now();
    QExpireTick = QCastTick + kQDurationMs;
    QTargetId = intended.IsValid() ? static_cast<int>(intended.NetworkId()) : 0;
    ActiveSequence = Sequence::QResetChain;
    return true;
}

inline RPlan BuildRPlan(const AIHeroClient& challengedTarget) {
    RPlan plan{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(challengedTarget)) return plan;
    plan.ChallengedTargetId = static_cast<int>(challengedTarget.NetworkId());
    plan.Context.ChallengedTargetInside = Challenged(challengedTarget) &&
        RSweepHits(player.Position(), challengedTarget.Position(),
                   challengedTarget.BoundingRadius());
    plan.Context.AlliesNearTarget = CountAlliedFollowup(
        challengedTarget.Position(), 850.0f);
    plan.Context.UnderEnemyTurret = Engine::UnderEnemyTurret(player.Position());
    plan.Context.PlayerHealthPercent = player.HealthPercent();
    const float raw = RRawDamage(SpellRank(3), player.BonusAttackDamage(),
                                 player.AP(), challengedTarget.Health());
    plan.Context.ChallengedTargetKillable =
        raw >= challengedTarget.Health() + challengedTarget.AllShield();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        if (RSweepHits(player.Position(), enemy.Position(),
                       enemy.BoundingRadius())) {
            ++plan.SweepHits;
            if (Challenged(enemy)) {
                ++plan.Context.EnemiesRemainingInside;
            } else {
                ++plan.Knockbacks;
                ++plan.Context.NonChallengedInside;
                if (TargetDisplacementImmune(enemy))
                    ++plan.Context.EnemiesRemainingInside;
            }
        } else if (RBlocksDamageSource(player.Position(), enemy.Position(),
                                        enemy.BoundingRadius()) &&
                   player.Position().Distance2D(enemy.Position()) <=
                       std::max(900.0f, enemy.AttackRange() + 250.0f)) {
            ++plan.Context.OutsideDamageThreats;
        }
    }
    plan.Context.OutsideDamageThreats = std::max(
        plan.Context.OutsideDamageThreats, IncomingOutsideThreats);
    plan.Score = IsolationSafetyScore(plan.Context);
    plan.Safe = SafeIsolation(plan.Context,
        static_cast<float>(Slider(RMenu, "IsolationScore", 220)));
    plan.Valid = plan.Context.ChallengedTargetInside && plan.SweepHits > 0;
    return plan;
}

inline bool CastR(UltimateReason reason,
                  Mode mode,
                  const AIHeroClient& target = {},
                  bool reactive = false) {
    if (!Ready(3) || SpellRank(3) <= 0 || !SpellEnabled(3, mode) || RActive ||
        !CastThrottleReady(3, reactive) || !Engine::ControllerCastSelf(3))
        return false;
    RCastTick = Now();
    RExpireTick = RCastTick + kRDurationMs;
    RActive = true;
    LastUltimateReason = reason;
    ActiveSequence = reason == UltimateReason::Isolation ||
                     reason == UltimateReason::Lethal
        ? Sequence::CrescentIsolation : Sequence::CrescentDefense;
    if (Engine::ValidEnemy(target)) LastRPlan = BuildRPlan(target);
    return true;
}

inline bool TryDefensiveR(Mode mode, UltimateReason request) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3) || RActive) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), 650.0f);
    bool cast = false;
    UltimateReason reason = request;
    if (request == UltimateReason::OutsideDamage) {
        cast = IncomingThreatUntil >= Now() && IncomingOutsideThreats > 0 &&
            (IncomingDamage >= player.Health() * 0.20f ||
             player.HealthPercent() <= Slider(RMenu, "OutsideThreatHp", 58));
    } else if (request == UltimateReason::Flee) {
        cast = enemies >= Slider(RMenu, "FleeEnemies", 2) &&
               player.HealthPercent() <= Slider(RMenu, "FleeHp", 48);
    } else {
        cast = enemies >= Slider(RMenu, "DefensiveEnemies", 3) &&
               player.HealthPercent() <= Slider(RMenu, "DefensiveHp", 42);
        reason = UltimateReason::MultiThreat;
    }
    return cast && CastR(reason, mode, {}, true);
}

inline bool TryOffensiveR(const AIHeroClient& target) {
    if (!Ready(3) || !Engine::ValidEnemy(target) || !Challenged(target))
        return false;
    const RPlan plan = BuildRPlan(target);
    LastRPlan = plan;
    if (!plan.Valid) return false;
    if (plan.Context.ChallengedTargetKillable && Bool(RMenu, "Lethal", true))
        return CastR(UltimateReason::Lethal, Mode::Combo, target);
    return Bool(RMenu, "Isolation", true) && plan.Safe &&
           plan.Knockbacks >= Slider(RMenu, "MinimumKnockbacks", 1) &&
           CastR(UltimateReason::Isolation, Mode::Combo, target);
}

inline bool TryInterrupt() {
    if (InterruptExpireTick < Now() || !Ready(3) ||
        !Bool(RMenu, "Interrupt", true)) return false;
    const AIHeroClient target = HeroByNetworkId(InterruptTargetId);
    const auto player = GameObjects::Player();
    if (!Engine::ValidEnemy(target) || Challenged(target) ||
        TargetDisplacementImmune(target) ||
        !RSweepHits(player.Position(), target.Position(),
                    target.BoundingRadius())) return false;
    return CastR(UltimateReason::Interrupt, Mode::Automatic, {}, true);
}

inline bool TryGapcloser(const AIHeroClient& fallback) {
    if (GapcloserExpireTick < Now()) return false;
    AIHeroClient target = HeroByNetworkId(GapcloserTargetId);
    if (!Engine::ValidEnemy(target)) target = fallback;
    const auto player = GameObjects::Player();
    if (!Engine::ValidEnemy(target) || !player.IsValid()) return false;
    const Vector3 end = GapcloserEnd.IsValid() ? GapcloserEnd : target.Position();
    if (Bool(RMenu, "GapcloseSweep", true) && Ready(3) &&
        !Challenged(target) && !TargetDisplacementImmune(target) &&
        RSweepHits(player.Position(), end, target.BoundingRadius()) &&
        player.HealthPercent() <= Slider(RMenu, "GapcloseHp", 55))
        return CastR(UltimateReason::Gapcloser, Mode::Automatic, {}, true);
    return Bool(WMenu, "GapcloseSlow", true) && Ready(1) &&
           CastW(target, Mode::Automatic, true);
}

inline bool TryCombo(const AIHeroClient& target) {
    if (TargetCannotBeDamaged(target)) return false;
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    if (TryDefensiveR(Mode::Automatic, UltimateReason::MultiThreat) ||
        TryOffensiveR(target)) return true;
    if (QActive && QStrikes >= 2) {
        ActiveSequence = Sequence::QKnockupReady;
        return false;
    }
    if (Challenged(target) && !InAttackRange(target, 60.0f) &&
        Bool(EMenu, "UseChallengeE", true) && CastE(target, Mode::Combo))
        return true;
    if (!Challenged(target) &&
        distance > kENormalRange + target.BoundingRadius() &&
        Bool(WMenu, "OpenChallenge", true) && CastW(target, Mode::Combo))
        return true;
    if (!QActive && InAttackRange(target, 45.0f) &&
        Bool(QMenu, "UseCombo", true) &&
        CastQ(Mode::Combo, AIBaseClient(target.Handle()))) return true;
    if (distance > player.AttackRange() + target.BoundingRadius() + 50.0f &&
        Bool(EMenu, "UseNormalE", true) && CastE(target, Mode::Combo))
        return true;
    const bool afterKnockup = LastQKnockupTick > 0 &&
                              Now() - LastQKnockupTick <= 850;
    return Bool(WMenu, "UseCombo", true) && (!QActive || afterKnockup) &&
           CastW(target, Mode::Combo);
}

inline bool TryHarass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (TargetCannotBeDamaged(target) ||
        player.ManaPercent() < Slider(TacticsMenu, "HarassMana", 44) ||
        QActive && QStrikes >= 2) return false;
    if (Bool(WMenu, "UseHarass", true) && CastW(target, Mode::Harass))
        return true;
    if (Bool(EMenu, "UseHarassChallenge", false) && Challenged(target) &&
        CastE(target, Mode::Harass)) return true;
    return Bool(QMenu, "UseHarass", true) && InAttackRange(target) &&
           CastQ(Mode::Harass, AIBaseClient(target.Handle()));
}

inline AIBaseClient BestEscapeChargeUnit(const AIHeroClient& pursuer) {
    const auto player = GameObjects::Player();
    const Vector3 cursor = Game::CursorPos();
    AIBaseClient best{};
    float bestScore = -FLT_MAX;
    auto consider = [&](const AIBaseClient& unit, bool challenged) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable()) return;
        const EPlan plan = BuildEPlan(unit, challenged, true);
        if (!plan.Valid || !plan.Safe) return;
        float score = player.Position().Distance2D(cursor) -
                      plan.Endpoint.Distance2D(cursor);
        if (pursuer.IsValid())
            score += plan.Endpoint.Distance2D(pursuer.Position()) -
                     player.Position().Distance2D(pursuer.Position());
        score += static_cast<float>(
            Engine::CountEnemiesAt(player.Position(), 650.0f) -
            plan.EnemiesAtEndpoint) * 180.0f;
        if (score > bestScore) { bestScore = score; best = unit; }
    };
    for (const auto& minion : GameObjects::EnemyLaneMinions())
        consider(AIBaseClient(minion.Handle()), false);
    for (const auto& monster : GameObjects::Jungle())
        consider(AIBaseClient(monster.Handle()), false);
    for (const auto& enemy : GameObjects::EnemyHeroes())
        consider(AIBaseClient(enemy.Handle()), Challenged(enemy));
    return bestScore >= Slider(EMenu, "EscapeMinimumGain", 120)
        ? best : AIBaseClient{};
}

inline bool TryFlee(const AIHeroClient& selected) {
    const AIHeroClient pursuer = NearestEnemyToPlayer(selected, 1100.0f);
    if (TryDefensiveR(Mode::Flee, UltimateReason::Flee)) return true;
    if (Bool(EMenu, "UseFlee", true)) {
        const AIBaseClient unit = BestEscapeChargeUnit(pursuer);
        if (unit.IsValid()) {
            const bool challenged = unit.IsHero() &&
                Challenged(AIHeroClient(unit.Handle()));
            if (CastE(unit, challenged, Mode::Flee, true)) return true;
        }
    }
    return Engine::ValidEnemy(pursuer) && Bool(WMenu, "UseFlee", true) &&
           CastW(pursuer, Mode::Flee);
}

inline bool CastWAtUnit(const AIBaseClient& target, Mode mode) {
    if (!Ready(1) || !SpellEnabled(1, mode) || !CastThrottleReady(1) ||
        !target.IsValid()) return false;
    const auto player = GameObjects::Player();
    const Vector3 direction = Direction2D(player.Position(), target.Position());
    if (direction.IsZero() || !WThrustHits(player.Position(), direction,
        target.Position(), target.BoundingRadius())) return false;
    return Engine::ControllerCastPosition(
        1, player.Position() + direction * kWRange);
}

inline bool TryJungle() {
    if (!Bool(FarmMenu, "JungleAbilities", true)) return false;
    const AIMinionClient monster =
        ControllerHelpers::SelectJungleTarget(kEChallengeRange, 0.15f);
    if (!monster.IsValid()) return false;
    const AIBaseClient unit(monster.Handle());
    if (Bool(EMenu, "JungleE", true) && CastE(unit, false, Mode::LaneClear)) {
        ActiveSequence = Sequence::JungleChain;
        return true;
    }
    if (Bool(QMenu, "JungleQ", true) && InAttackRange(unit) &&
        CastQ(Mode::LaneClear, unit)) {
        ActiveSequence = Sequence::JungleChain;
        return true;
    }
    return Bool(WMenu, "JungleW", true) &&
           CastWAtUnit(unit, Mode::LaneClear);
}

inline bool TryLaneW(Mode mode) {
    const auto player = GameObjects::Player();
    if (!Ready(1) || !SpellEnabled(1, Mode::LaneClear) ||
        player.ManaPercent() < Slider(FarmMenu, "LaneMana", 58)) return false;
    Vector3 bestAim{};
    int bestHits = 0;
    int bestLastHits = 0;
    for (const auto& anchor : GameObjects::EnemyLaneMinions()) {
        if (!anchor.IsValid() || anchor.IsDead()) continue;
        const Vector3 direction = Direction2D(player.Position(), anchor.Position());
        if (direction.IsZero()) continue;
        int hits = 0;
        int lastHits = 0;
        for (const auto& minion : GameObjects::EnemyLaneMinions()) {
            if (!minion.IsValid() || minion.IsDead() ||
                !WThrustHits(player.Position(), direction, minion.Position(),
                             minion.BoundingRadius())) continue;
            ++hits;
            const bool slash = WSlashHits(player.Position(), direction,
                                           minion.Position(),
                                           minion.BoundingRadius());
            const float raw = WRawDamage(SpellRank(1),
                player.TotalAttackDamage(), player.AP(), 0.0f, slash);
            if (raw >= minion.Health()) ++lastHits;
        }
        const int score = mode == Mode::LastHit
            ? lastHits * 100 + hits : hits * 100 + lastHits;
        const int oldScore = mode == Mode::LastHit
            ? bestLastHits * 100 + bestHits : bestHits * 100 + bestLastHits;
        if (score > oldScore) {
            bestHits = hits;
            bestLastHits = lastHits;
            bestAim = player.Position() + direction * kWRange;
        }
    }
    const int value = mode == Mode::LastHit ? bestLastHits : bestHits;
    const int required = mode == Mode::LastHit
        ? Slider(FarmMenu, "MinimumLastHits", 1)
        : Slider(FarmMenu, "MinimumLaneHits", 3);
    return value >= required && bestAim.IsValid() && CastThrottleReady(1) &&
           Engine::ControllerCastPosition(1, bestAim);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    LastMode = mode;
    AIHeroClient target = selected;
    if (!Engine::ValidEnemy(target)) target = Engine::SelectTarget(1150.0f);
    LastDecisionTargetId = target.IsValid()
        ? static_cast<int>(target.NetworkId()) : 0;
    if (TryInterrupt() ||
        TryDefensiveR(Mode::Automatic, UltimateReason::OutsideDamage) ||
        TryGapcloser(target)) return true;
    if (mode == Mode::Flee) return TryFlee(target);
    if (mode == Mode::Combo) return TryCombo(target);
    if (mode == Mode::Harass) return TryHarass(target);
    if (mode == Mode::LaneClear || mode == Mode::LastHit) {
        if (TryJungle()) return true;
        return Bool(FarmMenu, "LaneW", true) && TryLaneW(mode);
    }
    return mode == Mode::None && TryJungle();
}

inline void RecordCompletedAttack(int targetId,
                                  int tick,
                                  const char* spellName) {
    if (targetId == 0 ||
        (targetId == LastRecordedAutoTargetId && tick >= LastRecordedAutoTick &&
         tick - LastRecordedAutoTick < 70)) return;
    LastRecordedAutoTargetId = targetId;
    LastRecordedAutoTick = tick;
    const bool passiveProc =
        Engine::TextContains(spellName, "XinZhaoPassiveCritAttack");
    PassiveCompletedAttacks = passiveProc
        ? 0 : (PassiveCompletedAttacks + 1) % 3;
    if (passiveProc || PassiveCompletedAttacks == 0) {
        PassiveLastProcTick = tick;
        PassiveLastExpectedHeal = PassiveExpectedHeal();
    }
    const AIHeroClient hero = HeroByNetworkId(targetId);
    if (Engine::ValidEnemy(hero)) SetChallenge(targetId);
    if (!QActive) return;
    QStrikes = Engine::TextContains(spellName, "XinZhaoQThrust3")
        ? 3 : std::min(3, QStrikes + 1);
    if (QStrikes >= 3) {
        QActive = false;
        QExpireTick = 0;
        LastQKnockupTick = tick;
        ActiveSequence = Sequence::QKnockupLanded;
    } else if (QStrikes == 2) {
        ActiveSequence = Sequence::QKnockupReady;
    }
}

inline void ObserveLocalSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const int now = Now();
    const int targetId = static_cast<int>(args.TargetNetworkId != 0
        ? args.TargetNetworkId : args.Target.NetworkId);
    if (args.IsAutoAttack || Engine::TextContains(args.SpellName, "XinZhaoQThrust") ||
        Engine::TextContains(args.SpellName, "XinZhaoPassiveCritAttack")) {
        LastAutoTargetId = targetId;
        LastAutoTick = now;
        RecordCompletedAttack(targetId, now, args.SpellName);
        return;
    }
    if (args.Slot == 0 || Engine::TextContains(args.SpellName, "XinZhaoQ")) {
        QActive = true; QStrikes = 0; QCastTick = now;
        QExpireTick = now + kQDurationMs;
        QTargetId = targetId != 0 ? targetId : LastDecisionTargetId;
        ActiveSequence = Sequence::QResetChain;
    } else if (args.Slot == 1 || Engine::TextContains(args.SpellName, "XinZhaoW")) {
        WCastTick = now;
        WTargetId = targetId != 0 ? targetId : LastDecisionTargetId;
        if (args.StartPosition.IsValid() && args.EndPosition.IsValid())
            LastWDirection = Direction2D(args.StartPosition, args.EndPosition);
        if (WTargetId != 0) SetChallenge(WTargetId);
        ActiveSequence = Sequence::WChallenge;
    } else if (args.Slot == 2 || Engine::TextContains(args.SpellName, "XinZhaoE")) {
        ECastTick = now;
        ETargetId = targetId != 0 ? targetId : LastDecisionTargetId;
        EExpectedArrivalTick = now + 850;
        if (ETargetId != 0) SetChallenge(ETargetId);
        ActiveSequence = Sequence::ChallengeDash;
    } else if (args.Slot == 3 || Engine::TextContains(args.SpellName, "XinZhaoR")) {
        RCastTick = now; RExpireTick = now + kRDurationMs; RActive = true;
        if (!Engine::WasControllerCast(3)) {
            LastUltimateReason = UltimateReason::Manual;
            ActiveSequence = Sequence::CrescentDefense;
        }
    } else return;
    if (!Engine::WasControllerCast(args.Slot)) ManualOwnershipUntil = now + 300;
}

inline void ObserveEnemySpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const auto analysis = AnalyzeEnemyCast(
        args, 220.0f, 110.0f, 210, 250, 180, 1500, 520);
    const auto player = GameObjects::Player();
    if (!analysis.Valid || !player.IsValid() ||
        (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    if (player.Position().Distance2D(analysis.Enemy.Position()) <=
        kRGuardRadius + analysis.Enemy.BoundingRadius()) return;
    float damage = args.IsAutoAttack
        ? SDK::Damage::GetAutoAttackDamage(analysis.Enemy, player, true)
        : (args.Slot >= 0 && args.Slot < 4
            ? SDK::Damage::GetSpellDamage(analysis.Enemy, player,
                Engine::SlotFromIndex(args.Slot), SDK::DamageStage::Default)
            : 0.0f);
    if (!std::isfinite(damage) || damage <= 0.0f)
        damage = 60.0f + analysis.Enemy.TotalAttackDamage() * 0.65f +
                 analysis.Enemy.AP() * 0.40f;
    if (IncomingThreatUntil < Now()) IncomingDamage = 0.0f;
    IncomingDamage += damage;
    ++IncomingOutsideThreats;
    IncomingThreatUntil = Now() + 1250;
    if (Bool(RMenu, "BlockOutsideDamage", true))
        (void)TryDefensiveR(Mode::Automatic, UltimateReason::OutsideDamage);
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) ObserveLocalSpell(args);
    else ObserveEnemySpell(args);
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    int targetId = 0, tick = 0;
    if (CaptureLocalAutoAttack(args, targetId, tick))
        RecordCompletedAttack(targetId, tick, args.SpellName);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || !QActive || QStrikes < 2 ||
        LastMode == Mode::Flee || LastMode == Mode::LaneClear ||
        LastMode == Mode::LastHit) return;
    const AIBaseClient attackTarget(args.Target.Handle());
    const AIHeroClient desired = HeroByNetworkId(
        QTargetId != 0 ? QTargetId : LastDecisionTargetId);
    if (Engine::ValidEnemy(desired) &&
        (!attackTarget.IsHero() || attackTarget.NetworkId() != desired.NetworkId()) &&
        Bool(QMenu, "ProtectThirdStrike", true)) args.Process = false;
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    int targetId = 0, tick = 0;
    if (!CaptureAfterAttack(args, targetId, tick)) return;
    RecordCompletedAttack(targetId, tick, "");
    const AIBaseClient target(args.Target.Handle());
    if (!target.IsValid() || QActive || ManualOwnershipUntil >= Now()) return;
    if ((LastMode == Mode::Combo && Bool(QMenu, "UseCombo", true)) ||
        (LastMode == Mode::Harass && Bool(QMenu, "UseHarass", true)) ||
        ((LastMode == Mode::LaneClear || LastMode == Mode::LastHit) &&
         Bool(QMenu, "LaneQReset", true)))
        (void)CastQ(LastMode, target, true);
}

inline void UpdateBuff(const SDK::Events::BuffEventArgs& args, bool removed) {
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (NameEquals(args.BuffName, "XinZhaoQ") ||
            NameEquals(args.BuffName, "xinzhaoq")) {
            QActive = !removed;
            QExpireTick = removed ? 0 : now + RemainingMilliseconds(
                args.EndTime, kQDurationMs, 0, 5600);
            if (removed && QStrikes < 3) QStrikes = 0;
        } else if (NameEquals(args.BuffName, "XinZhaoRRangedImmunity") ||
                   NameEquals(args.BuffName, "XenZhaoParry") ||
                   NameEquals(args.BuffName, "XinZhaoR")) {
            RActive = !removed;
            RExpireTick = removed ? 0 : now + RemainingMilliseconds(
                args.EndTime, kRDurationMs, 0, 4600);
        } else if (NameEquals(args.BuffName, "XinZhaoEBattleCry") && !removed) {
            EDashActive = false;
        }
        return;
    }
    if (!ChallengeBuffName(args.BuffName) || !args.Sender.IsValid()) return;
    const int targetId = static_cast<int>(args.Sender.NetworkId);
    if (removed && targetId == ChallengeTargetId) {
        ChallengeTargetId = ChallengeExpireTick = 0;
    } else if (!removed) {
        SetChallenge(targetId, RemainingMilliseconds(
            args.EndTime, kChallengeMs, 0, 3600));
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) { UpdateBuff(args, false); }
inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) { UpdateBuff(args, false); }
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) { UpdateBuff(args, true); }

inline void BuildMenu(Menu* root) {
    TacticsMenu = root->AddSubMenu(new Menu(
        "XinZhaoOneTrick", "Xin Zhao one-trick tactics"));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass minimum mana (%)", 44, 0, 100));
    PassiveMenu = TacticsMenu->AddSubMenu(new Menu("Passive", "Determination cadence"));
    PassiveMenu->Add(new MenuSeparator("Heal", "Third attack heals from max HP + AP"));
    PassiveMenu->Add(new MenuSeparator("Ownership", "Safe minion/monster heal procs remain player-owned"));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Three Talon Strike"));
    QMenu->Add(new MenuBool("UseCombo", "Use Q in combo", true));
    QMenu->Add(new MenuBool("UseHarass", "Use Q in harass", true));
    QMenu->Add(new MenuBool("ProtectThirdStrike", "Protect Q3 for chosen champion", true));
    QMenu->Add(new MenuBool("LaneQReset", "Reset lane/jungle attack with Q", true));
    QMenu->Add(new MenuBool("JungleQ", "Use Q on jungle", true));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Wind Becomes Lightning"));
    WMenu->Add(new MenuBool("UseCombo", "Use W in combo", true));
    WMenu->Add(new MenuBool("OpenChallenge", "W before extended E", true));
    WMenu->Add(new MenuBool("UseHarass", "Use W in harass", true));
    WMenu->Add(new MenuBool("GapcloseSlow", "Slow gapcloser", true));
    WMenu->Add(new MenuBool("UseFlee", "W pursuer while fleeing", true));
    WMenu->Add(new MenuBool("JungleW", "Use W on jungle", true));
    WMenu->Add(new MenuSeparator("Geometry", "0.60s piercing thrust; slash is separate"));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Audacious Charge endpoint"));
    EMenu->Add(new MenuBool("UseChallengeE", "Use 1100 challenged E", true));
    EMenu->Add(new MenuBool("UseNormalE", "Use normal 650 E", true));
    EMenu->Add(new MenuBool("UseHarassChallenge", "Use challenged E in harass", false));
    EMenu->Add(new MenuBool("UseFlee", "Charge to escape unit", true));
    EMenu->Add(new MenuSlider("EscapeMinimumGain", "Minimum flee endpoint gain", 120, 0, 500));
    EMenu->Add(new MenuBool("JungleE", "Use E on jungle", true));
    EMenu->Add(new MenuSeparator("Safety", "Endpoint checks cursor, turret, density and dash hazards"));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Crescent Guard isolation"));
    RMenu->Add(new MenuBool("Isolation", "Use safe isolation R", true));
    RMenu->Add(new MenuSlider("IsolationScore", "Minimum isolation safety", 220, 0, 1000));
    RMenu->Add(new MenuSlider("MinimumKnockbacks", "Minimum knockbacks", 1, 1, 5));
    RMenu->Add(new MenuBool("Lethal", "Use lethal R", true));
    RMenu->Add(new MenuBool("BlockOutsideDamage", "Block real outside damage", true));
    RMenu->Add(new MenuSlider("OutsideThreatHp", "Outside-threat R HP (%)", 58, 10, 100));
    RMenu->Add(new MenuSlider("DefensiveEnemies", "Defensive R enemies", 3, 1, 5));
    RMenu->Add(new MenuSlider("DefensiveHp", "Defensive R HP (%)", 42, 10, 100));
    RMenu->Add(new MenuBool("GapcloseSweep", "Sweep gapcloser", true));
    RMenu->Add(new MenuSlider("GapcloseHp", "Gapcloser R HP (%)", 55, 10, 100));
    RMenu->Add(new MenuBool("Interrupt", "Interrupt with R knockback", true));
    RMenu->Add(new MenuSlider("FleeEnemies", "Flee R enemies", 2, 1, 5));
    RMenu->Add(new MenuSlider("FleeHp", "Flee R HP (%)", 48, 10, 100));
    RMenu->Add(new MenuSeparator("Rule", "500 sweep and 450 guard; challenged target stays"));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Farm", "Lane and jungle cadence"));
    FarmMenu->Add(new MenuBool("JungleAbilities", "Use abilities on jungle", true));
    FarmMenu->Add(new MenuBool("LaneW", "Use W on lane", true));
    FarmMenu->Add(new MenuSlider("LaneMana", "Lane W minimum mana (%)", 58, 0, 100));
    FarmMenu->Add(new MenuSlider("MinimumLaneHits", "Minimum W lane hits", 3, 1, 8));
    FarmMenu->Add(new MenuSlider("MinimumLastHits", "Minimum W last hits", 1, 1, 5));
}

inline void OnLoad() {
    ActiveSequence = Sequence::None; LastUltimateReason = UltimateReason::None;
    LastMode = Mode::None; LastDecisionTargetId = ManualOwnershipUntil = 0;
    PassiveCompletedAttacks = PassiveLastProcTick = 0; PassiveLastExpectedHeal = 0.0f;
    QActive = false; QStrikes = QCastTick = QExpireTick = QTargetId = LastQKnockupTick = 0;
    WCastTick = WTargetId = 0; LastWDirection = {}; LastWPlan = {};
    ChallengeTargetId = ChallengeExpireTick = 0;
    ECastTick = ETargetId = EExpectedArrivalTick = 0; EDashActive = false; LastEPlan = {};
    RActive = false; RCastTick = RExpireTick = 0; LastRPlan = {};
    LastAutoTargetId = LastAutoTick = LastRecordedAutoTargetId = LastRecordedAutoTick = 0;
    IncomingThreatUntil = IncomingOutsideThreats = 0; IncomingDamage = 0.0f;
    GapcloserTargetId = GapcloserExpireTick = 0; GapcloserEnd = {};
    InterruptTargetId = InterruptExpireTick = 0;
    ReconcileState();
}

inline void OnUnload() {
    TacticsMenu = PassiveMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Track Determination on every completed basic attack and proc on each third hit",
    "Reconcile passive cadence without double-counting DoCast and AfterAttack",
    "Estimate passive healing from maximum health and AP level breakpoints",
    "Allow a low-health heal proc on a safe minion or monster",
    "Use Q as an attack reset after a completed orbwalker attack",
    "Track all three Q attacks from special-attack spell events",
    "Reconcile a missing Q event from the live XinZhaoQ buff",
    "Protect Q3 from a minion or wrong champion",
    "Release Q3 protection in lane, last hit and flee modes",
    "Never cancel a valuable attack windup merely to cast Q",
    "Use current Q bonus damage and one-second-per-hit cooldown refund",
    "Predict W at its full 0.60-second slash-thrust lockout",
    "Aim W as a piercing line without invented minion collision",
    "Apply target radius to W's narrow thrust",
    "Treat W slash as a separate 475 by 260 region",
    "Count additional champions pierced by the W direction",
    "Use current slash and thrust damage formulas independently",
    "Open W when a target is outside normal E but inside challenged E",
    "Track Challenge from W, attacks, E and target buff events",
    "Measure E reach to collision-circle entry rather than target center",
    "Use 650 normal E and 1100 E only against challenged targets",
    "Predict E target before computing its radius-adjusted endpoint",
    "Reject E into terrain, turret, anti-dash hazard or unsupported density",
    "Reject automatic E opposite the player's cursor intent",
    "Leave manual E authoritative while reconciling Challenge",
    "Preserve orbwalker ownership of the Q3 attack",
    "Model R's 500 sweep separately from its 450 guard circle",
    "Keep the challenged target while counting non-challenged knockbacks",
    "Reject isolation when the intended target is not challenged",
    "Penalize displacement-immune enemies that remain after R",
    "Require follow-up and safety for ordinary isolation R",
    "Reject ordinary R isolation under an enemy turret",
    "Use current R base, bonus AD, AP and current-health damage",
    "Cast R for meaningful damage sourced outside the guard circle",
    "Do not defensive-R for a source already inside the guard",
    "Cast reactive R directly from the hostile spell event",
    "Sweep only a non-challenged gapcloser or interrupt target",
    "Never claim R interrupts the challenged target it preserves",
    "Search champions, lane minions and monsters for a safe flee E",
    "Use W on a pursuer when no safe flee charge exists",
    "Use W poke in harass without forcing E by default",
    "Respect harass and lane mana gates",
    "Use jungle Q, W and E only through explicit toggles",
    "Count piercing W lane hits and predicted last hits",
    "Preserve manual Q, W, E and R in the same state machine",
    "Poll Q, dash, Challenge and R state to repair missed events",
    "Never issue movement, attacks, Flash or force-target ownership",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "XinZhao";
    controller.ControllerId = "champion.kuroaio.ai.xinzhao.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIXinZhao.md";
    controller.ImplementationSummary =
        "Third-hit passive healing/damage cadence, event-reconciled Q reset and "
        "knockup ownership, dual-region piercing W prediction, challenged "
        "650/1100 E with radius-correct safe landing, and 500-sweep/450-guard "
        "R isolation, ranged-defense, peel and interrupt logic.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &OnDoCast;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &OnBuffUpdate;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<
        &GapcloserTargetId, &GapcloserEnd, &GapcloserExpireTick, 500, 1100>;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<
        &InterruptTargetId, &InterruptExpireTick, 650, 120, 2200>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::XinZhao
