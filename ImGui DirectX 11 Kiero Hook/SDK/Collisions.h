#pragma once
#include "GameObjects.h"
#include "GameObject.h"
#include "../core/Vector.h"
#include <vector>
#include <string>

// ============================================================================
// Collisions — Skillshot collision detection utilities
// Reference: EnsoulSharp.SDK/Core/Utils/Collision.cs
//
// Includes:
//   - Yasuo Wind Wall collision check
//   - Minion/champion collision for skillshots
//   - Wall collision detection
// ============================================================================

namespace SDK {
namespace Collisions {

    // ====================================================================
    // Yasuo Wind Wall Collision
    // Reference: NewOrbwalker.cs CanAttackWithWindWall()
    //
    // Scans for active Yasuo WindWall objects and checks if the line
    // from source to target passes through the wall.
    // ====================================================================

    // Champions whose AAs are blocked by Wind Wall (most ranged champs)
    // Champions NOT in this list have melee/special AAs that bypass wall
    inline bool IsWindWallBlockedChampion(const std::string& champName) {
        // These champions' basic attacks CAN pass through wind wall
        static const char* notBlocked[] = {
            // Melee champions are never blocked
            // Special ranged: Azir soldiers, Vel'Koz (beam), Thresh (chain)
            "Azir",    // Soldier attacks go through
            "Thresh",  // Flay auto is special
            nullptr
        };

        for (const char** p = notBlocked; *p; p++) {
            if (_stricmp(champName.c_str(), *p) == 0) return false;
        }
        return true; // Most ranged AA are blocked
    }

    // Check if there's a Yasuo Wind Wall between two positions
    inline bool HasYasuoWindWallCollision(const Vec3& from, const Vec3& to) {
        // WindWall object names:
        //   "YasuoWMovingWall" — the actual wall object
        //   Width = ~300 units per segment, extends perpendicular to cast direction

        // Search through all game objects for wind wall instances
        // Wind walls are typically in the minion/object list
        for (auto& obj : GameObjects::AllMinions) {
            if (!obj.IsValid() || !obj.IsAlive()) continue;
            std::string name = obj.GetName();
            if (name.find("YasuoWMovingWall") == std::string::npos &&
                name.find("WindWall") == std::string::npos)
                continue;

            // This is a wind wall segment!
            // The wall is a line perpendicular to Yasuo's cast direction
            // Wall width: ~300-450 units (scales with level)
            Vec3 wallPos = obj.GetPosition();
            float wallHalfWidth = 225.0f; // ~450/2 at max level

            // Get wall direction (perpendicular to its facing)
            // Wall "faces" the direction it was cast
            Vec2 wallCenter = wallPos.To2D();

            // Simple check: does the line from→to pass within wallHalfWidth of the wall center?
            Vec2 lineStart = from.To2D();
            Vec2 lineEnd = to.To2D();

            float dist = Geometry::PointToSegmentDistance(wallCenter, lineStart, lineEnd);
            if (dist <= wallHalfWidth + 50.0f) { // 50 extra buffer
                return true;
            }
        }

        // Also check Pets list (wind wall can show up there)
        for (auto& obj : GameObjects::Pets) {
            if (!obj.IsValid() || !obj.IsAlive()) continue;
            std::string name = obj.GetName();
            if (name.find("YasuoWMovingWall") == std::string::npos &&
                name.find("WindWall") == std::string::npos)
                continue;

            Vec3 wallPos = obj.GetPosition();
            float wallHalfWidth = 225.0f;
            Vec2 wallCenter = wallPos.To2D();
            Vec2 lineStart = from.To2D();
            Vec2 lineEnd = to.To2D();

            float dist = Geometry::PointToSegmentDistance(wallCenter, lineStart, lineEnd);
            if (dist <= wallHalfWidth + 50.0f) {
                return true;
            }
        }

        return false;
    }

    // Convenience: check if AA from source to target is blocked by wind wall
    inline bool IsAutoAttackBlockedByWindWall(const GameObject& source, const GameObject& target) {
        if (source.IsMelee()) return false; // Melee AAs not blocked

        std::string champName = source.GetChampionName();
        if (!champName.empty() && !IsWindWallBlockedChampion(champName))
            return false;

        return HasYasuoWindWallCollision(source.GetPosition(), target.GetPosition());
    }

    // ====================================================================
    // Skillshot Collision — Check if skillshot will hit minions
    // ====================================================================
    inline bool HasMinionCollision(const Vec3& from, const Vec3& to, float width,
                                    const std::vector<GameObject>& ignore = {}) {
        Vec2 lineStart = from.To2D();
        Vec2 lineEnd = to.To2D();

        auto checkList = [&](const std::vector<GameObject>& list) -> bool {
            for (auto& obj : list) {
                if (!obj.IsValid() || !obj.IsAlive()) continue;
                // Check if this object is in the ignore list
                bool skip = false;
                for (auto& ig : ignore) {
                    if (ig.address == obj.address) { skip = true; break; }
                }
                if (skip) continue;

                Vec2 objPos = obj.GetPosition().To2D();
                float hitbox = obj.GetBoundingRadius() + width / 2.0f;
                float dist = Geometry::PointToSegmentDistance(objPos, lineStart, lineEnd);
                if (dist <= hitbox)
                    return true;
            }
            return false;
        };

        // Check enemy + ally minions
        if (checkList(GameObjects::EnemyMinions)) return true;
        if (checkList(GameObjects::AllyMinions)) return true;

        return false;
    }

    // ====================================================================
    // Hero Collision — Check if skillshot will hit heroes
    // ====================================================================
    inline bool HasHeroCollision(const Vec3& from, const Vec3& to, float width,
                                  const GameObject& excludeTarget = GameObject()) {
        Vec2 lineStart = from.To2D();
        Vec2 lineEnd = to.To2D();

        for (auto& hero : GameObjects::EnemyHeroes) {
            if (!hero.IsValid() || !hero.IsAlive()) continue;
            if (hero.address == excludeTarget.address) continue;

            Vec2 heroPos = hero.GetPosition().To2D();
            float hitbox = hero.GetBoundingRadius() + width / 2.0f;
            float dist = Geometry::PointToSegmentDistance(heroPos, lineStart, lineEnd);
            if (dist <= hitbox)
                return true;
        }
        return false;
    }

} // namespace Collisions
} // namespace SDK
