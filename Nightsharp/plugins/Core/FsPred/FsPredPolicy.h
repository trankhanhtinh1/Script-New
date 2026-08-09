#pragma once

#include "../../../sdk/Enumerations/HitChance.h"
#include "../../../sdk/Enumerations/SpellSlot.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>

namespace Plugins::FsPred {

enum class FsPredPreset : int {
    Strict = 0,
    Balanced = 1,
    Aggressive = 2,
    Custom = 3,
};

enum class AntiBaitPolicy : int {
    Inherit = 0,
    Full = 1,
    IgnoreMaxRange = 2,
    Disabled = 3,
};

struct AntiBaitWeights {
    bool Enabled = true;
    int MaxRangeThresholdPercent = 90;
    int ReactionFloorMs = 180;
    int EvasiveTurnAngleDegrees = 60;
    int LongImpactHorizonMs = 750;
    int LongProjectileFlightMs = 650;
    int LongCastDistance = 900;
    bool TerrainCorridorBoost = false;
    SDK::HitChance AoeSecondaryMinimum = SDK::HitChance::High;
};

struct SlotCalibration {
    int DelayOffsetMs = 0;
    int SpeedScalePercent = 100;
    int SpeedOverride = 0;
    int RadiusOffset = 0;
    int RadiusOverride = 0;
    AntiBaitPolicy Policy = AntiBaitPolicy::Inherit;
};

struct FsPredConfig {
    FsPredPreset Preset = FsPredPreset::Balanced;
    int HardRangePercent = 100;
    int GlobalExtraDelayMs = 10;
    bool RecheckHitchance = true;
    bool UseDefaultSdk = false;
    bool RecordDebugPredictions = false;
    AntiBaitWeights AntiBait{};
    std::array<SlotCalibration, 4> Slots{};
};

struct ResolvedPredictionPolicy {
    AntiBaitWeights Weights{};
    SlotCalibration Calibration{};
    int SlotIndex = -1;
    bool HasKnownSlot = false;
    bool AntiBaitEnabled = true;
    bool IgnoreMaxRangePenalty = false;
};

struct SpellCalibrationValues {
    float DelaySeconds = 0.0f;
    float Speed = FLT_MAX;
    float Radius = 0.0f;
};

inline AntiBaitWeights PresetAntiBaitWeights(FsPredPreset preset) {
    AntiBaitWeights weights{};
    switch (preset) {
    case FsPredPreset::Strict:
        weights.MaxRangeThresholdPercent = 85;
        weights.ReactionFloorMs = 140;
        weights.EvasiveTurnAngleDegrees = 55;
        weights.LongImpactHorizonMs = 550;
        weights.LongProjectileFlightMs = 500;
        weights.LongCastDistance = 750;
        weights.AoeSecondaryMinimum = SDK::HitChance::High;
        break;
    case FsPredPreset::Aggressive:
        weights.MaxRangeThresholdPercent = 98;
        weights.ReactionFloorMs = 230;
        weights.EvasiveTurnAngleDegrees = 80;
        weights.LongImpactHorizonMs = 1000;
        weights.LongProjectileFlightMs = 950;
        weights.LongCastDistance = 1200;
        weights.AoeSecondaryMinimum = SDK::HitChance::Medium;
        break;
    case FsPredPreset::Balanced:
    case FsPredPreset::Custom:
    default:
        break;
    }
    return weights;
}

inline FsPredPreset PresetFromIndex(int index) {
    switch (index) {
    case 0: return FsPredPreset::Strict;
    case 2: return FsPredPreset::Aggressive;
    case 3: return FsPredPreset::Custom;
    case 1:
    default:
        return FsPredPreset::Balanced;
    }
}

inline int ChampionSlotIndex(SDK::SpellSlot slot) {
    switch (slot) {
    case SDK::SpellSlot::Q: return 0;
    case SDK::SpellSlot::W: return 1;
    case SDK::SpellSlot::E: return 2;
    case SDK::SpellSlot::R: return 3;
    default: return -1;
    }
}

inline ResolvedPredictionPolicy ResolvePredictionPolicy(
    const FsPredConfig& config,
    int slotIndex) {
    ResolvedPredictionPolicy resolved{};
    resolved.Weights = config.AntiBait;
    resolved.SlotIndex = slotIndex;
    resolved.HasKnownSlot = slotIndex >= 0 && slotIndex < 4;
    if (resolved.HasKnownSlot) {
        resolved.Calibration = config.Slots[static_cast<std::size_t>(slotIndex)];
    }

    switch (resolved.Calibration.Policy) {
    case AntiBaitPolicy::Full:
        resolved.AntiBaitEnabled = true;
        break;
    case AntiBaitPolicy::IgnoreMaxRange:
        resolved.AntiBaitEnabled = true;
        resolved.IgnoreMaxRangePenalty = true;
        break;
    case AntiBaitPolicy::Disabled:
        resolved.AntiBaitEnabled = false;
        break;
    case AntiBaitPolicy::Inherit:
    default:
        resolved.AntiBaitEnabled = config.AntiBait.Enabled;
        break;
    }
    return resolved;
}

inline SpellCalibrationValues ApplySlotCalibration(
    SpellCalibrationValues values,
    const SlotCalibration& calibration) {
    const float baseDelay = std::isfinite(values.DelaySeconds)
        ? values.DelaySeconds
        : 0.0f;
    values.DelaySeconds = std::max(
        0.0f,
        baseDelay + static_cast<float>(
            std::clamp(calibration.DelayOffsetMs, -100, 100)) / 1000.0f);

    if (calibration.SpeedOverride > 0) {
        values.Speed = static_cast<float>(calibration.SpeedOverride);
    } else if (std::isfinite(values.Speed) &&
               values.Speed > 0.0f &&
               values.Speed != FLT_MAX) {
        values.Speed *= static_cast<float>(
            std::clamp(calibration.SpeedScalePercent, 80, 120)) / 100.0f;
    }

    if (calibration.RadiusOverride > 0) {
        values.Radius = static_cast<float>(calibration.RadiusOverride);
    } else {
        const float baseRadius = std::isfinite(values.Radius)
            ? values.Radius
            : 0.0f;
        values.Radius = std::max(
            0.0f,
            baseRadius + static_cast<float>(
                std::clamp(calibration.RadiusOffset, -50, 100)));
    }
    return values;
}

inline float EscapeOpenFraction(int sampleCount, int blockedCount) {
    if (sampleCount <= 0) {
        return 1.0f;
    }
    const int blocked = std::clamp(blockedCount, 0, sampleCount);
    return static_cast<float>(sampleCount - blocked) /
        static_cast<float>(sampleCount);
}

} // namespace Plugins::FsPred
