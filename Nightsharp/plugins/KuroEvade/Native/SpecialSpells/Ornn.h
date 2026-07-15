#pragma once

#include "SpecialSpellCommon.h"
#include "../Engine/Geometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroEvade::SpecialSpells::Ornn {

inline bool ProcessCast(const CastContext& context, ProcessResult& result) {
    if (!EqualsSpell(context.Source, "OrnnE")) {
        return false;
    }

    const Vec2 start = context.Start.IsZero()
        ? context.Caster.ServerPosition().To2D()
        : context.Start;
    const Vec2 direction = SafeDirection(start, context.End, context.Caster);
    constexpr float dashRange = 650.0f;
    constexpr float wallCheat = 150.0f;
    constexpr float shockwaveRadius = 360.0f;
    constexpr float dashSpeed = 1600.0f;
    const float height = context.Caster.IsValid()
        ? context.Caster.ServerPosition().y
        : context.Start3.y;

    // Ornn's centre travels at most 650 units, but terrain up to 150 units
    // ahead or beside his collision footprint can trigger the shockwave.
    // FirstTerrainCollision returns that centre-at-contact directly.
    Vec2 impact;
    const bool hitsTerrain = SourceGeometry::FirstTerrainCollision(
        start, start + direction * (dashRange + wallCheat), height,
        wallCheat, impact, 12.0f);
    if (!hitsTerrain || impact.IsZero() ||
        start.Distance(impact) > dashRange + 1.0f) {
        result.Data.Range = dashRange;
        result.Data.MissileSpeed = dashSpeed;
        result.Data.UseEndPosition = false;
        result.Data.FixedRange = false;
        result.Data.Finalize();
        return true;
    }

    const int startTick = SDK::Variables::TickCount() - SDK::Game::Ping() / 2;
    const float distance = std::max(1.0f, start.Distance(impact));

    Database::SpellData charge = context.Source;
    charge.Range = distance;
    charge.MissileSpeed = dashSpeed;
    charge.UseEndPosition = true;
    charge.FixedRange = false;
    charge.Finalize();

    Database::SpellData shockwave = context.Source;
    shockwave.DisplayName = "Searing Charge (E Wall Shockwave)";
    shockwave.Type = Database::SkillShotType::SkillshotCircle;
    shockwave.Range = shockwaveRadius;
    shockwave.Radius = shockwaveRadius;
    shockwave.MissileSpeed = 0.0f;
    shockwave.Delay = std::max(0, context.Source.Delay) +
        static_cast<int>(std::lround(1000.0f * distance / dashSpeed));
    shockwave.ExtraEndTime = 120;
    shockwave.UseEndPosition = true;
    shockwave.FixedRange = false;
    shockwave.HasEndExplosion = false;
    shockwave.CollisionObjects.clear();
    shockwave.Finalize();

    AddExtra(result,
             From2D(start, height), From2D(impact, height),
             charge, startTick, true);
    AddExtra(result,
             From2D(impact, height), From2D(impact, height),
             shockwave, startTick, true);
    result.NoProcess = true;
    return true;
}

} // namespace Plugins::KuroEvade::SpecialSpells::Ornn
