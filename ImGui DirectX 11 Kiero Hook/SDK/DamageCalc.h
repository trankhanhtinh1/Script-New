#pragma once
#include "GameObject.h"
#include "Enums.h"
#include <algorithm>
#include <cmath>

// ============================================================================
// DamageCalc — Damage calculation library
// Reference: EnsoulSharp.SDK/Damage.cs + Script-New-main/SDK/DamageCalculator.h
// ============================================================================

namespace SDK {

    class DamageCalc {
    public:
        // ====================================================================
        // Physical Damage
        // Formula: rawDamage * (100 / (100 + effectiveArmor))
        // effectiveArmor = armor * (1 - armorPenPercent) - lethality
        // ====================================================================
        static float CalcPhysicalDamage(const GameObject& source, const GameObject& target, float rawDamage) {
            if (rawDamage <= 0) return 0.0f;

            float armor = target.GetArmor();
            float armorPenPercent = source.GetArmorPenPercent();
            float armorPenFlat = source.GetArmorPenFlat();
            float lethality = source.GetLethality();

            // Lethality scales with target level: flat = lethality * (0.6 + 0.4 * targetLevel / 18)
            int targetLevel = target.GetLevel();
            if (targetLevel <= 0) targetLevel = 1;
            float flatPen = armorPenFlat + lethality * (0.6f + 0.4f * (float)targetLevel / 18.0f);

            float damageMultiplier;
            if (armor < 0) {
                // Negative armor → amplifies damage
                damageMultiplier = 2.0f - (100.0f / (100.0f - armor));
            } else {
                // % pen first, then flat pen
                float effectiveArmor = armor * (1.0f - armorPenPercent) - flatPen;
                if (effectiveArmor < 0) effectiveArmor = 0;
                damageMultiplier = 100.0f / (100.0f + effectiveArmor);
            }

            return std::floor(rawDamage * damageMultiplier);
        }

        // ====================================================================
        // Magic Damage
        // Formula: rawDamage * (100 / (100 + effectiveMR))
        // effectiveMR = mr * (1 - magicPenPercent) - magicPenFlat
        // ====================================================================
        static float CalcMagicDamage(const GameObject& source, const GameObject& target, float rawDamage) {
            if (rawDamage <= 0) return 0.0f;

            float mr = target.GetMR();
            float magicPenPercent = source.GetMagicPenPercent();
            float magicPenFlat = source.GetMagicPenFlat();

            float damageMultiplier;
            if (mr < 0) {
                damageMultiplier = 2.0f - (100.0f / (100.0f - mr));
            } else {
                float effectiveMR = mr * (1.0f - magicPenPercent) - magicPenFlat;
                if (effectiveMR < 0) effectiveMR = 0;
                damageMultiplier = 100.0f / (100.0f + effectiveMR);
            }

            return std::floor(rawDamage * damageMultiplier);
        }

        // ====================================================================
        // True Damage (no reduction)
        // ====================================================================
        static float CalcTrueDamage(float rawDamage) {
            return std::floor(rawDamage);
        }

        // ====================================================================
        // Mixed Damage (half physical, half magical)
        // ====================================================================
        static float CalcMixedDamage(const GameObject& source, const GameObject& target, float rawDamage) {
            float half = rawDamage / 2.0f;
            return CalcPhysicalDamage(source, target, half) + CalcMagicDamage(source, target, half);
        }

        // ====================================================================
        // Generic calculator
        // ====================================================================
        static float CalcDamage(const GameObject& source, const GameObject& target,
                                DamageType type, float rawDamage) {
            switch (type) {
            case DamageType::Physical: return CalcPhysicalDamage(source, target, rawDamage);
            case DamageType::Magical:  return CalcMagicDamage(source, target, rawDamage);
            case DamageType::True:     return CalcTrueDamage(rawDamage);
            case DamageType::Mixed:    return CalcMixedDamage(source, target, rawDamage);
            default:                   return rawDamage;
            }
        }

        // ====================================================================
        // Auto Attack Damage
        // ====================================================================
        static float GetAutoAttackDamage(const GameObject& source, const GameObject& target,
                                         bool includeCrit = false) {
            float totalAD = source.GetTotalAD();
            float damage = totalAD;

            if (includeCrit) {
                float critChance = source.GetCrit();
                float critMulti = source.GetCritMultiplier();
                if (critMulti <= 0.0f) critMulti = 1.75f; // default crit multiplier
                damage = totalAD * (1.0f + critChance * (critMulti - 1.0f));
            }

            return CalcPhysicalDamage(source, target, damage);
        }

        // Guaranteed crit damage
        static float GetCritDamage(const GameObject& source, const GameObject& target) {
            float critMulti = source.GetCritMultiplier();
            if (critMulti <= 0.0f) critMulti = 1.75f;
            return CalcPhysicalDamage(source, target, source.GetTotalAD() * critMulti);
        }

        // ====================================================================
        // Hits to kill
        // ====================================================================
        static int GetAutoAttacksToKill(const GameObject& source, const GameObject& target) {
            float aaDmg = GetAutoAttackDamage(source, target, false);
            if (aaDmg <= 0) return 999;
            return (int)std::ceil(target.GetHealth() / aaDmg);
        }

        // ====================================================================
        // Effective Health (raw damage needed to kill)
        // ====================================================================
        static float GetEffectiveHealth(const GameObject& source, const GameObject& target,
                                        DamageType type) {
            float testDmg = 100.0f;
            float actual = CalcDamage(source, target, type, testDmg);
            if (actual <= 0) return 999999.0f;
            float multiplier = actual / testDmg;
            return target.GetHealth() / multiplier;
        }

        // ====================================================================
        // Target Score (lower = easier to kill = better target)
        // ====================================================================
        static float GetTargetScore(const GameObject& source, const GameObject& target, DamageType type) {
            float effectiveHP = GetEffectiveHealth(source, target, type);
            float distance = source.DistanceTo(target);
            return effectiveHP + (distance / 1000.0f) * 50.0f;
        }
    };

} // namespace SDK
