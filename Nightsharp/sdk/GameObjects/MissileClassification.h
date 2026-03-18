#pragma once
#include "core/RuntimeAPI.h"
#include <cstdint>
#include <string>
#include <algorithm>

// ============================================================================
// MissileClassification — Types and helpers for missile categorization
//
// Three missile categories for SDK integration:
//   1. MinionAutoAttack  → HealthPrediction (farming)
//   2. TurretShot        → Orbwalker turret farming logic
//   3. SpellMissile      → Evade / Prediction / Collision
//
// Uses RuntimeAPI functions (CompareTypeFlags, IsHero, IsTurret, IsMinion)
// for caster type detection.
//
// BasicAttack patterns from IDA sub_7FF7EB687DA0: %sBasicAttack, %sCritAttack
// ============================================================================

namespace SDK {

    // ========================================================================
    // MissileType — Classification enum
    // Values match RuntimeAPI::ClassifyMissile return values
    // ========================================================================
    enum class MissileType : uint8_t {
        Unknown         = 0,
        MinionAutoAttack = 1,   // Minion AA missile → HealthPrediction
        TurretShot       = 2,   // Turret shot → Orbwalker turret farming
        HeroAutoAttack   = 3,   // Hero AA missile (player/enemy)
        SpellMissile     = 4,   // Spell-based missile → Evade/Prediction
    };

    // ========================================================================
    // MissileClassifier — Static helper for classification
    // Delegates to RuntimeAPI for type checks (SEH protected)
    // ========================================================================
    class MissileClassifier {
    public:
        // Classify via RuntimeAPI::ClassifyMissile (SEH protected, fast)
        static MissileType ClassifyByAddress(uintptr_t missileAddr) {
            int result = RuntimeAPI::ClassifyMissile(missileAddr);
            if (result >= 0 && result <= 4) {
                return static_cast<MissileType>(result);
            }
            return MissileType::Unknown;
        }

        // Classify using resolved caster address + spell name + auto flag
        static MissileType Classify(uintptr_t casterAddr, const std::string& spellName, bool isAutoFlag) {
            if (!casterAddr) {
                return ClassifyByName(spellName);
            }

            // Use RuntimeAPI type checks (SEH protected)
            if (RuntimeAPI::IsTurret(casterAddr)) {
                return MissileType::TurretShot;
            }

            if (RuntimeAPI::IsMinion(casterAddr)) {
                if (IsAutoAttackName(spellName) || isAutoFlag) {
                    return MissileType::MinionAutoAttack;
                }
                return MissileType::SpellMissile;
            }

            if (RuntimeAPI::IsHero(casterAddr)) {
                if (IsAutoAttackName(spellName) || isAutoFlag) {
                    return MissileType::HeroAutoAttack;
                }
                return MissileType::SpellMissile;
            }

            return ClassifyByName(spellName);
        }

        // Quick check: is this spell name an auto attack?
        // Patterns from IDA sub_7FF7EB687DA0:
        //   BasicAttack, BasicAttack2..8, CritAttack, CritAttack2+
        static bool IsAutoAttackName(const std::string& name) {
            if (name.empty()) return false;
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            return (lower.find("basicattack") != std::string::npos ||
                    lower.find("critattack") != std::string::npos);
        }

        // Check if spell name indicates a turret shot
        static bool IsTurretShotName(const std::string& name) {
            if (name.empty()) return false;
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            return (lower.find("turretattack") != std::string::npos ||
                    lower.find("obeliskattack") != std::string::npos);
        }

        // Is any auto attack type (minion, turret, hero)
        static bool IsAnyAutoAttack(MissileType type) {
            return type == MissileType::MinionAutoAttack ||
                   type == MissileType::TurretShot ||
                   type == MissileType::HeroAutoAttack;
        }

    private:
        // Classify purely by spell/missile name when caster is unknown
        static MissileType ClassifyByName(const std::string& name) {
            if (name.empty()) return MissileType::Unknown;
            if (IsTurretShotName(name)) return MissileType::TurretShot;
            if (IsAutoAttackName(name)) return MissileType::HeroAutoAttack;
            return MissileType::SpellMissile;
        }
    };

} // namespace SDK
