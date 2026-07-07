#pragma once

#include "../../../Enumerations/CastTypes.h"
#include "../../../Enumerations/CollisionableObjects.h"
#include "../../../Enumerations/SpellSlot.h"
#include "../../../Enumerations/SpellTags.h"
#include "../../../Enumerations/SpellType.h"
#include "../../../Enums/BuffType.h"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace SDK {

struct SpellDatabaseEntry {
    int Angle = 45;
    std::vector<BuffType> AppliedBuffsOnAllies;
    std::vector<BuffType> AppliedBuffsOnEnemies;
    std::vector<BuffType> AppliedBuffsOnSelf;
    std::string AppliedBuffOnSelfName;
    std::string AppliedBuffOnAllyName;
    std::string AppliedBuffOnEnemyName;
    std::string AppliedBuffName;
    bool AvoidMaxRangeReduction = false;
    bool FixedRange = false;
    bool CanBeRemoved = false;
    std::vector<SDK::CastType> CastType;
    std::string ChampionName;
    std::vector<SDK::CollisionableObjects> CollisionObjects;
    int DangerValue = 1;
    int Delay = 250;
    std::vector<std::string> ExtraMissileNames;
    int ExtraRange = 0;
    std::vector<std::string> ExtraSpellNames;
    bool ForceRemove = false;
    std::string FromObject;
    std::vector<std::string> FromObjects;
    bool IsDangerous = false;
    int MissileAccel = 0;
    bool MissileDelayed = false;
    bool MissileFollowsCaster = false;
    int MissileMaxSpeed = 0;
    int MissileMinSpeed = 0;
    int MissileSpeed = 1000;
    std::string MissileSpellName;
    int ArcAngle = 0;
    int RingRadius = 0;
    int Radius = 0;
    int Range = std::numeric_limits<int>::max();
    bool ResetsAutoAttackTimer = false;
    SDK::SpellSlot Slot = SDK::SpellSlot::Q;
    std::string SourceObjectName;
    std::string SpellName;
    std::vector<SDK::SpellTags> SpellTags;
    SDK::SpellType SpellType = SDK::SpellType::SkillshotCircle;
    std::string ToggleParticleName;
    int Width = 50;
    int MinChannelDuration = 0;
    int MaxChannelDuration = 0;

    SpellDatabaseEntry() = default;

    SpellDatabaseEntry(
        std::string championName,
        std::string spellName,
        SDK::SpellSlot slot,
        SDK::SpellType spellType,
        std::vector<SDK::CastType> castType,
        std::vector<SDK::SpellTags> spellTags,
        bool resetsAutoAttackTimer = false,
        int range = std::numeric_limits<int>::max(),
        int delay = 250,
        int radius = 50,
        int width = 300,
        int missileSpeed = 1400,
        int angle = 360,
        int defaultDangerValue = 1)
        : Angle(angle),
          CastType(std::move(castType)),
          ChampionName(std::move(championName)),
          DangerValue(defaultDangerValue),
          Delay(delay),
          MissileSpeed(missileSpeed),
          Radius(radius),
          Range(range),
          ResetsAutoAttackTimer(resetsAutoAttackTimer),
          Slot(slot),
          SpellName(std::move(spellName)),
          SpellTags(std::move(spellTags)),
          SpellType(spellType),
          Width(width) {
    }
};

} // namespace SDK
