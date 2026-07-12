#pragma once

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cmath>
#include <cfloat>

namespace Plugins::ziblldev9898::Damage {

struct OffensiveStats {
    float Level = 1.0f;
    float TotalAttackDamage = 0.0f;
    float BaseAttackDamage = 0.0f;
    float BonusAttackDamage = 0.0f;
    float AbilityPower = 0.0f;
    float Lethality = 0.0f;
    float FlatArmorPenetration = 0.0f;
    float PercentArmorPenetration = 1.0f;
    float PercentBonusArmorPenetration = 1.0f;
    float FlatMagicPenetration = 0.0f;
    float MagicLethality = 0.0f;
    float PercentMagicPenetration = 1.0f;
    float PercentBonusMagicPenetration = 1.0f;
};

struct DefensiveStats {
    float Armor = 0.0f;
    float BonusArmor = 0.0f;
    float MagicResistance = 0.0f;
    float BonusMagicResistance = 0.0f;
};

struct RawDamage {
    float Physical = 0.0f;
    float Magical = 0.0f;
    float TrueDamage = 0.0f;

    float Total() const {
        return Physical + Magical + TrueDamage;
    }
};

struct DamageResult {
    RawDamage Raw = {};
    float PhysicalDamage = 0.0f;
    float MagicalDamage = 0.0f;
    float TrueDamage = 0.0f;
    float EffectiveArmor = 0.0f;
    float EffectiveMagicResistance = 0.0f;
    float PhysicalMultiplier = 1.0f;
    float MagicalMultiplier = 1.0f;
    float TotalDamage = 0.0f;

    float Total() const {
        return TotalDamage;
    }
};

struct AbilityInput {
    float BaseDamage = 0.0f;
    float TotalADRatio = 0.0f;
    float BonusADRatio = 0.0f;
    float APRatio = 0.0f;
    float TargetMaxHealthRatio = 0.0f;
    float TargetCurrentHealthRatio = 0.0f;
    float TargetMissingHealthRatio = 0.0f;
    float MixedPhysicalRatio = 0.5f;
    SDK::DamageType Type = SDK::DamageType::Physical;
};

inline float SafeNonNegative(float value) {
    return std::isfinite(value) ? std::max(value, 0.0f) : 0.0f;
}

inline float NormalizePercentMultiplier(float value) {
    if (!std::isfinite(value) || value <= 0.0f) {
        return 1.0f;
    }
    return value > 1.0f ? std::clamp(value * 0.01f, 0.0f, 1.0f) : value;
}

inline float LethalityMultiplier(float level) {
    const float clampedLevel = std::clamp(level, 1.0f, 18.0f);
    return 0.6f + 0.4f * clampedLevel / 18.0f;
}

inline OffensiveStats ReadOffensiveStats(const SDK::AIBaseClient& source) {
    OffensiveStats stats = {};
    if (!source.IsValid()) {
        return stats;
    }

    stats.Level = static_cast<float>(std::max(1, source.Level()));
    stats.TotalAttackDamage = SafeNonNegative(source.TotalAttackDamage());
    stats.BaseAttackDamage = SafeNonNegative(source.BaseAttackDamage());
    stats.BonusAttackDamage = SafeNonNegative(source.BonusAttackDamage());
    stats.AbilityPower = SafeNonNegative(source.AP());
    stats.Lethality = SafeNonNegative(source.Lethality());
    stats.FlatArmorPenetration = SafeNonNegative(source.FlatArmorPenetrationMod());
    stats.PercentArmorPenetration = NormalizePercentMultiplier(
        source.PercentArmorPenetrationMod());
    stats.PercentBonusArmorPenetration = NormalizePercentMultiplier(
        source.PercentBonusArmorPenetrationMod());
    stats.FlatMagicPenetration = SafeNonNegative(source.FlatMagicPenetrationMod());
    stats.MagicLethality = SafeNonNegative(source.MagicLethality());
    stats.PercentMagicPenetration = NormalizePercentMultiplier(
        source.PercentMagicPenetrationMod());
    stats.PercentBonusMagicPenetration = NormalizePercentMultiplier(
        source.PercentBonusMagicPenetrationMod());
    return stats;
}

inline DefensiveStats ReadDefensiveStats(const SDK::AIBaseClient& target) {
    DefensiveStats stats = {};
    if (!target.IsValid()) {
        return stats;
    }

    stats.Armor = std::isfinite(target.Armor()) ? target.Armor() : 0.0f;
    stats.BonusArmor = std::isfinite(target.BonusArmor())
        ? target.BonusArmor()
        : 0.0f;
    stats.MagicResistance = std::isfinite(target.SpellBlock())
        ? target.SpellBlock()
        : 0.0f;
    stats.BonusMagicResistance = std::isfinite(target.BonusSpellBlock())
        ? target.BonusSpellBlock()
        : 0.0f;
    return stats;
}

inline float EffectiveArmor(const OffensiveStats& source,
                            const DefensiveStats& target) {
    if (target.Armor < 0.0f) {
        return target.Armor;
    }

    const float flatPenetration = source.FlatArmorPenetration +
        source.Lethality * LethalityMultiplier(source.Level);
    return target.Armor * source.PercentArmorPenetration -
           target.BonusArmor * (1.0f - source.PercentBonusArmorPenetration) -
           flatPenetration;
}

inline float EffectiveMagicResistance(const OffensiveStats& source,
                                      const DefensiveStats& target) {
    if (target.MagicResistance < 0.0f) {
        return target.MagicResistance;
    }

    const float flatPenetration = source.FlatMagicPenetration + source.MagicLethality;
    return target.MagicResistance * source.PercentMagicPenetration -
           target.BonusMagicResistance * (1.0f - source.PercentBonusMagicPenetration) -
           flatPenetration;
}

inline float PhysicalMultiplier(const OffensiveStats& source,
                                const DefensiveStats& target) {
    if (target.Armor < 0.0f) {
        return std::max(0.0f, 2.0f - 100.0f / (100.0f - target.Armor));
    }

    const float armor = EffectiveArmor(source, target);
    return armor < 0.0f ? 1.0f : 100.0f / (100.0f + armor);
}

inline float MagicalMultiplier(const OffensiveStats& source,
                               const DefensiveStats& target) {
    if (target.MagicResistance < 0.0f) {
        return std::max(0.0f,
                        2.0f - 100.0f / (100.0f - target.MagicResistance));
    }

    const float resistance = EffectiveMagicResistance(source, target);
    return resistance < 0.0f ? 1.0f : 100.0f / (100.0f + resistance);
}

inline DamageResult CalculateFromStats(const OffensiveStats& source,
                                       const DefensiveStats& target,
                                       const RawDamage& raw) {
    DamageResult result = {};
    result.Raw.Physical = SafeNonNegative(raw.Physical);
    result.Raw.Magical = SafeNonNegative(raw.Magical);
    result.Raw.TrueDamage = SafeNonNegative(raw.TrueDamage);
    result.EffectiveArmor = EffectiveArmor(source, target);
    result.EffectiveMagicResistance = EffectiveMagicResistance(source, target);
    result.PhysicalMultiplier = PhysicalMultiplier(source, target);
    result.MagicalMultiplier = MagicalMultiplier(source, target);
    result.PhysicalDamage = std::floor(
        result.Raw.Physical * result.PhysicalMultiplier);
    result.MagicalDamage = std::floor(
        result.Raw.Magical * result.MagicalMultiplier);
    result.TrueDamage = std::floor(result.Raw.TrueDamage);
    result.TotalDamage = std::max(
        result.PhysicalDamage + result.MagicalDamage + result.TrueDamage,
        0.0f);
    return result;
}

inline DamageResult Calculate(const SDK::AIBaseClient& source,
                              const SDK::AIBaseClient& target,
                              const RawDamage& raw,
                              bool applyGameModifiers = true) {
    DamageResult result = CalculateFromStats(
        ReadOffensiveStats(source), ReadDefensiveStats(target), raw);
    if (!applyGameModifiers || !source.IsValid() || !target.IsValid()) {
        return result;
    }

    result.PhysicalDamage = SDK::DamageMod::DamageReductionMod(
        source,
        target,
        result.PhysicalDamage,
        SDK::DamageType::Physical);
    result.MagicalDamage = SDK::DamageMod::DamageReductionMod(
        source,
        target,
        result.MagicalDamage,
        SDK::DamageType::Magical);
    result.TrueDamage = SDK::DamageMod::DamageReductionMod(
        source,
        target,
        result.TrueDamage,
        SDK::DamageType::True);

    if (source.IsHero()) {
        const SDK::AIHeroClient hero(source.Address());
        result.PhysicalDamage = SDK::DamageMastery::Apply(
            hero, target, SDK::DamageType::Physical, result.PhysicalDamage);
        result.MagicalDamage = SDK::DamageMastery::Apply(
            hero, target, SDK::DamageType::Magical, result.MagicalDamage);
        result.TrueDamage = SDK::DamageMastery::Apply(
            hero, target, SDK::DamageType::True, result.TrueDamage);
    } else {
        result.PhysicalDamage = SDK::DamageMastery::ApplyIncoming(
            target, SDK::DamageType::Physical, result.PhysicalDamage);
        result.MagicalDamage = SDK::DamageMastery::ApplyIncoming(
            target, SDK::DamageType::Magical, result.MagicalDamage);
        result.TrueDamage = SDK::DamageMastery::ApplyIncoming(
            target, SDK::DamageType::True, result.TrueDamage);
    }

    result.PhysicalDamage = std::max(std::floor(result.PhysicalDamage), 0.0f);
    result.MagicalDamage = std::max(std::floor(result.MagicalDamage), 0.0f);
    result.TrueDamage = std::max(std::floor(result.TrueDamage), 0.0f);
    result.TotalDamage = std::max(
        result.PhysicalDamage + result.MagicalDamage + result.TrueDamage,
        0.0f);
    return result;
}

inline RawDamage ResolveAbilityRaw(const SDK::AIBaseClient& source,
                                   const SDK::AIBaseClient& target,
                                   const AbilityInput& input) {
    RawDamage raw = {};
    if (!source.IsValid() || !target.IsValid()) {
        return raw;
    }

    const float amount = SafeNonNegative(input.BaseDamage) +
        source.TotalAttackDamage() * input.TotalADRatio +
        source.BonusAttackDamage() * input.BonusADRatio +
        source.AP() * input.APRatio +
        target.MaxHealth() * input.TargetMaxHealthRatio +
        target.Health() * input.TargetCurrentHealthRatio +
        std::max(target.MaxHealth() - target.Health(), 0.0f) *
            input.TargetMissingHealthRatio;

    switch (input.Type) {
    case SDK::DamageType::Magical:
        raw.Magical = SafeNonNegative(amount);
        break;
    case SDK::DamageType::Mixed: {
        const float physicalRatio = std::clamp(input.MixedPhysicalRatio, 0.0f, 1.0f);
        raw.Physical = SafeNonNegative(amount * physicalRatio);
        raw.Magical = SafeNonNegative(amount * (1.0f - physicalRatio));
        break;
    }
    case SDK::DamageType::True:
        raw.TrueDamage = SafeNonNegative(amount);
        break;
    case SDK::DamageType::Physical:
    default:
        raw.Physical = SafeNonNegative(amount);
        break;
    }
    return raw;
}

inline DamageResult CalculateAbility(const SDK::AIBaseClient& source,
                                     const SDK::AIBaseClient& target,
                                     const AbilityInput& input,
                                     bool applyGameModifiers = true) {
    return Calculate(source, target, ResolveAbilityRaw(source, target, input),
                     applyGameModifiers);
}

inline RawDamage ResolveAutoAttackRaw(const SDK::AIHeroClient& source,
                                      const SDK::AIBaseClient& target,
                                      bool includePassives = true) {
    RawDamage raw = {};
    if (!source.IsValid() || !target.IsValid()) {
        return raw;
    }

    raw.Physical = SafeNonNegative(source.TotalAttackDamage());
    if (!includePassives) {
        return raw;
    }

    const auto passive = SDK::DamagePassives::GetPassiveDamageDetails(source, target);
    if (passive.Override) {
        raw = {};
    }
    raw.Physical += SafeNonNegative(passive.Physical);
    raw.Magical += SafeNonNegative(passive.Magical);
    raw.TrueDamage += SafeNonNegative(passive.True_);
    return raw;
}

inline DamageResult CalculateAutoAttack(const SDK::AIHeroClient& source,
                                        const SDK::AIBaseClient& target,
                                        bool includePassives = true,
                                        bool applyGameModifiers = true) {
    return Calculate(source, target,
                     ResolveAutoAttackRaw(source, target, includePassives),
                     applyGameModifiers);
}

inline float GetAutoAttackDamage(const SDK::AIHeroClient& source,
                                 const SDK::AIBaseClient& target,
                                 bool includePassives = true) {
    return CalculateAutoAttack(source, target, includePassives).TotalDamage;
}

inline float GetCurrentAttackDamage(const SDK::AIBaseClient& source) {
    return source.IsValid() ? source.TotalAttackDamage() : 0.0f;
}

inline float GetCurrentBonusAttackDamage(const SDK::AIBaseClient& source) {
    return source.IsValid() ? source.BonusAttackDamage() : 0.0f;
}

inline float GetCurrentAbilityPower(const SDK::AIBaseClient& source) {
    return source.IsValid() ? source.AP() : 0.0f;
}

inline float GetCurrentLethality(const SDK::AIBaseClient& source) {
    return source.IsValid() ? source.Lethality() : 0.0f;
}

inline float GetCurrentFlatArmorPenetration(const SDK::AIBaseClient& source) {
    return source.IsValid() ? source.FlatArmorPenetrationMod() : 0.0f;
}

inline float GetCurrentFlatMagicPenetration(const SDK::AIBaseClient& source) {
    return source.IsValid() ? source.FlatMagicPenetrationMod() : 0.0f;
}

inline float GetCurrentMagicLethality(const SDK::AIBaseClient& source) {
    return source.IsValid() ? source.MagicLethality() : 0.0f;
}

inline float GetCurrentPercentArmorPenetration(const SDK::AIBaseClient& source) {
    return source.IsValid()
        ? NormalizePercentMultiplier(source.PercentArmorPenetrationMod())
        : 1.0f;
}

inline float GetCurrentPercentBonusArmorPenetration(const SDK::AIBaseClient& source) {
    return source.IsValid()
        ? NormalizePercentMultiplier(source.PercentBonusArmorPenetrationMod())
        : 1.0f;
}

inline float GetCurrentPercentMagicPenetration(const SDK::AIBaseClient& source) {
    return source.IsValid()
        ? NormalizePercentMultiplier(source.PercentMagicPenetrationMod())
        : 1.0f;
}

inline float GetCurrentPercentBonusMagicPenetration(const SDK::AIBaseClient& source) {
    return source.IsValid()
        ? NormalizePercentMultiplier(source.PercentBonusMagicPenetrationMod())
        : 1.0f;
}

inline float GetSpellDamage(const SDK::AIBaseClient& source,
                            const SDK::AIBaseClient& target,
                            SDK::SpellSlot slot,
                            SDK::DamageStage stage = SDK::DamageStage::Default) {
    return SDK::Damage::GetSpellDamage(source, target, slot, stage);
}

inline float GetSpellDamage(const SDK::AIBaseClient& source,
                            const SDK::AIBaseClient& target,
                            SDK::SpellSlot slot) {
    return GetSpellDamage(source, target, slot, SDK::DamageStage::Default);
}

inline float GetDamage(const SDK::AIBaseClient& source,
                       const SDK::AIBaseClient& target,
                       SDK::DamageType type,
                       float rawAmount) {
    if (!source.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    RawDamage raw = {};
    switch (type) {
    case SDK::DamageType::Magical:
        raw.Magical = SafeNonNegative(rawAmount);
        break;
    case SDK::DamageType::Mixed:
        raw.Physical = SafeNonNegative(rawAmount * 0.5f);
        raw.Magical = SafeNonNegative(rawAmount * 0.5f);
        break;
    case SDK::DamageType::True:
        raw.TrueDamage = SafeNonNegative(rawAmount);
        break;
    case SDK::DamageType::Physical:
    default:
        raw.Physical = SafeNonNegative(rawAmount);
        break;
    }
    return Calculate(source, target, raw).TotalDamage;
}

} // namespace Plugins::ziblldev9898::Damage
