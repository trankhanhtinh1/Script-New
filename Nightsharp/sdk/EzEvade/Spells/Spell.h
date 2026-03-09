#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Spells/SpellData.h"
#include "sdk/EzEvade/Helpers/ObjectCache.h"
#include "sdk/EzEvade/Utils/EvadeUtils.h"
#include "sdk/EzEvade/Utils/MathUtils.h"
#include <cfloat>
#include <memory>
#include <string>

namespace EzEvade {

class Spell {
public:
    float StartTime = 0.0f;
    float EndTime = 0.0f;
    Vec2 StartPos = Vec2();
    Vec2 EndPos = Vec2();
    Vec2 Direction = Vec2();
    float Height = 0.0f;
    int HeroID = 0;
    int ProjectileID = 0;
    std::shared_ptr<SpellData> Info = nullptr;
    int SpellID = 0;
    SDK::GameObject SpellObject = SDK::GameObject();
    SpellType Type = SpellType::None;

    Vec2 CnLeft = Vec2();
    Vec2 CnRight = Vec2();
    Vec2 CnStart = Vec2();
    Vec2 CurrentSpellPosition = Vec2();
    Vec2 CurrentNegativePosition = Vec2();
    Vec2 PredictedEndPos = Vec2();

    float Radius = 0.0f;
    int Dangerlevel = 1;
    float EvadeTime = -FLT_MAX;
    float SpellHitTime = -FLT_MAX;
};

namespace SpellExtensions {

inline int GetSpellDangerLevel(const Spell& spell) {
    if (!spell.Info) return 1;
    if (!ObjectCache::Menu.Menu) return spell.Info->dangerlevel;

    const std::string key = spell.Info->spellName + "DangerLevel";
    auto* item = ObjectCache::Menu.Get(key);
    auto* list = dynamic_cast<SDK::MenuUI::MenuList*>(item);
    if (!list || list->Items.empty()) {
        return spell.Info->dangerlevel;
    }

    const int idx = std::clamp(list->Index, 0, (int)list->Items.size() - 1);
    const std::string val = list->Items[(size_t)idx];
    if (_stricmp(val.c_str(), "Low") == 0) return 1;
    if (_stricmp(val.c_str(), "High") == 0) return 3;
    if (_stricmp(val.c_str(), "Extreme") == 0) return 4;
    return 2;
}

inline std::string GetSpellDangerString(const Spell& spell) {
    const int level = GetSpellDangerLevel(spell);
    if (level == 1) return "Low";
    if (level == 3) return "High";
    if (level == 4) return "Extreme";
    return "Normal";
}

inline float GetSpellRadius(const Spell& spell) {
    if (!spell.Info) return 0.0f;

    float radius = spell.Info->radius;
    if (ObjectCache::Menu.Menu) {
        const std::string key = spell.Info->spellName + "SpellRadius";
        radius = (float)ObjectCache::Menu.GetSlider(key, (int)spell.Info->radius);
    }
    const float extraRadius = (float)ObjectCache::Menu.GetSlider("ExtraSpellRadius", 0);

    if (spell.Info->hasEndExplosion && spell.Type == SpellType::Circular) {
        return spell.Info->secondaryRadius + extraRadius;
    }

    if (spell.Type == SpellType::Arc) {
        const float spellRange = spell.StartPos.Distance(spell.EndPos);
        const float arcRadius = spell.Info->radius * (1.0f + spellRange / 100.0f) + extraRadius;
        return arcRadius;
    }

    return radius + extraRadius;
}

inline bool HasProjectile(const Spell& spell) {
    if (!spell.Info) return false;
    return spell.Info->projectileSpeed > 0.0f && spell.Info->projectileSpeed < FLT_MAX;
}

inline Vec2 GetSpellEndPosition(const Spell& spell) {
    return spell.PredictedEndPos.IsZero() ? spell.EndPos : spell.PredictedEndPos;
}

inline Vec2 GetCurrentSpellPosition(const Spell& spell, bool allowNegative = false, float delay = 0.0f,
                                    float extraDistance = 0.0f) {
    if (!spell.Info) return spell.StartPos;

    Vec2 spellPos = spell.StartPos;
    if (!spell.Info->updatePosition) {
        return spellPos;
    }

    if (spell.Type == SpellType::Line || spell.Type == SpellType::Arc) {
        const float spellTime = EvadeUtils::TickCount() - spell.StartTime - spell.Info->spellDelay
                              - std::max(0.0f, spell.Info->extraEndTime);

        if (spell.Info->projectileSpeed >= FLT_MAX) {
            return spell.StartPos;
        }

        if (spellTime >= 0.0f || allowNegative) {
            spellPos = spell.StartPos + spell.Direction * spell.Info->projectileSpeed * (spellTime / 1000.0f);
        }
    } else if (spell.Type == SpellType::Circular || spell.Type == SpellType::Cone) {
        spellPos = spell.EndPos;
    }

    if (spell.SpellObject.IsValid() && spell.SpellObject.IsVisible()
        && spell.SpellObject.GetPosition().To2D().Distance(ObjectCache::MyHeroCache.ServerPos2D) < spell.Info->range + 1000.0f) {
        spellPos = spell.SpellObject.GetPosition().To2D();
    }

    if (delay > 0.0f && spell.Info->projectileSpeed < FLT_MAX && spell.Type == SpellType::Line) {
        spellPos = spellPos + spell.Direction * spell.Info->projectileSpeed * (delay / 1000.0f);
    }

    if (extraDistance > 0.0f && spell.Info->projectileSpeed < FLT_MAX && spell.Type == SpellType::Line) {
        spellPos = spellPos + spell.Direction * extraDistance;
    }

    return spellPos;
}

inline void UpdateSpellInfo(Spell& spell) {
    spell.CurrentSpellPosition = GetCurrentSpellPosition(spell);
    spell.CurrentNegativePosition = GetCurrentSpellPosition(spell, true, 0.0f);
    spell.Dangerlevel = GetSpellDangerLevel(spell);
}

inline float GetSpellHitTime(const Spell& spell, const Vec2& pos) {
    if (!spell.Info) return FLT_MAX;

    switch (spell.Type) {
    case SpellType::Line: {
        if (spell.Info->projectileSpeed >= FLT_MAX) {
            return std::max(0.0f, spell.EndTime - EvadeUtils::TickCount() - ObjectCache::GamePing);
        }
        const Vec2 spellPos = GetCurrentSpellPosition(spell, true, ObjectCache::GamePing);
        return 1000.0f * spellPos.Distance(pos) / spell.Info->projectileSpeed;
    }
    case SpellType::Cone:
    case SpellType::Circular:
        return std::max(0.0f, spell.EndTime - EvadeUtils::TickCount() - ObjectCache::GamePing);
    default:
        break;
    }

    return FLT_MAX;
}

inline bool LineIntersectLinearSpell(const Spell& spell, const Vec2& a, const Vec2& b) {
    const auto& player = SDK::GameObjects::Player;
    const float myBoundingRadius = player.IsValid() ? player.GetBoundingRadius() : 65.0f;
    const Vec2 spellDir = spell.Direction;
    const Vec2 pSpellDir = spell.Direction.Perpendicular();
    const float spellRadius = spell.Radius;
    const Vec2 spellPos = spell.CurrentSpellPosition;
    const Vec2 endPos = GetSpellEndPosition(spell);

    const Vec2 startRightPos = spellPos + pSpellDir * (spellRadius + myBoundingRadius);
    const Vec2 startLeftPos = spellPos - pSpellDir * (spellRadius + myBoundingRadius);
    const Vec2 endRightPos = endPos + pSpellDir * (spellRadius + myBoundingRadius);
    const Vec2 endLeftPos = endPos - pSpellDir * (spellRadius + myBoundingRadius);

    const bool int1 = MathUtils::CheckLineIntersection(a, b, startRightPos, startLeftPos);
    const bool int2 = MathUtils::CheckLineIntersection(a, b, endRightPos, endLeftPos);
    const bool int3 = MathUtils::CheckLineIntersection(a, b, startRightPos, endRightPos);
    const bool int4 = MathUtils::CheckLineIntersection(a, b, startLeftPos, endLeftPos);

    return int1 || int2 || int3 || int4;
}

inline bool CanHeroEvade(const Spell& spell, const SDK::GameObject& hero,
                         float& outEvadeTime, float& outSpellHitTime) {
    outEvadeTime = 0.0f;
    outSpellHitTime = FLT_MAX;
    if (!spell.Info || !hero.IsValid()) {
        return false;
    }

    const Vec2 heroPos = hero.GetServerPosition().To2D();
    const float speed = hero.GetMoveSpeed();
    const float delay = ObjectCache::GamePing;

    if (speed <= 0.0f) {
        return false;
    }

    if (spell.Type == SpellType::Line || spell.Type == SpellType::Arc) {
        Vec2 proj = SDK::GeometryAdv::ProjectOn(heroPos, spell.StartPos, spell.EndPos).SegmentPoint;
        outEvadeTime = 1000.0f * (spell.Radius - heroPos.Distance(proj) + hero.GetBoundingRadius()) / speed;
        outSpellHitTime = GetSpellHitTime(spell, proj);
    } else if (spell.Type == SpellType::Circular) {
        outEvadeTime = 1000.0f * (spell.Radius - heroPos.Distance(spell.EndPos)) / speed;
        outSpellHitTime = GetSpellHitTime(spell, heroPos);
    } else if (spell.Type == SpellType::Cone) {
        auto p1 = SDK::GeometryAdv::ProjectOn(heroPos, spell.CnStart, spell.CnLeft).SegmentPoint;
        auto p2 = SDK::GeometryAdv::ProjectOn(heroPos, spell.CnLeft, spell.CnRight).SegmentPoint;
        auto p3 = SDK::GeometryAdv::ProjectOn(heroPos, spell.CnRight, spell.CnStart).SegmentPoint;
        Vec2 p = p1;
        float best = p1.Distance(heroPos);
        if (p2.Distance(heroPos) < best) { best = p2.Distance(heroPos); p = p2; }
        if (p3.Distance(heroPos) < best) { best = p3.Distance(heroPos); p = p3; }

        outEvadeTime = 1000.0f * (spell.Info->range / 2.0f - heroPos.Distance(p) + hero.GetBoundingRadius()) / speed;
        outSpellHitTime = GetSpellHitTime(spell, heroPos);
    }

    return (outSpellHitTime - delay) > outEvadeTime;
}

} // namespace SpellExtensions
} // namespace EzEvade
