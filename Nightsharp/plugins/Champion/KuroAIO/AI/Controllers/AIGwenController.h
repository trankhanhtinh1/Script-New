#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIGwenGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Gwen {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MaximumBuffCount;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Ready;
using ControllerHelpers::RemainingMilliseconds;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* SnipMenu = nullptr;
inline Menu* MistMenu = nullptr;
inline Menu* DashMenu = nullptr;
inline Menu* NeedleMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline SnipStackState Snips = {};
inline MistState Mist = {};
inline RecastState Needles = {};
inline bool EEmpowered = false;
inline int EEmpoweredUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastStackAttackTick = 0;
inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastDecisionTargetId = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingOutsideThreatUntil = 0;
inline int IncomingThreatId = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline Vector3 LastQAim = {};
inline Vector3 LastEEndpoint = {};
inline Vector3 LastRAim = {};

inline bool TargetProtected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        target.HasBuff("SivirE") || target.HasBuff("NocturneShroudofDarkness") ||
        target.HasBuff("BansheesVeil") || target.HasBuff("EdgeOfNight") ||
        target.HasBuff("VladimirSanguinePool") || target.HasBuff("FizzEIcon") ||
        target.HasBuff("FioraW") || target.HasBuff("KayleR") ||
        target.HasBuff("kindredrnodeathbuff");
}

inline float ShieldedHealth(const AIHeroClient& target) {
    return target.IsValid() ? target.Health() + target.AllShield() : FLT_MAX;
}

inline float QDamageTo(const AIHeroClient& target, bool center) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const auto split = QDamage(SpellRank(0), Snips.Stacks, player.AP(),
                               target.MaxHealth(), center, false);
    return player.CalculateMagicDamage(target, split.Magical + split.PassiveMagical) + split.True;
}

inline float RDamageTo(const AIHeroClient& target, int castNumber) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float magic = RCastRawDamage(SpellRank(3), castNumber, player.AP()) +
        RCastPassiveRawDamage(castNumber, target.MaxHealth(), player.AP(), false);
    return player.CalculateMagicDamage(target, magic);
}

inline float EAttackDamageTo(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return SDK::Damage::GetAutoAttackDamage(player, target, true) +
        player.CalculateMagicDamage(target, EOnHitRawDamage(player.AP()) +
            PassiveRawDamage(target.MaxHealth(), player.AP()));
}

inline bool Lethal(const AIHeroClient& target, float damage) {
    return Engine::ValidEnemy(target) && !TargetProtected(target) &&
           damage + 1.0f >= ShieldedHealth(target);
}

inline bool EnoughMana(int index, float reserve = 0.0f,
                       bool defensive = false, bool lethal = false) {
    return HasMana({ CurrentResource(), SpellCost(index), reserve, defensive, lethal });
}

inline bool CanUse(int index, Mode mode, bool reactive = false,
                   float reserve = 0.0f, bool lethal = false) {
    return Ready(index) && SpellEnabled(index, mode) &&
           CastThrottleReady(index, 42, reactive ? 0 : -1) &&
           EnoughMana(index, reserve, reactive, lethal);
}

inline bool RecentAttackOn(const AIBaseClient& target, int windowMs = 430) {
    return target.IsValid() && LastAutoTargetId == static_cast<int>(target.NetworkId()) &&
           Now() - LastAutoTick <= windowMs;
}

inline void ObserveAttack(int targetId, int tick) {
    LastAutoTargetId = targetId;
    LastAutoTick = tick;
    if (tick - LastStackAttackTick >= 80) {
        Snips.AddAttack(tick);
        LastStackAttackTick = tick;
    }
    if (EEmpowered) EEmpowered = false;
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    const int observedSnips = MaximumBuffCount(player, { "GwenQStacker", "gwenqstacker" });
    const bool snipBuff = player.HasBuff("GwenQStacker") || player.HasBuff("gwenqstacker");
    Snips.Poll(now, observedSnips, snipBuff);
    const bool mistBuff = player.HasBuff("GwenW_GwenInsideW") ||
                          player.HasBuff("GwenW") || player.HasBuff("gwenw_gweninsidew");
    if (mistBuff && !Mist.Active) Mist.Begin(player.Position(), now);
    Mist.Poll(now, mistBuff);
    EEmpowered = player.HasBuff("GwenEAttackBuff") || player.HasBuff("gweneattackbuff") ||
                 (EEmpowered && now < EEmpoweredUntil);
    if (now >= EEmpoweredUntil && !player.HasBuff("GwenEAttackBuff") &&
        !player.HasBuff("gweneattackbuff")) EEmpowered = false;
    const bool recastBuff = player.HasBuff("GwenRRecast") || player.HasBuff("gwenrrecast");
    Needles.Poll(now, recastBuff);
    if (IncomingOutsideThreatUntil < now) IncomingThreatId = 0;
    if (GapcloserExpireTick < now) {
        GapcloserTargetId = 0;
        GapcloserEndpoint = {};
    }
}

inline AIHeroClient CooperatingTarget(const AIHeroClient& selected) {
    if (Engine::ValidEnemy(selected)) return selected;
    const AIHeroClient remembered = HeroByNetworkId(LastDecisionTargetId);
    if (Needles.CastNumber > 0 && Engine::ValidEnemy(remembered, kRGameplayRange + 100.0f))
        return remembered;
    return Engine::SelectTarget(Needles.CastNumber > 0 ? kRGameplayRange + 100.0f : 750.0f);
}

inline bool IncomingSourceOutsideMist() {
    if (!Mist.Active || IncomingOutsideThreatUntil < Now()) return false;
    const AIHeroClient source = HeroByNetworkId(IncomingThreatId);
    return !source.IsValid() || ThreatBlockedByMist(source.Position(),
        GameObjects::Player().Position(), Mist.Center, source.BoundingRadius());
}

inline bool CastMist(Mode mode, bool reactive, const Vector3& desired = {}) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !CanUse(1, mode, reactive)) return false;
    if (!Mist.Active) {
        if (!reactive && mode != Mode::Combo && mode != Mode::Flee) return false;
        if (!reactive && Engine::CountEnemiesAt(player.Position(), 850.0f) <= 0) return false;
        if (!Engine::ControllerCastSelf(1)) return false;
        LastWCastTick = Now();
        Mist.Begin(player.Position(), LastWCastTick);
        return true;
    }
    Vector3 wanted = desired;
    if (!wanted.IsValid() || wanted.IsZero()) wanted = Game::CursorPos();
    if (!ShouldRecenterMist(player.Position(), Mist.Center, wanted,
            Mist.RecastAvailable, IncomingSourceOutsideMist(),
            static_cast<float>(Slider(MistMenu, "BoundaryReserve", 55)))) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastWCastTick = Now();
    Mist.Reposition(player.Position(), LastWCastTick);
    return true;
}

inline Vector3 PredictedTarget(const AIHeroClient& target, float seconds) {
    const Vector3 predicted = PredictPosition(target, seconds);
    return predicted.IsValid() && !predicted.IsZero() ? predicted : target.Position();
}

inline bool CastQ(const AIHeroClient& target, Mode mode,
                  bool reactive = false, bool force = false) {
    if (!Engine::ValidEnemy(target, kQGameplayRange + 90.0f) || TargetProtected(target)) return false;
    const bool centerLethal = Lethal(target, QDamageTo(target, true));
    const float reserve = mode == Mode::Harass
        ? static_cast<float>(Slider(SnipMenu, "HarassReserve", 70)) : 0.0f;
    if (!CanUse(0, mode, reactive, reserve, centerLethal)) return false;
    if (!reactive && Orbwalker::IsWindingUp() && !centerLethal) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vector3 predicted = prediction.GetCastPosition().IsValid() &&
        !prediction.GetCastPosition().IsZero() ? prediction.GetCastPosition() :
        PredictedTarget(target, kQCastSeconds);
    const QHit hit = EvaluateQHit(GameObjects::Player().Position(), predicted,
                                  predicted, target.BoundingRadius());
    if (!hit.Outer || !hit.Center) return false;
    if (prediction.Hitchance < SDK::HitChance::High &&
        !Engine::IsHardCrowdControlled(target) && !centerLethal) return false;
    const bool stacked = Snips.Stacks >= Slider(SnipMenu, "ComboStacks", 3);
    const bool expiring = Snips.Stacks > 0 && Snips.ExpireTick - Now() <=
        Slider(SnipMenu, "SpendBeforeExpireMs", 800);
    const bool committed = target.IsDashing() || Engine::IsHardCrowdControlled(target) ||
                           RecentAttackOn(target, 380);
    if (!force && !reactive && !centerLethal && !stacked && !expiring && !committed) return false;
    if (Engine::UnderEnemyTurret(GameObjects::Player().Position()) && !centerLethal &&
        !Bool(Engine::ComboMenu, "AllowTurretDive", false)) return false;
    if (!Engine::ControllerCastPosition(0, predicted)) return false;
    LastQCastTick = Now();
    LastQAim = predicted;
    Snips.Spend();
    return true;
}

inline bool EndpointSafe(const Vector3& endpoint, const AIHeroClient& target,
                         bool lethal, bool fleeing) {
    const auto player = GameObjects::Player();
    DashSafety safety{};
    safety.EndpointValid = endpoint.IsValid() && !endpoint.IsZero();
    safety.TerrainBlocked = SDK::NavMesh::IsWall(endpoint);
    safety.NewEnemyTurret = Engine::UnderEnemyTurret(endpoint) &&
                            !Engine::UnderEnemyTurret(player.Position());
    safety.DashHazard = HasReadyDashHazardAt(endpoint);
    safety.PointClickThreat = HasReadyPointClickThreatAt(endpoint);
    safety.Lethal = lethal && Bool(DashMenu, "AllowLethalDive", false);
    safety.Fleeing = fleeing;
    safety.EnemiesAtEndpoint = Engine::CountEnemiesAt(endpoint, 575.0f);
    safety.AlliesAtEndpoint = Engine::CountAlliesAt(endpoint, 700.0f);
    safety.MaximumEnemies = Slider(DashMenu, "MaxEndpointEnemies", 2);
    if (Engine::ValidEnemy(target)) {
        safety.KeepsTargetInAttackRange = endpoint.Distance2D(
            PredictedTarget(target, kEDashRange / kEDashSpeed)) <=
            ControllerHelpers::AutoAttackRange(target, EAttackRangeBonus(SpellRank(2))) + 35.0f;
    } else safety.KeepsTargetInAttackRange = fleeing;
    return SafeDash(safety);
}

inline bool CastE(const AIHeroClient& target, Mode mode,
                  bool fleeing = false, bool force = false) {
    if (!CanUse(2, mode, fleeing) || PlayerMobilityLocked()) return false;
    const auto player = GameObjects::Player();
    Vector3 desired = fleeing ? Game::CursorPos() : Vector3{};
    bool lethal = false;
    if (!fleeing) {
        if (!Engine::ValidEnemy(target, 850.0f) || TargetProtected(target)) return false;
        lethal = Lethal(target, EAttackDamageTo(target) + QDamageTo(target, true));
        desired = PredictedTarget(target, kEDashRange / kEDashSpeed);
        if (InAutoAttackRange(target, EEmpowered ? EAttackRangeBonus(SpellRank(2)) : 0.0f)) {
            const Vec3 direction = SharedGeometry::Direction2D(player.Position(), desired);
            const Vec3 cursorDirection = SharedGeometry::Direction2D(player.Position(), Game::CursorPos());
            if (!cursorDirection.IsZero() && cursorDirection.Dot(direction) > -0.1f)
                desired = player.Position() + cursorDirection * 120.0f;
        }
    }
    const Vector3 endpoint = ClampDashEndpoint(player.Position(), desired);
    if (!EndpointSafe(endpoint, target, lethal, fleeing)) return false;
    if (!force && !fleeing && !lethal && !RecentAttackOn(target, 420) &&
        InAutoAttackRange(target)) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    LastECastTick = Now();
    LastEEndpoint = endpoint;
    EEmpowered = true;
    EEmpoweredUntil = LastECastTick + static_cast<int>(kEBuffSeconds * 1000.0f);
    return true;
}

inline std::vector<LineBody> NeedleBodies(float delaySeconds) {
    std::vector<LineBody> bodies;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kRGameplayRange + 160.0f)) continue;
        bodies.push_back({ PredictedTarget(enemy, delaySeconds), enemy.BoundingRadius(),
                           static_cast<int>(enemy.NetworkId()), true });
    }
    return bodies;
}

inline bool CastR(const AIHeroClient& target, Mode mode,
                  bool defensive = false, bool force = false) {
    if (!Engine::ValidEnemy(target, kRGameplayRange + 100.0f) || TargetProtected(target)) return false;
    const int now = Now();
    const int castNumber = Needles.NextCastNumber();
    const bool recast = Needles.CastNumber > 0;
    if (recast && !Needles.CanCast(now)) return false;
    const bool lethal = Lethal(target, RDamageTo(target, castNumber));
    if (!CanUse(3, mode, recast, 0.0f, lethal)) return false;
    const float delay = recast ? kRRecastSeconds : kRFirstCastSeconds;
    const auto prediction = Engine::RuntimeSpells[3]->GetPrediction(target);
    const Vector3 predicted = prediction.GetCastPosition().IsValid() &&
        !prediction.GetCastPosition().IsZero() ? prediction.GetCastPosition() :
        PredictedTarget(target, delay);
    const NeedleLine line = EvaluateNeedleLine(GameObjects::Player().Position(), predicted,
        NeedleBodies(delay), static_cast<int>(target.NetworkId()));
    const bool expiring = recast && Needles.ExpireTick - now <=
        Slider(NeedleMenu, "ForceRecastBeforeMs", 850);
    RPolicy policy{};
    policy.Ready = true;
    policy.HitsPrimary = line.HitsPrimary;
    policy.HitchanceGood = prediction.Hitchance >= SDK::HitChance::High ||
                           Engine::IsHardCrowdControlled(target) || expiring;
    policy.TargetProtected = TargetProtected(target);
    policy.AttackWindingUp = Orbwalker::IsWindingUp();
    policy.Lethal = lethal;
    policy.Defensive = defensive;
    policy.RecastExpiring = expiring;
    policy.CastNumber = castNumber;
    policy.ChampionHits = line.ChampionHits;
    policy.MinimumFirstCastHits = Slider(NeedleMenu, "FirstCastHits", 2);
    if (!force && !MayCastNeedles(policy)) return false;
    if (!Engine::ControllerCastPosition(3, predicted)) return false;
    LastRCastTick = now;
    LastRAim = predicted;
    LastDecisionTargetId = static_cast<int>(target.NetworkId());
    if (!recast) Needles.Begin(now); else Needles.Advance(now);
    return true;
}

inline bool TryDefensive(const AIHeroClient& threat, Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool outside = IncomingSourceOutsideMist();
    const bool gap = GapcloserExpireTick >= Now() && Engine::ValidEnemy(threat, 750.0f);
    const bool low = player.HealthPercent() <= Slider(TacticsMenu, "DefensiveHP", 36);
    if (!(outside || gap || low)) return false;
    if (Mist.Active && CastMist(mode, true, Game::CursorPos())) return true;
    if (!Mist.Active && outside && CastMist(mode, true)) return true;
    if (gap && CastQ(threat, mode, true, true)) return true;
    if (gap && CastE(threat, Mode::Flee, true, true)) return true;
    return Engine::ValidEnemy(threat) && CastR(threat, mode, true, false);
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target) || TargetProtected(target)) return false;
    if (Lethal(target, QDamageTo(target, true)) && CastQ(target, mode, true, true)) return true;
    if (Lethal(target, RDamageTo(target, Needles.NextCastNumber())) &&
        CastR(target, mode, false, true)) return true;
    if (Lethal(target, EAttackDamageTo(target) + QDamageTo(target, true)) &&
        CastE(target, mode, false, true)) return true;
    return false;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Needles.CastNumber > 0 && CastR(target, Mode::Combo)) return true;
    const float distance = GameObjects::Player().Position().Distance2D(target.Position());
    if (distance > 525.0f && CastR(target, Mode::Combo)) return true;
    if (Snips.Stacks >= Slider(SnipMenu, "ComboStacks", 3) && CastQ(target, Mode::Combo)) return true;
    if (!InAutoAttackRange(target, EEmpowered ? EAttackRangeBonus(SpellRank(2)) : 0.0f) &&
        CastE(target, Mode::Combo)) return true;
    if (RecentAttackOn(target) && CastQ(target, Mode::Combo)) return true;
    if (RecentAttackOn(target) && CastE(target, Mode::Combo)) return true;
    if (target.HealthPercent() <= Slider(NeedleMenu, "SingleTargetHP", 48) &&
        CastR(target, Mode::Combo)) return true;
    return false;
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || CurrentResource() < Slider(SnipMenu, "HarassReserve", 70))
        return false;
    if (Snips.Stacks >= Slider(SnipMenu, "HarassStacks", 4) && CastQ(target, Mode::Harass)) return true;
    return RecentAttackOn(target) && !Engine::UnderEnemyTurret(target.Position()) &&
           CastE(target, Mode::Harass);
}

inline bool CastFarmQ(const AIBaseClient& anchor, Mode mode, int hits, bool exactLastHit) {
    if (!anchor.IsValid() || !CanUse(0, mode, false,
            static_cast<float>(Slider(FarmMenu, "ManaReserve", 90)))) return false;
    const int required = mode == Mode::Jungle ? Slider(FarmMenu, "JungleQHits", 1) :
                                                Slider(FarmMenu, "LaneQHits", 3);
    if (!exactLastHit && hits < required) return false;
    if (!Engine::ControllerCastPosition(0, anchor.Position())) return false;
    LastQCastTick = Now(); LastQAim = anchor.Position(); Snips.Spend();
    return true;
}

inline bool TryFarm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Engine::CountEnemiesAt(player.Position(), 1000.0f) > 0) return false;
    const bool jungle = mode == Mode::Jungle;
    AIBaseClient best{};
    int bestHits = 0;
    bool bestLastHit = false;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable() ||
            minion.IsJungle() != jungle || player.Position().Distance2D(minion.Position()) >
                kQGameplayRange + minion.BoundingRadius()) continue;
        int hits = 0;
        for (const auto& other : GameObjects::EnemyMinions()) {
            if (!other.IsValid() || other.IsDead() || !other.IsTargetable() ||
                other.IsJungle() != jungle) continue;
            if (EvaluateQHit(player.Position(), minion.Position(), other.Position(),
                             other.BoundingRadius()).Outer) ++hits;
        }
        const auto split = QDamage(SpellRank(0), Snips.Stacks, player.AP(),
                                   minion.MaxHealth(), true, jungle);
        const float dealt = player.CalculateMagicDamage(minion,
            split.Magical + split.PassiveMagical) + split.True;
        const bool lastHit = dealt + 1.0f >= minion.Health();
        if (hits > bestHits || (lastHit && !bestLastHit)) {
            best = minion; bestHits = hits; bestLastHit = lastHit;
        }
    }
    if (mode == Mode::LastHit && !bestLastHit) return false;
    if (best.IsValid() && CastFarmQ(best, mode, bestHits, bestLastHit)) return true;
    if (jungle && best.IsValid() && RecentAttackOn(best) && Bool(FarmMenu, "JungleE", true)) {
        const Vector3 endpoint = ClampDashEndpoint(player.Position(), Game::CursorPos());
        if (CanUse(2, mode) && !SDK::NavMesh::IsWall(endpoint) &&
            !Engine::UnderEnemyTurret(endpoint) && Engine::ControllerCastPosition(2, endpoint)) {
            LastECastTick = Now(); LastEEndpoint = endpoint; EEmpowered = true;
            EEmpoweredUntil = Now() + static_cast<int>(kEBuffSeconds * 1000.0f);
            return true;
        }
    }
    return false;
}

inline bool TryFlee(const AIHeroClient& threat) {
    if (Mist.Active && CastMist(Mode::Flee, true, Game::CursorPos())) return true;
    if (!Mist.Active && CastMist(Mode::Flee, true)) return true;
    if (CastE(threat, Mode::Flee, true, true)) return true;
    return Engine::ValidEnemy(threat) && CastR(threat, Mode::Flee, true, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const AIHeroClient target = CooperatingTarget(selected);
    if (Engine::ValidEnemy(target)) LastDecisionTargetId = static_cast<int>(target.NetworkId());
    const AIHeroClient threat = NearestEnemyToPlayer(target, 1250.0f);
    if (mode == Mode::Flee) { (void)TryFlee(threat); return true; }
    if (TryDefensive(threat, mode)) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: (void)TryCombo(target); break;
    case Mode::Harass: (void)TryHarass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)TryFarm(mode); break;
    case Mode::Automatic: {
        const bool expiring = Needles.CastNumber > 0 && Needles.ExpireTick - Now() <=
            Slider(NeedleMenu, "ForceRecastBeforeMs", 850);
        const bool lethal = Engine::ValidEnemy(target) &&
            (Lethal(target, QDamageTo(target, true)) ||
             Lethal(target, RDamageTo(target, Needles.NextCastNumber())));
        if (AutomaticAllowed({ IncomingSourceOutsideMist(), GapcloserExpireTick >= Now(),
                               lethal, expiring, false })) {
            if (expiring && Engine::ValidEnemy(target))
                (void)CastR(target, Mode::Automatic, false, true);
            else (void)TryKillSecure(target, Mode::Automatic);
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
        const auto analysis = ControllerHelpers::AnalyzeEnemyCast(
            args, 240.0f, 120.0f, 250, 260, 260, 1800, 450);
        if (!analysis.Valid || !analysis.CrossesPlayer) return;
        const AIHeroClient source = HeroByNetworkId(static_cast<int>(args.Sender.NetworkId));
        const bool outside = !Mist.Active || !source.IsValid() ||
            !InsideMist(source.Position(), Mist.Center, source.BoundingRadius());
        if (outside) {
            IncomingOutsideThreatUntil = now + 850;
            IncomingThreatId = static_cast<int>(args.Sender.NetworkId);
        }
        return;
    }
    if (args.IsAutoAttack) {
        int id = 0, tick = 0;
        if (CaptureLocalAutoAttack(args, id, tick)) ObserveAttack(id, tick);
        return;
    }
    const int slot = args.Slot;
    if (slot < 0 || slot >= 4) return;
    const bool owned = Engine::WasControllerCast(slot);
    if (!owned) ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 520);
    if (slot == 0) {
        LastQCastTick = now; Snips.Spend();
        if (args.EndPosition.IsValid() && !args.EndPosition.IsZero()) LastQAim = args.EndPosition;
    } else if (slot == 1) {
        LastWCastTick = now;
        if (Engine::TextContains(args.SpellName, "Recast") || Mist.Active)
            Mist.Reposition(player.Position(), now);
        else Mist.Begin(player.Position(), now);
    } else if (slot == 2) {
        LastECastTick = now; EEmpowered = true;
        EEmpoweredUntil = now + static_cast<int>(kEBuffSeconds * 1000.0f);
        LastEEndpoint = ClampDashEndpoint(player.Position(), args.EndPosition);
    } else if (slot == 3) {
        LastRCastTick = now;
        if (Engine::TextContains(args.SpellName, "Recast") || Needles.CastNumber > 0)
            Needles.Advance(now); else Needles.Begin(now);
        if (args.EndPosition.IsValid() && !args.EndPosition.IsZero()) LastRAim = args.EndPosition;
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    int id = 0, tick = 0;
    if (CaptureLocalAutoAttack(args, id, tick)) ObserveAttack(id, tick);
}

inline void UpdateBuff(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (Engine::TextContains(args.BuffName, "GwenQStacker")) {
        if (added) Snips.Observe(args.Count, now + RemainingMilliseconds(
            args.EndTime, kSnipStackDurationMs, 100, 7000));
        else Snips.Spend();
    } else if (Engine::TextContains(args.BuffName, "GwenW")) {
        if (added) {
            if (!Mist.Active) Mist.Begin(GameObjects::Player().Position(), now);
            Mist.ExpireTick = now + RemainingMilliseconds(args.EndTime,
                static_cast<int>(kMistDurationSeconds * 1000.0f), 100, 5000);
        } else Mist.Clear();
    } else if (Engine::TextContains(args.BuffName, "GwenEAttackBuff")) {
        EEmpowered = added;
        EEmpoweredUntil = added ? now + RemainingMilliseconds(args.EndTime,
            static_cast<int>(kEBuffSeconds * 1000.0f), 100, 5000) : 0;
    } else if (Engine::TextContains(args.BuffName, "GwenRRecast")) {
        if (added && Needles.CastNumber <= 0) Needles.Begin(now);
        if (!added && Needles.CastNumber >= 3) Needles.Clear();
        if (added) Needles.ExpireTick = now + RemainingMilliseconds(args.EndTime,
            static_cast<int>(kRRecastWindowSeconds * 1000.0f), 100, 7000);
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    int id = 0, tick = 0;
    if (CaptureAfterAttack(args, id, tick)) ObserveAttack(id, tick);
}

inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawPlans", false)) return;
    Drawing::DrawCircle(player.Position(), kQGameplayRange, 0xFF72D8FFu, 1.5f, 42);
    if (Mist.Active) {
        Drawing::DrawCircle(Mist.Center, kMistRadius, 0xFFC991FFu, 2.0f, 52);
        Drawing::DrawLine(player.Position(), Mist.Center, 0xFFC991FFu, 1.4f);
    }
    if (!LastEEndpoint.IsZero()) {
        Drawing::DrawLine(player.Position(), LastEEndpoint, 0xFF5BE0B3u, 2.0f);
        Drawing::DrawCircle(LastEEndpoint, 45.0f, 0xFF5BE0B3u, 1.5f, 24);
    }
    if (!LastQAim.IsZero()) Drawing::DrawLine(player.Position(), LastQAim, 0xFF72D8FFu, 2.0f);
    if (!LastRAim.IsZero()) Drawing::DrawLine(player.Position(), LastRAim, 0xFFC991FFu, 2.0f);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("GwenOneTrick", "Gwen scissors and mist mechanics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 520, 180, 1200));
    TacticsMenu->Add(new MenuSlider("DefensiveHP", "Defensive rotation HP (%)", 36, 10, 75));
    SnipMenu = TacticsMenu->AddSubMenu(new Menu("GwenSnips", "Snip Snip! stack and center policy"));
    SnipMenu->Add(new MenuSlider("ComboStacks", "Combo Q minimum stacks", 3, 0, 4));
    SnipMenu->Add(new MenuSlider("HarassStacks", "Harass Q minimum stacks", 4, 1, 4));
    SnipMenu->Add(new MenuSlider("SpendBeforeExpireMs", "Spend stack before expiry (ms)", 800, 150, 1800));
    SnipMenu->Add(new MenuSlider("HarassReserve", "Harass mana reserve", 70, 0, 220));
    MistMenu = TacticsMenu->AddSubMenu(new Menu("GwenMist", "Hallowed Mist boundary control"));
    MistMenu->Add(new MenuSlider("BoundaryReserve", "Recenter at boundary reserve", 55, 20, 140));
    DashMenu = TacticsMenu->AddSubMenu(new Menu("GwenDash", "Skip 'n Slash endpoint safety"));
    DashMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum enemies at E endpoint", 2, 1, 5));
    DashMenu->Add(new MenuBool("AllowLethalDive", "Allow verified lethal E turret dive", false));
    NeedleMenu = TacticsMenu->AddSubMenu(new Menu("GwenNeedles", "Needlework recast control"));
    NeedleMenu->Add(new MenuSlider("FirstCastHits", "Ordinary R1 minimum champions", 2, 1, 5));
    NeedleMenu->Add(new MenuSlider("SingleTargetHP", "Single-target R1 HP threshold", 48, 10, 100));
    NeedleMenu->Add(new MenuSlider("ForceRecastBeforeMs", "Force usable recast before expiry", 850, 250, 1800));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("GwenFarm", "Stack-aware scissors farming"));
    FarmMenu->Add(new MenuSlider("ManaReserve", "Farm mana reserve", 90, 0, 260));
    FarmMenu->Add(new MenuSlider("LaneQHits", "Lane Q minimum hits", 3, 1, 7));
    FarmMenu->Add(new MenuSlider("JungleQHits", "Jungle Q minimum hits", 1, 1, 5));
    FarmMenu->Add(new MenuBool("JungleE", "E reset after jungle attack", true));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("GwenCoach", "Center, boundary and endpoint drawing"));
    CoachMenu->Add(new MenuBool("DrawPlans", "Draw Q/W/E/R plans", false));
}

inline void OnLoad() {
    Snips = {}; Mist = {}; Needles = {}; EEmpowered = false; EEmpoweredUntil = 0;
    LastAutoTargetId = LastAutoTick = LastStackAttackTick = 0;
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastDecisionTargetId = ManualOwnershipUntil = 0;
    IncomingOutsideThreatUntil = IncomingThreatId = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = LastQAim = LastEEndpoint = LastRAim = {};
    ReconcileState();
}
inline void OnUnload() {
    TacticsMenu = SnipMenu = MistMenu = DashMenu = NeedleMenu = FarmMenu = CoachMenu = nullptr;
    Snips = {}; Mist = {}; Needles = {};
}

inline constexpr const char* Scenarios[] = {
    "Pin Gwen to Riot 26.15 and CommunityDragon PC 16.15",
    "Track zero through four Q stacks from buff events and polling",
    "Add completed attacks without double-counting event routes",
    "Expire and consume the six-second Q stack state",
    "Model one base mini-snip plus one per stack and the final snip",
    "Use live Q rank bases and AP ratios",
    "Apply Thousand Cuts once per Q snip at Q effectiveness",
    "Cap passive monster damage with live data",
    "Require Q's narrow 60-wide center on the predicted target",
    "Convert half of centered Q spell damage to true damage",
    "Keep passive magic damage outside Q true conversion",
    "Preserve attack windup and stack economy before Q",
    "Track W from casts, buff events and polling",
    "Use the live 370-unit mist gameplay boundary",
    "Identify and react only to enemy spell sources outside mist",
    "Unlock one W reposition after the half-second delay",
    "Recenter mist when Gwen approaches its boundary",
    "Clamp E endpoints to 350 units and predict at 800 speed",
    "Reject E terrain, new turret, dash hazard and point-click danger",
    "Reject outnumbered E endpoints without allied cooperation",
    "Require offensive E to retain empowered attack reach",
    "Use live E buff, attack speed, refund and on-hit data",
    "Use R range 1200, width 120, speed 1800 and piercing lines",
    "Track R1 R2 R3 from casts, buff events and polling",
    "Model one three and five needles with one passive proc each",
    "Require primary hit and prediction on each R cast",
    "Hold ordinary single-target R1 but preserve committed recasts",
    "Force a valid recast before the six-second window expires",
    "Retain selected or remembered target through recasts",
    "Yield after manual Q W E or R without duplicate routes",
    "Respect mana reserves, target protection and mitigated lethal checks",
    "Cover Combo Harass LaneClear Jungle LastHit Flee and Automatic modes",
    "Automatic mode permits only defense gapcloser lethal or expiring recast",
    "Draw actual Q aim W center E endpoint and R line",
    "Own Gwen's full loop without generic fallback",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Gwen";
    controller.ControllerId = "champion.kuroaio.ai.gwen.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIGwen.md";
    controller.ImplementationSummary =
        "Event-and-poll repaired Snip stacks, narrow centered Q, outside-source mist "
        "protection and boundary reposition, turret-safe E endpoints, and 1/3/5 R recasts.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &OnDoCast;
    controller.OnBuffAdd = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuff, true>;
    controller.OnBuffRemove = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuff, false>;
    controller.OnBuffUpdate = &ControllerHelpers::ForwardExpiringBuffStateEvent<&UpdateBuff>;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, 650, 900>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Gwen
