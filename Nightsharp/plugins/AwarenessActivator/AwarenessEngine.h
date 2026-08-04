#pragma once

#include "AwarenessActivatorCore.h"
#include "AwarenessInsights.h"
#include "../../SectionProfiler.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>
namespace NightSharp::Companion {

class AwarenessEngine final {
public:
    AwarenessEngine()
        : arbiter_(&decisionLog_) {}

    void Reset() {
        clock_.Reset();
        store_.Reset();
        eventBus_.Clear();
        decisionLog_.Clear();
        arbiter_.Clear();
        insights_.Reset();
        lastNow_ = 0.0f;
        lastTimerAt_ = -1.0f;
        lastPruneAt_ = -1.0f;
    }

    void SetMode(RuntimeMode mode) noexcept {
        mode_ = mode;
        store_.SetMode(mode);
    }
    void SetRuleset(RoleQuestRuleset ruleset) noexcept {
        store_.SetRuleset(ruleset);
        registry_.SetGameContext(store_.MapId(), ruleset);
    }
    RoleQuestRuleset Ruleset() const noexcept {
        return store_.Ruleset();
    }
    void SetMapId(int mapId) noexcept {
        store_.SetMapId(mapId);
        registry_.SetGameContext(mapId, store_.Ruleset());
    }
    int MapId() const noexcept { return store_.MapId(); }

    RuntimeMode Mode() const noexcept { return mode_; }
    ClockSnapshot UpdateClock(float rawTime, int pingMs) noexcept {
        const ClockSnapshot result = clock_.Update(rawTime, pingMs, mode_);
        lastNow_ = result.logicalGameTime;
        if (result.remakeDetected ||
            (result.reconnectAdjusted && mode_ == RuntimeMode::Replay)) {
            ResetSessionState();
        } else if (result.reconnectAdjusted) {
            ReconcileAfterReconnect();
        }
        store_.SetMode(mode_);
        store_.SetNow(lastNow_);
        if (result.reconnectAdjusted) {
            GameEvent reset{};
            reset.type = EventType::GameReset;
            reset.at = result.logicalGameTime;
            reset.value = static_cast<int>(result.generation);
            reset.visibleAtEvent = true;
            eventBus_.Publish(reset);
            store_.TeamfightTimeline().Push(reset);
        }
        return result;
    }

    void BeginFrame(float now) {
        NS_PROFILE("Awareness.Engine.BeginFrame");
        lastNow_ = now;
        store_.SetNow(now);

        // OnUpdate can be tied to uncapped render FPS. Cooldown/timer state
        // does not need to be recomputed hundreds of times per second.
        constexpr float kTimerStep = 1.0f / 30.0f;
        if (lastTimerAt_ < 0.0f || now < lastTimerAt_ ||
            now - lastTimerAt_ >= kTimerStep) {
            TickTimers(now);
        }
    }

    void ObserveChampion(const ChampionObservation& observation) {
        NS_PROFILE("Awareness.Engine.ObserveChampion");
        if (observation.networkId == 0 ||
            observation.networkId == 0xFFFFFFFFu) {
            return;
        }
        if (!store_.FindChampion(observation.networkId)) {
            const std::uint32_t previousId =
                FindRecreatedChampion(observation);
            if (previousId != 0) {
                store_.RekeyChampion(previousId, observation.networkId);
            }
        }
        ChampionState& state =
            store_.GetOrCreateChampion(observation.networkId);
        const bool wasVisible = state.visible;
        const bool hadObservation = state.health.evidence.IsKnown();
        const float previousNeutralCs = state.neutralMinionsKilled;
        const bool wasDead = state.dead;
        const bool wasPossession = state.possession;
        const std::uint64_t previousInventory = state.inventorySignature;
        const std::uint64_t previousSummoners = state.summonerSignature;
        const std::array<ObservedItem, 8> previousItems = state.items;
        const std::size_t previousItemCount = state.itemCount;
        const int previousLevel = state.level.value;
        const RoleQuestState previousRoleQuest = state.roleQuest;
        state.networkId = observation.networkId;
        state.team = observation.team;
        state.enemy = observation.enemy;
        state.ally = observation.ally;
        state.local = observation.local;
        state.clone = observation.clone;
        if (observation.visible || observation.ally ||
            observation.local) {
            state.possession = observation.possession;
            state.recalling = observation.recalling;
            state.teleporting = observation.teleporting ||
                                (state.teleporting &&
                                 observation.channeling);
            state.channeling = observation.channeling;
            state.targetable = observation.targetable;
            state.invulnerable = observation.invulnerable;
        }
        state.visible = observation.visible;
        state.dead = observation.dead;
        state.lastObservedAt = lastNow_;

        if (!observation.visible) {
            state.visibilityAge = std::max(0.0f, lastNow_ - state.lastSeenAt);
            if (wasVisible) {
                MarkChampionLastSeen(state);
                PublishVisibility(state, false);
            }
            if (observation.dead && !wasDead) {
                PublishUnitDeath(state);
            }
            return;
        }

        const Evidence visibleEvidence{ Provenance::VisibleNow, Confidence::Confirmed,
                                        lastNow_, 0.0f, HashId("sdk.visible") };
        state.visibilityAge = 0.0f;
        state.lastSeenAt = lastNow_;
        state.position = observation.position;
        state.observedEventPosition = {};
        state.observedEventAt = 0.0f;
        state.observedEventUntil = 0.0f;
        state.observedEventEvidence = {};
        state.lastSeenPosition = observation.position;
        state.lastDirection = Normalize2D(observation.direction);
        state.lastSeenSpeed = observation.moveSpeed;
        state.moveSpeed = observation.moveSpeed;
        state.abilityHaste = observation.abilityHaste;
        state.neutralMinionsKilled = observation.neutralMinionsKilled;
        state.reachableRadius = 0.0f;
        state.pathBranches = std::max(0, observation.pathBranches);
        CopyText(state.name, observation.name);
        CopyText(state.championId, observation.championId);
        if (!state.baseChampionId[0]) {
            CopyText(state.baseChampionId, observation.championId);
        }
        CopyText(state.activeFormId, observation.championId);
        state.health.Set(observation.health, visibleEvidence);
        state.maxHealth.Set(observation.maxHealth, visibleEvidence);
        state.mana.Set(observation.mana, visibleEvidence);
        state.maxMana.Set(observation.maxMana, visibleEvidence);
        state.level.Set(observation.level, visibleEvidence);
        state.allShield = std::max(
            0.0f, observation.allShield);
        state.healthRegen = std::max(
            0.0f, observation.healthRegen);
        EconomyInsightService::ObserveGold(
            state, observation.currentGold, observation.totalGold,
            visibleEvidence);
        state.roleQuest = observation.roleQuest;
        state.roleQuest.evidence = visibleEvidence;

        const std::uint64_t inventorySignature = ComputeInventorySignature(observation);
        state.inventorySignature = inventorySignature;
        state.summonerSignature = ComputeSummonerSignature(observation);
        CopyItems(state, observation, visibleEvidence);
        CopySpells(state, observation, visibleEvidence);
        CopyBuffs(state, observation, visibleEvidence);
        ReconcileSpecialState(
            state, previousRoleQuest, wasPossession, visibleEvidence);
        if (lastNow_ - state.lastActivitySampleAt >= 1.0f) {
            ActivitySample sample{};
            sample.position = state.position;
            sample.at = lastNow_;
            sample.team = state.team;
            sample.kind = ActivityKind::ChampionSeen;
            sample.evidence = visibleEvidence;
            insights_.activity.Push(sample);
            state.lastActivitySampleAt = lastNow_;
        }
        if (state.enemy && hadObservation &&
            observation.neutralMinionsKilled > previousNeutralCs + 0.5f &&
            IsJungleCandidate(state)) {
            InferCampFromNeutralCs(
                state,
                observation.neutralMinionsKilled - previousNeutralCs);
        }

        if (!wasVisible) PublishVisibility(state, true);
        if (previousInventory != 0 && previousInventory != inventorySignature) {
            GameEvent event{};
            event.type = EventType::InventoryChanged;
            event.at = lastNow_;
            event.objectId = state.networkId;
            event.visibleAtEvent = true;
            eventBus_.Publish(event);
            store_.TeamfightTimeline().Push(event);
            const SDK::ItemId purchased =
                EconomyInsightService::FindNewItem(
                    previousItems, previousItemCount,
                    state.items, state.itemCount);
            if (purchased != SDK::ItemId::Unknown) {
                PublishPurchaseInsight(
                    state, purchased, hadObservation && !wasVisible);
            }
        }
        if (previousSummoners != 0 &&
            previousSummoners != state.summonerSignature) {
            GameEvent event{};
            event.type = EventType::SummonerSpellChanged;
            event.at = lastNow_;
            event.objectId = state.networkId;
            event.visibleAtEvent = true;
            eventBus_.Publish(event);
            store_.TeamfightTimeline().Push(event);
        }
        if (previousLevel > 0 && state.level.value > previousLevel &&
            (state.level.value == 6 || state.level.value == 11 ||
             state.level.value == 16)) {
            PublishLevelSpike(state);
        }
        if (state.dead && !wasDead) {
            PublishUnitDeath(state);
        } else if (!state.dead && wasDead) {
            GameEvent event{};
            event.type = EventType::UnitRespawned;
            event.at = lastNow_;
            event.objectId = state.networkId;
            event.visibleAtEvent = true;
            eventBus_.Publish(event);
            store_.TeamfightTimeline().Push(event);
            state.reviveInProgress = false;
            if (state.reviveSource == ReviveSource::GuardianAngel) {
                state.reviveAvailable = false;
            }
            state.deathAt = 0.0f;
            state.respawnAt = 0.0f;
        }
    }
    void CompleteChampionSnapshot(const std::uint32_t* observedIds,
                                  std::size_t observedCount) {
        store_.ForEachChampion([&](ChampionState& state) {
            bool observed = false;
            for (std::size_t i = 0; i < observedCount; ++i) {
                if (observedIds && observedIds[i] == state.networkId) {
                    observed = true;
                    break;
                }
            }
            if (observed || state.lastObservedAt >= lastNow_) return;
            if (state.visible) {
                MarkChampionLastSeen(state);
                state.visible = false;
                PublishVisibility(state, false);
            }
            if (state.clone) {
                state.cloneExpired = true;
                state.cloneExpiresAt = lastNow_;
                state.dead = true;
            }
        });
    }

    void ObserveWard(const WardState& observation) {
        if (observation.networkId == 0) return;
        Upsert( store_.Wards(), observation.networkId,
                [&](WardState& current) { current = observation; });
    }

    void ObserveObjective(const ObjectiveState& observation) {
        UpsertBy(store_.Objectives(), static_cast<std::uint32_t>(observation.kind) + 0x10000u,
                 [&](ObjectiveState& current) {
                     const bool wasAlive = current.status == ObjectiveStatus::AliveVisible ||
                                           current.status == ObjectiveStatus::AliveUnknown ||
                                           current.status == ObjectiveStatus::InCombatVisible;
                     current = observation;
                     if (wasAlive && observation.status == ObjectiveStatus::Dead) {
                         GameEvent event{};
                         event.type = EventType::ObjectiveKilled;
                         event.at = lastNow_;
                         event.objectId = observation.networkId;
                         event.value = static_cast<int>(observation.kind);
                         event.visibleAtEvent = observation.visible;
                         eventBus_.Publish(event);
                         store_.TeamfightTimeline().Push(event);
                     }
                 },
                 [](const ObjectiveState& value) {
                     return static_cast<std::uint32_t>(value.kind) + 0x10000u;
                 });
    }

    void ObserveJungleCamp(const JungleCampState& observation) {
        const std::uint32_t key = observation.campKey != 0
            ? observation.campKey
            : (observation.networkId != 0 ? observation.networkId : HashId(observation.campId));
        UpsertBy(store_.Jungles(), key,
                 [&](JungleCampState& current) { current = observation; },
                 [](const JungleCampState& value) {
                     return value.campKey != 0
                         ? value.campKey
                         : (value.networkId != 0 ? value.networkId : HashId(value.campId));
                 });
    }


    void ObserveThreat(const ThreatState& observation) {
        if (observation.id == 0) return;
        UpsertBy(store_.Threats(), observation.id,
                 [&](ThreatState& current) { current = observation; },
                 [](const ThreatState& value) { return value.id; });
    }

    void RemoveThreat(std::uint32_t id) {
        for (std::size_t i = 0; i < store_.Threats().Size(); ++i) {
            ThreatState& threat = store_.Threats().At(i);
            if (threat.id == id) {
                threat.endAt = std::min(threat.endAt, lastNow_);
                threat.visible = false;
                GameEvent event{};
                event.type = EventType::ThreatRemoved;
                event.at = lastNow_;
                event.objectId = id;
                eventBus_.Publish(event);
                store_.TeamfightTimeline().Push(event);
                return;
            }
        }
    }

    void RecordDamage(const DamageRecord& record) {
        store_.Damage().Push(record);
        GameEvent event{};
        event.type = EventType::DamageObserved;
        event.at = record.at > 0.0f ? record.at : lastNow_;
        event.sourceId = record.sourceId;
        event.targetId = record.targetId;
        event.valueFloat = record.amount;
        CopyText(event.name, record.source);
        store_.TeamfightTimeline().Push(event);
        eventBus_.Publish(event);
        const ChampionState* target =
            store_.FindChampion(record.targetId);
        if (target && target->position.IsValid()) {
            ActivitySample sample{};
            sample.position = target->position;
            sample.at = record.at > 0.0f ? record.at : lastNow_;
            sample.team = target->team;
            sample.kind = ActivityKind::Damage;
            sample.evidence = record.evidence;
            insights_.activity.Push(sample);
        }
    }

    void ApplyEvent(const GameEvent& event) {
        if (event.at > 0.0f) lastNow_ = std::max(lastNow_, event.at);
        ChampionState* subject = store_.FindChampion(event.objectId);
        const bool hiddenEnemyEventAllowed =
            event.enemy && !event.visibleAtEvent;
        const bool observedEventAllowed =
            event.visibleAtEvent || hiddenEnemyEventAllowed;
        switch (event.type) {
        case EventType::SpellCastStarted:
            if (subject && observedEventAllowed) {
                ApplySpellEvent(*subject, event);
                ApplyObservedEventReveal(*subject, event);
            }
            break;
        case EventType::SpellCastCompleted:
        case EventType::SpellCastCancelled:
            if (subject && observedEventAllowed) {
                subject->channeling = false;
                subject->recalling = false;
                subject->teleporting = false;
            }
            break;
        case EventType::BuffAdded:
        case EventType::BuffUpdated:
            if (subject && event.visibleAtEvent) ApplyBuffEvent(*subject, event, true);
            break;
        case EventType::BuffRemoved:
            if (subject && event.visibleAtEvent) ApplyBuffEvent(*subject, event, false);
            break;
        case EventType::VisibilityChanged:
            if (subject) subject->visible = event.value != 0;
            break;
        case EventType::ThreatCreated: {
            ChampionState* source = store_.FindChampion(event.sourceId);
            if (source && observedEventAllowed) {
                ApplyObservedEventReveal(*source, event);
            }
            break;
        }
        case EventType::PathChanged:
            if (subject && event.visibleAtEvent) {
                const Point3 start = IsValidPathPoint(event.startPosition)
                    ? event.startPosition : subject->position;
                const Point3 direction = Normalize2D(
                    event.endPosition - start);
                if (!direction.IsZero()) subject->lastDirection = direction;
                subject->pathBranches = std::max(0, event.value);
                subject->lastSeenSpeed = std::max(0.0f, event.speed);
                subject->reachableRadius = 0.0f;

                const float pathDistance =
                    IsValidPathPoint(event.endPosition)
                        ? start.Distance(event.endPosition) : 0.0f;
                if (event.value > 0 && pathDistance >= 35.0f) {
                    subject->pathStartPosition = start;
                    subject->pathTargetPosition = event.endPosition;
                    subject->pathUpdatedAt = event.at;
                    subject->pathNodeCount = event.value;
                    const float travelSeconds = event.speed > 1.0f
                        ? pathDistance / event.speed : 2.0f;
                    const float lifetime = std::clamp(
                        travelSeconds + 0.75f, 1.0f, 8.0f);
                    subject->pathExpectedArrivalAt =
                        event.at + std::clamp(
                            travelSeconds, 0.25f, 7.25f);
                    subject->pathEvidence = {
                        Provenance::ObservedEvent, Confidence::Confirmed,
                        event.at, event.at + lifetime,
                        HashId("event.path-target")
                    };
                } else {
                    ClearPathTarget(*subject);
                }
            }
            break;
        case EventType::RecallStarted:
            if (subject) {
                subject->recalling = true;
                subject->channeling = true;
                ClearPathTarget(*subject);
            }
            break;
        case EventType::RecallEnded:
            if (subject) {
                subject->recalling = false;
                subject->channeling = false;
            }
            break;
        case EventType::TeleportStarted:
            if (subject) {
                subject->teleporting = true;
                subject->channeling = true;
                ClearPathTarget(*subject);
                if (subject->enemy && event.visibleAtEvent) {
                    AlertState alert{};
                    alert.id = subject->networkId ^ HashId("alert.enemy-teleport");
                    alert.source = subject->networkId;
                    alert.priority = 70;
                    alert.at = lastNow_;
                    alert.expiresAt = lastNow_ +
                        std::max(2.0f, event.duration > 0.0f ? event.duration : 4.0f);
                    alert.cooldownUntil = lastNow_ + 8.0f;
                    alert.confidence = Confidence::Confirmed;
                    CopyText(alert.title, "Enemy Teleport observed");
                    CopyText(alert.detail,
                             "Visible enemy began a Teleport channel.");
                    RaiseAlert(alert);
                }
            }
            break;
        case EventType::TeleportEnded:
            if (subject) {
                subject->teleporting = false;
                subject->channeling = false;
            }
            break;
        case EventType::ChannelInterrupted:
            if (subject) {
                subject->channeling = false;
                subject->recalling = false;
                subject->teleporting = false;
            }
            break;
        case EventType::WardDestroyed:
            if (event.ally && event.visibleAtEvent) {
                AlertState alert{};
                alert.id = event.objectId ^
                           HashId("alert.vision-gap");
                alert.source = event.objectId;
                alert.priority = 45;
                alert.at = lastNow_;
                alert.expiresAt = lastNow_ + 6.0f;
                alert.cooldownUntil = lastNow_ + 10.0f;
                alert.confidence = Confidence::Confirmed;
                CopyText(alert.title, "Vision gap observed");
                CopyText(
                    alert.detail,
                    "An allied ward was destroyed; nearby routes are less covered.");
                RaiseAlert(alert);
            }
            break;
        case EventType::UnitDied:
            if (subject) {
                subject->dead = true;
                subject->deathAt = event.at;
            }
            HandleTakedownReset(event.sourceId);
            break;
        case EventType::UnitRespawned:
            if (subject) {
                subject->dead = false;
                subject->respawnAt = event.at;
                subject->reviveInProgress = false;
                if (subject->reviveSource ==
                    ReviveSource::GuardianAngel) {
                    subject->reviveAvailable = false;
                }
            }
            break;
        case EventType::ObjectDeleted:
            if (event.visibleAtEvent) {
                store_.EraseChampion(event.objectId);
            }
            break;
        case EventType::InventoryChanged:
        case EventType::SummonerSpellChanged:
            if (subject) {
                subject->inventorySignature = 0;
                subject->summonerSignature = 0;
            }
            break;
        default:
            break;
        }
        if (event.type != EventType::GameReset) {
            store_.TeamfightTimeline().Push(event);
        }
    }

    void TickTimers(float now) {
        NS_PROFILE("Awareness.Engine.TickTimers");
        store_.SetNow(now);
        float elapsed = 0.0f;
        if (lastTimerAt_ >= 0.0f && now >= lastTimerAt_) {
            elapsed = now - lastTimerAt_;
        }
        lastTimerAt_ = now;
        store_.ForEachChampion([&](ChampionState& champion) {
            if (!champion.visible &&
                champion.health.evidence.provenance == Provenance::LastSeen) {
                champion.visibilityAge =
                    std::max(0.0f, now - champion.lastSeenAt);
                const float travel = std::clamp(
                    std::max(0.0f, champion.lastSeenSpeed) *
                        champion.visibilityAge,
                    0.0f, 16000.0f);
                const float uncertainty =
                    120.0f + champion.visibilityAge * 35.0f;
                champion.reachableRadius =
                    std::clamp(travel + uncertainty, 120.0f, 18000.0f);
                if (champion.pathBranches > 1) {
                    champion.reachableRadius *= 1.15f;
                }
                DegradeLastSeenEvidence(champion);
            }
            TickSpellCooldowns(champion, elapsed, now);
            TickItemCooldowns(champion, elapsed);
            RemoveExpiredBuffs(champion, now);
            if (champion.clone && !champion.cloneExpired &&
                champion.cloneExpiresAt > 0.0f &&
                champion.cloneExpiresAt <= now) {
                champion.cloneExpired = true;
                champion.visible = false;
                champion.dead = true;
            }
            if (champion.inStasis &&
                champion.stasisEndsAt > 0.0f &&
                champion.stasisEndsAt <= now) {
                champion.inStasis = false;
                champion.stasisSource = StasisSource::None;
                champion.stasisEndsAt = 0.0f;
            }
        });

        for (std::size_t i = 0; i < store_.Threats().Size(); ++i) {
            auto& threat = store_.Threats().At(i);
            if (threat.endAt > 0.0f && threat.endAt <= now) threat.visible = false;
        }
        for (std::size_t i = 0; i < store_.Wards().Size(); ++i) {
            auto& ward = store_.Wards().At(i);
            if (ward.bonusVisionUntil > 0.0f &&
                ward.bonusVisionUntil <= now) {
                ward.bonusVisionUntil = 0.0f;
                ward.bonusVisionObserved = false;
                if (ward.faelight) ward.radius /= 1.25f;
            }
            if (!ward.destroyed && ward.expiresAt > 0.0f &&
                ward.expiresAt <= now) {
                ward.destroyed = true;
                ward.destroyedAt = ward.expiresAt;
                ward.visible = false;
                ward.faelight = false;
                ward.bonusVisionObserved = false;
                ward.evidence = { Provenance::Estimated, Confidence::Medium,
                                  now, 0.0f, HashId("timer.ward-expiry") };
            }
        }
        for (std::size_t i = 0; i < store_.Objectives().Size(); ++i) {
            auto& objective = store_.Objectives().At(i);
            if ((objective.status == ObjectiveStatus::Dead ||
                 objective.status == ObjectiveStatus::Respawning) &&
                objective.respawnAt > 0.0f) {
                objective.status = objective.respawnAt <= now
                    ? ObjectiveStatus::AliveUnknown
                    : ObjectiveStatus::Respawning;
                if (objective.status == ObjectiveStatus::AliveUnknown) {
                    objective.evidence = { Provenance::Estimated, Confidence::Medium,
                                           now, 0.0f, HashId("timer.objective-respawn") };
                }
            }
            if (objective.status == ObjectiveStatus::NotSpawned &&
                objective.spawnAt > 0.0f && objective.spawnAt > now &&
                objective.spawnAt - now <= 15.0f) {
                objective.status = ObjectiveStatus::SpawningSoon;
            }
            if (objective.status == ObjectiveStatus::SpawningSoon) {
                if (objective.spawnAt > now && objective.spawnAt - now <= 15.0f) {
                    AlertState alert{};
                    alert.id = static_cast<std::uint32_t>(objective.kind) ^
                               HashId("alert.objective-spawning");
                    alert.source = objective.networkId;
                    alert.priority = 45;
                    alert.at = now;
                    alert.expiresAt = objective.spawnAt + 2.0f;
                    alert.cooldownUntil = objective.spawnAt + 5.0f;
                    alert.confidence = objective.evidence.confidence;
                    CopyText(alert.title, "Objective spawning soon");
                    CopyText(alert.detail,
                             "Timer entered the fifteen-second spawn window.");
                    RaiseAlert(alert);
                }
                if (objective.spawnAt > 0.0f && objective.spawnAt <= now) {
                    objective.status = objective.visible
                        ? ObjectiveStatus::AliveVisible
                        : ObjectiveStatus::AliveUnknown;
                }
            }
        }
        for (std::size_t i = 0; i < store_.Jungles().Size(); ++i) {
            auto& camp = store_.Jungles().At(i);
            if (!camp.alive && camp.respawnAt > 0.0f && camp.respawnAt <= now) {
                camp.alive = true;
                camp.visible = false;
                camp.confidence = Confidence::Medium;
                camp.evidence = { Provenance::Estimated, Confidence::Medium,
                                  now, 0.0f, HashId("timer.camp-respawn") };
                camp.respawnAt = 0.0f;
                camp.observedDeath = false;
            }
        }
        for (std::size_t i = 0; i < store_.Alerts().Size(); ++i) {
            auto& alert = store_.Alerts().At(i);
            if (alert.expiresAt > 0.0f && alert.expiresAt <= now) alert.priority = 0;
        }
        // Pruning scans and compacts every fixed ring. Running it once per
        // second is enough because individual records already carry expiry
        // flags and visibility checks.
        if (lastPruneAt_ < 0.0f || now < lastPruneAt_ ||
            now - lastPruneAt_ >= 1.0f) {
            store_.PruneInvalid(now);
            lastPruneAt_ = now;
        }
    }
    void RaiseAlert(const AlertState& alert) {
        if (alert.id == 0) return;
        bool accepted = false;
        UpsertBy(store_.Alerts(), alert.id,
                 [&](AlertState& current) {
                     if (current.cooldownUntil > lastNow_ &&
                         alert.priority <= current.priority) {
                         return;
                     }
                     current = alert;
                     accepted = true;
                 },
                 [](const AlertState& value) {
                     return value.id;
                 });
        if (accepted) {
            decisionLog_.Add(
                alert.at, alert.title, "alert raised",
                alert.detail, alert.priority, alert.confidence);
        }
    }

    void ObserveWave(const WaveObservation& observation) noexcept {
        store_.Wave() = WaveAnalysisService::Analyze(observation);
    }

    void RefreshInsights() noexcept {
        AwarenessInsightService::Refresh(store_, insights_, lastNow_);
    }

    PatchRegistry& Registry() noexcept { return registry_; }
    const PatchRegistry& Registry() const noexcept { return registry_; }
    StateStore& Store() noexcept { return store_; }
    const StateStore& Store() const noexcept { return store_; }
    EventBus& Bus() noexcept { return eventBus_; }
    const EventBus& Bus() const noexcept { return eventBus_; }
    DecisionLog& Log() noexcept { return decisionLog_; }
    const DecisionLog& Log() const noexcept { return decisionLog_; }
    ActionArbiter& Arbiter() noexcept { return arbiter_; }
    const ActionArbiter& Arbiter() const noexcept { return arbiter_; }
    GameClock& Clock() noexcept { return clock_; }
    float Now() const noexcept { return lastNow_; }
    AwarenessInsightState& Insights() noexcept { return insights_; }
    const AwarenessInsightState& Insights() const noexcept {
        return insights_;
    }
    static void MarkChampionLastSeen(ChampionState& state) noexcept {
        const auto mark = [](auto& value) {
            if (value.evidence.provenance == Provenance::VisibleNow) {
                value.evidence.provenance = Provenance::LastSeen;
                value.evidence.confidence = Confidence::High;
            }
        };
        mark(state.health);
        mark(state.maxHealth);
        mark(state.mana);
        mark(state.maxMana);
        mark(state.level);
        if (state.roleQuest.evidence.provenance == Provenance::VisibleNow) {
            state.roleQuest.evidence.provenance = Provenance::LastSeen;
            state.roleQuest.evidence.confidence = Confidence::High;
        }
        for (std::size_t i = 0; i < state.itemCount; ++i) {
            if (state.items[i].evidence.provenance == Provenance::VisibleNow) {
                state.items[i].evidence.provenance = Provenance::LastSeen;
                state.items[i].evidence.confidence = Confidence::High;
            }
        }
        for (std::size_t i = 0; i < state.spellCount; ++i) {
            if (state.spells[i].evidence.provenance == Provenance::VisibleNow) {
                state.spells[i].evidence.provenance = Provenance::LastSeen;
                state.spells[i].evidence.confidence = Confidence::High;
            }
            if (!state.visible &&
                (state.spells[i].ready ||
                 state.spells[i].cooldownRemaining <= 0.0f)) {
                state.spells[i].ready = false;
                state.spells[i].cooldownKind = CooldownKind::Unknown;
                state.spells[i].evidence.confidence = Confidence::Low;
            } else if (!state.visible && state.spells[i].resetOnTakedown) {
                state.spells[i].cooldownKind = CooldownKind::RangeEstimated;
                state.spells[i].cooldownMin = 0.0f;
            }
        }
        for (std::size_t i = 0; i < state.buffCount; ++i) {
            if (state.buffs[i].evidence.provenance == Provenance::VisibleNow) {
                state.buffs[i].evidence.provenance = Provenance::LastSeen;
                state.buffs[i].evidence.confidence = Confidence::High;
            }
        }
    }

    static Confidence ConfidenceForLastSeenAge(float age) noexcept {
        if (age <= 8.0f) return Confidence::High;
        if (age <= 20.0f) return Confidence::Medium;
        return Confidence::Low;
    }

    static void DegradeLastSeenEvidence(ChampionState& state) noexcept {
        const Confidence confidence = ConfidenceForLastSeenAge(
            state.visibilityAge);
        const auto degrade = [confidence](auto& value) {
            if (value.evidence.provenance == Provenance::LastSeen) {
                value.evidence.confidence = confidence;
            }
        };
        degrade(state.health);
        degrade(state.maxHealth);
        degrade(state.mana);
        degrade(state.maxMana);
        degrade(state.level);
        if (state.roleQuest.evidence.provenance == Provenance::LastSeen) {
            state.roleQuest.evidence.confidence = confidence;
        }
        for (std::size_t i = 0; i < state.itemCount; ++i) {
            if (state.items[i].evidence.provenance == Provenance::LastSeen) {
                state.items[i].evidence.confidence = confidence;
            }
        }
        for (std::size_t i = 0; i < state.spellCount; ++i) {
            auto& spell = state.spells[i];
            if (spell.evidence.provenance == Provenance::LastSeen) {
                spell.evidence.confidence = confidence;
            }
            if (!state.visible && spell.cooldownRemaining <= 0.0f) {
                spell.ready = false;
                spell.cooldownKind = CooldownKind::Unknown;
                spell.evidence.confidence = Confidence::Low;
            }
        }
        for (std::size_t i = 0; i < state.buffCount; ++i) {
            if (state.buffs[i].evidence.provenance == Provenance::LastSeen) {
                state.buffs[i].evidence.confidence = confidence;
            }
        }
    }

    static void TickSpellCooldowns(ChampionState& champion,
                                   float elapsed,
                                   float now) noexcept {
        for (std::size_t i = 0; i < champion.spellCount; ++i) {
            auto& spell = champion.spells[i];
            const float before = spell.cooldownRemaining;
            spell.cooldownRemaining =
                std::max(0.0f, before - elapsed);
            spell.cooldownMin =
                std::max(0.0f, spell.cooldownMin - elapsed);
            spell.cooldownMax =
                std::max(0.0f, spell.cooldownMax - elapsed);
            if (spell.maxCharges > 1 && before > 0.0f &&
                spell.cooldownRemaining <= 0.0f &&
                spell.charges < spell.maxCharges) {
                ++spell.charges;
                if (spell.charges < spell.maxCharges &&
                    spell.rechargeDuration > 0.0f) {
                    spell.cooldownRemaining =
                        spell.rechargeDuration;
                    spell.cooldownMin = spell.rechargeDuration;
                    spell.cooldownMax = spell.rechargeDuration;
                }
            }
            const bool observedReady = spell.maxCharges > 1
                ? spell.charges > 0
                : spell.cooldownRemaining <= 0.0f;
            spell.rechargeReadyAt =
                spell.maxCharges > 1 &&
                spell.cooldownRemaining > 0.0f
                    ? now + spell.cooldownRemaining
                    : 0.0f;
            if (champion.visible) {
                spell.ready = observedReady;
            } else if (spell.cooldownRemaining <= 0.0f) {
                spell.ready = false;
                spell.cooldownKind = CooldownKind::Unknown;
                spell.evidence.confidence = Confidence::Low;
            }
        }
    }

    static void TickItemCooldowns(ChampionState& champion, float elapsed) noexcept {
        for (std::size_t i = 0; i < champion.itemCount; ++i) {
            auto& item = champion.items[i];
            item.cooldownRemaining = std::max(
                0.0f, item.cooldownRemaining - elapsed);
            if (item.cooldownRemaining <= 0.0f) item.usable = true;
        }
    }

    static void RemoveExpiredBuffs(ChampionState& champion, float now) noexcept {
        std::size_t index = 0;
        while (index < champion.buffCount) {
            const auto& buff = champion.buffs[index];
            if (buff.endTime <= 0.0f || buff.endTime > now) {
                ++index;
                continue;
            }
            champion.buffs[index] = champion.buffs[champion.buffCount - 1];
            --champion.buffCount;
        }
    }

private:
    template <typename Record, std::size_t Capacity, typename UpdateFn>
    static void Upsert(FixedRing<Record, Capacity>& ring, std::uint32_t key,
                       UpdateFn&& update) {
        UpsertBy(ring, key, std::forward<UpdateFn>(update),
                 [](const Record& record) {
                     return static_cast<std::uint32_t>(record.networkId);
                 });
    }

    template <typename Record, std::size_t Capacity, typename UpdateFn, typename KeyFn>
    static void UpsertBy(FixedRing<Record, Capacity>& ring, std::uint32_t key,
                         UpdateFn&& update, KeyFn&& keyFn) {
        for (std::size_t i = 0; i < ring.Size(); ++i) {
            if (keyFn(ring.At(i)) == key) {
                update(ring.At(i));
                return;
            }
        }
        Record value{};
        update(value);
        ring.Push(value);
    }

    void ResetSessionState() {
        store_.Reset();
        decisionLog_.Clear();
        arbiter_.Clear();
        insights_.Reset();
        lastTimerAt_ = -1.0f;
        lastPruneAt_ = -1.0f;
    }

    void ReconcileAfterReconnect() {
        store_.ForEachChampion([&](ChampionState& state) {
            state.lastObservedAt = -1.0f;
            if (state.visible && !state.local) {
                MarkChampionLastSeen(state);
                state.visible = false;
            }
            for (std::size_t i = 0; i < state.spellCount; ++i) {
                ObservedSpell& spell = state.spells[i];
                if (spell.cooldownKind == CooldownKind::ExactObserved) {
                    spell.cooldownKind = CooldownKind::RangeEstimated;
                    spell.cooldownMin = std::max(
                        0.0f, spell.cooldownRemaining -
                                  clock_.PingSeconds());
                    spell.cooldownMax =
                        spell.cooldownRemaining + clock_.PingSeconds();
                    spell.ready = false;
                    spell.evidence.provenance = Provenance::LastSeen;
                    spell.evidence.confidence = Confidence::Medium;
                }
            }
        });
    }

    std::uint32_t FindRecreatedChampion(
        const ChampionObservation& observation) const noexcept {
        if (observation.clone || !observation.name[0]) return 0;
        std::uint32_t bestId = 0;
        float bestAt = -1.0f;
        store_.ForEachChampion([&](const ChampionState& state) {
            if (state.clone || state.networkId == observation.networkId ||
                state.team != observation.team || !state.name[0] ||
                !TextEqualsInsensitive(state.name, observation.name)) {
                return;
            }
            if (state.visible && state.lastObservedAt >= lastNow_) return;
            if (state.lastObservedAt > bestAt) {
                bestAt = state.lastObservedAt;
                bestId = state.networkId;
            }
        });
        return bestId;
    }

    static bool IsMultiFormChampion(std::string_view champion) noexcept {
        static constexpr std::array<std::string_view, 12> kChampions = {
            "Nidalee", "Elise", "Jayce", "Gnar", "Shyvana", "Kled",
            "RekSai", "Udyr", "Aphelios", "KSante", "Kayle", "Aurora"
        };
        for (const std::string_view value : kChampions) {
            if (TextEqualsInsensitive(champion, value)) return true;
        }
        return false;
    }

    static const ObservedSpell* FindSpellBySlot(
        const std::array<ObservedSpell, 8>& spells,
        std::size_t count,
        int slot) noexcept {
        for (std::size_t i = 0;
             i < count && i < spells.size(); ++i) {
            if (spells[i].slot == slot) return &spells[i];
        }
        return nullptr;
    }

    static std::uint64_t ComputeFormSignature(
        const std::array<ObservedSpell, 8>& spells,
        std::size_t count) noexcept {
        std::uint64_t signature = 1469598103934665603ull;
        for (std::size_t i = 0;
             i < count && i < spells.size(); ++i) {
            if (spells[i].slot < 0 || spells[i].slot > 3) continue;
            signature ^= static_cast<std::uint64_t>(
                spells[i].idHash + 31u *
                static_cast<std::uint32_t>(spells[i].slot + 1));
            signature *= 1099511628211ull;
        }
        return signature;
    }

    static CloneKind ClassifyClone(
        const ChampionState& state) noexcept {
        if (!state.clone) return CloneKind::None;
        const auto matches = [&](std::string_view needle) {
            if (TextContainsInsensitive(state.activeFormId, needle) ||
                TextContainsInsensitive(state.baseChampionId, needle)) {
                return true;
            }
            for (std::size_t i = 0; i < state.buffCount; ++i) {
                if (TextContainsInsensitive(
                        state.buffs[i].name, needle)) {
                    return true;
                }
            }
            return false;
        };
        if (matches("shaco")) return CloneKind::ShacoHallucination;
        if (matches("leblanc")) return CloneKind::LeblancMirror;
        if (matches("neeko")) return CloneKind::NeekoTrick;
        if (matches("monkeyking") || matches("wukong")) {
            return CloneKind::WukongDecoy;
        }
        return CloneKind::Unknown;
    }

    static float CloneLifetime(CloneKind kind) noexcept {
        switch (kind) {
        case CloneKind::ShacoHallucination: return 18.0f;
        case CloneKind::LeblancMirror: return 8.0f;
        case CloneKind::NeekoTrick: return 3.0f;
        case CloneKind::WukongDecoy: return 3.25f;
        default: return 5.0f;
        }
    }

    static bool HasItem(const ChampionState& state,
                        SDK::ItemId itemId) noexcept {
        for (std::size_t i = 0; i < state.itemCount; ++i) {
            if (state.items[i].itemId == itemId) return true;
        }
        return false;
    }

    static bool BuffMatches(const ChampionState& state,
                            std::string_view needle) noexcept {
        for (std::size_t i = 0; i < state.buffCount; ++i) {
            if (TextContainsInsensitive(
                    state.buffs[i].name, needle)) {
                return true;
            }
        }
        return false;
    }

    static float MatchingBuffEnd(const ChampionState& state) noexcept {
        float endAt = 0.0f;
        for (std::size_t i = 0; i < state.buffCount; ++i) {
            const ObservedBuff& buff = state.buffs[i];
            if (TextContainsInsensitive(buff.name, "stasis") ||
                TextContainsInsensitive(buff.name, "golden") ||
                TextContainsInsensitive(buff.name, "zhonya") ||
                TextContainsInsensitive(buff.name, "bardr") ||
                TextContainsInsensitive(buff.name, "lissandra") ||
                TextContainsInsensitive(buff.name, "guardianangel") ||
                TextContainsInsensitive(buff.name, "chronorevive")) {
                endAt = std::max(endAt, buff.endTime);
            }
        }
        return endAt;
    }

    static StasisSource ResolveStasisSource(
        const ChampionState& state) noexcept {
        if (BuffMatches(state, "bardr")) return StasisSource::Bard;
        if (BuffMatches(state, "lissandra")) {
            return StasisSource::Lissandra;
        }
        if (BuffMatches(state, "guardianangel")) {
            return StasisSource::GuardianAngel;
        }
        if (BuffMatches(state, "zhonya")) return StasisSource::Zhonya;
        const bool generic =
            BuffMatches(state, "stasis") ||
            BuffMatches(state, "golden");
        if (!generic) return StasisSource::None;
        if (HasItem(state, SDK::ItemId::Zhonya_s_Hourglass)) {
            return StasisSource::Zhonya;
        }
        if (HasItem(state, SDK::ItemId::Seeker_s_Armguard)) {
            return StasisSource::Seeker;
        }
        return StasisSource::Unknown;
    }

    static ReviveSource ResolveReviveSource(
        const ChampionState& state) noexcept {
        if (BuffMatches(state, "chronoshift") ||
            BuffMatches(state, "zilean")) {
            return ReviveSource::Zilean;
        }
        if (BuffMatches(state, "renata") ||
            BuffMatches(state, "bailout")) {
            return ReviveSource::Renata;
        }
        if (BuffMatches(state, "aniviarebirth")) {
            return ReviveSource::Anivia;
        }
        if (BuffMatches(state, "zacrebirth") ||
            BuffMatches(state, "zacpassive")) {
            return ReviveSource::Zac;
        }
        if (BuffMatches(state, "guardianangel") ||
            HasItem(state, SDK::ItemId::Guardian_Angel)) {
            return ReviveSource::GuardianAngel;
        }
        return ReviveSource::None;
    }

    void ReconcileSpecialState(
        ChampionState& state,
        const RoleQuestState& previousRoleQuest,
        bool wasPossession,
        Evidence evidence) {
        state.cloneKind = ClassifyClone(state);
        if (state.clone) {
            if (state.cloneKind == CloneKind::None) {
                state.cloneKind = CloneKind::Unknown;
            }
            if (state.cloneExpiresAt <= lastNow_ ||
                state.cloneExpired) {
                state.cloneExpiresAt =
                    lastNow_ + CloneLifetime(state.cloneKind);
            }
            for (std::size_t i = 0; i < state.buffCount; ++i) {
                state.cloneExpiresAt = std::max(
                    state.cloneExpiresAt, state.buffs[i].endTime);
            }
            state.cloneExpired = false;
        } else {
            state.cloneKind = CloneKind::None;
            state.cloneExpired = false;
            state.cloneExpiresAt = 0.0f;
        }

        if (TextEqualsInsensitive(state.baseChampionId, "Viego") &&
            !TextEqualsInsensitive(
                state.activeFormId, state.baseChampionId)) {
            state.possession = true;
        }
        if (state.possession && !wasPossession) {
            state.possessionStartedAt = lastNow_;
            ++state.formGeneration;
            state.formChangedAt = lastNow_;
        } else if (!state.possession && wasPossession) {
            ++state.formGeneration;
            state.formChangedAt = lastNow_;
        }

        state.stasisSource = ResolveStasisSource(state);
        state.inStasis = state.stasisSource != StasisSource::None;
        state.stasisEndsAt = state.inStasis
            ? MatchingBuffEnd(state) : 0.0f;

        const ReviveSource observedRevive =
            ResolveReviveSource(state);
        state.reviveSource = observedRevive;
        state.reviveAvailable =
            observedRevive != ReviveSource::None;
        if (state.dead && state.reviveAvailable) {
            state.reviveInProgress = true;
            state.reviveAt = state.stasisEndsAt > lastNow_
                ? state.stasisEndsAt
                : lastNow_ +
                  (observedRevive == ReviveSource::GuardianAngel
                       ? 4.0f : 3.0f);
            if (observedRevive == ReviveSource::GuardianAngel) {
                state.stasisSource = StasisSource::GuardianAngel;
                state.inStasis = true;
                state.stasisEndsAt = state.reviveAt;
            }
        }

        const bool questAdvanced =
            (state.roleQuest.completed &&
             !previousRoleQuest.completed) ||
            (state.roleQuest.rewardObserved &&
             !previousRoleQuest.rewardObserved) ||
            (state.roleQuest.itemId != previousRoleQuest.itemId &&
             previousRoleQuest.itemId != SDK::ItemId::Unknown);
        if (questAdvanced) {
            state.roleQuestUpgradeAt = lastNow_;
            if (TextEqualsInsensitive(state.roleQuest.role, "Top") ||
                TextEqualsInsensitive(state.roleQuest.role, "Jungle")) {
                state.roleQuestSpellUpgrade = true;
            }
            if (TextEqualsInsensitive(state.roleQuest.role, "Bot")) {
                state.bonusInventorySlot = true;
            }
            if (TextEqualsInsensitive(state.roleQuest.role, "Support")) {
                state.supportWardSlot = true;
            }
        }
        state.specialStateEvidence = evidence;
    }

    void HandleTakedownReset(std::uint32_t sourceId) {
        ChampionState* source = store_.FindChampion(sourceId);
        if (!source) return;
        for (std::size_t i = 0; i < source->spellCount; ++i) {
            ObservedSpell& spell = source->spells[i];
            if (!spell.resetOnTakedown) continue;
            spell.cooldownRemaining = 0.0f;
            spell.cooldownMin = 0.0f;
            spell.cooldownMax = 0.0f;
            spell.ready = true;
            spell.resetObserved = true;
            spell.cooldownKind = CooldownKind::ExactObserved;
            spell.evidence = {
                Provenance::ObservedEvent, Confidence::Confirmed,
                lastNow_, 0.0f, HashId("event.takedown-reset")
            };
        }
    }

    std::uint32_t InferTakedownSource(
        std::uint32_t targetId) const noexcept {
        const auto& damage = store_.Damage();
        for (std::size_t offset = 0; offset < damage.Size(); ++offset) {
            const DamageRecord& record =
                damage.At(damage.Size() - 1 - offset);
            if (record.targetId != targetId ||
                record.sourceId == 0) {
                continue;
            }
            const float at = record.at > 0.0f
                ? record.at : lastNow_;
            if (lastNow_ - at <= 10.0f) return record.sourceId;
            break;
        }
        return 0;
    }

    static std::uint64_t ComputeInventorySignature(const ChampionObservation& observation) noexcept {
        std::uint64_t signature = 1469598103934665603ull;
        for (std::size_t i = 0; i < observation.itemCount && i < observation.items.size(); ++i) {
            signature ^= static_cast<std::uint64_t>(observation.items[i].Id() + 31 * observation.items[i].slot);
            signature *= 1099511628211ull;
        }
        return signature;
    }

    static std::uint64_t ComputeSummonerSignature(const ChampionObservation& observation) noexcept {
        std::uint64_t signature = 1099511628211ull;
        for (std::size_t i = 0; i < observation.spellCount && i < observation.spells.size(); ++i) {
            if (observation.spells[i].slot < 4) continue;
            signature ^= observation.spells[i].idHash + static_cast<std::uint32_t>(observation.spells[i].slot);
            signature *= 1469598103934665603ull;
        }
        return signature;
    }

    void CopyItems(ChampionState& state, const ChampionObservation& observation, Evidence evidence) {
        state.itemCount = std::min(observation.itemCount, state.items.size());
        for (std::size_t i = 0; i < state.itemCount; ++i) {
            state.items[i] = observation.items[i];
            state.items[i].evidence = evidence;
        }
    }
    void CopySpells(ChampionState& state,
                    const ChampionObservation& observation,
                    Evidence evidence) {
        const std::array<ObservedSpell, 8> previous = state.spells;
        const std::size_t previousCount = state.spellCount;
        const std::uint64_t previousFormSignature =
            state.formSignature;
        state.spellCount =
            std::min(observation.spellCount, state.spells.size());
        for (std::size_t i = 0; i < state.spellCount; ++i) {
            ObservedSpell output = observation.spells[i];
            const ObservedSpell* prior =
                FindSpellBySlot(previous, previousCount, output.slot);
            const bool changed = prior && prior->idHash != 0 &&
                                 output.idHash != 0 &&
                                 prior->idHash != output.idHash;
            output.previousIdHash = changed ? prior->idHash : 0;
            output.evidence = evidence;
            if (const SpellDefinition* definition =
                    registry_.FindSpell(output.idHash)) {
                output.resetOnTakedown =
                    output.resetOnTakedown ||
                    definition->resetOnTakedown;
                output.maxCharges = std::max(
                    output.maxCharges, definition->maxCharges);
            }
            if (output.maxCharges > 1 &&
                output.cooldownRemaining > 0.0f) {
                output.rechargeReadyAt =
                    lastNow_ + output.cooldownRemaining;
            }
            if (TextEqualsInsensitive(
                    state.baseChampionId, "Sylas") &&
                output.slot == 3 && output.name[0] &&
                !TextContainsInsensitive(output.name, "sylasr")) {
                output.stolen = true;
                output.origin = SpellOrigin::StolenUltimate;
            } else if (TextEqualsInsensitive(
                           state.baseChampionId, "Zoe") &&
                       output.slot == 1 && changed) {
                output.origin = SpellOrigin::AcquiredSummoner;
            } else if (output.slot >= 4 && changed) {
                const SummonerDefinition* before =
                    registry_.ResolveSummoner(
                        prior->idHash, prior->name);
                const SummonerDefinition* after =
                    registry_.ResolveSummoner(
                        output.idHash, output.name);
                const bool evolved =
                    before && after &&
                    before->capability == after->capability &&
                    (after->capability == Capability::Teleport ||
                     after->capability == Capability::Smite);
                output.origin = evolved
                    ? SpellOrigin::EvolvedSummoner
                    : SpellOrigin::SwappedSummoner;
                if (evolved) state.roleQuestSpellUpgrade = true;
            } else if (changed && output.slot >= 0 &&
                       output.slot < 4 &&
                       IsMultiFormChampion(
                           state.baseChampionId)) {
                output.origin = SpellOrigin::FormScoped;
            }
            state.spells[i] = output;
        }
        state.formSignature =
            ComputeFormSignature(state.spells, state.spellCount);
        if (previousFormSignature != 0 &&
            state.formSignature != previousFormSignature &&
            IsMultiFormChampion(state.baseChampionId)) {
            ++state.formGeneration;
            state.formChangedAt = lastNow_;
        }
    }
    void CopyBuffs(ChampionState& state, const ChampionObservation& observation, Evidence evidence) {
        state.buffCount = std::min(observation.buffCount, state.buffs.size());
        for (std::size_t i = 0; i < state.buffCount; ++i) {
            state.buffs[i] = observation.buffs[i];
            state.buffs[i].evidence = evidence;
        }
    }

    void PublishPurchaseInsight(ChampionState& state,
                                SDK::ItemId purchased,
                                bool afterReappearance) {
        state.lastPurchasedItem = purchased;
        state.lastPurchaseAt = lastNow_;
        ++state.purchaseCount;
        const auto* item = EconomyInsightService::ItemData(purchased);
        const char* itemName =
            item && !item->Name.empty() ? item->Name.data() : "item";
        if (!state.enemy) return;

        AlertState alert{};
        alert.id = state.networkId ^ HashId(
            EconomyInsightService::IsPowerSpike(purchased)
                ? "alert.item-spike" : "alert.enemy-purchase");
        alert.source = state.networkId;
        alert.priority = EconomyInsightService::IsPowerSpike(purchased)
            ? 60 : 35;
        alert.at = lastNow_;
        alert.expiresAt = lastNow_ + 8.0f;
        alert.cooldownUntil = lastNow_ + 5.0f;
        alert.confidence = Confidence::Confirmed;
        CopyText(alert.title,
                 EconomyInsightService::IsPowerSpike(purchased)
                     ? "Visible item power spike"
                     : "Enemy purchase observed");
        char detail[160] = {};
        std::snprintf(
            detail, sizeof(detail), "%s%s %s.",
            state.name[0] ? state.name : "Enemy",
            afterReappearance ? " returned with" : " acquired",
            itemName);
        CopyText(alert.detail, detail);
        RaiseAlert(alert);
    }

    void PublishLevelSpike(const ChampionState& state) {
        if (!state.enemy) return;
        AlertState alert{};
        alert.id = state.networkId ^ HashId("alert.level-spike") ^
                   static_cast<std::uint32_t>(state.level.value);
        alert.source = state.networkId;
        alert.priority = state.level.value == 6 ? 60 : 45;
        alert.at = lastNow_;
        alert.expiresAt = lastNow_ + 7.0f;
        alert.cooldownUntil = lastNow_ + 1.0f;
        alert.confidence = Confidence::Confirmed;
        CopyText(alert.title, "Visible level power spike");
        char detail[160] = {};
        std::snprintf(
            detail, sizeof(detail), "%s reached level %d.",
            state.name[0] ? state.name : "Enemy",
            state.level.value);
        CopyText(alert.detail, detail);
        RaiseAlert(alert);
    }

    void PublishVisibility(const ChampionState& state, bool visible) {
        GameEvent event{};
        event.type = EventType::VisibilityChanged;
        event.at = lastNow_;
        event.objectId = state.networkId;
        event.value = visible ? 1 : 0;
        event.visibleAtEvent =
            visible || state.ally || state.local;
        event.enemy = state.enemy;
        event.ally = state.ally;
        event.position = state.lastSeenPosition;
        eventBus_.Publish(event);
        store_.TeamfightTimeline().Push(event);
        if (visible || !state.enemy || state.clone) return;

        const ChampionState* nearest = nullptr;
        float nearestDistance = 100000.0f;
        store_.ForEachChampion([&](const ChampionState& candidate) {
            if ((!candidate.ally && !candidate.local) ||
                candidate.dead || !candidate.visible ||
                !candidate.position.IsValid()) {
                return;
            }
            const float distance =
                candidate.position.Distance(state.lastSeenPosition);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearest = &candidate;
            }
        });

        AlertState alert{};
        alert.id = state.networkId ^ HashId("alert.enemy-missing");
        alert.priority = 35;
        alert.at = lastNow_;
        alert.expiresAt = lastNow_ + 8.0f;
        alert.cooldownUntil = lastNow_ + 12.0f;
        alert.confidence = state.health.evidence.IsKnown()
            ? Confidence::High : Confidence::Low;
        alert.source = state.networkId;

        if (nearest && nearestDistance <= 8000.0f) {
            const float speed = std::max(325.0f, state.lastSeenSpeed);
            const float earliest = std::max(
                0.0f, (nearestDistance - 900.0f) / speed);
            const float latest = earliest + 4.0f +
                (state.pathBranches > 1 ? 2.0f : 0.0f);
            const Point3 route = Normalize2D(
                nearest->position - state.lastSeenPosition);
            const float alignment =
                route.x * state.lastDirection.x +
                route.z * state.lastDirection.z;
            if (alignment < 0.2f) {
                alert.confidence = Confidence::Medium;
            }
            bool wardedRoute = false;
            const Point3 midpoint =
                state.lastSeenPosition +
                (nearest->position - state.lastSeenPosition) * 0.5f;
            for (std::size_t i = 0; i < store_.Wards().Size(); ++i) {
                const WardState& ward = store_.Wards().At(i);
                if (!ward.ally || ward.destroyed) continue;
                if (ward.position.Distance(midpoint) <=
                    std::max(900.0f, ward.radius)) {
                    wardedRoute = true;
                    break;
                }
            }
            alert.priority = earliest <= 12.0f ? 70 : 50;
            CopyText(alert.title, "Possible gank route");
            char detail[160] = {};
            std::snprintf(
                detail, sizeof(detail),
                "%s left vision; nearest ally %s in %.0f-%.0fs%s.",
                state.name[0] ? state.name : "Enemy",
                nearest->name[0] ? nearest->name : "ally",
                earliest, latest,
                wardedRoute ? "; route warded" : "; route unwarded");
            CopyText(alert.detail, detail);
        } else {
            CopyText(alert.title, "Enemy missing");
            CopyText(
                alert.detail,
                "Observed vision loss; reachable area remains an estimate.");
        }
        RaiseAlert(alert);
    }

    static bool IsValidPathPoint(const Point3& point) noexcept {
        return point.IsValid() && !point.IsZero();
    }

    static void ClearPathTarget(ChampionState& state) noexcept {
        state.pathStartPosition = {};
        state.pathTargetPosition = {};
        state.pathUpdatedAt = 0.0f;
        state.pathExpectedArrivalAt = 0.0f;
        state.pathNodeCount = 0;
        state.pathEvidence = {};
    }

    bool IsJungleCandidate(const ChampionState& state) const noexcept {
        if (std::strcmp(state.roleQuest.role, "Jungle") == 0) return true;
        for (std::size_t i = 0; i < state.spellCount; ++i) {
            const SummonerDefinition* definition =
                registry_.ResolveSummoner(
                    state.spells[i].idHash, state.spells[i].name);
            if (definition && definition->capability == Capability::Smite) {
                return true;
            }
        }
        return false;
    }

    void InferCampFromNeutralCs(const ChampionState& jungler,
                                float csDelta) {
        JungleCampState* best = nullptr;
        float bestDistance = 5000.0f;
        for (std::size_t i = 0; i < store_.Jungles().Size(); ++i) {
            JungleCampState& camp = store_.Jungles().At(i);
            if (camp.visible || !camp.alive ||
                !camp.evidence.IsKnown() ||
                !camp.position.IsValid()) {
                continue;
            }
            const float distance = camp.position.Distance(jungler.position);
            if (distance < bestDistance) {
                bestDistance = distance;
                best = &camp;
            }
        }
        if (!best) return;

        best->alive = false;
        best->observedDeath = false;
        best->estimatedDeathAt = lastNow_;
        best->sourceJunglerId = jungler.networkId;
        const bool buffCamp =
            TextContainsInsensitive(best->campId, "blue") ||
            TextContainsInsensitive(best->campId, "red") ||
            TextContainsInsensitive(best->campId, "bramble") ||
            TextContainsInsensitive(best->campId, "sentinel");
        const bool swiftplay =
            store_.Ruleset() == RoleQuestRuleset::Swiftplay;
        const float respawnDuration = swiftplay
            ? (buffCamp ? 270.0f : 120.0f)
            : (buffCamp ? 300.0f : 135.0f);
        best->respawnAt = lastNow_ + respawnDuration;
        best->confidence =
            bestDistance <= 1800.0f && csDelta >= 2.0f
                ? Confidence::Medium : Confidence::Low;
        best->evidence = {
            Provenance::Estimated, best->confidence, lastNow_, 0.0f,
            HashId("inference.jungle-cs")
        };
        decisionLog_.Add(
            lastNow_, best->campId[0] ? best->campId : "jungle camp",
            "estimated down",
            "Observed enemy jungler neutral CS increased near this camp.",
            30, best->confidence);
    }

    void PublishUnitDeath(ChampionState& state) {
        state.deathAt = lastNow_;
        if (state.local) {
            insights_.deathRecap = DeathRecapService::Build(
                store_, state.networkId, lastNow_);
        }
        ActivitySample sample{};
        sample.position = state.position.IsValid()
            ? state.position : state.lastSeenPosition;
        sample.at = lastNow_;
        sample.team = state.team;
        sample.kind = ActivityKind::Death;
        sample.evidence = {
            Provenance::ObservedEvent, Confidence::Confirmed,
            lastNow_, 0.0f, HashId("activity.death")
        };
        if (sample.position.IsValid()) insights_.activity.Push(sample);
        GameEvent event{};
        event.type = EventType::UnitDied;
        event.at = lastNow_;
        event.objectId = state.networkId;
        event.visibleAtEvent = state.visible || state.ally || state.local;
        event.enemy = state.enemy;
        event.ally = state.ally;
        event.sourceId = InferTakedownSource(state.networkId);
        eventBus_.Publish(event);
        store_.TeamfightTimeline().Push(event);
        HandleTakedownReset(event.sourceId);
    }

    static ObservedSpell* FindSpell(ChampionState& state, int slot, std::uint32_t hash) {
        for (std::size_t i = 0; i < state.spellCount; ++i) {
            if ((slot >= 0 && state.spells[i].slot == slot) || (hash != 0 && state.spells[i].idHash == hash)) return &state.spells[i];
        }
        if (state.spellCount >= state.spells.size()) return nullptr;
        auto& result = state.spells[state.spellCount++];
        result.slot = slot; result.idHash = hash;
        return &result;
    }

    void ApplyObservedEventReveal(ChampionState& state,
                                  const GameEvent& event) {
        Point3 reveal = event.startPosition;
        if (!reveal.IsValid() || reveal.IsZero()) reveal = event.position;
        if (!reveal.IsValid() || reveal.IsZero()) reveal = event.endPosition;
        const bool hasReveal = reveal.IsValid() && !reveal.IsZero();
        state.observedEventPosition = hasReveal ? reveal : Point3{};
        state.observedEventAt = event.at;
        state.observedEventUntil = event.at + 2.5f;
        const bool hiddenMemoryEvent = !event.visibleAtEvent && event.enemy;
        state.observedEventEvidence = {
            Provenance::ObservedEvent,
            (event.visibleAtEvent || hiddenMemoryEvent)
                ? Confidence::Confirmed : Confidence::High,
            event.at, event.at + 2.5f,
            HashId(hiddenMemoryEvent
                       ? "event.memory-cast-reveal"
                       : "event.cast-reveal")
        };
    }

    void ApplySpellEvent(ChampionState& state, const GameEvent& event) {
        ObservedSpell* spell = FindSpell(state, event.slot, event.nameHash);
        if (!spell) return;
        spell->idHash = event.nameHash != 0 ? event.nameHash : spell->idHash;
        CopyText(spell->name, event.name);
        spell->lastCastAt = event.at;
        const SummonerDefinition* summoner =
            registry_.ResolveSummoner(spell->idHash, spell->name);
        const SpellDefinition* definition =
            registry_.FindSpell(spell->idHash);
        const float baseCooldown = summoner
            ? summoner->cooldown
            : (definition ? definition->cooldown : 0.0f);
        if (baseCooldown > 0.0f) {
            const float haste = summoner
                ? 0.0f : std::max(0.0f, state.abilityHaste);
            const float cooldown =
                baseCooldown * 100.0f / (100.0f + haste);
            const float uncertainty = summoner ? 0.15f : 0.05f;
            spell->cooldownRemaining = cooldown;
            spell->cooldownMin = cooldown * (1.0f - uncertainty);
            spell->cooldownMax = cooldown * (1.0f + uncertainty);
            spell->rechargeDuration = cooldown;
            spell->cooldownKind = CooldownKind::RangeEstimated;
        } else {
            spell->cooldownRemaining = 0.0f;
            spell->cooldownMin = 0.0f;
            spell->cooldownMax = 0.0f;
            spell->rechargeDuration = 0.0f;
            spell->cooldownKind = CooldownKind::Unknown;
        }
        if (summoner) spell->maxCharges = summoner->maxCharges;
        if (summoner && summoner->capability == Capability::Teleport) {
            state.teleporting = true;
            state.channeling = true;
        }
        if (spell->maxCharges > 1 && spell->charges > 0) --spell->charges;
        spell->ready = spell->maxCharges > 1 && spell->charges > 0;
        spell->rechargeReadyAt =
            spell->maxCharges > 1 &&
            spell->cooldownRemaining > 0.0f
                ? event.at + spell->cooldownRemaining
                : 0.0f;
        if (definition) {
            spell->resetOnTakedown = definition->resetOnTakedown;
            state.channeling = definition->channel;
        }
        spell->evidence = {
            Provenance::ObservedEvent,
            baseCooldown > 0.0f ? Confidence::High : Confidence::Medium,
            event.at,
            baseCooldown > 0.0f ? event.at + spell->cooldownMax : 0.0f,
            HashId("sdk.spell-cast")
        };
    }

    void ApplyBuffEvent(ChampionState& state, const GameEvent& event, bool active) {
        for (std::size_t i = 0; i < state.buffCount; ++i) {
            auto& buff = state.buffs[i];
            if (buff.idHash != event.nameHash && std::strcmp(buff.name, event.name) != 0) continue;
            if (!active) {
                state.buffs[i] = state.buffs[state.buffCount - 1];
                --state.buffCount;
            } else {
                buff.stacks = event.value; buff.startTime = event.at; buff.endTime = event.at + event.duration;
            }
            return;
        }
        if (!active || state.buffCount >= state.buffs.size()) return;
        auto& buff = state.buffs[state.buffCount++];
        buff.idHash = event.nameHash; buff.stacks = event.value;
        buff.startTime = event.at; buff.endTime = event.at + event.duration;
        CopyText(buff.name, event.name);
        buff.evidence = { Provenance::ObservedEvent, Confidence::High, event.at, buff.endTime, HashId("sdk.buff") };
    }

    PatchRegistry registry_{};
    StateStore store_{};
    EventBus eventBus_{};
    DecisionLog decisionLog_{};
    ActionArbiter arbiter_{};
    AwarenessInsightState insights_{};
    GameClock clock_{};
    RuntimeMode mode_ = RuntimeMode::Companion;
    float lastNow_ = 0.0f;
    float lastTimerAt_ = -1.0f;
    float lastPruneAt_ = -1.0f;
};

} // namespace NightSharp::Companion
