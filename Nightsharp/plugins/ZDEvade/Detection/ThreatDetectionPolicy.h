#pragma once

#include "../Database/SpellData.h"
#include "../../../Core/Vector.h"

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
            TickDistance(candidate.startTick, existing.startTick) > 120u)
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
        if (candidate.castIdentity != 0 && existing.castIdentity != 0) {
            if (candidate.castIdentity == existing.castIdentity &&
                directionDot >= 0.9985f)
                return &existing;
            continue;
        }
        if (existing.startPos.DistanceSqr(candidate.startPos) > 160000.0f ||
            directionDot < 0.9985f)
            continue;
        if (existing.Type() != ZDSpellType::Line &&
            existing.endPos.DistanceSqr(candidate.endPos) > 22500.0f)
            continue;
        return &existing;
    }
    return nullptr;
}

template <typename ThreatState>
inline int MergeNormalizedCastDuplicate(
        ThreatState& existing,
        const ThreatState& candidate) {
    if (existing.missileBound) return -1;
    const int originalId = existing.id;
    const int originalStart = existing.startTick;
    const int originalRevision = existing.revision;
    existing = candidate;
    existing.id = originalId;
    existing.startTick = std::min(originalStart, candidate.startTick);
    existing.revision = originalRevision + 1;
    return originalId;
}

inline bool CompatibleCastGeometry(const CastEventKey& left,
                                   const CastEventKey& right) {
    const auto usable = [](const Vec2& position) {
        return position.IsValid() && !position.IsZero();
    };
    const bool leftStart = usable(left.startPosition);
    const bool rightStart = usable(right.startPosition);
    bool compared = false;
    if (leftStart && rightStart) {
        compared = true;
        if (left.startPosition.DistanceSqr(right.startPosition) > 160000.0f)
            return false;
    }

    const Vec2 leftEndpoints[] = {
        left.endPosition,
        left.castPosition
    };
    const Vec2 rightEndpoints[] = {
        right.endPosition,
        right.castPosition
    };
    for (const Vec2& leftEndpoint : leftEndpoints) {
        if (!usable(leftEndpoint)) continue;
        for (const Vec2& rightEndpoint : rightEndpoints) {
            if (!usable(rightEndpoint)) continue;
            compared = true;
            if (leftEndpoint.DistanceSqr(rightEndpoint) > 160000.0f)
                return false;
            if (!leftStart || !rightStart) continue;
            const Vec2 leftDirection =
                (leftEndpoint - left.startPosition).Normalized();
            const Vec2 rightDirection =
                (rightEndpoint - right.startPosition).Normalized();
            if (!leftDirection.IsZero() && !rightDirection.IsZero() &&
                leftDirection.Dot(rightDirection) < 0.98f)
                return false;
        }
    }
    return compared;
}

inline bool SameLogicalCast(const CastEventKey& left,
                            const CastEventKey& right,
                            int toleranceMs = 120) {
    if (left.casterNetworkId != right.casterNetworkId || left.slot != right.slot)
        return false;
    if (TickDistance(left.tick, right.tick) >
        static_cast<std::uint64_t>(std::max(0, toleranceMs)))
        return false;
    if (left.castIdentity != 0 && right.castIdentity != 0)
        return left.castIdentity == right.castIdentity;
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
};

struct MissileBindObservation {
    std::uint32_t casterNetworkId = 0;
    std::uintptr_t castIdentity = 0;
    std::uintptr_t spellIdentity = 0;
    Vec2 direction = {};
    std::int64_t tick = 0;
};

inline bool CanBindMissile(const MissileBindKey& cast,
                           const MissileBindObservation& missile,
                           float minimumDirectionDot = 0.95f) {
    if (cast.casterNetworkId == 0 ||
        cast.casterNetworkId != missile.casterNetworkId ||
        cast.spellIdentity == 0 ||
        cast.spellIdentity != missile.spellIdentity)
        return false;
    if (cast.castIdentity != 0 && missile.castIdentity != 0 &&
        cast.castIdentity != missile.castIdentity)
        return false;
    if (missile.tick < cast.castTick ||
        TickDistance(missile.tick, cast.castTick) >
            static_cast<std::uint64_t>(MissileBindWindowMs(
                cast.spellDelayMs, cast.expectedLaunchDelayMs)))
        return false;
    const Vec2 castDirection = cast.direction.Normalized();
    const Vec2 missileDirection = missile.direction.Normalized();
    return castDirection.IsValid() && missileDirection.IsValid() &&
           !castDirection.IsZero() && !missileDirection.IsZero() &&
           castDirection.Dot(missileDirection) >= minimumDirectionDot;
}

inline bool ShouldTerminateMissingMissile(
        bool missileBound,
        bool projectileTerminated,
        bool missileObserved,
        std::int64_t missingSinceTick,
        std::int64_t currentTick,
        int observationGraceMs = 250) {
    if (!missileBound || projectileTerminated || missileObserved ||
        missingSinceTick < 0 || currentTick < missingSinceTick)
        return false;
    return TickDistance(currentTick, missingSinceTick) >=
        static_cast<std::uint64_t>(std::max(0, observationGraceMs));
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
