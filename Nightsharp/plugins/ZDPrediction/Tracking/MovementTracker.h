#pragma once

#include "../Analysis/MovementPatternAnalyzer.h"
#include "../Learning/TrainedProfile.h"
#include "../Math/Vector2.h"
#include "../../../sdk/SDK.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ZDPrediction {

struct MovementSnapshot {
    bool valid = false;
    bool moving = false;
    bool visible = false;
    bool historyReliable = false;
    bool positionDiscontinuity = false;
    Math::Vector2 position = {};
    Math::Vector2 recentCenter = {};
    Math::Vector2 velocity = {};
    Math::Vector2 acceleration = {};
    double angularVelocity = 0.0;
    double averageSpeed = 0.0;
    double directionStability = 0.0;
    double speedStability = 0.0;
    double pathAgeSeconds = 10.0;
    double stationarySeconds = 0.0;
    double visibleSeconds = 0.0;
    double positionStableSeconds = 0.0;
    double directionStableSeconds = 10.0;
    double sampleSpanSeconds = 0.0;
    double pathChangesPerSecond = 0.0;
    double directionReversalsPerSecond = 0.0;
    double displacementEfficiency = 1.0;
    int directionReversalCount = 0;
    int repeatedDestinationCount = 0;
    std::vector<Math::Vector2> path;
};

class MovementTracker {
public:
    static void Initialize() {
        if (initialized_) return;
        initialized_ = true;
        SDK::Events::AddOnNewPath(&OnNewPath);
        SDK::Events::AddOnGameUpdate(&OnGameUpdate);
        Update();
    }

    static void Shutdown() {
        if (!initialized_) return;
        initialized_ = false;
        SDK::Events::RemoveOnGameUpdate(&OnGameUpdate);
        SDK::Events::RemoveOnNewPath(&OnNewPath);
        AcquireSRWLockExclusive(&lock_);
        tracks_.clear();
        sourceInitialized_ = false;
        sourcePosition_ = {};
        sourceTick_ = 0;
        sourceDiscontinuityTick_ = 0;
        ReleaseSRWLockExclusive(&lock_);
    }

    static void Update() {
        if (!initialized_) return;
        const int now = SDK::Variables::TickCount();
        UpdateSource();
        for (const auto& hero : SDK::ObjectManager::EnemyHeroes()) {
            if (!hero.IsValid()) continue;
            if (hero.IsDead()) {
                AcquireSRWLockExclusive(&lock_);
                tracks_.erase(hero.NetworkId());
                ReleaseSRWLockExclusive(&lock_);
                continue;
            }
            UpdateUnit(hero, now);
        }
    }

    static bool ObserveSourcePosition(const Math::Vector2& current,
                                      double moveSpeed,
                                      int holdMs = 120) {
        if (!current.IsFinite()) return true;
        const int now = SDK::Variables::TickCount();
        AcquireSRWLockExclusive(&lock_);
        if (sourceInitialized_) {
            const double elapsed = static_cast<double>(now - sourceTick_) / 1000.0;
            if (MovementPatternAnalyzer::IsPositionDiscontinuity(
                    sourcePosition_, current, elapsed, moveSpeed, 140.0)) {
                sourceDiscontinuityTick_ = now;
            }
        }
        sourceInitialized_ = true;
        sourcePosition_ = current;
        sourceTick_ = now;
        const bool recent = sourceDiscontinuityTick_ > 0 &&
            now - sourceDiscontinuityTick_ < std::max(0, holdMs);
        ReleaseSRWLockExclusive(&lock_);
        return recent;
    }

    static bool SourceRecentlyDiscontinuous(int windowMs = 120) {
        const int now = SDK::Variables::TickCount();
        AcquireSRWLockShared(&lock_);
        const bool recent = sourceDiscontinuityTick_ > 0 &&
            now - sourceDiscontinuityTick_ < std::max(0, windowMs);
        ReleaseSRWLockShared(&lock_);
        return recent;
    }

    static MovementSnapshot Snapshot(const SDK::AIBaseClient& unit, int historyWindowMs) {
        MovementSnapshot snapshot;
        if (!unit.IsValid()) return snapshot;

        const int now = SDK::Variables::TickCount();
        const Vec3 server = ResolvePosition(unit);
        const Vec2 position2D = server.To2D();
        snapshot.valid = position2D.IsValid() && !position2D.IsZero();
        snapshot.position = ToMath(position2D);
        snapshot.recentCenter = snapshot.position;
        snapshot.moving = unit.IsMoving();
        snapshot.visible = unit.IsVisible();

        Track track;
        bool found = false;
        AcquireSRWLockShared(&lock_);
        const auto iterator = tracks_.find(unit.NetworkId());
        if (iterator != tracks_.end()) {
            track = iterator->second;
            found = true;
        }
        ReleaseSRWLockShared(&lock_);

        const auto liveWaypoints = unit.GetWaypoints();
        snapshot.path.reserve(liveWaypoints.size() + 1);
        for (const auto& waypoint : liveWaypoints) {
            const Vec2 point = waypoint.To2D();
            if (point.IsValid() && !point.IsZero()) snapshot.path.push_back(ToMath(point));
        }
        if (snapshot.path.empty() && found) snapshot.path = track.path;
        if (snapshot.valid && (snapshot.path.empty() ||
            Math::DistanceSquared(snapshot.path.front(), snapshot.position) > 400.0)) {
            snapshot.path.insert(snapshot.path.begin(), snapshot.position);
        }
        RemoveDuplicatePathPoints(snapshot.path);

        if (!found) {
            const Vec2 rawVelocity = unit.Velocity().To2D();
            snapshot.velocity = ToMath(rawVelocity);
            snapshot.averageSpeed = snapshot.velocity.Length();
            snapshot.directionStability = snapshot.moving ? 0.5 : 1.0;
            snapshot.speedStability = snapshot.moving ? 0.5 : 1.0;
            snapshot.positionDiscontinuity = true;
            return snapshot;
        }

        snapshot.moving = track.moving;
        snapshot.visible = unit.IsVisible() && track.visible;
        if (!track.samples.empty()) {
            const MovementSample& latest = track.samples.back();
            const int sampleAgeMs = std::max(0, now - latest.tick);
            const double elapsed = static_cast<double>(sampleAgeMs) / 1000.0;
            const bool stale = sampleAgeMs > 500;
            const bool liveDiscontinuity = MovementPatternAnalyzer::IsPositionDiscontinuity(
                latest.position,
                snapshot.position,
                elapsed,
                static_cast<double>(unit.MoveSpeed()));
            if (stale || liveDiscontinuity) {
                const Vec2 rawVelocity = unit.Velocity().To2D();
                snapshot.velocity = ToMath(rawVelocity);
                snapshot.averageSpeed = snapshot.velocity.IsFinite()
                    ? snapshot.velocity.Length()
                    : 0.0;
                snapshot.directionStability = snapshot.moving ? 0.25 : 1.0;
                snapshot.speedStability = snapshot.moving ? 0.5 : 1.0;
                snapshot.positionDiscontinuity = true;
                return snapshot;
            }
        }
        snapshot.pathAgeSeconds = track.lastPathTick > 0
            ? std::max(0.0, static_cast<double>(now - track.lastPathTick) / 1000.0)
            : 10.0;
        snapshot.stationarySeconds = !track.moving && track.movementStateTick > 0
            ? std::max(0.0, static_cast<double>(now - track.movementStateTick) / 1000.0)
            : 0.0;
        snapshot.visibleSeconds = snapshot.visible && track.visibilityStateTick > 0
            ? std::max(0.0, static_cast<double>(now - track.visibilityStateTick) / 1000.0)
            : 0.0;
        snapshot.positionStableSeconds = track.lastDiscontinuityTick > 0
            ? std::max(0.0, static_cast<double>(now - track.lastDiscontinuityTick) / 1000.0)
            : 10.0;
        snapshot.directionStableSeconds = track.lastDirectionChangeTick > 0
            ? std::max(0.0, static_cast<double>(now - track.lastDirectionChangeTick) / 1000.0)
            : 10.0;
        snapshot.positionDiscontinuity = track.lastDiscontinuityTick > 0 &&
            now - track.lastDiscontinuityTick < 180;

        const int velocityWindowMs = std::clamp(historyWindowMs, 180, 800);
        const int cutoff = now - velocityWindowMs;
        Math::Vector2 weightedVelocity;
        double totalWeight = 0.0;
        double weightedSpeed = 0.0;
        std::vector<std::pair<Math::Vector2, double>> velocities;
        std::vector<MovementHistoryPoint> history;
        history.reserve(track.samples.size());
        int oldestSampleTick = 0;
        int newestSampleTick = 0;
        for (const auto& sample : track.samples) {
            if (sample.tick < cutoff || !sample.velocity.IsFinite()) continue;
            const double age = std::max(0, now - sample.tick);
            const double weight = 1.0 / (1.0 + age / 150.0);
            weightedVelocity += sample.velocity * weight;
            weightedSpeed += sample.velocity.Length() * weight;
            totalWeight += weight;
            velocities.push_back({sample.velocity, weight});
            history.push_back({age / 1000.0, sample.position, sample.velocity});
            if (oldestSampleTick == 0) oldestSampleTick = sample.tick;
            newestSampleTick = sample.tick;
        }
        const Math::Vector2 latestVelocity = track.filteredVelocity.IsFinite()
            ? track.filteredVelocity
            : Math::Vector2{};
        if (totalWeight > Math::Epsilon) {
            const Math::Vector2 historicalVelocity = weightedVelocity / totalWeight;
            snapshot.velocity = latestVelocity.IsZero()
                ? historicalVelocity
                : historicalVelocity * 0.30 + latestVelocity * 0.70;
            const double historicalSpeed = weightedSpeed / totalWeight;
            snapshot.averageSpeed = latestVelocity.IsZero()
                ? historicalSpeed
                : historicalSpeed * 0.35 + latestVelocity.Length() * 0.65;
        } else {
            snapshot.velocity = latestVelocity;
            snapshot.averageSpeed = snapshot.velocity.Length();
        }

        snapshot.acceleration = track.filteredAcceleration;
        snapshot.angularVelocity = track.filteredAngularVelocity;
        const double accelerationMagnitude = snapshot.acceleration.Length();
        if (!snapshot.acceleration.IsFinite() || accelerationMagnitude > 3000.0) {
            snapshot.acceleration = {};
        }

        const Math::Vector2 meanDirection = snapshot.velocity.Normalized();
        double directionScore = 0.0;
        double directionWeight = 0.0;
        double speedVariance = 0.0;
        for (const auto& entry : velocities) {
            const double speed = entry.first.Length();
            if (speed > 20.0 && !meanDirection.IsZero()) {
                const double alignment = Math::Clamp(entry.first.Normalized().Dot(meanDirection), -1.0, 1.0);
                directionScore += ((alignment + 1.0) * 0.5) * entry.second;
                directionWeight += entry.second;
            }
            const double delta = speed - snapshot.averageSpeed;
            speedVariance += delta * delta * entry.second;
        }
        snapshot.directionStability = directionWeight > Math::Epsilon
            ? Math::Clamp(directionScore / directionWeight, 0.0, 1.0)
            : (snapshot.moving ? 0.5 : 1.0);
        const double deviation = totalWeight > Math::Epsilon
            ? std::sqrt(speedVariance / totalWeight)
            : 0.0;
        snapshot.speedStability = snapshot.averageSpeed > 20.0
            ? Math::Clamp(1.0 - deviation / snapshot.averageSpeed, 0.0, 1.0)
            : 1.0;

        const MovementHistorySummary historySummary =
            MovementPatternAnalyzer::SummarizeHistory(history);
        snapshot.recentCenter = historySummary.recentCenter.IsFinite()
            ? historySummary.recentCenter
            : snapshot.position;
        snapshot.displacementEfficiency = historySummary.displacementEfficiency;
        snapshot.directionReversalsPerSecond =
            historySummary.directionReversalsPerSecond;
        snapshot.directionReversalCount = historySummary.directionReversalCount;

        int recentChanges = 0;
        int pathReversals = 0;
        int firstRecentPathTick = 0;
        int lastRecentPathTick = 0;
        Math::Vector2 previousPathDirection;
        if (!track.pathEvents.empty()) {
            const Math::Vector2 destination = track.pathEvents.back().destination;
            for (const auto& event : track.pathEvents) {
                if (now - event.tick <= 1100) {
                    ++recentChanges;
                    if (firstRecentPathTick == 0) firstRecentPathTick = event.tick;
                    lastRecentPathTick = event.tick;
                    if (!previousPathDirection.IsZero() && !event.direction.IsZero() &&
                        previousPathDirection.Dot(event.direction) < -0.20) {
                        ++pathReversals;
                    }
                    if (!event.direction.IsZero()) previousPathDirection = event.direction;
                }
                if (now - event.tick <= 2500 &&
                    Math::DistanceSquared(event.destination, destination) <= 4900.0) {
                    ++snapshot.repeatedDestinationCount;
                }
            }
        }
        snapshot.pathChangesPerSecond = static_cast<double>(recentChanges) / 1.10;
        const int activePathReversals = MovementPatternAnalyzer::ActiveDirectionReversalCount(
            pathReversals, snapshot.directionStableSeconds);
        snapshot.directionReversalCount = std::max(
            snapshot.directionReversalCount, activePathReversals);
        if (activePathReversals > 0) {
            const double pathDuration = std::max(
                0.25, static_cast<double>(lastRecentPathTick - firstRecentPathTick) / 1000.0);
            snapshot.directionReversalsPerSecond = std::max(
                snapshot.directionReversalsPerSecond,
                static_cast<double>(activePathReversals) / pathDuration);
        }
        snapshot.directionStability *= 0.25 +
            snapshot.displacementEfficiency * 0.75;
        snapshot.sampleSpanSeconds = oldestSampleTick > 0 && newestSampleTick >= oldestSampleTick
            ? static_cast<double>(newestSampleTick - oldestSampleTick) / 1000.0
            : 0.0;
        snapshot.historyReliable = snapshot.visible &&
            MovementPatternAnalyzer::IsHistoryReliable(
                static_cast<int>(velocities.size()),
                snapshot.sampleSpanSeconds,
                snapshot.visibleSeconds,
                snapshot.positionStableSeconds);
        return snapshot;
    }

private:
    struct MovementSample {
        int tick = 0;
        Math::Vector2 position = {};
        Math::Vector2 velocity = {};
    };

    struct PathEvent {
        int tick = 0;
        Math::Vector2 destination = {};
        Math::Vector2 direction = {};
    };

    struct Track {
        int networkId = 0;
        int lastSampleTick = 0;
        int lastPathTick = 0;
        int movementStateTick = 0;
        int visibilityStateTick = 0;
        int lastDiscontinuityTick = 0;
        int lastDirectionChangeTick = 0;
        bool moving = false;
        bool visible = false;
        bool targetable = false;
        bool filterInitialized = false;
        Math::Vector2 filteredVelocity = {};
        Math::Vector2 filteredAcceleration = {};
        double filteredAngularVelocity = 0.0;
        std::deque<MovementSample> samples;
        std::deque<PathEvent> pathEvents;
        std::vector<Math::Vector2> path;
    };

    static inline SRWLOCK lock_ = SRWLOCK_INIT;
    static inline std::unordered_map<int, Track> tracks_;
    static inline bool initialized_ = false;
    static inline bool sourceInitialized_ = false;
    static inline Math::Vector2 sourcePosition_ = {};
    static inline int sourceTick_ = 0;
    static inline int sourceDiscontinuityTick_ = 0;

    static Math::Vector2 ToMath(const Vec2& point) {
        return {static_cast<double>(point.x), static_cast<double>(point.y)};
    }

    static Vec3 ResolvePosition(const SDK::AIBaseClient& unit) {
        Vec3 position = unit.ServerPosition();
        if (!position.IsValid() || position.IsZero()) position = unit.Position();
        return position;
    }

    static void RemoveDuplicatePathPoints(std::vector<Math::Vector2>& path) {
        path.erase(std::unique(path.begin(), path.end(), [](const auto& left, const auto& right) {
            return Math::DistanceSquared(left, right) <= 4.0;
        }), path.end());
    }

    static void UpdateSource() {
        const SDK::AIHeroClient player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) return;
        const Vec2 current2D = ResolvePosition(player).To2D();
        if (!current2D.IsValid() || current2D.IsZero()) return;
        const Math::Vector2 current = ToMath(current2D);
        ObserveSourcePosition(current, static_cast<double>(player.MoveSpeed()));
    }

    static void ResetMotion(Track& track,
                            const Math::Vector2& position,
                            const Math::Vector2& velocity,
                            int now) {
        track.samples.clear();
        track.pathEvents.clear();
        track.filteredVelocity = velocity;
        track.filteredAcceleration = {};
        track.filteredAngularVelocity = 0.0;
        track.filterInitialized = true;
        track.lastSampleTick = now;
        track.lastPathTick = now;
        track.lastDiscontinuityTick = now;
        track.lastDirectionChangeTick = now;
        track.samples.push_back({now, position, velocity});
    }

    static void UpdateUnit(const SDK::AIHeroClient& hero, int now) {
        const Vec2 current2D = ResolvePosition(hero).To2D();
        if (!current2D.IsValid() || current2D.IsZero()) return;
        const Math::Vector2 current = ToMath(current2D);
        const Vec2 rawVelocity2D = hero.Velocity().To2D();
        Math::Vector2 rawVelocity = ToMath(rawVelocity2D);
        if (!rawVelocity.IsFinite() || rawVelocity.Length() > 5000.0) rawVelocity = {};
        const bool moving = hero.IsMoving() || rawVelocity.Length() > 20.0;
        const bool visible = hero.IsVisible();
        const bool targetable = hero.IsTargetable();

        std::vector<Math::Vector2> livePath;
        for (const auto& waypoint : hero.GetWaypoints()) {
            const Vec2 point = waypoint.To2D();
            if (point.IsValid() && !point.IsZero()) livePath.push_back(ToMath(point));
        }
        if (livePath.empty() || Math::DistanceSquared(livePath.front(), current) > 400.0) {
            livePath.insert(livePath.begin(), current);
        }
        RemoveDuplicatePathPoints(livePath);

        AcquireSRWLockExclusive(&lock_);
        Track& track = tracks_[hero.NetworkId()];
        const bool newTrack = track.networkId == 0;
        const bool becameVisible = !newTrack && !track.visible && visible;
        const bool becameTargetable = !newTrack && !track.targetable && targetable;
        if (newTrack) {
            track.networkId = hero.NetworkId();
            track.moving = moving;
            track.visible = visible;
            track.targetable = targetable;
            track.movementStateTick = now;
            track.visibilityStateTick = now;
        }
        if (track.moving != moving) {
            track.moving = moving;
            track.movementStateTick = now;
        }
        if (track.visible != visible) {
            track.visible = visible;
            track.visibilityStateTick = now;
        }
        track.targetable = targetable;
        track.path = std::move(livePath);

        if (!visible || !targetable) {
            track.samples.clear();
            track.pathEvents.clear();
            track.filteredVelocity = {};
            track.filteredAcceleration = {};
            track.filteredAngularVelocity = 0.0;
            track.filterInitialized = false;
            track.lastSampleTick = 0;
            ReleaseSRWLockExclusive(&lock_);
            return;
        }
        if (becameVisible || becameTargetable) {
            ResetMotion(track, current, rawVelocity, now);
            ReleaseSRWLockExclusive(&lock_);
            return;
        }

        if (track.lastSampleTick == 0 || now - track.lastSampleTick >= 35) {
            Math::Vector2 velocity = rawVelocity;
            double sampleElapsed = 0.0;
            if (!track.samples.empty()) {
                const MovementSample& previous = track.samples.back();
                sampleElapsed = static_cast<double>(now - previous.tick) / 1000.0;
                const bool stale = now - previous.tick > 500;
                const bool discontinuity = MovementPatternAnalyzer::IsPositionDiscontinuity(
                    previous.position,
                    current,
                    sampleElapsed,
                    static_cast<double>(hero.MoveSpeed()));
                if (stale || discontinuity) {
                    ResetMotion(track, current, rawVelocity, now);
                    ReleaseSRWLockExclusive(&lock_);
                    return;
                }
                if (sampleElapsed > 0.005) {
                    const Math::Vector2 measured = (current - previous.position) / sampleElapsed;
                    if (measured.IsFinite() && measured.Length() <= 5000.0) {
                        velocity = rawVelocity.IsZero()
                            ? measured
                            : measured * 0.7 + rawVelocity * 0.3;
                    }
                }
            }
            if (!track.filterInitialized) {
                track.filteredVelocity = velocity;
                track.filteredAcceleration = {};
                track.filteredAngularVelocity = 0.0;
                track.filterInitialized = true;
            } else {
                const Math::Vector2 previousFiltered = track.filteredVelocity;
                track.filteredVelocity = previousFiltered * (1.0 - TrainedProfile::VelocityAlpha) +
                    velocity * TrainedProfile::VelocityAlpha;
                if (sampleElapsed > 0.005) {
                    const Math::Vector2 measuredAcceleration =
                        (track.filteredVelocity - previousFiltered) / sampleElapsed;
                    track.filteredAcceleration = track.filteredAcceleration *
                        (1.0 - TrainedProfile::AccelerationAlpha) +
                        measuredAcceleration * TrainedProfile::AccelerationAlpha;
                    if (previousFiltered.Length() > 40.0 &&
                        track.filteredVelocity.Length() > 40.0) {
                        const double angle = std::atan2(
                            previousFiltered.Cross(track.filteredVelocity),
                            previousFiltered.Dot(track.filteredVelocity));
                        const double measuredTurn = std::clamp(
                            angle / sampleElapsed, -6.0, 6.0);
                        track.filteredAngularVelocity =
                            track.filteredAngularVelocity *
                                (1.0 - TrainedProfile::AccelerationAlpha) +
                            measuredTurn * TrainedProfile::AccelerationAlpha;
                    }
                }
            }
            track.samples.push_back({now, current, track.filteredVelocity});
            track.lastSampleTick = now;
            while (!track.samples.empty() && now - track.samples.front().tick > 3000) {
                track.samples.pop_front();
            }
            while (track.samples.size() > 72) track.samples.pop_front();
        }
        ReleaseSRWLockExclusive(&lock_);
    }

    static void OnNewPath(const SDK::Events::NewPathEventArgs& args) {
        if (!initialized_ || !args.Sender.IsValid() || args.PathCount <= 0) return;
        const SDK::AIBaseClient unit(args.Sender.Ptr);
        if (!unit.IsValid() || !unit.IsHero() || !unit.IsEnemy() ||
            !unit.IsVisible() || !unit.IsTargetable()) return;

        const int now = SDK::Variables::TickCount();
        std::vector<Math::Vector2> path;
        const Vec2 current = ResolvePosition(unit).To2D();
        if (current.IsValid() && !current.IsZero()) path.push_back(ToMath(current));
        for (int index = 0; index < args.PathCount; ++index) {
            const Vec2 point = args.Path[index].To2D();
            if (point.IsValid() && !point.IsZero()) path.push_back(ToMath(point));
        }
        RemoveDuplicatePathPoints(path);
        if (path.empty()) return;

        AcquireSRWLockExclusive(&lock_);
        Track& track = tracks_[unit.NetworkId()];
        track.networkId = unit.NetworkId();
        track.lastPathTick = now;
        track.path = path;
        const Math::Vector2 direction = path.size() > 1
            ? (path[1] - path[0]).Normalized()
            : Math::Vector2{};
        Math::Vector2 previousDirection = !track.pathEvents.empty()
            ? track.pathEvents.back().direction
            : track.filteredVelocity.Normalized();
        if (previousDirection.IsZero()) previousDirection = track.filteredVelocity.Normalized();
        const bool sharpDirectionChange = !previousDirection.IsZero() &&
            !direction.IsZero() && previousDirection.Dot(direction) < 0.35;
        if (sharpDirectionChange) {
            const Math::Vector2 committedVelocity = direction *
                std::max(0.0, static_cast<double>(unit.MoveSpeed()));
            track.samples.clear();
            track.filteredVelocity = committedVelocity;
            track.filteredAcceleration = {};
            track.filteredAngularVelocity = 0.0;
            track.filterInitialized = true;
            track.lastSampleTick = now;
            track.lastDirectionChangeTick = now;
            track.samples.push_back({now, path.front(), committedVelocity});
        }
        track.pathEvents.push_back({now, path.back(), direction});
        while (!track.pathEvents.empty() && now - track.pathEvents.front().tick > 5000) {
            track.pathEvents.pop_front();
        }
        while (track.pathEvents.size() > 32) track.pathEvents.pop_front();
        ReleaseSRWLockExclusive(&lock_);
    }

    static void OnGameUpdate(const SDK::Events::GameUpdateEventArgs&) {
        Update();
    }
};

}
