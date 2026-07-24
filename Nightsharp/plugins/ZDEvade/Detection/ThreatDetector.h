#pragma once

#include "Threat.h"
#include "ThreatDetectionPolicy.h"
#include "../Database/ThreatDatabase.h"
#include "../../../SDK/SDK.h"
#include "../../../SDK/GameObjects/YasuoWallTracker.h"
#include "../../../Core/CoreNavGrid.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ZDEvade {

class ThreatDetector {
public:
    static void Initialize() {
        if (initialized) return;
        ThreatDatabase::Initialize();
        ResetState();
        const auto player = SDK::ObjectManager::Player();
        localPlayerNetworkId.store(
            player.IsValid() ? static_cast<std::uint32_t>(player.NetworkId()) : 0u,
            std::memory_order_release);
        localPlayerTeam.store(
            player.IsValid() ? static_cast<std::uint32_t>(player.Team()) : 0u,
            std::memory_order_release);
        initialized = true;
        SDK::Events::AddOnCoreHook(SDK::Events::Hooks::OnProcessSpell, &OnRawProcessSpell);
        SDK::Events::AddOnCoreHook(SDK::Events::Hooks::OnDoCast, &OnRawDoCast);
        SDK::Events::AddOnMissileCreate(&OnMissileCreate);
        SDK::Events::AddOnMissileDelete(&OnMissileDelete);
        SDK::Events::AddOnCreateObject(&OnObjectCreate);
        SDK::Events::AddOnDeleteObject(&OnObjectDelete);
        SDK::Events::AddOnGameUpdate(&OnGameUpdate);
    }

    static void Shutdown() {
        if (!initialized) return;
        initialized = false;
        SDK::Events::RemoveOnGameUpdate(&OnGameUpdate);
        SDK::Events::RemoveOnDeleteObject(&OnObjectDelete);
        SDK::Events::RemoveOnCreateObject(&OnObjectCreate);
        SDK::Events::RemoveOnMissileDelete(&OnMissileDelete);
        SDK::Events::RemoveOnMissileCreate(&OnMissileCreate);
        SDK::Events::RemoveOnCoreHook(SDK::Events::Hooks::OnDoCast, &OnRawDoCast);
        SDK::Events::RemoveOnCoreHook(SDK::Events::Hooks::OnProcessSpell, &OnRawProcessSpell);
        ResetState();
    }

    static std::vector<Threat> Snapshot() {
        AcquireSRWLockShared(&storeLock);
        std::vector<Threat> result = threats;
        ReleaseSRWLockShared(&storeLock);
        return result;
    }

    static int ChangeSerial() { return changeSerial.load(std::memory_order_acquire); }
    static int LastChangeTick() { return lastChangeTick.load(std::memory_order_acquire); }
    static int LastChangedThreatId() { return lastChangedThreatId.load(std::memory_order_acquire); }
    static int DroppedRawEvents() { return droppedRawEvents.load(std::memory_order_acquire); }
    static int CoalescedRawEvents() {
        return coalescedRawEvents.load(std::memory_order_acquire);
    }
    static int ProtectedOverflowRawEvents() {
        return protectedOverflowRawEvents.load(std::memory_order_acquire);
    }
    static int ReplacedLowerPriorityRawEvents() {
        return replacedLowerPriorityRawEvents.load(std::memory_order_acquire);
    }
    static int DroppedLowerPriorityRawEvents() {
        return droppedLowerPriorityRawEvents.load(std::memory_order_acquire);
    }
    static int DroppedCapacityAllProtectedRawEvents() {
        return droppedCapacityAllProtectedRawEvents.load(
            std::memory_order_acquire);
    }
    static int DroppedProtectedOverflowLimitRawEvents() {
        return droppedProtectedOverflowLimitRawEvents.load(
            std::memory_order_acquire);
    }
    static int UnsupportedArcDropped() {
        AcquireSRWLockShared(&admissionLock);
        const int result = admissionCounters.unsupportedArcDropped;
        ReleaseSRWLockShared(&admissionLock);
        return result;
    }
    static std::size_t DatabaseCount() { return ThreatDatabase::Count(); }

    static void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
        if (!initialized || !args.Sender.Ptr) return;
        const SDK::GameObject object(args.Sender.Ptr, args.Sender.Type);
        if (!object.IsValid() || object.IsAlly()) return;
        const auto player = SDK::ObjectManager::Player();
        if (player.IsValid() && args.Sender.Team != 0 &&
            args.Sender.Team == static_cast<std::uint32_t>(player.Team())) return;
        const int objectId = args.Sender.NetworkId != 0
            ? static_cast<int>(args.Sender.NetworkId)
            : object.NetworkId();
        if (objectId == 0) return;

        const std::string objectName = args.Sender.Name[0]
            ? args.Sender.Name
            : object.Name();
        const std::string characterName = args.Sender.CharacterName[0]
            ? args.Sender.CharacterName
            : object.CharacterName();
        const SpellData* data = nullptr;
        for (const auto& entry : SpellDatabase::Spells) {
            if (!entry.hasTrap || entry.trapBaseName.empty()) continue;
            if (ContainsNoCase(objectName.c_str(), entry.trapBaseName.c_str()) ||
                ContainsNoCase(characterName.c_str(), entry.trapBaseName.c_str())) {
                data = &entry;
                break;
            }
        }
        if (!data) return;
        if (!AdmitData(data)) return;

        Vec2 position = args.Sender.Position.To2D();
        if (!HasUsablePosition(position)) position = object.Position().To2D();
        if (!HasUsablePosition(position)) return;

        std::uint32_t casterNetworkId = 0;
        for (const auto& hero : SDK::GameObjects::EnemyHeroes()) {
            if (hero.IsValid() && EqualsNoCase(data->charName, hero.CharacterName().c_str())) {
                casterNetworkId = static_cast<std::uint32_t>(hero.NetworkId());
                break;
            }
        }

        int changedId = -1;
        AcquireSRWLockExclusive(&storeLock);
        const bool exists = std::any_of(threats.begin(), threats.end(),
            [objectId](const Threat& threat) {
                return !threat.expired && threat.trapObjectId == objectId;
            });
        if (!exists) {
            Threat trap;
            trap.id = nextThreatId++;
            trap.data = data;
            trap.startPos = position;
            trap.endPos = position;
            trap.authoredEndPos = position;
            trap.startTick = SDK::Variables::TickCount();
            trap.endTick = std::numeric_limits<int>::max();
            trap.casterNetworkId = casterNetworkId;
            trap.slot = static_cast<int>(data->spellKey);
            trap.trapObjectId = objectId;
            trap.persistent = true;
            changedId = trap.id;
            threats.push_back(std::move(trap));
        }
        ReleaseSRWLockExclusive(&storeLock);
        if (changedId >= 0) MarkChanged(changedId);
    }

    static void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
        if (!initialized) return;
        int objectId = static_cast<int>(args.Sender.NetworkId);
        if (objectId == 0 && args.Sender.Ptr)
            objectId = SDK::GameObject(args.Sender.Ptr, args.Sender.Type).NetworkId();
        if (objectId == 0) return;
        int changedId = -1;
        AcquireSRWLockExclusive(&storeLock);
        for (auto& threat : threats) {
            if (!threat.expired && threat.trapObjectId == objectId) {
                threat.expired = true;
                changedId = threat.id;
            }
        }
        ReleaseSRWLockExclusive(&storeLock);
        if (changedId >= 0) MarkChanged(changedId);
    }

private:
    struct MissileObservation {
        int threatId = -1;
        std::uint32_t networkId = 0;
        std::uintptr_t objectIdentity = 0;
        Vec2 position = {};
        Vec2 endPosition = {};
        int tick = 0;
    };

    struct CollisionEvent {
        float distance = FLT_MAX;
        Vec2 point = {};
        Vec2 unitCenter = {};
        ZDCollisionKind kind = ZDCollisionKind::None;
        int networkId = 0;
        std::uintptr_t unitObjectIdentity = 0;
    };

    struct SpecialCastProfile {
        float range = 0.0f;
        int delay = 0;
    };

    static inline constexpr int kPendingCapacity = 256;
    using PendingQueue = PendingEventQueue<
        SDK::Events::ProcessSpellEventArgs, kPendingCapacity>;
    static inline SRWLOCK pendingLock = SRWLOCK_INIT;
    static inline PendingQueue pendingQueue = {};
    static inline std::atomic<int> droppedRawEvents = 0;
    static inline std::atomic<int> coalescedRawEvents = 0;
    static inline std::atomic<int> protectedOverflowRawEvents = 0;
    static inline std::atomic<int> replacedLowerPriorityRawEvents = 0;
    static inline std::atomic<int> droppedLowerPriorityRawEvents = 0;
    static inline std::atomic<int> droppedCapacityAllProtectedRawEvents = 0;
    static inline std::atomic<int> droppedProtectedOverflowLimitRawEvents = 0;
    static inline std::atomic<std::uint32_t> localPlayerNetworkId = 0;
    static inline std::atomic<std::uint32_t> localPlayerTeam = 0;
    static inline std::atomic<std::uint32_t> gameUpdateThreadId = 0;

    static inline SRWLOCK admissionLock = SRWLOCK_INIT;
    static inline ThreatAdmissionCounters admissionCounters = {};
    static inline constexpr std::size_t kLogicalEpisodeCapacity = 128;
    static inline SRWLOCK logicalEpisodeLock = SRWLOCK_INIT;
    static inline LogicalCastEpisodeResolver<
        kLogicalEpisodeCapacity> logicalCastEpisodes = {};
    static inline SRWLOCK storeLock = SRWLOCK_INIT;
    static inline std::vector<Threat> threats;
    static inline int nextThreatId = 1;
    static inline std::atomic<int> changeSerial = 0;
    static inline std::atomic<int> lastChangeTick = 0;
    static inline std::atomic<int> lastChangedThreatId = -1;
    static inline std::atomic<bool> initialized = false;
    static inline std::atomic<int> lastCollisionTick = 0;
    static inline std::unordered_map<int, int> sionFirstSeen;
    static inline thread_local bool processingRawCast = false;

    static void ResetState() {
        AcquireSRWLockExclusive(&pendingLock);
        pendingQueue.Clear();
        droppedRawEvents = 0;
        coalescedRawEvents = 0;
        protectedOverflowRawEvents = 0;
        replacedLowerPriorityRawEvents = 0;
        droppedLowerPriorityRawEvents = 0;
        droppedCapacityAllProtectedRawEvents = 0;
        droppedProtectedOverflowLimitRawEvents = 0;
        ReleaseSRWLockExclusive(&pendingLock);
        gameUpdateThreadId.store(0, std::memory_order_release);
        processingRawCast = false;

        AcquireSRWLockExclusive(&admissionLock);
        ResetThreatAdmissionCounters(admissionCounters);
        ReleaseSRWLockExclusive(&admissionLock);

        AcquireSRWLockExclusive(&logicalEpisodeLock);
        logicalCastEpisodes.Reset();
        ReleaseSRWLockExclusive(&logicalEpisodeLock);

        AcquireSRWLockExclusive(&storeLock);
        threats.clear();
        sionFirstSeen.clear();
        nextThreatId = 1;
        lastCollisionTick = 0;
        changeSerial = 0;
        lastChangeTick = 0;
        lastChangedThreatId = -1;
        ReleaseSRWLockExclusive(&storeLock);
    }

    static bool ContainsNoCase(const char* value, const char* token) {
        if (!value || !token || !value[0] || !token[0]) return false;
        const std::size_t valueLength = std::strlen(value);
        const std::size_t tokenLength = std::strlen(token);
        if (tokenLength > valueLength) return false;
        for (std::size_t i = 0; i + tokenLength <= valueLength; ++i) {
            bool equal = true;
            for (std::size_t j = 0; j < tokenLength; ++j) {
                const unsigned char left = static_cast<unsigned char>(value[i + j]);
                const unsigned char right = static_cast<unsigned char>(token[j]);
                if (std::tolower(left) != std::tolower(right)) {
                    equal = false;
                    break;
                }
            }
            if (equal) return true;
        }
        return false;
    }

    static bool EqualsNoCase(const std::string& left, const char* right) {
        if (!right || left.size() != std::strlen(right)) return false;
        for (std::size_t i = 0; i < left.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(left[i])) !=
                std::tolower(static_cast<unsigned char>(right[i]))) return false;
        }
        return true;
    }

    static bool IsBasicAttackName(const char* name) {
        return name && name[0] &&
            (IsExplicitBasicAttackCastName(name) ||
             SDK::Orbwalker::IsAutoAttack(std::string(name)));
    }

    static bool IsUtilityName(const char* name) {
        if (!name || !name[0]) return false;
        return ContainsNoCase(name, "summoner") || ContainsNoCase(name, "item");
    }

    static bool HasBasicAttackName(const SDK::Events::ProcessSpellEventArgs& args) {
        return IsBasicAttackName(args.SpellName) ||
               IsBasicAttackName(args.PayloadSpellName) ||
               IsBasicAttackName(args.ScriptName) ||
               IsBasicAttackName(args.SpellSlotName) ||
               IsBasicAttackName(args.MissileName) ||
               IsBasicAttackName(args.PayloadMissileName);
    }

    static bool HasUtilityName(const SDK::Events::ProcessSpellEventArgs& args) {
        return IsUtilityName(args.SpellName) ||
               IsUtilityName(args.PayloadSpellName) ||
               IsUtilityName(args.ScriptName) ||
               IsUtilityName(args.SpellSlotName) ||
               IsUtilityName(args.MissileName) ||
               IsUtilityName(args.PayloadMissileName);
    }

    static PendingPriority PendingPriorityFor(
            const SDK::Events::ProcessSpellEventArgs& args) {
        const ProcessSpellMatchResult match = MatchProcessSpellResult(args);
        if (IsRejectedProcessSpellMatch(match))
            return PendingPriority::Noise;
        const SpellData* databaseMatch = match.data;
        const bool hasName =
            args.SpellName[0] || args.PayloadSpellName[0] ||
            args.ScriptName[0] || args.SpellSlotName[0] ||
            args.MissileName[0] || args.PayloadMissileName[0];
        const bool hasPosition =
            HasUsablePosition(args.StartPosition.To2D()) ||
            HasUsablePosition(args.EndPosition.To2D()) ||
            HasUsablePosition(args.CastPosition.To2D());
        if (args.Slot < 0 || args.Slot > 3 || !hasName || !hasPosition)
            return PendingPriority::Noise;

        const std::uint32_t playerId =
            localPlayerNetworkId.load(std::memory_order_acquire);
        const std::uint32_t playerTeam =
            localPlayerTeam.load(std::memory_order_acquire);
        // ThreatDatabase is initialized before callbacks are registered and is
        // immutable while detection is active, so this read-only match is safe
        // before taking the pending-queue lock.
        return ClassifyPendingCastPriority({
            false,
            args.Sender.IsValid(),
            args.Sender.NetworkId,
            args.Sender.Team,
            args.CasterNetworkId,
            playerId,
            playerTeam,
            databaseMatch != nullptr,
        });
    }

    static NormalizedCastNameSet PendingSpellNames(
            const SDK::Events::ProcessSpellEventArgs& args) {
        NormalizedCastNameSet result;
        const char* names[] = {
            args.SpellName,
            args.PayloadSpellName,
            args.ScriptName,
            args.SpellSlotName,
            args.MissileName,
            args.PayloadMissileName
        };
        for (const char* name : names)
            AddCastSpellName(result, name);
        return result;
    }

    template <std::size_t Size>
    static void MergeCastName(char (&current)[Size],
                              const char (&incoming)[Size]) {
        if (ShouldPreferCastName(current, incoming))
            std::memcpy(current, incoming, Size);
    }

    static void MergePendingArgs(
            SDK::Events::ProcessSpellEventArgs& current,
            const SDK::Events::ProcessSpellEventArgs& incoming) {
        if (ShouldSelectIncomingSpellName(
                current.SpellName,
                current.SpellNameFromSlotFallback,
                incoming.SpellName,
                incoming.SpellNameFromSlotFallback)) {
            std::memcpy(
                current.SpellName,
                incoming.SpellName,
                sizeof(current.SpellName));
            current.SpellNameFromSlotFallback =
                incoming.SpellNameFromSlotFallback;
        }
        MergeCastName(current.PayloadSpellName, incoming.PayloadSpellName);
        MergeCastName(current.ScriptName, incoming.ScriptName);
        MergeCastName(current.SpellSlotName, incoming.SpellSlotName);
        MergeCastName(current.MissileName, incoming.MissileName);
        MergeCastName(current.PayloadMissileName, incoming.PayloadMissileName);
        if (!HasUsablePosition(current.StartPosition.To2D()) &&
            HasUsablePosition(incoming.StartPosition.To2D()))
            current.StartPosition = incoming.StartPosition;
        if (!HasUsablePosition(current.EndPosition.To2D()) &&
            HasUsablePosition(incoming.EndPosition.To2D()))
            current.EndPosition = incoming.EndPosition;
        if (!HasUsablePosition(current.CastPosition.To2D()) &&
            HasUsablePosition(incoming.CastPosition.To2D()))
            current.CastPosition = incoming.CastPosition;
        if (!current.Sender.IsValid() && incoming.Sender.IsValid())
            current.Sender = incoming.Sender;
        if (!current.Target.IsValid() && incoming.Target.IsValid())
            current.Target = incoming.Target;
        if (current.CastInfo == 0) current.CastInfo = incoming.CastInfo;
        if (current.SpellInput == 0) current.SpellInput = incoming.SpellInput;
        if (current.SpellData == 0) current.SpellData = incoming.SpellData;
        if (current.SpellDataResource == 0)
            current.SpellDataResource = incoming.SpellDataResource;
        if (current.Spellbook == 0) current.Spellbook = incoming.Spellbook;
        if (current.CasterNetworkId == 0)
            current.CasterNetworkId = incoming.CasterNetworkId;
        if (current.TargetNetworkId == 0)
            current.TargetNetworkId = incoming.TargetNetworkId;
        if (current.SourceIndex < 0) current.SourceIndex = incoming.SourceIndex;
        if (current.TargetIndex < 0) current.TargetIndex = incoming.TargetIndex;
        if (current.CastDelay <= 0.0f) current.CastDelay = incoming.CastDelay;
        if (current.MissileSpeed <= 0.0f)
            current.MissileSpeed = incoming.MissileSpeed;
        current.IsSpell = current.IsSpell || incoming.IsSpell;
        current.IsAutoAttack = current.IsAutoAttack || incoming.IsAutoAttack;
        current.IsSpecialAttack =
            current.IsSpecialAttack || incoming.IsSpecialAttack;
    }

    static void PushPending(const SDK::Events::ProcessSpellEventArgs& args) {
        if (!initialized ||
            (!args.Sender.IsValid() && args.CasterNetworkId == 0)) return;

        PendingEventDescriptor descriptor;
        descriptor.key = {
            args.Sender.NetworkId != 0
                ? args.Sender.NetworkId
                : args.CasterNetworkId,
            args.CastInfo,
            args.Slot,
            static_cast<std::int64_t>(::GetTickCount64())
        };
        descriptor.key.spellNames = PendingSpellNames(args);
        descriptor.key.startPosition = args.StartPosition.To2D();
        descriptor.key.endPosition = args.EndPosition.To2D();
        descriptor.key.castPosition = args.CastPosition.To2D();
        descriptor.priority = PendingPriorityFor(args);

        AcquireSRWLockExclusive(&pendingLock);
        const PendingQueueDecision decision =
            pendingQueue.Push(args, descriptor, MergePendingArgs);
        switch (decision) {
        case PendingQueueDecision::Append:
            break;
        case PendingQueueDecision::AppendProtectedOverflow:
            ++protectedOverflowRawEvents;
            break;
        case PendingQueueDecision::Coalesce:
            ++coalescedRawEvents;
            break;
        case PendingQueueDecision::ReplaceLowerPriority:
            ++replacedLowerPriorityRawEvents;
            ++droppedRawEvents;
            break;
        case PendingQueueDecision::DropLowerPriority:
            ++droppedLowerPriorityRawEvents;
            ++droppedRawEvents;
            break;
        case PendingQueueDecision::DropCapacityAllProtected:
            ++droppedCapacityAllProtectedRawEvents;
            ++droppedRawEvents;
            break;
        case PendingQueueDecision::DropProtectedOverflowLimit:
            ++droppedProtectedOverflowLimitRawEvents;
            ++droppedRawEvents;
            break;
        }
        ReleaseSRWLockExclusive(&pendingLock);
    }

    static bool PopPending(SDK::Events::ProcessSpellEventArgs& out) {
        AcquireSRWLockExclusive(&pendingLock);
        const bool popped = pendingQueue.Pop(out);
        ReleaseSRWLockExclusive(&pendingLock);
        return popped;
    }

    static std::size_t PendingCount() {
        AcquireSRWLockShared(&pendingLock);
        const std::size_t count = pendingQueue.Size();
        ReleaseSRWLockShared(&pendingLock);
        return count;
    }

    static bool HasUsablePosition(const Vec2& position) {
        return position.IsValid() && !position.IsZero();
    }

    static bool IsSpellForSender(const SpellData& data,
                                 const SDK::Events::ProcessSpellEventArgs& args) {
        if (data.charName.empty() || data.charName == "AllChampions") return true;
        if (!args.Sender.CharacterName[0]) return true;
        return EqualsNoCase(data.charName, args.Sender.CharacterName);
    }

    static bool HasUsableCastGeometry(
            const SDK::Events::ProcessSpellEventArgs& args) {
        const Vec2 start = args.StartPosition.To2D();
        if (!HasUsablePosition(start)) return false;
        const Vec2 endpoints[] = {
            args.EndPosition.To2D(),
            args.CastPosition.To2D()
        };
        for (const Vec2& endpoint : endpoints) {
            if (HasUsablePosition(endpoint) &&
                start.DistanceSqr(endpoint) > 1.0f)
                return true;
        }
        return false;
    }

    static ProcessSpellMatchResult MatchProcessSpellResult(
            const SDK::Events::ProcessSpellEventArgs& args) {
        ProcessSpellMatchInput input;
        input.authoritativeNames = {{
            args.SpellName,
            args.PayloadSpellName,
            args.ScriptName,
            args.MissileName,
            args.PayloadMissileName,
        }};
        input.spellSlotName = args.SpellSlotName;
        input.spellNameFromSlotFallback =
            args.SpellNameFromSlotFallback;
        input.isAutoAttack = args.IsAutoAttack;
        input.hasRuntimeAttackSignature = HasBasicAttackName(args);
        input.slot = args.Slot;
        input.targetNetworkId = args.TargetNetworkId != 0
            ? args.TargetNetworkId
            : args.Target.NetworkId;
        input.targetIndex = args.TargetIndex;
        input.hasUsableCastGeometry = HasUsableCastGeometry(args);
        return MatchProcessSpellDatabaseFirst(
            input,
            [&](const char* name) -> const SpellData* {
                const SpellData* data = ThreatDatabase::FindAny(
                    name, args.Sender.CharacterName);
                return data && IsSpellForSender(*data, args)
                    ? data
                    : nullptr;
            });
    }

    static const SpellData* MatchProcessSpell(
            const SDK::Events::ProcessSpellEventArgs& args) {
        return MatchProcessSpellResult(args).data;
    }

    static Vec2 ResolveStart(const SDK::Events::ProcessSpellEventArgs& args) {
        Vec2 start = args.StartPosition.To2D();
        if (!HasUsablePosition(start)) start = args.Sender.Position.To2D();
        return start;
    }

    static Vec2 ResolveEnd(const SpellData& data,
                           const SDK::Events::ProcessSpellEventArgs& args,
                           const Vec2& start,
                           float rangeOverride = -1.0f) {
        Vec2 end = args.EndPosition.To2D();
        if (!HasUsablePosition(end)) end = args.CastPosition.To2D();
        if (!HasUsablePosition(end)) end = start;
        Vec2 direction = (end - start).Normalized();
        if (direction.IsZero()) direction = Vec2(1.0f, 0.0f);
        const float range = rangeOverride >= 0.0f ? rangeOverride : data.range;
        if (data.spellType == ZDSpellType::Arc) {
            return end;
        }
        if (data.spellType == ZDSpellType::Line || data.spellType == ZDSpellType::Cone) {
            if (!data.useEndPosition || start.Distance(end) > range) {
                end = start + direction * std::max(1.0f, range);
            }
        } else if (range > 0.0f && start.Distance(end) > range) {
            end = start + direction * range;
        }
        return end;
    }

    static bool IsAlliedSender(const ::Core::Events::ObjectInfo& sender) {
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return true;
        if (sender.NetworkId == static_cast<std::uint32_t>(player.NetworkId())) return true;
        if (sender.Team != 0 && sender.Team == static_cast<std::uint32_t>(player.Team())) return true;
        if (sender.NetworkId != 0) {
            const auto unit = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
                static_cast<int>(sender.NetworkId));
            if (unit.IsValid() && unit.IsAlly()) return true;
        }
        return false;
    }

    template <std::size_t Size>
    static void CopyResolvedName(char (&destination)[Size],
                                 const std::string& source) {
        const std::size_t length =
            std::min(source.size(), Size - 1);
        std::memcpy(destination, source.data(), length);
        destination[length] = '\0';
    }

    static bool ResolveProcessCaster(
            const SDK::Events::ProcessSpellEventArgs& incoming,
            SDK::Events::ProcessSpellEventArgs& resolved) {
        resolved = incoming;
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return false;

        if (incoming.Sender.IsValid())
            return !IsAlliedSender(incoming.Sender);

        const auto caster =
            SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
                static_cast<int>(incoming.CasterNetworkId));
        const Vec2 casterPosition = caster.IsValid()
            ? caster.ServerPosition().To2D()
            : Vec2();
        const std::string characterName = caster.IsValid()
            ? caster.CharacterName()
            : std::string();
        const CastCasterResolutionDecision decision =
            DecideCastCasterResolution({
                false,
                incoming.Sender.NetworkId,
                incoming.Sender.Team,
                incoming.CasterNetworkId,
                caster.IsValid() &&
                    !characterName.empty() &&
                    HasUsablePosition(casterPosition),
                caster.IsValid()
                    ? static_cast<std::uint32_t>(caster.NetworkId())
                    : 0u,
                caster.IsValid()
                    ? static_cast<std::uint32_t>(caster.Team())
                    : 0u,
                static_cast<std::uint32_t>(player.NetworkId()),
                static_cast<std::uint32_t>(player.Team()),
            });
        if (decision !=
            CastCasterResolutionDecision::UseResolvedNetworkId)
            return false;

        resolved.Sender.Ptr = caster.Address();
        resolved.Sender.NetworkId =
            static_cast<std::uint32_t>(caster.NetworkId());
        resolved.Sender.Index =
            static_cast<std::uint32_t>(caster.Index());
        resolved.Sender.Team =
            static_cast<std::uint32_t>(caster.Team());
        resolved.Sender.Type = caster.Type();
        resolved.Sender.IsDead = caster.IsDead();
        resolved.Sender.Position = caster.ServerPosition();
        CopyResolvedName(resolved.Sender.Name, caster.Name());
        CopyResolvedName(
            resolved.Sender.CharacterName, characterName);
        return resolved.Sender.IsValid();
    }

    static float DirectionDot(const Vec2& left, const Vec2& right) {
        if (left.IsZero() || right.IsZero()) return -1.0f;
        return left.Normalized().Dot(right.Normalized());
    }

    static Vec2 RotateDirection(const Vec2& direction, float degrees) {
        const float radians = degrees * 3.14159265358979323846f / 180.0f;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        return Vec2(
            direction.x * cosine - direction.y * sine,
            direction.x * sine + direction.y * cosine).Normalized();
    }

    static int ResolveProjectileLaneIndex(
        const SpellData& data,
        const Vec2& episodeDirection,
        const Vec2& projectileDirection) {
        const int projectileCount = data.spellType == ZDSpellType::Line
            ? std::clamp(data.multipleNumber, 1, 15)
            : 1;
        if (projectileCount <= 1 ||
            episodeDirection.IsZero() ||
            projectileDirection.IsZero()) {
            return -1;
        }
        const float centerIndex =
            static_cast<float>(projectileCount - 1) * 0.5f;
        int bestIndex = -1;
        float bestDot = -1.0f;
        for (int index = 0; index < projectileCount; ++index) {
            const float angleOffset =
                (static_cast<float>(index) - centerIndex) *
                data.multipleAngle;
            const float dot = RotateDirection(
                episodeDirection,
                angleOffset).Dot(
                    projectileDirection.Normalized());
            if (dot > bestDot) {
                bestDot = dot;
                bestIndex = index;
            }
        }
        return bestDot >= kLogicalLaneDirectionDot
            ? bestIndex
            : -1;
    }

    static Threat* FindDuplicateLocked(const Threat& candidate) {
        return FindNormalizedCastDuplicate(threats, candidate);
    }

    static bool AdmitData(const SpellData* data) {
        AcquireSRWLockExclusive(&admissionLock);
        const bool admitted = AdmitThreatData(data, admissionCounters);
        ReleaseSRWLockExclusive(&admissionLock);
        return admitted;
    }

    static std::uint64_t ResolveLogicalCastEpisode(
        const SpellData* data,
        std::uint32_t casterNetworkId,
        int slot,
        int episodeTick,
        int observationTick,
        const Vec2& start,
        const Vec2& end,
        const Vec2& direction,
        Vec2* episodeDirectionOut = nullptr,
        bool projectileObservation = false,
        bool* matchedExistingOut = nullptr,
        std::uint64_t* projectileLaneKeyOut = nullptr) {
        LogicalCastEpisodeObservation observation;
        observation.casterNetworkId = casterNetworkId;
        observation.data = data;
        observation.slot = slot;
        observation.episodeTick = episodeTick;
        observation.observationTick = observationTick;
        observation.start = start;
        observation.end = end;
        observation.direction = direction;
        observation.projectileObservation =
            projectileObservation;
        AcquireSRWLockExclusive(&logicalEpisodeLock);
        const std::uint64_t result =
            logicalCastEpisodes.Resolve(
                observation,
                episodeDirectionOut,
                matchedExistingOut,
                projectileLaneKeyOut);
        ReleaseSRWLockExclusive(&logicalEpisodeLock);
        return result;
    }

    static std::uint64_t ResolveLogicalProjectileLaneKey(
        std::uint64_t logicalCastEpisodeId,
        const Vec2& authoredInitialWorldDirection,
        int projectileIndex = -1) {
        AcquireSRWLockExclusive(&logicalEpisodeLock);
        const std::uint64_t result =
            logicalCastEpisodes.ResolveLaneKey(
                logicalCastEpisodeId,
                authoredInitialWorldDirection,
                projectileIndex);
        ReleaseSRWLockExclusive(&logicalEpisodeLock);
        return result;
    }

    static void PruneLogicalCastEpisodes(int now) {
        AcquireSRWLockExclusive(&logicalEpisodeLock);
        logicalCastEpisodes.Prune(now);
        ReleaseSRWLockExclusive(&logicalEpisodeLock);
    }

    static int AddOrUpdateThreat(const Threat& candidate) {
        if (!AdmitData(candidate.data)) return -1;
        int changedId = -1;
        AcquireSRWLockExclusive(&storeLock);
        if (Threat* existing = FindDuplicateLocked(candidate)) {
            changedId =
                MergeNormalizedCastDuplicate(*existing, candidate);
        } else {
            Threat inserted = candidate;
            inserted.id = nextThreatId++;
            changedId = inserted.id;
            threats.push_back(std::move(inserted));
        }
        ReleaseSRWLockExclusive(&storeLock);
        if (changedId >= 0) MarkChanged(changedId);
        return changedId;
    }

    static SDK::AIBaseClient ResolveCaster(
            const ::Core::Events::ObjectInfo& sender) {
        if (sender.NetworkId != 0) {
            const auto caster = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
                static_cast<int>(sender.NetworkId));
            if (caster.IsValid()) return caster;
        }
        return sender.Ptr ? SDK::AIBaseClient(sender.Ptr, sender.Type) : SDK::AIBaseClient();
    }

    static int EventCastDelayMs(float castDelay) {
        if (!std::isfinite(castDelay) || castDelay <= 0.0f) return 0;
        const double milliseconds = castDelay < 60.0f
            ? static_cast<double>(castDelay) * 1000.0
            : static_cast<double>(castDelay);
        return static_cast<int>(std::clamp(
            milliseconds, 0.0, 60000.0));
    }

    static SpecialCastProfile ResolveSpecialCast(
            const SpellData& data,
            const SDK::Events::ProcessSpellEventArgs& args,
            const Vec2& start) {
        const int eventDelay = EventCastDelayMs(args.CastDelay);
        SpecialCastProfile profile{
            data.range,
            std::max(std::max(0, data.spellDelay), eventDelay)
        };
        const SDK::AIBaseClient caster = ResolveCaster(args.Sender);
        if (EqualsNoCase(data.spellName, "WarwickR") && caster.IsValid()) {
            profile.range = std::ceil(2.5f * std::max(0.0f, caster.MoveSpeed()));
        } else if (EqualsNoCase(data.spellName, "JinxW") && caster.IsValid()) {
            profile.delay = std::max(eventDelay, static_cast<int>(std::max(
                400.0f, 600.0f - caster.AttackSpeedMod() / 2.5f * 200.0f)));
        } else if (EqualsNoCase(data.spellName, "ZiggsR")) {
            Vec2 castEnd = args.EndPosition.To2D();
            if (!HasUsablePosition(castEnd)) castEnd = args.CastPosition.To2D();
            const float range = std::max(1.0f, profile.range);
            const float distance = HasUsablePosition(castEnd)
                ? std::min(range, start.Distance(castEnd))
                : 0.0f;
            profile.delay = std::max(eventDelay, static_cast<int>(
                1500.0f + 1500.0f * distance / range));
        }
        return profile;
    }

    static void AddZiggsBounceThreats(
            const SpellData& source,
            const SDK::Events::ProcessSpellEventArgs& args,
            const Vec2& start,
            const Vec2& firstEnd,
            const Vec2& direction,
            int castTick) {
        if (!EqualsNoCase(source.spellName, "ZiggsQ")) return;
        const SpellData* second = ThreatDatabase::FindAny("ZiggsQBounce1", "Ziggs");
        const SpellData* third = ThreatDatabase::FindAny("ZiggsQBounce2", "Ziggs");
        if (!second || !third) return;
        const float firstDistance = start.Distance(firstEnd);
        if (firstDistance <= 1.0f) return;
        const float secondDistance = firstDistance * 0.4f;
        const float thirdDistance = secondDistance * 0.69f;
        const Vec2 secondEnd = firstEnd + direction * secondDistance;
        const Vec2 thirdEnd = secondEnd + direction * thirdDistance;
        const int firstTravel = ClampTickOffset(std::ceil(
            1000.0f * firstDistance /
            std::max(1.0f, source.projectileSpeed)));
        const int secondLaunch = SaturatingTickAdd(
            SaturatingTickAdd(std::max(0, source.spellDelay), firstTravel), 500);
        const int thirdLaunch = SaturatingTickAdd(
            SaturatingTickAdd(secondLaunch, 480), 500);
        const auto addBounce = [&](const SpellData* data,
                                   const Vec2& bounceStart,
                                   const Vec2& bounceEnd,
                                   float distance,
                                   int launchOffset) {
            Threat bounce;
            bounce.data = data;
            bounce.startPos = bounceStart;
            bounce.endPos = bounceEnd;
            bounce.authoredEndPos = bounceEnd;
            bounce.direction = direction;
            bounce.startTick = SaturatingTickAdd(
                SaturatingTickAdd(castTick, launchOffset),
                -std::max(0, data->spellDelay));
            bounce.endTick = SaturatingTickAdd(
                SaturatingTickAdd(
                    SaturatingTickAdd(castTick, launchOffset), 480),
                std::max(0, data->extraEndTime));
            bounce.castIdentity = args.CastInfo;
            bounce.casterNetworkId = args.Sender.NetworkId;
            bounce.slot = static_cast<int>(ZDSpellSlot::Q);
            bounce.logicalCastEpisodeId =
                ResolveLogicalCastEpisode(
                    data,
                    bounce.casterNetworkId,
                    bounce.slot,
                    bounce.startTick,
                    SDK::Variables::TickCount(),
                    bounce.startPos,
                    bounce.endPos,
                    bounce.direction);
            bounce.observedSpeed = std::max(1.0f, distance * 1000.0f / 480.0f);
            AddOrUpdateThreat(bounce);
        };
        addBounce(second, firstEnd, secondEnd, secondDistance, secondLaunch);
        addBounce(third, secondEnd, thirdEnd, thirdDistance, thirdLaunch);
    }

    static void UpdateSpecialThreats(int now) {
        std::vector<Threat> snapshots;
        AcquireSRWLockShared(&storeLock);
        for (const auto& threat : threats) {
            if (!threat.expired && EqualsNoCase(threat.SpellName(), "SionR"))
                snapshots.push_back(threat);
        }
        ReleaseSRWLockShared(&storeLock);

        std::vector<int> changedIds;
        for (auto& snapshot : snapshots) {
            const int revision = snapshot.revision;
            const int casterId = static_cast<int>(snapshot.casterNetworkId);
            const SDK::AIBaseClient caster =
                SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(casterId);
            bool changed = false;
            if (!caster.IsValid() || caster.IsDead()) {
                snapshot.expired = true;
                sionFirstSeen.erase(casterId);
                changed = true;
            } else {
                const auto inserted = sionFirstSeen.emplace(casterId, now);
                const std::int64_t elapsed =
                    TickDifference(now, inserted.first->second);
                if (!caster.HasBuff("SionR") && elapsed > 600) {
                    snapshot.expired = true;
                    sionFirstSeen.erase(casterId);
                    changed = true;
                } else {
                    const Vec2 direction = SionChargeDirection(
                        caster.Direction().To2D(), snapshot.direction);
                    const Vec2 position = caster.ServerPosition().To2D();
                    if (HasUsablePosition(position) && !direction.IsZero()) {
                        const float speed = std::max(1.0f, caster.MoveSpeed());
                        const float projectionRange =
                            SionChargeProjectionRange(snapshot.Range());
                        const Vec2 end = SionChargeCorridorEnd(
                            position, direction, projectionRange);
                        changed = true;
                        snapshot.startPos = position;
                        snapshot.endPos = end;
                        snapshot.authoredEndPos = end;
                        snapshot.direction = direction;
                        snapshot.startTick = SaturatingTickAdd(now, -snapshot.Delay());
                        snapshot.launchTick = 0;
                        snapshot.endTick = SaturatingTickAdd(
                            now, ClampTickOffset(std::ceil(
                                1000.0f * projectionRange / speed)));
                        snapshot.observedHead = {};
                        snapshot.observedTick = 0;
                        snapshot.observedSpeed = speed;
                        snapshot.positionUncertainty = 0.0f;
                    }
                }
            }
            if (!changed) continue;
            AcquireSRWLockExclusive(&storeLock);
            const auto found = std::find_if(threats.begin(), threats.end(),
                [id = snapshot.id](const Threat& threat) { return threat.id == id; });
            if (found != threats.end() && found->revision == revision) {
                snapshot.revision = revision + 1;
                *found = std::move(snapshot);
                changedIds.push_back(found->id);
            }
            ReleaseSRWLockExclusive(&storeLock);
        }
        for (int id : changedIds) MarkChanged(id);
    }

    static void ProcessCast(
            const SDK::Events::ProcessSpellEventArgs& incoming) {
        SDK::Events::ProcessSpellEventArgs args;
        if (!ResolveProcessCaster(incoming, args)) return;
        const SpellData* data = MatchProcessSpell(args);
        if (!data || data->noProcess) return;
        if (!AdmitData(data)) return;

        const Vec2 start = ResolveStart(args);
        if (!HasUsablePosition(start)) return;
        const SpecialCastProfile special = ResolveSpecialCast(*data, args, start);
        const Vec2 end = ResolveEnd(*data, args, start, special.range);
        if (!HasUsablePosition(end)) return;

        const int now = SDK::Variables::TickCount();
        const int castTick = SaturatingTickAdd(
            now, -std::max(0, SDK::Game::Ping() / 2));
        Vec2 baseDirection = data->spellType == ZDSpellType::Arc
            ? Vec2()
            : (end - start).Normalized();
        if (data->spellType != ZDSpellType::Arc && baseDirection.IsZero())
            baseDirection = Vec2(1.0f, 0.0f);
        const float pathLength = std::max(1.0f, start.Distance(end));
        const int projectileCount = data->spellType == ZDSpellType::Line
            ? std::clamp(data->multipleNumber, 1, 15)
            : 1;
        const std::uint64_t logicalCastEpisodeId =
            ResolveLogicalCastEpisode(
                data,
                args.Sender.NetworkId,
                static_cast<int>(data->spellKey),
                castTick,
                now,
                start,
                end,
                baseDirection);
        const float centerIndex = static_cast<float>(projectileCount - 1) * 0.5f;
        for (int index = 0; index < projectileCount; ++index) {
            const float angleOffset =
                (static_cast<float>(index) - centerIndex) * data->multipleAngle;
            Threat threat;
            threat.data = data;
            threat.startPos = start;
            threat.direction = data->spellType == ZDSpellType::Arc
                ? Vec2()
                : RotateDirection(baseDirection, angleOffset);
            threat.endPos = data->spellType == ZDSpellType::Arc
                ? end
                : start + threat.direction * pathLength;
            threat.authoredEndPos = threat.endPos;
            threat.startTick = castTick;
            threat.launchTick = 0;
            threat.delayOverride = special.delay;
            threat.endTick = CalculateThreatEndTick(
                *threat.data, start, threat.endPos, threat.startTick, 0,
                special.delay);
            threat.castIdentity = args.CastInfo;
            threat.logicalCastEpisodeId =
                logicalCastEpisodeId;
            threat.projectileLaneKey =
                ResolveLogicalProjectileLaneKey(
                    logicalCastEpisodeId,
                    threat.direction,
                    index);
            threat.casterNetworkId = args.Sender.NetworkId;
            threat.slot = static_cast<int>(data->spellKey);
            threat.projectileIndex =
                StableProjectileLaneIndex(
                    projectileCount,
                    index);
            AddOrUpdateThreat(threat);
        }
        AddZiggsBounceThreats(*data, args, start, end, baseDirection, castTick);
    }

#if defined(_MSC_VER)
    __declspec(noinline)
#endif
    static bool InvokeProcessCastSeh(
            const SDK::Events::ProcessSpellEventArgs* args) {
#if defined(_MSC_VER)
        // Keep this SEH frame free of C++ objects that require unwinding.
        __try {
            ProcessCast(*args);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
        return true;
#else
        ProcessCast(*args);
        return true;
#endif
    }

    static void ProcessCastImmediateOrQueue(
            const SDK::Events::ProcessSpellEventArgs& args) {
        if (!initialized) return;
        const ImmediateCastDispatchDecision decision =
            DecideImmediateCastDispatch(
                gameUpdateThreadId.load(std::memory_order_acquire),
                static_cast<std::uint32_t>(::GetCurrentThreadId()),
                processingRawCast);
        if (decision !=
            ImmediateCastDispatchDecision::ProcessImmediately) {
            PushPending(args);
            return;
        }

        processingRawCast = true;
        const bool succeeded = InvokeProcessCastSeh(&args);
        processingRawCast = false;
        if (ShouldQueueCastAfterImmediateAttempt(decision, succeeded))
            PushPending(args);
    }

    static void DrainPendingCasts() {
        const std::uint32_t currentThreadId =
            static_cast<std::uint32_t>(::GetCurrentThreadId());
        if (!IsKnownGameThread(
                gameUpdateThreadId.load(std::memory_order_acquire),
                currentThreadId) ||
            processingRawCast)
            return;
        const std::size_t pendingAtStart = PendingCount();
        SDK::Events::ProcessSpellEventArgs event;
        for (std::size_t processed = 0;
             processed < pendingAtStart && PopPending(event);
             ++processed) {
            ProcessCastImmediateOrQueue(event);
        }
    }

    static void OnRawProcessSpell(const SDK::Events::CoreHookArgs& raw) {
        if (raw.Id != SDK::Events::Hooks::OnProcessSpell) return;
        ProcessCastImmediateOrQueue(
            ::Core::Events::DecodeProcessSpell(raw));
    }

    static void OnRawDoCast(const SDK::Events::CoreHookArgs& raw) {
        if (raw.Id != SDK::Events::Hooks::OnDoCast) return;
        ProcessCastImmediateOrQueue(
            ::Core::Events::DecodeDoCast(raw));
    }

    static void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
        if (!initialized) return;
        DrainPendingCasts();
        if (!args.Sender.IsValid()) return;
        const SDK::MissileClient missile(args.Sender.Ptr);
        if (!missile.IsValid()) return;

        std::uint32_t casterNetworkId = args.SourceNetworkId;
        if (casterNetworkId == 0) casterNetworkId = static_cast<std::uint32_t>(missile.CasterNetworkId());
        const auto player = SDK::ObjectManager::Player();
        if (player.IsValid() &&
            (casterNetworkId == static_cast<std::uint32_t>(player.NetworkId()) ||
             (args.Source.Team != 0 && args.Source.Team == static_cast<std::uint32_t>(player.Team())))) return;
        const auto caster = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
            static_cast<int>(casterNetworkId));
        if (caster.IsValid() && caster.IsAlly()) return;
        std::string casterName = args.Source.CharacterName;
        if (casterName.empty() && caster.IsValid()) casterName = caster.CharacterName();

        const std::string runtimeMissileName = missile.MissileName();
        const std::string runtimeSpellName = missile.SpellName();
        const char* names[] = {
            args.MissileName,
            runtimeMissileName.c_str(),
            args.SpellName,
            runtimeSpellName.c_str(),
        };
        bool runtimeAttackSignature = false;
        for (const char* name : names) {
            runtimeAttackSignature =
                runtimeAttackSignature || IsBasicAttackName(name);
        }
        MissileMatchInput missileInput;
        missileInput.names = {{
            names[0], names[1], names[2], names[3]
        }};
        missileInput.hasRuntimeAttackSignature = runtimeAttackSignature;
        missileInput.eventTargetNetworkId = args.TargetNetworkId;
        missileInput.eventTargetIndex = args.TargetIndex;
        missileInput.runtimeTargetNetworkId =
            static_cast<std::uint32_t>(missile.TargetNetworkId());
        const MissileMatchResult missileMatch =
            MatchMissileDatabaseFirst(
                missileInput,
                [&](const char* name) {
                    return ThreatDatabase::FindMissile(
                        name, casterName.c_str());
                });
        const SpellData* data = missileMatch.data;
        if (!data || data->noProcess) return;
        if (!AdmitData(data)) return;
        // Arc missile kinematics are not implemented; never bind an Arc to
        // the straight-missile observation pipeline.
        if (data->spellType == ZDSpellType::Arc) return;

        Vec2 start = args.StartPosition.To2D();
        if (!HasUsablePosition(start)) start = missile.StartPosition().To2D();
        Vec2 end = args.EndPosition.To2D();
        if (!HasUsablePosition(end)) end = missile.EndPosition().To2D();
        if (!HasUsablePosition(start) || !HasUsablePosition(end)) return;
        Vec2 direction = (end - start).Normalized();
        if (direction.IsZero()) direction = Vec2(1.0f, 0.0f);
        if (data->spellType == ZDSpellType::Line && !data->useEndPosition) {
            end = start + direction * std::max(1.0f, data->range);
        }

        const int now = SDK::Variables::TickCount();
        const int launchTick = SaturatingTickAdd(
            now, -std::max(0, SDK::Game::Ping() / 2));
        bool isViktorAugmentedRay = false;
        for (const char* name : names) {
            if (name && EqualsNoCase(std::string(name), "ViktorEAugMissile")) {
                isViktorAugmentedRay = true;
                break;
            }
        }
        if (isViktorAugmentedRay && EqualsNoCase(data->spellName, "ViktorE")) {
            const SpellData* aftershock = ThreatDatabase::FindAny(
                "ViktorEAftershock", casterName.c_str());
            if (aftershock) {
                Threat delayed;
                delayed.data = aftershock;
                delayed.startPos = start;
                delayed.direction = direction;
                delayed.endPos = start + direction * std::max(1.0f, aftershock->range);
                delayed.authoredEndPos = delayed.endPos;
                delayed.startTick = launchTick;
                delayed.launchTick = 0;
                delayed.endTick = CalculateThreatEndTick(
                    *aftershock, delayed.startPos, delayed.endPos, delayed.startTick, 0);
                delayed.casterNetworkId = casterNetworkId;
                delayed.slot = static_cast<int>(ZDSpellSlot::E);
                delayed.logicalCastEpisodeId =
                    ResolveLogicalCastEpisode(
                        aftershock,
                        delayed.casterNetworkId,
                        delayed.slot,
                        delayed.startTick,
                        now,
                        delayed.startPos,
                        delayed.endPos,
                        delayed.direction);
                AddOrUpdateThreat(delayed);
            }
        }
        const std::uint32_t missileNetworkId = static_cast<std::uint32_t>(missile.NetworkId());
        const std::uintptr_t missileObjectIdentity = missile.Address();
        Vec2 observedHead = missile.Position().To2D();
        if (!HasUsablePosition(observedHead)) observedHead = start;
        Vec2 episodeDirection;
        bool matchedRecentEpisode = false;
        std::uint64_t projectileLaneKey = 0;
        const std::uint64_t logicalCastEpisodeId =
            ResolveLogicalCastEpisode(
                data,
                casterNetworkId,
                static_cast<int>(data->spellKey),
                WrappingTickAdd(
                    launchTick,
                    -std::max(0, data->spellDelay)),
                now,
                start,
                end,
                direction,
                &episodeDirection,
                true,
                &matchedRecentEpisode,
                &projectileLaneKey);
        const int projectileIndex =
            matchedRecentEpisode
            ? ResolveProjectileLaneIndex(
                *data,
                episodeDirection,
                direction)
            : -1;
        int changedId = -1;
        AcquireSRWLockExclusive(&storeLock);
        for (const auto& existing : threats) {
            if (missileNetworkId != 0 &&
                !existing.expired &&
                existing.missileBound &&
                MatchesMissileEpisode(
                    existing.missileNetworkId,
                    existing.missileObjectIdentity,
                    missileNetworkId,
                    missileObjectIdentity)) {
                ReleaseSRWLockExclusive(&storeLock);
                return;
            }
        }
        Threat* best = nullptr;
        float bestScore = -2.0f;
        for (auto& existing : threats) {
            if (existing.expired || existing.missileBound ||
                existing.projectileTerminated)
                continue;
            if (existing.casterNetworkId != casterNetworkId) continue;
            if (existing.SpellName() != data->spellName) continue;
            if (existing.logicalCastEpisodeId != 0 &&
                logicalCastEpisodeId != 0 &&
                existing.logicalCastEpisodeId !=
                    logicalCastEpisodeId)
                continue;
            const MissileBindKey castKey = {
                existing.casterNetworkId,
                existing.castIdentity,
                reinterpret_cast<std::uintptr_t>(existing.data),
                existing.direction,
                existing.startTick,
                existing.data ? existing.data->spellDelay : existing.Delay(),
                existing.Delay(),
                existing.slot,
                existing.startPos,
                existing.AuthoredEnd()
            };
            const MissileBindObservation observation = {
                casterNetworkId,
                args.CastIdentity,
                reinterpret_cast<std::uintptr_t>(data),
                direction,
                now,
                static_cast<int>(data->spellKey),
                start,
                end
            };
            const float score = MissileBindScore(castKey, observation);
            if (!std::isfinite(score)) continue;
            if (score > bestScore) {
                bestScore = score;
                best = &existing;
            }
        }
        if (best) {
            CorrectExistingThreatFromMissile(
                *best,
                [&](Threat& bound) {
                    bound.data = data;
                    bound.startPos = start;
                    bound.authoredEndPos = end;
                    bound.endPos = end;
                    bound.direction = direction;
                    bound.launchTick = launchTick;
                    bound.endTick = CalculateThreatEndTick(
                        *bound.data, start, end, bound.startTick, launchTick);
                    bound.missileNetworkId = missileNetworkId;
                    bound.missileObjectIdentity = missileObjectIdentity;
                    bound.observedHead = observedHead;
                    bound.observedTick = now;
                    bound.observedSpeed = data->projectileSpeed;
                    bound.positionUncertainty = 0.0f;
                    if (bound.castIdentity == 0)
                        bound.castIdentity = args.CastIdentity;
                    if (bound.logicalCastEpisodeId == 0)
                        bound.logicalCastEpisodeId =
                            logicalCastEpisodeId;
                    if (bound.projectileLaneKey == 0)
                        bound.projectileLaneKey =
                            projectileLaneKey;
                    if (bound.projectileIndex < 0)
                        bound.projectileIndex =
                            projectileIndex;
                    ++bound.revision;
                });
            changedId = best->id;
        } else {
            Threat threat;
            threat.id = nextThreatId++;
            threat.data = data;
            threat.startPos = start;
            threat.authoredEndPos = end;
            threat.endPos = end;
            threat.direction = direction;
            threat.startTick = SaturatingTickAdd(
                launchTick, -std::max(0, data->spellDelay));
            threat.launchTick = launchTick;
            threat.endTick = CalculateThreatEndTick(
                *threat.data, start, end, threat.startTick, launchTick);
            threat.castIdentity = args.CastIdentity;
            threat.logicalCastEpisodeId =
                logicalCastEpisodeId;
            threat.projectileLaneKey =
                projectileLaneKey;
            threat.casterNetworkId = casterNetworkId;
            threat.slot = static_cast<int>(data->spellKey);
            threat.projectileIndex = projectileIndex;
            threat.missileNetworkId = missileNetworkId;
            threat.missileObjectIdentity = missileObjectIdentity;
            threat.observedHead = observedHead;
            threat.observedTick = now;
            threat.observedSpeed = data->projectileSpeed;
            threat.positionUncertainty = 0.0f;
            threat.missileBound = true;
            changedId = threat.id;
            threats.push_back(std::move(threat));
        }
        ReleaseSRWLockExclusive(&storeLock);
        if (changedId >= 0) MarkChanged(changedId);
    }

    static bool HasCollisionObject(const SpellData& data, ZDCollisionObjectType type) {
        return std::find(data.collisionObjects.begin(), data.collisionObjects.end(), type) !=
               data.collisionObjects.end();
    }

    static Vec2 ProjectOnSegment(const Vec2& point,
                                 const Vec2& start,
                                 const Vec2& end,
                                 bool* onSegment = nullptr) {
        const Vec2 delta = end - start;
        const float lengthSqr = delta.LengthSqr();
        if (lengthSqr <= 0.0001f) {
            if (onSegment) *onSegment = false;
            return start;
        }
        const float raw = (point - start).Dot(delta) / lengthSqr;
        if (onSegment) *onSegment = raw >= 0.0f && raw <= 1.0f;
        return start + delta * std::clamp(raw, 0.0f, 1.0f);
    }

    static Vec2 PositionAfter(const std::vector<Vec2>& path, float distance) {
        if (path.empty()) return {};
        if (path.size() == 1 || distance <= 0.0f) return path.front();
        for (std::size_t i = 1; i < path.size(); ++i) {
            const float length = path[i - 1].Distance(path[i]);
            if (length <= 0.001f) continue;
            if (distance <= length)
                return path[i - 1] + (path[i] - path[i - 1]) * (distance / length);
            distance -= length;
        }
        return path.back();
    }

    static bool SegmentIntersection(const Vec2& firstStart,
                                    const Vec2& firstEnd,
                                    const Vec2& secondStart,
                                    const Vec2& secondEnd,
                                    Vec2& point) {
        const Vec2 first = firstEnd - firstStart;
        const Vec2 second = secondEnd - secondStart;
        const float cross = first.x * second.y - first.y * second.x;
        if (std::abs(cross) <= 0.0001f) return false;
        const Vec2 offset = secondStart - firstStart;
        const float firstParameter = (offset.x * second.y - offset.y * second.x) / cross;
        const float secondParameter = (offset.x * first.y - offset.y * first.x) / cross;
        if (firstParameter < 0.0f || firstParameter > 1.0f ||
            secondParameter < 0.0f || secondParameter > 1.0f) return false;
        point = firstStart + first * firstParameter;
        return HasUsablePosition(point);
    }

    static bool FirstTerrainCollision(const Vec2& start,
                                      const Vec2& end,
                                      float height,
                                      float radius,
                                      Vec2& point) {
        point = {};
        const Vec2 direction = (end - start).Normalized();
        const float length = start.Distance(end);
        if (direction.IsZero() || length <= 1.0f) return false;
        const Vec2 normal(-direction.y, direction.x);
        const int samples = std::max(1, static_cast<int>(std::ceil(length / 18.0f)));
        const auto blocked = [&](const Vec2& center) {
            const float clearance = std::max(0.0f, radius);
            const Vec2 probes[] = {
                center,
                center + normal * clearance,
                center - normal * clearance,
                center + direction * clearance
            };
            for (const Vec2& probe : probes) {
                const Vec3 world = Vec3::From2D(probe, height);
                if (CoreNavGrid::IsWall(world) || CoreNavGrid::IsBuilding(world)) return true;
            }
            return false;
        };
        float previous = 0.0f;
        for (int index = 1; index <= samples; ++index) {
            const float distance = length * static_cast<float>(index) /
                                   static_cast<float>(samples);
            if (!blocked(start + direction * distance)) {
                previous = distance;
                continue;
            }
            float low = previous;
            float high = distance;
            for (int iteration = 0; iteration < 6; ++iteration) {
                const float middle = (low + high) * 0.5f;
                if (blocked(start + direction * middle)) high = middle;
                else low = middle;
            }
            point = start + direction * high;
            return true;
        }
        return false;
    }

    static void AddUnitCollision(const SDK::AIBaseClient& unit,
                                 const Threat& threat,
                                 const Vec2& from,
                                 const Vec2& authoredEnd,
                                 const Vec2& routeDirection,
                                 float projectileRadius,
                                 std::unordered_set<int>& visited,
                                 std::vector<CollisionEvent>& events,
                                 std::vector<std::pair<int, float>>& pending) {
        if (!unit.IsValid() || unit.IsDead() || unit.NetworkId() == 0 ||
            unit.NetworkId() == static_cast<int>(threat.casterNetworkId) ||
            !visited.insert(unit.NetworkId()).second ||
            std::find(threat.consumedCollisionUnits.begin(),
                      threat.consumedCollisionUnits.end(), unit.NetworkId()) !=
                threat.consumedCollisionUnits.end()) return;

        Vec2 position = unit.ServerPosition().To2D();
        const auto rawWaypoints = unit.GetWaypoints();
        if (rawWaypoints.size() >= 2 && unit.MoveSpeed() > 0.0f && threat.Speed() > 1.0f) {
            std::vector<Vec2> waypoints;
            waypoints.reserve(rawWaypoints.size());
            for (const Vec3& waypoint : rawWaypoints) waypoints.push_back(waypoint.To2D());
            for (int iteration = 0; iteration < 2; ++iteration) {
                const Vec2 projected = ProjectOnSegment(position, from, authoredEnd);
                const float travelSeconds = from.Distance(projected) /
                                            std::max(1.0f, threat.Speed());
                position = PositionAfter(waypoints, unit.MoveSpeed() * travelSeconds);
            }
        }
        if (!HasUsablePosition(position)) return;

        bool onSegment = false;
        const Vec2 projected = ProjectOnSegment(position, from, authoredEnd, &onSegment);
        if (!onSegment) return;
        const float combinedRadius = projectileRadius +
            std::max(0.0f, unit.BoundingRadius() - 10.0f);
        const float lateral = projected.Distance(position);
        if (lateral > combinedRadius) return;
        const float centerDistance = from.Distance(projected);
        const float entryOffset = std::sqrt(std::max(
            0.0f, combinedRadius * combinedRadius - lateral * lateral));
        const float contactDistance = std::clamp(
            centerDistance - entryOffset, 0.0f, from.Distance(authoredEnd));
        const Vec2 contact = from + routeDirection * contactDistance;
        events.push_back({
            contactDistance,
            contact,
            position,
            ZDCollisionKind::Unit,
            unit.NetworkId(),
            unit.Address()
        });
        const Vec2 fullDirection = (authoredEnd - threat.startPos).Normalized();
        pending.emplace_back(unit.NetworkId(), std::clamp(
            (contact - threat.startPos).Dot(fullDirection),
            0.0f, threat.startPos.Distance(authoredEnd)));
    }

    static bool ResolveCollision(Threat& threat,
                                 int now,
                                 bool& geometryChanged) {
        geometryChanged = false;
        const auto clearPredictionAndReturn = [&]() {
            geometryChanged =
                ClearPredictedCollisionMetadata(threat);
            return geometryChanged;
        };
        if (!threat.data || threat.persistent || threat.projectileTerminated ||
            threat.Type() != ZDSpellType::Line ||
            threat.data->collisionObjects.empty())
            return clearPredictionAndReturn();
        const bool liveBoundPrediction =
            !ShouldCommitPredictedCollision(
                threat.missileBound,
                threat.projectileTerminated);
        if (liveBoundPrediction &&
            !CanRefreshLiveBoundCollisionPrediction(
                threat,
                now))
            return clearPredictionAndReturn();
        const Vec2 authoredEnd = threat.AuthoredEnd();
        const Vec2 fullDirection = (authoredEnd - threat.startPos).Normalized();
        const Vec2 from = threat.missileBound && HasUsablePosition(threat.observedHead)
            ? threat.observedHead
            : threat.startPos;
        const Vec2 routeDirection = (authoredEnd - from).Normalized();
        if (!HasUsablePosition(from) || !HasUsablePosition(authoredEnd) ||
            fullDirection.IsZero() || routeDirection.IsZero())
            return clearPredictionAndReturn();

        const int targetLimit = threat.CollisionTargetLimit();
        const float currentProgress = std::clamp(
            (ProjectOnSegment(from, threat.startPos, authoredEnd) - threat.startPos)
                .Dot(fullDirection),
            0.0f, threat.startPos.Distance(authoredEnd));
        if (!liveBoundPrediction && targetLimit > 1) {
            for (const auto& pending : threat.pendingUnitCollisions) {
                if (pending.second > currentProgress + 6.0f) continue;
                if (std::find(threat.consumedCollisionUnits.begin(),
                              threat.consumedCollisionUnits.end(), pending.first) ==
                    threat.consumedCollisionUnits.end()) {
                    threat.consumedCollisionUnits.push_back(pending.first);
                    threat.lastConsumedCollisionPoint = threat.startPos +
                        fullDirection * pending.second;
                }
            }
        }

        std::vector<CollisionEvent> events;
        std::vector<std::pair<int, float>> pending;
        std::unordered_set<int> visited;
        // Collision prediction models the authored projectile body. The
        // global evade safety padding belongs only to danger geometry.
        const float projectileRadius =
            std::max(0.0f, threat.ProjectileCollisionRadius());
        if (HasCollisionObject(*threat.data, ZDCollisionObjectType::EnemyMinions)) {
            for (const auto& minion : SDK::GameObjects::AllyMinions())
                AddUnitCollision(SDK::AIBaseClient(minion.Handle()), threat, from,
                                 authoredEnd, routeDirection, projectileRadius,
                                 visited, events, pending);
            for (const auto& minion : SDK::GameObjects::Jungle())
                AddUnitCollision(SDK::AIBaseClient(minion.Handle()), threat, from,
                                 authoredEnd, routeDirection, projectileRadius,
                                 visited, events, pending);
        }
        if (HasCollisionObject(*threat.data, ZDCollisionObjectType::EnemyChampions)) {
            const auto player = SDK::ObjectManager::Player();
            if (player.IsValid())
                AddUnitCollision(
                    SDK::AIBaseClient(player.Handle()),
                    threat,
                    from,
                    authoredEnd,
                    routeDirection,
                    projectileRadius,
                    visited,
                    events,
                    pending);
            for (const auto& hero : SDK::GameObjects::AllyHeroes()) {
                const SDK::AIBaseClient unit(hero.Handle());
                AddUnitCollision(unit, threat, from, authoredEnd, routeDirection,
                                 projectileRadius, visited, events, pending);
            }
        }

        if (HasCollisionObject(*threat.data, ZDCollisionObjectType::Terrain)) {
            float height = 0.0f;
            const auto caster = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
                static_cast<int>(threat.casterNetworkId));
            if (caster.IsValid()) height = caster.ServerPosition().y;
            else if (SDK::ObjectManager::Player().IsValid())
                height = SDK::ObjectManager::Player().ServerPosition().y;
            Vec2 point;
            if (FirstTerrainCollision(from, authoredEnd, height, projectileRadius, point))
                events.push_back({from.Distance(point), point, {},
                                  ZDCollisionKind::Terrain, 0});
        }

        if (HasCollisionObject(*threat.data, ZDCollisionObjectType::EnemyYasuoWall)) {
            const auto caster = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
                static_cast<int>(threat.casterNetworkId));
            for (const auto& wall : SDK::YasuoWallTracker::ActiveWalls()) {
                const SDK::GameObject wallObject(wall.main);
                if (caster.IsValid() && wallObject.IsValid() &&
                    caster.Team() == wallObject.Team()) continue;
                const Vec2 wallStart = wall.start.To2D();
                const Vec2 wallEnd = wall.end.To2D();
                Vec2 point;
                float gap = FLT_MAX;
                if (SegmentIntersection(from, authoredEnd, wallStart, wallEnd, point)) {
                    gap = 0.0f;
                } else {
                    const Vec2 candidates[] = {
                        ProjectOnSegment(wallStart, from, authoredEnd),
                        ProjectOnSegment(wallEnd, from, authoredEnd),
                        from,
                        authoredEnd
                    };
                    for (const Vec2& candidate : candidates) {
                        const float candidateGap = candidate.Distance(
                            ProjectOnSegment(candidate, wallStart, wallEnd));
                        if (candidateGap < gap) {
                            gap = candidateGap;
                            point = candidate;
                        }
                    }
                }
                if (gap <= projectileRadius + 75.0f && HasUsablePosition(point))
                    events.push_back({from.Distance(point), point, {},
                                      ZDCollisionKind::ProjectileWall, 0});
            }
        }

        std::sort(events.begin(), events.end(),
            [](const CollisionEvent& left, const CollisionEvent& right) {
                if (left.distance != right.distance) return left.distance < right.distance;
                const bool leftTerminal = left.kind != ZDCollisionKind::Unit;
                const bool rightTerminal = right.kind != ZDCollisionKind::Unit;
                if (leftTerminal != rightTerminal) return leftTerminal;
                return left.networkId < right.networkId;
            });
        std::sort(pending.begin(), pending.end(),
            [](const auto& left, const auto& right) { return left.second < right.second; });
        if (!liveBoundPrediction)
            threat.pendingUnitCollisions = std::move(pending);

        CollisionEvent stop;
        bool stopped = false;
        int unitHits = !liveBoundPrediction && targetLimit > 1
            ? static_cast<int>(threat.consumedCollisionUnits.size())
            : 0;
        if (!liveBoundPrediction &&
            unitHits >= targetLimit &&
            !threat.lastConsumedCollisionPoint.IsZero()) {
            stop = {threat.startPos.Distance(threat.lastConsumedCollisionPoint),
                    threat.lastConsumedCollisionPoint, {}, ZDCollisionKind::Unit, 0};
            stopped = true;
        } else {
            for (const auto& event : events) {
                if (event.kind == ZDCollisionKind::Unit) {
                    ++unitHits;
                    if (unitHits < targetLimit) continue;
                }
                stop = event;
                stopped = true;
                break;
            }
        }

        if (liveBoundPrediction) {
            geometryChanged =
                RefreshLiveBoundCollisionPrediction(
                    threat,
                    stopped ? stop.kind : ZDCollisionKind::None,
                    stopped ? stop.networkId : 0,
                    stopped ? stop.unitCenter : Vec2(),
                    stopped ? stop.point : Vec2(),
                    now,
                    stopped ? stop.unitObjectIdentity : 0);
            return true;
        }

        const bool predictionCleared =
            ClearPredictedCollisionMetadata(threat);
        const Vec2 nextEnd = stopped ? stop.point : authoredEnd;
        const ZDCollisionKind nextKind = stopped ? stop.kind : ZDCollisionKind::None;
        const Vec2 nextUnitCenter = stopped && stop.kind == ZDCollisionKind::Unit
            ? stop.unitCenter
            : Vec2();
        const int nextUnitNetworkId = stopped && stop.kind == ZDCollisionKind::Unit
            ? stop.networkId
            : 0;
        geometryChanged = predictionCleared ||
            threat.endPos.DistanceSqr(nextEnd) > 1.0f ||
            threat.collisionKind != nextKind ||
            threat.collisionUnitCenter.DistanceSqr(nextUnitCenter) > 1.0f ||
            threat.collisionUnitNetworkId != nextUnitNetworkId ||
            threat.collisionStopped != stopped ||
            threat.collisionHitCount != unitHits;
        threat.endPos = nextEnd;
        threat.collisionKind = nextKind;
        threat.collisionUnitCenter = nextUnitCenter;
        threat.collisionUnitNetworkId = nextUnitNetworkId;
        threat.collisionUnitObjectIdentity = 0;
        threat.collisionStopped = stopped;
        threat.collisionUnitTargetAuthoritative = false;
        threat.collisionHitCount = unitHits;
        if (!stopped || stop.kind != ZDCollisionKind::Unit) {
            threat.collisionExplosionCenter = {};
            threat.collisionEndExplosionRadius = 0.0f;
            threat.collisionEndExplosionDelay = -1;
        }
        if (!threat.missileBound)
            threat.endTick = CalculateThreatEndTick(
                *threat.data,
                threat.startPos,
                threat.endPos,
                threat.startTick,
                threat.launchTick);
        return true;
    }

    static void UpdateCollisions(int now) {
        if (WrappingTickDifference(
                now,
                lastCollisionTick.load(std::memory_order_acquire)) < 50) return;
        lastCollisionTick.store(now, std::memory_order_release);
        std::vector<Threat> snapshots;
        AcquireSRWLockShared(&storeLock);
        snapshots = threats;
        ReleaseSRWLockShared(&storeLock);

        std::vector<int> changedIds;
        for (auto& snapshot : snapshots) {
            const int revision = snapshot.revision;
            bool geometryChanged = false;
            if (!ResolveCollision(snapshot, now, geometryChanged)) continue;
            AcquireSRWLockExclusive(&storeLock);
            const auto found = std::find_if(threats.begin(), threats.end(),
                [id = snapshot.id](const Threat& threat) { return threat.id == id; });
            if (found != threats.end() && !found->expired && found->revision == revision) {
                if (geometryChanged) ++snapshot.revision;
                *found = std::move(snapshot);
                if (geometryChanged) changedIds.push_back(found->id);
            }
            ReleaseSRWLockExclusive(&storeLock);
        }
        for (int id : changedIds) MarkChanged(id);
    }

    static void UpdateRetainedExplosions(int now) {
        std::vector<Threat> snapshots;
        AcquireSRWLockShared(&storeLock);
        for (const auto& threat : threats) {
            if (!threat.expired && threat.projectileTerminated && threat.data &&
                threat.collisionUnitNetworkId != 0 &&
                threat.collisionUnitTargetAuthoritative &&
                (threat.data->endExplosionFollowsUnit ||
                 threat.data->endExplosionDetonatesOnUnitDeath))
                snapshots.push_back(threat);
        }
        ReleaseSRWLockShared(&storeLock);

        std::vector<int> changedIds;
        for (const auto& snapshot : snapshots) {
            const auto target = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
                snapshot.collisionUnitNetworkId);
            if (!target.IsValid() ||
                !MatchesAttachedUnitIdentity(
                    snapshot.collisionUnitTargetAuthoritative,
                    snapshot.collisionUnitObjectIdentity,
                    target.Address()))
                continue;
            const Vec2 center = target.ServerPosition().To2D();
            AcquireSRWLockExclusive(&storeLock);
            const auto found = std::find_if(threats.begin(), threats.end(),
                [id = snapshot.id](const Threat& threat) { return threat.id == id; });
            if (found != threats.end() && !found->expired &&
                found->revision == snapshot.revision) {
                const bool changed =
                    UpdateAttachedUnitExplosion(
                        *found,
                        center,
                        target.IsDead(),
                        now);
                if (changed) {
                    ++found->revision;
                    changedIds.push_back(found->id);
                }
            }
            ReleaseSRWLockExclusive(&storeLock);
        }
        for (int id : changedIds) MarkChanged(id);
    }

    static std::size_t ActiveMissileEpisodeOwnerCountLocked(
            std::uint32_t missileNetworkId) {
        return static_cast<std::size_t>(std::count_if(
            threats.begin(),
            threats.end(),
            [missileNetworkId](const Threat& threat) {
                return !threat.expired &&
                    UsesMissileLifecycle(
                        threat.Type(),
                        threat.missileBound) &&
                    !threat.projectileTerminated &&
                    threat.missileNetworkId == missileNetworkId;
            }));
    }

    static void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
        if (!initialized) return;
        const std::uint32_t deletedId = args.MissileNetworkId != 0
            ? args.MissileNetworkId
            : args.Sender.NetworkId;
        if (deletedId == 0) return;
        const std::uintptr_t deletedObjectIdentity = args.Sender.Ptr;
        const int now = SDK::Variables::TickCount();
        std::vector<Threat> snapshots;
        AcquireSRWLockShared(&storeLock);
        const std::size_t activeNetworkIdOwnerCount =
            ActiveMissileEpisodeOwnerCountLocked(deletedId);
        if (!ShouldAcceptMissileDeleteOwnership(
                deletedObjectIdentity,
                activeNetworkIdOwnerCount)) {
            ReleaseSRWLockShared(&storeLock);
            return;
        }
        for (const auto& threat : threats) {
            if (UsesMissileLifecycle(
                    threat.Type(), threat.missileBound) &&
                ShouldFinalizeMissileDelete(
                    threat.missileBound,
                    threat.projectileTerminated,
                    threat.missileNetworkId,
                    threat.missileObjectIdentity,
                    deletedId,
                    deletedObjectIdentity,
                    activeNetworkIdOwnerCount))
                snapshots.push_back(threat);
        }
        ReleaseSRWLockShared(&storeLock);

        std::vector<int> changedIds;
        for (auto& snapshot : snapshots) {
            const int revision = snapshot.revision;
            Vec2 impact = args.Sender.Position.To2D();
            const bool hasAuthoritativeDeleteImpact =
                HasUsablePosition(impact);
            if (!HasUsablePosition(impact)) impact = snapshot.observedHead;
            if (!HasUsablePosition(impact)) impact = snapshot.endPos;
            const Vec2 intendedEnd = snapshot.AuthoredEnd();
            const ZDCollisionKind predictedCollisionKind =
                snapshot.predictedCollisionKind;
            const Vec2 predictedUnitImpact =
                snapshot.predictedCollisionPoint;
            const int predictedUnitNetworkId =
                snapshot.predictedCollisionUnitNetworkId;
            const std::uintptr_t predictedUnitObjectIdentity =
                snapshot.predictedCollisionUnitObjectIdentity;
            const int predictedCollisionTick =
                snapshot.predictedCollisionTick;
            const bool predictionMatchesMissileEpisode =
                MatchesMissileEpisode(
                    snapshot.predictedCollisionMissileNetworkId,
                    snapshot.predictedCollisionMissileObjectIdentity,
                    deletedId,
                    deletedObjectIdentity);
            const bool allowsUnitCollision =
                snapshot.data &&
                (HasCollisionObject(
                     *snapshot.data,
                     ZDCollisionObjectType::EnemyChampions) ||
                 HasCollisionObject(
                     *snapshot.data,
                     ZDCollisionObjectType::EnemyMinions));
            const bool terminatedBeforeEnd = HasUsablePosition(impact) &&
                HasUsablePosition(intendedEnd) &&
                impact.Distance(intendedEnd) >
                    std::max(
                        90.0f,
                        snapshot.AuthoredRadius() * 0.5f);
            if (HasUsablePosition(impact)) snapshot.endPos = impact;

            snapshot.collisionKind = ZDCollisionKind::None;
            snapshot.collisionStopped = false;
            snapshot.collisionHitCount = 0;
            snapshot.collisionUnitNetworkId = 0;
            snapshot.collisionUnitObjectIdentity = 0;
            snapshot.collisionUnitCenter = {};
            snapshot.collisionExplosionCenter = {};
            snapshot.collisionEndExplosionRadius = 0.0f;
            snapshot.collisionEndExplosionDelay = -1;
            snapshot.collisionUnitTargetAuthoritative = false;
            SDK::AIBaseClient collisionTarget;
            const bool explicitTarget =
                args.TargetNetworkId != 0 || args.Target.IsValid();
            const bool predictedUnitMatch =
                ShouldClassifyDeleteAsUnitCollision(
                    explicitTarget,
                    predictedCollisionKind,
                    predictedUnitNetworkId,
                    predictedUnitImpact,
                    predictedCollisionTick,
                    now,
                    predictionMatchesMissileEpisode,
                    allowsUnitCollision,
                    hasAuthoritativeDeleteImpact
                        ? impact
                        : Vec2());
            ClearPredictedCollisionMetadata(snapshot);
            if (args.TargetNetworkId != 0)
                collisionTarget = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
                    static_cast<int>(args.TargetNetworkId));
            if (!collisionTarget.IsValid() && args.Target.IsValid())
                collisionTarget = SDK::AIBaseClient(args.Target.Ptr, args.Target.Type);
            const int explicitMetadataUnitNetworkId =
                args.TargetNetworkId != 0
                ? static_cast<int>(args.TargetNetworkId)
                : static_cast<int>(args.Target.NetworkId);
            const std::uintptr_t explicitMetadataUnitObjectIdentity =
                args.Target.Ptr;
            const Vec2 explicitUnitCenter =
                collisionTarget.IsValid()
                ? collisionTarget.ServerPosition().To2D()
                : Vec2();
            const bool explicitUnitAttached =
                collisionTarget.IsValid() &&
                ShouldAttachExplicitUnitAtDelete(
                    explicitTarget,
                    explicitMetadataUnitNetworkId,
                    explicitMetadataUnitObjectIdentity,
                    collisionTarget.NetworkId(),
                    collisionTarget.Address(),
                    explicitUnitCenter);
            Vec2 attachedUnitCenter;
            bool targetlessUnitAttached = false;
            if (!explicitTarget &&
                predictedUnitMatch &&
                predictedUnitNetworkId != 0) {
                collisionTarget =
                    SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
                        predictedUnitNetworkId);
                attachedUnitCenter = collisionTarget.IsValid()
                    ? collisionTarget.ServerPosition().To2D()
                    : Vec2();
                const bool resolvedUnitAllowed =
                    collisionTarget.IsValid() &&
                    snapshot.data &&
                    ((collisionTarget.IsHero() &&
                      HasCollisionObject(
                          *snapshot.data,
                          ZDCollisionObjectType::EnemyChampions)) ||
                     (collisionTarget.IsMinion() &&
                      HasCollisionObject(
                          *snapshot.data,
                          ZDCollisionObjectType::EnemyMinions)));
                const bool supportsAttachedUnitLifecycle =
                    snapshot.data &&
                    (snapshot.data->endExplosionFollowsUnit ||
                     snapshot.data->endExplosionDetonatesOnUnitDeath);
                targetlessUnitAttached =
                    ShouldAttachPredictedUnitAtDelete(
                        predictedUnitNetworkId,
                        collisionTarget.IsValid()
                            ? collisionTarget.NetworkId()
                            : 0,
                        predictedUnitObjectIdentity,
                        collisionTarget.IsValid()
                            ? collisionTarget.Address()
                            : 0,
                        collisionTarget.IsValid(),
                        collisionTarget.IsValid() &&
                            collisionTarget.IsDead(),
                        resolvedUnitAllowed,
                        supportsAttachedUnitLifecycle,
                        attachedUnitCenter,
                        impact);
            }
            if (predictedUnitMatch) {
                const int targetId = explicitTarget &&
                        collisionTarget.IsValid()
                    ? collisionTarget.NetworkId()
                    : args.TargetNetworkId != 0
                        ? static_cast<int>(args.TargetNetworkId)
                        : args.Target.NetworkId != 0
                            ? static_cast<int>(args.Target.NetworkId)
                            : predictedUnitNetworkId;
                ConfirmDeleteUnitCollision(
                    snapshot,
                    targetId,
                    impact,
                    explicitUnitAttached ||
                        targetlessUnitAttached,
                    explicitUnitAttached
                        ? explicitUnitCenter
                        : targetlessUnitAttached
                        ? attachedUnitCenter
                        : Vec2(),
                    explicitUnitAttached
                        ? collisionTarget.Address()
                        : targetlessUnitAttached
                            ? collisionTarget.Address()
                            : 0);
                if (snapshot.data && snapshot.data->endExplosionDetonatesOnUnitDeath &&
                    explicitUnitAttached &&
                    collisionTarget.IsDead())
                    snapshot.collisionEndExplosionDelay = 0;
            } else if (snapshot.data && HasUsablePosition(impact)) {
                const Vec3 world = Vec3::From2D(impact, args.Sender.Position.y);
                if (HasCollisionObject(*snapshot.data, ZDCollisionObjectType::Terrain) &&
                    (CoreNavGrid::IsWall(world) || CoreNavGrid::IsBuilding(world))) {
                    snapshot.collisionKind = ZDCollisionKind::Terrain;
                    snapshot.collisionStopped = true;
                } else if (HasCollisionObject(
                               *snapshot.data, ZDCollisionObjectType::EnemyYasuoWall)) {
                    for (const auto& wall : SDK::YasuoWallTracker::ActiveWalls()) {
                        if (impact.Distance(ProjectOnSegment(
                                impact, wall.start.To2D(), wall.end.To2D())) <=
                            snapshot.AuthoredRadius() + 100.0f) {
                            snapshot.collisionKind = ZDCollisionKind::ProjectileWall;
                            snapshot.collisionStopped = true;
                            break;
                        }
                    }
                }
            }

            snapshot.projectileTerminated = true;
            snapshot.projectileTerminationTick = now;
            snapshot.missileMissingSinceTick = -1;
            snapshot.missilePositionUnavailable = false;
            snapshot.missileBound = false;
            const bool hasEndExplosion =
                snapshot.HasEndExplosionArea();
            const bool retain = RetainProjectileTermination(
                snapshot.Type(),
                snapshot.ExtraEndTime(),
                hasEndExplosion);
            snapshot.expired = !retain;
            if (retain) {
                const int linger = ThreatLifecycleLinger(
                    snapshot.ExtraEndTime(),
                    hasEndExplosion,
                    snapshot.EndExplosionDelay(),
                    snapshot.EndExplosionDuration());
                snapshot.endTick = SaturatingTickAdd(now, linger);
            }
            const bool cancelDependents = EqualsNoCase(snapshot.SpellName(), "ZiggsQ") &&
                (terminatedBeforeEnd ||
                 snapshot.collisionKind == ZDCollisionKind::ProjectileWall);

            AcquireSRWLockExclusive(&storeLock);
            const auto found = std::find_if(threats.begin(), threats.end(),
                [id = snapshot.id](const Threat& threat) { return threat.id == id; });
            const std::size_t finalActiveNetworkIdOwnerCount =
                ActiveMissileEpisodeOwnerCountLocked(deletedId);
            if (found != threats.end() &&
                ShouldFinalizeMissileDelete(
                    found->missileBound,
                    found->projectileTerminated,
                    found->missileNetworkId,
                    found->missileObjectIdentity,
                    deletedId,
                    deletedObjectIdentity,
                    finalActiveNetworkIdOwnerCount)) {
                snapshot.revision = std::max(revision, found->revision) + 1;
                *found = std::move(snapshot);
                changedIds.push_back(found->id);
                if (cancelDependents) {
                    for (auto& dependent : threats) {
                        if (dependent.expired ||
                            dependent.casterNetworkId != found->casterNetworkId ||
                            std::llabs(TickDifference(
                                dependent.startTick, found->startTick)) > 5000 ||
                            (!EqualsNoCase(dependent.SpellName(), "ZiggsQBounce1") &&
                             !EqualsNoCase(dependent.SpellName(), "ZiggsQBounce2"))) continue;
                        dependent.expired = true;
                        ++dependent.revision;
                        changedIds.push_back(dependent.id);
                    }
                }
            }
            ReleaseSRWLockExclusive(&storeLock);
        }
        for (int id : changedIds) MarkChanged(id);
    }

    static void RefreshMissileObservations(int now) {
        struct TrackedMissile {
            int threatId = -1;
            std::uint32_t networkId = 0;
            std::uintptr_t objectIdentity = 0;
        };
        std::vector<TrackedMissile> tracked;
        AcquireSRWLockShared(&storeLock);
        tracked.reserve(threats.size());
        for (const auto& threat : threats) {
            if (!threat.expired &&
                UsesMissileLifecycle(
                    threat.Type(), threat.missileBound) &&
                threat.missileNetworkId != 0)
                tracked.push_back({
                    threat.id,
                    threat.missileNetworkId,
                    threat.missileObjectIdentity
                });
        }
        ReleaseSRWLockShared(&storeLock);
        if (!ShouldEnumerateMissileManager(tracked.size())) return;

        std::vector<TrackedMissile> observedEpisodes;
        observedEpisodes.reserve(tracked.size());
        std::vector<MissileObservation> observations;
        observations.reserve(tracked.size());
        // Enumerate the Core missile manager directly. Constructing the known
        // manager entries as MissileClient avoids generic type-cache filtering.
        std::uintptr_t missileEntries[8192] = {};
        const int missileCount =
            ::Core::ObjectManager::EnumerateMissiles(
                missileEntries,
                static_cast<int>(std::size(missileEntries)));
        for (int missileIndex = 0;
             missileIndex < missileCount;
             ++missileIndex) {
            const SDK::MissileClient missile(
                missileEntries[missileIndex]);
            if (!missile.IsValid()) continue;
            const std::uint32_t networkId =
                static_cast<std::uint32_t>(missile.NetworkId());
            const std::uintptr_t objectIdentity = missile.Address();
            for (const auto& trackedEntry : tracked) {
                if (!MatchesObservedMissileEpisode(
                        trackedEntry.networkId,
                        trackedEntry.objectIdentity,
                        networkId,
                        objectIdentity))
                    continue;
                observedEpisodes.push_back(trackedEntry);
                const Vec2 position = missile.Position().To2D();
                if (!HasUsablePosition(position)) continue;
                observations.push_back({
                    trackedEntry.threatId,
                    networkId,
                    objectIdentity,
                    position,
                    missile.EndPosition().To2D(),
                    now
                });
            }
        }

        std::vector<int> changedIds;
        AcquireSRWLockExclusive(&storeLock);
        for (auto& threat : threats) {
            if (threat.expired ||
                !UsesMissileLifecycle(
                    threat.Type(), threat.missileBound) ||
                threat.missileNetworkId == 0)
                continue;
            const bool missileObserved = std::any_of(
                observedEpisodes.begin(),
                observedEpisodes.end(),
                [&](const TrackedMissile& observed) {
                    return observed.threatId == threat.id &&
                        MatchesObservedMissileEpisode(
                            threat.missileNetworkId,
                            threat.missileObjectIdentity,
                            observed.networkId,
                            observed.objectIdentity);
                });
            const bool positionUsable = std::any_of(
                observations.begin(),
                observations.end(),
                [&](const MissileObservation& observation) {
                    return observation.threatId == threat.id &&
                        MatchesObservedMissileEpisode(
                            threat.missileNetworkId,
                            threat.missileObjectIdentity,
                            observation.networkId,
                            observation.objectIdentity);
                });
            const bool startedMissing =
                !missileObserved &&
                threat.missileMissingSinceTick < 0;
            const MissileEvidenceStateUpdate evidence =
                ResolveMissileEvidenceState(
                    missileObserved,
                    positionUsable,
                    threat.missileMissingSinceTick,
                    threat.missilePositionUnavailable,
                    now);
            threat.missileMissingSinceTick =
                evidence.missingSinceTick;
            threat.missilePositionUnavailable =
                evidence.positionUnavailable;
            if (evidence.stateChanged) {
                ++threat.revision;
                changedIds.push_back(threat.id);
            }
            if (missileObserved)
                continue;
            if (startedMissing)
                continue;
            const int evidenceAnchorTick = threat.observedTick != 0
                ? threat.observedTick
                : threat.launchTick;
            const MissileEvidenceWindow evidenceWindow =
                ResolveMissileEvidenceWindow(
                    evidenceAnchorTick,
                    threat.RemainingTravelDurationMs());
            if (!ShouldTerminateMissingMissile(
                    threat.missileBound,
                    threat.projectileTerminated,
                    false,
                    threat.missileMissingSinceTick,
                    evidenceWindow.deadlineTick,
                    now))
                continue;

            const int terminationTick =
                evidenceWindow.terminationTick;
            threat.endPos = TerminatedProjectileEnd(
                threat.Type(),
                threat.missileBound,
                threat.endPos,
                threat.observedHead);
            threat.projectileTerminated = true;
            threat.missingMissileTermination = true;
            threat.projectileTerminationTick = terminationTick;
            threat.missileMissingSinceTick = -1;
            threat.missilePositionUnavailable = false;
            threat.missileBound = false;
            // Evidence loss is not collision evidence. Retain only terminal
            // hazards whose database configuration does not require impact.
            threat.collisionKind = ZDCollisionKind::None;
            threat.collisionStopped = false;
            threat.collisionHitCount = 0;
            threat.collisionUnitNetworkId = 0;
            threat.collisionUnitObjectIdentity = 0;
            threat.collisionUnitCenter = {};
            threat.collisionExplosionCenter = {};
            threat.collisionUnitTargetAuthoritative = false;
            threat.collisionEndExplosionRadius = 0.0f;
            threat.collisionEndExplosionDelay = -1;
            threat.pendingUnitCollisions.clear();
            threat.consumedCollisionUnits.clear();
            threat.ClearPredictedCollision();
            const bool hasEndExplosion =
                threat.HasEndExplosionArea();
            const bool retain = RetainProjectileTermination(
                threat.Type(),
                threat.ExtraEndTime(),
                hasEndExplosion);
            int terminalHazardEndTick = terminationTick;
            if (retain) {
                terminalHazardEndTick = std::max(
                    SaturatingTickAdd(
                        terminationTick,
                        threat.ExtraEndTime()),
                    hasEndExplosion
                        ? threat.EndExplosionEndTick()
                        : terminationTick);
                threat.endTick = terminalHazardEndTick;
            }
            threat.expired = !retain ||
                ShouldExpireMissingTerminationAt(
                    now,
                    terminalHazardEndTick);
            ++threat.revision;
            changedIds.push_back(threat.id);
        }
        for (const auto& observation : observations) {
            const auto found = std::find_if(
                threats.begin(),
                threats.end(),
                [&](const Threat& threat) {
                    return threat.id == observation.threatId &&
                        MatchesObservedMissileEpisode(
                            threat.missileNetworkId,
                            threat.missileObjectIdentity,
                            observation.networkId,
                            observation.objectIdentity);
                });
            if (found == threats.end() || found->expired ||
                !found->missileBound || found->projectileTerminated)
                continue;
            const bool yuumiRoute = EqualsNoCase(found->SpellName(), "YuumiQCast");
            const std::int64_t elapsed =
                TickDifference(observation.tick, found->observedTick);
            if (yuumiRoute && found->observedTick > 0 && elapsed <= 100) continue;
            const bool steering =
                found->RouteMode() == MissileRouteMode::Steering;
            const Vec2 previousHead = found->observedHead;
            const Vec2 acceptedPosition = MonotonicMissileHead(
                previousHead,
                observation.position,
                found->direction,
                !steering);
            const Vec2 movement = acceptedPosition - previousHead;
            const Vec2 predicted = found->HeadAtTick(observation.tick);
            const MissileRouteObservationUpdate routeUpdate =
                ResolveMissileRouteObservationUpdate(
                found->RouteMode(),
                found->direction,
                found->startPos,
                found->AuthoredEnd(),
                found->endPos,
                acceptedPosition,
                observation.endPosition,
                movement,
                yuumiRoute,
                found->collisionStopped);
            const Vec2 routeDirection = routeUpdate.direction.Normalized();
            if (found->observedTick > 0 && elapsed >= 5 && elapsed <= 250) {
                found->observedSpeed = FilteredProjectedSpeed(
                    found->observedSpeed,
                    movement,
                    routeDirection,
                    static_cast<float>(elapsed));
            }
            const float correction = predicted.IsValid()
                ? predicted.Distance(acceptedPosition)
                : 0.0f;
            found->positionUncertainty = std::clamp(
                found->positionUncertainty * 0.8f + correction * 0.2f,
                0.0f,
                80.0f);
            const bool geometryChanged = routeUpdate.geometryChanged;
            if (steering && geometryChanged) {
                found->ClearPredictedCollision();
                found->direction = routeUpdate.direction;
                found->authoredEndPos = routeUpdate.authoredEnd;
                found->endPos = routeUpdate.effectiveEnd;
                if (yuumiRoute) {
                    found->collisionUnitCenter = {};
                    found->collisionExplosionCenter = {};
                    found->collisionKind = ZDCollisionKind::None;
                    found->collisionUnitNetworkId = 0;
                    found->collisionUnitObjectIdentity = 0;
                    found->collisionStopped = false;
                    found->collisionUnitTargetAuthoritative = false;
                    found->collisionHitCount = 0;
                    found->pendingUnitCollisions.clear();
                }
            }
            found->observedHead = acceptedPosition;
            found->observedTick = observation.tick;
            found->missileMissingSinceTick = -1;
            found->missilePositionUnavailable = false;
            if (steering) {
                const int linger = ThreatLifecycleLinger(
                    found->ExtraEndTime(),
                    found->HasEndExplosionArea(),
                    found->EndExplosionDelay(),
                    found->EndExplosionDuration());
                found->endTick =
                    SaturatingTickAdd(found->ArrivalTick(), linger);
            }
            ++found->revision;
            if (geometryChanged) changedIds.push_back(found->id);
        }
        ReleaseSRWLockExclusive(&storeLock);
        for (int id : changedIds) MarkChanged(id);
    }

    static void OnGameUpdate(const SDK::Events::GameUpdateEventArgs&) {
        gameUpdateThreadId.store(
            static_cast<std::uint32_t>(::GetCurrentThreadId()),
            std::memory_order_release);
        DrainPendingCasts();

        const int now = SDK::Variables::TickCount();
        PruneLogicalCastEpisodes(now);
        RefreshMissileObservations(now);
        UpdateSpecialThreats(now);
        UpdateCollisions(now);
        UpdateRetainedExplosions(now);
        bool removed = false;
        AcquireSRWLockExclusive(&storeLock);
        const auto oldSize = threats.size();
        threats.erase(
            std::remove_if(threats.begin(), threats.end(), [now](const Threat& threat) {
                return threat.IsExpiredAt(now);
            }),
            threats.end());
        removed = threats.size() != oldSize;
        ReleaseSRWLockExclusive(&storeLock);
        if (removed) MarkChanged(-1);
    }

    static void MarkChanged(int threatId) {
        changeSerial.fetch_add(1, std::memory_order_release);
        lastChangedThreatId.store(threatId, std::memory_order_release);
        lastChangeTick.store(SDK::Variables::TickCount(), std::memory_order_release);
    }
};

}
