#pragma once

#include "AwarenessActivatorCore.h"
#include "../../sdk/Data/ItemData.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string_view>
#include <utility>

namespace NightSharp::Companion {

struct WaveObservation {
    int allyMinions = 0;
    int enemyMinions = 0;
    float allyHealth = 0.0f;
    float enemyHealth = 0.0f;
    float allyFront = 0.0f;
    float enemyFront = 0.0f;
    Point3 center{};
    float at = 0.0f;
};

class WaveAnalysisService final {
public:
    static WaveState Analyze(const WaveObservation& observation) noexcept {
        WaveState state{};
        state.allyMinions = std::max(0, observation.allyMinions);
        state.enemyMinions = std::max(0, observation.enemyMinions);
        state.allyHealth = std::max(0.0f, observation.allyHealth);
        state.enemyHealth = std::max(0.0f, observation.enemyHealth);
        state.allyFront = observation.allyFront;
        state.enemyFront = observation.enemyFront;
        state.center = observation.center;
        state.observedAt = observation.at;
        const int observed = state.allyMinions + state.enemyMinions;
        if (observed == 0) {
            CopyText(state.classification, "No visible wave");
            state.evidence = {
                Provenance::ObservedEvent, Confidence::Low,
                observation.at, observation.at + 1.5f,
                HashId("analysis.wave-empty")
            };
            return state;
        }

        const float countBias = static_cast<float>(
            state.allyMinions - state.enemyMinions);
        const float healthBias =
            (state.allyHealth - state.enemyHealth) / 450.0f;
        state.laneBias = countBias + healthBias;
        if (state.laneBias >= 3.5f) {
            CopyText(state.classification, "Fast push allied");
        } else if (state.laneBias >= 1.25f) {
            CopyText(state.classification, "Slow push allied");
        } else if (state.laneBias <= -3.5f) {
            CopyText(state.classification, "Fast push enemy");
        } else if (state.laneBias <= -1.25f) {
            CopyText(state.classification, "Slow push enemy");
        } else if (observed >= 4) {
            CopyText(state.classification, "Freeze estimate");
        } else {
            CopyText(state.classification, "Neutral estimate");
        }
        state.evidence = {
            Provenance::ObservedEvent,
            observed >= 6 ? Confidence::High : Confidence::Medium,
            observation.at, observation.at + 1.5f,
            HashId("analysis.wave-visible")
        };
        return state;
    }
};

struct RecallAdvice {
    bool recommended = false;
    int score = 0;
    float at = 0.0f;
    char reason[192] = {};
    Evidence evidence{};
};

class RecallTimingService final {
public:
    static RecallAdvice Evaluate(const ChampionState& player,
                                 const StateStore& store,
                                 float now) noexcept {
        RecallAdvice advice{};
        advice.at = now;
        if (!player.local || player.dead || !player.visible ||
            player.channeling || !player.health.evidence.IsKnown()) {
            CopyText(advice.reason, "Recall unavailable in current state.");
            return advice;
        }

        const float healthPercent = player.maxHealth.value > 0.0f
            ? player.health.value * 100.0f / player.maxHealth.value
            : 100.0f;
        if (healthPercent <= 35.0f) advice.score += 35;
        else if (healthPercent <= 55.0f) advice.score += 18;
        if (player.currentGold.value >= 1300.0f) advice.score += 30;
        else if (player.currentGold.value >= 800.0f) advice.score += 15;

        const WaveState& wave = store.Wave();
        if (std::string_view(wave.classification).find("push allied") !=
            std::string_view::npos) {
            advice.score += 25;
        } else if (std::string_view(wave.classification).find("enemy") !=
                   std::string_view::npos) {
            advice.score -= 20;
        }

        bool nearbyEnemy = false;
        store.ForEachChampion([&](const ChampionState& champion) {
            if (champion.enemy && champion.visible && !champion.dead &&
                champion.position.Distance(player.position) <= 1800.0f) {
                nearbyEnemy = true;
            }
        });
        if (nearbyEnemy) advice.score -= 45;

        float nextObjective = 9999.0f;
        for (std::size_t i = 0; i < store.Objectives().Size(); ++i) {
            const ObjectiveState& objective = store.Objectives().At(i);
            float eventAt = 0.0f;
            if (objective.status == ObjectiveStatus::Dead ||
                objective.status == ObjectiveStatus::Respawning) {
                eventAt = objective.respawnAt;
            } else if (objective.status == ObjectiveStatus::NotSpawned ||
                       objective.status == ObjectiveStatus::SpawningSoon) {
                eventAt = objective.spawnAt;
            }
            if (eventAt >= now) nextObjective = std::min(nextObjective, eventAt - now);
        }
        if (nextObjective <= 45.0f) advice.score -= 35;
        else if (nextObjective >= 90.0f) advice.score += 10;

        advice.score = std::clamp(advice.score, 0, 100);
        advice.recommended = advice.score >= 55;
        if (nearbyEnemy) {
            CopyText(advice.reason,
                     "Do not recall: a visible enemy is nearby.");
        } else if (nextObjective <= 45.0f) {
            CopyText(advice.reason,
                     "Delay recall: an observed objective window is near.");
        } else if (advice.recommended &&
                   std::string_view(wave.classification).find("push allied") !=
                       std::string_view::npos) {
            CopyText(advice.reason,
                     "Recall window: allied wave is pushing and danger is low.");
        } else if (advice.recommended) {
            CopyText(advice.reason,
                     "Recall window: health or held gold justifies resetting.");
        } else {
            CopyText(advice.reason,
                     "No strong evidence for a safe recall window.");
        }
        advice.evidence = {
            Provenance::ObservedEvent,
            advice.recommended ? Confidence::High : Confidence::Medium,
            now, now + 2.0f, HashId("analysis.recall")
        };
        return advice;
    }
};

class EconomyInsightService final {
public:
    static void ObserveGold(ChampionState& state,
                            float currentGold,
                            float totalGold,
                            const Evidence& evidence) noexcept {
        state.currentGold.Set(std::max(0.0f, currentGold), evidence);
        state.totalGold.Set(std::max(0.0f, totalGold), evidence);
        state.estimatedGoldMin = std::max(0.0f, currentGold);
        state.estimatedGoldMax = std::max(0.0f, currentGold);
        state.lastGoldObservedAt = evidence.observedAt;
        state.goldEstimateEvidence = evidence;
    }

    static void UpdateHiddenEstimate(ChampionState& state,
                                     float now) noexcept {
        if (!state.enemy || state.visible || state.lastGoldObservedAt <= 0.0f) {
            return;
        }
        const float unseen = std::max(0.0f, now - state.lastGoldObservedAt);
        const float passiveIncome = unseen * 2.04f;
        state.estimatedGoldMin = std::max(
            0.0f, state.currentGold.value + passiveIncome);
        state.estimatedGoldMax = state.currentGold.value +
            passiveIncome + unseen * 7.5f;
        if (state.dead || state.recalling) state.estimatedGoldMin = 0.0f;
        state.goldEstimateEvidence = {
            Provenance::Estimated,
            unseen <= 20.0f ? Confidence::Medium : Confidence::Low,
            state.lastGoldObservedAt, 0.0f,
            HashId("estimate.enemy-gold")
        };
    }

    static SDK::ItemId FindNewItem(
        const std::array<ObservedItem, 8>& previous,
        std::size_t previousCount,
        const std::array<ObservedItem, 8>& current,
        std::size_t currentCount) noexcept {
        const std::size_t oldCount = std::min(previousCount, previous.size());
        const std::size_t newCount = std::min(currentCount, current.size());
        for (std::size_t i = 0; i < newCount; ++i) {
            if (current[i].itemId == SDK::ItemId::Unknown) continue;
            int priorCopies = 0;
            int currentCopies = 0;
            for (std::size_t j = 0; j < oldCount; ++j) {
                if (previous[j].itemId == current[i].itemId) ++priorCopies;
            }
            for (std::size_t j = 0; j <= i; ++j) {
                if (current[j].itemId == current[i].itemId) ++currentCopies;
            }
            if (currentCopies > priorCopies) return current[i].itemId;
        }
        return SDK::ItemId::Unknown;
    }

    static const SDK::Data::ItemDatabase::ItemDataEntry* ItemData(
        SDK::ItemId itemId) noexcept {
        return SDK::Data::ItemDatabase::FindById(
            static_cast<int>(SDK::ItemIdValue(itemId)));
    }

    static bool IsPowerSpike(SDK::ItemId itemId) noexcept {
        const auto* item = ItemData(itemId);
        return item && item->InStore && item->PriceTotal >= 2400;
    }
};

struct ObjectiveSetupAssessment {
    ObjectiveKind kind = ObjectiveKind::Unknown;
    int score = 0;
    int nearbyAllies = 0;
    int nearbyEnemies = 0;
    int alliedVision = 0;
    float assessedAt = 0.0f;
    char rating[24] = "Unknown";
    Evidence evidence{};
};

class ObjectiveSetupService final {
public:
    static ObjectiveSetupAssessment Evaluate(
        const ObjectiveState& objective,
        const StateStore& store,
        float now) noexcept {
        ObjectiveSetupAssessment result{};
        result.kind = objective.kind;
        result.assessedAt = now;
        result.score = 50;
        store.ForEachChampion([&](const ChampionState& champion) {
            if (!champion.visible || champion.dead ||
                champion.position.Distance(objective.position) > 2600.0f) {
                return;
            }
            if (champion.ally || champion.local) ++result.nearbyAllies;
            if (champion.enemy) ++result.nearbyEnemies;
        });
        for (std::size_t i = 0; i < store.Wards().Size(); ++i) {
            const WardState& ward = store.Wards().At(i);
            if (ward.ally && !ward.destroyed &&
                ward.position.Distance(objective.position) <= 2200.0f) {
                ++result.alliedVision;
            }
        }
        result.score += result.nearbyAllies * 10;
        result.score -= result.nearbyEnemies * 12;
        result.score += std::min(3, result.alliedVision) * 8;
        if (objective.visible) result.score += 8;
        result.score = std::clamp(result.score, 0, 100);
        CopyText(result.rating,
                 result.score >= 80 ? "Excellent" :
                 result.score >= 65 ? "Good" :
                 result.score >= 45 ? "Contested" : "Poor");
        result.evidence = objective.evidence;
        result.evidence.observedAt = now;
        return result;
    }
};

struct WardEfficiencySummary {
    int alliedObserved = 0;
    int active = 0;
    int destroyed = 0;
    int objectiveCoverage = 0;
    float averageLifetime = 0.0f;
    int score = 0;
    Evidence evidence{};
};

class WardEfficiencyService final {
public:
    static WardEfficiencySummary Evaluate(const StateStore& store,
                                          float now) noexcept {
        WardEfficiencySummary result{};
        float lifetime = 0.0f;
        for (std::size_t i = 0; i < store.Wards().Size(); ++i) {
            const WardState& ward = store.Wards().At(i);
            if (!ward.ally || !ward.evidence.IsKnown()) continue;
            ++result.alliedObserved;
            if (ward.destroyed) ++result.destroyed;
            else ++result.active;
            const float endedAt = ward.destroyedAt > 0.0f
                ? ward.destroyedAt : now;
            lifetime += std::max(0.0f, endedAt - ward.placedAt);
            bool coversObjective = false;
            for (std::size_t j = 0; j < store.Objectives().Size(); ++j) {
                if (ward.position.Distance(
                        store.Objectives().At(j).position) <= 2200.0f) {
                    coversObjective = true;
                    break;
                }
            }
            if (coversObjective) ++result.objectiveCoverage;
        }
        if (result.alliedObserved > 0) {
            result.averageLifetime = lifetime /
                static_cast<float>(result.alliedObserved);
            result.score = std::clamp(
                static_cast<int>(result.averageLifetime * 0.45f) +
                    result.objectiveCoverage * 12,
                0, 100);
            result.evidence = {
                Provenance::ObservedEvent, Confidence::High,
                now, now + 2.0f, HashId("analysis.ward-efficiency")
            };
        }
        return result;
    }
};

enum class ActivityKind : std::uint8_t {
    ChampionSeen = 0,
    Damage,
    Death,
};

struct ActivitySample {
    Point3 position{};
    float at = 0.0f;
    std::uint32_t team = 0;
    ActivityKind kind = ActivityKind::ChampionSeen;
    Evidence evidence{};
};

struct DeathRecapSource {
    std::uint32_t sourceId = 0;
    float damage = 0.0f;
    char name[64] = {};
};

struct DeathRecapSummary {
    float deathAt = 0.0f;
    float totalDamage = 0.0f;
    float firstDamageAt = 0.0f;
    int sourceCount = 0;
    std::array<DeathRecapSource, 3> sources{};
    Evidence evidence{};
};

class DeathRecapService final {
public:
    static DeathRecapSummary Build(const StateStore& store,
                                   std::uint32_t victimId,
                                   float deathAt) noexcept {
        DeathRecapSummary result{};
        result.deathAt = deathAt;
        std::array<DeathRecapSource, 16> aggregate{};
        std::size_t aggregateCount = 0;
        for (std::size_t i = 0; i < store.Damage().Size(); ++i) {
            const DamageRecord& damage = store.Damage().At(i);
            if (damage.targetId != victimId || damage.at > deathAt + 0.1f ||
                damage.at < deathAt - 15.0f) {
                continue;
            }
            result.totalDamage += std::max(0.0f, damage.amount);
            result.firstDamageAt = result.firstDamageAt <= 0.0f
                ? damage.at : std::min(result.firstDamageAt, damage.at);
            std::size_t slot = aggregateCount;
            for (std::size_t j = 0; j < aggregateCount; ++j) {
                if (aggregate[j].sourceId == damage.sourceId) {
                    slot = j;
                    break;
                }
            }
            if (slot == aggregateCount && aggregateCount < aggregate.size()) {
                ++aggregateCount;
                aggregate[slot].sourceId = damage.sourceId;
                CopyText(aggregate[slot].name, damage.source);
            }
            if (slot < aggregateCount) {
                aggregate[slot].damage += std::max(0.0f, damage.amount);
            }
        }
        std::sort(aggregate.begin(), aggregate.begin() + aggregateCount,
                  [](const DeathRecapSource& left,
                     const DeathRecapSource& right) {
                      return left.damage > right.damage;
                  });
        result.sourceCount = static_cast<int>(
            std::min<std::size_t>(result.sources.size(), aggregateCount));
        for (int i = 0; i < result.sourceCount; ++i) {
            result.sources[static_cast<std::size_t>(i)] =
                aggregate[static_cast<std::size_t>(i)];
        }
        if (result.totalDamage > 0.0f) {
            result.evidence = {
                Provenance::ObservedEvent, Confidence::Confirmed,
                deathAt, 0.0f, HashId("analysis.death-recap")
            };
        }
        return result;
    }
};

struct AwarenessInsightState {
    RecallAdvice recall{};
    FixedRing<ObjectiveSetupAssessment, 16> objectiveSetups{};
    WardEfficiencySummary wardEfficiency{};
    DeathRecapSummary deathRecap{};
    FixedRing<ActivitySample, 512> activity{};
    float lastActivitySampleAt = -100.0f;

    void Reset() noexcept { *this = {}; }
};
struct EvidenceVisualStyle {
    std::uint32_t color = 0x88777777u;
    float thickness = 1.0f;
    char marker = '?';
};

class AwarenessPresentationPolicy final {
public:
    static EvidenceVisualStyle StyleFor(
        const Evidence& evidence) noexcept {
        EvidenceVisualStyle style{};
        switch (evidence.provenance) {
        case Provenance::VisibleNow:
            style = { 0xFF55D6FFu, 2.0f, '!' };
            break;
        case Provenance::ObservedEvent:
            style = { 0xFF66EE99u, 1.8f, '+' };
            break;
        case Provenance::LastSeen:
            style = { 0xAA8866FFu, 1.5f, '~' };
            break;
        case Provenance::ManualInput:
            style = { 0xFF66AAFFu, 1.8f, '*' };
            break;
        case Provenance::Estimated:
            style = { 0xBBDD9944u, 1.2f, '?' };
            break;
        default:
            break;
        }
        if (evidence.confidence == Confidence::Low) {
            style.color = 0x88777799u;
            style.thickness = std::min(style.thickness, 1.0f);
        } else if (evidence.confidence == Confidence::Confirmed) {
            style.thickness = std::max(style.thickness, 2.0f);
        }
        return style;
    }

    static std::size_t PrioritizeAlerts(
        const FixedRing<AlertState, 64>& alerts,
        float now,
        std::array<const AlertState*, 64>& output) noexcept {
        output.fill(nullptr);
        std::size_t count = 0;
        for (std::size_t i = 0;
             i < alerts.Size() && count < output.size(); ++i) {
            const AlertState& alert = alerts.At(i);
            if (alert.expiresAt > 0.0f &&
                alert.expiresAt <= now) {
                continue;
            }
            output[count++] = &alert;
        }
        std::sort(
            output.begin(), output.begin() +
                static_cast<std::ptrdiff_t>(count),
            [](const AlertState* left,
               const AlertState* right) {
                if (left->priority != right->priority) {
                    return left->priority > right->priority;
                }
                if (left->at != right->at) {
                    return left->at > right->at;
                }
                return left->id < right->id;
            });
        return count;
    }
};

class PresentationLayerGuard final {
public:
    template <typename Fn>
    bool Run(std::uint32_t layer,
             bool enabled,
             Fn&& draw) noexcept {
        if (!enabled || layer == 0 ||
            (faultMask_ & layer) != 0) {
            return false;
        }
        try {
            std::forward<Fn>(draw)();
            return true;
        } catch (...) {
            faultMask_ |= layer;
            return false;
        }
    }

    void Reset() noexcept { faultMask_ = 0; }
    std::uint32_t FaultMask() const noexcept { return faultMask_; }
    bool IsFaulted(std::uint32_t layer) const noexcept {
        return (faultMask_ & layer) != 0;
    }
private:
    std::uint32_t faultMask_ = 0;
};


class AwarenessInsightService final {
public:
    static void Refresh(StateStore& store,
                        AwarenessInsightState& insights,
                        float now) noexcept {
        ChampionState* player = nullptr;
        store.ForEachChampion([&](ChampionState& champion) {
            EconomyInsightService::UpdateHiddenEstimate(champion, now);
            if (champion.local) player = &champion;
        });
        insights.recall = player
            ? RecallTimingService::Evaluate(*player, store, now)
            : RecallAdvice{};
        insights.wardEfficiency = WardEfficiencyService::Evaluate(store, now);
        insights.objectiveSetups.Clear();
        for (std::size_t i = 0; i < store.Objectives().Size(); ++i) {
            const ObjectiveState& objective = store.Objectives().At(i);
            if (!objective.position.IsValid()) continue;
            insights.objectiveSetups.Push(
                ObjectiveSetupService::Evaluate(objective, store, now));
        }
    }
};

enum class AwarenessLocale : std::uint8_t {
    English = 0,
    Vietnamese,
};

class AwarenessLocalization final {
public:
    static const char* Text(std::string_view key,
                            AwarenessLocale locale) noexcept {
        const bool vi = locale == AwarenessLocale::Vietnamese;
        if (key == "wave") return vi ? "Trạng thái lính" : "Wave";
        if (key == "recall") return vi ? "Thời điểm Biến Về" : "Recall timing";
        if (key == "gold") return vi ? "Ước tính vàng" : "Gold estimate";
        if (key == "ultimate") return vi ? "Chiêu cuối" : "Ultimate";
        if (key == "objective") return vi ? "Chuẩn bị mục tiêu" : "Objective setup";
        if (key == "wards") return vi ? "Hiệu quả mắt" : "Ward efficiency";
        if (key == "death") return vi ? "Tổng kết tử vong" : "Death recap";
        if (key == "ready") return vi ? "sẵn sàng" : "ready";
        if (key == "cooldown") return vi ? "hồi chiêu" : "cooldown";
        if (key == "unknown") return vi ? "không rõ" : "unknown";
        return key.data();
    }
};

enum class AwarenessPreset : std::uint8_t {
    Balanced = 0,
    Top,
    Jungle,
    Mid,
    Bot,
    Support,
    Champion,
};

struct AwarenessPresetProfile {
    bool enemyHud = true;
    bool combat = true;
    bool wards = true;
    bool jungle = true;
    bool objectives = true;
    bool heatmaps = false;
    float defensiveHorizon = 1.25f;
};

class AwarenessPresetService final {
public:
    static AwarenessPresetProfile ForRole(AwarenessPreset preset) noexcept {
        AwarenessPresetProfile profile{};
        switch (preset) {
        case AwarenessPreset::Top:
            profile.jungle = false;
            profile.defensiveHorizon = 1.1f;
            break;
        case AwarenessPreset::Jungle:
            profile.enemyHud = false;
            profile.jungle = true;
            profile.objectives = true;
            profile.heatmaps = true;
            break;
        case AwarenessPreset::Mid:
            profile.wards = true;
            profile.jungle = true;
            profile.defensiveHorizon = 1.5f;
            break;
        case AwarenessPreset::Bot:
            profile.combat = true;
            profile.defensiveHorizon = 1.65f;
            break;
        case AwarenessPreset::Support:
            profile.enemyHud = false;
            profile.wards = true;
            profile.objectives = true;
            profile.heatmaps = true;
            break;
        default:
            break;
        }
        return profile;
    }

    static AwarenessPresetProfile ForChampion(
        std::string_view championId) noexcept {
        AwarenessPresetProfile profile{};
        static constexpr std::array<std::string_view, 10> kSupports = {
            "Janna", "Lulu", "Milio", "Nami", "Rakan",
            "Renata", "Sona", "Soraka", "Thresh", "Yuumi"
        };
        static constexpr std::array<std::string_view, 9> kGlobalUltimates = {
            "Ashe", "Briar", "Draven", "Ezreal", "Gangplank",
            "Jinx", "Karthus", "Nocturne", "Shen"
        };
        if (std::find(kSupports.begin(), kSupports.end(), championId) !=
            kSupports.end()) {
            return ForRole(AwarenessPreset::Support);
        }
        if (std::find(kGlobalUltimates.begin(), kGlobalUltimates.end(),
                      championId) != kGlobalUltimates.end()) {
            profile.enemyHud = true;
            profile.objectives = true;
            profile.heatmaps = true;
            profile.defensiveHorizon = 1.4f;
        }
        return profile;
    }
};

class LocalTelemetryExporter final {
public:
    static bool Export(const char* path,
                       const StateStore& store,
                       const AwarenessInsightState& insights,
                       const DecisionLog& decisions,
                       bool redactIdentities) {
        if (!path || !path[0]) return false;
        std::ofstream output(path, std::ios::out | std::ios::trunc);
        if (!output) return false;
        output << "{\n\"schema\":1,\"now\":" << store.Now()
               << ",\"mode\":" << static_cast<int>(store.Mode())
               << ",\"wave\":{\"classification\":";
        WriteString(output, store.Wave().classification);
        output << ",\"ally\":" << store.Wave().allyMinions
               << ",\"enemy\":" << store.Wave().enemyMinions
               << ",\"confidence\":";
        WriteString(output, ConfidenceName(store.Wave().evidence.confidence));
        output << "},\n\"champions\":[";
        bool first = true;
        store.ForEachChampion([&](const ChampionState& champion) {
            if (!first) output << ',';
            first = false;
            output << "{\"id\":"
                   << (redactIdentities ? 0u : champion.networkId)
                   << ",\"name\":";
            WriteString(output, redactIdentities ? "redacted" : champion.name);
            output << ",\"visible\":"
                   << (champion.visible ? "true" : "false")
                   << ",\"goldMin\":" << champion.estimatedGoldMin
                   << ",\"goldMax\":" << champion.estimatedGoldMax
                   << ",\"goldConfidence\":";
            WriteString(output,
                        ConfidenceName(champion.goldEstimateEvidence.confidence));
            if (champion.visible) {
                output << ",\"position\":[" << champion.position.x << ','
                       << champion.position.y << ',' << champion.position.z
                       << ']';
            } else if (champion.lastSeenAt > 0.0f) {
                output << ",\"lastSeen\":[" << champion.lastSeenPosition.x
                       << ',' << champion.lastSeenPosition.y << ','
                       << champion.lastSeenPosition.z << ']'
                       << ",\"positionProvenance\":\"LastSeen\"";
            }
            output << '}';
        });
        output << "],\n\"alerts\":[";
        for (std::size_t i = 0; i < store.Alerts().Size(); ++i) {
            if (i > 0) output << ',';
            const AlertState& alert = store.Alerts().At(i);
            output << "{\"at\":" << alert.at << ",\"priority\":"
                   << alert.priority << ",\"confidence\":";
            WriteString(output, ConfidenceName(alert.confidence));
            output << ",\"title\":";
            WriteString(output, alert.title);
            output << ",\"detail\":";
            WriteString(
                output,
                redactIdentities ? "redacted" : alert.detail);
            output << '}';
        }
        output << "],\n\"decisions\":[";
        for (std::size_t i = 0; i < decisions.Entries().Size(); ++i) {
            if (i > 0) output << ',';
            const DecisionLogEntry& entry = decisions.Entries().At(i);
            output << "{\"at\":" << entry.at << ",\"priority\":"
                   << entry.priority << ",\"subject\":";
            WriteString(
                output,
                redactIdentities ? "redacted" : entry.subject);
            output << ",\"outcome\":";
            WriteString(output, entry.outcome);
            output << ",\"reason\":";
            WriteString(
                output,
                redactIdentities ? "redacted" : entry.reason);
            output << '}';
        }
        output << "],\n\"timeline\":[";
        for (std::size_t i = 0; i < store.TeamfightTimeline().Size(); ++i) {
            if (i > 0) output << ',';
            const GameEvent& event = store.TeamfightTimeline().At(i);
            output << "{\"at\":" << event.at << ",\"type\":"
                   << static_cast<int>(event.type) << ",\"source\":"
                   << (redactIdentities ? 0u : event.sourceId)
                   << ",\"target\":"
                   << (redactIdentities ? 0u : event.targetId) << '}';
        }
        output << "],\n\"insights\":{\"recallScore\":"
               << insights.recall.score << ",\"recallRecommended\":"
               << (insights.recall.recommended ? "true" : "false")
               << ",\"wardEfficiency\":"
               << insights.wardEfficiency.score
               << ",\"deathRecapDamage\":"
               << insights.deathRecap.totalDamage << "}\n}\n";
        return output.good();
    }
private:
    static void WriteString(std::ofstream& output, const char* value) {
        output << '"';
        for (const char* cursor = value ? value : ""; *cursor; ++cursor) {
            switch (*cursor) {
            case '\\': output << "\\\\"; break;
            case '"': output << "\\\""; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default: output << *cursor; break;
            }
        }
        output << '"';
    }
};

} // namespace NightSharp::Companion
