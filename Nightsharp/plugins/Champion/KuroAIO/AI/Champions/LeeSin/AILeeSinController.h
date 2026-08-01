#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AILeeSinGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::LeeSin {

using namespace Geometry;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::OrbwalkerHeroTarget;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::RuntimeNameContains;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::UnitByNetworkId;

inline Menu* TacticsMenu = nullptr;
inline Menu* SonicMenu = nullptr;
inline Menu* SafeguardMenu = nullptr;
inline Menu* TempestMenu = nullptr;
inline Menu* KickMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline int QMarkTargetId = 0;
inline int QMarkExpireTick = 0;
inline bool QMarkConfirmed = false;
inline int TempestTargetId = 0;
inline int TempestMarkExpireTick = 0;
inline int FlurryStacks = 0;
inline int FlurryExpireTick = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int PlayerOverrideUntil = 0;
inline int QCastTick = 0;
inline int WCastTick = 0;
inline int ECastTick = 0;
inline int RCastTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline Vector3 LastQLine = {};
inline Vector3 LastSafeguardEndpoint = {};
inline Vector3 LastKickEndpoint = {};
inline Vector3 SuggestedBehindPosition = {};
inline bool QWasManual = false;
inline bool WWasManual = false;
inline bool EWasManual = false;
inline bool RWasManual = false;

inline constexpr int kManualOwnershipMs = 520;
inline constexpr int kCastThrottleMs = 70;

using ControllerHelpers::Now;

inline int SpellRank(int index) {
    return index >= 0 && index < 4 && Engine::RuntimeSpells[index]
        ? std::clamp(Engine::RuntimeSpells[index]->Level(), 1,
                     index == 3 ? 3 : 5)
        : 1;
}

using ControllerHelpers::Ready;

inline int LastCastTick(int index) {
    return index == 0 ? QCastTick : index == 1 ? WCastTick :
           index == 2 ? ECastTick : RCastTick;
}

inline bool Throttle(int index) {
    return Now() - LastCastTick(index) >= kCastThrottleMs;
}

inline bool QSecondCast() {
    return RuntimeNameContains(0, "QTwo") ||
           RuntimeNameContains(0, "ResonatingStrike");
}

inline bool WSecondCast() {
    return RuntimeNameContains(1, "WTwo") ||
           RuntimeNameContains(1, "IronWill");
}

inline bool ESecondCast() {
    return RuntimeNameContains(2, "ETwo") ||
           RuntimeNameContains(2, "Cripple");
}

inline bool TargetCannotBeDamaged(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           target.HasBuff("SivirE") || target.HasBuff("NocturneShroudofDarkness") ||
           target.HasBuff("MorganaE") || target.HasBuff("BlackShield") ||
           target.HasBuff("BansheesVeil") || target.HasBuff("EdgeOfNight") ||
           target.HasBuff("FioraW") || target.HasBuff("VladimirSanguinePool") ||
           target.HasBuff("FizzEIcon") || target.HasBuff("KayleR") ||
           target.HasBuff("kindredrnodeathbuff") || target.HasBuff("ChronoShift");
}

inline bool CursorAgrees(const Vector3& endpoint, float minimumDot = -0.05f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const Vec3 towardCursor = Direction2D(player.Position(), Game::CursorPos());
    const Vec3 towardEndpoint = Direction2D(player.Position(), endpoint);
    return towardCursor.IsZero() || towardEndpoint.IsZero() ||
           towardCursor.Dot(towardEndpoint) >= minimumDot;
}

inline float RawQ1(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid()
        ? Q1RawDamage(SpellRank(0), player.BonusAttackDamage()) : 0.0f;
}

inline float RawQ2(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid()
        ? Q2RawDamage(SpellRank(0), player.BonusAttackDamage(),
                      target.Health(), target.MaxHealth()) : 0.0f;
}

inline float RawE() {
    const auto player = GameObjects::Player();
    return player.IsValid()
        ? ERawDamage(SpellRank(2), player.BonusAttackDamage()) : 0.0f;
}

inline float RawR() {
    const auto player = GameObjects::Player();
    return player.IsValid()
        ? RRawDamage(SpellRank(3), player.BonusAttackDamage()) : 0.0f;
}

using ControllerHelpers::Lethal;

inline bool EnergyFor(int index, float reserve = 0.0f) {
    return HasEnergy(CurrentResource(), SpellCost(index), reserve);
}

inline EndpointSafety SafetyAt(const Vector3& endpoint,
                               bool fleeing,
                               bool lethal,
                               int maximumEnemies = 2) {
    EndpointSafety safety{};
    const auto player = GameObjects::Player();
    safety.Valid = endpoint.IsValid() && !endpoint.IsZero();
    safety.Walkable = safety.Valid && !SDK::NavMesh::IsWall(endpoint);
    safety.UnderEnemyTurret = Engine::UnderEnemyTurret(endpoint);
    safety.StartingUnderEnemyTurret = player.IsValid() &&
        Engine::UnderEnemyTurret(player.Position());
    safety.PointClickThreat = ControllerHelpers::HasReadyPointClickThreatAt(endpoint);
    safety.DashHazard = ControllerHelpers::HasReadyDashHazardAt(endpoint);
    safety.NearbyEnemies = Engine::CountEnemiesAt(endpoint, 625.0f);
    safety.NearbyAllies = Engine::CountAlliesAt(endpoint, 750.0f);
    safety.MaximumEnemies = maximumEnemies;
    safety.Fleeing = fleeing;
    safety.Lethal = lethal;
    return safety;
}

inline void ClearQMark() {
    QMarkTargetId = 0;
    QMarkExpireTick = 0;
    QMarkConfirmed = false;
}

inline bool QMarkActive() {
    if (QMarkTargetId == 0 || Now() >= QMarkExpireTick) return false;
    const AIBaseClient target = UnitByNetworkId(QMarkTargetId);
    return target.IsValid() && !target.IsDead() && target.IsTargetable();
}

inline bool HasTempestMark(const AIBaseClient& target) {
    if (!target.IsValid()) return false;
    return target.HasBuff("BlindMonkEOne") || target.HasBuff("BlindMonkTempest") ||
           target.HasBuff("blindmonketarone") || target.HasBuff("LeeSinE");
}

inline int TempestMarkedEnemies() {
    int count = 0;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy, kE2Radius + 80.0f) && HasTempestMark(enemy)) ++count;
    }
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (minion.IsValid() && !minion.IsDead() && minion.IsTargetable() &&
            player.Position().Distance2D(minion.Position()) <= kE2Radius + 80.0f &&
            HasTempestMark(minion)) ++count;
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (monster.IsValid() && !monster.IsDead() && monster.IsTargetable() &&
            player.Position().Distance2D(monster.Position()) <= kE2Radius + 80.0f &&
            HasTempestMark(monster)) ++count;
    }
    return count;
}

inline void ReconcileState() {
    const int now = Now();
    if (QMarkExpireTick > 0 && now >= QMarkExpireTick) ClearQMark();
    if (TempestMarkExpireTick > 0 && now >= TempestMarkExpireTick) {
        TempestTargetId = 0;
        TempestMarkExpireTick = 0;
    }
    if (FlurryExpireTick > 0 && now >= FlurryExpireTick) {
        FlurryStacks = 0;
        FlurryExpireTick = 0;
    }
    if (!QSecondCast() && !QMarkConfirmed && now - QCastTick > 700) ClearQMark();
    if (QSecondCast() && QMarkTargetId != 0) {
        QMarkExpireTick = std::max(QMarkExpireTick, now + 180);
    }
    const auto player = GameObjects::Player();
    if (player.IsValid() &&
        (player.HasBuff("blindmonkpassive_cosmetic") ||
         player.HasBuff("BlindMonkPassive"))) {
        FlurryStacks = std::max(FlurryStacks, 1);
        FlurryExpireTick = std::max(FlurryExpireTick, now + 180);
    }
}

inline std::vector<Body> SonicBodies(const Vector3& targetPosition,
                                     int targetId) {
    std::vector<Body> bodies;
    bodies.reserve(64);
    const auto append = [&](const AIBaseClient& unit, const Vector3& position,
                            bool primary) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            unit.NetworkId() == 0 || position.IsZero()) return;
        bodies.push_back({ position, unit.BoundingRadius(),
                           static_cast<int>(unit.NetworkId()), true, primary });
    };
    for (const auto& minion : GameObjects::EnemyMinions()) {
        append(AIBaseClient(minion.Handle()), PredictPosition(minion, 0.25f), false);
    }
    for (const auto& monster : GameObjects::Jungle()) {
        append(AIBaseClient(monster.Handle()), PredictPosition(monster, 0.25f), false);
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        append(AIBaseClient(enemy.Handle()),
               static_cast<int>(enemy.NetworkId()) == targetId
                   ? targetPosition : PredictPosition(enemy, 0.25f),
               static_cast<int>(enemy.NetworkId()) == targetId);
    }
    return bodies;
}

inline bool CastQ1(const AIBaseClient& target, Mode mode, bool reactive = false) {
    if (!target.IsValid() || target.IsDead() || !target.IsTargetable() ||
        QSecondCast() || !Ready(0, mode) || !Throttle(0) || !EnergyFor(0)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vector3 predicted = prediction.GetCastPosition().IsValid() &&
        !prediction.GetCastPosition().IsZero()
        ? prediction.GetCastPosition() : PredictPosition(target, 0.25f);
    const SDK::HitChance required = reactive ? SDK::HitChance::Medium : SDK::HitChance::High;
    if (prediction.Hitchance < required || predicted.IsZero() ||
        player.Position().Distance2D(predicted) > kQ1Range + target.BoundingRadius() ||
        !CursorAgrees(predicted, reactive ? -0.35f : 0.0f)) return false;
    const Vec3 direction = Direction2D(player.Position(), predicted);
    if (direction.IsZero()) return false;
    const Vector3 end = player.Position() + direction * kQ1Range;
    const SonicCollision collision = EvaluateSonicCollision(
        player.Position(), end,
        SonicBodies(predicted, static_cast<int>(target.NetworkId())),
        static_cast<int>(target.NetworkId()));
    if (!collision.TargetFirst ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(
            end, kQ1Width * 0.5f)) return false;
    if (Orbwalker::IsWindingUp() && Bool(Engine::HumanMenu, "PreserveAttacks", true) &&
        !Lethal(target, RawQ1(target))) return false;
    if (!Engine::ControllerCastPosition(0, predicted)) return false;
    QCastTick = Now();
    QMarkTargetId = static_cast<int>(target.NetworkId());
    QMarkExpireTick = Now() + kRecastWindowMs;
    QMarkConfirmed = false;
    LastQLine = end;
    QWasManual = false;
    return true;
}

inline bool CastQ2(Mode mode, bool fleeing = false) {
    if (!QSecondCast() || !QMarkActive() || !Ready(0, mode) || !Throttle(0) ||
        !EnergyFor(0)) return false;
    const auto player = GameObjects::Player();
    const AIBaseClient target = UnitByNetworkId(QMarkTargetId);
    if (!player.IsValid() || !target.IsValid() ||
        player.Position().Distance2D(target.Position()) > kQ2Range + target.BoundingRadius()) return false;
    const bool lethal = Lethal(target, RawQ2(target));
    const EndpointSafety safety = SafetyAt(target.Position(), fleeing, lethal,
        Slider(SafeguardMenu, "MaxEndpointEnemies", 2));
    const bool exitAvailable = safety.NearbyAllies > 0 ||
        (Ready(1, mode) && !WSecondCast());
    if (!MayTakeResonatingStrike(true, QMarkConfirmed, true,
            EndpointSafe(safety), lethal, exitAvailable,
            safety.NearbyEnemies, safety.MaximumEnemies)) return false;
    if (!CursorAgrees(target.Position(), fleeing ? -0.60f : -0.05f)) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    QCastTick = Now();
    QWasManual = false;
    ClearQMark();
    return true;
}

inline std::vector<SafeguardCandidate> SafeguardCandidates(bool fleeing,
                                                           bool combo) {
    std::vector<SafeguardCandidate> result;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return result;
    const auto append = [&](const AIBaseClient& unit, bool ward, bool champion) {
        if (!unit.IsValid() || unit.IsDead() || unit.NetworkId() == player.NetworkId() ||
            player.Position().Distance2D(unit.Position()) > kWRange + 15.0f) return;
        SafeguardCandidate candidate{};
        candidate.Position = unit.Position();
        candidate.NetworkId = static_cast<int>(unit.NetworkId());
        candidate.TargetValid = true;
        candidate.TargetVisible = unit.IsVisible();
        candidate.Targetable = unit.IsTargetable();
        candidate.IsWard = ward;
        candidate.IsChampion = champion;
        candidate.CursorDistance = unit.Position().Distance2D(Game::CursorPos());
        candidate.RouteGain = player.Position().Distance2D(Game::CursorPos()) -
                              unit.Position().Distance2D(Game::CursorPos());
        candidate.Safety = SafetyAt(unit.Position(), fleeing, false,
            Slider(SafeguardMenu, "MaxEndpointEnemies", 2));
        if (MaySafeguard(candidate,
                player.Position().Distance2D(unit.Position()), true,
                player.IsDashing(), Bool(SafeguardMenu, "AllowExistingWardHop", true),
                fleeing || combo)) result.push_back(candidate);
    };
    for (const auto& ally : GameObjects::AllyHeroes()) {
        append(AIBaseClient(ally.Handle()), false, true);
    }
    for (const auto& minion : GameObjects::AllyMinions()) {
        const bool ward = SDK::HasFlag(minion.GetMinionType(), SDK::MinionTypes::Ward);
        append(AIBaseClient(minion.Handle()), ward, false);
    }
    return result;
}

inline bool CastSafeguardHop(Mode mode, bool fleeing, bool combo) {
    if (WSecondCast() || !Ready(1, mode) || !Throttle(1) || !EnergyFor(1)) return false;
    const SafeguardCandidate candidate = SelectSafeguard(
        SafeguardCandidates(fleeing, combo), true, true);
    if (candidate.NetworkId == 0 || !CursorAgrees(candidate.Position,
            fleeing ? 0.15f : -0.05f)) return false;
    const AIBaseClient target = UnitByNetworkId(candidate.NetworkId);
    if (!target.IsValid() || !Engine::ControllerCastUnit(1, target)) return false;
    WCastTick = Now();
    LastSafeguardEndpoint = candidate.Position;
    WWasManual = false;
    return true;
}

inline bool CastSelfSafeguard(Mode mode) {
    if (WSecondCast() || !Ready(1, mode) || !Throttle(1) || !EnergyFor(1)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    WCastTick = Now();
    LastSafeguardEndpoint = GameObjects::Player().Position();
    WWasManual = false;
    return true;
}

inline bool CastIronWill(Mode mode) {
    if (!WSecondCast() || !Ready(1, mode) || !Throttle(1) || !EnergyFor(1)) return false;
    if (FlurryStacks > 0 && Now() - LastAutoTick > 500 && CurrentResource() < 90.0f) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    WCastTick = Now();
    WWasManual = false;
    return true;
}

inline bool CastTempest(const AIBaseClient& target, Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid() || ESecondCast() ||
        !Ready(2, mode) || !Throttle(2) || !EnergyFor(2) ||
        !TempestHits(player.Position(), PredictPosition(target, 0.20f),
                     target.BoundingRadius())) return false;
    if (Orbwalker::IsWindingUp() && Bool(Engine::HumanMenu, "PreserveAttacks", true) &&
        !Lethal(target, RawE())) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    ECastTick = Now();
    TempestTargetId = static_cast<int>(target.NetworkId());
    TempestMarkExpireTick = Now() + kRecastWindowMs;
    EWasManual = false;
    return true;
}

inline bool CastCripple(const AIBaseClient& target, Mode mode, bool defensive = false) {
    if (!ESecondCast() || !Ready(2, mode) || !Throttle(2) || !EnergyFor(2)) return false;
    const int marked = TempestMarkedEnemies();
    const bool escaping = target.IsValid() &&
        target.Position().Distance2D(GameObjects::Player().Position()) >= 280.0f;
    if (!MayCripple(true, marked > 0 || TempestMarkExpireTick > Now(), marked,
                    escaping, defensive)) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    ECastTick = Now();
    TempestTargetId = 0;
    TempestMarkExpireTick = 0;
    EWasManual = false;
    return true;
}

inline Vector3 AlliedKickGoal(const AIHeroClient& target) {
    Vector3 sum{};
    int count = 0;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally, 1800.0f) ||
            ally.NetworkId() == GameObjects::Player().NetworkId()) continue;
        sum = sum + ally.Position();
        ++count;
    }
    if (count > 0) return sum / static_cast<float>(count);
    return target.IsValid() ? Game::CursorPos() : Vector3{};
}

inline std::vector<Body> KickBodies(int primaryId) {
    std::vector<Body> bodies;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1700.0f)) continue;
        bodies.push_back({ PredictPosition(enemy, 0.25f), enemy.BoundingRadius(),
                           static_cast<int>(enemy.NetworkId()), true,
                           static_cast<int>(enemy.NetworkId()) != primaryId &&
                               enemy.TotalAttackDamage() + enemy.AP() * 0.65f > 220.0f });
    }
    return bodies;
}

inline KickPlan BuildKickPlan(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return {};
    const Body primary{ PredictPosition(target, 0.12f), target.BoundingRadius(),
                        static_cast<int>(target.NetworkId()), true, true };
    return EvaluateKickPlan(player.Position(), primary,
                            KickBodies(primary.Id), AlliedKickGoal(target));
}

inline bool CastKick(const AIHeroClient& target, Mode mode, bool defensive = false) {
    if (!Engine::ValidEnemy(target, kRRange + 35.0f) || !Ready(3, mode) ||
        !Throttle(3) || !EnergyFor(3) || TargetCannotBeDamaged(target)) return false;
    const KickPlan plan = BuildKickPlan(target);
    if (plan.Endpoint.IsZero()) return false;
    const bool lethal = Lethal(target, RawR());
    const int minimumHits = Slider(KickMenu, "MinimumKickHits", 2);
    const bool multi = plan.SecondaryHits + 1 >= minimumHits;
    const bool teamward = plan.TeamwardGain >=
        static_cast<float>(Slider(KickMenu, "MinimumTeamwardGain", 260));
    if (!defensive && !lethal && !multi && !teamward) return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    RCastTick = Now();
    LastKickEndpoint = plan.Endpoint;
    SuggestedBehindPosition = plan.Behind;
    RWasManual = false;
    return true;
}

inline bool TryDefensive(const AIHeroClient& threat) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool emergency = IncomingHardCCUntil > Now() ||
        player.HealthPercent() <= Slider(SafeguardMenu, "EmergencyHP", 30);
    if (!emergency) return false;
    if (CastSelfSafeguard(Mode::Automatic)) return true;
    if (Engine::ValidEnemy(threat) && CastCripple(threat, Mode::Automatic, true)) return true;
    return Engine::ValidEnemy(threat) && CastKick(threat, Mode::Automatic, true);
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (QSecondCast() && QMarkTargetId == static_cast<int>(target.NetworkId()) &&
        Lethal(target, RawQ2(target)) && CastQ2(mode)) return true;
    if (!QSecondCast() && Lethal(target, RawQ1(target)) && CastQ1(target, mode, true)) return true;
    if (Lethal(target, RawE()) && CastTempest(target, mode)) return true;
    return Lethal(target, RawR()) && CastKick(target, mode, false);
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (QSecondCast() && CastQ2(Mode::Combo)) return true;
    if (CastTempest(target, Mode::Combo)) return true;
    if (CastCripple(target, Mode::Combo)) return true;
    if (target.DistanceToPlayer() <= kRRange + 30.0f && CastKick(target, Mode::Combo)) return true;
    if (CastQ1(target, Mode::Combo)) return true;
    if (CastIronWill(Mode::Combo)) return true;
    return target.DistanceToPlayer() > 500.0f && CastSafeguardHop(Mode::Combo, false, true);
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (QSecondCast() && QMarkTargetId == static_cast<int>(target.NetworkId()) &&
        Bool(SonicMenu, "HarassTakeQ2", false) && CastQ2(Mode::Harass)) return true;
    if (CastQ1(target, Mode::Harass)) return true;
    if (CastTempest(target, Mode::Harass)) return true;
    if (CastCripple(target, Mode::Harass)) return true;
    return GameObjects::Player().HealthPercent() < 65.0f && CastSelfSafeguard(Mode::Harass);
}

inline int FarmUnitsInTempest(Mode mode) {
    int count = 0;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    const auto& units = mode == Mode::Jungle ? GameObjects::Jungle() : GameObjects::EnemyMinions();
    for (const auto& unit : units) {
        if (unit.IsValid() && !unit.IsDead() && unit.IsTargetable() &&
            TempestHits(player.Position(), unit.Position(), unit.BoundingRadius())) ++count;
    }
    return count;
}

inline AIBaseClient FarmQTarget(Mode mode, bool lastHitOnly) {
    AIBaseClient best{};
    float bestHealth = FLT_MAX;
    const auto& units = mode == Mode::Jungle ? GameObjects::Jungle() : GameObjects::EnemyMinions();
    for (const auto& unit : units) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            unit.DistanceToPlayer() > kQ1Range + unit.BoundingRadius()) continue;
        if (lastHitOnly && unit.Health() > RawQ1(unit)) continue;
        if (unit.Health() < bestHealth) {
            best = AIBaseClient(unit.Handle());
            bestHealth = unit.Health();
        }
    }
    return best;
}

inline bool TryFarm(Mode mode) {
    const float reserve = static_cast<float>(Slider(FarmMenu, "EnergyReserve", 70));
    if (QSecondCast() && QMarkActive() && mode == Mode::Jungle && EnergyFor(0, reserve) && CastQ2(mode)) return true;
    if (!ESecondCast() && EnergyFor(2, reserve) &&
        FarmUnitsInTempest(mode) >= Slider(FarmMenu,
            mode == Mode::Jungle ? "JungleEHits" : "LaneEHits", mode == Mode::Jungle ? 1 : 3)) {
        const AIBaseClient target = FarmQTarget(mode, false);
        if (target.IsValid() && CastTempest(target, mode)) return true;
    }
    if (ESecondCast() && EnergyFor(2, reserve) && FarmUnitsInTempest(mode) >= (mode == Mode::Jungle ? 1 : 3)) {
        const AIBaseClient target = FarmQTarget(mode, false);
        if (CastCripple(target, mode)) return true;
    }
    if (!QSecondCast() && EnergyFor(0, reserve)) {
        const AIBaseClient target = FarmQTarget(mode, mode != Mode::Jungle);
        if (target.IsValid() && CastQ1(target, mode)) return true;
    }
    return false;
}

inline bool TryFlee(const AIHeroClient& threat) {
    if (CastSafeguardHop(Mode::Flee, true, false)) return true;
    if (Engine::ValidEnemy(threat) && CastCripple(threat, Mode::Flee, true)) return true;
    return Engine::ValidEnemy(threat) && CastKick(threat, Mode::Flee, true);
}

inline AIHeroClient CooperativeTarget(const AIHeroClient& selected) {
    if (Engine::ValidEnemy(selected, kQ2Range + 80.0f)) return selected;
    const AIHeroClient orbTarget = OrbwalkerHeroTarget(kQ2Range + 80.0f);
    if (Engine::ValidEnemy(orbTarget)) return orbTarget;
    return Engine::SelectTarget(kQ2Range + 80.0f);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (PlayerOverrideUntil > Now()) return true;
    const AIHeroClient target = CooperativeTarget(selected);
    const AIHeroClient threat = NearestEnemyToPlayer(target, 1250.0f);
    if (mode == Mode::Flee) {
        (void)TryFlee(threat);
        return true;
    }
    if (TryDefensive(threat)) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: (void)TryCombo(target); break;
    case Mode::Harass: (void)TryHarass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)TryFarm(mode); break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target) &&
            (IncomingHardCCUntil > Now() ||
             Lethal(target, RawQ1(target)))) {
            (void)TryKillSecure(target, Mode::Automatic);
        }
        break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !args.Sender.IsValid()) return;
    const int now = Now();
    if (!IsLocalPlayer(args.Sender)) {
        const auto threat = ControllerHelpers::AnalyzeEnemyCast(args, 230.0f, 110.0f, 250, 280, 260, 1500, 430);
        if (threat.Valid && threat.CrossesPlayer) {
            IncomingThreatUntil = threat.LineThreatUntilTick;
            if (threat.LikelyHardCrowdControl) IncomingHardCCUntil = now + 650;
        }
        return;
    }
    if (args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
        LastAutoTick = now;
        if (FlurryStacks > 0) --FlurryStacks;
        return;
    }
    const int slot = args.Slot;
    if (slot < 0 || slot > 3) return;
    const bool controllerOwned = Engine::WasControllerCast(slot);
    if (!controllerOwned) PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", kManualOwnershipMs);
    if (slot == 0) {
        QCastTick = now;
        QWasManual = !controllerOwned;
        if (args.TargetNetworkId != 0) QMarkTargetId = static_cast<int>(args.TargetNetworkId);
        if (!QSecondCast()) QMarkExpireTick = now + kRecastWindowMs;
    } else if (slot == 1) {
        WCastTick = now;
        WWasManual = !controllerOwned;
        if (args.Target.IsValid()) LastSafeguardEndpoint = args.Target.Position;
    } else if (slot == 2) {
        ECastTick = now;
        EWasManual = !controllerOwned;
        TempestMarkExpireTick = now + kRecastWindowMs;
    } else {
        RCastTick = now;
        RWasManual = !controllerOwned;
    }
    FlurryStacks = 2;
    FlurryExpireTick = now + 3000;
}

inline bool IsQMarkName(const char* name) {
    return name && (Engine::TextContains(name, "BlindMonkQOne") ||
                    Engine::TextContains(name, "SonicWave") ||
                    Engine::TextContains(name, "LeeSinQMark"));
}

inline bool IsTempestMarkName(const char* name) {
    return name && (Engine::TextContains(name, "BlindMonkEOne") ||
                    Engine::TextContains(name, "Tempest") ||
                    Engine::TextContains(name, "LeeSinE"));
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "blindmonkpassive") || Engine::TextContains(args.BuffName, "Flurry")) {
            FlurryStacks = args.Count > 0 ? std::clamp(args.Count, 1, 2) : 2;
            FlurryExpireTick = now + ControllerHelpers::RemainingMilliseconds(args.EndTime, 3000, 150, 4000);
        }
        return;
    }
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (IsQMarkName(args.BuffName)) {
        QMarkTargetId = id;
        QMarkConfirmed = true;
        QMarkExpireTick = now + ControllerHelpers::RemainingMilliseconds(args.EndTime, kRecastWindowMs, 150, 4000);
    }
    if (IsTempestMarkName(args.BuffName)) {
        TempestTargetId = id;
        TempestMarkExpireTick = now + ControllerHelpers::RemainingMilliseconds(args.EndTime, kRecastWindowMs, 150, 4000);
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "blindmonkpassive") || Engine::TextContains(args.BuffName, "Flurry")) {
            FlurryStacks = 0;
            FlurryExpireTick = 0;
        }
        return;
    }
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (id == QMarkTargetId && IsQMarkName(args.BuffName)) ClearQMark();
    if (id == TempestTargetId && IsTempestMarkName(args.BuffName)) {
        TempestTargetId = 0;
        TempestMarkExpireTick = 0;
    }
}


inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || FlurryStacks <= 0) return;
    if (CurrentResource() < 80.0f && ESecondCast() && TempestMarkExpireTick - Now() > 500) args.Process = true;
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
    LastAutoTick = Now();
    if (FlurryStacks > 0) --FlurryStacks;
}

inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawPlans", false)) return;
    Drawing::DrawCircle(player.Position(), kQ1Range, 0xFFB5D53Au, 1.0f, 56);
    if (!LastQLine.IsZero()) Drawing::DrawLine(player.Position(), LastQLine, 0xFFD4E85Au, 1.0f);
    if (!LastSafeguardEndpoint.IsZero()) Drawing::DrawCircle(LastSafeguardEndpoint, 45.0f, 0xFF2F9B5Fu, 2.0f, 24);
    if (!LastKickEndpoint.IsZero()) Drawing::DrawLine(player.Position(), LastKickEndpoint, 0xFFE56B3Au, 2.0f);
    if (!SuggestedBehindPosition.IsZero()) Drawing::DrawCircle(SuggestedBehindPosition, 55.0f, 0xFFFFD45Au, 2.0f, 28);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("LeeSinOneTrick", "Lee Sin one-trick mechanics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 520, 180, 1100));
    SonicMenu = TacticsMenu->AddSubMenu(new Menu("Sonic", "Sonic Wave and safe Q2"));
    SonicMenu->Add(new MenuBool("HarassTakeQ2", "Take safe Q2 during harass", false));
    SafeguardMenu = TacticsMenu->AddSubMenu(new Menu("Safeguard", "Ally and existing-ward endpoint safety"));
    SafeguardMenu->Add(new MenuBool("AllowExistingWardHop", "Hop only to an existing allied ward", true));
    SafeguardMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum endpoint enemies", 2, 1, 5));
    SafeguardMenu->Add(new MenuSlider("EmergencyHP", "Self shield below HP", 30, 10, 70));
    TempestMenu = TacticsMenu->AddSubMenu(new Menu("Tempest", "Tempest mark and Cripple timing"));
    TempestMenu->Add(new MenuBool("HoldCrippleForFlurry", "Wait for Flurry energy before Cripple", true));
    KickMenu = TacticsMenu->AddSubMenu(new Menu("DragonsRage", "Kick collision and teamward geometry"));
    KickMenu->Add(new MenuSlider("MinimumKickHits", "Minimum total kick-line hits", 2, 1, 5));
    KickMenu->Add(new MenuSlider("MinimumTeamwardGain", "Minimum teamward displacement", 260, 80, 700));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("LeeSinFarm", "Energy-reserved lane and jungle usage"));
    FarmMenu->Add(new MenuSlider("EnergyReserve", "Energy kept after farm", 70, 0, 160));
    FarmMenu->Add(new MenuSlider("LaneEHits", "Lane E minimum units", 3, 1, 8));
    FarmMenu->Add(new MenuSlider("JungleEHits", "Jungle E minimum units", 1, 1, 4));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("LeeSinCoach", "Manual insec and route coaching"));
    CoachMenu->Add(new MenuBool("DrawPlans", "Draw Q, W and kick geometry", false));
}

inline void OnLoad() {
    ClearQMark();
    TempestTargetId = TempestMarkExpireTick = 0;
    FlurryStacks = FlurryExpireTick = 0;
    IncomingThreatUntil = IncomingHardCCUntil = PlayerOverrideUntil = 0;
    QCastTick = WCastTick = ECastTick = RCastTick = 0;
    LastAutoTargetId = LastAutoTick = 0;
    LastQLine = LastSafeguardEndpoint = LastKickEndpoint = SuggestedBehindPosition = {};
    QWasManual = WWasManual = EWasManual = RWasManual = false;
    ReconcileState();
}

inline void OnUnload() {
    TacticsMenu = SonicMenu = SafeguardMenu = TempestMenu = KickMenu = FarmMenu = CoachMenu = nullptr;
    ClearQMark();
}

inline constexpr const char* Scenarios[] = {
    "Pin Summoner's Rift mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Treat 200 energy as a bounded resource rather than mana percent",
    "Restore 20 then 10 energy from the two Flurry attacks",
    "Preserve Flurry autos when an immediate recast would starve the sequence",
    "Track Flurry stacks from spell, buff and attack events",
    "Use Sonic Wave 1200 range, 60 width, 1800 speed and prediction",
    "Require the champion or farm unit to be Sonic Wave's first collision body",
    "Reject Sonic Wave through a minion, monster or intervening champion",
    "Use current Sonic Wave base damage and 115 percent bonus AD ratio",
    "Track the Sonic Wave mark from target buff events and runtime Q2 state",
    "Expire the Q mark after the real three-second recast window",
    "Use Resonating Strike's 1300 marked-target reach",
    "Scale Resonating Strike up to double damage with missing health",
    "Reject Q2 when the mark is inferred but not confirmed",
    "Reject Q2 into an unsafe turret, point-click threat or dash hazard",
    "Reject Q2 into excess enemies unless the strike is lethal",
    "Require an ally, ready Safeguard or low enemy count as a Q2 exit",
    "Preserve cursor agreement before taking an automatic Q2",
    "Use Safeguard only on Lee Sin, a live ally, allied minion or existing allied ward",
    "Never place a ward, press an item active or synthesize a ward-hop anchor",
    "Require an existing ward to be visible and targetable",
    "Allow automatic ward hops only for flee or explicit combo routing",
    "Reject Safeguard endpoints in walls, new turret danger or excess enemies",
    "Reject Safeguard endpoints with ready point-click or anti-dash hazards",
    "Prefer a route that gains distance toward the player's cursor",
    "Prefer a champion anchor over an equivalent ward anchor",
    "Use self Safeguard for verified defensive pressure without inventing movement",
    "Treat Iron Will as W2 and never attempt an ally-target cast during W2",
    "Hold Iron Will when a Flurry attack is needed to recover energy",
    "Use Tempest only inside the true 450 effect radius",
    "Track Tempest marks from enemy buff events and polling",
    "Use Cripple only while E2 and at least one Tempest mark are confirmed",
    "Spend Cripple on an escaping target, peel, or a multi-target slow",
    "Use current Tempest damage and Cripple slow ranks",
    "Model Dragon's Rage as a 375-range targeted kick",
    "Project the primary target through the 1200-unit knockback corridor",
    "Expand kick collision by the primary and secondary collision radii",
    "Count each secondary champion only once in the kick corridor",
    "Reward priority carries struck by the kicked target",
    "Compute teamward gain against the live allied centroid",
    "Draw the behind-target position required for a teamward kick",
    "Never move, ward, Flash or issue attack-move to manufacture an insec",
    "Allow a single-target kick for lethal damage, peel or interrupt",
    "Require multi-hit or meaningful teamward gain for an ordinary proactive kick",
    "Preserve the selected target while it is legally reachable",
    "Cooperate with the orbwalker target before policy target selection",
    "Combo uses safe Q2, Tempest, Cripple, kick geometry, Q1 and bounded W routing",
    "Harass defaults to Q1 and does not take Q2 unless explicitly enabled",
    "LaneClear preserves energy and requires configured Tempest unit count",
    "Jungle permits marked Q2 only with energy reserve and endpoint safety",
    "LastHit uses an unblocked lethal Q1 and never spends Q2 speculatively",
    "Flee prefers an existing cursorward anchor before Cripple or defensive kick",
    "Automatic mode remains reactive to lethal, hard CC and defensive pressure",
    "Reconcile Q, E and Flurry state through both events and runtime polling",
    "Yield the full decision loop after every player-owned Q, W, E or R",
    "Continue observing marks after a manual cast without claiming ownership",
    "Preserve attack windups except for a verified lethal spell",
    "Never automate Flash, Ignite, Smite, trinkets or item actives",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::LeeSin;
    controller.ControllerId = "champion.kuroaio.ai.leesin.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AILeeSin.md";
    controller.ImplementationSummary =
        "Collision-authoritative Q and marked Q2 safety, existing-anchor-only "
        "Safeguard routing, Flurry-aware energy, Tempest mark timing, kick-line "
        "geometry and manual insec ownership across every orbwalker mode.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &OnProcessSpell;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &ControllerHelpers::ForwardActiveBuffEvent<&OnBuffAdd>;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::LeeSin
