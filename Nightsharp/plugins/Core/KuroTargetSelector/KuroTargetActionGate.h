#pragma once

#include "KuroTargetSelectorContracts.h"

#include "../../../sdk/Enumerations/ChampionId.h"
#include "../../../sdk/Enumerations/SpellSlot.h"

#include "../../../sdk/Extensions/Unit.h"
#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/Math/Collision.h"
#include "../../../sdk/Utils/Invulnerable.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>

#include <cstring>

namespace SDK::KuroTargetSelector {

struct ActionGateResult {
    bool Legal = false;
    RejectReason Rejection = RejectReason::None;
};

// One gate is shared by ranking and execution.  Ranking can consume the
// targetability sample captured in the current snapshot; execution keeps a
// live recheck for race safety.  IsDead remains the cheap first filter.
class KuroTargetActionGate final {
public:
    static ActionGateResult Evaluate(
        const TargetRequest& request,
        const AIHeroClient& target) {
        if (!target.IsValid() || target.NetworkId() <= 0) {
            return Evaluate(request, target, false);
        }
        if (target.IsDead() && !target.IsZombie()) {
            ActionGateResult result{};
            result.Rejection = RejectReason::Dead;
            return result;
        }
        return Evaluate(request, target, target.IsTargetable());
    }

    static ActionGateResult Evaluate(
        const TargetRequest& request,
        const AIHeroClient& target,
        bool targetable) {
        ActionGateResult result{};
        const auto player = GameObjects::Player();

        if (!target.IsValid()) {
            result.Rejection = RejectReason::Invalid;
            return result;
        }
        if (target.NetworkId() <= 0) {
            result.Rejection = RejectReason::Despawned;
            return result;
        }
        if (target.IsDead() && !target.IsZombie()) {
            result.Rejection = RejectReason::Dead;
            return result;
        }
        if (player.IsValid() && target.Team() == player.Team()) {
            result.Rejection = RejectReason::WrongTeam;
            return result;
        }

        // A target that cannot currently be acted on is rejected from the
        // snapshot sample; stasis is retained as a diagnostic detail.
        if (!targetable) {
            result.Rejection = IsStasis(target)
                ? RejectReason::Stasis
                : RejectReason::Untargetable;
            return result;
        }

        const bool requireVisible = request.RequireVisible ||
            request.Route.RequireVisible;
        if (requireVisible && !target.IsVisible()) {
            result.Rejection = RejectReason::NotVisible;
            return result;
        }

        if (target.IsInvulnerable() ||
            Utils::Invulnerable::Check(
                target,
                request.Damage.Type,
                request.Damage.IgnoreShields)) {
            result.Rejection = RejectReason::Invulnerable;
            return result;
        }

        if (request.Route.IntendedTargetId != 0 &&
            request.Route.IntendedTargetId != target.NetworkId()) {
            result.Rejection = RejectReason::RouteIllegal;
            return result;
        }

        if (request.Range > 0.0f && request.Range < FLT_MAX) {
            const Vector3 source = ResolveSource(request, player);
            const Vector3 destination = ResolveRangeDestination(request, target);
            if (!source.IsValid() || source.IsZero() ||
                !destination.IsValid() || destination.IsZero()) {
                result.Rejection = RejectReason::OutOfRange;
                return result;
            }

            float allowedRange = request.Range;
            if (request.Route.Kind == RouteKind::AutoAttack &&
                !request.Route.RangeIncludesHitboxes) {
                const float sourceRadius = request.Route.SourceBoundingRadius > 0.0f
                    ? request.Route.SourceBoundingRadius
                    : (player.IsValid() ? player.BoundingRadius() : 0.0f);
                allowedRange += std::max(0.0f, sourceRadius) +
                    std::max(0.0f, target.BoundingRadius());
            }
            if (source.DistanceSqr2D(destination) >
                allowedRange * allowedRange) {
                result.Rejection = RejectReason::OutOfRange;
                return result;
            }
        }

        if (request.Phase == DecisionPhase::Execution) {
            if (request.Route.SpellShieldAtImpact) {
                result.Rejection = RejectReason::SpellShield;
                return result;
            }
            if (request.Route.ImmunityAtImpact) {
                result.Rejection = RejectReason::Immunity;
                return result;
            }
        }

        result.Rejection = EvaluateRoute(request, target, player);
        result.Legal = result.Rejection == RejectReason::None;
        return result;
    }

    static bool IsLegal(const TargetRequest& request,
                        const AIHeroClient& target) {
        return Evaluate(request, target).Legal;
    }

    static bool IsProjectileRoute(const RouteDescriptor& route) {
        return route.ProjectileWallCheck ||
            route.Kind == RouteKind::AutoAttack ||
            route.Kind == RouteKind::UnitProjectile ||
            route.Kind == RouteKind::SkillshotProjectile ||
            route.Kind == RouteKind::ChargedProjectile;
    }

    static bool IsApheliosSeverumMainHand(const AIHeroClient& player) {
        if (!player.IsValid() ||
            ChampionIdFromName(player.CharacterName().c_str()) !=
                ChampionId::Aphelios) {
            return false;
        }
        const auto q = player.Spellbook().GetSpell(SpellSlot::Q);
        if (!q.IsValid()) {
            return false;
        }
        return _stricmp(q.ScriptName().c_str(), "ApheliosSeverumQ") == 0 ||
            _stricmp(q.Name().c_str(), "ApheliosSeverumQ") == 0 ||
            _stricmp(q.IconName().c_str(), "ApheliosSeverumQ") == 0;
    }

    static bool IsProjectileWallBlocked(const Vector3& start,
                                        const Vector3& destination,
                                        float radius = 0.0f) {
        if (!start.IsValid() || start.IsZero() ||
            !destination.IsValid() || destination.IsZero()) {
            return false;
        }
        return Collision::HasProjectileWallCollision(
            start, destination, std::max(0.0f, radius));
    }
    // Severum basic attacks bypass Yasuo/Samira projectile walls, while Mel
    // W remains a valid blocker for the same auto attack.

    static bool IsAutoAttackWallBlocked(const Vector3& start,
                                        const Vector3& destination,
                                        float radius,
                                        const AIHeroClient& player) {
        if (IsApheliosSeverumMainHand(player)) {
            return Collision::HasMelWallCollision(
                start, destination, std::max(0.0f, radius));
        }
        return IsProjectileWallBlocked(start, destination, radius);
    }

    static TargetRequest MakeAutoAttackRequest(
        const Vector3& source,
        float range,
        DecisionPhase phase = DecisionPhase::Planning,
        std::uint32_t requesterId = 0,
        bool rangeIncludesHitboxes = false) {
        TargetRequest request{};
        request.RequesterId = requesterId;
        request.Purpose = TargetPurpose::AutoAttack;
        request.Phase = phase;
        request.Source = source;
        request.Range = range;
        request.Damage.Type = DamageType::Physical;
        request.Route.Kind = RouteKind::AutoAttack;
        request.Route.Start = source;
        const auto player = GameObjects::Player();
        request.Route.SourceBoundingRadius = player.IsValid()
            ? std::max(0.0f, player.BoundingRadius())
            : 0.0f;
        request.Route.RangeIncludesHitboxes = rangeIncludesHitboxes;
        request.Route.ProjectileWallCheck = true;
        request.Route.RequireLineOfSight = true;
        return request;
    }

private:
    static Vector3 ResolveSource(const TargetRequest& request,
                                 const AIHeroClient& player) {
        if (request.Route.Start.IsValid() && !request.Route.Start.IsZero()) {
            return request.Route.Start;
        }
        if (request.Source.IsValid() && !request.Source.IsZero()) {
            return request.Source;
        }
        return player.IsValid() ? player.Position() : Vector3();
    }

    static Vector3 ResolveDestination(const TargetRequest& request,
                                      const AIHeroClient& target) {
        if (request.Route.Prediction.IsValid() &&
            !request.Route.Prediction.IsZero()) {
            return request.Route.Prediction;
        }
        if (request.Route.Destination.IsValid() &&
            !request.Route.Destination.IsZero()) {
            return request.Route.Destination;
        }
        return target.Position();
    }


    static Vector3 ResolveRangeDestination(const TargetRequest& request,
                                           const AIHeroClient& target) {
        switch (request.Route.Kind) {
        case RouteKind::SkillshotProjectile:
        case RouteKind::ChargedProjectile:
        case RouteKind::Mobility:
        case RouteKind::Custom:
            return ResolveDestination(request, target);
        case RouteKind::AutoAttack:
        case RouteKind::UnitProjectile:
        case RouteKind::NonProjectile:
        default:
            return target.Position();
        }
    }

    static RejectReason EvaluateRoute(const TargetRequest& request,
                                      const AIHeroClient& target,
                                      const AIHeroClient& player) {
        const auto& route = request.Route;
        if (route.RequireLineOfSight && route.LineOfSightKnown &&
            !route.LineOfSightClear) {
            return RejectReason::RouteIllegal;
        }

        if (route.CollisionCheck && route.RequireNoCollision &&
            route.PredictionCollides) {
            return RejectReason::Collision;
        }
        if (route.MinimumHitChance > static_cast<int>(HitChance::None)) {
            const bool predictionSupplied = route.PredictionAvailable ||
                route.PredictionCollides ||
                route.PredictionHitChance !=
                    static_cast<int>(HitChance::None);
            const bool deferredPlanningPrediction =
                request.Phase == DecisionPhase::Planning &&
                !predictionSupplied &&
                (route.PredictionSpell != nullptr ||
                 route.PredictionType != SpellType::None);
            if (!deferredPlanningPrediction &&
                (!route.PredictionAvailable ||
                 route.PredictionHitChance < route.MinimumHitChance)) {
                return RejectReason::PredictionLow;
            }
        }

        if (!IsProjectileRoute(route)) {
            return RejectReason::None;
        }

        // Starting a charge is legal without a wall check.  Release is an
        // execution action and must validate the actual release destination.
        if (route.IsChargeStart && !route.IsChargedRelease) {
            return RejectReason::None;
        }

        const Vector3 fallbackStart = ResolveSource(request, player);
        const Vector3 fallbackEnd = ResolveDestination(request, target);
        const float radius = std::max(0.0f, route.ProjectileRadius);
        const auto wallBlocked = [&](const Vector3& start,
                                     const Vector3& destination,
                                     float segmentRadius) {
            return route.Kind == RouteKind::AutoAttack
                ? IsAutoAttackWallBlocked(
                    start, destination, segmentRadius, player)
                : IsProjectileWallBlocked(
                    start, destination, segmentRadius);
        };

        if (route.CheckAllSegments && route.SegmentCount > 0) {
            const std::size_t count =
                std::min(route.SegmentCount, route.Segments.size());
            for (std::size_t i = 0; i < count; ++i) {
                const auto& segment = route.Segments[i];
                if (segment.Projectile && wallBlocked(
                        segment.Start, segment.End,
                        std::max(radius, segment.Radius))) {
                    return RejectReason::ProjectileWall;
                }
            }
        } else if (wallBlocked(fallbackStart, fallbackEnd, radius)) {
            return RejectReason::ProjectileWall;
        }

        // A shadow/ball/object origin is an additional segment only when the
        // caller explicitly marks the route as multi-origin.  Non-projectile
        // mobility and custom actions therefore remain wall-legal.
        if (route.CheckAllSegments &&
            route.SecondarySource.IsValid() &&
            !route.SecondarySource.IsZero() &&
            wallBlocked(route.SecondarySource, fallbackEnd, radius)) {
            return RejectReason::ProjectileWall;
        }
        return RejectReason::None;
    }

    static bool IsStasis(const AIHeroClient& target) {
        return target.HasBuff("bardrstasis") ||
            target.HasBuff("zhonyasringshield") ||
            target.HasBuff("lissandrarself") ||
            target.HasBuff("vladimirsanguinepool") ||
            target.HasBuff("fizztrickslippery");
    }

};

} // namespace SDK::KuroTargetSelector

namespace Plugins::KuroTargetSelector {
using ::SDK::KuroTargetSelector::ActionGateResult;
using ::SDK::KuroTargetSelector::KuroTargetActionGate;
} // namespace Plugins::KuroTargetSelector
