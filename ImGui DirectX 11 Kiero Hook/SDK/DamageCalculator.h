#pragma once
#include "GameObject.h"
#include <algorithm>
#include <cmath>

namespace SDK
{
    enum class DamageType {
        Physical,
        Magical,
        True,
        Mixed
    };

    class DamageCalculator
    {
    public:
        // ============================================================================
        // Physical Damage Calculation
        // ============================================================================
        // Formula: damage * (100 / (100 + effectiveArmor))
        // effectiveArmor = armor * (1 - armorPenPercent) - armorPenFlat
        static float CalculatePhysicalDamage(GameObject* source, GameObject* target, float rawDamage) {
            if (!source || !target || rawDamage <= 0) return 0.0f;

            float armor = target->GetArmor();
            float armorPenPercent = source->GetArmorPenPercent();  // Xuyên giáp %
            float armorPenFlat = source->GetArmorPenFlat();        // Sát lực

            float damageMultiplier;

            if (armor < 0) {
                // Negative armor increases damage taken
                damageMultiplier = 2.0f - (100.0f / (100.0f - armor));
            }
            else {
                // Calculate effective armor: % pen first, then flat pen
                float effectiveArmor = armor * (1.0f - armorPenPercent) - armorPenFlat;
                
                if (effectiveArmor < 0) {
                    damageMultiplier = 1.0f; // Penetration can't reduce below 0
                }
                else {
                    damageMultiplier = 100.0f / (100.0f + effectiveArmor);
                }
            }

            return std::floor(rawDamage * damageMultiplier);
        }

        // ============================================================================
        // Magic Damage Calculation
        // ============================================================================
        // Formula: damage * (100 / (100 + effectiveMR))
        // effectiveMR = mr * (1 - magicPenPercent) - magicPenFlat
        static float CalculateMagicDamage(GameObject* source, GameObject* target, float rawDamage) {
            if (!source || !target || rawDamage <= 0) return 0.0f;

            float magicResist = target->GetMagicResist();
            float magicPenPercent = source->GetMagicPenPercent();  // Xuyên kháng phép %
            float magicPenFlat = source->GetMagicPenFlat();        // Xuyên kháng phép (chỉ số)

            float damageMultiplier;

            if (magicResist < 0) {
                // Negative MR increases damage taken
                damageMultiplier = 2.0f - (100.0f / (100.0f - magicResist));
            }
            else {
                // Calculate effective MR: % pen first, then flat pen
                float effectiveMR = magicResist * (1.0f - magicPenPercent) - magicPenFlat;
                
                if (effectiveMR < 0) {
                    damageMultiplier = 1.0f;
                }
                else {
                    damageMultiplier = 100.0f / (100.0f + effectiveMR);
                }
            }

            return std::floor(rawDamage * damageMultiplier);
        }

        // ============================================================================
        // True Damage (No reduction)
        // ============================================================================
        static float CalculateTrueDamage(float rawDamage) {
            return std::floor(rawDamage);
        }

        // ============================================================================
        // Mixed Damage (Half Physical, Half Magical)
        // ============================================================================
        static float CalculateMixedDamage(GameObject* source, GameObject* target, float rawDamage) {
            float halfDamage = rawDamage / 2.0f;
            return CalculatePhysicalDamage(source, target, halfDamage) 
                 + CalculateMagicDamage(source, target, halfDamage);
        }

        // ============================================================================
        // Generic Damage Calculator
        // ============================================================================
        static float CalculateDamage(GameObject* source, GameObject* target, DamageType type, float rawDamage) {
            switch (type) {
                case DamageType::Physical:
                    return CalculatePhysicalDamage(source, target, rawDamage);
                case DamageType::Magical:
                    return CalculateMagicDamage(source, target, rawDamage);
                case DamageType::True:
                    return CalculateTrueDamage(rawDamage);
                case DamageType::Mixed:
                    return CalculateMixedDamage(source, target, rawDamage);
                default:
                    return rawDamage;
            }
        }

        // ============================================================================
        // Auto Attack Damage
        // ============================================================================
        // includeCrit = false: Sát thương đòn đánh thường (không chí mạng)
        // includeCrit = true: Sát thương trung bình (tính xác suất chí mạng)
        static float GetAutoAttackDamage(GameObject* source, GameObject* target, bool includeCrit = false) {
            if (!source || !target) return 0.0f;

            float totalAD = source->GetTotalAD();
            float damage = totalAD;

            // Tính sát thương trung bình với crit
            // Formula: AD * (1 + critChance * (critDamage - 1))
            // Ví dụ: 100 AD, 25% crit, 2.15 crit damage
            // = 100 * (1 + 0.25 * (2.15 - 1)) = 100 * 1.2875 = 128.75
            if (includeCrit) {
                float critChance = source->GetCritChance();
                float critDamage = source->GetCritDamage();  // 1.75 base, 2.15 với IE
                damage = totalAD * (1.0f + critChance * (critDamage - 1.0f));
            }

            return CalculatePhysicalDamage(source, target, damage);
        }
        
        // Sát thương khi chí mạng (guaranteed crit)
        static float GetCriticalDamage(GameObject* source, GameObject* target) {
            if (!source || !target) return 0.0f;
            
            float totalAD = source->GetTotalAD();
            float critDamage = source->GetCritDamage();
            float damage = totalAD * critDamage;
            
            return CalculatePhysicalDamage(source, target, damage);
        }

        // ============================================================================
        // Hits to Kill (Auto Attacks needed to kill target)
        // ============================================================================
        static int GetAutoAttacksToKill(GameObject* source, GameObject* target) {
            if (!source || !target) return 999;

            float aaDamage = GetAutoAttackDamage(source, target, false);
            if (aaDamage <= 0) return 999;

            float targetHealth = target->GetHealth();
            return (int)std::ceil(targetHealth / aaDamage);
        }

        // ============================================================================
        // Effective Health (for Target Selector)
        // ============================================================================
        // Returns how much raw damage needed to kill target
        static float GetEffectiveHealth(GameObject* source, GameObject* target, DamageType type) {
            if (!source || !target) return 999999.0f;

            float health = target->GetHealth();
            
            // Calculate damage multiplier based on resistances
            float testDamage = 100.0f;
            float actualDamage = CalculateDamage(source, target, type, testDamage);
            
            if (actualDamage <= 0) return 999999.0f;
            
            // Effective health = health / damage_multiplier
            float damageMultiplier = actualDamage / testDamage;
            return health / damageMultiplier;
        }

        // ============================================================================
        // Smart Target Score (Lower = Better target) - Like NewTargetSelector's AaIndicator
        // ============================================================================
        static float GetTargetScore(GameObject* source, GameObject* target, DamageType type) {
            if (!source || !target) return 999999.0f;

            // Score based on: Effective Health / Auto Attacks to kill
            // Lower score = easier to kill = better target
            float effectiveHealth = GetEffectiveHealth(source, target, type);
            
            // Factor in distance (closer targets are slightly preferred)
            float distance = source->GetPosition().Distance(target->GetPosition());
            float distancePenalty = distance / 1000.0f; // Small penalty for distance
            
            return effectiveHealth + distancePenalty * 50.0f;
        }
    };
}
