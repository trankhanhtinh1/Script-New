#pragma once
#include <string>
#include <vector>

// ============================================================================
// EzEvade::SpellData — Data structure for enemy skillshots to dodge.
//
// Ported from SpellDatabase.lua (NightSharp evade system).
// Patch target: 26.5 (2026-03-03)
//
// Each entry represents a single skillshot or targeted spell that the
// evade system should detect and dodge.
// ============================================================================

namespace EzEvade {

    // ========================================================================
    // SkillshotType — The geometric shape of a skillshot.
    // ========================================================================
    enum class SkillshotType : int {
        Line         = 0,   // Linear skillshot (Morgana Q)
        Circle       = 1,   // Circular skillshot (Cho'Gath Q)
        Cone         = 2,   // Cone skillshot (Annie W)
        Ring         = 3,   // Ring skillshot (Veigar E)
        Arc          = 4,   // Arc skillshot (Diana Q)
        MissileLine  = 5,   // Line missile (Ezreal Q)
        MissileArc   = 6,   // Arc missile
        None         = -1,
    };

    // ========================================================================
    // CollisionType — What can block a spell missile.
    // ========================================================================
    enum CollisionType : int {
        CollisionNone       = 0,
        CollisionMinions    = 1 << 0,
        CollisionHeroes     = 1 << 1,
        CollisionYasuoWall  = 1 << 2,
        CollisionBraumShield = 1 << 3,
        CollisionWalls      = 1 << 4,
    };

    // ========================================================================
    // DangerLevel — How dangerous/important a spell is to dodge.
    // ========================================================================
    enum class DangerLevel : int {
        Low      = 1,
        Medium   = 2,
        High     = 3,
        Extreme  = 4,
        Ultimate = 5,
    };

    // ========================================================================
    // SpellData — Describes a single spell entry.
    // ========================================================================
    struct SpellData {
        // Identity
        std::string charName;           // Champion name (e.g. "Morgana")
        std::string spellName;          // Internal spell name (e.g. "MorganaQ")
        std::string displayName;        // Display name for menu (e.g. "Q - Dark Binding")

        // Geometry
        SkillshotType type = SkillshotType::Line;
        float range        = 0.0f;      // Max range of the spell
        float radius       = 0.0f;      // Radius (circle) or half-width (line)
        float speed        = 0.0f;      // Missile speed (0 = instant)
        float castDelay    = 0.25f;     // Cast delay in seconds
        float extraRange   = 0.0f;      // Extra range beyond listed range
        int   angle        = 0;         // Cone angle (degrees)

        // Flags
        bool isMissile     = false;     // Whether this spell has a visible missile
        bool isCC          = false;     // Does this spell apply crowd control?
        bool fixedRange    = false;     // Is the range fixed?
        bool canBeRemoved  = true;      // Can the spell zone be removed?
        bool forceRemove   = false;     // Force remove on timeout

        // Collision
        int  collisionObjects = CollisionNone;

        // Danger assessment
        int dangerLevel    = 3;         // 1-5 danger level

        // Missile identification
        std::string missileSpellName;                // Missile name if different
        std::vector<std::string> extraMissileNames;  // Alternative missile names
        std::vector<std::string> extraSpellNames;    // Alternative spell names

        // Menu evade settings
        bool defaultEnabled   = true;   // Enabled by default
        int  defaultEvadePct  = 100;    // Default dodge percentage (0=only-kill-me, 100=all)
        bool defaultFow       = false;  // Dodge from fog of war

        // ====================================================================
        // Convenience
        // ====================================================================
        bool IsSkillshot() const {
            return type != SkillshotType::None;
        }

        bool CollidesWithMinions() const {
            return (collisionObjects & CollisionMinions) != 0;
        }

        bool CanBeWindWalled() const {
            return (collisionObjects & CollisionYasuoWall) != 0;
        }

        DangerLevel GetDangerLevel() const {
            return static_cast<DangerLevel>(dangerLevel);
        }
    };

} // namespace EzEvade
