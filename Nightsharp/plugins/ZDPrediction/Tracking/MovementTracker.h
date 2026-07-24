#pragma once

#include "../Analysis/MovementPatternAnalyzer.h"
#include "../Math/Vector2.h"
#include "../../../sdk/SDK.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ZDPrediction {

struct MovementSnapshot {
    bool valid = false;
    bool moving = false;
    bool visible = false;
    bool targetable = false;
    bool historyReliable = false;
    bool positionDiscontinuity = false;
    bool reborn = false;
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
    double repeatedDestinationCount = 0.0;
    double windingUpRemainingSeconds = 0.0;
    double specialCastRemainingSeconds = 0.0;
    double lastAutoAttackSeconds = 10.0;
    double lastStopMoveSeconds = 10.0;
    double rebornRemainingSeconds = 0.0;
    int directionReversalCount = 0;
    std::vector<Math::Vector2> path;
};

class MovementTracker {
public:
    static void Initialize() {
        if (initialized_) return;
        initialized_ = true;
        SDK::Events::AddOnGameUpdate(&OnGameUpdate);
        SDK::Events::AddOnNewPath(&OnNewPath);
        SDK::Events::AddOnProcessSpell(&OnProcessSpell);
        SDK::Events::AddOnDoCast(&OnDoCast);
        SDK::Events::AddOnBuffRemove(&OnBuffRemove);
        Update();
    }

    static void Shutdown() {
        if (!initialized_) return;
        initialized_ = false;
        SDK::Events::RemoveOnBuffRemove(&OnBuffRemove);
        SDK::Events::RemoveOnDoCast(&OnDoCast);
        SDK::Events::RemoveOnProcessSpell(&OnProcessSpell);
        SDK::Events::RemoveOnNewPath(&OnNewPath);
        SDK::Events::RemoveOnGameUpdate(&OnGameUpdate);
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

    static MovementSnapshot Snapshot(const SDK::AIBaseClient& unit, int historyWindowMs) {
        MovementSnapshot snapshot;
        if (!unit.IsValid()) return snapshot;

        const int now = SDK::Variables::TickCount();
        const Vec3 server = ResolvePosition(unit);
        const Vec2 position2D = server.To2D();
        snapshot.valid = position2D.IsValid() && !position2D.IsZero();
        if (!snapshot.valid) return snapshot;
        snapshot.position = ToMath(position2D);
        snapshot.recentCenter = snapshot.position;
        snapshot.moving = unit.IsMoving();
        snapshot.visible = unit.IsVisible();
        snapshot.targetable = unit.IsTargetable();

        Track track;
        bool found = false;
        AcquireSRWLockShared(&lock_);
        const auto iterator = tracks_.find(unit.NetworkId());
        if (iterator != tracks_.end()) {
            track = iterator->second;
            found = true;
        }
        ReleaseSRWLockShared(&lock_);

        snapshot.path = BuildPath(unit, snapshot.position);
        if (snapshot.path.empty() && found) snapshot.path = track.path;
        RemoveDuplicatePathPoints(snapshot.path);

        if (!found) {
            snapshot.velocity = ToMath(unit.Velocity().To2D());
            if (!snapshot.velocity.IsFinite()) snapshot.velocity = {};
            snapshot.averageSpeed = snapshot.velocity.Length();
            snapshot.directionStability = snapshot.moving ? 0.35 : 1.0;
            snapshot.speedStability = snapshot.moving ? 0.45 : 1.0;
            snapshot.positionDiscontinuity = unit.IsHero();
            snapshot.pathAgeSeconds = 10.0;
            snapshot.visibleSeconds = snapshot.visible ? 0.0 : 10.0;
            snapshot.historyReliable = !unit.IsHero();
            return snapshot;
        }

        snapshot.moving = track.moving;
        snapshot.visible = unit.IsVisible() && track.visible;
        snapshot.targetable = unit.IsTargetable();
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
        snapshot.windingUpRemainingSeconds = track.windingEndTick > now
            ? static_cast<double>(track.windingEndTick - now) / 1000.0
            : 0.0;
        snapshot.specialCastRemainingSeconds = track.specialEndTick > now
            ? static_cast<double>(track.specialEndTick - now) / 1000.0
            : 0.0;
        snapshot.lastAutoAttackSeconds = track.lastAutoAttackTick > 0
            ? std::max(0.0, static_cast<double>(now - track.lastAutoAttackTick) / 1000.0)
            : 10.0;
        snapshot.lastStopMoveSeconds = track.lastStopMoveTick > 0
            ? std::max(0.0, static_cast<double>(now - track.lastStopMoveTick) / 1000.0)
            : 10.0;
        snapshot.reborn = track.rebornEndTick > now;
        snapshot.rebornRemainingSeconds = snapshot.reborn
            ? static_cast<double>(track.rebornEndTick - now) / 1000.0
            : 0.0;

        const int velocityWindowMs = std::clamp(historyWindowMs, 180, 1000);
        const int cutoff = now - velocityWindowMs;
        Math::Vector2 weightedVelocity;
        double totalWeight = 0.0;
        double weightedSpeed = 0.0;
        double speedVariance = 0.0;
        std::vector<std::pair<Math::Vector2, double>> velocities;
        std::vector<MovementHistoryPoint> history;
        int oldestSampleTick = 0;
        int newestSampleTick = 0;

        for (const auto& sample : track.samples) {
            if (sample.tick < cutoff || !sample.velocity.IsFinite()) continue;
            const double age = std::max(0.0, static_cast<double>(now - sample.tick) / 1000.0);
            const double weight = 1.0 / (1.0 + age / 0.15);
            weightedVelocity += sample.velocity * weight;
            weightedSpeed += sample.velocity.Length() * weight;
            totalWeight += weight;
            velocities.push_back({sample.velocity, weight});
            history.push_back({age, sample.position, sample.velocity});
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
            snapshot.averageSpeed = latestVelocity.Length();
        }

        snapshot.acceleration = track.filteredAcceleration;
        snapshot.angularVelocity = track.filteredAngularVelocity;
        if (!snapshot.acceleration.IsFinite() || snapshot.acceleration.Length() > 3000.0) {
            snapshot.acceleration = {};
        }

        const Math::Vector2 meanDirection = snapshot.velocity.Normalized();
        double directionScore = 0.0;
        double directionWeight = 0.0;
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
            : (snapshot.moving ? 0.45 : 1.0);
        const double deviation = totalWeight > Math::Epsilon
            ? std::sqrt(speedVariance / totalWeight)
            : 0.0;
        snapshot.speedStability = snapshot.averageSpeed > 20.0
            ? Math::Clamp(1.0 - deviation / snapshot.averageSpeed, 0.0, 1.0)
            : 1.0;

        const MovementHistorySummary summary = MovementPatternAnalyzer::SummarizeHistory(history);
        snapshot.recentCenter = summary.recentCenter.IsFinite()
            ? summary.recentCenter
            : snapshot.position;
        snapshot.displacementEfficiency = summary.displacementEfficiency;
        snapshot.directionReversalsPerSecond = summary.directionReversalsPerSecond;
        snapshot.directionReversalCount = summary.directionReversalCount;

        int recentPathChanges = 0;
        int pathReversals = 0;
        int firstPathTick = 0;
        int lastPathTick = 0;
        Math::Vector2 previousDirection;
        Math::Vector2 destination;
        if (!track.pathEvents.empty()) destination = track.pathEvents.back().destination;
        for (const auto& event : track.pathEvents) {
            const int age = now - event.tick;
            if (age <= 1200) {
                ++recentPathChanges;
                if (firstPathTick == 0) firstPathTick = event.tick;
                lastPathTick = event.tick;
                if (!previousDirection.IsZero() && !event.direction.IsZero() &&
                    previousDirection.Dot(event.direction) < -0.20) ++pathReversals;
                if (!event.direction.IsZero()) previousDirection = event.direction;
            }
            if (age <= 2600 && Math::DistanceSquared(event.destination, destination) <= 4900.0) {
                snapshot.repeatedDestinationCount += 1.0;
            }
        }
        snapshot.pathChangesPerSecond = static_cast<double>(recentPathChanges) / 1.20;
        if (pathReversals > 0) {
            const double duration = std::max(0.25, static_cast<double>(lastPathTick - firstPathTick) / 1000.0);
            snapshot.directionReversalsPerSecond = std::max(
                snapshot.directionReversalsPerSecond,
                static_cast<double>(pathReversals) / duration);
            snapshot.directionReversalCount = std::max(snapshot.directionReversalCount, pathReversals);
        }
        snapshot.directionStability *= 0.25 + snapshot.displacementEfficiency * 0.75;
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
        int lastStopMoveTick = 0;
        int lastAutoAttackTick = 0;
        int windingEndTick = 0;
        int specialEndTick = 0;
        int rebornEndTick = 0;
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

    static std::vector<Math::Vector2> BuildPath(const SDK::AIBaseClient& unit,
                                                 const Math::Vector2& position) {
        std::vector<Math::Vector2> path;
        for (const auto& waypoint : unit.GetWaypoints()) {
            const Vec2 point = waypoint.To2D();
            if (point.IsValid() && !point.IsZero()) path.push_back(ToMath(point));
        }
        if (path.empty() || Math::DistanceSquared(path.front(), position) > 400.0) {
            path.insert(path.begin(), position);
        }
        return path;
    }

    static void RemoveDuplicatePathPoints(std::vector<Math::Vector2>& path) {
        path.erase(std::unique(path.begin(), path.end(), [](const auto& left, const auto& right) {
            return Math::DistanceSquared(left, right) <= 4.0;
        }), path.end());
    }

    static void ResetMotion(Track& track,
                            const Math::Vector2& position,
                            const Math::Vector2& velocity,
                            int now) {
        track.samples.clear();
        track.filteredVelocity = velocity;
        track.filteredAcceleration = {};
        track.filteredAngularVelocity = 0.0;
        track.filterInitialized = true;
        track.lastSampleTick = now;
        track.lastDiscontinuityTick = now;
        track.lastDirectionChangeTick = now;
        track.samples.push_back({now, position, velocity});
    }

    static void UpdateSource() {
        const SDK::AIHeroClient player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) return;
        const Vec2 current2D = ResolvePosition(player).To2D();
        if (!current2D.IsValid() || current2D.IsZero()) return;
        ObserveSourcePosition(ToMath(current2D), static_cast<double>(player.MoveSpeed()));
    }

    static void UpdateUnit(const SDK::AIHeroClient& hero, int now) {
        const Vec3 current3D = ResolvePosition(hero);
        const Vec2 current2D = current3D.To2D();
        if (!current2D.IsValid() || current2D.IsZero()) return;
        const Math::Vector2 current = ToMath(current2D);
        Math::Vector2 rawVelocity = ToMath(hero.Velocity().To2D());
        if (!rawVelocity.IsFinite() || rawVelocity.Length() > 5000.0) rawVelocity = {};
        const bool moving = hero.IsMoving() || rawVelocity.Length() > 20.0;
        const bool visible = hero.IsVisible();
        const bool targetable = hero.IsTargetable();
        std::vector<Math::Vector2> livePath = BuildPath(hero, current);
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
            track.lastStopMoveTick = now;
        }
        if (track.moving != moving) {
            if (!moving) track.lastStopMoveTick = now;
            track.moving = moving;
            track.movementStateTick = now;
        }
        if (track.visible != visible) {
            track.visible = visible;
            track.visibilityStateTick = now;
        }
        track.targetable = targetable;
        track.path = livePath;

        if (becameVisible || becameTargetable) {
            ResetMotion(track, current, rawVelocity, now);
            ReleaseSRWLockExclusive(&lock_);
            return;
        }
        if (!visible || !targetable) {
            ReleaseSRWLockExclusive(&lock_);
            return;
        }

        if (track.lastSampleTick == 0 || now - track.lastSampleTick >= 30) {
            Math::Vector2 velocity = rawVelocity;
            double elapsed = 0.0;
            if (!track.samples.empty()) {
                const MovementSample& previous = track.samples.back();
                elapsed = static_cast<double>(now - previous.tick) / 1000.0;
                const bool stale = now - previous.tick > 500;
                const bool discontinuity = MovementPatternAnalyzer::IsPositionDiscontinuity(
                    previous.position,
                    current,
                    elapsed,
                    static_cast<double>(hero.MoveSpeed()));
                if (stale || discontinuity) {
                    ResetMotion(track, current, rawVelocity, now);
                    ReleaseSRWLockExclusive(&lock_);
                    return;
                }
                if (elapsed > 0.005) {
                    const Math::Vector2 measured = (current - previous.position) / elapsed;
                    if (measured.IsFinite() && measured.Length() <= 5000.0) {
                        velocity = rawVelocity.IsZero()
                            ? measured
                            : measured * 0.70 + rawVelocity * 0.30;
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
                track.filteredVelocity = previousFiltered * 0.52 + velocity * 0.48;
                if (elapsed > 0.005) {
                    const Math::Vector2 measuredAcceleration =
                        (track.filteredVelocity - previousFiltered) / elapsed;
                    track.filteredAcceleration = track.filteredAcceleration * 0.86 + measuredAcceleration * 0.14;
                    if (previousFiltered.Length() > 40.0 && track.filteredVelocity.Length() > 40.0) {
                        const double angle = std::atan2(
                            previousFiltered.Cross(track.filteredVelocity),
                            previousFiltered.Dot(track.filteredVelocity));
                        const double measuredTurn = std::clamp(angle / elapsed, -6.0, 6.0);
                        track.filteredAngularVelocity = track.filteredAngularVelocity * 0.86 + measuredTurn * 0.14;
                    }
                }
            }
            track.samples.push_back({now, current, track.filteredVelocity});
            track.lastSampleTick = now;
            while (!track.samples.empty() && now - track.samples.front().tick > 3200) {
                track.samples.pop_front();
            }
            while (track.samples.size() > 96) track.samples.pop_front();
        }

        static int lastPruneTick = 0;
        if (now - lastPruneTick > 5000) {
            lastPruneTick = now;
            for (auto it = tracks_.begin(); it != tracks_.end(); ) {
                if (now - it->second.lastSampleTick > 10000 && now - it->second.lastPathTick > 10000) {
                    it = tracks_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        ReleaseSRWLockExclusive(&lock_);
    }

    static int CastDurationMs(const SDK::AIBaseClient& unit,
                              const SDK::Events::ProcessSpellEventArgs& args) {
        double duration = static_cast<double>(args.CastDelay);
        if (duration > 10.0) return std::clamp(static_cast<int>(duration), 80, 3000);
        if (duration > 0.0) return std::clamp(static_cast<int>(duration * 1000.0), 80, 3000);
        const float windup = SDK::AttackWindup(unit);
        return std::clamp(static_cast<int>(std::max(0.08f, windup) * 1000.0f), 80, 1800);
    }

    static void RecordCast(const SDK::Events::ProcessSpellEventArgs& args) {
        if (!args.Sender.IsValid() || !initialized_) return;
        const SDK::AIBaseClient unit(args.Sender.Ptr, args.Sender.Type);
        if (!unit.IsValid() || !unit.IsHero()) return;
        const int now = SDK::Variables::TickCount();
        const bool autoAttack = args.IsAutoAttack || args.Slot == 64;
        const int duration = CastDurationMs(unit, args);
        AcquireSRWLockExclusive(&lock_);
        Track& track = tracks_[unit.NetworkId()];
        track.networkId = unit.NetworkId();
        if (autoAttack) {
            track.lastAutoAttackTick = now;
            track.windingEndTick = now + duration;
        } else if (args.IsSpecialAttack || duration >= 180) {
            track.specialEndTick = std::max(track.specialEndTick, now + duration);
        }
        ReleaseSRWLockExclusive(&lock_);
    }

    static void OnNewPath(const SDK::Events::NewPathEventArgs& args) {
        if (!initialized_ || !args.Sender.IsValid() || args.PathCount <= 0) return;
        const SDK::AIBaseClient unit(args.Sender.Ptr, args.Sender.Type);
        if (!unit.IsValid() || !unit.IsHero()) return;
        const int now = SDK::Variables::TickCount();
        const Math::Vector2 current = ToMath(ResolvePosition(unit).To2D());
        std::vector<Math::Vector2> path;
        if (current.IsFinite() && !current.IsZero()) path.push_back(current);
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
        if (!previousDirection.IsZero() && !direction.IsZero() && previousDirection.Dot(direction) < 0.35) {
            track.lastDirectionChangeTick = now;
            track.filteredVelocity = direction * std::max(0.0, static_cast<double>(unit.MoveSpeed()));
            track.filteredAcceleration = {};
            track.filteredAngularVelocity = 0.0;
            track.lastSampleTick = now;
            track.samples.clear();
            track.samples.push_back({now, path.front(), track.filteredVelocity});
        }
        track.pathEvents.push_back({now, path.back(), direction});
        while (!track.pathEvents.empty() && now - track.pathEvents.front().tick > 5200) {
            track.pathEvents.pop_front();
        }
        while (track.pathEvents.size() > 40) track.pathEvents.pop_front();
        ReleaseSRWLockExclusive(&lock_);
    }

    static void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
        RecordCast(args);
    }

    static void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
        RecordCast(args);
    }

    static void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
        if (!initialized_ || !args.Sender.IsValid()) return;
        if (_stricmp(args.BuffName, "willrevive") != 0 &&
            _stricmp(args.BuffName, "chronorevive") != 0 &&
            _stricmp(args.BuffName, "guardianangelrebirth") != 0) return;
        const SDK::AIBaseClient unit(args.Sender.Ptr, args.Sender.Type);
        if (!unit.IsValid() || !unit.IsHero()) return;
        AcquireSRWLockExclusive(&lock_);
        Track& track = tracks_[unit.NetworkId()];
        track.networkId = unit.NetworkId();
        track.rebornEndTick = SDK::Variables::TickCount() + 4000;
        ReleaseSRWLockExclusive(&lock_);
    }

    static void OnGameUpdate(const SDK::Events::GameUpdateEventArgs&) {
        Update();
    }
};

}
