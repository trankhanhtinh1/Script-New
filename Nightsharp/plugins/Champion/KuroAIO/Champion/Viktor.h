#pragma once

#include "../Helper/KuroAIOCommon.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::Viktor {

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* RBlacklistMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneMenu = nullptr;
inline Menu* JungleMenu = nullptr;
inline Menu* KillstealMenu = nullptr;
inline Menu* MiscMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 652.0f };
inline Spell W{ SpellSlot::W, 800.0f };
inline Spell E{ SpellSlot::E, 525.0f };
inline Spell R{ SpellSlot::R, 700.0f };

inline bool Loaded = false;
inline bool QPendingAttack = false;
inline uint32_t PendingQTargetId = 0;
inline int PendingQSince = 0;
inline int LastUpdateTick = 0;
inline int LastClearTick = 0;
inline int LastRFollowTick = 0;
inline int LastAutoWTick = 0;

static constexpr float kERange = 525.0f;
static constexpr float kEMaxRange = 1200.0f;
static constexpr float kELength = 700.0f;
static constexpr float kESpeed = 1050.0f;
static constexpr float kEWidth = 90.0f;
static constexpr float kEAftershockWidth = 80.0f;
static constexpr float kEAftershockDelay = 1.0f;
static constexpr float kEAftershockSpeed = 1500.0f;
static constexpr float kEStartMargin = 6.0f;
static constexpr float kELineLead = 85.0f;

struct LaserFarmLocation {
    Vector2 Start = {};
    Vector2 End = {};
    int MinionsHit = 0;
};

struct ETargetPrediction {
    AIHeroClient Target = {};
    Vector2 InitialPosition = {};
    Vector2 AftershockPosition = {};
    HitChance InitialHitchance = HitChance::None;
    HitChance AftershockHitchance = HitChance::None;
    bool Primary = false;
};

struct ECandidate {
    Vector2 Start = {};
    Vector2 End = {};
    float Score = -FLT_MAX;
    int InitialHits = 0;
    int AftershockHits = 0;
    int DoubleHits = 0;
    bool PrimaryInitialHit = false;
};

static bool ShouldRunNow(int& lastTick, int intervalMs) {
    const int now = SDK::Variables::TickCount();
    if (lastTick > 0 && now - lastTick < intervalMs) {
        return false;
    }
    lastTick = now;
    return true;
}

static bool HasName(const Events::ProcessSpellEventArgs& args, const char* name) {
    return EqualsIgnoreCase(args.SpellName, name) ||
           EqualsIgnoreCase(args.ScriptName, name) ||
           EqualsIgnoreCase(args.SpellSlotName, name) ||
           EqualsIgnoreCase(args.PayloadSpellName, name);
}

static bool IsRInitialCast() {
    return EqualsIgnoreCase(R.Instance().Name().c_str(), "ViktorChaosStorm");
}

static bool HasActiveChaosStorm() {
    const auto player = Player();
    if (player.IsValid() &&
        (player.HasBuff("ViktorChaosStorm") || player.HasBuff("viktorchaosstorm") ||
         player.HasBuff("ViktorChaosStormGuide") || player.HasBuff("viktorchaosstormguide"))) {
        return true;
    }
    return EqualsIgnoreCase(R.Instance().Name().c_str(), "ViktorChaosStormGuide");
}

static bool HasHardCrowdControl(const AIBaseClient& target) {
    return SDK::HasBuffOfType(target, SDK::BuffType::Stun) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Snare) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Taunt) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Fear) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Charm) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Suppression) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Knockup) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Knockback) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Asleep);
}

static float EffectiveMagicalHealth(const AIBaseClient& target) {
    return target.Health() + target.AllShield() + target.MagicalShield();
}

static std::string RBlacklistKey(const AIHeroClient& enemy) {
    return "BlockR." + std::to_string(enemy.NetworkId());
}

static bool RBlocked(const AIHeroClient& enemy) {
    if (!RBlacklistMenu || !enemy.IsValid()) {
        return false;
    }
    const std::string key = RBlacklistKey(enemy);
    const auto* item = RBlacklistMenu->Get<MenuBool>(key.c_str());
    return item && item->Value;
}

static int CountEnemiesNear(const Vector3& position, float range) {
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy) && enemy.Position().Distance2D(position) <= range) {
            ++count;
        }
    }
    return count;
}

static float DistancePointSegmentSqr(const Vector2& point,
                                     const Vector2& start,
                                     const Vector2& end) {
    const Vector2 segment = end - start;
    const float lengthSqr = segment.LengthSqr();
    if (lengthSqr <= FLT_EPSILON) {
        return point.DistanceSqr(start);
    }
    const float t = std::clamp((point - start).Dot(segment) / lengthSqr, 0.0f, 1.0f);
    return point.DistanceSqr(start + segment * t);
}

static void ResetEGeometry() {
    E.Range = kERange;
    E.From = {};
    E.RangeCheckFrom = {};
    E.Speed = kESpeed;
}

static bool CastE(Vector3 start, Vector3 end) {
    start.y = NavMesh::GetHeightForPosition(start);
    end.y = NavMesh::GetHeightForPosition(end);
    ResetEGeometry();
    return E.Cast(start, end);
}

static bool HasEAugment() {
    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }

    if (player.HasBuff("viktoreaug") ||
        player.HasBuff("viktorqeaug") ||
        player.HasBuff("viktorweaug") ||
        player.HasBuff("viktorewaug") ||
        player.HasBuff("viktorqweaug") ||
        player.HasBuff("ViktorEAug") ||
        player.HasBuff("ViktorEAugment")) {
        return true;
    }

    // Covers patch-specific combined augment buff names without depending on
    // one exact capitalization or Q/W/E ordering.
    return CoreBuffs::HasActiveBuffContaining(player.Address(), "eaug");
}

static float EHitchanceWeight(HitChance hitchance) {
    switch (hitchance) {
    case HitChance::Dash:
        return 1.12f;
    case HitChance::Immobile:
        return 1.25f;
    case HitChance::VeryHigh:
        return 1.10f;
    case HitChance::High:
        return 1.0f;
    case HitChance::Medium:
        return 0.72f;
    case HitChance::Low:
        return 0.30f;
    default:
        return 0.0f;
    }
}

static Vector2 EPredictedPosition(const AIHeroClient& target,
                                  float delay,
                                  float radius,
                                  HitChance& hitchance) {
    const auto prediction = SDK::Prediction::GetPrediction(target, delay, radius);
    hitchance = prediction.Hitchance;
    Vector3 position = prediction.GetUnitPosition();
    if (!position.IsValid() || position.IsZero()) {
        position = target.Position();
    }
    return position.To2D();
}

static ETargetPrediction BuildETargetPrediction(const AIHeroClient& target,
                                                bool primary,
                                                bool augmented) {
    ETargetPrediction result;
    result.Target = target;
    result.Primary = primary;

    const auto player = Player();
    const float distance = player.IsValid()
        ? player.Position().Distance2D(target.Position())
        : target.DistanceToPlayer();
    const float startDistance = std::clamp(
        distance - kELineLead,
        0.0f,
        kERange - kEStartMargin);
    const float distanceAlongRay = std::clamp(
        distance - startDistance,
        0.0f,
        kELength);
    const float firstHitDelay = 0.025f + distanceAlongRay / kESpeed;
    const float aftershockHitDelay =
        kEAftershockDelay + distanceAlongRay / kEAftershockSpeed;

    result.InitialPosition = EPredictedPosition(
        target,
        firstHitDelay,
        kEWidth,
        result.InitialHitchance);
    if (augmented) {
        result.AftershockPosition = EPredictedPosition(
            target,
            aftershockHitDelay,
            kEAftershockWidth,
            result.AftershockHitchance);
    } else {
        result.AftershockPosition = result.InitialPosition;
        result.AftershockHitchance = HitChance::None;
    }
    return result;
}

static void AddECandidate(std::vector<ECandidate>& candidates,
                          const Vector2& playerPosition,
                          const Vector2& proposedStart,
                          const Vector2& proposedEnd) {
    if (!std::isfinite(proposedStart.x) || !std::isfinite(proposedStart.y) ||
        !std::isfinite(proposedEnd.x) || !std::isfinite(proposedEnd.y)) {
        return;
    }

    const Vector2 direction = proposedEnd - proposedStart;
    const float directionLengthSqr = direction.LengthSqr();
    if (directionLengthSqr < 25.0f * 25.0f ||
        proposedStart.DistanceSqr(playerPosition) >
            (kERange - kEStartMargin) * (kERange - kEStartMargin)) {
        return;
    }

    const Vector2 start = proposedStart;
    const Vector2 end = start +
        direction * (kELength / std::sqrt(directionLengthSqr));
    for (const auto& candidate : candidates) {
        if (candidate.Start.DistanceSqr(start) <= 14.0f * 14.0f &&
            candidate.End.DistanceSqr(end) <= 22.0f * 22.0f) {
            return;
        }
    }

    ECandidate candidate;
    candidate.Start = start;
    candidate.End = end;
    candidates.push_back(candidate);
}

static void AddESinglePointCandidates(std::vector<ECandidate>& candidates,
                                      const Vector2& playerPosition,
                                      const Vector2& point) {
    const Vector2 offset = point - playerPosition;
    const float distanceSqr = offset.LengthSqr();
    if (distanceSqr < 25.0f * 25.0f) {
        return;
    }

    const float distance = std::sqrt(distanceSqr);
    const Vector2 direction = offset * (1.0f / distance);
    const float startRange = kERange - kEStartMargin;
    const float outwardStartDistance = std::clamp(
        distance - kELineLead,
        0.0f,
        startRange);
    const Vector2 outwardStart =
        playerPosition + direction * outwardStartDistance;
    AddECandidate(
        candidates,
        playerPosition,
        outwardStart,
        outwardStart + direction * kELength);

    // Close targets can be swept in the opposite direction. This catches a
    // second enemy behind Viktor and leaves more laser behind a retreating unit.
    if (distance < startRange) {
        const float reverseStartDistance = std::min(
            startRange,
            distance + kELineLead);
        const Vector2 reverseStart =
            playerPosition + direction * reverseStartDistance;
        AddECandidate(
            candidates,
            playerPosition,
            reverseStart,
            reverseStart - direction * kELength);
    }
}

static void AddELineThroughOrientation(std::vector<ECandidate>& candidates,
                                       const Vector2& playerPosition,
                                       const Vector2& first,
                                       const Vector2& second) {
    const Vector2 delta = second - first;
    const float separationSqr = delta.LengthSqr();
    if (separationSqr < 25.0f * 25.0f) {
        return;
    }

    const float separation = std::sqrt(separationSqr);
    const float edgeTolerance = kEWidth * 0.75f;
    if (separation > kELength + edgeTolerance * 2.0f) {
        return;
    }

    const Vector2 direction = delta * (1.0f / separation);
    const Vector2 firstFromPlayer = first - playerPosition;
    const float along = firstFromPlayer.Dot(direction);
    const float perpendicularSqr = std::max(
        0.0f,
        firstFromPlayer.LengthSqr() - along * along);
    const float startRange = kERange - kEStartMargin;
    if (perpendicularSqr > startRange * startRange) {
        return;
    }

    const float circleOffset = std::sqrt(
        std::max(0.0f, startRange * startRange - perpendicularSqr));
    const float feasibleLow = std::max(
        -along - circleOffset,
        separation - kELength - edgeTolerance);
    const float feasibleHigh = std::min(
        -along + circleOffset,
        edgeTolerance);
    if (feasibleLow > feasibleHigh) {
        return;
    }

    const float startParameter = std::clamp(
        -kELineLead,
        feasibleLow,
        feasibleHigh);
    const Vector2 start = first + direction * startParameter;
    AddECandidate(
        candidates,
        playerPosition,
        start,
        start + direction * kELength);
}

static void AddELinesThroughPoints(std::vector<ECandidate>& candidates,
                                   const Vector2& playerPosition,
                                   const Vector2& first,
                                   const Vector2& second) {
    AddELineThroughOrientation(candidates, playerPosition, first, second);
    AddELineThroughOrientation(candidates, playerPosition, second, first);
}

static bool ELineContains(const ECandidate& candidate,
                          const Vector2& position,
                          float radius) {
    return DistancePointSegmentSqr(position, candidate.Start, candidate.End) <=
           radius * radius;
}

static void ScoreECandidate(ECandidate& candidate,
                            const std::vector<ETargetPrediction>& targets,
                            bool augmented) {
    candidate.Score = 0.0f;
    candidate.InitialHits = 0;
    candidate.AftershockHits = 0;
    candidate.DoubleHits = 0;
    candidate.PrimaryInitialHit = false;
    const float aftershockScale = augmented ? 1.0f : 0.0f;

    for (const auto& target : targets) {
        const float boundingRadius = std::clamp(
            target.Target.BoundingRadius(),
            25.0f,
            80.0f);
        const HitChance minimumInitialHitchance =
            target.Primary ? HitChance::High : HitChance::Medium;
        const bool initialHit =
            target.InitialHitchance >= minimumInitialHitchance &&
            ELineContains(
                candidate,
                target.InitialPosition,
                kEWidth + boundingRadius);
        const bool aftershockHit =
            target.AftershockHitchance >= HitChance::Medium &&
            ELineContains(
                candidate,
                target.AftershockPosition,
                kEAftershockWidth + boundingRadius);

        if (initialHit) {
            ++candidate.InitialHits;
            candidate.PrimaryInitialHit =
                candidate.PrimaryInitialHit || target.Primary;
            const float value = target.Primary ? 300.0f : 180.0f;
            candidate.Score +=
                value * EHitchanceWeight(target.InitialHitchance);
        }
        if (aftershockHit) {
            ++candidate.AftershockHits;
            const float value = target.Primary ? 135.0f : 85.0f;
            candidate.Score += aftershockScale * value *
                EHitchanceWeight(target.AftershockHitchance);
        }
        if (initialHit && aftershockHit) {
            ++candidate.DoubleHits;
            candidate.Score += aftershockScale *
                (target.Primary ? 60.0f : 35.0f);
        }
    }

    if (!candidate.PrimaryInitialHit) {
        candidate.Score = -FLT_MAX;
        return;
    }

    const int extraInitialHits = std::max(0, candidate.InitialHits - 1);
    candidate.Score += 45.0f * static_cast<float>(
        extraInitialHits * extraInitialHits);
    candidate.Score += aftershockScale * 10.0f * static_cast<float>(
        candidate.AftershockHits * candidate.AftershockHits);
    candidate.Score += aftershockScale * 22.0f * static_cast<float>(
        candidate.DoubleHits);
}

static std::vector<ECandidate> BuildECandidates(
    const Vector2& playerPosition,
    const std::vector<ETargetPrediction>& targets,
    bool augmented) {
    std::vector<ECandidate> candidates;
    candidates.reserve(128);

    for (const auto& target : targets) {
        AddESinglePointCandidates(
            candidates,
            playerPosition,
            target.InitialPosition);
        if (augmented) {
            AddESinglePointCandidates(
                candidates,
                playerPosition,
                target.AftershockPosition);
            AddELinesThroughPoints(
                candidates,
                playerPosition,
                target.InitialPosition,
                target.AftershockPosition);
        }
    }

    const int targetCount = static_cast<int>(targets.size());
    for (int i = 0; i < targetCount; ++i) {
        for (int j = i + 1; j < targetCount; ++j) {
            AddELinesThroughPoints(
                candidates,
                playerPosition,
                targets[i].InitialPosition,
                targets[j].InitialPosition);
            if (augmented) {
                AddELinesThroughPoints(
                    candidates,
                    playerPosition,
                    targets[i].InitialPosition,
                    targets[j].AftershockPosition);
                AddELinesThroughPoints(
                    candidates,
                    playerPosition,
                    targets[i].AftershockPosition,
                    targets[j].InitialPosition);
                AddELinesThroughPoints(
                    candidates,
                    playerPosition,
                    targets[i].AftershockPosition,
                    targets[j].AftershockPosition);
            }
        }
    }

    for (auto& candidate : candidates) {
        ScoreECandidate(candidate, targets, augmented);
    }
    std::sort(candidates.begin(), candidates.end(), [](const ECandidate& left,
                                                       const ECandidate& right) {
        if (left.Score != right.Score) {
            return left.Score > right.Score;
        }
        if (left.InitialHits != right.InitialHits) {
            return left.InitialHits > right.InitialHits;
        }
        if (left.DoubleHits != right.DoubleHits) {
            return left.DoubleHits > right.DoubleHits;
        }
        return left.AftershockHits > right.AftershockHits;
    });
    return candidates;
}

static bool CastESingleTargetFallback(const AIHeroClient& target,
                                      const Vector2& predictedPosition) {
    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }

    const Vector2 playerPosition = player.Position().To2D();
    const Vector2 offset = predictedPosition - playerPosition;
    const float distanceSqr = offset.LengthSqr();
    if (distanceSqr < 25.0f * 25.0f) {
        return false;
    }

    const float distance = std::sqrt(distanceSqr);
    const Vector2 direction = offset * (1.0f / distance);
    const float startDistance = std::clamp(
        distance - kELineLead,
        0.0f,
        kERange - kEStartMargin);
    const Vector2 start2D = playerPosition + direction * startDistance;
    Vector3 start = Vector3::From2D(start2D);
    start.y = NavMesh::GetHeightForPosition(start);

    E.From = start;
    E.RangeCheckFrom = start;
    E.Range = kELength;
    E.Speed = kESpeed;
    const auto prediction = E.GetPrediction(target);
    if (prediction.Hitchance < HitChance::High) {
        ResetEGeometry();
        return false;
    }

    const Vector2 refinedOffset =
        prediction.GetUnitPosition().To2D() - start2D;
    const float refinedDistanceSqr = refinedOffset.LengthSqr();
    if (refinedDistanceSqr < 25.0f * 25.0f ||
        refinedDistanceSqr > kELength * kELength) {
        ResetEGeometry();
        return false;
    }

    const Vector2 end2D = start2D + refinedOffset *
        (kELength / std::sqrt(refinedDistanceSqr));
    Vector3 end = Vector3::From2D(end2D);
    end.y = NavMesh::GetHeightForPosition(end);
    ResetEGeometry();
    return CastE(start, end);
}

static bool PredictCastE(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !E.IsReady() || !ValidHeroTarget(target, kEMaxRange)) {
        return false;
    }

    const bool augmented = HasEAugment();
    std::vector<ETargetPrediction> targets;
    targets.reserve(5);
    targets.push_back(BuildETargetPrediction(target, true, augmented));
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (enemy.Address() == target.Address() ||
            !ValidHeroTarget(enemy, kEMaxRange + enemy.BoundingRadius())) {
            continue;
        }
        targets.push_back(BuildETargetPrediction(enemy, false, augmented));
    }

    auto candidates = BuildECandidates(
        player.Position().To2D(),
        targets,
        augmented);
    const int validationCount = std::min(
        static_cast<int>(candidates.size()),
        16);
    for (int i = 0; i < validationCount; ++i) {
        const auto& candidate = candidates[i];
        if (!candidate.PrimaryInitialHit || candidate.Score <= -FLT_MAX * 0.5f) {
            continue;
        }

        Vector3 start = Vector3::From2D(candidate.Start);
        Vector3 end = Vector3::From2D(candidate.End);
        start.y = NavMesh::GetHeightForPosition(start);
        end.y = NavMesh::GetHeightForPosition(end);
        E.From = start;
        E.RangeCheckFrom = start;
        E.Range = kELength;
        E.Speed = kESpeed;
        const auto validation = E.GetPrediction(target);
        const float hitRadius = kEWidth + std::clamp(
            target.BoundingRadius(),
            25.0f,
            80.0f);
        if (validation.Hitchance < HitChance::High ||
            DistancePointSegmentSqr(
                validation.GetUnitPosition().To2D(),
                candidate.Start,
                candidate.End) > hitRadius * hitRadius) {
            continue;
        }

        ResetEGeometry();
        return CastE(start, end);
    }

    ResetEGeometry();
    return CastESingleTargetFallback(
        target,
        targets.front().InitialPosition);
}

static std::vector<AIBaseClient> FarmUnits(bool jungle) {
    std::vector<AIBaseClient> result;
    if (jungle) {
        for (const auto& monster : GameObjects::Jungle()) {
            if (ValidTarget(monster, kEMaxRange)) {
                result.emplace_back(monster.Handle());
            }
        }
    } else {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (ValidTarget(minion, kEMaxRange)) {
                result.emplace_back(minion.Handle());
            }
        }
    }
    return result;
}

static LaserFarmLocation GetBestLaserFarmLocation(bool jungle) {
    const auto player = Player();
    LaserFarmLocation best;
    const auto units = FarmUnits(jungle);
    if (!player.IsValid() || units.empty()) {
        return best;
    }

    std::vector<Vector2> unitPositions;
    std::vector<Vector2> candidatePositions;
    unitPositions.reserve(units.size());
    candidatePositions.reserve(units.size() * units.size());
    for (const auto& unit : units) {
        unitPositions.push_back(unit.Position().To2D());
        candidatePositions.push_back(unit.Position().To2D());
    }
    const int count = static_cast<int>(unitPositions.size());
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            candidatePositions.push_back((unitPositions[i] + unitPositions[j]) * 0.5f);
        }
    }

    for (const auto& startUnit : units) {
        if (startUnit.DistanceToPlayer() > kERange) {
            continue;
        }
        const Vector2 start = startUnit.Position().To2D();
        for (const auto& candidate : candidatePositions) {
            const Vector2 direction = candidate - start;
            if (direction.LengthSqr() < 25.0f * 25.0f ||
                direction.LengthSqr() > kELength * kELength) {
                continue;
            }
            const Vector2 end = start + direction.Normalized() * kELength;
            int hitCount = 0;
            for (const auto& unitPosition : unitPositions) {
                if (DistancePointSegmentSqr(unitPosition, start, end) <= 140.0f * 140.0f) {
                    ++hitCount;
                }
            }
            if (hitCount > best.MinionsHit) {
                best.Start = start;
                best.End = end;
                best.MinionsHit = hitCount;
            }
        }
    }
    return best;
}

static bool PredictCastMinionE(bool jungle) {
    const auto farm = GetBestLaserFarmLocation(jungle);
    const int required = jungle ? 1 : Slider(LaneMenu, "EHits", 3);
    if (farm.MinionsHit < required) {
        return false;
    }
    return CastE(Vector3::From2D(farm.Start), Vector3::From2D(farm.End));
}

static float RDamage(const AIBaseClient& target) {
    if (!target.IsValid() || R.Level() <= 0) {
        return 0.0f;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return 0.0f;
    }
    const int level = std::clamp(R.Level(), 1, 3);
    const int extraTicks = Slider(ComboMenu, "RTicks", 3);
    const float initialDamage = 100.0f + 75.0f * static_cast<float>(level - 1) + 0.5f * player.AP();
    const float tickDamage = 65.0f + 40.0f * static_cast<float>(level - 1);
    return player.CalculateMagicDamage(
        target,
        initialDamage + tickDamage * static_cast<float>(extraTicks));
}

static bool IsPendingQKill(const AIHeroClient& target) {
    return QPendingAttack && target.IsValid() && PendingQTargetId == target.NetworkId();
}

static bool CanCastR(const AIHeroClient& target) {
    if (!Bool(ComboMenu, "ConserveR", true)) {
        return true;
    }
    if (!ValidHeroTarget(target, R.Range)) {
        return false;
    }
    if (Q.IsReady() && Bool(ComboMenu, "UseQ", true) &&
        ValidHeroTarget(target, Q.Range) &&
        Q.GetDamage(target) >= EffectiveMagicalHealth(target)) {
        return false;
    }
    if (IsPendingQKill(target)) {
        return false;
    }
    if (E.IsReady() && Bool(ComboMenu, "UseE", true) &&
        ValidHeroTarget(target, kEMaxRange) &&
        E.GetDamage(target) >= EffectiveMagicalHealth(target)) {
        return false;
    }
    if (SDK::HealthPrediction::GetPrediction(target, 250) <= 0.0f) {
        return false;
    }
    return !(target.HealthPercent() <= 10.0f && target.CountAllyHeroesInRange(400.0f) >= 2);
}

static bool CastW(const AIHeroClient& target) {
    if (!W.IsReady() || !ValidHeroTarget(target, W.Range)) {
        return false;
    }
    const int mode = List(ComboMenu, "WMode", 1);
    if (mode == 0) {
        return W.CastIfHitchanceMinimum(target, HitChance::Medium) == CastStates::SuccessfullyCasted;
    }
    if (mode == 1 && target.GetPathLength() < 2 &&
        (SDK::HasBuffOfType(target, SDK::BuffType::Slow) || HasHardCrowdControl(target))) {
        return W.CastIfHitchanceMinimum(target, HitChance::Medium) == CastStates::SuccessfullyCasted;
    }
    return false;
}

static bool LogicR() {
    if (!R.IsReady() || !IsRInitialCast() || List(ComboMenu, "RMode", 1) == 2) {
        return false;
    }

    const int minimumEnemies = Slider(ComboMenu, "RHits", 3);
    for (const auto& enemy : EnemyHeroes(R.Range)) {
        if (CountEnemiesNear(enemy.Position(), 300.0f) < minimumEnemies) {
            continue;
        }
        if (List(ComboMenu, "WMode", 1) == 2 && W.IsReady() && ValidHeroTarget(enemy, W.Range)) {
            const auto prediction = W.GetPrediction(enemy);
            if (prediction.Hitchance >= HitChance::Medium && W.Cast(prediction.GetCastPosition())) {
                return true;
            }
        }
        const auto prediction = R.GetPrediction(enemy, true);
        if (prediction.Hitchance >= HitChance::High &&
            prediction.AoeTargetsHitCount >= minimumEnemies && R.Cast(prediction.GetCastPosition())) {
            return true;
        }
    }

    for (const auto& enemy : EnemyHeroesByHealth(R.Range)) {
        if (RBlocked(enemy)) {
            continue;
        }
        const auto prediction = R.GetPrediction(enemy);
        if (prediction.Hitchance < HitChance::High) {
            continue;
        }
        if (List(ComboMenu, "RMode", 1) == 0 &&
            enemy.HealthPercent() <= Slider(ComboMenu, "RHealth", 40)) {
            return R.Cast(prediction.GetCastPosition());
        }
        if (List(ComboMenu, "RMode", 1) == 1 &&
            RDamage(enemy) >= EffectiveMagicalHealth(enemy) && CanCastR(enemy)) {
            return R.Cast(prediction.GetCastPosition());
        }
    }
    return false;
}

static void Combo() {
    if (!IsComboMode() || Orbwalker::IsWindingUp()) {
        return;
    }

    const int wMode = List(ComboMenu, "WMode", 1);
    if (wMode < 2 && W.IsReady()) {
        const auto target = GetMagicalTarget(W.Range);
        if (target.IsValid() && CastW(target)) {
            return;
        }
    }
    if (Bool(ComboMenu, "UseE", true) && E.IsReady()) {
        const auto target = GetMagicalTarget(kEMaxRange);
        if (target.IsValid() && PredictCastE(target)) {
            return;
        }
    }
    if (LogicR()) {
        return;
    }
    if (Bool(ComboMenu, "UseQ", true) && Q.IsReady()) {
        const auto target = GetMagicalTarget(Q.Range + 75.0f);
        if (target.IsValid() && ValidHeroTarget(target, Q.Range + target.BoundingRadius()) &&
            Q.Cast(target) == CastStates::SuccessfullyCasted) {
            QPendingAttack = true;
            PendingQSince = SDK::Variables::TickCount();
            if (Q.GetDamage(target) >= EffectiveMagicalHealth(target)) {
                PendingQTargetId = target.NetworkId();
            }
        }
    }
}

static void Harass() {
    if (!IsHarassMode() || Player().ManaPercent() <= Slider(HarassMenu, "Mana", 60)) {
        return;
    }
    if (Bool(HarassMenu, "UseQ", true) && Q.IsReady()) {
        const auto target = GetMagicalTarget(Q.Range);
        if (target.IsValid() && Q.Cast(target) == CastStates::SuccessfullyCasted) {
            return;
        }
    }
    if (Bool(HarassMenu, "UseE", true) && E.IsReady()) {
        const auto target = GetMagicalTarget(kEMaxRange);
        if (target.IsValid()) {
            (void)PredictCastE(target);
        }
    }
}

static void LaneClear() {
    if (!IsClearMode() || Player().ManaPercent() <= Slider(LaneMenu, "Mana", 40) ||
        !ShouldRunNow(LastClearTick, 100)) {
        return;
    }

    if (Bool(LaneMenu, "UseQ", true) && Q.IsReady()) {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!ValidTarget(minion, Q.Range)) {
                continue;
            }
            const float health = Q.GetHealthPrediction(minion);
            if (health > 0.0f && health < Q.GetDamage(minion) &&
                Q.Cast(minion) == CastStates::SuccessfullyCasted) {
                return;
            }
        }
    }
    if (Bool(LaneMenu, "UseE", true) && E.IsReady()) {
        (void)PredictCastMinionE(false);
    }
}

static void JungleClear() {
    if (!IsClearMode() || Player().ManaPercent() <= Slider(LaneMenu, "Mana", 40)) {
        return;
    }
    if (Bool(JungleMenu, "UseQ", true) && Q.IsReady()) {
        auto monsters = GameObjects::Jungle();
        std::sort(monsters.begin(), monsters.end(), [](const AIMinionClient& left, const AIMinionClient& right) {
            return left.MaxHealth() > right.MaxHealth();
        });
        for (const auto& monster : monsters) {
            if (ValidTarget(monster, Q.Range) &&
                Q.Cast(monster) == CastStates::SuccessfullyCasted) {
                return;
            }
        }
    }
    if (Bool(JungleMenu, "UseE", true) && E.IsReady()) {
        (void)PredictCastMinionE(true);
    }
}

static void Killsteal() {
    for (const auto& enemy : EnemyHeroesByHealth(kEMaxRange)) {
        if (Bool(KillstealMenu, "UseQ", true) && Q.IsReady() &&
            ValidHeroTarget(enemy, Q.Range) &&
            Q.GetDamage(enemy) >= EffectiveMagicalHealth(enemy) &&
            Q.Cast(enemy) == CastStates::SuccessfullyCasted) {
            PendingQTargetId = enemy.NetworkId();
            QPendingAttack = true;
            PendingQSince = SDK::Variables::TickCount();
            return;
        }
        if (Bool(KillstealMenu, "UseE", true) && E.IsReady() &&
            E.GetDamage(enemy) >= EffectiveMagicalHealth(enemy) && PredictCastE(enemy)) {
            return;
        }
    }
}

static void AutoWCC() {
    if (!Bool(MiscMenu, "AutoWCC", true) || !W.IsReady() ||
        !ShouldRunNow(LastAutoWTick, 100)) {
        return;
    }
    for (const auto& enemy : EnemyHeroes(W.Range)) {
        const auto prediction = W.GetPrediction(enemy);
        if ((HasHardCrowdControl(enemy) || prediction.Hitchance == HitChance::Immobile) &&
            W.Cast(prediction.GetCastPosition())) {
            return;
        }
    }
}

static void FollowR() {
    if (!Bool(ComboMenu, "FollowR", true) || !HasActiveChaosStorm()) {
        return;
    }
    const int now = SDK::Variables::TickCount();
    if (LastRFollowTick > 0 && now - LastRFollowTick < 500) {
        return;
    }
    const auto target = GetMagicalTarget(1100.0f);
    if (target.IsValid() && R.Cast(target.Position())) {
        LastRFollowTick = now;
    }
}

static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    const AIBaseClient target(args.Target.Handle());
    const auto player = Player();
    if (!Bool(MiscMenu, "DisableAA", false) || !target.IsHero() || !IsComboMode() ||
        player.Level() < Slider(MiscMenu, "DisableAALevel", 12)) {
        return;
    }
    const bool qUnavailable = !Q.IsReady() || player.Mana() < Q.Instance().ManaCost();
    const bool eUnavailable = !E.IsReady() || player.Mana() < E.Instance().ManaCost();
    args.Process = qUnavailable && eUnavailable && player.HasBuff("viktorpowertransferreturn");
}

static void OnNonKillableMinion(OrbwalkingActionArgs& args) {
    if ((!IsClearMode() && Orbwalker::ActiveMode() != OrbwalkingMode::LastHit) ||
        !Bool(LaneMenu, "UseQ", true) || !Q.IsReady() ||
        Player().ManaPercent() <= Slider(LaneMenu, "Mana", 40)) {
        return;
    }
    const AIBaseClient target(args.Target.Handle());
    if (!ValidTarget(target, Q.Range)) {
        return;
    }
    const float health = Q.GetHealthPrediction(target);
    if (health > 0.0f && health < Q.GetDamage(target)) {
        (void)Q.Cast(target);
    }
}

static void OnDoCast(const Events::ProcessSpellEventArgs& args) {
    if (Events::IsLocalPlayer(args.Sender) && HasName(args, "ViktorPowerTransfer")) {
        QPendingAttack = true;
        PendingQSince = SDK::Variables::TickCount();
    }
}

static void OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    if (Events::IsLocalPlayer(args.Sender) && HasName(args, "ViktorPowerTransferReturn")) {
        QPendingAttack = false;
        PendingQTargetId = 0;
        PendingQSince = 0;
    }
}

static void OnTeleport(const Events::Teleport::TeleportEventArgs& args) {
    if (!Bool(MiscMenu, "AutoWCC", true) || !W.IsReady() ||
        args.Type != TeleportType::Teleport || args.Status != TeleportStatus::Start ||
        !args.IsTarget) {
        return;
    }
    const AIBaseClient target(args.Object);
    if (ValidTarget(target, W.Range)) {
        (void)W.Cast(target.Position());
    }
}

static void OnInterruptable(const Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    if (args.DangerLevel < DangerLevel::High) {
        return;
    }
    const AIHeroClient target(args.Sender);
    if (!ValidHeroTarget(target, R.Range)) {
        return;
    }
    if (Bool(MiscMenu, "InterruptW", true) && W.IsReady() && ValidHeroTarget(target, W.Range) &&
        SDK::Game::Time() + 1.5f + W.Delay >= args.EndTime) {
        const auto prediction = W.GetPrediction(target);
        if (W.Cast(prediction.GetCastPosition())) {
            return;
        }
    }
    if (Bool(MiscMenu, "InterruptR", true) && R.IsReady() && IsRInitialCast()) {
        (void)R.Cast(target.Position());
    }
}

static void OnGapcloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "AntiGap", true) || !W.IsReady()) {
        return;
    }
    const auto player = Player();
    const AIHeroClient sender(args.Sender);
    if (player.IsValid() && ValidHeroTarget(sender, W.Range) &&
        args.Start.Distance2D(player.Position()) > args.End.Distance2D(player.Position()) &&
        args.End.Distance2D(player.Position()) <= W.Range) {
        (void)W.Cast(args.End);
    }
}

static void Drawing_OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "DrawQ", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFDEB887u, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawW", false) && W.IsReady()) {
        Drawing::DrawCircle(player.Position(), W.Range, 0xFFFF5A5Au, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawE", false) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), kERange, 0xFF9450DCu, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawMaxE", false) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), kEMaxRange, 0xFFFFDC46u, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawR", false) && R.IsReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFFFFA53Cu, 1.5f, 64);
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    if (!ShouldRunNow(LastUpdateTick, 35)) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling() || Game::IsChatOpen()) {
        return;
    }

    if (QPendingAttack && PendingQSince > 0 &&
        SDK::Variables::TickCount() - PendingQSince > 4000 &&
        !player.HasBuff("viktorpowertransferreturn")) {
        QPendingAttack = false;
        PendingQTargetId = 0;
        PendingQSince = 0;
    }

    FollowR();
    Killsteal();
    AutoWCC();
    Combo();
    Harass();
    if (IsClearMode()) {
        LaneClear();
        JungleClear();
    }
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.kuroaio.viktor", "Kuro - Viktor", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo Settings"));
    ComboMenu->Add(new MenuBool("UseQ", "Use Q", true));
    ComboMenu->Add(new MenuList("WMode", "Use W", { "Always", "Only Slow or CC", "With R", "Never" }, 1));
    ComboMenu->Add(new MenuBool("UseE", "Use E", true));
    ComboMenu->Add(new MenuList("RMode", "Use R", { "Health below percent", "Killable", "Disable" }, 1));
    ComboMenu->Add(new MenuSlider("RHealth", "R target health percent", 40, 0, 100));
    ComboMenu->Add(new MenuSlider("RTicks", "Extra R damage ticks", 3, 1, 6));
    ComboMenu->Add(new MenuSlider("RHits", "R minimum enemies", 3, 1, 5));
    ComboMenu->Add(new MenuBool("FollowR", "Auto follow with R", true));
    ComboMenu->Add(new MenuBool("ConserveR", "Save R if other spell kills", true));

    RBlacklistMenu = ComboMenu->AddSubMenu(new Menu("RBlacklist", "R Blacklist"));
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const std::string key = RBlacklistKey(enemy);
        std::string label = enemy.CharacterName();
        if (label.empty()) {
            label = "Enemy " + std::to_string(enemy.NetworkId());
        }
        RBlacklistMenu->Add(new MenuBool(key.c_str(), label.c_str(), false));
    }

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass", "Harass Settings"));
    HarassMenu->Add(new MenuBool("UseQ", "Use Q", true));
    HarassMenu->Add(new MenuBool("UseE", "Use E", true));
    HarassMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 60, 0, 100));

    LaneMenu = MenuRoot->AddSubMenu(new Menu("LaneClear", "Lane Clear Settings"));
    LaneMenu->Add(new MenuBool("UseQ", "Use Q", true));
    LaneMenu->Add(new MenuBool("UseE", "Use E", true));
    LaneMenu->Add(new MenuSlider("EHits", "E minimum minions", 3, 1, 6));
    LaneMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 40, 0, 100));

    JungleMenu = MenuRoot->AddSubMenu(new Menu("JungleClear", "Jungle Clear Settings"));
    JungleMenu->Add(new MenuBool("UseQ", "Use Q", true));
    JungleMenu->Add(new MenuBool("UseE", "Use E", true));

    KillstealMenu = MenuRoot->AddSubMenu(new Menu("Killsteal", "Killsteal Settings"));
    KillstealMenu->Add(new MenuBool("UseQ", "Use Q", true));
    KillstealMenu->Add(new MenuBool("UseE", "Use E", true));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc", "Misc Settings"));
    MiscMenu->Add(new MenuBool("DisableAA", "Disable AA in Combo", false));
    MiscMenu->Add(new MenuSlider("DisableAALevel", "Disable attacks from level", 12, 1, 18));
    MiscMenu->Add(new MenuBool("InterruptW", "W to interrupt spells", true));
    MiscMenu->Add(new MenuBool("InterruptR", "R to interrupt spells", true));
    MiscMenu->Add(new MenuBool("AutoWCC", "Auto W on CC targets", true));
    MiscMenu->Add(new MenuBool("AntiGap", "Use W against gapclosers", true));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Draw Settings"));
    DrawMenu->Add(new MenuBool("DrawQ", "Draw Q range", false));
    DrawMenu->Add(new MenuBool("DrawW", "Draw W range", false));
    DrawMenu->Add(new MenuBool("DrawE", "Draw E start range", false));
    DrawMenu->Add(new MenuBool("DrawMaxE", "Draw E maximum range", false));
    DrawMenu->Add(new MenuBool("DrawR", "Draw R range", false));

    MenuRoot->Attach();
}

static void RemoveMenu() {
    if (!MenuRoot) {
        return;
    }
    MenuManager::Instance().Remove(MenuRoot);
    MenuRoot = nullptr;
    ComboMenu = nullptr;
    RBlacklistMenu = nullptr;
    HarassMenu = nullptr;
    LaneMenu = nullptr;
    JungleMenu = nullptr;
    KillstealMenu = nullptr;
    MiscMenu = nullptr;
    DrawMenu = nullptr;
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 652.0f);
    W = Spell(SpellSlot::W, 800.0f);
    E = Spell(SpellSlot::E, kERange);
    R = Spell(SpellSlot::R, 700.0f);
    Q.SetTargetted(0.25f, 2000.0f);
    W.SetSkillshot(0.4f, 300.0f, FLT_MAX, false, SkillshotType::SkillshotCircle);
    E.SetSkillshot(0.0f, kEWidth, kESpeed, false, SkillshotType::SkillshotLine);
    R.SetSkillshot(0.6f, 450.0f, FLT_MAX, false, SkillshotType::SkillshotCircle);

    QPendingAttack = false;
    PendingQTargetId = 0;
    PendingQSince = 0;
    LastUpdateTick = 0;
    LastClearTick = 0;
    LastRFollowTick = 0;
    LastAutoWTick = 0;

    BuildMenu();
    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Drawing::OnDraw += &Drawing_OnDraw;
    Events::hook.OnDoCast += &OnDoCast;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Events::hook.OnTeleport += &OnTeleport;
    Events::hook.OnInterruptableTarget += &OnInterruptable;
    Events::hook.OnGapCloser += &OnGapcloser;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Orbwalker::OnNonKillableMinion += &OnNonKillableMinion;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>Kuro - Viktor loaded</font>");
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }
    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Drawing::OnDraw -= &Drawing_OnDraw;
    Events::hook.OnDoCast -= &OnDoCast;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Events::hook.OnTeleport -= &OnTeleport;
    Events::hook.OnInterruptableTarget -= &OnInterruptable;
    Events::hook.OnGapCloser -= &OnGapcloser;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Orbwalker::OnNonKillableMinion -= &OnNonKillableMinion;
    RemoveMenu();
    QPendingAttack = false;
    PendingQTargetId = 0;
    PendingQSince = 0;
    Loaded = false;
}

} // namespace Plugins::KuroAIO::Viktor