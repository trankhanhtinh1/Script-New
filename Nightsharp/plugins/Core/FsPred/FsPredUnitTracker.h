#pragma once

#include "FsPredMotionState.h"
#include "../../../sdk/Events/Events.h"
#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/GameObjects/ObjectManager.h"
#include "../../../sdk/Math/Prediction/Movement.h"
#include "../../../core/CoreBuffs.h"
#include "../../../SectionProfiler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Plugins::FsPred {

struct PathEvent {
    StablePathIntent Intent{};
    int Tick = 0;
};

struct UnitTrackerInfo {
    std::uint32_t NetworkId = 0;
    int NewPathTick = 0;
    int StopMoveTick = 0;
    int LastInvisibleTick = 0;
    int LastSeenTick = 0;
    StablePathIntent LastIntent{};
    std::array<PathEvent, 6> PathEvents{};
    std::size_t PathEventCount = 0;
    std::size_t NextPathEvent = 0;
    bool WasMoving = false;
    bool SawInvisible = false;
    SDK::HitChance CachedMotionState = SDK::HitChance::None;
    bool HasCachedMotionState = false;
};

class UnitTracker {
public:
    static void Initialize() {
        if (initialized_) {
            return;
        }

        initialized_ = true;
        lastUpdateTick_ = -1;
        slots_ = {};
        pathHookSubscribed_ =
            SDK::Events::AddOnNewPath(&UnitTracker::OnNewPath);
        deleteHookSubscribed_ =
            SDK::Events::AddOnDeleteObject(&UnitTracker::OnDeleteObject);
        SeedVisibleEnemies();
    }

    static void Shutdown() {
        if (pathHookSubscribed_) {
            SDK::Events::RemoveOnNewPath(&UnitTracker::OnNewPath);
        }
        if (deleteHookSubscribed_) {
            SDK::Events::RemoveOnDeleteObject(&UnitTracker::OnDeleteObject);
        }
        pathHookSubscribed_ = false;
        deleteHookSubscribed_ = false;
        initialized_ = false;
        lastUpdateTick_ = -1;
        slots_ = {};
    }

    static void Update() {
        if (!initialized_) {
            Initialize();
        }

        const int tick = SDK::Variables::TickCount();
        if (lastUpdateTick_ == tick) {
            return;
        }
        NS_PROFILE("FsPred.Tracker");
        lastUpdateTick_ = tick;

        for (const auto& hero : SDK::GameObjects::EnemyHeroesFrame()) {
            if (!hero.IsValid()) {
                continue;
            }

            UnitTrackerInfo& info = AcquireSlot(hero.NetworkId(), tick);
            const bool isMoving = hero.IsMoving();
            info.LastSeenTick = tick;

            if (!hero.IsVisible()) {
                info.LastInvisibleTick = tick;
                info.SawInvisible = true;
            }
            if (info.WasMoving && !isMoving) {
                info.StopMoveTick = tick;
            }

            if (!pathHookSubscribed_) {
                const auto& path = hero.CachedWaypoints();
                RecordPathIntent(
                    info,
                    ExtractStablePathIntent(
                        std::span<const SDK::Vector3>(path.data(), path.size())),
                    tick);
            }
            info.WasMoving = isMoving;
        }
    }

    static MotionFacts GetMotionFacts(const SDK::AIBaseClient& unit) {
        if (!unit.IsValid()) {
            MotionFacts facts{};
            facts.CanMove = false;
            return facts;
        }
        Update();
        return BuildMotionFacts(unit, FindSlot(unit.NetworkId()));
    }

    static void RefreshCachedMotionStates() {
        Update();
        for (const auto& hero : SDK::GameObjects::EnemyHeroesFrame()) {
            if (!hero.IsValid()) {
                continue;
            }
            UnitTrackerInfo* info = FindSlot(hero.NetworkId());
            if (!info) {
                continue;
            }
            info->CachedMotionState =
                ClassifyMotion(BuildMotionFacts(hero, info));
            info->HasCachedMotionState = true;
        }
    }

    static bool TryGetCachedMotionState(std::uint32_t networkId,
                                        SDK::HitChance& state) {
        const UnitTrackerInfo* info = FindSlot(networkId);
        if (!info || !info->HasCachedMotionState) {
            return false;
        }
        state = info->CachedMotionState;
        return true;
    }

    static double GetLastNewPathTime(const SDK::AIBaseClient& unit) {
        const UnitTrackerInfo* info = FindSlot(unit.NetworkId());
        return info
            ? static_cast<double>(SDK::Variables::TickCount() - info->NewPathTick)
            : 0.0;
    }

    static double GetLastVisibleTime(const SDK::AIBaseClient& unit) {
        const UnitTrackerInfo* info = FindSlot(unit.NetworkId());
        return info
            ? static_cast<double>(SDK::Variables::TickCount() - info->LastInvisibleTick)
            : 0.0;
    }

    static double GetLastStopMoveTime(const SDK::AIBaseClient& unit) {
        const UnitTrackerInfo* info = FindSlot(unit.NetworkId());
        return info
            ? static_cast<double>(SDK::Variables::TickCount() - info->StopMoveTick)
            : 0.0;
    }

private:
    static MotionFacts BuildMotionFacts(const SDK::AIBaseClient& unit,
                                        UnitTrackerInfo* info) {
        MotionFacts facts{};
        if (!unit.IsValid()) {
            facts.CanMove = false;
            return facts;
        }

        const int tick = SDK::Variables::TickCount();
        facts.IsDashing = SDK::Extensions::IsDashing(unit);
        facts.IsRecalling = unit.HasBuff("Recall");
        facts.CanMove = SDK::CanMove(unit);
        facts.IsCasting = unit.Spellbook().IsWindingUp();
        facts.IsMoving = unit.IsMoving();

        const float gameTime = SDK::Game::Time();
        const auto* buffs =
            CoreBuffs::GetOrBuildFrameBuffSnapshot(unit.Address(), gameTime);
        facts.HasHardCrowdControl =
            RemainingImmobilitySeconds(buffs, gameTime) >= 0.0;

        if (info) {
            facts.BecameVisible = info->SawInvisible && unit.IsVisible();
            facts.VisibleAgeMs =
                static_cast<double>(tick - info->LastInvisibleTick);
            facts.StopAgeMs = static_cast<double>(tick - info->StopMoveTick);
            facts.PathAgeMs = static_cast<double>(tick - info->NewPathTick);

            const auto reversal = LatestReversal(*info, tick);
            facts.ReversalAngleDegrees = reversal.AngleDegrees;
            facts.ReversalAgeMs = reversal.AgeMs;
            facts.HasStableHeading =
                info->LastIntent.HasDestination &&
                reversal.AngleDegrees <= kReversalAngleDeg;
        }

        const auto& path = unit.CachedWaypoints();
        const StablePathIntent currentIntent = ExtractStablePathIntent(
            std::span<const SDK::Vector3>(path.data(), path.size()));
        facts.HasPath = currentIntent.HasDestination && facts.IsMoving;
        if (currentIntent.HasDestination) {
            const SDK::Vector3 position = unit.ServerPosition();
            const float dx = currentIntent.DestinationX - position.x;
            const float dz = currentIntent.DestinationZ - position.z;
            facts.NearPathEnd = dx * dx + dz * dz <= 100.0f * 100.0f;
        }
        return facts;
    }

    struct ReversalFact {
        float AngleDegrees = 0.0f;
        double AgeMs = 0.0;
    };

    static constexpr std::size_t kMaxEnemySlots = 10;
    static constexpr std::size_t kPathEventCapacity = 6;
    static constexpr float kReversalWindowMs = 400.0f;
    static constexpr float kReversalAngleDeg = 100.0f;

    static UnitTrackerInfo* FindSlot(std::uint32_t networkId) {
        if (networkId == 0 || networkId == 0xFFFFFFFFu) {
            return nullptr;
        }
        for (auto& slot : slots_) {
            if (slot.NetworkId == networkId) {
                return &slot;
            }
        }
        return nullptr;
    }

    static const UnitTrackerInfo* FindSlot(std::uint32_t networkId,
                                           const std::array<UnitTrackerInfo,
                                                            kMaxEnemySlots>& slots) {
        if (networkId == 0 || networkId == 0xFFFFFFFFu) {
            return nullptr;
        }
        for (const auto& slot : slots) {
            if (slot.NetworkId == networkId) {
                return &slot;
            }
        }
        return nullptr;
    }

    static UnitTrackerInfo& AcquireSlot(std::uint32_t networkId, int tick) {
        if (UnitTrackerInfo* existing = FindSlot(networkId)) {
            return *existing;
        }

        UnitTrackerInfo* selected = nullptr;
        for (auto& slot : slots_) {
            if (slot.NetworkId == 0) {
                selected = &slot;
                break;
            }
        }
        if (!selected) {
            selected = &*std::min_element(
                slots_.begin(),
                slots_.end(),
                [](const UnitTrackerInfo& left, const UnitTrackerInfo& right) {
                    return left.LastSeenTick < right.LastSeenTick;
                });
        }

        *selected = {};
        selected->NetworkId = networkId;
        selected->NewPathTick = tick;
        selected->StopMoveTick = tick;
        selected->LastInvisibleTick = tick - 10000;
        selected->LastSeenTick = tick;
        return *selected;
    }

    static void SeedVisibleEnemies() {
        const int tick = SDK::Variables::TickCount();
        for (const auto& hero : SDK::GameObjects::EnemyHeroesFrame()) {
            if (!hero.IsValid()) {
                continue;
            }
            UnitTrackerInfo& info = AcquireSlot(hero.NetworkId(), tick);
            info.WasMoving = hero.IsMoving();
            info.StopMoveTick = info.WasMoving ? tick - 10000 : tick;
            if (!hero.IsVisible()) {
                info.LastInvisibleTick = tick;
                info.SawInvisible = true;
            }
            const auto& path = hero.CachedWaypoints();
            info.LastIntent = ExtractStablePathIntent(
                std::span<const SDK::Vector3>(path.data(), path.size()));
        }
    }

    static void RecordPathIntent(UnitTrackerInfo& info,
                                 const StablePathIntent& intent,
                                 int tick) {
        if (SameStableDestination(info.LastIntent, intent)) {
            return;
        }

        info.LastIntent = intent;
        info.NewPathTick = tick;
        if (!intent.HasDestination) {
            return;
        }

        info.PathEvents[info.NextPathEvent] = { intent, tick };
        info.NextPathEvent =
            (info.NextPathEvent + 1) % kPathEventCapacity;
        info.PathEventCount =
            std::min(info.PathEventCount + 1, kPathEventCapacity);
    }

    static const PathEvent& ChronologicalPathEvent(
        const UnitTrackerInfo& info,
        std::size_t index) {
        const std::size_t first =
            (info.NextPathEvent + kPathEventCapacity - info.PathEventCount) %
            kPathEventCapacity;
        return info.PathEvents[(first + index) % kPathEventCapacity];
    }

    static ReversalFact LatestReversal(const UnitTrackerInfo& info,
                                       int tick) {
        ReversalFact result{};
        if (info.PathEventCount < 2) {
            return result;
        }

        for (std::size_t index = 1; index < info.PathEventCount; ++index) {
            const PathEvent& previous =
                ChronologicalPathEvent(info, index - 1);
            const PathEvent& current =
                ChronologicalPathEvent(info, index);
            if (!previous.Intent.HasHeading || !current.Intent.HasHeading) {
                continue;
            }

            const double age = static_cast<double>(tick - current.Tick);
            if (age < 0.0 || age > kReversalWindowMs) {
                continue;
            }
            const float dot = std::clamp(
                previous.Intent.HeadingX * current.Intent.HeadingX +
                    previous.Intent.HeadingZ * current.Intent.HeadingZ,
                -1.0f,
                1.0f);
            const float angle =
                std::acos(dot) * 180.0f / 3.14159265358979323846f;
            if (angle > result.AngleDegrees) {
                result.AngleDegrees = angle;
                result.AgeMs = age;
            }
        }
        return result;
    }

    static void OnNewPath(const SDK::Events::NewPathEventArgs& args) {
        if (!initialized_ || args.IsDash ||
            args.Sender.NetworkId == 0 ||
            args.Sender.NetworkId == 0xFFFFFFFFu) {
            return;
        }

        const int count = std::clamp(args.PathCount, 0, 32);
        const int tick = SDK::Variables::TickCount();
        UnitTrackerInfo& info = AcquireSlot(args.Sender.NetworkId, tick);
        info.LastSeenTick = tick;
        RecordPathIntent(
            info,
            ExtractStablePathIntent(
                std::span<const ::Vec3>(args.Path, count)),
            tick);
    }

    static void OnDeleteObject(const SDK::Events::ObjectEventArgs& args) {
        if (!initialized_) {
            return;
        }
        if (UnitTrackerInfo* info = FindSlot(args.Sender.NetworkId)) {
            *info = {};
        }
    }

    inline static bool initialized_ = false;
    inline static bool pathHookSubscribed_ = false;
    inline static bool deleteHookSubscribed_ = false;
    inline static int lastUpdateTick_ = -1;
    inline static std::array<UnitTrackerInfo, kMaxEnemySlots> slots_{};
};

} // namespace Plugins::FsPred
