#pragma once

#include "Threat.h"
#include "../Database/ThreatDatabase.h"
#include "../../../SDK/SDK.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace ZDEvade {

class ThreatDetector {
public:
    static void Initialize() {
        if (initialized) return;
        ThreatDatabase::Initialize();
        ResetState();
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
    static std::size_t DatabaseCount() { return ThreatDatabase::Count(); }

private:
    struct PendingCast {
        SDK::Events::ProcessSpellEventArgs args = {};
    };

    struct MissileObservation {
        int threatId = -1;
        Vec2 position = {};
        Vec2 endPosition = {};
        int tick = 0;
    };

    static inline constexpr int kPendingCapacity = 256;
    static inline SRWLOCK pendingLock = SRWLOCK_INIT;
    static inline std::array<PendingCast, kPendingCapacity> pendingCasts = {};
    static inline int pendingHead = 0;
    static inline int pendingCount = 0;
    static inline std::atomic<int> droppedRawEvents = 0;

    static inline SRWLOCK storeLock = SRWLOCK_INIT;
    static inline std::vector<Threat> threats;
    static inline int nextThreatId = 1;
    static inline std::atomic<int> changeSerial = 0;
    static inline std::atomic<int> lastChangeTick = 0;
    static inline std::atomic<int> lastChangedThreatId = -1;
    static inline std::atomic<bool> initialized = false;

    static void ResetState() {
        AcquireSRWLockExclusive(&pendingLock);
        pendingHead = 0;
        pendingCount = 0;
        droppedRawEvents = 0;
        for (auto& event : pendingCasts) event = {};
        ReleaseSRWLockExclusive(&pendingLock);

        AcquireSRWLockExclusive(&storeLock);
        threats.clear();
        nextThreatId = 1;
        changeSerial = 0;
        lastChangeTick = 0;
        lastChangedThreatId = -1;
        ReleaseSRWLockExclusive(&storeLock);
    }

    static void PushPending(const SDK::Events::ProcessSpellEventArgs& args) {
        if (!initialized || !args.Sender.IsValid()) return;
        AcquireSRWLockExclusive(&pendingLock);
        if (pendingCount >= kPendingCapacity) {
            pendingCasts[pendingHead].args = args;
            pendingHead = (pendingHead + 1) % kPendingCapacity;
            ++droppedRawEvents;
            ReleaseSRWLockExclusive(&pendingLock);
            return;
        }
        const int tail = (pendingHead + pendingCount) % kPendingCapacity;
        pendingCasts[tail].args = args;
        ++pendingCount;
        ReleaseSRWLockExclusive(&pendingLock);
    }

    static bool PopPending(PendingCast& out) {
        AcquireSRWLockExclusive(&pendingLock);
        if (pendingCount <= 0) {
            ReleaseSRWLockExclusive(&pendingLock);
            return false;
        }
        out = pendingCasts[pendingHead];
        pendingCasts[pendingHead] = {};
        pendingHead = (pendingHead + 1) % kPendingCapacity;
        --pendingCount;
        if (pendingCount == 0) pendingHead = 0;
        ReleaseSRWLockExclusive(&pendingLock);
        return true;
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
        return ContainsNoCase(name, "basicattack");
    }

    static bool IsUtilityName(const char* name) {
        if (!name || !name[0]) return false;
        return ContainsNoCase(name, "summoner") || ContainsNoCase(name, "item");
    }

    static bool HasBasicAttackName(const SDK::Events::ProcessSpellEventArgs& args) {
        return args.IsAutoAttack ||
               IsBasicAttackName(args.SpellName) ||
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

    static bool HasUsablePosition(const Vec2& position) {
        return position.IsValid() && !position.IsZero();
    }

    static bool IsSpellForSender(const SpellData& data,
                                 const SDK::Events::ProcessSpellEventArgs& args) {
        if (data.charName.empty() || data.charName == "AllChampions") return true;
        if (!args.Sender.CharacterName[0]) return true;
        return EqualsNoCase(data.charName, args.Sender.CharacterName);
    }

    static const SpellData* MatchProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
        if (HasBasicAttackName(args)) return nullptr;
        const char* primary[] = {
            args.SpellName,
            args.PayloadSpellName,
            args.ScriptName,
            args.MissileName,
            args.PayloadMissileName,
        };
        if (HasUtilityName(args)) {
            for (const char* name : primary) {
                if (!IsUtilityName(name)) continue;
                const auto* data = ThreatDatabase::FindAny(name, args.Sender.CharacterName);
                if (data && IsSpellForSender(*data, args)) return data;
            }
            return nullptr;
        }
        for (const char* name : primary) {
            if (!name || !name[0]) continue;
            const auto* data = ThreatDatabase::FindAny(name, args.Sender.CharacterName);
            if (data && IsSpellForSender(*data, args)) return data;
        }

        const bool validSlot = args.Slot >= 0 && args.Slot <= 3;
        const bool validCast = HasUsablePosition(args.CastPosition.To2D()) ||
                               HasUsablePosition(args.EndPosition.To2D());
        const bool safeFallback = validSlot && validCast && args.PayloadSpellName[0] &&
                                  !args.IsAutoAttack && args.TargetNetworkId == 0 &&
                                  args.TargetIndex <= 0 &&
                                  !IsBasicAttackName(args.SpellName) &&
                                  !IsUtilityName(args.SpellName) &&
                                  !IsUtilityName(args.ScriptName) &&
                                  !IsUtilityName(args.MissileName);
        if (!safeFallback || !args.SpellSlotName[0]) return nullptr;
        const auto* data = ThreatDatabase::FindCast(args.SpellSlotName, args.Sender.CharacterName);
        return data && IsSpellForSender(*data, args) ? data : nullptr;
    }

    static Vec2 ResolveStart(const SDK::Events::ProcessSpellEventArgs& args) {
        Vec2 start = args.StartPosition.To2D();
        if (!HasUsablePosition(start)) start = args.Sender.Position.To2D();
        return start;
    }

    static Vec2 ResolveEnd(const SpellData& data,
                           const SDK::Events::ProcessSpellEventArgs& args,
                           const Vec2& start) {
        Vec2 end = args.EndPosition.To2D();
        if (!HasUsablePosition(end)) end = args.CastPosition.To2D();
        if (!HasUsablePosition(end)) end = start;
        Vec2 direction = (end - start).Normalized();
        if (direction.IsZero()) direction = Vec2(1.0f, 0.0f);
        if (data.spellType == ZDSpellType::Line || data.spellType == ZDSpellType::Cone) {
            if (!data.useEndPosition || start.Distance(end) > data.range) {
                end = start + direction * std::max(1.0f, data.range);
            }
        } else if (data.range > 0.0f && start.Distance(end) > data.range) {
            end = start + direction * data.range;
        }
        return end;
    }

    static int CalculateEndTick(const SpellData& data,
                                const Vec2& start,
                                const Vec2& end,
                                int startTick,
                                int launchTick) {
        int result = startTick + std::max(0, data.spellDelay) + std::max(0, data.extraEndTime);
        if (std::isfinite(data.projectileSpeed) &&
            data.projectileSpeed > 1.0f &&
            data.projectileSpeed < 100000.0f) {
            const int travel = static_cast<int>(std::ceil(
                1000.0f * start.Distance(end) / data.projectileSpeed));
            const int travelBase = launchTick > 0
                ? launchTick
                : startTick + std::max(0, data.spellDelay);
            result = travelBase + travel + std::max(0, data.extraEndTime);
        }
        return result;
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

    static Threat* FindDuplicateLocked(const Threat& candidate) {
        for (auto& existing : threats) {
            if (existing.expired || existing.casterNetworkId != candidate.casterNetworkId) continue;
            if (existing.SpellName() != candidate.SpellName()) continue;
            if (candidate.sourceObjectNetworkId != 0 || existing.sourceObjectNetworkId != 0) {
                if (candidate.sourceObjectNetworkId == existing.sourceObjectNetworkId) return &existing;
                continue;
            }
            if (std::abs(candidate.startTick - existing.startTick) > 120) continue;
            const float directionDot = DirectionDot(existing.direction, candidate.direction);
            if (candidate.castIdentity != 0 && existing.castIdentity != 0) {
                if (candidate.castIdentity == existing.castIdentity && directionDot >= 0.9985f) return &existing;
                continue;
            }
            if (existing.startPos.DistanceSqr(candidate.startPos) > 160000.0f) continue;
            if (directionDot < 0.9985f) continue;
            if (existing.Type() != ZDSpellType::Line &&
                existing.endPos.DistanceSqr(candidate.endPos) > 22500.0f) continue;
            return &existing;
        }
        return nullptr;
    }

    static int AddOrUpdateThreat(const Threat& candidate) {
        int changedId = -1;
        AcquireSRWLockExclusive(&storeLock);
        if (Threat* existing = FindDuplicateLocked(candidate)) {
            if (!existing->missileBound && !existing->objectBound) {
                const int originalId = existing->id;
                const int originalStart = existing->startTick;
                const int originalRevision = existing->revision;
                *existing = candidate;
                existing->id = originalId;
                existing->startTick = std::min(originalStart, candidate.startTick);
                existing->revision = originalRevision + 1;
                changedId = originalId;
            }
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

    static void ProcessCast(const SDK::Events::ProcessSpellEventArgs& args) {
        if (!args.Sender.IsValid() || IsAlliedSender(args.Sender)) return;
        const SpellData* data = MatchProcessSpell(args);
        if (!data || data->noProcess) return;

        const Vec2 start = ResolveStart(args);
        if (!HasUsablePosition(start)) return;
        const Vec2 end = ResolveEnd(*data, args, start);
        if (!HasUsablePosition(end)) return;

        const int now = SDK::Variables::TickCount();
        Vec2 baseDirection = (end - start).Normalized();
        if (baseDirection.IsZero()) baseDirection = Vec2(1.0f, 0.0f);
        const float pathLength = std::max(1.0f, start.Distance(end));
        const int projectileCount = data->spellType == ZDSpellType::Line
            ? std::clamp(data->multipleNumber, 1, 15)
            : 1;
        const float centerIndex = static_cast<float>(projectileCount - 1) * 0.5f;
        for (int index = 0; index < projectileCount; ++index) {
            const float angleOffset =
                (static_cast<float>(index) - centerIndex) * data->multipleAngle;
            Threat threat;
            threat.data = data;
            threat.startPos = start;
            threat.direction = RotateDirection(baseDirection, angleOffset);
            threat.endPos = start + threat.direction * pathLength;
            threat.startTick = now - std::max(0, SDK::Game::Ping() / 2);
            threat.launchTick = 0;
            threat.endTick = CalculateEndTick(
                *threat.data, start, threat.endPos, threat.startTick, 0);
            threat.castIdentity = args.CastInfo;
            threat.casterNetworkId = args.Sender.NetworkId;
            threat.slot = static_cast<int>(data->spellKey);
            AddOrUpdateThreat(threat);
        }
    }

    static ::Core::Events::ObjectInfo HydrateLifecycleObject(
        const ::Core::Events::ObjectInfo& identity) {
        ::Core::Events::ObjectInfo object = identity;
        if (!object.Ptr) return object;
        object.NetworkId = object.NetworkId != 0
            ? object.NetworkId
            : ::Core::Objects::ReadNetworkId(object.Ptr);
        object.Index = object.Index != 0
            ? object.Index
            : ::Core::Objects::ReadIndex(object.Ptr);
        object.Team = ::Core::Objects::ReadTeamValue(object.Ptr);
        object.Position = ::Core::Objects::ReadPosition(object.Ptr);
        ::Core::Objects::ReadCharacterName(
            object.Ptr,
            object.CharacterName,
            static_cast<int>(sizeof(object.CharacterName)));
        ::Core::Objects::ReadName(
            object.Ptr,
            object.Name,
            static_cast<int>(sizeof(object.Name)));
        return object;
    }

    static const SpellData* MatchTrapObject(const ::Core::Events::ObjectInfo& object) {
        const char* names[] = { object.CharacterName, object.Name };
        for (const char* name : names) {
            const SpellData* data = ThreatDatabase::FindTrap(name);
            if (data) return data;
        }
        return nullptr;
    }

    static void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
        if (!initialized || !args.Sender.IsValid()) return;
        const ::Core::Events::ObjectInfo object = HydrateLifecycleObject(args.Sender);
        if (IsAlliedSender(object)) return;
        const SpellData* data = MatchTrapObject(object);
        if (!data) return;
        const Vec2 position = object.Position.To2D();
        if (!HasUsablePosition(position) || object.NetworkId == 0) return;

        const int now = SDK::Variables::TickCount();
        Threat threat;
        threat.data = data;
        threat.startPos = position;
        threat.endPos = position;
        threat.direction = Vec2(1.0f, 0.0f);
        threat.startTick = now;
        threat.launchTick = now;
        threat.endTick = std::numeric_limits<int>::max() - 1000;
        threat.castIdentity = object.Ptr;
        threat.casterNetworkId = object.NetworkId;
        threat.sourceObjectNetworkId = object.NetworkId;
        threat.slot = static_cast<int>(data->spellKey);
        threat.radiusOverride = data->trapRadius > 0.0f ? data->trapRadius : data->radius;
        threat.delayOverride = std::max(0, data->trapActivationDelay);
        threat.objectBound = true;
        AddOrUpdateThreat(threat);
    }

    static void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
        if (!initialized || args.Sender.NetworkId == 0) return;
        int changedId = -1;
        AcquireSRWLockExclusive(&storeLock);
        for (auto& threat : threats) {
            if (threat.objectBound &&
                threat.sourceObjectNetworkId == args.Sender.NetworkId) {
                threat.expired = true;
                changedId = threat.id;
            }
        }
        ReleaseSRWLockExclusive(&storeLock);
        if (changedId >= 0) MarkChanged(changedId);
    }

    static void OnRawProcessSpell(const SDK::Events::CoreHookArgs& raw) {
        if (raw.Id != SDK::Events::Hooks::OnProcessSpell) return;
        PushPending(::Core::Events::DecodeProcessSpell(raw));
    }

    static void OnRawDoCast(const SDK::Events::CoreHookArgs& raw) {
        if (raw.Id != SDK::Events::Hooks::OnDoCast) return;
        PushPending(::Core::Events::DecodeDoCast(raw));
    }

    static void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
        if (!initialized || !args.Sender.IsValid()) return;
        const SDK::MissileClient missile(args.Sender.Ptr);
        if (!missile.IsValid()) return;
        if (args.TargetNetworkId != 0 ||
            (args.TargetIndex != 0 && args.TargetIndex != 0xFFFFFFFFu) ||
            missile.TargetNetworkId() != 0) return;

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
        bool utilityMissile = false;
        for (const char* name : names) {
            if (IsBasicAttackName(name)) return;
            utilityMissile = utilityMissile || IsUtilityName(name);
        }
        const SpellData* data = nullptr;
        for (const char* name : names) {
            if (!name || !name[0]) continue;
            if (utilityMissile && !IsUtilityName(name)) continue;
            data = ThreatDatabase::FindMissile(name, casterName.c_str());
            if (data) break;
        }
        if (!data) return;

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
        const int launchTick = now - std::max(0, SDK::Game::Ping() / 2);
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
                delayed.startTick = launchTick;
                delayed.launchTick = 0;
                delayed.endTick = CalculateEndTick(
                    *aftershock, delayed.startPos, delayed.endPos, delayed.startTick, 0);
                delayed.casterNetworkId = casterNetworkId;
                delayed.slot = static_cast<int>(ZDSpellSlot::E);
                AddOrUpdateThreat(delayed);
            }
        }
        const std::uint32_t missileNetworkId = static_cast<std::uint32_t>(missile.NetworkId());
        Vec2 observedHead = missile.Position().To2D();
        if (!HasUsablePosition(observedHead)) observedHead = start;
        int changedId = -1;
        AcquireSRWLockExclusive(&storeLock);
        for (const auto& existing : threats) {
            if (!existing.expired && existing.missileBound &&
                existing.missileNetworkId == missileNetworkId) {
                ReleaseSRWLockExclusive(&storeLock);
                return;
            }
        }
        Threat* best = nullptr;
        float bestScore = -2.0f;
        for (auto& existing : threats) {
            if (existing.expired || existing.missileBound) continue;
            if (existing.casterNetworkId != casterNetworkId) continue;
            if (existing.SpellName() != data->spellName) continue;
            if (now - existing.startTick > 1600) continue;
            const float dot = DirectionDot(existing.direction, direction);
            if (dot < 0.95f) continue;
            const float score = dot - existing.startPos.Distance(start) / 10000.0f;
            if (score > bestScore) {
                bestScore = score;
                best = &existing;
            }
        }
        if (best) {
            best->data = data;
            best->startPos = start;
            best->endPos = end;
            best->direction = direction;
            best->launchTick = launchTick;
            best->endTick = CalculateEndTick(*best->data, start, end, best->startTick, launchTick);
            best->missileNetworkId = missileNetworkId;
            best->observedHead = observedHead;
            best->observedTick = now;
            best->observedSpeed = data->projectileSpeed;
            best->positionUncertainty = 0.0f;
            best->missileBound = true;
            ++best->revision;
            changedId = best->id;
        } else {
            Threat threat;
            threat.id = nextThreatId++;
            threat.data = data;
            threat.startPos = start;
            threat.endPos = end;
            threat.direction = direction;
            threat.startTick = launchTick - std::max(0, data->spellDelay);
            threat.launchTick = launchTick;
            threat.endTick = CalculateEndTick(*threat.data, start, end, threat.startTick, launchTick);
            threat.casterNetworkId = casterNetworkId;
            threat.missileNetworkId = missileNetworkId;
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

    static void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
        if (!initialized) return;
        const std::uint32_t deletedId = args.MissileNetworkId != 0
            ? args.MissileNetworkId
            : args.Sender.NetworkId;
        if (deletedId == 0) return;
        int changedId = -1;
        AcquireSRWLockExclusive(&storeLock);
        for (auto& threat : threats) {
            if (threat.missileBound && threat.missileNetworkId == deletedId) {
                threat.expired = true;
                changedId = threat.id;
            }
        }
        ReleaseSRWLockExclusive(&storeLock);
        if (changedId >= 0) MarkChanged(changedId);
    }

    static void RefreshMissileObservations(int now) {
        std::vector<std::pair<int, std::uint32_t>> tracked;
        AcquireSRWLockShared(&storeLock);
        tracked.reserve(threats.size());
        for (const auto& threat : threats) {
            if (!threat.expired && threat.missileBound && threat.missileNetworkId != 0)
                tracked.emplace_back(threat.id, threat.missileNetworkId);
        }
        ReleaseSRWLockShared(&storeLock);

        std::vector<MissileObservation> observations;
        observations.reserve(tracked.size());
        for (const auto& entry : tracked) {
            const auto missile = SDK::ObjectManager::GetUnitByNetworkId<SDK::MissileClient>(
                static_cast<int>(entry.second));
            if (!missile.IsValid()) continue;
            const Vec2 position = missile.Position().To2D();
            if (!HasUsablePosition(position)) continue;
            observations.push_back({entry.first, position, missile.EndPosition().To2D(), now});
        }
        if (observations.empty()) return;

        AcquireSRWLockExclusive(&storeLock);
        for (const auto& observation : observations) {
            const auto found = std::find_if(
                threats.begin(),
                threats.end(),
                [&](const Threat& threat) { return threat.id == observation.threatId; });
            if (found == threats.end() || found->expired) continue;
            const Vec2 predicted = found->HeadAtTick(observation.tick);
            const int elapsed = observation.tick - found->observedTick;
            if (found->observedTick > 0 && elapsed >= 5 && elapsed <= 250) {
                const Vec2 movement = observation.position - found->observedHead;
                const float distance = movement.Length();
                const float sampleSpeed = distance * 1000.0f / static_cast<float>(elapsed);
                if (std::isfinite(sampleSpeed) && sampleSpeed > 25.0f && sampleSpeed < 100000.0f) {
                    found->observedSpeed = found->observedSpeed > 1.0f
                        ? found->observedSpeed * 0.75f + sampleSpeed * 0.25f
                        : sampleSpeed;
                }
            }
            const float correction = predicted.IsValid()
                ? predicted.Distance(observation.position)
                : 0.0f;
            found->positionUncertainty = std::clamp(
                found->positionUncertainty * 0.8f + correction * 0.2f,
                0.0f,
                80.0f);
            found->observedHead = observation.position;
            found->observedTick = observation.tick;
            if (HasUsablePosition(observation.endPosition) &&
                observation.position.Distance(observation.endPosition) > 1.0f) {
                const Vec2 routeDirection = (observation.endPosition - found->startPos).Normalized();
                if (!routeDirection.IsZero()) {
                    found->endPos = observation.endPosition;
                    found->direction = routeDirection;
                }
            }
            const float remaining = observation.position.Distance(found->endPos);
            found->endTick = observation.tick + static_cast<int>(std::ceil(
                1000.0f * remaining / std::max(1.0f, found->Speed()))) +
                found->ExtraEndTime();
            ++found->revision;
        }
        ReleaseSRWLockExclusive(&storeLock);
    }

    static void OnGameUpdate(const SDK::Events::GameUpdateEventArgs&) {
        PendingCast event;
        int processed = 0;
        while (processed < kPendingCapacity && PopPending(event)) {
            ProcessCast(event.args);
            ++processed;
        }

        const int now = SDK::Variables::TickCount();
        RefreshMissileObservations(now);
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
