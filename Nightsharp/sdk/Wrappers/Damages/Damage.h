#pragma once

#include "DamageJson.h"
#include "DamageMastery.h"
#include "DamagePassives.h"
#include "DamageLibrary.h"

#include <algorithm>
#include <cmath>

namespace SDK::Damage {

    inline float CalculateMixedDamage(const AIBaseClient& source,
                                      const AIBaseClient& target,
                                      float physicalAmount,
                                      float magicalAmount) {
        if (!source.IsValid() || !target.IsValid()) {
            return 0.0f;
        }

        return source.CalculatePhysicalDamage(target, std::max(physicalAmount, 0.0f)) +
               source.CalculateMagicDamage(target, std::max(magicalAmount, 0.0f));
    }

    inline float CalculateDamage(const AIBaseClient& source,
                                 const AIBaseClient& target,
                                 DamageType damageType,
                                 float amount) {
        if (!source.IsValid() || !target.IsValid()) {
            return 0.0f;
        }

        const float safeAmount = std::max(amount, 0.0f);
        float damage = 0.0f;
        switch (damageType) {
        case DamageType::Magical:
            damage = source.CalculateMagicDamage(target, safeAmount);
            break;
        case DamageType::Physical:
            damage = source.CalculatePhysicalDamage(target, safeAmount);
            break;
        case DamageType::Mixed:
            damage = CalculateMixedDamage(source, target, safeAmount * 0.5f, safeAmount * 0.5f);
            break;
        case DamageType::True:
        default:
            damage = std::floor(safeAmount);
            break;
        }

        if (!source.IsHero()) {
            return DamageMastery::ApplyIncoming(target, damageType, damage);
        }
        return DamageMastery::Apply(AIHeroClient(source.Address()), target, damageType, damage);
    }

    inline float GetSpellDamage(const AIBaseClient& source,
                                const AIBaseClient& target,
                                SpellSlot slot,
                                DamageStage stage = DamageStage::Default) {
        const float raw = DamageLibrary::GetSpellDamage(source, target, slot, stage);
        if (!source.IsHero()) {
            return DamageMastery::ApplyIncoming(target, DamageLibrary::GetSpellDamageType(source, slot, stage), raw);
        }
        return DamageMastery::Apply(
            AIHeroClient(source.Address()),
            target,
            DamageLibrary::GetSpellDamageType(source, slot, stage),
            raw);
    }

    inline float GetSpellDamage(const AIBaseClient& source,
                                const AIBaseClient& target,
                                SpellSlot slot) {
        return GetSpellDamage(source, target, slot, DamageStage::Default);
    }

    inline float GetPassiveDamage(const AIHeroClient& source,
                                  const AIBaseClient& target) {
        return DamagePassives::GetPassiveDamage(source, target);
    }

    // Matching EnsoulSharp: GetAutoAttackDamage considers passives + override
    inline float GetAutoAttackDamage(const AIHeroClient& source,
                                     const AIBaseClient& target,
                                     bool includePassives = true) {
        if (!source.IsValid() || !target.IsValid()) return 0.0f;

        float damage = 0.0f;

        if (includePassives) {
            const auto passiveInfo = DamagePassives::GetPassiveDamageDetails(source, target);
            if (passiveInfo.Override) {
                // Override replaces base AA damage (e.g. TF cards)
                damage = source.CalculatePhysicalDamage(target, passiveInfo.Physical)
                       + source.CalculateMagicDamage(target, passiveInfo.Magical)
                       + passiveInfo.True_;
            } else {
                // Base AA + passive bonus
                damage = source.GetAutoAttackDamage(target, false);
                damage += source.CalculatePhysicalDamage(target, passiveInfo.Physical)
                        + source.CalculateMagicDamage(target, passiveInfo.Magical)
                        + passiveInfo.True_;
            }
        } else {
            damage = source.GetAutoAttackDamage(target, false);
        }

        return DamageMastery::Apply(source, target, DamageType::Physical, damage);
    }

} // namespace SDK::Damage
