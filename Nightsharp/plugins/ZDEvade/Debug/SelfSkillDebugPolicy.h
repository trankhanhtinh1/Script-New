#pragma once

#include "../Detection/Threat.h"
#include "../Detection/ThreatDetectionPolicy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace ZDEvade {

enum class SelfSkillDebugPhase : std::uint8_t {
    Pending,
    Live,
    Terminal
};

struct SelfSkillDebugVisibility {
    bool masterEnabled = false;
    bool drawPending = true;
    bool drawLive = true;
};

inline bool ShouldDrawSelfSkillPhase(
        const SelfSkillDebugVisibility& visibility,
        SelfSkillDebugPhase phase) {
    if (!visibility.masterEnabled) return false;
    return phase == SelfSkillDebugPhase::Pending
        ? visibility.drawPending
        : visibility.drawLive;
}

inline bool IsExactLocalSelfSource(std::uint32_t localPlayerNetworkId,
                                   std::uint32_t sourceNetworkId) {
    return localPlayerNetworkId != 0 &&
        localPlayerNetworkId != std::numeric_limits<std::uint32_t>::max() &&
        sourceNetworkId == localPlayerNetworkId;
}

inline bool IsSelfSkillDebugDataSupported(const SpellData* data) {
    return data &&
        !data->noProcess &&
        data->spellType != ZDSpellType::Arc &&
        data->HasValidGeometryFields();
}

struct SelfSkillProcessObservation {
    std::uint32_t localPlayerNetworkId = 0;
    std::uint32_t sourceNetworkId = 0;
    const SpellData* data = nullptr;
    ProcessSpellMatchDisposition matchDisposition =
        ProcessSpellMatchDisposition::Unmatched;
    int slot = -1;
    int tick = 0;
    Vec2 start = {};
    Vec2 end = {};
    std::uintptr_t castIdentity = 0;
    bool hasTarget = false;
    bool attackOrUtility = false;
};

struct SelfSkillMissileObservation {
    std::uint32_t localPlayerNetworkId = 0;
    std::uint32_t sourceNetworkId = 0;
    const SpellData* data = nullptr;
    MissileMatchDisposition matchDisposition =
        MissileMatchDisposition::Unmatched;
    int tick = 0;
    Vec2 start = {};
    Vec2 end = {};
    Vec2 head = {};
    std::uint32_t missileNetworkId = 0;
    std::uintptr_t missileObjectIdentity = 0;
    std::uintptr_t castIdentity = 0;
    bool hasTarget = false;
    bool attackOrUtility = false;
};

enum class SelfSkillDebugResultDisposition : std::uint8_t {
    Ignored,
    Matched,
    Duplicate,
    Unmatched,
    Rejected,
    CapacityDropped
};

struct SelfSkillDebugResult {
    bool accepted = false;
    bool duplicate = false;
    std::uint64_t recordId = 0;
    SelfSkillDebugResultDisposition disposition =
        SelfSkillDebugResultDisposition::Ignored;
};

inline bool IsMatchedSelfSkillMissileCreate(
        const SelfSkillDebugResult& result) {
    return result.disposition ==
            SelfSkillDebugResultDisposition::Matched ||
        result.disposition ==
            SelfSkillDebugResultDisposition::Duplicate;
}

struct SelfSkillDebugCounters {
    std::uint64_t processSeen = 0;
    std::uint64_t processMatched = 0;
    std::uint64_t processRejected = 0;
    std::uint64_t processUnmatched = 0;
    std::uint64_t missileCreateMatched = 0;
    std::uint64_t missileCreateOrphan = 0;
    std::uint64_t missileCreateDuplicate = 0;
    std::uint64_t missileCreateRejected = 0;
    std::uint64_t missileCreateUnmatched = 0;
    std::uint64_t missileDeleteMatched = 0;
    std::uint64_t missileDeleteUnmatched = 0;
    std::uint64_t timeouts = 0;
    std::uint64_t capacityDrops = 0;
};

struct SelfSkillDebugSnapshot {
    std::uint64_t id = 0;
    SelfSkillDebugPhase phase = SelfSkillDebugPhase::Pending;
    Threat visual = {};
    int phaseTick = 0;
    int terminalUntilTick = 0;
    std::uint32_t missileNetworkId = 0;
    std::uintptr_t missileObjectIdentity = 0;
};

struct SelfSkillSparseMissileInput {
    std::uint32_t localPlayerNetworkId = 0;
    std::uint32_t eventSourceNetworkId = 0;
    std::uint32_t eventSourceObjectNetworkId = 0;
    std::uint32_t runtimeCasterNetworkId = 0;
    Vec2 eventStart = {};
    Vec2 runtimeStart = {};
    Vec2 eventEnd = {};
    Vec2 runtimeEnd = {};
    Vec2 eventHead = {};
    Vec2 runtimeHead = {};
};

struct SelfSkillSparseMissileFields {
    std::uint32_t sourceNetworkId = 0;
    Vec2 start = {};
    Vec2 end = {};
    Vec2 head = {};
    bool exactLocalSource = false;
};

inline bool IsUsableSelfSkillPosition(const Vec2& position) {
    return position.IsValid() &&
        !position.IsZero() &&
        std::fabs(position.x) <= 30000.0f &&
        std::fabs(position.y) <= 30000.0f;
}

struct SelfSkillProcessGeometry {
    Vec2 start = {};
    Vec2 end = {};
    bool valid = false;
};

inline SelfSkillProcessGeometry ResolveSelfSkillProcessGeometry(
        const Vec2& eventStart,
        const Vec2& senderPosition,
        const Vec2& eventEnd,
        const Vec2& castPosition) {
    SelfSkillProcessGeometry result;
    result.start = IsUsableSelfSkillPosition(eventStart)
        ? eventStart
        : senderPosition;
    result.end = IsUsableSelfSkillPosition(eventEnd)
        ? eventEnd
        : castPosition;
    if (!IsUsableSelfSkillPosition(result.start) ||
        !IsUsableSelfSkillPosition(result.end)) {
        return result;
    }
    const Vec2 direction = (result.end - result.start).Normalized();
    result.valid = direction.IsValid() && !direction.IsZero();
    return result;
}

inline SelfSkillSparseMissileFields ResolveSelfSkillSparseMissileFields(
        const SelfSkillSparseMissileInput& input) {
    SelfSkillSparseMissileFields result;
    result.sourceNetworkId =
        input.eventSourceNetworkId != 0
        ? input.eventSourceNetworkId
        : input.eventSourceObjectNetworkId != 0
            ? input.eventSourceObjectNetworkId
            : input.runtimeCasterNetworkId;
    result.start = IsUsableSelfSkillPosition(input.eventStart)
        ? input.eventStart
        : input.runtimeStart;
    result.end = IsUsableSelfSkillPosition(input.eventEnd)
        ? input.eventEnd
        : input.runtimeEnd;
    result.head = IsUsableSelfSkillPosition(input.eventHead)
        ? input.eventHead
        : input.runtimeHead;
    result.exactLocalSource = IsExactLocalSelfSource(
        input.localPlayerNetworkId,
        result.sourceNetworkId);
    return result;
}

inline MissileMatchInput BuildSelfSkillMissileMatchInput(
        const char* eventMissileName,
        const char* runtimeMissileName,
        const char* eventSpellName,
        const char* runtimeSpellName,
        std::uint32_t eventTargetNetworkId,
        std::uint32_t eventTargetIndex,
        std::uint32_t runtimeTargetNetworkId = 0) {
    MissileMatchInput input;
    input.names = {{
        eventMissileName,
        runtimeMissileName,
        eventSpellName,
        runtimeSpellName,
    }};
    input.hasRuntimeAttackSignature =
        HasExplicitMissileAttackName(input);
    input.eventTargetNetworkId = eventTargetNetworkId;
    input.eventTargetIndex = eventTargetIndex;
    input.runtimeTargetNetworkId = runtimeTargetNetworkId;
    return input;
}

inline bool ShouldProcessSelfMissileDelete(
        std::uint32_t localPlayerNetworkId,
        std::uint32_t eventSourceNetworkId,
        bool storeOwnsMissile) {
    return IsExactLocalSelfSource(
            localPlayerNetworkId,
            eventSourceNetworkId) ||
        storeOwnsMissile;
}

inline Vec2 ResolveSelfSkillDebugEnd(const SpellData& data,
                                    const Vec2& start,
                                    const Vec2& observedEnd) {
    if (!IsUsableSelfSkillPosition(start) ||
        !IsUsableSelfSkillPosition(observedEnd)) {
        return {};
    }
    const Vec2 direction = (observedEnd - start).Normalized();
    if (!direction.IsValid() || direction.IsZero()) return {};
    if (data.spellType == ZDSpellType::Line && !data.useEndPosition)
        return start + direction * std::max(1.0f, data.range);
    return observedEnd;
}

inline int SelfSkillWrappingSubtract(int tick, int duration) {
    const std::uint32_t value =
        static_cast<std::uint32_t>(tick) -
        static_cast<std::uint32_t>(std::max(0, duration));
    return static_cast<int>(static_cast<std::int32_t>(value));
}

template <std::size_t Capacity>
class SelfSkillDebugStore {
    static_assert(Capacity > 0, "self-skill debug store must be bounded");

public:
    static constexpr int kPendingTimeoutMs = 1500;
    static constexpr int kLiveSafetyTimeoutMs = 10000;
    static constexpr int kHiddenTombstoneMinimumRetentionMs =
        kPendingTimeoutMs;

    SelfSkillDebugResult ObserveProcess(
            const SelfSkillProcessObservation& observation) {
        if (!IsExactLocalSelfSource(
                observation.localPlayerNetworkId,
                observation.sourceNetworkId)) {
            return {};
        }
        ++counters_.processSeen;
        if (!observation.data ||
            observation.matchDisposition ==
                ProcessSpellMatchDisposition::Unmatched) {
            ++counters_.processUnmatched;
            return {
                false,
                false,
                0,
                SelfSkillDebugResultDisposition::Unmatched,
            };
        }
        if (IsRejectedProcessSpellMatch({
                observation.data,
                observation.matchDisposition}) ||
            observation.hasTarget ||
            observation.attackOrUtility ||
            !IsSelfSkillDebugDataSupported(observation.data)) {
            ++counters_.processRejected;
            return {
                false,
                false,
                0,
                SelfSkillDebugResultDisposition::Rejected,
            };
        }

        const Vec2 end = ResolveSelfSkillDebugEnd(
            *observation.data,
            observation.start,
            observation.end);
        if (!IsUsableSelfSkillPosition(observation.start) ||
            !IsUsableSelfSkillPosition(end)) {
            ++counters_.processRejected;
            return {
                false,
                false,
                0,
                SelfSkillDebugResultDisposition::Rejected,
            };
        }

        const int projectileCount =
            observation.data->spellType == ZDSpellType::Line
            ? std::clamp(observation.data->multipleNumber, 1, 15)
            : 1;
        const Vec2 centerDirection =
            (end - observation.start).Normalized();
        const float pathLength =
            std::max(1.0f, observation.start.Distance(end));
        const float centerIndex =
            static_cast<float>(projectileCount - 1) * 0.5f;
        SelfSkillDebugResult result;
        for (int projectileIndex = 0;
             projectileIndex < projectileCount;
             ++projectileIndex) {
            const float angle =
                (static_cast<float>(projectileIndex) - centerIndex) *
                observation.data->multipleAngle;
            const Vec2 direction = Rotate(centerDirection, angle);
            const Vec2 laneEnd =
                observation.start + direction * pathLength;
            Record* existing = FindProcessCompatible(
                observation,
                direction);
            if (existing) {
                existing->processObserved = true;
                existing->visual.castIdentity =
                    existing->visual.castIdentity != 0
                    ? existing->visual.castIdentity
                    : observation.castIdentity;
                if (existing->phase == SelfSkillDebugPhase::Pending)
                    existing->visual = MakePendingThreat(
                        observation,
                        laneEnd,
                        direction,
                        projectileIndex,
                        projectileCount);
                result = {
                    true,
                    false,
                    existing->id,
                    SelfSkillDebugResultDisposition::Matched,
                };
                continue;
            }

            Record* slot = Allocate(observation.tick);
            if (!slot) {
                ++counters_.capacityDrops;
                result.disposition =
                    SelfSkillDebugResultDisposition::CapacityDropped;
                continue;
            }
            *slot = {};
            slot->active = true;
            slot->processObserved = true;
            slot->id = NextId();
            slot->phase = SelfSkillDebugPhase::Pending;
            slot->phaseTick = observation.tick;
            slot->creationTick = observation.tick;
            slot->lastSuccessfulObservationTick =
                observation.tick;
            slot->visual = MakePendingThreat(
                observation,
                laneEnd,
                direction,
                projectileIndex,
                projectileCount);
            result = {
                true,
                false,
                slot->id,
                SelfSkillDebugResultDisposition::Matched,
            };
        }
        if (result.accepted)
            ++counters_.processMatched;
        return result;
    }

    SelfSkillDebugResult ObserveMissileCreate(
            const SelfSkillMissileObservation& observation) {
        if (!IsExactLocalSelfSource(
                observation.localPlayerNetworkId,
                observation.sourceNetworkId)) {
            return {};
        }
        if (!observation.data ||
            observation.matchDisposition ==
                MissileMatchDisposition::Unmatched) {
            ++counters_.missileCreateUnmatched;
            return {
                false,
                false,
                0,
                SelfSkillDebugResultDisposition::Unmatched,
            };
        }
        if (observation.matchDisposition !=
                MissileMatchDisposition::Matched ||
            observation.hasTarget ||
            observation.attackOrUtility ||
            !IsSelfSkillDebugDataSupported(observation.data)) {
            ++counters_.missileCreateRejected;
            return {
                false,
                false,
                0,
                SelfSkillDebugResultDisposition::Rejected,
            };
        }
        const Vec2 end = ResolveSelfSkillDebugEnd(
            *observation.data,
            observation.start,
            observation.end);
        if (!IsUsableSelfSkillPosition(observation.start) ||
            !IsUsableSelfSkillPosition(end) ||
            observation.missileNetworkId == 0) {
            ++counters_.missileCreateRejected;
            return {
                false,
                false,
                0,
                SelfSkillDebugResultDisposition::Rejected,
            };
        }

        for (Record& record : records_) {
            if (!record.active ||
                record.phase == SelfSkillDebugPhase::Pending ||
                record.missileNetworkId !=
                    observation.missileNetworkId) {
                continue;
            }
            if (record.missileObjectIdentity == 0 ||
                observation.missileObjectIdentity == 0 ||
                record.missileObjectIdentity ==
                    observation.missileObjectIdentity) {
                ++counters_.missileCreateMatched;
                ++counters_.missileCreateDuplicate;
                return {
                    false,
                    true,
                    record.id,
                    SelfSkillDebugResultDisposition::Duplicate,
                };
            }
        }

        const Vec2 direction =
            (end - observation.start).Normalized();
        Record* record = FindPendingCompatible(
            observation,
            direction);
        const bool orphan = record == nullptr;
        if (!record) {
            record = Allocate(observation.tick);
            if (!record) {
                ++counters_.capacityDrops;
                return {
                    false,
                    false,
                    0,
                    SelfSkillDebugResultDisposition::CapacityDropped,
                };
            }
            *record = {};
            record->active = true;
            record->id = NextId();
        }

        record->phase = SelfSkillDebugPhase::Live;
        record->phaseTick = observation.tick;
        record->creationTick = observation.tick;
        record->lastSuccessfulObservationTick =
            observation.tick;
        record->missileNetworkId =
            observation.missileNetworkId;
        record->missileObjectIdentity =
            observation.missileObjectIdentity;
        record->visual = MakeLiveThreat(
            observation,
            end,
            direction,
            record->processObserved
                ? record->visual.projectileIndex
                : -1);
        ++counters_.missileCreateMatched;
        if (orphan) ++counters_.missileCreateOrphan;
        return {
            true,
            false,
            record->id,
            SelfSkillDebugResultDisposition::Matched,
        };
    }

    SelfSkillDebugResult ObserveMissileDelete(
            std::uint32_t missileNetworkId,
            std::uintptr_t missileObjectIdentity,
            int tick,
            int terminalHoldMs) {
        std::size_t networkOwners = 0;
        for (const Record& record : records_) {
            if (record.active &&
                record.phase == SelfSkillDebugPhase::Live &&
                record.missileNetworkId == missileNetworkId) {
                ++networkOwners;
            }
        }
        for (Record& record : records_) {
            if (!record.active ||
                record.phase != SelfSkillDebugPhase::Live ||
                record.missileNetworkId != missileNetworkId) {
                continue;
            }
            const bool identityMatch =
                missileObjectIdentity != 0
                ? record.missileObjectIdentity ==
                    missileObjectIdentity
                : networkOwners == 1;
            if (!identityMatch) continue;

            const std::uint64_t id = record.id;
            ++counters_.missileDeleteMatched;
            if (terminalHoldMs <= 0) {
                record.phase = SelfSkillDebugPhase::Terminal;
                record.hiddenTombstone = true;
                record.phaseTick = tick;
                record.terminalUntilTick = WrappingTickAdd(
                    tick,
                    HiddenTombstoneRetentionMs(record));
                record.visual.missileBound = true;
                record.visual.projectileTerminated = false;
                record.visual.missilePositionUnavailable = true;
            } else {
                record.phase = SelfSkillDebugPhase::Terminal;
                record.hiddenTombstone = false;
                record.phaseTick = tick;
                record.terminalUntilTick = WrappingTickAdd(
                    tick,
                    std::clamp(terminalHoldMs, 0, 1000));
                // Keep a frozen live visual for the configured terminal hold.
                record.visual.missileBound = true;
                record.visual.projectileTerminated = false;
                record.visual.missilePositionUnavailable = true;
            }
            return {
                true,
                false,
                id,
                SelfSkillDebugResultDisposition::Matched,
            };
        }
        ++counters_.missileDeleteUnmatched;
        return {
            false,
            false,
            0,
            SelfSkillDebugResultDisposition::Unmatched,
        };
    }

    bool OwnsMissile(
            std::uint32_t missileNetworkId,
            std::uintptr_t missileObjectIdentity) const {
        if (missileNetworkId == 0) return false;
        std::size_t networkOwners = 0;
        std::size_t exactOwners = 0;
        for (const Record& record : records_) {
            if (!record.active ||
                record.phase != SelfSkillDebugPhase::Live ||
                record.missileNetworkId != missileNetworkId) {
                continue;
            }
            ++networkOwners;
            if (missileObjectIdentity != 0 &&
                record.missileObjectIdentity ==
                    missileObjectIdentity) {
                ++exactOwners;
            }
        }
        return missileObjectIdentity != 0
            ? exactOwners == 1
            : networkOwners == 1;
    }

    bool RefreshMissile(std::uint64_t recordId,
                        const Vec2& position,
                        const Vec2& end,
                        int tick) {
        for (Record& record : records_) {
            if (!record.active ||
                record.id != recordId ||
                record.phase != SelfSkillDebugPhase::Live) {
                continue;
            }
            if (!IsUsableSelfSkillPosition(position)) return false;
            record.visual.observedHead = position;
            record.visual.observedTick = tick;
            record.visual.missilePositionUnavailable = false;
            record.lastSuccessfulObservationTick = tick;
            if (record.visual.RouteMode() ==
                    MissileRouteMode::Steering &&
                IsUsableSelfSkillPosition(end)) {
                const Vec2 direction = (end - position).Normalized();
                if (!direction.IsZero()) {
                    record.visual.direction = direction;
                    record.visual.authoredEndPos = end;
                    record.visual.endPos = end;
                }
            }
            return true;
        }
        return false;
    }

    void MarkMissilePositionUnavailable(std::uint64_t recordId,
                                        int tick) {
        (void)tick;
        for (Record& record : records_) {
            if (record.active &&
                record.id == recordId &&
                record.phase == SelfSkillDebugPhase::Live) {
                record.visual.missilePositionUnavailable = true;
                return;
            }
        }
    }

    void Update(int now) {
        for (Record& record : records_) {
            if (!record.active) continue;
            bool timeout = false;
            if (record.phase == SelfSkillDebugPhase::Pending) {
                const std::int64_t age =
                    WrappingTickDifference(now, record.phaseTick);
                timeout = age > kPendingTimeoutMs;
            } else if (record.phase == SelfSkillDebugPhase::Live) {
                const std::int64_t age =
                    WrappingTickDifference(
                        now,
                        record.lastSuccessfulObservationTick);
                timeout = age > kLiveSafetyTimeoutMs;
            } else {
                timeout = WrappingTickDifference(
                    now,
                    record.terminalUntilTick) > 0;
            }
            if (timeout) {
                record = {};
                ++counters_.timeouts;
            }
        }
    }

    void Clear() {
        records_ = {};
        counters_ = {};
        nextId_ = 1;
    }

    std::size_t Size() const {
        std::size_t count = 0;
        for (const Record& record : records_)
            count += record.active ? 1u : 0u;
        return count;
    }

    std::size_t Count(SelfSkillDebugPhase phase) const {
        std::size_t count = 0;
        for (const Record& record : records_) {
            if (record.active &&
                !record.hiddenTombstone &&
                record.phase == phase)
                ++count;
        }
        return count;
    }

    std::size_t HiddenTombstoneCount() const {
        std::size_t count = 0;
        for (const Record& record : records_) {
            if (record.active && record.hiddenTombstone)
                ++count;
        }
        return count;
    }

    std::vector<SelfSkillDebugSnapshot> Snapshot() const {
        std::vector<SelfSkillDebugSnapshot> result;
        result.reserve(Size());
        for (const Record& record : records_) {
            if (!record.active || record.hiddenTombstone)
                continue;
            result.push_back({
                record.id,
                record.phase,
                record.visual,
                record.phaseTick,
                record.terminalUntilTick,
                record.missileNetworkId,
                record.missileObjectIdentity,
            });
        }
        return result;
    }

    const SelfSkillDebugCounters& Counters() const {
        return counters_;
    }

private:
    struct Record {
        bool active = false;
        bool processObserved = false;
        bool hiddenTombstone = false;
        std::uint64_t id = 0;
        SelfSkillDebugPhase phase =
            SelfSkillDebugPhase::Pending;
        int phaseTick = 0;
        int creationTick = 0;
        int lastSuccessfulObservationTick = 0;
        int terminalUntilTick = 0;
        std::uint32_t missileNetworkId = 0;
        std::uintptr_t missileObjectIdentity = 0;
        Threat visual = {};
    };

    static Vec2 Rotate(const Vec2& direction,
                       float angleDegrees) {
        const float radians =
            angleDegrees * 3.14159265358979323846f / 180.0f;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        return Vec2(
            direction.x * cosine - direction.y * sine,
            direction.x * sine + direction.y * cosine).Normalized();
    }

    static bool CompatibleDirection(const Vec2& left,
                                    const Vec2& right) {
        const Vec2 normalizedLeft = left.Normalized();
        const Vec2 normalizedRight = right.Normalized();
        return normalizedLeft.IsValid() &&
            normalizedRight.IsValid() &&
            !normalizedLeft.IsZero() &&
            !normalizedRight.IsZero() &&
            normalizedLeft.Dot(normalizedRight) >=
                kLogicalLaneDirectionDot;
    }

    Record* FindProcessCompatible(
            const SelfSkillProcessObservation& observation,
            const Vec2& direction) {
        Record* best = nullptr;
        std::uint32_t bestDistance =
            std::numeric_limits<std::uint32_t>::max();
        for (Record& record : records_) {
            if (!record.active ||
                record.visual.data != observation.data ||
                record.visual.casterNetworkId !=
                    observation.sourceNetworkId ||
                (record.phase != SelfSkillDebugPhase::Pending &&
                 record.phase != SelfSkillDebugPhase::Live &&
                 record.phase != SelfSkillDebugPhase::Terminal) ||
                !CompatibleDirection(
                    record.visual.direction,
                    direction)) {
                continue;
            }
            if (record.phase != SelfSkillDebugPhase::Pending &&
                record.processObserved) {
                continue;
            }
            const int episodeTick =
                record.phase == SelfSkillDebugPhase::Pending
                ? record.phaseTick
                : record.creationTick;
            const std::uint32_t distance = static_cast<std::uint32_t>(
                LogicalEpisodeTickDistance(
                    observation.tick,
                    episodeTick));
            if (distance >
                    static_cast<std::uint32_t>(
                        MissileBindWindowMs(
                            observation.data->spellDelay,
                            observation.data->spellDelay)) ||
                distance >= bestDistance) {
                continue;
            }
            bestDistance = distance;
            best = &record;
        }
        return best;
    }

    Record* FindPendingCompatible(
            const SelfSkillMissileObservation& observation,
            const Vec2& direction) {
        Record* best = nullptr;
        float bestDot = -2.0f;
        for (Record& record : records_) {
            if (!record.active ||
                record.phase != SelfSkillDebugPhase::Pending ||
                record.visual.data != observation.data ||
                record.visual.casterNetworkId !=
                    observation.sourceNetworkId) {
                continue;
            }
            const std::int64_t age = WrappingTickDifference(
                observation.tick,
                record.phaseTick);
            if (age < 0 ||
                age > MissileBindWindowMs(
                    observation.data->spellDelay,
                    observation.data->spellDelay)) {
                continue;
            }
            const float dot =
                record.visual.direction.Normalized().Dot(
                    direction.Normalized());
            if (dot >= kLogicalLaneDirectionDot &&
                dot > bestDot) {
                bestDot = dot;
                best = &record;
            }
        }
        return best;
    }

    static Threat MakePendingThreat(
            const SelfSkillProcessObservation& observation,
            const Vec2& end,
            const Vec2& direction,
            int projectileIndex,
            int projectileCount) {
        Threat threat;
        threat.data = observation.data;
        threat.startPos = observation.start;
        threat.endPos = end;
        threat.authoredEndPos = end;
        threat.direction = direction;
        threat.startTick = observation.tick;
        threat.endTick = CalculateThreatEndTick(
            *observation.data,
            observation.start,
            end,
            observation.tick,
            0);
        threat.castIdentity = observation.castIdentity;
        threat.casterNetworkId =
            observation.sourceNetworkId;
        threat.slot = observation.slot;
        threat.projectileIndex =
            StableProjectileLaneIndex(
                projectileCount,
                projectileIndex);
        return threat;
    }

    static Threat MakeLiveThreat(
            const SelfSkillMissileObservation& observation,
            const Vec2& end,
            const Vec2& direction,
            int projectileIndex) {
        Threat threat;
        threat.data = observation.data;
        threat.startPos = observation.start;
        threat.endPos = end;
        threat.authoredEndPos = end;
        threat.direction = direction;
        threat.startTick = SelfSkillWrappingSubtract(
            observation.tick,
            observation.data->spellDelay);
        threat.launchTick = observation.tick;
        threat.endTick = CalculateThreatEndTick(
            *observation.data,
            observation.start,
            end,
            threat.startTick,
            threat.launchTick);
        threat.castIdentity = observation.castIdentity;
        threat.casterNetworkId =
            observation.sourceNetworkId;
        threat.slot =
            static_cast<int>(observation.data->spellKey);
        threat.projectileIndex = projectileIndex;
        threat.missileNetworkId =
            observation.missileNetworkId;
        threat.missileObjectIdentity =
            observation.missileObjectIdentity;
        threat.observedHead =
            IsUsableSelfSkillPosition(observation.head)
            ? observation.head
            : observation.start;
        threat.observedTick = observation.tick;
        threat.observedSpeed =
            observation.data->projectileSpeed;
        threat.missileBound = true;
        return threat;
    }

    static int HiddenTombstoneRetentionMs(
            const Record& record) {
        const int delay = record.visual.data
            ? std::max(0, record.visual.data->spellDelay)
            : 0;
        return std::max(
            kHiddenTombstoneMinimumRetentionMs,
            MissileBindWindowMs(delay, delay));
    }

    Record* Allocate(int now) {
        for (Record& record : records_) {
            if (!record.active) return &record;
        }
        Record* associated = nullptr;
        for (Record& record : records_) {
            if (!record.active || !record.hiddenTombstone)
                continue;
            if (WrappingTickDifference(
                    now,
                    record.terminalUntilTick) > 0) {
                record = {};
                ++counters_.timeouts;
                return &record;
            }
            if (record.processObserved && !associated)
                associated = &record;
        }
        if (associated) {
            *associated = {};
            return associated;
        }
        return nullptr;
    }

    std::uint64_t NextId() {
        std::uint64_t id = nextId_++;
        if (id == 0) id = nextId_++;
        return id;
    }

    std::array<Record, Capacity> records_ = {};
    SelfSkillDebugCounters counters_ = {};
    std::uint64_t nextId_ = 1;
};

} // namespace ZDEvade
