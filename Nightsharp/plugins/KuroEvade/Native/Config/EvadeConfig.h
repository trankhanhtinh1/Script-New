#pragma once

#include <cstdint>

namespace Plugins::KuroEvade {

// Native mirror of Config.cs.  Runtime-only values are deliberately kept
// out of this object so the engine cannot silently reintroduce the former
// Kuro preset/gate system.
struct EvadeSettings {
    bool Enabled = false;
    bool OnlyDangerous = false;
    bool FocusOnEvade = true;
    bool EnhanceDetect = true;

    bool SmoothEvadeSpell = true;
    bool ImproveMove = true;
    bool LowEvadeSmooth = false;
    bool UseCurrentPath = true;
    bool PreferPathHold = true;

    bool TestOnAllies = false;
    bool EnableCollision = true;
    bool MinionCollision = true;
    bool HeroCollision = true;
    bool YasuoCollision = true;

    bool EnableDrawings = true;
    bool DrawWarningMessage = true;
    bool ShowEvadeStatus = true;
    bool DisableFow = false;
    bool DisableEvadeForOlafR = true;

    int BlockSpells = 1;
    int AllowAutoAttackDangerLevel = 3;
    int BorderWidth = 2;

    int SkillShotsExtraRadius = 8;
    int SkillShotsExtraRange = 10;
    int ExtraEvadeDistance = 20;
    int PathFindingDistance = 300;
    int PathFindingDistance2 = 100;
    int DiagonalEvadePointsCount = 7;
    int DiagonalEvadePointsStep = 20;
    int CrossingTimeOffset = 190;
    int EvadingFirstTimeOffset = 180;
    int EvadingSecondTimeOffset = 80;
    int EvadePointChangeInterval = 240;
    int PathOnlyHoldMaxMs = 240;
    int EnemyAvoidance = 35;

    std::uint32_t EnabledColor = 0;
    std::uint32_t DisabledColor = 0;
    std::uint32_t MissileColor = 0;
};

struct RecommendedConfig {
    int SkillShotsExtraRadius;
    int SkillShotsExtraRange;
    int ExtraEvadeDistance;
    int PathFindingDistance;
    int PathFindingDistance2;
    int DiagonalEvadePointsCount;
    int DiagonalEvadePointsStep;
    int CrossingTimeOffset;
    int EvadingFirstTimeOffset;
    int EvadingSecondTimeOffset;
    int EvadePointChangeInterval;
    int PathOnlyHoldMaxMs;
    int EnemyAvoidance;
    int AllowAutoAttackDangerLevel;
    int BlockSpells;
    bool FocusOnEvade;
    bool OnlyDangerous;
};

class EvadeConfig final {
public:
    static RecommendedConfig Recommended(int index) {
        switch (index) {
        case 1:
            // Safe / High Ping: larger geometry margins, earlier replans and
            // no fresh attacks while movement is critical.
            return { 16, 20, 35, 300, 100, 7, 20,
                     280, 260, 100, 180, 200, 55, 0, 2,
                     true, false };
        case 2:
            // Smooth / Low Ping: small margins with stronger target stability.
            return { 4, 5, 15, 300, 100, 7, 20,
                     140, 130, 60, 300, 280, 30, 3, 1,
                     true, false };
        case 3:
            // Combat: spend as little movement/DPS as possible and only react
            // to entries explicitly marked dangerous.
            return { 6, 5, 15, 300, 100, 7, 20,
                     160, 150, 60, 240, 180, 25, 4, 1,
                     true, true };
        default:
            // Balanced: conservative enough for normal latency without
            // inflating every hitbox or forcing long exits.
            return { 8, 10, 20, 300, 100, 7, 20,
                     190, 180, 80, 240, 240, 35, 3, 1,
                     true, false };
        }
    }
};

} // namespace Plugins::KuroEvade
