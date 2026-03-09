#pragma once
#include "Enums.h"
#include <string>
#include <vector>

// ============================================================================
// SpellDatabaseEntry — C++ port of EnsoulSharp.SDK SpellDatabaseEntry.cs
//
// Represents a single spell entry in the spell database.
// Contains all properties needed for prediction, evade, and spell tracking.
//
// Source: EnsoulSharp.SDK/Core/Wrappers/Spells/Database/SpellDatabaseEntry.cs
// Data:   EnsoulSharp.SDK/Resources/Data/Database.json
// ============================================================================

namespace SDK {

    // ========================================================================
    // SpellDatabaseEntry
    // ========================================================================
    struct SpellDatabaseEntry {

        // ====================================================================
        // Champion & Spell Identity
        // ====================================================================

        /// Champion name (e.g. "Aatrox", "Ahri")
        std::string ChampionName;

        /// Internal spell name from SData (e.g. "AatroxQ", "AhriSeduce")
        std::string SpellName;

        /// Spell slot (Q/W/E/R)
        SpellSlotId Slot = SpellSlotId::Q;

        /// Comprehensive spell type (SkillshotLine, Targeted, etc.)
        SpellType Type = SpellType::SkillshotLine;

        // ====================================================================
        // Skillshot Geometry
        // ====================================================================

        /// The raw spell range
        int Range = INT_MAX;

        /// Skillshot radius (for circle skillshots)
        int Radius = 0;

        /// Skillshot width (for line skillshots)
        int Width = 50;

        /// Skillshot cone angle (degrees)
        int Angle = 45;

        /// Arc skillshot angle
        int ArcAngle = 0;

        /// Ring skillshot inner radius
        int RingRadius = 0;

        /// Extra range added to skillshots
        int ExtraRange = 0;

        /// Whether the skillshot has a fixed range
        bool FixedRange = false;

        /// Whether to avoid reducing range (e.g. Orianna Q)
        bool AvoidMaxRangeReduction = false;

        // ====================================================================
        // Timing
        // ====================================================================

        /// Cast delay in milliseconds
        int Delay = 250;

        /// Missile average travel speed (units/sec)
        int MissileSpeed = 1000;

        /// Missile acceleration
        int MissileAccel = 0;

        /// Missile minimum speed
        int MissileMinSpeed = 0;

        /// Missile maximum speed
        int MissileMaxSpeed = 0;

        /// Is the missile delayed?
        bool MissileDelayed = false;

        /// Does the missile follow the caster?
        bool MissileFollowsCaster = false;

        // ====================================================================
        // Missile Identification
        // ====================================================================

        /// Missile spell name (for missile-based detection)
        std::string MissileSpellName;

        /// Extra missile names (alternative missile names)
        std::vector<std::string> ExtraMissileNames;

        /// Extra spell names (alternative spell names)
        std::vector<std::string> ExtraSpellNames;

        // ====================================================================
        // Collision
        // ====================================================================

        /// What the spell missile can collide with (bitmask of CollisionableObjects)
        int CollisionObjects = CollisionNone;

        // ====================================================================
        // Danger Assessment
        // ====================================================================

        /// Danger value on scale of 1-5
        int DangerValue = 1;

        /// Is this spell dangerous?
        bool IsDangerous = false;

        // ====================================================================
        // Cast Type & Tags
        // ====================================================================

        /// How the spell can be cast
        std::vector<CastType> CastTypes;

        /// Tags describing spell properties
        std::vector<SpellTags> Tags;

        // ====================================================================
        // Buff Application
        // ====================================================================

        /// Buffs applied on allies
        std::vector<BuffType> AppliedBuffsOnAllies;

        /// Buffs applied on enemies
        std::vector<BuffType> AppliedBuffsOnEnemies;

        /// Buffs applied on self
        std::vector<BuffType> AppliedBuffsOnSelf;

        /// Buff name applied on self
        std::string AppliedBuffOnSelfName;

        /// Buff name applied on ally
        std::string AppliedBuffOnAllyName;

        /// Buff name applied on enemy
        std::string AppliedBuffOnEnemyName;

        /// Generic buff name applied by spell
        std::string AppliedBuffName;

        // ====================================================================
        // Source Object
        // ====================================================================

        /// Source object name (for special spell sources)
        std::string SourceObjectName;

        /// Source object names (for multiple sources)
        std::vector<std::string> FromObjects;

        /// Single source object name
        std::string FromObject;

        // ====================================================================
        // Misc
        // ====================================================================

        /// Does the spell reset the auto attack timer?
        bool ResetsAutoAttackTimer = false;

        /// Can the spell be removed?
        bool CanBeRemoved = false;

        /// Should the spell be forcefully removed?
        bool ForceRemove = false;

        /// Toggle particle name
        std::string ToggleParticleName;

        /// Minimum channel duration (ms)
        int MinChannelDuration = 0;

        /// Maximum channel duration (ms)
        int MaxChannelDuration = 0;

        // ====================================================================
        // Convenience Methods
        // ====================================================================

        /// Is this a CC spell? (DangerValue >= 3)
        bool IsCC() const { return DangerValue >= 3; }

        /// Is this a skillshot?
        bool IsSkillshot() const {
            return Type == SpellType::SkillshotLine
                || Type == SpellType::SkillshotCircle
                || Type == SpellType::SkillshotCone
                || Type == SpellType::SkillshotMissileLine
                || Type == SpellType::SkillshotMissileCircle
                || Type == SpellType::SkillshotMissileCone
                || Type == SpellType::SkillshotMissileArc
                || Type == SpellType::SkillshotRing
                || Type == SpellType::SkillshotArc;
        }

        /// Does this spell collide with minions?
        bool CollidesWithMinions() const {
            return (CollisionObjects & CollisionMinions) != 0;
        }

        /// Does this spell collide with heroes?
        bool CollidesWithHeroes() const {
            return (CollisionObjects & CollisionHeroes) != 0;
        }

        /// Can this spell be blocked by Yasuo wall?
        bool CanBeWindWalled() const {
            return (CollisionObjects & CollisionYasuoWall) != 0;
        }

        /// Has a specific tag?
        bool HasTag(SpellTags tag) const {
            for (auto& t : Tags) {
                if (t == tag) return true;
            }
            return false;
        }

        /// Has a specific cast type?
        bool HasCastType(CastType ct) const {
            for (auto& c : CastTypes) {
                if (c == ct) return true;
            }
            return false;
        }

        /// Get the real width (Width for line, Radius for circle)
        int GetRealWidth() const {
            switch (Type) {
            case SpellType::SkillshotCircle:
            case SpellType::SkillshotMissileCircle:
            case SpellType::SkillshotRing:
                return Radius;
            default:
                return Width;
            }
        }

        /// Convert delay from ms to seconds
        float GetDelayInSeconds() const {
            return Delay / 1000.0f;
        }

        /// Get effective missile speed (0 = instant)
        float GetMissileSpeedF() const {
            return static_cast<float>(MissileSpeed);
        }

        /// Convert SkillshotType for prediction (simplified 3-type)
        SkillshotType GetSkillshotType() const {
            switch (Type) {
            case SpellType::SkillshotCircle:
            case SpellType::SkillshotMissileCircle:
            case SpellType::SkillshotRing:
                return SkillshotType::Circle;
            case SpellType::SkillshotCone:
            case SpellType::SkillshotMissileCone:
                return SkillshotType::Cone;
            default:
                return SkillshotType::Line;
            }
        }
    };

} // namespace SDK
