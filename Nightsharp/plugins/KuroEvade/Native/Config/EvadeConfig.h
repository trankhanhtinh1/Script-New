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
    bool OnlyEvadeWhileCan = false;
    bool EnhanceDetect = true;

    bool SmoothEvadeSpell = true;
    bool ImproveMove = true;
    bool LowEvadeSmooth = false;
    bool UseCurrentPath = true;

    bool TestOnAllies = false;
    bool EnableCollision = false;
    bool MinionCollision = false;
    bool HeroCollision = false;
    bool YasuoCollision = true;

    bool EnableDrawings = true;
    bool DrawWarningMessage = true;
    bool ShowEvadeStatus = false;
    bool DisableFow = false;
    bool DisableEvadeForOlafR = true;

    int BlockSpells = 1;
    int AllowAutoAttackDangerLevel = 4;
    int BorderWidth = 2;

    int SkillShotsExtraRadius = 0;
    int SkillShotsExtraRange = 0;
    int GridSize = 10;
    int ExtraEvadeDistance = 25;
    int PathFindingDistance = 300;
    int PathFindingDistance2 = 100;
    int DiagonalEvadePointsCount = 7;
    int DiagonalEvadePointsStep = 20;
    int CrossingTimeOffset = 250;
    int EvadingFirstTimeOffset = 250;
    int EvadingSecondTimeOffset = 80;
    int EvadingRouteChangeTimeOffset = 250;
    int EvadePointChangeInterval = 250;

    std::uint32_t EnabledColor = 0;
    std::uint32_t DisabledColor = 0;
    std::uint32_t MissileColor = 0;
};

struct RecommendedConfig {
    int SkillShotsExtraRadius;
    int SkillShotsExtraRange;
    int GridSize;
    int ExtraEvadeDistance;
    int PathFindingDistance;
    int PathFindingDistance2;
    int DiagonalEvadePointsCount;
    int DiagonalEvadePointsStep;
    int CrossingTimeOffset;
    int EvadingFirstTimeOffset;
    int EvadingSecondTimeOffset;
    int EvadingRouteChangeTimeOffset;
    int EvadePointChangeInterval;
};

class EvadeConfig final {
public:
    static RecommendedConfig Recommended(int index) {
        switch (index) {
        case 1:
            return { 0, 0, 10, 25, 300, 100, 7, 20,
                     200, 250, 80, 150, 500 };
        case 2:
            // Config.cs writes values above the visible slider maxima for the
            // last preset. NightSharp exposes matching maxima so no value is
            // truncated while applying the source preset.
            return { 0, 0, 0, 0, 400, 200, 7, 20,
                     150, 0, 0, 200, 250 };
        default:
            return { 13, 20, 10, 30, 200, 100, 7, 20,
                     250, 250, 80, 250, 300 };
        }
    }
};

} // namespace Plugins::KuroEvade
