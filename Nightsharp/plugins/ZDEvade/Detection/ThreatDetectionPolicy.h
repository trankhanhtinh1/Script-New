#pragma once

#include "Threat.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <utility>

namespace ZDEvade {

struct ThreatAdmissionCounters {
    int unsupportedArcDropped = 0;
};

inline bool IsThreatDataAdmissible(const SpellData* data) {
    if (!data) return false;
    return data->spellType != ZDSpellType::Arc &&
        data->HasValidGeometryFields();
}

inline bool AdmitThreatData(const SpellData* data,
                            ThreatAdmissionCounters& counters) {
    const bool admitted = IsThreatDataAdmissible(data);
    if (!admitted && data && data->spellType == ZDSpellType::Arc)
        ++counters.unsupportedArcDropped;
    return admitted;
}

inline void ResetThreatAdmissionCounters(ThreatAdmissionCounters& counters) {
    counters = {};
}

enum class ImmediateCastDispatchDecision : std::uint8_t {
    ProcessImmediately,
    QueueUnknownGameThread,
    QueueWrongThread,
    QueueReentrant
};

inline bool IsKnownGameThread(std::uint32_t gameThreadId,
                              std::uint32_t currentThreadId) {
    return gameThreadId != 0 &&
        currentThreadId != 0 &&
        gameThreadId == currentThreadId;
}

inline ImmediateCastDispatchDecision DecideImmediateCastDispatch(
        std::uint32_t gameThreadId,
        std::uint32_t currentThreadId,
        bool reentrant) {
    if (gameThreadId == 0)
        return ImmediateCastDispatchDecision::QueueUnknownGameThread;
    if (gameThreadId != currentThreadId)
        return ImmediateCastDispatchDecision::QueueWrongThread;
    if (reentrant)
        return ImmediateCastDispatchDecision::QueueReentrant;
    return ImmediateCastDispatchDecision::ProcessImmediately;
}

inline bool ShouldQueueCastAfterImmediateAttempt(
        ImmediateCastDispatchDecision decision,
        bool attemptSucceeded) {
    return decision !=
            ImmediateCastDispatchDecision::ProcessImmediately ||
        !attemptSucceeded;
}

enum class ProcessSpellMatchDisposition : std::uint8_t {
    Matched,
    RejectBasicAttack,
    RejectAutoAttack,
    RejectUtility,
    Unmatched
};

struct ProcessSpellMatchInput {
    // SpellName, PayloadSpellName, ScriptName, MissileName, and
    // PayloadMissileName. SpellSlotName is deliberately kept separate because
    // it is resolved from the current spellbook slot, not the cast payload.
    std::array<const char*, 5> authoritativeNames = {};
    const char* spellSlotName = nullptr;
    bool spellNameFromSlotFallback = false;
    bool isAutoAttack = false;
    bool hasRuntimeAttackSignature = false;
    int slot = -1;
    std::uint32_t targetNetworkId = 0;
    int targetIndex = -1;
    bool hasUsableCastGeometry = false;
};

struct ProcessSpellMatchResult {
    const SpellData* data = nullptr;
    ProcessSpellMatchDisposition disposition =
        ProcessSpellMatchDisposition::Unmatched;
};

inline bool ProcessSpellNameContainsNoCase(const char* name,
                                           const char* token) {
    if (!name || !token || !name[0] || !token[0]) return false;
    const std::size_t nameLength = std::strlen(name);
    const std::size_t tokenLength = std::strlen(token);
    if (tokenLength > nameLength) return false;
    for (std::size_t offset = 0;
         offset + tokenLength <= nameLength;
         ++offset) {
        bool equal = true;
        for (std::size_t index = 0; index < tokenLength; ++index) {
            unsigned char left =
                static_cast<unsigned char>(name[offset + index]);
            unsigned char right =
                static_cast<unsigned char>(token[index]);
            if (left >= static_cast<unsigned char>('A') &&
                left <= static_cast<unsigned char>('Z'))
                left = static_cast<unsigned char>(
                    left - static_cast<unsigned char>('A') +
                    static_cast<unsigned char>('a'));
            if (right >= static_cast<unsigned char>('A') &&
                right <= static_cast<unsigned char>('Z'))
                right = static_cast<unsigned char>(
                    right - static_cast<unsigned char>('A') +
                    static_cast<unsigned char>('a'));
            if (left != right) {
                equal = false;
                break;
            }
        }
        if (equal) return true;
    }
    return false;
}

inline bool IsExplicitBasicAttackCastName(const char* name) {
    return ProcessSpellNameContainsNoCase(name, "basicattack") ||
        ProcessSpellNameContainsNoCase(name, "critattack");
}

inline bool IsUtilityProcessSpellName(const char* name) {
    return ProcessSpellNameContainsNoCase(name, "summoner") ||
        ProcessSpellNameContainsNoCase(name, "item");
}

inline bool ProcessSpellNamesEqualNoCase(const char* left,
                                         const char* right) {
    if (!left || !right || !left[0] || !right[0]) return false;
    const std::size_t leftLength = std::strlen(left);
    if (leftLength != std::strlen(right)) return false;
    for (std::size_t index = 0; index < leftLength; ++index) {
        unsigned char leftValue =
            static_cast<unsigned char>(left[index]);
        unsigned char rightValue =
            static_cast<unsigned char>(right[index]);
        if (leftValue >= static_cast<unsigned char>('A') &&
            leftValue <= static_cast<unsigned char>('Z'))
            leftValue = static_cast<unsigned char>(
                leftValue - static_cast<unsigned char>('A') +
                static_cast<unsigned char>('a'));
        if (rightValue >= static_cast<unsigned char>('A') &&
            rightValue <= static_cast<unsigned char>('Z'))
            rightValue = static_cast<unsigned char>(
                rightValue - static_cast<unsigned char>('A') +
                static_cast<unsigned char>('a'));
        if (leftValue != rightValue) return false;
    }
    return true;
}

inline bool ShouldSelectIncomingSpellName(
        const char* currentName,
        bool currentFromSlotFallback,
        const char* incomingName,
        bool incomingFromSlotFallback) {
    if (!incomingName || !incomingName[0]) return false;
    if (!currentName || !currentName[0]) return true;
    if (currentFromSlotFallback != incomingFromSlotFallback)
        return currentFromSlotFallback && !incomingFromSlotFallback;
    return std::strlen(incomingName) > std::strlen(currentName);
}

inline bool ProcessSpellHasTarget(const ProcessSpellMatchInput& input) {
    return input.targetNetworkId != 0 || input.targetIndex >= 0;
}

inline bool HasExplicitProcessSpellAttackName(
        const ProcessSpellMatchInput& input) {
    if (input.hasRuntimeAttackSignature) return true;
    for (const char* name : input.authoritativeNames) {
        if (IsExplicitBasicAttackCastName(name)) return true;
    }
    return IsExplicitBasicAttackCastName(input.spellSlotName);
}

inline bool HasUtilityProcessSpellName(
        const ProcessSpellMatchInput& input) {
    for (const char* name : input.authoritativeNames) {
        if (IsUtilityProcessSpellName(name)) return true;
    }
    return IsUtilityProcessSpellName(input.spellSlotName);
}

template <typename Lookup>
inline ProcessSpellMatchResult MatchProcessSpellDatabaseFirst(
        const ProcessSpellMatchInput& input,
        Lookup&& lookup) {
    const bool hasBasicAttackName =
        HasExplicitProcessSpellAttackName(input);
    const bool hasUtilityName = HasUtilityProcessSpellName(input);

    for (std::size_t index = 0;
         index < input.authoritativeNames.size();
         ++index) {
        const char* name = input.authoritativeNames[index];
        if (!name || !name[0]) continue;
        if (IsExplicitBasicAttackCastName(name) ||
            IsUtilityProcessSpellName(name))
            continue;
        if (index == 0 &&
            input.spellNameFromSlotFallback &&
            ProcessSpellNamesEqualNoCase(name, input.spellSlotName))
            continue;
        if (const SpellData* data = lookup(name))
            return {data, ProcessSpellMatchDisposition::Matched};
    }

    const bool guardedSlotFallback =
        input.slot >= static_cast<int>(ZDSpellSlot::Q) &&
        input.slot <= static_cast<int>(ZDSpellSlot::R) &&
        input.spellSlotName &&
        input.spellSlotName[0] &&
        !hasBasicAttackName &&
        !input.isAutoAttack &&
        !ProcessSpellHasTarget(input) &&
        input.hasUsableCastGeometry;
    if (guardedSlotFallback) {
        if (const SpellData* data = lookup(input.spellSlotName)) {
            if (static_cast<int>(data->spellKey) == input.slot)
                return {data, ProcessSpellMatchDisposition::Matched};
        }
    }

    if (hasBasicAttackName)
        return {nullptr, ProcessSpellMatchDisposition::RejectBasicAttack};
    if (input.isAutoAttack)
        return {nullptr, ProcessSpellMatchDisposition::RejectAutoAttack};
    if (hasUtilityName)
        return {nullptr, ProcessSpellMatchDisposition::RejectUtility};
    return {nullptr, ProcessSpellMatchDisposition::Unmatched};
}

inline bool IsRejectedProcessSpellMatch(
        const ProcessSpellMatchResult& match) {
    return match.disposition ==
            ProcessSpellMatchDisposition::RejectBasicAttack ||
        match.disposition ==
            ProcessSpellMatchDisposition::RejectAutoAttack ||
        match.disposition ==
            ProcessSpellMatchDisposition::RejectUtility;
}

inline bool IsTargetedMissileObservation(
        std::uint32_t eventTargetNetworkId,
        std::uint32_t eventTargetIndex,
        std::uint32_t runtimeTargetNetworkId) {
    return eventTargetNetworkId != 0 ||
        (eventTargetIndex != 0 &&
         eventTargetIndex != std::numeric_limits<std::uint32_t>::max()) ||
        runtimeTargetNetworkId != 0;
}

enum class MissileMatchDisposition : std::uint8_t {
    Matched,
    RejectBasicAttack,
    RejectTargeted,
    RejectUtility,
    Unmatched
};

struct MissileMatchInput {
    // Event MissileName, runtime MissileName, event SpellName, and runtime
    // SpellName. Runtime attack classification includes SDK orbwalker aliases.
    std::array<const char*, 4> names = {};
    bool hasRuntimeAttackSignature = false;
    std::uint32_t eventTargetNetworkId = 0;
    std::uint32_t eventTargetIndex = 0;
    std::uint32_t runtimeTargetNetworkId = 0;
};

struct MissileMatchResult {
    const SpellData* data = nullptr;
    MissileMatchDisposition disposition =
        MissileMatchDisposition::Unmatched;
};

inline bool HasExplicitMissileAttackName(
        const MissileMatchInput& input) {
    if (input.hasRuntimeAttackSignature) return true;
    for (const char* name : input.names) {
        if (IsExplicitBasicAttackCastName(name)) return true;
    }
    return false;
}

template <typename Lookup>
inline MissileMatchResult MatchMissileDatabaseFirst(
        const MissileMatchInput& input,
        Lookup&& lookup) {
    if (HasExplicitMissileAttackName(input))
        return {nullptr, MissileMatchDisposition::RejectBasicAttack};

    bool hasUtilityName = false;
    for (const char* name : input.names) {
        if (IsUtilityProcessSpellName(name)) {
            hasUtilityName = true;
            break;
        }
    }
    for (const char* name : input.names) {
        if (!name || !name[0]) continue;
        if (hasUtilityName && !IsUtilityProcessSpellName(name)) continue;
        if (const SpellData* data = lookup(name))
            return {data, MissileMatchDisposition::Matched};
    }

    if (IsTargetedMissileObservation(
            input.eventTargetNetworkId,
            input.eventTargetIndex,
            input.runtimeTargetNetworkId))
        return {nullptr, MissileMatchDisposition::RejectTargeted};
    if (hasUtilityName)
        return {nullptr, MissileMatchDisposition::RejectUtility};
    return {nullptr, MissileMatchDisposition::Unmatched};
}

inline bool CreatesCastOriginThreat(
        const ProcessSpellMatchResult& match) {
    return match.data && !match.data->noProcess;
}

template <typename ThreatState, typename Correction>
inline void CorrectExistingThreatFromMissile(
        ThreatState& existing,
        Correction&& correction) {
    const int castThreatId = existing.id;
    const int castStartTick = existing.startTick;
    std::forward<Correction>(correction)(existing);
    existing.id = castThreatId;
    existing.startTick = castStartTick;
    existing.collisionUnitCenter = {};
    existing.collisionExplosionCenter = {};
    existing.lastConsumedCollisionPoint = {};
    existing.collisionEndExplosionRadius = 0.0f;
    existing.collisionEndExplosionDelay = -1;
    existing.collisionHitCount = 0;
    existing.collisionUnitNetworkId = 0;
    existing.collisionUnitObjectIdentity = 0;
    existing.collisionKind = {};
    existing.pendingUnitCollisions.clear();
    existing.consumedCollisionUnits.clear();
    existing.collisionStopped = false;
    existing.collisionUnitTargetAuthoritative = false;
    existing.predictedCollisionKind = {};
    existing.predictedCollisionUnitNetworkId = 0;
    existing.predictedCollisionUnitCenter = {};
    existing.predictedCollisionPoint = {};
    existing.predictedCollisionTick = -1;
    existing.predictedCollisionMissileNetworkId = 0;
    existing.predictedCollisionMissileObjectIdentity = 0;
    existing.predictedCollisionUnitObjectIdentity = 0;
    existing.projectileTerminated = false;
    existing.projectileTerminationTick = 0;
    existing.missingMissileTermination = false;
    existing.missileMissingSinceTick = -1;
    existing.missilePositionUnavailable = false;
    existing.expired = false;
    existing.missileBound = true;
}

struct NormalizedCastNameSet {
    static inline constexpr std::size_t kCapacity = 6;
    std::array<std::uint64_t, kCapacity> values = {};
    std::uint8_t count = 0;
};

inline std::uint64_t CastSpellFingerprint(const char* spellName) {
    if (!spellName || !spellName[0]) return 0;
    constexpr std::uint64_t kOffset = 14695981039346656037ull;
    constexpr std::uint64_t kPrime = 1099511628211ull;
    std::uint64_t hash = kOffset;
    for (const unsigned char* cursor =
             reinterpret_cast<const unsigned char*>(spellName);
         *cursor;
         ++cursor) {
        unsigned char value = *cursor;
        if (value >= static_cast<unsigned char>('A') &&
            value <= static_cast<unsigned char>('Z'))
            value = static_cast<unsigned char>(
                value - static_cast<unsigned char>('A') +
                static_cast<unsigned char>('a'));
        hash ^= value;
        hash *= kPrime;
    }
    return hash == 0 ? 1 : hash;
}

inline void AddCastSpellName(NormalizedCastNameSet& names,
                             const char* spellName) {
    const std::uint64_t fingerprint = CastSpellFingerprint(spellName);
    if (fingerprint == 0) return;
    for (std::size_t index = 0; index < names.count; ++index) {
        if (names.values[index] == fingerprint) return;
    }
    if (names.count < names.values.size())
        names.values[names.count++] = fingerprint;
}

inline bool CastSpellNamesOverlap(const NormalizedCastNameSet& left,
                                  const NormalizedCastNameSet& right) {
    if (left.count == 0 || right.count == 0) return false;
    for (std::size_t leftIndex = 0; leftIndex < left.count; ++leftIndex) {
        for (std::size_t rightIndex = 0; rightIndex < right.count; ++rightIndex) {
            if (left.values[leftIndex] == right.values[rightIndex])
                return true;
        }
    }
    return false;
}

inline void MergeCastSpellNames(NormalizedCastNameSet& current,
                                const NormalizedCastNameSet& incoming) {
    for (std::size_t index = 0; index < incoming.count; ++index) {
        const std::uint64_t fingerprint = incoming.values[index];
        bool present = false;
        for (std::size_t currentIndex = 0;
             currentIndex < current.count;
             ++currentIndex) {
            if (current.values[currentIndex] == fingerprint) {
                present = true;
                break;
            }
        }
        if (!present && current.count < current.values.size())
            current.values[current.count++] = fingerprint;
    }
}

struct CastEventKey {
    std::uint32_t casterNetworkId = 0;
    std::uintptr_t castIdentity = 0;
    int slot = -1;
    std::int64_t tick = 0;
    NormalizedCastNameSet spellNames = {};
    Vec2 startPosition = {};
    Vec2 endPosition = {};
    Vec2 castPosition = {};
};

// ProcessSpell and DoCast are observations of one engine cast episode. Keep
// this deliberately below 200 ms: it absorbs normal hook/tick skew without
// joining a plausible later cast of the same slot.
inline constexpr int kLogicalCastEpisodeWindowMs = 180;
inline constexpr float kLogicalLaneDirectionDot = 0.995f;
// Missile-only lanes are matched to their immutable first observation within
// three degrees. This absorbs normal 0.1-1 degree runtime jitter while staying
// far below authored spreads such as Wild Cards' 28 degrees.
inline constexpr float kMissileLaneRegistryDirectionDot = 0.998629535f;
inline constexpr float kLogicalCastStartTolerance = 160.0f;
inline constexpr float kLogicalCastEndTolerance = 200.0f;
inline constexpr int kLogicalCastEpisodeRetentionMs = 12000;

struct LogicalCastEpisodeObservation {
    std::uint32_t casterNetworkId = 0;
    const SpellData* data = nullptr;
    int slot = -1;
    int episodeTick = 0;
    int observationTick = 0;
    Vec2 start = {};
    Vec2 end = {};
    Vec2 direction = {};
    bool projectileObservation = false;
};

inline std::uint64_t CanonicalSpellEpisodeHash(
    const SpellData* data) {
    constexpr std::uint64_t offset = 1469598103934665603ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offset;
    if (!data) return hash;
    const auto mixText = [&](const std::string& text) {
        for (unsigned char value : text) {
            if (value >= 'A' && value <= 'Z')
                value = static_cast<unsigned char>(
                    value - 'A' + 'a');
            hash ^= value;
            hash *= prime;
        }
        hash ^= std::numeric_limits<std::uint64_t>::max();
        hash *= prime;
    };
    mixText(data->charName);
    mixText(data->spellName);
    hash ^= static_cast<std::uint64_t>(data->spellType);
    hash *= prime;
    return hash;
}

inline std::uint32_t LogicalEpisodeTickDistance(
    int left,
    int right) {
    const std::uint32_t forward =
        static_cast<std::uint32_t>(left) -
        static_cast<std::uint32_t>(right);
    const std::uint32_t reverse =
        static_cast<std::uint32_t>(right) -
        static_cast<std::uint32_t>(left);
    return std::min(forward, reverse);
}

inline std::int64_t LogicalEpisodeForwardAge(
    int current,
    int previous) {
    const std::uint32_t difference =
        static_cast<std::uint32_t>(current) -
        static_cast<std::uint32_t>(previous);
    if (difference <=
        static_cast<std::uint32_t>(
            std::numeric_limits<std::int32_t>::max())) {
        return static_cast<std::int64_t>(difference);
    }
    return -static_cast<std::int64_t>(
        std::numeric_limits<std::uint32_t>::max() -
        difference + 1u);
}

inline bool CompatibleLogicalEpisodeGeometry(
    const LogicalCastEpisodeObservation& observation,
    const Vec2& start,
    const Vec2& end,
    const Vec2& direction,
    bool hasCastObservation) {
    const Vec2 episodeDirection = direction.Normalized();
    const Vec2 observedDirection =
        observation.direction.Normalized();
    const bool commonGeometry =
        observation.start.IsValid() &&
        observation.end.IsValid() &&
        start.IsValid() &&
        end.IsValid() &&
        !episodeDirection.IsZero() &&
        !observedDirection.IsZero() &&
        start.DistanceSqr(observation.start) <=
            kLogicalCastStartTolerance *
                kLogicalCastStartTolerance;
    if (!commonGeometry) return false;
    if (observation.data &&
        observation.data->spellType == ZDSpellType::Line &&
        observation.data->multipleNumber > 1) {
        const int projectileCount = std::clamp(
            observation.data->multipleNumber,
            1,
            15);
        if (!hasCastObservation) {
            // A missile-only episode has no authoritative center lane. Any
            // pair of authored lanes can differ by the complete configured
            // spread, regardless of which lane happened to arrive first.
            const float fullSpreadDegrees = std::min(
                180.0f,
                std::fabs(observation.data->multipleAngle) *
                    static_cast<float>(projectileCount - 1));
            const float minimumDot = std::cos(
                fullSpreadDegrees *
                3.14159265358979323846f / 180.0f);
            return episodeDirection.Dot(observedDirection) >=
                minimumDot - 0.00001f;
        }
        if (!observation.projectileObservation) {
            return episodeDirection.Dot(observedDirection) >=
                    kLogicalLaneDirectionDot &&
                end.DistanceSqr(observation.end) <=
                    kLogicalCastEndTolerance *
                        kLogicalCastEndTolerance;
        }
        const float centerIndex =
            static_cast<float>(projectileCount - 1) * 0.5f;
        for (int index = 0; index < projectileCount; ++index) {
            const float radians =
                (static_cast<float>(index) - centerIndex) *
                observation.data->multipleAngle *
                3.14159265358979323846f / 180.0f;
            const float cosine = std::cos(radians);
            const float sine = std::sin(radians);
            const Vec2 expected(
                episodeDirection.x * cosine -
                    episodeDirection.y * sine,
                episodeDirection.x * sine +
                    episodeDirection.y * cosine);
            if (expected.Dot(observedDirection) >=
                kLogicalLaneDirectionDot) {
                return true;
            }
        }
        return false;
    }
    return episodeDirection.Dot(observedDirection) >=
            kLogicalLaneDirectionDot &&
        end.DistanceSqr(observation.end) <=
            kLogicalCastEndTolerance *
                kLogicalCastEndTolerance;
}

inline std::uint64_t StableProjectileLaneKey(
    const Vec2& authoredInitialWorldDirection);

template <std::size_t Capacity>
class LogicalCastEpisodeResolver {
    static_assert(Capacity > 0);

public:
    std::uint64_t Resolve(
        const LogicalCastEpisodeObservation& observation,
        Vec2* episodeDirectionOut = nullptr,
        bool* matchedExistingOut = nullptr,
        std::uint64_t* projectileLaneKeyOut = nullptr) {
        Prune(observation.observationTick);
        if (matchedExistingOut)
            *matchedExistingOut = false;
        if (projectileLaneKeyOut)
            *projectileLaneKeyOut = 0;
        const std::uint64_t spellHash =
            CanonicalSpellEpisodeHash(observation.data);
        for (Record& record : records) {
            if (!record.active ||
                record.casterNetworkId !=
                    observation.casterNetworkId ||
                record.spellHash != spellHash ||
                record.slot != observation.slot ||
                LogicalEpisodeTickDistance(
                    record.episodeTick,
                    observation.episodeTick) >
                    static_cast<std::uint32_t>(
                        kLogicalCastEpisodeWindowMs) ||
                !CompatibleLogicalEpisodeGeometry(
                    observation,
                    record.start,
                    record.end,
                    record.direction,
                    record.hasCastObservation)) {
                continue;
            }
            if (LogicalEpisodeForwardAge(
                    observation.observationTick,
                    record.lastSeenTick) >= 0) {
                record.lastSeenTick =
                    observation.observationTick;
            }
            if (!observation.projectileObservation &&
                !record.hasCastObservation) {
                record.start = observation.start;
                record.end = observation.end;
                record.direction =
                    observation.direction.Normalized();
                record.hasCastObservation = true;
            }
            if (episodeDirectionOut) {
                *episodeDirectionOut = record.hasCastObservation
                    ? record.direction
                    : Vec2();
            }
            if (matchedExistingOut)
                *matchedExistingOut = true;
            if (projectileLaneKeyOut &&
                observation.projectileObservation) {
                *projectileLaneKeyOut =
                    ObservationLaneKey(record, observation);
            }
            return record.id;
        }

        Record* target = nullptr;
        for (Record& record : records) {
            if (!record.active) {
                target = &record;
                break;
            }
        }
        if (!target) {
            target = &records.front();
            std::int64_t oldestAge = -1;
            for (Record& record : records) {
                const std::int64_t age =
                    LogicalEpisodeForwardAge(
                        observation.observationTick,
                        record.lastSeenTick);
                if (age > oldestAge) {
                    oldestAge = age;
                    target = &record;
                }
            }
        }
        std::uint64_t id = nextId++;
        if (id == 0) id = nextId++;
        *target = {};
        target->id = id;
        target->casterNetworkId = observation.casterNetworkId;
        target->spellHash = spellHash;
        target->slot = observation.slot;
        target->episodeTick = observation.episodeTick;
        target->lastSeenTick = observation.observationTick;
        target->start = observation.start;
        target->end = observation.end;
        target->direction = observation.direction.Normalized();
        target->hasCastObservation =
            !observation.projectileObservation;
        target->active = true;
        if (episodeDirectionOut) {
            *episodeDirectionOut = target->hasCastObservation
                ? target->direction
                : Vec2();
        }
        if (projectileLaneKeyOut &&
            observation.projectileObservation) {
            *projectileLaneKeyOut =
                ObservationLaneKey(*target, observation);
        }
        return id;
    }

    std::uint64_t ResolveLaneKey(
        std::uint64_t episodeId,
        const Vec2& authoredInitialWorldDirection,
        int projectileIndex = -1) {
        if (episodeId == 0) return 0;
        for (Record& record : records) {
            if (record.active && record.id == episodeId) {
                return RegisteredLaneKey(
                    record,
                    authoredInitialWorldDirection,
                    projectileIndex);
            }
        }
        return 0;
    }

    void Prune(int now) {
        for (Record& record : records) {
            const std::int64_t age =
                LogicalEpisodeForwardAge(
                    now,
                    record.lastSeenTick);
            if (record.active &&
                age > kLogicalCastEpisodeRetentionMs) {
                record = {};
            }
        }
    }

    void Reset() {
        records = {};
        nextId = 1;
    }

    std::size_t Size() const {
        return static_cast<std::size_t>(std::count_if(
            records.begin(),
            records.end(),
            [](const Record& record) {
                return record.active;
            }));
    }

private:
    struct LaneRecord {
        Vec2 initialDirection = {};
        std::uint64_t key = 0;
        int projectileIndex = -1;
    };

    struct Record {
        std::uint64_t id = 0;
        std::uint32_t casterNetworkId = 0;
        std::uint64_t spellHash = 0;
        int slot = -1;
        int episodeTick = 0;
        int lastSeenTick = 0;
        Vec2 start = {};
        Vec2 end = {};
        Vec2 direction = {};
        std::array<LaneRecord, 15> lanes = {};
        std::uint8_t laneCount = 0;
        std::uint64_t nextLaneKey = 1;
        bool hasCastObservation = false;
        bool active = false;
    };

    static bool UsesEpisodeLaneRegistry(
        const LogicalCastEpisodeObservation& observation) {
        return observation.data &&
            observation.data->spellType == ZDSpellType::Line &&
            observation.data->multipleNumber > 1;
    }

    static std::uint64_t RegisteredLaneKey(
        Record& record,
        const Vec2& initialWorldDirection,
        int projectileIndex = -1) {
        const Vec2 direction = initialWorldDirection.Normalized();
        if (!direction.IsValid() || direction.IsZero()) return 0;
        if (projectileIndex >= 0) {
            for (std::size_t index = 0;
                 index < record.laneCount;
                 ++index) {
                if (record.lanes[index].projectileIndex ==
                    projectileIndex) {
                    return record.lanes[index].key;
                }
            }
        }
        LaneRecord* best = nullptr;
        float bestDot = -1.0f;
        for (std::size_t index = 0; index < record.laneCount; ++index) {
            if (projectileIndex >= 0 &&
                record.lanes[index].projectileIndex >= 0) {
                continue;
            }
            const float dot =
                record.lanes[index].initialDirection.Dot(direction);
            if (dot > bestDot) {
                bestDot = dot;
                best = &record.lanes[index];
            }
        }
        if (best &&
            bestDot >= kMissileLaneRegistryDirectionDot) {
            if (projectileIndex >= 0) {
                best->projectileIndex = projectileIndex;
            }
            return best->key;
        }
        if (record.laneCount >= record.lanes.size())
            return 0;
        std::uint64_t key = record.nextLaneKey++;
        if (key == 0) key = record.nextLaneKey++;
        record.lanes[record.laneCount++] = {
            direction,
            key,
            projectileIndex,
        };
        return key;
    }

    static int AuthoredProjectileIndex(
        const Record& record,
        const LogicalCastEpisodeObservation& observation) {
        if (!record.hasCastObservation ||
            !UsesEpisodeLaneRegistry(observation)) {
            return -1;
        }
        const int projectileCount = std::clamp(
            observation.data->multipleNumber,
            1,
            15);
        const float centerIndex =
            static_cast<float>(projectileCount - 1) * 0.5f;
        const Vec2 projectileDirection =
            observation.direction.Normalized();
        int bestIndex = -1;
        float bestDot = -1.0f;
        for (int index = 0; index < projectileCount; ++index) {
            const float radians =
                (static_cast<float>(index) - centerIndex) *
                observation.data->multipleAngle *
                3.14159265358979323846f / 180.0f;
            const float cosine = std::cos(radians);
            const float sine = std::sin(radians);
            const Vec2 expected(
                record.direction.x * cosine -
                    record.direction.y * sine,
                record.direction.x * sine +
                    record.direction.y * cosine);
            const float dot = expected.Dot(projectileDirection);
            if (dot > bestDot) {
                bestDot = dot;
                bestIndex = index;
            }
        }
        return bestDot >= kLogicalLaneDirectionDot
            ? bestIndex
            : -1;
    }

    static std::uint64_t ObservationLaneKey(
        Record& record,
        const LogicalCastEpisodeObservation& observation) {
        const int projectileIndex =
            UsesEpisodeLaneRegistry(observation)
            ? AuthoredProjectileIndex(record, observation)
            : 0;
        return RegisteredLaneKey(
            record,
            observation.direction,
            projectileIndex);
    }

    std::array<Record, Capacity> records = {};
    std::uint64_t nextId = 1;
};

inline int StableProjectileLaneIndex(int projectileCount,
                                     int projectileIndex) {
    return projectileCount > 1 &&
        projectileIndex >= 0 &&
        projectileIndex < projectileCount
        ? projectileIndex
        : -1;
}

// This deterministic direction key remains available for legacy/non-episode
// callers. Logical cast lanes, including single-projectile episodes, use the
// retained episode registry so jitter cannot change identity.
inline constexpr float kProjectileLaneDirectionQuantum = 0.000001f;

inline std::uint64_t StableProjectileLaneKey(
    const Vec2& authoredInitialWorldDirection) {
    const Vec2 direction = authoredInitialWorldDirection.Normalized();
    if (!direction.IsValid() || direction.IsZero()) return 0;
    const auto quantize = [](float component) {
        return static_cast<std::int32_t>(std::llround(
            static_cast<double>(component) /
            static_cast<double>(kProjectileLaneDirectionQuantum)));
    };
    const std::uint32_t x = static_cast<std::uint32_t>(
        quantize(direction.x));
    const std::uint32_t y = static_cast<std::uint32_t>(
        quantize(direction.y));
    return (static_cast<std::uint64_t>(x) << 32u) |
        static_cast<std::uint64_t>(y);
}

inline std::uint64_t TickDistance(std::int64_t left, std::int64_t right) {
    if ((left < 0) == (right < 0)) {
        return left >= right
            ? static_cast<std::uint64_t>(left - right)
            : static_cast<std::uint64_t>(right - left);
    }
    const auto magnitude = [](std::int64_t value) {
        return value < 0
            ? static_cast<std::uint64_t>(-(value + 1)) + 1u
            : static_cast<std::uint64_t>(value);
    };
    return magnitude(left) + magnitude(right);
}

template <typename ThreatRange, typename ThreatState>
inline typename ThreatRange::value_type* FindNormalizedCastDuplicate(
        ThreatRange& threats,
        const ThreatState& candidate) {
    for (auto& existing : threats) {
        if (existing.expired ||
            existing.casterNetworkId != candidate.casterNetworkId ||
            existing.SpellName() != candidate.SpellName() ||
            TickDistance(candidate.startTick, existing.startTick) >
                static_cast<std::uint64_t>(
                    kLogicalCastEpisodeWindowMs))
            continue;
        if (existing.slot >= 0 && candidate.slot >= 0 &&
            existing.slot != candidate.slot)
            continue;
        if (existing.logicalCastEpisodeId != 0 &&
            candidate.logicalCastEpisodeId != 0 &&
            existing.logicalCastEpisodeId !=
                candidate.logicalCastEpisodeId)
            continue;
        if (existing.projectileLaneKey != 0 &&
            candidate.projectileLaneKey != 0 &&
            existing.projectileLaneKey !=
                candidate.projectileLaneKey)
            continue;
        if ((existing.projectileLaneKey == 0 ||
             candidate.projectileLaneKey == 0) &&
            existing.projectileIndex >= 0 &&
            candidate.projectileIndex >= 0 &&
            existing.projectileIndex != candidate.projectileIndex)
            continue;
        const Vec2 existingDirection = existing.direction.Normalized();
        const Vec2 candidateDirection = candidate.direction.Normalized();
        const float directionDot =
            existingDirection.IsValid() &&
                candidateDirection.IsValid() &&
                !existingDirection.IsZero() &&
                !candidateDirection.IsZero()
            ? existingDirection.Dot(candidateDirection)
            : -1.0f;
        if (directionDot < kLogicalLaneDirectionDot ||
            existing.startPos.DistanceSqr(candidate.startPos) >
                kLogicalCastStartTolerance *
                    kLogicalCastStartTolerance)
            continue;
        if (existing.Type() != ZDSpellType::Line &&
            existing.endPos.DistanceSqr(candidate.endPos) >
                kLogicalCastEndTolerance *
                    kLogicalCastEndTolerance)
            continue;
        return &existing;
    }
    return nullptr;
}

template <typename ThreatState>
inline int MergeNormalizedCastDuplicate(
        ThreatState& existing,
        const ThreatState& candidate) {
    // A delayed hook must not rewind live missile correction or replace a
    // retained terminal explosion with its earlier cast-only geometry.
    if (existing.missileBound || existing.projectileTerminated) return -1;
    const int originalId = existing.id;
    const int originalStart = existing.startTick;
    const int originalRevision = existing.revision;
    const int originalProjectileIndex =
        existing.projectileIndex;
    const std::uint64_t originalProjectileLaneKey =
        existing.projectileLaneKey;
    const std::uint64_t originalLogicalCastEpisodeId =
        existing.logicalCastEpisodeId;
    existing = candidate;
    existing.id = originalId;
    existing.startTick = std::min(originalStart, candidate.startTick);
    existing.revision = originalRevision + 1;
    if (existing.projectileIndex < 0)
        existing.projectileIndex =
            originalProjectileIndex;
    if (originalProjectileLaneKey != 0)
        existing.projectileLaneKey =
            originalProjectileLaneKey;
    if (originalLogicalCastEpisodeId != 0)
        existing.logicalCastEpisodeId =
            originalLogicalCastEpisodeId;
    return originalId;
}

inline bool CompatibleCastGeometry(const CastEventKey& left,
                                   const CastEventKey& right) {
    const auto usable = [](const Vec2& position) {
        return position.IsValid() && !position.IsZero();
    };
    const bool leftStart = usable(left.startPosition);
    const bool rightStart = usable(right.startPosition);
    const bool startCompared = leftStart && rightStart;
    if (leftStart && rightStart) {
        if (left.startPosition.DistanceSqr(right.startPosition) >
            kLogicalCastStartTolerance *
                kLogicalCastStartTolerance)
            return false;
    }

    const auto compatibleEndpointPair =
        [&](const Vec2& leftEndpoint, const Vec2& rightEndpoint) {
            if (leftEndpoint.DistanceSqr(rightEndpoint) >
                kLogicalCastEndTolerance *
                    kLogicalCastEndTolerance)
                return false;
            if (!leftStart || !rightStart) return true;
            const Vec2 leftDirection =
                (leftEndpoint - left.startPosition).Normalized();
            const Vec2 rightDirection =
                (rightEndpoint - right.startPosition).Normalized();
            return leftDirection.IsZero() || rightDirection.IsZero() ||
                leftDirection.Dot(rightDirection) >=
                    kLogicalLaneDirectionDot;
        };

    // Prefer fields with the same meaning. Hooks can populate EndPosition and
    // CastPosition differently, so a valid same-semantic match must not be
    // vetoed by comparing it against the other field.
    const std::pair<Vec2, Vec2> sameSemanticPairs[] = {
        {left.endPosition, right.endPosition},
        {left.castPosition, right.castPosition}
    };
    bool sameSemanticCompared = false;
    bool sameSemanticCompatible = false;
    for (const auto& pair : sameSemanticPairs) {
        if (!usable(pair.first) || !usable(pair.second)) continue;
        sameSemanticCompared = true;
        sameSemanticCompatible =
            sameSemanticCompatible ||
            compatibleEndpointPair(pair.first, pair.second);
    }
    if (sameSemanticCompared) return sameSemanticCompatible;

    const std::pair<Vec2, Vec2> crossFieldPairs[] = {
        {left.endPosition, right.castPosition},
        {left.castPosition, right.endPosition}
    };
    bool crossFieldCompared = false;
    bool crossFieldCompatible = false;
    for (const auto& pair : crossFieldPairs) {
        if (!usable(pair.first) || !usable(pair.second)) continue;
        crossFieldCompared = true;
        crossFieldCompatible =
            crossFieldCompatible ||
            compatibleEndpointPair(pair.first, pair.second);
    }
    if (crossFieldCompared) return crossFieldCompatible;
    return startCompared;
}

inline bool HasComparableCastGeometry(const CastEventKey& left,
                                      const CastEventKey& right) {
    const auto usable = [](const Vec2& position) {
        return position.IsValid() && !position.IsZero();
    };
    if (usable(left.startPosition) && usable(right.startPosition))
        return true;
    const Vec2 leftEndpoints[] = {left.endPosition, left.castPosition};
    const Vec2 rightEndpoints[] = {right.endPosition, right.castPosition};
    for (const Vec2& leftEndpoint : leftEndpoints) {
        if (!usable(leftEndpoint)) continue;
        for (const Vec2& rightEndpoint : rightEndpoints) {
            if (usable(rightEndpoint)) return true;
        }
    }
    return false;
}

inline bool SameLogicalCast(const CastEventKey& left,
                            const CastEventKey& right,
                            int toleranceMs =
                                kLogicalCastEpisodeWindowMs) {
    if (left.casterNetworkId != right.casterNetworkId || left.slot != right.slot)
        return false;
    if (TickDistance(left.tick, right.tick) >
        static_cast<std::uint64_t>(std::max(0, toleranceMs)))
        return false;
    // Equal pointer identity is strong evidence and permits sparse callbacks.
    // Different pointers are common across hook domains, so they fall through
    // to canonical spell/geometry correlation instead of vetoing it.
    if (left.castIdentity != 0 && right.castIdentity != 0 &&
        left.castIdentity == right.castIdentity)
        return !HasComparableCastGeometry(left, right) ||
            CompatibleCastGeometry(left, right);
    return CastSpellNamesOverlap(left.spellNames, right.spellNames) &&
           CompatibleCastGeometry(left, right);
}

enum class PendingPriority : std::uint8_t {
    Noise = 0,
    StructuralCast = 1,
    HostileCast = 2
};

inline PendingPriority ClassifyPendingPriority(
        bool noise,
        bool structurallyHostile,
        bool databaseMatched) {
    if (noise) return PendingPriority::Noise;
    return structurallyHostile && databaseMatched
        ? PendingPriority::HostileCast
        : PendingPriority::StructuralCast;
}

struct PendingCastPriorityInput {
    bool noise = false;
    bool senderValid = false;
    std::uint32_t senderNetworkId = 0;
    std::uint32_t senderTeam = 0;
    std::uint32_t casterNetworkId = 0;
    std::uint32_t playerNetworkId = 0;
    std::uint32_t playerTeam = 0;
    bool databaseMatched = false;
};

inline PendingPriority ClassifyPendingCastPriority(
        const PendingCastPriorityInput& input) {
    if (input.noise) return PendingPriority::Noise;
    if (!input.databaseMatched) return PendingPriority::StructuralCast;

    const std::uint32_t effectiveCasterId =
        input.senderNetworkId != 0
            ? input.senderNetworkId
            : input.casterNetworkId;
    if (effectiveCasterId == 0 ||
        effectiveCasterId == input.playerNetworkId)
        return PendingPriority::StructuralCast;

    const bool knownEnemy =
        input.senderValid &&
        input.senderTeam != 0 &&
        input.playerTeam != 0 &&
        input.senderTeam != input.playerTeam;
    const bool unresolvedIdOnly =
        !input.senderValid &&
        input.casterNetworkId != 0;
    return knownEnemy || unresolvedIdOnly
        ? PendingPriority::HostileCast
        : PendingPriority::StructuralCast;
}

enum class PendingQueueDecision : std::uint8_t {
    Append,
    AppendProtectedOverflow,
    Coalesce,
    ReplaceLowerPriority,
    DropLowerPriority,
    DropCapacityAllProtected,
    DropProtectedOverflowLimit
};

struct PendingEventDescriptor {
    CastEventKey key = {};
    PendingPriority priority = PendingPriority::Noise;
};

struct PendingQueueChoice {
    PendingQueueDecision decision = PendingQueueDecision::Append;
    std::size_t index = std::numeric_limits<std::size_t>::max();
};

inline PendingQueueChoice ChoosePendingQueueAction(
        const PendingEventDescriptor* entries,
        std::size_t count,
        std::size_t capacity,
        const PendingEventDescriptor& incoming) {
    for (std::size_t index = 0; index < count; ++index) {
        const bool crossesNoiseBoundary =
            (entries[index].priority == PendingPriority::Noise) !=
            (incoming.priority == PendingPriority::Noise);
        if (!crossesNoiseBoundary &&
            SameLogicalCast(entries[index].key, incoming.key))
            return {PendingQueueDecision::Coalesce, index};
    }
    if (count < capacity)
        return {PendingQueueDecision::Append, count};
    if (count == 0)
        return {PendingQueueDecision::DropCapacityAllProtected,
                std::numeric_limits<std::size_t>::max()};
    if (count == capacity &&
        incoming.priority == PendingPriority::HostileCast) {
        bool allHostile = true;
        for (std::size_t index = 0; index < count; ++index) {
            if (entries[index].priority != PendingPriority::HostileCast) {
                allHostile = false;
                break;
            }
        }
        if (allHostile)
            return {PendingQueueDecision::AppendProtectedOverflow, count};
    }

    std::size_t replaceable = 0;
    for (std::size_t index = 1; index < count; ++index) {
        const int priority = static_cast<int>(entries[index].priority);
        const int selectedPriority =
            static_cast<int>(entries[replaceable].priority);
        if (priority < selectedPriority)
            replaceable = index;
    }

    const int incomingPriority = static_cast<int>(incoming.priority);
    const int selectedPriority =
        static_cast<int>(entries[replaceable].priority);
    if (incomingPriority < selectedPriority)
        return {PendingQueueDecision::DropLowerPriority,
                std::numeric_limits<std::size_t>::max()};
    if (incomingPriority == selectedPriority)
        return {PendingQueueDecision::DropCapacityAllProtected,
                std::numeric_limits<std::size_t>::max()};
    return {PendingQueueDecision::ReplaceLowerPriority, replaceable};
}

inline std::size_t LogicalRingIndex(std::uint64_t head,
                                    std::uint64_t offset,
                                    std::size_t capacity) {
    if (capacity == 0) return 0;
    const std::size_t base =
        static_cast<std::size_t>(head % capacity);
    const std::size_t delta =
        static_cast<std::size_t>(offset % capacity);
    const std::size_t remaining = capacity - base;
    return delta >= remaining ? delta - remaining : base + delta;
}

struct PendingQueueCounters {
    int coalesced = 0;
    int protectedOverflow = 0;
    int replacedLowerPriority = 0;
    int droppedLowerPriority = 0;
    int droppedCapacityAllProtected = 0;
    int droppedProtectedOverflowLimit = 0;
};

inline Vec2 PreferredCastPosition(const Vec2& current, const Vec2& incoming);

template <typename Event, std::size_t Capacity>
class PendingEventQueue {
public:
    static_assert(Capacity > 0, "PendingEventQueue requires nonzero capacity");
    static_assert(
        Capacity <= std::numeric_limits<std::size_t>::max() / 4u,
        "PendingEventQueue protected overflow capacity would wrap");

    // The protected path may retain four additional ringfuls. Runtime uses a
    // 256-slot ring, so its separately allocated overflow is capped at 1024.
    static constexpr std::size_t ProtectedOverflowCapacity() {
        return Capacity * 4u;
    }

    template <typename Merge>
    PendingQueueDecision Push(const Event& event,
                              const PendingEventDescriptor& descriptor,
                              Merge&& merge) {
        std::array<PendingEventDescriptor, Capacity> logicalDescriptors = {};
        for (std::size_t index = 0; index < count_; ++index)
            logicalDescriptors[index] = SlotAt(index).descriptor;
        const PendingQueueChoice choice = ChoosePendingQueueAction(
            logicalDescriptors.data(), count_, Capacity, descriptor);
        if (choice.decision != PendingQueueDecision::Coalesce &&
            protectedOverflow_) {
            for (Slot& existing : *protectedOverflow_) {
                const bool crossesNoiseBoundary =
                    (existing.descriptor.priority ==
                        PendingPriority::Noise) !=
                    (descriptor.priority == PendingPriority::Noise);
                if (crossesNoiseBoundary) continue;
                if (!SameLogicalCast(existing.descriptor.key, descriptor.key))
                    continue;
                merge(existing.event, event);
                MergeDescriptor(existing.descriptor, descriptor);
                ++counters_.coalesced;
                return PendingQueueDecision::Coalesce;
            }
        }
        switch (choice.decision) {
        case PendingQueueDecision::Append:
            Append(event, descriptor);
            break;
        case PendingQueueDecision::AppendProtectedOverflow:
            if (protectedOverflow_ &&
                protectedOverflow_->size() >= ProtectedOverflowCapacity()) {
                ++counters_.droppedProtectedOverflowLimit;
                return PendingQueueDecision::DropProtectedOverflowLimit;
            }
            if (!protectedOverflow_)
                protectedOverflow_ = std::make_unique<std::deque<Slot>>();
            protectedOverflow_->push_back({event, descriptor});
            ++counters_.protectedOverflow;
            break;
        case PendingQueueDecision::Coalesce: {
            Slot& existing = SlotAt(choice.index);
            merge(existing.event, event);
            MergeDescriptor(existing.descriptor, descriptor);
            ++counters_.coalesced;
            break;
        }
        case PendingQueueDecision::ReplaceLowerPriority:
            RemoveAt(choice.index);
            Append(event, descriptor);
            ++counters_.replacedLowerPriority;
            break;
        case PendingQueueDecision::DropLowerPriority:
            ++counters_.droppedLowerPriority;
            break;
        case PendingQueueDecision::DropCapacityAllProtected:
            ++counters_.droppedCapacityAllProtected;
            break;
        case PendingQueueDecision::DropProtectedOverflowLimit:
            ++counters_.droppedProtectedOverflowLimit;
            break;
        }
        return choice.decision;
    }

    bool Pop(Event& event) {
        if (count_ == 0) return false;
        event = std::move(slots_[head_].event);
        slots_[head_] = {};
        head_ = LogicalRingIndex(head_, 1u, Capacity);
        --count_;
        if (protectedOverflow_ && !protectedOverflow_->empty()) {
            const std::size_t tail =
                LogicalRingIndex(head_, count_, Capacity);
            slots_[tail] = std::move(protectedOverflow_->front());
            protectedOverflow_->pop_front();
            ++count_;
            if (protectedOverflow_->empty())
                protectedOverflow_.reset();
        } else if (count_ == 0) {
            head_ = 0;
        }
        return true;
    }

    std::size_t Size() const {
        return count_ +
            (protectedOverflow_ ? protectedOverflow_->size() : 0u);
    }
    const PendingQueueCounters& Counters() const { return counters_; }

    void Clear() {
        slots_ = {};
        protectedOverflow_.reset();
        head_ = 0;
        count_ = 0;
        counters_ = {};
    }

private:
    struct Slot {
        Event event = {};
        PendingEventDescriptor descriptor = {};
    };

    Slot& SlotAt(std::size_t logicalIndex) {
        return slots_[LogicalRingIndex(head_, logicalIndex, Capacity)];
    }

    const Slot& SlotAt(std::size_t logicalIndex) const {
        return slots_[LogicalRingIndex(head_, logicalIndex, Capacity)];
    }

    static void MergeDescriptor(PendingEventDescriptor& current,
                                const PendingEventDescriptor& incoming) {
        if (static_cast<int>(incoming.priority) >
            static_cast<int>(current.priority))
            current.priority = incoming.priority;
        if (current.key.castIdentity == 0)
            current.key.castIdentity = incoming.key.castIdentity;
        MergeCastSpellNames(current.key.spellNames, incoming.key.spellNames);
        current.key.startPosition = PreferredCastPosition(
            current.key.startPosition, incoming.key.startPosition);
        current.key.endPosition = PreferredCastPosition(
            current.key.endPosition, incoming.key.endPosition);
        current.key.castPosition = PreferredCastPosition(
            current.key.castPosition, incoming.key.castPosition);
        current.key.tick = std::min(current.key.tick, incoming.key.tick);
    }

    void Append(const Event& event,
                const PendingEventDescriptor& descriptor) {
        const std::size_t tail = LogicalRingIndex(head_, count_, Capacity);
        slots_[tail] = {event, descriptor};
        ++count_;
    }

    void RemoveAt(std::size_t logicalIndex) {
        for (std::size_t index = logicalIndex + 1; index < count_; ++index)
            SlotAt(index - 1) = std::move(SlotAt(index));
        SlotAt(count_ - 1) = {};
        --count_;
    }

    std::array<Slot, Capacity> slots_ = {};
    // std::deque has no reserve operation. Allocate the deque object and its
    // blocks lazily only after the fixed ring fills with protected casts.
    std::unique_ptr<std::deque<Slot>> protectedOverflow_;
    std::size_t head_ = 0;
    std::size_t count_ = 0;
    PendingQueueCounters counters_ = {};
};

inline bool ShouldPreferCastName(const char* current, const char* incoming) {
    if (!incoming || !incoming[0]) return false;
    if (!current || !current[0]) return true;
    return std::strlen(incoming) > std::strlen(current);
}

inline Vec2 PreferredCastPosition(const Vec2& current, const Vec2& incoming) {
    const bool currentUsable = current.IsValid() && !current.IsZero();
    const bool incomingUsable = incoming.IsValid() && !incoming.IsZero();
    return !currentUsable && incomingUsable ? incoming : current;
}

inline int MissileBindWindowMs(int spellDelayMs, int expectedLaunchDelayMs) {
    constexpr std::int64_t kBaseToleranceMs = 250;
    constexpr std::int64_t kMaximumWindowMs = 5000;
    const std::int64_t spellDelay =
        std::max<std::int64_t>(0, spellDelayMs);
    const std::int64_t expectedLaunchDelay =
        std::max<std::int64_t>(0, expectedLaunchDelayMs);
    const std::int64_t requestedWindow =
        std::max(spellDelay, expectedLaunchDelay) + kBaseToleranceMs;
    return static_cast<int>(
        std::clamp(requestedWindow, std::int64_t{0}, kMaximumWindowMs));
}

struct MissileBindKey {
    std::uint32_t casterNetworkId = 0;
    std::uintptr_t castIdentity = 0;
    std::uintptr_t spellIdentity = 0;
    Vec2 direction = {};
    std::int64_t castTick = 0;
    int spellDelayMs = 0;
    int expectedLaunchDelayMs = 0;
    int slot = -1;
    Vec2 startPosition = {};
    Vec2 endPosition = {};
};

struct MissileBindObservation {
    std::uint32_t casterNetworkId = 0;
    std::uintptr_t castIdentity = 0;
    std::uintptr_t spellIdentity = 0;
    Vec2 direction = {};
    std::int64_t tick = 0;
    int slot = -1;
    Vec2 startPosition = {};
    Vec2 endPosition = {};
};

inline bool CanBindMissile(const MissileBindKey& cast,
                           const MissileBindObservation& missile,
                           float minimumDirectionDot = 0.95f) {
    if (cast.casterNetworkId == 0 ||
        cast.casterNetworkId != missile.casterNetworkId ||
        cast.spellIdentity == 0 ||
        cast.spellIdentity != missile.spellIdentity)
        return false;
    if (cast.slot >= 0 && missile.slot >= 0 &&
        cast.slot != missile.slot)
        return false;
    if (missile.tick < cast.castTick ||
        TickDistance(missile.tick, cast.castTick) >
            static_cast<std::uint64_t>(MissileBindWindowMs(
                cast.spellDelayMs, cast.expectedLaunchDelayMs)))
        return false;
    const Vec2 castDirection = cast.direction.Normalized();
    const Vec2 missileDirection = missile.direction.Normalized();
    if (!castDirection.IsValid() || !missileDirection.IsValid() ||
        castDirection.IsZero() || missileDirection.IsZero() ||
        castDirection.Dot(missileDirection) < minimumDirectionDot)
        return false;
    const auto usable = [](const Vec2& position) {
        return position.IsValid() && !position.IsZero();
    };
    if (usable(cast.startPosition) && usable(missile.startPosition) &&
        cast.startPosition.DistanceSqr(missile.startPosition) >
            kLogicalCastStartTolerance *
                kLogicalCastStartTolerance)
        return false;
    if (usable(cast.endPosition) && usable(missile.endPosition)) {
        const Vec2 castEndDirection =
            (cast.endPosition - cast.startPosition).Normalized();
        const Vec2 missileEndDirection =
            (missile.endPosition - missile.startPosition).Normalized();
        if (!castEndDirection.IsZero() && !missileEndDirection.IsZero() &&
            castEndDirection.Dot(missileEndDirection) <
                minimumDirectionDot)
            return false;
    }
    return true;
}

inline float MissileBindScore(const MissileBindKey& cast,
                              const MissileBindObservation& missile,
                              float minimumDirectionDot = 0.95f) {
    if (!CanBindMissile(cast, missile, minimumDirectionDot))
        return -std::numeric_limits<float>::infinity();
    const float directionScore =
        cast.direction.Normalized().Dot(missile.direction.Normalized());
    const float identityHint =
        cast.castIdentity != 0 &&
            cast.castIdentity == missile.castIdentity
        ? 0.02f
        : 0.0f;
    const auto usable = [](const Vec2& position) {
        return position.IsValid() && !position.IsZero();
    };
    const float startPenalty =
        usable(cast.startPosition) && usable(missile.startPosition)
        ? cast.startPosition.Distance(missile.startPosition) / 10000.0f
        : 0.0f;
    const std::int64_t expectedLaunchTick =
        cast.castTick +
        std::max<std::int64_t>(0, cast.expectedLaunchDelayMs);
    const float timingPenalty =
        static_cast<float>(std::min<std::uint64_t>(
            TickDistance(missile.tick, expectedLaunchTick), 5000u)) /
        100000.0f;
    return directionScore + identityHint - startPenalty - timingPenalty;
}

// Manager snapshots can temporarily omit a live missile. Only treat sustained
// evidence loss beyond the last trustworthy travel/lifecycle prediction as a
// fallback termination, with enough grace to span normal snapshot jitter.
inline constexpr int kMissileEvidenceLossGraceMs = 750;
inline constexpr int kMissileEvidenceLossMaximumMs = 10000;
inline constexpr float kDeletePredictedUnitImpactTolerance = 75.0f;
inline constexpr int kDeletePredictedCollisionMaximumAgeMs = 250;

// Game ticks are signed views of a wrapping 32-bit counter. Evidence deadlines
// are short enough to compare unambiguously in that modular domain.
inline int WrappingTickAdd(int tick, int duration) {
    const std::uint32_t value =
        static_cast<std::uint32_t>(tick) +
        static_cast<std::uint32_t>(std::max(0, duration));
    const std::int64_t signedValue =
        value <= static_cast<std::uint32_t>(std::numeric_limits<int>::max())
        ? static_cast<std::int64_t>(value)
        : static_cast<std::int64_t>(value) -
            (static_cast<std::int64_t>(
                std::numeric_limits<std::uint32_t>::max()) + 1);
    return static_cast<int>(signedValue);
}

inline std::int64_t WrappingTickDifference(int left, int right) {
    const std::uint32_t difference =
        static_cast<std::uint32_t>(left) -
        static_cast<std::uint32_t>(right);
    return difference <=
            static_cast<std::uint32_t>(std::numeric_limits<int>::max())
        ? static_cast<std::int64_t>(difference)
        : static_cast<std::int64_t>(difference) -
            (static_cast<std::int64_t>(
                std::numeric_limits<std::uint32_t>::max()) + 1);
}

struct MissileEvidenceWindow {
    int deadlineTick = 0;
    int terminationTick = 0;
    bool usedRemainingTravel = false;
};

inline MissileEvidenceWindow ResolveMissileEvidenceWindow(
        int evidenceAnchorTick,
        int remainingTravelDurationMs,
        int evidenceLossGraceMs = kMissileEvidenceLossGraceMs,
        int evidenceLossMaximumMs = kMissileEvidenceLossMaximumMs) {
    const int safeGrace = std::max(0, evidenceLossGraceMs);
    const bool finiteDuration =
        remainingTravelDurationMs >= 0 &&
        remainingTravelDurationMs <=
            std::numeric_limits<int>::max() - safeGrace;
    if (!finiteDuration) {
        const int fallback = WrappingTickAdd(
            evidenceAnchorTick,
            evidenceLossMaximumMs);
        return {fallback, fallback, false};
    }
    const int termination = WrappingTickAdd(
        evidenceAnchorTick,
        remainingTravelDurationMs);
    return {
        WrappingTickAdd(termination, safeGrace),
        termination,
        true
    };
}

inline int MissileEvidenceLossDeadlineTick(
        int arrivalTick,
        int evidenceAnchorTick,
        bool arrivalTickTrustworthy = true,
        int evidenceLossGraceMs = kMissileEvidenceLossGraceMs,
        int evidenceLossMaximumMs = kMissileEvidenceLossMaximumMs) {
    // INT_MAX is the saturating ArrivalTick sentinel, not a finite prediction.
    // A trustworthy finite arrival is never shortened by the fallback cap.
    return arrivalTickTrustworthy &&
            arrivalTick != std::numeric_limits<int>::max()
        ? WrappingTickAdd(arrivalTick, evidenceLossGraceMs)
        : WrappingTickAdd(evidenceAnchorTick, evidenceLossMaximumMs);
}

inline int MissingMissileTerminationTick(
        int arrivalTick,
        int evidenceAnchorTick,
        bool arrivalTickTrustworthy = true,
        int evidenceLossMaximumMs = kMissileEvidenceLossMaximumMs) {
    return arrivalTickTrustworthy &&
            arrivalTick != std::numeric_limits<int>::max()
        ? arrivalTick
        : WrappingTickAdd(evidenceAnchorTick, evidenceLossMaximumMs);
}

inline bool ShouldExpireMissingTerminationAt(
        int currentTick,
        int terminalHazardEndTick) {
    return WrappingTickDifference(currentTick, terminalHazardEndTick) > 0;
}

inline bool ShouldEnumerateMissileManager(std::size_t trackedThreatCount) {
    return trackedThreatCount != 0;
}

inline bool MissileObjectIsLiveEvidence(bool objectPresent,
                                        bool positionUsable) {
    (void)positionUsable;
    return objectPresent;
}

struct MissileEvidenceStateUpdate {
    int missingSinceTick = -1;
    bool positionUnavailable = false;
    bool stateChanged = false;
};

inline MissileEvidenceStateUpdate ResolveMissileEvidenceState(
        bool objectPresent,
        bool positionUsable,
        int currentMissingSinceTick,
        bool currentPositionUnavailable,
        int now) {
    MissileEvidenceStateUpdate result;
    if (objectPresent) {
        result.missingSinceTick = -1;
        result.positionUnavailable = !positionUsable;
    } else {
        result.missingSinceTick = currentMissingSinceTick >= 0
            ? currentMissingSinceTick
            : now;
        result.positionUnavailable = false;
    }
    result.stateChanged =
        result.missingSinceTick != currentMissingSinceTick ||
        result.positionUnavailable != currentPositionUnavailable;
    return result;
}

inline bool ShouldTerminateMissingMissile(
        bool missileBound,
        bool projectileTerminated,
        bool missileObserved,
        std::int64_t missingSinceTick,
        int evidenceLossDeadlineTick,
        int currentTick) {
    if (!missileBound || projectileTerminated || missileObserved ||
        missingSinceTick < 0 ||
        WrappingTickDifference(
            currentTick, static_cast<int>(missingSinceTick)) < 0)
        return false;
    // The projectile remains active through the exact deadline tick and
    // terminates on the first later tick, including across signed wrap.
    return WrappingTickDifference(
        currentTick, evidenceLossDeadlineTick) > 0;
}

inline bool ShouldCommitPredictedCollision(bool missileBound,
                                           bool projectileTerminated) {
    return !missileBound || projectileTerminated;
}

template <typename ThreatState>
inline bool ClearPredictedCollisionMetadata(ThreatState& threat) {
    const bool changed =
        threat.predictedCollisionKind != ZDCollisionKind::None ||
        threat.predictedCollisionUnitNetworkId != 0 ||
        !threat.predictedCollisionUnitCenter.IsZero() ||
        !threat.predictedCollisionPoint.IsZero() ||
        threat.predictedCollisionTick != -1 ||
        threat.predictedCollisionMissileNetworkId != 0 ||
        threat.predictedCollisionMissileObjectIdentity != 0 ||
        threat.predictedCollisionUnitObjectIdentity != 0;
    threat.ClearPredictedCollision();
    return changed;
}

template <typename ThreatState>
inline bool CanRefreshLiveBoundCollisionPrediction(
        const ThreatState& threat,
        int currentTick,
        int maximumObservationAgeMs =
            kDeletePredictedCollisionMaximumAgeMs) {
    if (ShouldCommitPredictedCollision(
            threat.missileBound,
            threat.projectileTerminated) ||
        threat.missileMissingSinceTick >= 0 ||
        threat.missilePositionUnavailable ||
        threat.observedTick == 0 ||
        !threat.observedHead.IsValid() ||
        threat.observedHead.IsZero())
        return false;
    const std::int64_t observationAge =
        WrappingTickDifference(currentTick, threat.observedTick);
    return observationAge >= 0 &&
        observationAge <=
            std::max(0, maximumObservationAgeMs);
}

template <typename ThreatState>
inline bool RefreshLiveBoundCollisionPrediction(
        ThreatState& threat,
        ZDCollisionKind kind,
        int unitNetworkId,
        const Vec2& unitCenter,
        const Vec2& collisionPoint,
        int currentTick,
        std::uintptr_t unitObjectIdentity = 0) {
    if (!CanRefreshLiveBoundCollisionPrediction(
            threat,
            currentTick))
        return ClearPredictedCollisionMetadata(threat);
    const bool hasPrediction =
        kind != ZDCollisionKind::None &&
        collisionPoint.IsValid() &&
        !collisionPoint.IsZero();
    const ZDCollisionKind nextKind =
        hasPrediction ? kind : ZDCollisionKind::None;
    const int nextUnitNetworkId =
        hasPrediction && kind == ZDCollisionKind::Unit
        ? unitNetworkId
        : 0;
    const Vec2 nextUnitCenter =
        hasPrediction && kind == ZDCollisionKind::Unit
        ? unitCenter
        : Vec2();
    const Vec2 nextPoint =
        hasPrediction ? collisionPoint : Vec2();
    const int nextTick =
        hasPrediction ? threat.observedTick : -1;
    const std::uint32_t nextMissileNetworkId =
        hasPrediction ? threat.missileNetworkId : 0;
    const std::uintptr_t nextMissileObjectIdentity =
        hasPrediction ? threat.missileObjectIdentity : 0;
    const std::uintptr_t nextUnitObjectIdentity =
        hasPrediction && kind == ZDCollisionKind::Unit
        ? unitObjectIdentity
        : 0;
    const bool changed =
        threat.predictedCollisionKind != nextKind ||
        threat.predictedCollisionUnitNetworkId != nextUnitNetworkId ||
        threat.predictedCollisionUnitCenter.DistanceSqr(
            nextUnitCenter) > 1.0f ||
        threat.predictedCollisionPoint.DistanceSqr(nextPoint) > 1.0f ||
        threat.predictedCollisionTick != nextTick ||
        threat.predictedCollisionMissileNetworkId !=
            nextMissileNetworkId ||
        threat.predictedCollisionMissileObjectIdentity !=
            nextMissileObjectIdentity ||
        threat.predictedCollisionUnitObjectIdentity !=
            nextUnitObjectIdentity;
    threat.predictedCollisionKind = nextKind;
    threat.predictedCollisionUnitNetworkId = nextUnitNetworkId;
    threat.predictedCollisionUnitCenter = nextUnitCenter;
    threat.predictedCollisionPoint = nextPoint;
    threat.predictedCollisionTick = nextTick;
    threat.predictedCollisionMissileNetworkId =
        nextMissileNetworkId;
    threat.predictedCollisionMissileObjectIdentity =
        nextMissileObjectIdentity;
    threat.predictedCollisionUnitObjectIdentity =
        nextUnitObjectIdentity;
    return changed;
}

inline bool ShouldAttachPredictedUnitAtDelete(
        int predictedUnitNetworkId,
        int resolvedUnitNetworkId,
        std::uintptr_t predictedUnitObjectIdentity,
        std::uintptr_t resolvedUnitObjectIdentity,
        bool resolvedUnitValid,
        bool resolvedUnitDead,
        bool resolvedUnitAllowed,
        bool supportsAttachedUnitLifecycle,
        const Vec2& resolvedUnitCenter,
        const Vec2& deleteImpact,
        float tolerance = kDeletePredictedUnitImpactTolerance) {
    if (predictedUnitNetworkId == 0 ||
        predictedUnitNetworkId != resolvedUnitNetworkId ||
        predictedUnitObjectIdentity == 0 ||
        predictedUnitObjectIdentity != resolvedUnitObjectIdentity ||
        !resolvedUnitValid ||
        resolvedUnitDead ||
        !resolvedUnitAllowed ||
        !supportsAttachedUnitLifecycle ||
        !resolvedUnitCenter.IsValid() ||
        resolvedUnitCenter.IsZero() ||
        !deleteImpact.IsValid() ||
        deleteImpact.IsZero())
        return false;
    const float safeTolerance =
        std::isfinite(tolerance)
        ? std::max(0.0f, tolerance)
        : 0.0f;
    return resolvedUnitCenter.DistanceSqr(deleteImpact) <=
        safeTolerance * safeTolerance;
}

template <typename ThreatState>
inline void ConfirmDeleteUnitCollision(
        ThreatState& threat,
        int unitNetworkId,
        const Vec2& deleteImpact,
        bool unitTargetAuthoritative,
        const Vec2& attachedUnitCenter = {},
        std::uintptr_t attachedUnitObjectIdentity = 0) {
    const Vec2 center =
        unitTargetAuthoritative &&
            attachedUnitCenter.IsValid() &&
            !attachedUnitCenter.IsZero()
        ? attachedUnitCenter
        : deleteImpact;
    threat.endPos = deleteImpact;
    threat.collisionKind = ZDCollisionKind::Unit;
    threat.collisionStopped = true;
    threat.collisionHitCount =
        std::max(1, threat.collisionHitCount);
    threat.collisionUnitNetworkId = unitNetworkId;
    threat.collisionUnitObjectIdentity =
        unitTargetAuthoritative
        ? attachedUnitObjectIdentity
        : 0;
    threat.collisionUnitCenter = center;
    threat.collisionExplosionCenter = center;
    threat.collisionUnitTargetAuthoritative =
        unitTargetAuthoritative;
    if (unitNetworkId != 0 &&
        std::find(
            threat.consumedCollisionUnits.begin(),
            threat.consumedCollisionUnits.end(),
            unitNetworkId) ==
            threat.consumedCollisionUnits.end())
        threat.consumedCollisionUnits.push_back(unitNetworkId);
    ClearPredictedCollisionMetadata(threat);
}

inline bool MatchesAttachedUnitIdentity(
        bool unitTargetAuthoritative,
        std::uintptr_t attachedUnitObjectIdentity,
        std::uintptr_t resolvedUnitObjectIdentity) {
    return unitTargetAuthoritative &&
        attachedUnitObjectIdentity != 0 &&
        attachedUnitObjectIdentity ==
            resolvedUnitObjectIdentity;
}

inline bool ShouldAttachExplicitUnitAtDelete(
        bool hasExplicitTargetMetadata,
        int metadataUnitNetworkId,
        std::uintptr_t metadataUnitObjectIdentity,
        int resolvedUnitNetworkId,
        std::uintptr_t resolvedUnitObjectIdentity,
        const Vec2& resolvedUnitCenter) {
    if (!hasExplicitTargetMetadata ||
        (metadataUnitNetworkId == 0 &&
         metadataUnitObjectIdentity == 0) ||
        resolvedUnitNetworkId == 0 ||
        resolvedUnitObjectIdentity == 0 ||
        !resolvedUnitCenter.IsValid() ||
        resolvedUnitCenter.IsZero())
        return false;
    if (metadataUnitNetworkId != 0 &&
        metadataUnitNetworkId != resolvedUnitNetworkId)
        return false;
    if (metadataUnitObjectIdentity != 0 &&
        metadataUnitObjectIdentity !=
            resolvedUnitObjectIdentity)
        return false;
    return true;
}

template <typename ThreatState>
inline bool UpdateAttachedUnitExplosion(
        ThreatState& threat,
        const Vec2& unitCenter,
        bool unitDead,
        int currentTick) {
    if (!threat.data ||
        !threat.collisionUnitTargetAuthoritative)
        return false;
    bool changed = false;
    if (threat.data->endExplosionFollowsUnit &&
        unitCenter.IsValid() &&
        !unitCenter.IsZero() &&
        threat.collisionUnitCenter.DistanceSqr(unitCenter) > 1.0f) {
        threat.collisionUnitCenter = unitCenter;
        if (!threat.collisionExplosionCenter.IsZero())
            threat.collisionExplosionCenter = unitCenter;
        changed = true;
    }
    if (threat.data->endExplosionDetonatesOnUnitDeath &&
        unitDead &&
        WrappingTickDifference(
            threat.EndExplosionStartTick(),
            currentTick) > 0) {
        threat.collisionEndExplosionDelay = 0;
        threat.projectileTerminationTick = currentTick;
        threat.endTick = SaturatingTickAdd(
            currentTick,
            std::max(
                threat.ExtraEndTime(),
                threat.EndExplosionDuration()));
        changed = true;
    }
    return changed;
}

inline Vec2 MonotonicMissileHead(const Vec2& currentHead,
                                 const Vec2& observedHead,
                                 const Vec2& routeDirection,
                                 bool clampForward) {
    if (!currentHead.IsValid() || currentHead.IsZero()) return observedHead;
    if (!observedHead.IsValid() || observedHead.IsZero()) return currentHead;
    if (!clampForward) return observedHead;
    const Vec2 direction = routeDirection.Normalized();
    if (!direction.IsValid() || direction.IsZero()) return currentHead;
    return (observedHead - currentHead).Dot(direction) < 0.0f
        ? currentHead
        : observedHead;
}

inline bool MatchesMissileEpisode(
        std::uint32_t trackedMissileNetworkId,
        std::uintptr_t trackedMissileObjectIdentity,
        std::uint32_t eventMissileNetworkId,
        std::uintptr_t eventMissileObjectIdentity) {
    if (trackedMissileNetworkId == 0 ||
        trackedMissileNetworkId != eventMissileNetworkId)
        return false;
    return trackedMissileObjectIdentity == 0 ||
        eventMissileObjectIdentity == 0 ||
        trackedMissileObjectIdentity == eventMissileObjectIdentity;
}

inline bool MatchesObservedMissileEpisode(
        std::uint32_t trackedMissileNetworkId,
        std::uintptr_t trackedMissileObjectIdentity,
        std::uint32_t observedMissileNetworkId,
        std::uintptr_t observedMissileObjectIdentity) {
    return trackedMissileNetworkId != 0 &&
        trackedMissileObjectIdentity != 0 &&
        trackedMissileNetworkId == observedMissileNetworkId &&
        trackedMissileObjectIdentity == observedMissileObjectIdentity;
}

inline bool ShouldClassifyDeleteAsUnitCollision(
        bool hasExplicitTargetMetadata,
        ZDCollisionKind predictedKind,
        int predictedUnitNetworkId,
        const Vec2& predictedImpact,
        int predictedTick,
        int currentTick,
        bool predictionMatchesMissileEpisode,
        bool allowsUnitCollision,
        const Vec2& deleteImpact,
        float tolerance = kDeletePredictedUnitImpactTolerance,
        int maximumPredictionAgeMs =
            kDeletePredictedCollisionMaximumAgeMs) {
    if (hasExplicitTargetMetadata) return true;
    const std::int64_t predictionAge =
        WrappingTickDifference(currentTick, predictedTick);
    if (predictedKind != ZDCollisionKind::Unit ||
        predictedUnitNetworkId == 0 ||
        !predictionMatchesMissileEpisode ||
        !allowsUnitCollision ||
        predictedTick == -1 ||
        predictionAge < 0 ||
        predictionAge > std::max(0, maximumPredictionAgeMs) ||
        !predictedImpact.IsValid() || predictedImpact.IsZero() ||
        !deleteImpact.IsValid() || deleteImpact.IsZero())
        return false;
    const float safeTolerance =
        std::isfinite(tolerance) ? std::max(0.0f, tolerance) : 0.0f;
    return predictedImpact.DistanceSqr(deleteImpact) <=
        safeTolerance * safeTolerance;
}

inline bool ShouldAcceptMissileDeleteOwnership(
        std::uintptr_t eventMissileObjectIdentity,
        std::size_t activeNetworkIdOwnerCount) {
    return eventMissileObjectIdentity != 0 ||
        activeNetworkIdOwnerCount == 1;
}

inline bool ShouldFinalizeMissileDelete(
        bool missileBound,
        bool projectileTerminated,
        std::uint32_t trackedMissileNetworkId,
        std::uintptr_t trackedMissileObjectIdentity,
        std::uint32_t eventMissileNetworkId,
        std::uintptr_t eventMissileObjectIdentity,
        std::size_t activeNetworkIdOwnerCount = 1) {
    return missileBound &&
        !projectileTerminated &&
        ShouldAcceptMissileDeleteOwnership(
            eventMissileObjectIdentity,
            activeNetworkIdOwnerCount) &&
        MatchesMissileEpisode(
            trackedMissileNetworkId,
            trackedMissileObjectIdentity,
            eventMissileNetworkId,
            eventMissileObjectIdentity);
}

inline bool UsesMissileLifecycle(ZDSpellType type, bool missileBound) {
    return missileBound && type != ZDSpellType::Arc;
}

inline bool MatchesMissileLifecycle(ZDSpellType type,
                                    bool missileBound,
                                    std::uint32_t trackedMissileNetworkId,
                                    std::uint32_t eventMissileNetworkId) {
    return UsesMissileLifecycle(type, missileBound) &&
        trackedMissileNetworkId != 0 &&
        trackedMissileNetworkId == eventMissileNetworkId;
}

inline Vec2 TerminatedProjectileEnd(ZDSpellType type,
                                    bool missileBound,
                                    const Vec2& currentEnd,
                                    const Vec2& observedPosition) {
    return UsesMissileLifecycle(type, missileBound) &&
            observedPosition.IsValid() &&
            !observedPosition.IsZero()
        ? observedPosition
        : currentEnd;
}

inline bool RetainProjectileTermination(ZDSpellType type,
                                        int extraEndTime,
                                        bool hasEndExplosionArea) {
    return hasEndExplosionArea ||
        (type == ZDSpellType::Circular && extraEndTime > 0);
}

enum class CastCasterResolutionDecision : std::uint8_t {
    Reject,
    UseSender,
    UseResolvedNetworkId
};

struct CastCasterResolutionInput {
    bool senderValid = false;
    std::uint32_t senderNetworkId = 0;
    std::uint32_t senderTeam = 0;
    std::uint32_t casterNetworkId = 0;
    bool resolvedValid = false;
    std::uint32_t resolvedNetworkId = 0;
    std::uint32_t resolvedTeam = 0;
    std::uint32_t playerNetworkId = 0;
    std::uint32_t playerTeam = 0;
};

inline CastCasterResolutionDecision DecideCastCasterResolution(
        const CastCasterResolutionInput& input) {
    if (input.senderValid)
        return CastCasterResolutionDecision::UseSender;
    if (input.casterNetworkId == 0 ||
        !input.resolvedValid ||
        input.resolvedNetworkId == 0 ||
        input.resolvedNetworkId != input.casterNetworkId ||
        input.playerNetworkId == 0 ||
        input.playerTeam == 0 ||
        input.resolvedTeam == 0 ||
        input.resolvedNetworkId == input.playerNetworkId ||
        input.resolvedTeam == input.playerTeam)
        return CastCasterResolutionDecision::Reject;
    return CastCasterResolutionDecision::UseResolvedNetworkId;
}

inline Vec2 ProjectedVelocity(const Vec2& displacement,
                              const Vec2& routeDirection,
                              float elapsedMs) {
    const Vec2 direction = routeDirection.Normalized();
    if (!direction.IsValid() || direction.IsZero() || !std::isfinite(elapsedMs) ||
        elapsedMs <= 0.0f)
        return {};
    const float projectedDistance = displacement.Dot(direction);
    return direction * (projectedDistance * 1000.0f / elapsedMs);
}

inline float FilteredProjectedSpeed(float currentSpeed,
                                    const Vec2& displacement,
                                    const Vec2& routeDirection,
                                    float elapsedMs) {
    const Vec2 direction = routeDirection.Normalized();
    const Vec2 velocity =
        ProjectedVelocity(displacement, direction, elapsedMs);
    const float sampleSpeed = velocity.Dot(direction);
    if (!std::isfinite(sampleSpeed) ||
        sampleSpeed <= 25.0f ||
        sampleSpeed >= 100000.0f)
        return currentSpeed;
    return std::isfinite(currentSpeed) && currentSpeed > 1.0f
        ? currentSpeed * 0.75f + sampleSpeed * 0.25f
        : sampleSpeed;
}

inline Vec2 ObservationRouteDirection(const Vec2& currentDirection,
                                      const Vec2& startPosition,
                                      const Vec2& observedPosition,
                                      const Vec2& observedEndPosition,
                                      const Vec2& observedMovement,
                                      bool steering,
                                      bool movementRouted) {
    const Vec2 movementDirection = observedMovement.Normalized();
    const Vec2 normalizedCurrent = currentDirection.Normalized();
    if (movementRouted) {
        if (movementDirection.IsValid() && !movementDirection.IsZero())
            return movementDirection;
        return normalizedCurrent.IsValid() ? normalizedCurrent : Vec2();
    }
    if (steering &&
        (!normalizedCurrent.IsValid() || normalizedCurrent.IsZero())) {
        if (movementDirection.IsValid() && !movementDirection.IsZero())
            return movementDirection;
        return {};
    }

    if (observedEndPosition.IsValid() && !observedEndPosition.IsZero()) {
        const Vec2 routeOrigin = steering
            ? observedPosition
            : startPosition;
        const Vec2 endpointDirection =
            (observedEndPosition - routeOrigin).Normalized();
        if (endpointDirection.IsValid() && !endpointDirection.IsZero())
            return endpointDirection;
    }
    if (normalizedCurrent.IsValid() && !normalizedCurrent.IsZero())
        return normalizedCurrent;
    return {};
}

struct MissileRouteObservationUpdate {
    Vec2 direction = {};
    Vec2 authoredEnd = {};
    Vec2 effectiveEnd = {};
    bool geometryChanged = false;
};

inline MissileRouteObservationUpdate ResolveMissileRouteObservationUpdate(
        MissileRouteMode routeMode,
        const Vec2& currentDirection,
        const Vec2& startPosition,
        const Vec2& currentAuthoredEnd,
        const Vec2& currentEffectiveEnd,
        const Vec2& observedPosition,
        const Vec2& observedEndPosition,
        const Vec2& observedMovement,
        bool movementRouted,
        bool collisionStopped) {
    MissileRouteObservationUpdate result{
        currentDirection,
        currentAuthoredEnd,
        currentEffectiveEnd,
        false
    };
    if (routeMode != MissileRouteMode::Steering)
        return result;

    const Vec2 routeDirection = ObservationRouteDirection(
        currentDirection,
        startPosition,
        observedPosition,
        observedEndPosition,
        observedMovement,
        true,
        movementRouted);
    if (routeDirection.IsZero())
        return result;

    Vec2 routeEnd = currentAuthoredEnd;
    if (movementRouted) {
        routeEnd = observedPosition + routeDirection * 500.0f;
    } else if (observedEndPosition.IsValid() &&
               !observedEndPosition.IsZero() &&
               observedPosition.Distance(observedEndPosition) > 1.0f) {
        routeEnd = observedEndPosition;
    }

    result.geometryChanged =
        currentDirection.DistanceSqr(routeDirection) > 0.0001f ||
        currentAuthoredEnd.DistanceSqr(routeEnd) > 1.0f;
    result.direction = routeDirection;
    result.authoredEnd = routeEnd;
    if (!collisionStopped)
        result.effectiveEnd = routeEnd;
    return result;
}

inline Vec2 SionChargeDirection(const Vec2& facing, const Vec2& fallback) {
    const Vec2 normalizedFacing = facing.Normalized();
    if (normalizedFacing.IsValid() && !normalizedFacing.IsZero())
        return normalizedFacing;
    const Vec2 normalizedFallback = fallback.Normalized();
    return normalizedFallback.IsValid() ? normalizedFallback : Vec2();
}

inline float SionChargeProjectionRange(float authoredRange) {
    return std::isfinite(authoredRange) && authoredRange > 1.0f
        ? authoredRange
        : 1.0f;
}

inline Vec2 SionChargeCorridorEnd(const Vec2& origin,
                                  const Vec2& direction,
                                  float authoredRange) {
    const Vec2 normalizedDirection = direction.Normalized();
    if (!origin.IsValid() || !normalizedDirection.IsValid() ||
        normalizedDirection.IsZero())
        return origin;
    return origin +
        normalizedDirection * SionChargeProjectionRange(authoredRange);
}

inline bool SionChargeCorridorCoversPoint(const Vec2& origin,
                                          const Vec2& direction,
                                          float authoredRange,
                                          float halfWidth,
                                          const Vec2& point) {
    const Vec2 normalizedDirection = direction.Normalized();
    if (!origin.IsValid() || !point.IsValid() ||
        !normalizedDirection.IsValid() || normalizedDirection.IsZero())
        return false;
    const Vec2 offset = point - origin;
    const float forward = offset.Dot(normalizedDirection);
    if (forward < 0.0f ||
        forward > SionChargeProjectionRange(authoredRange))
        return false;
    const Vec2 lateral = offset - normalizedDirection * forward;
    return lateral.LengthSqr() <=
        std::max(0.0f, halfWidth) * std::max(0.0f, halfWidth);
}

} // namespace ZDEvade
