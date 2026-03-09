#pragma once
#include "SpellData.h"
#include "../../Wrappers/Spells/SpellDatabase.h"
#include <algorithm>
#include <climits>
#include <string>
#include <utility>
#include <vector>

// ============================================================================
// SpellDatabase
// EzEvade-facing spell database built from sdk/Data/Database.json via
// SDK::SpellDatabase. This keeps coverage aligned with the maintained wrapper
// database instead of duplicating hundreds of hand-written entries.
//
// Sources:
//   sdk/Wrappers/Spells/SpellDatabase.h
//   sdk/Data/Database.json
//   SpellDatabase.lua (reference for future alias/detection expansion)
// Patch target:
//   26.5 (2026-03-03)
// References:
//   - Riot patch notes 26.5
//   - League Wiki champion ability pages
// ============================================================================

namespace EzEvade {
namespace detail {

inline constexpr const char* kSpellDatabasePatchVersion = "26.5";

inline SpellSlotId ConvertSlot(SDK::SpellSlotId slot) {
    switch (slot) {
    case SDK::SpellSlotId::Q: return SpellSlotId::Q;
    case SDK::SpellSlotId::W: return SpellSlotId::W;
    case SDK::SpellSlotId::E: return SpellSlotId::E;
    case SDK::SpellSlotId::R: return SpellSlotId::R;
    case SDK::SpellSlotId::Summoner1: return SpellSlotId::F;
    case SDK::SpellSlotId::Summoner2: return SpellSlotId::T;
    default: return SpellSlotId::None;
    }
}

inline SpellType ConvertSpellType(SDK::SpellType type) {
    switch (type) {
    case SDK::SpellType::SkillshotCircle:
    case SDK::SpellType::SkillshotMissileCircle:
        return SpellType::Circular;
    case SDK::SpellType::SkillshotCone:
    case SDK::SpellType::SkillshotMissileCone:
        return SpellType::Cone;
    case SDK::SpellType::SkillshotRing:
        return SpellType::Ring;
    case SDK::SpellType::SkillshotArc:
        return SpellType::Arc;
    case SDK::SpellType::SkillshotMissileArc:
        return SpellType::MissileArc;
    case SDK::SpellType::SkillshotLine:
    case SDK::SpellType::SkillshotMissileLine:
    default:
        return SpellType::Line;
    }
}

inline std::vector<CollisionObjectType> ConvertCollisionObjects(int mask) {
    std::vector<CollisionObjectType> result;
    if ((mask & SDK::CollisionHeroes) != 0) {
        result.push_back(CollisionObjectType::EnemyChampions);
    }
    if ((mask & SDK::CollisionMinions) != 0) {
        result.push_back(CollisionObjectType::EnemyMinions);
    }
    if ((mask & SDK::CollisionYasuoWall) != 0) {
        result.push_back(CollisionObjectType::YasuoWall);
    }
    if ((mask & SDK::CollisionWalls) != 0) {
        result.push_back(CollisionObjectType::Terrain);
    }
    return result;
}

inline CCType ConvertCcType(const SDK::SpellDatabaseEntry& entry) {
    bool hasSoftCc = false;
    for (const auto& buff : entry.AppliedBuffsOnEnemies) {
        switch (buff) {
        case SDK::BuffType::Stun:
        case SDK::BuffType::Silence:
        case SDK::BuffType::Taunt:
        case SDK::BuffType::Berserk:
        case SDK::BuffType::Snare:
        case SDK::BuffType::Fear:
        case SDK::BuffType::Suppression:
        case SDK::BuffType::Asleep:
        case SDK::BuffType::Charm:
        case SDK::BuffType::Polymorph:
        case SDK::BuffType::Knockup:
        case SDK::BuffType::Knockback:
            return CCType::Hard;
        case SDK::BuffType::Slow:
        case SDK::BuffType::Blind:
        case SDK::BuffType::NearSight:
        case SDK::BuffType::Grounded:
        case SDK::BuffType::Drowsy:
        case SDK::BuffType::Flee:
            hasSoftCc = true;
            break;
        default:
            break;
        }
    }
    if (entry.HasTag(SDK::SpellTags::CrowdControl)) {
        return hasSoftCc ? CCType::Soft : CCType::Hard;
    }
    return hasSoftCc ? CCType::Soft : CCType::None;
}

inline float ConvertRadius(const SDK::SpellDatabaseEntry& entry) {
    switch (entry.Type) {
    case SDK::SpellType::SkillshotCircle:
    case SDK::SpellType::SkillshotMissileCircle:
    case SDK::SpellType::SkillshotRing:
        return static_cast<float>(entry.Radius > 0 ? entry.Radius : entry.Width);
    case SDK::SpellType::SkillshotCone:
    case SDK::SpellType::SkillshotMissileCone:
        return static_cast<float>(entry.Radius > 0 ? entry.Radius : entry.Width);
    default:
        return static_cast<float>(entry.Width > 0 ? entry.Width : entry.Radius);
    }
}

inline float ConvertRange(const SDK::SpellDatabaseEntry& entry) {
    float range = static_cast<float>(entry.Range);
    if (entry.ExtraRange > 0 && entry.Range < (INT_MAX / 2)) {
        range += static_cast<float>(entry.ExtraRange);
    }
    return range;
}

inline bool HasName(const SpellData& data, const std::string& value) {
    if (value.empty()) {
        return false;
    }
    if (data.spellName == value || data.missileName == value) {
        return true;
    }
    for (const auto& extra : data.extraSpellNames) {
        if (extra == value) {
            return true;
        }
    }
    for (const auto& extra : data.extraMissileNames) {
        if (extra == value) {
            return true;
        }
    }
    return false;
}

inline bool IsDuplicate(const std::vector<SpellData>& db, const SpellData& candidate) {
    for (const auto& existing : db) {
        if (existing.charName != candidate.charName) {
            continue;
        }
        if (HasName(existing, candidate.spellName) || HasName(existing, candidate.missileName)) {
            return true;
        }
    }
    return false;
}

inline void ApplyPatch265Normalization(SpellData& data) {
    // 26.5 introduced tuning changes (damage/cooldown focused) but no
    // evade-geometry reworks that require hard overrides here.
    // Keep this hook to make future patch updates localized.
    (void)data;
}

} // namespace detail

static std::vector<SpellData> BuildSpellDatabase() {
    SDK::SpellDatabase::Init();

    std::vector<SpellData> db;
    db.reserve(SDK::SpellDatabase::GetCount() + 1);

    // Global ARAM snowball entry.
    db.push_back({
        .charName="AllChampions",
        .name="Mark/Snowball",
        .spellName="SummonerSnowball",
        .missileName="SummonerSnowball",
        .extraSpellNames={"summonersnowball", "summonerporothrow"},
        .extraMissileNames={"summonersnowball"},
        .spellKey=SpellSlotId::F,
        .spellType=SpellType::Line,
        .radius=60.0f,
        .range=1600.0f,
        .spellDelay=0.0f,
        .projectileSpeed=1300.0f,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions},
        .dangerlevel=1,
        .detectionType=DetectionType::Missile
    });

    for (const auto& entry : SDK::SpellDatabase::Spells()) {
        if (!entry.IsSkillshot()) {
            continue;
        }

        bool missileBacked =
            entry.Type == SDK::SpellType::SkillshotMissileLine ||
            entry.Type == SDK::SpellType::SkillshotMissileCircle ||
            entry.Type == SDK::SpellType::SkillshotMissileCone ||
            entry.Type == SDK::SpellType::SkillshotMissileArc ||
            !entry.MissileSpellName.empty();

        SpellData data;
        data.charName = entry.ChampionName.empty() ? "AllChampions" : entry.ChampionName;
        data.name = entry.SpellName;
        data.spellName = entry.SpellName;
        data.missileName = entry.MissileSpellName;
        data.extraSpellNames = entry.ExtraSpellNames;
        data.extraMissileNames = entry.ExtraMissileNames;
        data.spellKey = detail::ConvertSlot(entry.Slot);
        data.spellType = detail::ConvertSpellType(entry.Type);
        data.radius = detail::ConvertRadius(entry);
        data.range = detail::ConvertRange(entry);
        data.angle = static_cast<float>(entry.Angle);
        data.secondaryRadius = static_cast<float>(entry.RingRadius);
        data.spellDelay = static_cast<float>(entry.Delay);
        data.projectileSpeed = missileBacked ? static_cast<float>(entry.MissileSpeed) : 0.0f;
        data.fixedRange = entry.FixedRange;
        data.isSpecial = entry.AvoidMaxRangeReduction || entry.MissileFollowsCaster ||
                         !entry.SourceObjectName.empty() || !entry.FromObject.empty() ||
                         !entry.FromObjects.empty();
        data.hasEndExplosion = entry.RingRadius > 0;
        data.collisionObjects = detail::ConvertCollisionObjects(entry.CollisionObjects);
        data.dangerlevel = entry.DangerValue > 0 ? entry.DangerValue : 1;
        data.detectionType = missileBacked ? DetectionType::Missile : DetectionType::CastSpell;
        data.ccType = detail::ConvertCcType(entry);
        if (!entry.SourceObjectName.empty()) {
            data.extraMissileNames.push_back(entry.SourceObjectName);
        }
        if (!entry.FromObject.empty()) {
            data.extraMissileNames.push_back(entry.FromObject);
        }
        for (const auto& source : entry.FromObjects) {
            if (!source.empty()) {
                data.extraMissileNames.push_back(source);
            }
        }

        detail::ApplyPatch265Normalization(data);

        if (!detail::IsDuplicate(db, data)) {
            db.push_back(std::move(data));
        }
    }

    std::stable_sort(db.begin() + 1, db.end(), [](const SpellData& left, const SpellData& right) {
        if (left.charName != right.charName) {
            return left.charName < right.charName;
        }
        if (left.spellKey != right.spellKey) {
            return static_cast<int>(left.spellKey) < static_cast<int>(right.spellKey);
        }
        if (left.spellName != right.spellName) {
            return left.spellName < right.spellName;
        }
        return left.missileName < right.missileName;
    });

    return db;
}

inline std::vector<SpellData>& GetSpellDatabase() {
    SDK::SpellDatabase::Init();

    static std::vector<SpellData> db;
    static size_t lastSourceCount = 0;

    const size_t sourceCount = SDK::SpellDatabase::GetCount();
    if (db.empty() || sourceCount != lastSourceCount || (db.size() <= 1 && sourceCount > 0)) {
        db = BuildSpellDatabase();
        lastSourceCount = SDK::SpellDatabase::GetCount();
    }

    return db;
}

inline std::vector<const SpellData*> GetSpellsForChampion(const std::string& champName) {
    std::vector<const SpellData*> result;
    for (auto& spell : GetSpellDatabase()) {
        if (spell.charName == champName || spell.charName == "AllChampions") {
            result.push_back(&spell);
        }
    }
    return result;
}

inline const SpellData* FindSpellByMissileName(const std::string& missileName) {
    for (auto& spell : GetSpellDatabase()) {
        if (spell.missileName == missileName) {
            return &spell;
        }
        for (auto& extra : spell.extraMissileNames) {
            if (extra == missileName) {
                return &spell;
            }
        }
    }
    return nullptr;
}

inline const SpellData* FindSpellBySpellName(const std::string& spellName) {
    for (auto& spell : GetSpellDatabase()) {
        if (spell.spellName == spellName) {
            return &spell;
        }
        for (auto& extra : spell.extraSpellNames) {
            if (extra == spellName) {
                return &spell;
            }
        }
    }
    return nullptr;
}

} // namespace EzEvade
