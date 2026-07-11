#pragma once

#include "EvadeCommandEngine.h"
#include "EvadePlanner.h"
#include "../Detection/ThreatDetector.h"
#include "../EvadeSpells/EvadeSpellEngine.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace ZDEvade {

struct EvadeRuntimeConfig {
    bool enabled = true;
    bool walkingEnabled = true;
    bool evadeSpellsEnabled = true;
    bool leastDangerFallback = true;
    int minimumDanger = 1;
    int evadeSpellMinimumDanger = 3;
    float evadeSpellMarginThresholdMs = 45.0f;
    const ThreatRuleMap* threatRules = nullptr;
    int moveIntervalMs = 75;
    int moveRefreshMs = 260;
    int replanIntervalMs = 70;
    int fallbackReplanIntervalMs = 45;
    EvadeSettings planner;
};

class EvadeController {
public:
    void Reset() {
        command.EndControl();
        spellEngine.Reset();
        spellHoldUntilTick = 0;
        state = EvadeControllerState::Idle;
        locked = {};
        lastPlan = {};
        lastThreatSerial = -1;
        lastPlanTick = 0;
        safeSinceTick = 0;
        unsafePathStopped = false;
    }

    void Update(const EvadeRuntimeConfig& config) {
        const int now = SDK::Variables::TickCount();
        const auto player = SDK::ObjectManager::Player();
        const std::vector<Threat> snapshot = ThreatDetector::Snapshot();
        if (!config.enabled || (!config.walkingEnabled && !config.evadeSpellsEnabled) ||
            !player.IsValid() || player.IsDead()) {
            Reset();
            return;
        }

        const std::vector<Threat> threats = FilterThreats(
            snapshot,
            config.minimumDanger,
            player.HealthPercent(),
            config.threatRules,
            now);
        const Vec2 heroPos = player.ServerPosition().To2D();
        const float heroRadius = std::max(10.0f, player.BoundingRadius());
        const bool controlWasActive = command.ControlActive() ||
            state != EvadeControllerState::Idle;
        EvadeRuntimeConfig evadeConfig = config;
        evadeConfig.planner.endpointBuffer = std::max(
            config.planner.endpointBuffer,
            config.planner.releaseBuffer + 8.0f);
        const bool endangered = EvadeGeometry::HeroThreatenedNow(
            threats,
            heroPos,
            heroRadius,
            controlWasActive
                ? std::max(config.planner.pathBuffer, config.planner.releaseBuffer)
                : config.planner.pathBuffer,
            now,
            config.planner.maxThreatHorizonMs);
        bool unsafeCurrentPath = false;
        if (config.walkingEnabled && player.HasPath()) {
            const std::vector<Vec2> currentPath = BuildCurrentPath(
                player,
                player.PathEnd().To2D());
            if (currentPath.size() >= 2) {
                const CandidateEvaluation intendedPath = EvadeGeometry::EvaluatePathCandidate(
                    currentPath,
                    PlannerCandidateSource::Cursor,
                    -1,
                    SDK::Game::CursorPos().To2D(),
                    player.ServerPosition().y,
                    std::max(50.0f, player.MoveSpeed()),
                    heroRadius,
                    now,
                    controlWasActive ? evadeConfig.planner : config.planner,
                    threats);
                unsafeCurrentPath = intendedPath.valid && intendedPath.walkable &&
                    (!intendedPath.pathSafe || !intendedPath.endpointSafe);
            }
        }

        if (!endangered) {
            if (spellHoldUntilTick > now) {
                command.BeginControl();
                safeSinceTick = 0;
                return;
            }
            if (controlWasActive) {
                if (safeSinceTick == 0) safeSinceTick = now;
                if (now - safeSinceTick < 75) {
                    state = EvadeControllerState::Release;
                    return;
                }
                HandleRelease();
            } else {
                safeSinceTick = 0;
            }
            if (unsafeCurrentPath) {
                if (!unsafePathStopped)
                    unsafePathStopped = command.StopUnsafeMovement();
            } else {
                unsafePathStopped = false;
            }
            return;
        }
        safeSinceTick = 0;
        unsafePathStopped = false;
        state = state == EvadeControllerState::Idle ||
                state == EvadeControllerState::Release
            ? EvadeControllerState::Assessing
            : state;
        if (spellHoldUntilTick > now) {
            command.BeginControl();
            return;
        }
        if (config.walkingEnabled) command.BeginControl();
        else command.EndControl();

        const int serial = ThreatDetector::ChangeSerial();
        CandidateEvaluation currentLocked;
        const bool lockedValid = ValidateLocked(
            player,
            threats,
            evadeConfig,
            now,
            &currentLocked);
        if (lockedValid && state == EvadeControllerState::FallbackEvade &&
            currentLocked.strictSafe) {
            locked = currentLocked;
            state = EvadeControllerState::StrictEvade;
        }
        const bool periodicFallbackReplan =
            state == EvadeControllerState::FallbackEvade &&
            lastPlanTick > 0 &&
            now - lastPlanTick >= std::max(90, evadeConfig.fallbackReplanIntervalMs);
        const bool shouldReplan = !lockedValid || serial != lastThreatSerial ||
            lastPlanTick == 0 || periodicFallbackReplan;

        if (shouldReplan) {
            const EvadeControllerState previousState = state;
            state = EvadeControllerState::Assessing;
            PlannerResult plan = evadeConfig.walkingEnabled
                ? EvadePlanner::FindBest(player, threats, evadeConfig.planner)
                : PlannerResult{};
            lastPlan = plan;
            lastPlanTick = now;
            lastThreatSerial = serial;
            const bool weakWalkingPlan = !evadeConfig.walkingEnabled || !plan.found || !plan.strictSafe ||
                plan.selected.timeMarginMs < evadeConfig.evadeSpellMarginThresholdMs ||
                plan.selected.minimumClearance <
                    std::max(0.0f, evadeConfig.planner.preferredClearance * 0.5f);
            if (evadeConfig.evadeSpellsEnabled && weakWalkingPlan) {
                const EvadeSpellCastResult spell = spellEngine.TryUse(
                    player,
                    threats,
                    evadeConfig.planner,
                    evadeConfig.evadeSpellMinimumDanger);
                if (spell.casted) {
                    command.BeginControl();
                    locked = spell.destination;
                    locked.source = PlannerCandidateSource::EvadeSpell;
                    locked.strictSafe = true;
                    lastPlan.found = true;
                    lastPlan.strictSafe = true;
                    lastPlan.selected = locked;
                    state = EvadeControllerState::StrictEvade;
                    spellHoldUntilTick = spell.holdUntilTick;
                    return;
                }
            }
            if (plan.found && (plan.strictSafe || evadeConfig.leastDangerFallback)) {
                const float requiredGain = std::max(
                    8.0f,
                    evadeConfig.planner.preferredClearance * 0.5f);
                const bool materiallySafer = plan.strictSafe && lockedValid &&
                    plan.selected.minimumClearance >= currentLocked.minimumClearance + requiredGain;
                const bool avoidsTurret = plan.strictSafe && lockedValid &&
                    plan.selected.turretPenalty + 20.0f < currentLocked.turretPenalty;
                const bool keepStrictLock = previousState == EvadeControllerState::StrictEvade &&
                    lockedValid && plan.strictSafe && !materiallySafer && !avoidsTurret;
                const bool keepFallbackLock = previousState == EvadeControllerState::FallbackEvade &&
                    lockedValid && !plan.strictSafe &&
                    !FallbackMateriallyBetter(plan.selected, currentLocked);
                if (keepStrictLock || keepFallbackLock) {
                    locked = currentLocked;
                    lastPlan.found = true;
                    lastPlan.strictSafe = locked.strictSafe;
                    lastPlan.selected = locked;
                    state = locked.strictSafe
                        ? EvadeControllerState::StrictEvade
                        : EvadeControllerState::FallbackEvade;
                } else {
                    locked = plan.selected;
                    state = plan.strictSafe
                        ? EvadeControllerState::StrictEvade
                        : EvadeControllerState::FallbackEvade;
                }
            } else if (lockedValid) {
                locked = currentLocked;
                lastPlan.found = true;
                lastPlan.strictSafe = locked.strictSafe;
                lastPlan.selected = locked;
                state = locked.strictSafe
                    ? EvadeControllerState::StrictEvade
                    : EvadeControllerState::FallbackEvade;
            } else {
                locked = {};
                state = EvadeControllerState::FallbackEvade;
            }
        }

        if (evadeConfig.walkingEnabled && locked.valid && locked.walkable) {
            if (!command.MoveTo(
                    player,
                    locked.position,
                    evadeConfig.moveIntervalMs,
                    evadeConfig.moveRefreshMs)) {
                locked.valid = false;
                lastPlanTick = 0;
            }
        } else if (evadeConfig.walkingEnabled && player.HasPath()) {
            command.StopUnsafeMovement();
        }
    }

    EvadeControllerState State() const { return state; }
    bool IsEvading() const {
        return state == EvadeControllerState::StrictEvade ||
               state == EvadeControllerState::FallbackEvade ||
               state == EvadeControllerState::Assessing;
    }
    const CandidateEvaluation& Locked() const { return locked; }
    const PlannerResult& LastPlan() const { return lastPlan; }
    const EvadeCommandEngine& Command() const { return command; }

    bool ShouldBlockMove(const Vec2& destination,
                         const EvadeRuntimeConfig& config) const {
        if (!config.enabled || !config.walkingEnabled || !destination.IsValid() || destination.IsZero())
            return false;
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) return false;
        const int now = SDK::Variables::TickCount();
        const std::vector<Threat> threats = FilterThreats(
            ThreatDetector::Snapshot(),
            config.minimumDanger,
            player.HealthPercent(),
            config.threatRules,
            now);
        if (threats.empty()) return false;
        const CandidateEvaluation movement = EvadeGeometry::EvaluateCandidate(
            destination,
            PlannerCandidateSource::Cursor,
            -1,
            player.ServerPosition().To2D(),
            SDK::Game::CursorPos().To2D(),
            player.ServerPosition().y,
            std::max(50.0f, player.MoveSpeed()),
            std::max(10.0f, player.BoundingRadius()),
            now,
            config.planner,
            threats);
        return movement.valid && movement.walkable &&
            (!movement.pathSafe || !movement.endpointSafe);
    }

private:
    EvadeControllerState state = EvadeControllerState::Idle;
    EvadeCommandEngine command;
    EvadeSpellEngine spellEngine;
    CandidateEvaluation locked;
    PlannerResult lastPlan;
    int lastThreatSerial = -1;
    int lastPlanTick = 0;
    int spellHoldUntilTick = 0;
    int safeSinceTick = 0;
    bool unsafePathStopped = false;

    static std::vector<Threat> FilterThreats(const std::vector<Threat>& input,
                                             int minimumDanger,
                                             float healthPercent,
                                             const ThreatRuleMap* rules,
                                             int now) {
        std::vector<Threat> output;
        output.reserve(input.size());
        for (const auto& threat : input) {
            if (threat.IsExpiredAt(now)) continue;
            Threat filtered = threat;
            bool hasRule = false;
            if (rules) {
                const auto rule = rules->find(threat.SpellName());
                if (rule != rules->end()) {
                    hasRule = true;
                    if (!rule->second.enabled || healthPercent > rule->second.dodgeHealthPercent) continue;
                    filtered.dangerOverride = std::clamp(rule->second.danger, 1, 4);
                }
            }
            if (!hasRule && threat.DefaultOff()) continue;
            if (filtered.Danger() >= std::max(1, minimumDanger)) output.push_back(std::move(filtered));
        }
        return output;
    }

    static std::vector<Vec2> BuildCurrentPath(const SDK::AIHeroClient& player,
                                              const Vec2& fallbackEnd) {
        std::vector<Vec2> path;
        const Vec2 start = player.ServerPosition().To2D();
        if (!start.IsValid() || start.IsZero()) return path;
        path.push_back(start);
        for (const Vec3& waypoint : player.GetPath(24)) {
            const Vec2 point = waypoint.To2D();
            if (!point.IsValid() || point.IsZero() || path.back().Distance(point) <= 1.0f) continue;
            path.push_back(point);
        }
        if (fallbackEnd.IsValid() && !fallbackEnd.IsZero() &&
            path.back().Distance(fallbackEnd) > 1.0f) path.push_back(fallbackEnd);
        return path;
    }

    bool ValidateLocked(const SDK::AIHeroClient& player,
                        const std::vector<Threat>& threats,
                        const EvadeRuntimeConfig& config,
                        int now,
                        CandidateEvaluation* evaluationOut) const {
        if (!locked.valid || !locked.walkable || locked.position.IsZero()) return false;
        if (player.ServerPosition().To2D().Distance(locked.position) < 18.0f) return false;
        const bool followsLockedPath = player.HasPath() &&
            player.PathEnd().To2D().Distance(locked.position) <= 80.0f;
        const std::vector<Vec2> currentPath = followsLockedPath
            ? BuildCurrentPath(player, locked.position)
            : std::vector<Vec2>();
        CandidateEvaluation current = currentPath.size() >= 2
            ? EvadeGeometry::EvaluatePathCandidate(
                currentPath,
                locked.source,
                locked.sourceThreatId,
                SDK::Game::CursorPos().To2D(),
                player.ServerPosition().y,
                std::max(50.0f, player.MoveSpeed()),
                std::max(10.0f, player.BoundingRadius()),
                now,
                config.planner,
                threats)
            : EvadeGeometry::EvaluateCandidate(
                locked.position,
                locked.source,
                locked.sourceThreatId,
                player.ServerPosition().To2D(),
                SDK::Game::CursorPos().To2D(),
                player.ServerPosition().y,
                std::max(50.0f, player.MoveSpeed()),
                std::max(10.0f, player.BoundingRadius()),
                now,
                config.planner,
                threats);
        current.enemyDistance = locked.enemyDistance;
        current.turretPenalty = locked.turretPenalty;
        if (evaluationOut) *evaluationOut = current;
        if (!current.valid || !current.walkable) return false;
        if (state == EvadeControllerState::StrictEvade) {
            if (current.strictSafe) return true;
            return current.endpointSafe && !current.reenteredDanger &&
                current.exitDistance + 4.0f < current.travelDistance &&
                current.firstCollisionTimeMs <= config.planner.inputDelayMs + 35.0f;
        }
        if (state == EvadeControllerState::FallbackEvade) {
            if (current.strictSafe) return true;
            if (current.endpointDanger > locked.endpointDanger) return false;
            if (current.maxDanger > locked.maxDanger) return false;
            if (current.dangerExposureMs > locked.dangerExposureMs + 35.0f) return false;
            return true;
        }
        return false;
    }

    static bool FallbackMateriallyBetter(const CandidateEvaluation& candidate,
                                         const CandidateEvaluation& current) {
        if (candidate.strictSafe != current.strictSafe) return candidate.strictSafe;
        if (candidate.endpointDanger != current.endpointDanger)
            return candidate.endpointDanger < current.endpointDanger;
        if (candidate.maxDanger != current.maxDanger)
            return candidate.maxDanger < current.maxDanger;
        if (candidate.collisionCount != current.collisionCount)
            return candidate.collisionCount < current.collisionCount;
        if (candidate.dangerExposureMs + 70.0f < current.dangerExposureMs) return true;
        if (candidate.firstCollisionTimeMs != FLT_MAX &&
            current.firstCollisionTimeMs != FLT_MAX &&
            candidate.firstCollisionTimeMs > current.firstCollisionTimeMs + 90.0f) return true;
        return candidate.minimumClearance > current.minimumClearance + 18.0f &&
            candidate.travelDistance <= current.travelDistance + 70.0f;
    }

    void HandleRelease() {
        if (state == EvadeControllerState::Idle && !command.ControlActive()) return;
        command.EndControl();
        state = EvadeControllerState::Idle;
        locked = {};
        lastPlan = {};
        spellHoldUntilTick = 0;
        safeSinceTick = 0;
        unsafePathStopped = false;
        lastThreatSerial = ThreatDetector::ChangeSerial();
    }
};

}
