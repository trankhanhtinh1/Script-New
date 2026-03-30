#pragma once

#include "DamageJson.h"
#include "DamageMastery.h"
#include "DamagePassives.h"
#include "DamageLibrary.h"

namespace SDK::Damage {

    inline float GetSpellDamage(const AIBaseClient& source,
                                const AIBaseClient& target,
                                SpellSlot slot,
                                DamageStage stage = DamageStage::Default) {
        const float raw = DamageLibrary::GetSpellDamage(source, target, slot, stage);
        if (!source.IsHero()) {
            return raw;
        }
        return DamageMastery::Apply(AIHeroClient(source.Address()), target, DamageType::Physical, raw);
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
