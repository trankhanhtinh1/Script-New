#pragma once

#include "../../../../SDK/Wrappers/Spells/Database/SpellDatabaseEntry.h"

#include <algorithm>
#include <cmath>
#include <climits>
#include <string>
#include <vector>

namespace Plugins::KuroEvade::Database {

enum class SpellSlot {
    Q,
    W,
    E,
    R,
    Unknown,
};

// Names intentionally follow the supplied Evade SpellData model. The
// database owns these values; SDK::SpellDatabaseEntry is built only at the
// NightSharp integration boundary.
enum class SkillShotType {
    SkillshotLine,
    SkillshotCircle,
    SkillshotCone,
    SkillshotArc,
};

enum class CollisionObjectType {
    EnemyChampions,
    EnemyMinions,
    EnemyYasuoWall,
    Terrain,
};

enum class CrowdControlType {
    None,
    Slow,
    Stun,
    Snare,
    KnockUp,
    KnockBack,
    Charm,
    Suppression,
    Silence,
    Terrain,
    Blind,
    Fear,
    Taunt,
    Sleep,
};

struct SpellData {
    std::string CharacterName;
    int DangerValue = 1;
    std::string DisplayName;
    std::string MissileSpellName;
    float MissileSpeed = 0.0f;
    float Radius = 0.0f;
    float Range = 0.0f;
    int Delay = 250;
    SpellSlot Slot = SpellSlot::Unknown;
    std::string SpellName;
    SkillShotType Type = SkillShotType::SkillshotLine;
    CrowdControlType CrowdControl = CrowdControlType::None;

    std::vector<std::string> ExtraSpellNames;
    std::vector<std::string> ExtraMissileNames;
    std::vector<CollisionObjectType> CollisionObjects;

    bool FixedRange = false;
    bool IsDangerous = false;
    bool IsSpecial = false;
    bool DisabledByDefault = false;
    bool DontProcess = false;
    bool HasEndExplosion = false;
    bool UseEndPosition = false;
    float SecondaryRadius = 0.0f;
    int ExtraEndTime = 0;
    float MultipleAngle = 0.0f;
    int ExtraDelay = 0;
    bool HasTrap = false;
    std::string TrapBaseName;

    // KuroEvade-only adapters needed by special spells and the ImGui drawer.
    bool Centered = false;
    bool CollisionExceptMini = false;
    bool FollowCaster = false;
    bool Invert = false;
    bool IsPerpendicular = false;
    bool IsHorizontal = false;
    bool NoTarget = false;
    bool UsePacket = false;
    std::string TrapTroyName;
    int MultipleNumber = -1;
    float ExtraDrawHeight = 0.0f;

    SDK::SpellDatabaseEntry Runtime;

    void Finalize() {
        Runtime = {};
        Runtime.ChampionName = CharacterName;
        Runtime.SpellName = SpellName;
        Runtime.MissileSpellName = MissileSpellName;
        Runtime.DangerValue = DangerValue;
        Runtime.Delay = Delay;
        Runtime.Radius = static_cast<int>(std::lround(Radius));
        Runtime.Range = static_cast<int>(std::lround(Range));
        Runtime.Width = Runtime.Radius;
        Runtime.MissileSpeed = MissileSpeed > 0.0f
            ? static_cast<int>(std::lround(MissileSpeed))
            : INT_MAX;
        Runtime.FixedRange = FixedRange;
        Runtime.ExtraSpellNames = ExtraSpellNames;
        Runtime.ExtraMissileNames = ExtraMissileNames;
        Runtime.Angle = MultipleAngle > 0.0f
            ? static_cast<int>(std::lround(MultipleAngle))
            : 0;
        Runtime.ArcAngle = Runtime.Angle;

        switch (Slot) {
        case SpellSlot::Q: Runtime.Slot = SDK::SpellSlot::Q; break;
        case SpellSlot::W: Runtime.Slot = SDK::SpellSlot::W; break;
        case SpellSlot::E: Runtime.Slot = SDK::SpellSlot::E; break;
        case SpellSlot::R: Runtime.Slot = SDK::SpellSlot::R; break;
        default: Runtime.Slot = SDK::SpellSlot::Unknown; break;
        }

        switch (Type) {
        case SkillShotType::SkillshotCircle:
            Runtime.SpellType = SDK::SpellType::SkillshotCircle;
            break;
        case SkillShotType::SkillshotCone:
            Runtime.SpellType = SDK::SpellType::SkillshotCone;
            break;
        case SkillShotType::SkillshotArc:
            Runtime.SpellType = SDK::SpellType::SkillshotMissileLine;
            break;
        default:
            Runtime.SpellType = SDK::SpellType::SkillshotLine;
            break;
        }

        for (const CollisionObjectType object : CollisionObjects) {
            switch (object) {
            case CollisionObjectType::EnemyChampions:
                Runtime.CollisionObjects.push_back(SDK::CollisionableObjects::Heroes);
                break;
            case CollisionObjectType::EnemyMinions:
                Runtime.CollisionObjects.push_back(SDK::CollisionableObjects::Minions);
                break;
            case CollisionObjectType::EnemyYasuoWall:
                Runtime.CollisionObjects.push_back(SDK::CollisionableObjects::YasuoWall);
                break;
            case CollisionObjectType::Terrain:
                Runtime.CollisionObjects.push_back(SDK::CollisionableObjects::Walls);
                break;
            }
        }
    }

    bool MatchesChampion(const char* name) const {
        return name && _stricmp(CharacterName.c_str(), name) == 0;
    }
};

} // namespace Plugins::KuroEvade::Database
