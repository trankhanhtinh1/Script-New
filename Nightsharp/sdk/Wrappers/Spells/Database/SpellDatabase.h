#pragma once

#include "SpellDatabaseEntry.h"

#include "../../../Data/Database.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace SDK {

class SpellDatabase {
public:
    using Entry = SpellDatabaseEntry;

    static const std::vector<Entry>& Spells() {
        return SpellsList();
    }

    static const std::vector<Entry>& Entries() {
        return Spells();
    }

    static const std::vector<Data::SpellData>& RawEntries() {
        return Data::GetSpellDatabase();
    }

    static std::vector<const Entry*> Get(std::function<bool(const Entry&)> predicate = {}) {
        std::vector<const Entry*> result;
        for (const auto& entry : Spells()) {
            if (!predicate || predicate(entry)) {
                result.push_back(&entry);
            }
        }
        return result;
    }

    static const Entry* GetByMissileName(std::string_view missileSpellName) {
        const auto needle = ToLower(missileSpellName);
        if (needle.empty()) {
            return nullptr;
        }

        for (const auto& entry : Spells()) {
            if (!entry.MissileSpellName.empty() && EqualsLower(entry.MissileSpellName, needle)) {
                return &entry;
            }
            if (ContainsLower(entry.ExtraMissileNames, needle)) {
                return &entry;
            }
        }
        return nullptr;
    }

    static const Entry* GetByName(std::string_view spellName) {
        const auto needle = ToLower(spellName);
        if (needle.empty()) {
            return nullptr;
        }

        for (const auto& entry : Spells()) {
            if (EqualsLower(entry.SpellName, needle) || ContainsLower(entry.ExtraSpellNames, needle)) {
                return &entry;
            }
        }
        return nullptr;
    }

    static const Entry* GetBySourceObjectName(std::string_view objectName) {
        const auto objectNameLower = ToLower(objectName);
        if (objectNameLower.empty()) {
            return nullptr;
        }

        for (const auto& entry : Spells()) {
            if (!entry.SourceObjectName.empty() &&
                objectNameLower.find(ToLower(entry.SourceObjectName)) != std::string::npos) {
                return &entry;
            }
        }
        return nullptr;
    }

    static const Entry* FindBySpellName(std::string_view championName, std::string_view spellName) {
        const auto championNeedle = ToLower(championName);
        const auto spellNeedle = ToLower(spellName);
        if (spellNeedle.empty()) {
            return nullptr;
        }

        for (const auto& entry : Spells()) {
            if (!championNeedle.empty() && !EqualsLower(entry.ChampionName, championNeedle)) {
                continue;
            }
            if (EqualsLower(entry.SpellName, spellNeedle) || ContainsLower(entry.ExtraSpellNames, spellNeedle)) {
                return &entry;
            }
        }
        return nullptr;
    }

private:
    static const std::vector<Entry>& SpellsList() {
        static const std::vector<Entry> spells = BuildEntries();
        return spells;
    }

    static std::vector<Entry> BuildEntries() {
        std::vector<Entry> spells;
        const auto& rawEntries = Data::GetSpellDatabase();
        spells.reserve(rawEntries.size());

        for (const auto& raw : rawEntries) {
            spells.push_back(FromData(raw));
        }
        return spells;
    }

    static Entry FromData(const Data::SpellData& raw) {
        Entry entry;

        entry.Angle = ToInt(raw.angle, entry.Angle);
        entry.AppliedBuffOnSelfName = raw.appliedBuffOnSelfName;
        entry.AppliedBuffOnAllyName = raw.appliedBuffOnAllyName;
        entry.AppliedBuffOnEnemyName = raw.appliedBuffOnEnemyName;
        entry.AppliedBuffName = raw.appliedBuffName;
        entry.AvoidMaxRangeReduction = raw.avoidMaxRangeReduction;
        entry.FixedRange = raw.fixedRange;
        entry.CanBeRemoved = raw.canBeRemoved;
        entry.CastType = ToCastTypes(raw.castTypeMask);
        entry.ChampionName = raw.charName;
        entry.CollisionObjects = ToCollisionObjects(raw);
        entry.DangerValue = raw.dangerValue != 1 ? raw.dangerValue : raw.dangerlevel;
        entry.Delay = ToInt(raw.spellDelay, entry.Delay);
        entry.ExtraMissileNames = raw.extraMissileNames;
        entry.ExtraRange = ToInt(raw.extraRange, entry.ExtraRange);
        entry.ExtraSpellNames = raw.extraSpellNames;
        entry.ForceRemove = raw.forceRemove;
        entry.FromObject = raw.fromObject;
        entry.FromObjects = raw.fromObjects;
        entry.IsDangerous = raw.isDangerous;
        entry.MissileAccel = ToInt(raw.missileAccel, entry.MissileAccel);
        entry.MissileDelayed = raw.missileDelayed;
        entry.MissileFollowsCaster = raw.missileFollowsCaster;
        entry.MissileMaxSpeed = ToInt(raw.missileMaxSpeed, entry.MissileMaxSpeed);
        entry.MissileMinSpeed = ToInt(raw.missileMinSpeed, entry.MissileMinSpeed);
        entry.MissileSpeed = ToMissileSpeed(raw.projectileSpeed, entry.MissileSpeed);
        entry.MissileSpellName = !raw.missileSpellName.empty() ? raw.missileSpellName : raw.missileName;
        entry.ArcAngle = ToInt(raw.arcAngle, entry.ArcAngle);
        entry.RingRadius = ToInt(raw.ringRadius, entry.RingRadius);
        entry.Radius = ToInt(raw.radius, entry.Radius);
        entry.Range = raw.range > 0.0f ? ToInt(raw.range, entry.Range) : entry.Range;
        entry.ResetsAutoAttackTimer = raw.resetsAutoAttackTimer;
        entry.Slot = raw.spellKey;
        entry.SourceObjectName = raw.sourceObjectName;
        entry.SpellName = raw.spellName;
        entry.SpellTags = ToSpellTags(raw.spellTagsMask);
        entry.SpellType = ToPublicSpellType(raw);
        entry.ToggleParticleName = raw.toggleParticleName;
        entry.Width = ToInt(raw.width, entry.Width);
        entry.MinChannelDuration = ToInt(raw.minChannelDuration, entry.MinChannelDuration);
        entry.MaxChannelDuration = ToInt(raw.maxChannelDuration, entry.MaxChannelDuration);

        return entry;
    }

    static std::vector<SDK::CastType> ToCastTypes(std::uint32_t mask) {
        std::vector<SDK::CastType> values;
        AddIf(values, mask, Data::CastType_EnemyChampions, SDK::CastType::EnemyChampions);
        AddIf(values, mask, Data::CastType_EnemyMinions, SDK::CastType::EnemyMinions);
        AddIf(values, mask, Data::CastType_EnemyTurrets, SDK::CastType::EnemyTurrets);
        AddIf(values, mask, Data::CastType_AllyChampions, SDK::CastType::AllyChampions);
        AddIf(values, mask, Data::CastType_AllyMinions, SDK::CastType::AllyMinions);
        AddIf(values, mask, Data::CastType_AllyTurrets, SDK::CastType::AllyTurrets);
        AddIf(values, mask, Data::CastType_Self, SDK::CastType::Self);
        AddIf(values, mask, Data::CastType_Position, SDK::CastType::Position);
        AddIf(values, mask, Data::CastType_Direction, SDK::CastType::Direction);
        return values;
    }

    static std::vector<SDK::CollisionableObjects> ToCollisionObjects(const Data::SpellData& raw) {
        std::vector<SDK::CollisionableObjects> values;
        AddIf(values, raw.collisionObjectsMask, Data::Collision_Minions, SDK::CollisionableObjects::Minions);
        AddIf(values, raw.collisionObjectsMask, Data::Collision_Heroes, SDK::CollisionableObjects::Heroes);
        AddIf(values, raw.collisionObjectsMask, Data::Collision_YasuoWall, SDK::CollisionableObjects::YasuoWall);
        AddIf(values, raw.collisionObjectsMask, Data::Collision_BraumShield, SDK::CollisionableObjects::BraumShield);
        AddIf(values, raw.collisionObjectsMask, Data::Collision_Walls, SDK::CollisionableObjects::Walls);

        for (const auto object : raw.collisionObjects) {
            switch (object) {
            case Data::EnemyChampions:
                AddUnique(values, SDK::CollisionableObjects::Heroes);
                break;
            case Data::EnemyMinions:
                AddUnique(values, SDK::CollisionableObjects::Minions);
                break;
            case Data::YasuoWall:
                AddUnique(values, SDK::CollisionableObjects::YasuoWall);
                break;
            case Data::BraumShield:
                AddUnique(values, SDK::CollisionableObjects::BraumShield);
                break;
            case Data::Walls:
                AddUnique(values, SDK::CollisionableObjects::Walls);
                break;
            default:
                break;
            }
        }
        return values;
    }

    static std::vector<SDK::SpellTags> ToSpellTags(std::uint32_t mask) {
        std::vector<SDK::SpellTags> values;
        AddIf(values, mask, Data::Tag_Damage, SDK::SpellTags::Damage);
        AddIf(values, mask, Data::Tag_AoE, SDK::SpellTags::AoE);
        AddIf(values, mask, Data::Tag_AppliesOnHitEffects, SDK::SpellTags::AppliesOnHitEffects);
        AddIf(values, mask, Data::Tag_CrowdControl, SDK::SpellTags::CrowdControl);
        AddIf(values, mask, Data::Tag_Shield, SDK::SpellTags::Shield);
        AddIf(values, mask, Data::Tag_Heal, SDK::SpellTags::Heal);
        AddIf(values, mask, Data::Tag_Dash, SDK::SpellTags::Dash);
        AddIf(values, mask, Data::Tag_Blink, SDK::SpellTags::Blink);
        return values;
    }

    static SDK::SpellType ToPublicSpellType(const Data::SpellData& raw) {
        const bool hasMissile =
            raw.isMissile ||
            !raw.missileName.empty() ||
            !raw.missileSpellName.empty() ||
            !raw.extraMissileNames.empty();

        switch (raw.type) {
        case Data::SkillshotType::MissileLine:
            return SDK::SpellType::SkillshotMissileLine;
        case Data::SkillshotType::Circle:
            return hasMissile ? SDK::SpellType::SkillshotMissileCircle : SDK::SpellType::SkillshotCircle;
        case Data::SkillshotType::Cone:
            return hasMissile ? SDK::SpellType::SkillshotMissileCone : SDK::SpellType::SkillshotCone;
        case Data::SkillshotType::Ring:
            return SDK::SpellType::SkillshotRing;
        case Data::SkillshotType::Arc:
            return SDK::SpellType::SkillshotArc;
        case Data::SkillshotType::MissileArc:
            return SDK::SpellType::SkillshotMissileArc;
        case Data::SkillshotType::None:
            return hasMissile ? SDK::SpellType::TargetedMissile : SDK::SpellType::Targeted;
        case Data::SkillshotType::Line:
        default:
            break;
        }

        switch (raw.spellType) {
        case Data::SpellType::Line:
            return hasMissile ? SDK::SpellType::SkillshotMissileLine : SDK::SpellType::SkillshotLine;
        case Data::SpellType::Circular:
            return hasMissile ? SDK::SpellType::SkillshotMissileCircle : SDK::SpellType::SkillshotCircle;
        case Data::SpellType::Cone:
            return hasMissile ? SDK::SpellType::SkillshotMissileCone : SDK::SpellType::SkillshotCone;
        case Data::SpellType::Arc:
            return hasMissile ? SDK::SpellType::SkillshotMissileArc : SDK::SpellType::SkillshotArc;
        case Data::SpellType::None:
        default:
            return hasMissile ? SDK::SpellType::TargetedMissile : SDK::SpellType::Targeted;
        }
    }

    template <typename T>
    static void AddUnique(std::vector<T>& values, T value) {
        if (std::find(values.begin(), values.end(), value) == values.end()) {
            values.push_back(value);
        }
    }

    template <typename T>
    static void AddIf(std::vector<T>& values, std::uint32_t mask, std::uint32_t flag, T value) {
        if ((mask & flag) != 0) {
            AddUnique(values, value);
        }
    }

    static int ToInt(float value, int fallback) {
        if (!std::isfinite(value)) {
            return fallback;
        }
        if (value > static_cast<float>(std::numeric_limits<int>::max())) {
            return std::numeric_limits<int>::max();
        }
        if (value < static_cast<float>(std::numeric_limits<int>::min())) {
            return std::numeric_limits<int>::min();
        }
        return static_cast<int>(std::lround(value));
    }

    static int ToMissileSpeed(float value, int fallback) {
        if (!std::isfinite(value) || value <= 0.0f || value > 10000000.0f) {
            return fallback;
        }
        return ToInt(value, fallback);
    }

    static std::string ToLower(std::string_view value) {
        std::string lower(value);
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return lower;
    }

    static bool EqualsLower(std::string_view value, std::string_view lowerNeedle) {
        return ToLower(value) == lowerNeedle;
    }

    static bool ContainsLower(const std::vector<std::string>& values, std::string_view lowerNeedle) {
        return std::any_of(values.begin(), values.end(), [&](const std::string& value) {
            return EqualsLower(value, lowerNeedle);
        });
    }
};

} // namespace SDK
