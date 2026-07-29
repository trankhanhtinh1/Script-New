#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace OrbwalkerKuro::FarmLogic {

inline constexpr float kLastHitDamageSafety = 0.97f;
inline constexpr float kCritDamageDecisionProbability = 0.90f;
inline constexpr float kCertainCritProbability = 0.999f;
inline constexpr int kLastHitWindowTieMs = 75;
inline constexpr int kCritChanceBucketCount = 21;
inline constexpr int kCritChanceBucketSteps = kCritChanceBucketCount - 1;
inline constexpr int kCritDroughtStateCount = 64;
inline constexpr float kCritPriorWeight = 24.0f;
inline constexpr std::uint32_t kCritCounterDecayAt = 4096;

inline float ClampProbability(float value) {
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    return std::clamp(value, 0.0f, 1.0f);
}

// Return the stationary proc rate produced by h(n) = min(1, coefficient * n).
// A cycle starts immediately after a crit and ends at the next crit.
inline float LinearHazardLongRunRate(float coefficient) {
    const double safeCoefficient = std::clamp(
        static_cast<double>(coefficient), 0.0, 1.0);
    if (safeCoefficient <= 0.0) {
        return 0.0f;
    }

    double survival = 1.0;
    double expectedCycleLength = 1.0;
    for (int attempt = 1; attempt <= 4096 && survival > 1.0e-14; ++attempt) {
        const double hazard = std::min(1.0, safeCoefficient * attempt);
        survival *= 1.0 - hazard;
        expectedCycleLength += survival;
    }
    return static_cast<float>(1.0 / expectedCycleLength);
}

// Find C(p) so the increasing discrete hazard keeps the requested long-run
// crit chance instead of silently adding expected DPS.
inline float SolveLinearHazardCoefficient(float critChance) {
    const float chance = ClampProbability(critChance);
    if (chance <= 0.0f || chance >= 1.0f) {
        return chance;
    }

    float low = 0.0f;
    float high = 1.0f;
    for (int iteration = 0; iteration < 64; ++iteration) {
        const float middle = (low + high) * 0.5f;
        if (LinearHazardLongRunRate(middle) < chance) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return (low + high) * 0.5f;
}

inline float LinearHazardProbability(float coefficient, int consecutiveNonCrits) {
    if (coefficient <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(
        coefficient * static_cast<float>(std::max(0, consecutiveNonCrits) + 1),
        0.0f,
        1.0f);
}

inline float LinearHazardProbabilityForChance(float critChance,
                                              int consecutiveNonCrits) {
    return LinearHazardProbability(
        SolveLinearHazardCoefficient(critChance), consecutiveNonCrits);
}

inline int CritChanceBucket(float critChance) {
    const float chance = ClampProbability(critChance);
    return std::clamp(
        static_cast<int>(std::lround(chance * kCritChanceBucketSteps)),
        0,
        kCritChanceBucketSteps);
}

inline int CritDroughtState(int consecutiveNonCrits) {
    return std::clamp(consecutiveNonCrits, 0, kCritDroughtStateCount - 1);
}

inline bool IsCriticalAttackName(const char* value) {
    if (!value || !value[0]) {
        return false;
    }

    std::string name(value);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    const bool containsCrit = name.find("crit") != std::string::npos;
    const bool containsAttack = name.find("attack") != std::string::npos;
    return name.find("critattack") != std::string::npos ||
           name.find("attackcrit") != std::string::npos ||
           (containsCrit && containsAttack);
}

struct CritBucketStats {
    std::array<std::uint32_t, kCritDroughtStateCount> attempts = {};
    std::array<std::uint32_t, kCritDroughtStateCount> crits = {};
};

class CritSequenceTracker {
public:
    void Observe(float critChance, bool didCrit) {
        const float chance = ClampProbability(critChance);
        // Attacks at zero crit chance do not reveal the crit RNG sequence. In
        // particular, forced cosmetic crits must not seed a fake drought.
        if (chance <= 0.0f) {
            return;
        }

        CritBucketStats& stats = buckets_[CritChanceBucket(chance)];
        const int state = CritDroughtState(consecutiveNonCrits_);
        if (stats.attempts[state] >= kCritCounterDecayAt) {
            for (int i = 0; i < kCritDroughtStateCount; ++i) {
                stats.attempts[i] = (stats.attempts[i] + 1u) / 2u;
                stats.crits[i] = (stats.crits[i] + 1u) / 2u;
            }
        }

        ++stats.attempts[state];
        if (didCrit) {
            ++stats.crits[state];
            consecutiveNonCrits_ = 0;
        } else {
            consecutiveNonCrits_ = std::min(
                consecutiveNonCrits_ + 1, kCritDroughtStateCount - 1);
        }
        ++observedAttacks_;
    }

    float PredictNextCritProbability(float critChance) const {
        const float chance = ClampProbability(critChance);
        if (chance <= 0.0f || chance >= 1.0f) {
            return chance;
        }

        const float coefficient = SolveLinearHazardCoefficient(chance);
        const CritBucketStats& stats = buckets_[CritChanceBucket(chance)];

        struct IsotonicBlock {
            int first = 0;
            int last = 0;
            double weight = 0.0;
            double weightedProbability = 0.0;

            double Mean() const {
                return weight > 0.0 ? weightedProbability / weight : 0.0;
            }
        };

        std::array<IsotonicBlock, kCritDroughtStateCount> blocks = {};
        int blockCount = 0;
        for (int state = 0; state < kCritDroughtStateCount; ++state) {
            const float prior = LinearHazardProbability(coefficient, state);
            const double weight =
                kCritPriorWeight + static_cast<double>(stats.attempts[state]);
            const double successes =
                kCritPriorWeight * prior + static_cast<double>(stats.crits[state]);

            blocks[blockCount++] = { state, state, weight, successes };
            // Pool-adjacent-violators: observed noise cannot make a longer
            // non-crit streak predict a lower next-attack crit probability.
            while (blockCount >= 2 &&
                   blocks[blockCount - 2].Mean() > blocks[blockCount - 1].Mean()) {
                IsotonicBlock& left = blocks[blockCount - 2];
                const IsotonicBlock& right = blocks[blockCount - 1];
                left.last = right.last;
                left.weight += right.weight;
                left.weightedProbability += right.weightedProbability;
                --blockCount;
            }
        }

        const int wantedState = CritDroughtState(consecutiveNonCrits_);
        for (int i = 0; i < blockCount; ++i) {
            if (wantedState >= blocks[i].first && wantedState <= blocks[i].last) {
                return ClampProbability(static_cast<float>(blocks[i].Mean()));
            }
        }
        return LinearHazardProbability(coefficient, wantedState);
    }

    int ConsecutiveNonCrits() const {
        return consecutiveNonCrits_;
    }

    std::uint32_t ObservedAttacks() const {
        return observedAttacks_;
    }

    std::uint32_t AttemptsAt(float critChance, int droughtState) const {
        return buckets_[CritChanceBucket(critChance)]
            .attempts[CritDroughtState(droughtState)];
    }

    std::uint32_t CritsAt(float critChance, int droughtState) const {
        return buckets_[CritChanceBucket(critChance)]
            .crits[CritDroughtState(droughtState)];
    }

private:
    std::array<CritBucketStats, kCritChanceBucketCount> buckets_ = {};
    int consecutiveNonCrits_ = 0;
    std::uint32_t observedAttacks_ = 0;
};

inline bool ShouldApplyPredictedCritDamage(float nextCritProbability,
                                           float critChance,
                                           bool isSiegeMinion) {
    const float chance = ClampProbability(critChance);
    const float probability = ClampProbability(nextCritProbability);
    if (chance >= kCertainCritProbability) {
        return true;
    }
    if (isSiegeMinion) {
        return probability >= kCertainCritProbability;
    }
    return probability >= kCritDamageDecisionProbability;
}

inline bool IsInsideLastHitDamageWindow(float predictedHealth,
                                        float attackDamage,
                                        float damageSafety = kLastHitDamageSafety) {
    if (!std::isfinite(predictedHealth) || !std::isfinite(attackDamage) ||
        predictedHealth <= 0.0f || attackDamage <= 0.0f) {
        return false;
    }
    return predictedHealth < attackDamage * std::clamp(damageSafety, 0.0f, 1.0f);
}

inline float LastHitBoundarySafety(float predictedHealth,
                                   float attackDamage,
                                   float damageSafety = kLastHitDamageSafety) {
    const float safeDamage = attackDamage * std::clamp(damageSafety, 0.0f, 1.0f);
    if (predictedHealth <= 0.0f || safeDamage <= predictedHealth) {
        return -std::numeric_limits<float>::infinity();
    }
    return std::min(predictedHealth, safeDamage - predictedHealth);
}

struct LastHitWindowCandidate {
    bool valid = false;
    int closingWindowMs = std::numeric_limits<int>::max();
    float boundarySafety = -std::numeric_limits<float>::infinity();
    float predictedHealth = std::numeric_limits<float>::infinity();
    int stableOrder = std::numeric_limits<int>::max();
};

inline bool PreferLastHitCandidate(const LastHitWindowCandidate& candidate,
                                   const LastHitWindowCandidate& current) {
    if (!candidate.valid) {
        return false;
    }
    if (!current.valid) {
        return true;
    }

    const bool candidateCloses =
        candidate.closingWindowMs != std::numeric_limits<int>::max();
    const bool currentCloses =
        current.closingWindowMs != std::numeric_limits<int>::max();
    if (candidateCloses != currentCloses) {
        return candidateCloses;
    }
    if (candidateCloses &&
        std::abs(candidate.closingWindowMs - current.closingWindowMs) >
            kLastHitWindowTieMs) {
        return candidate.closingWindowMs < current.closingWindowMs;
    }
    if (std::fabs(candidate.boundarySafety - current.boundarySafety) > 0.01f) {
        return candidate.boundarySafety > current.boundarySafety;
    }
    if (std::fabs(candidate.predictedHealth - current.predictedHealth) > 0.01f) {
        return candidate.predictedHealth < current.predictedHealth;
    }
    return candidate.stableOrder < current.stableOrder;
}

} // namespace OrbwalkerKuro::FarmLogic
