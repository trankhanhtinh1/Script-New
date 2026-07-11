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
        const bool endangered = EvadeGeometry::HeroThreatenedNow(
            threats,
            heroPos,
            heroRadius,
            state == EvadeControllerState::Idle
                ? config.planner.pathBuffer
                : std::min(config.planner.releaseBuffer, config.planner.endpointBuffer),
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
                    config.planner,
                    threats);
                unsafeCurrentPath = intendedPath.valid && intendedPath.walkable &&
                    (!intendedPath.pathSafe || !intendedPath.endpointSafe);
            }
        }

        if (!endangered) {
            HandleRelease();
            if (unsafeCurrentPath) command.StopUnsafeMovement();
            return;
        }
        state = state == EvadeControllerState::Idle
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
            config,
            now,
            &currentLocked);
        int planInterval = state == EvadeControllerState::FallbackEvade
            ? config.fallbackReplanIntervalMs
            : config.replanIntervalMs;
        if (state == EvadeControllerState::StrictEvade && lockedValid)
            planInterval = std::clamp(planInterval * 3, 120, 240);
        if (state == EvadeControllerState::FallbackEvade &&
            lastPlan.selected.firstCollisionTimeMs != FLT_MAX && lastPlanTick > 0) {
            const float remaining = lastPlan.selected.firstCollisionTimeMs -
                static_cast<float>(now - lastPlanTick);
            planInterval = std::min(
                planInterval,
                std::max(20, static_cast<int>(std::round(remaining * 0.2f))));
        }
        const bool shouldReplan = !lockedValid || serial != lastThreatSerial ||
            lastPlanTick == 0 || now - lastPlanTick >= std::max(20, planInterval);

        if (shouldReplan) {
            const EvadeControllerState previousState = state;
            state = EvadeControllerState::Assessing;
            PlannerResult plan = config.walkingEnabled
                ? EvadePlanner::FindBest(player, threats, config.planner)
                : PlannerResult{};
            lastPlan = plan;
            lastPlanTick = now;
            lastThreatSerial = serial;
            const bool weakWalkingPlan = !config.walkingEnabled || !plan.found || !plan.strictSafe ||
                plan.selected.timeMarginMs < config.evadeSpellMarginThresholdMs ||
                plan.selected.minimumClearance <
                    std::max(0.0f, config.planner.preferredClearance * 0.5f);
            if (config.evadeSpellsEnabled && weakWalkingPlan) {
                const EvadeSpellCastResult spell = spellEngine.TryUse(
                    player,
                    threats,
                    config.planner,
                    config.evadeSpellMinimumDanger);
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
            if (plan.found && (plan.strictSafe || config.leastDangerFallback)) {
                const float requiredGain = std::max(
                    8.0f,
                    config.planner.preferredClearance * 0.5f);
                const bool materiallySafer = plan.strictSafe && lockedValid &&
                    plan.selected.minimumClearance >= currentLocked.minimumClearance + requiredGain;
                const bool avoidsTurret = plan.strictSafe && lockedValid &&
                    plan.selected.turretPenalty + 20.0f < currentLocked.turretPenalty;
                const bool keepStrictLock = previousState == EvadeControllerState::StrictEvade &&
                    lockedValid && plan.strictSafe && !materiallySafer && !avoidsTurret;
                if (keepStrictLock) {
                    locked = currentLocked;
                    lastPlan.found = true;
                    lastPlan.strictSafe = true;
                    lastPlan.selected = locked;
                    state = EvadeControllerState::StrictEvade;
                } else {
                    locked = plan.selected;
                    state = plan.strictSafe
                        ? EvadeControllerState::StrictEvade
                        : EvadeControllerState::FallbackEvade;
                }
            } else {
                locked = {};
                state = EvadeControllerState::FallbackEvade;
            }
        }

        if (config.walkingEnabled && locked.valid && locked.walkable) {
            if (!command.MoveTo(
                    player,
                    locked.position,
                    config.moveIntervalMs,
                    config.moveRefreshMs)) {
                locked.valid = false;
                lastPlanTick = 0;
            }
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
        if (state == EvadeControllerState::StrictEvade) return current.strictSafe;
        if (state == EvadeControllerState::FallbackEvade) {
            if (current.strictSafe) return false;
            if (current.endpointDanger > locked.endpointDanger) return false;
            if (current.maxDanger > locked.maxDanger) return false;
            if (current.dangerExposureMs > locked.dangerExposureMs + 35.0f) return false;
            return true;
        }
        return false;
    }

    void HandleRelease() {
        if (state == EvadeControllerState::Idle && !command.ControlActive()) return;
        command.EndControl();
        state = EvadeControllerState::Idle;
        locked = {};
        lastPlan = {};
        spellHoldUntilTick = 0;
        lastThreatSerial = ThreatDetector::ChangeSerial();
    }
};

}
