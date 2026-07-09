#pragma once

#include "../KuroEvadeDatabase.generated.h"
#include "../MathUtils.h"

#include "../../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::KuroEvade::SpecialSpells {

struct ExtraSpellCast {
    Vector3 Start;
    Vector3 End;
    Generated::SpellDataEntry Data;
    int OverrideStartTick = 0;
};

struct ProcessResult {
    Generated::SpellDataEntry Data;
    bool NoProcess = false;
    std::vector<ExtraSpellCast> ExtraSpells;
    std::vector<std::string> RemoveSpellNames;
};

using SpellLookupFn = const Generated::SpellDataEntry* (*)(const char*);

struct CastContext {
    const SDK::AIBaseClient& Caster;
    const SDK::Events::ProcessSpellEventArgs& Args;
    const Generated::SpellDataEntry& Source;
    SpellLookupFn Lookup = nullptr;
    Vector3 Start3;
    Vector3 End3;
    Vec2 Start;
    Vec2 End;
    Vec2 Direction;
};

inline bool EqualsSpell(const Generated::SpellDataEntry& data, const char* name) {
    return name && _stricmp(data.sdk.SpellName.c_str(), name) == 0;
}

inline bool EqualsText(const std::string& value, const char* name) {
    return name && !value.empty() && _stricmp(value.c_str(), name) == 0;
}

inline std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

inline bool NameContains(const char* name, const char* needle) {
    if (!name || !needle) {
        return false;
    }

    return ToLower(name).find(ToLower(needle)) != std::string::npos;
}

inline Vector3 From2D(const Vec2& value, float y) {
    return Vec3::From2D(value, y);
}

inline float ClampedCastDistance(const Vector3& start, const Vector3& end, float range) {
    return std::min(range, start.Distance(end));
}

inline Vec2 Perpendicular(const Vec2& value) {
    return Vec2(-value.y, value.x);
}

inline Vec2 SafeDirection(const Vec2& start, const Vec2& end, const SDK::AIBaseClient& caster) {
    Vec2 direction = (end - start).Normalized();
    if (direction.IsZero()) {
        direction = caster.Direction().To2D().Normalized();
    }
    if (direction.IsZero()) {
        direction = Vec2(1.0f, 0.0f);
    }
    return direction;
}

inline CastContext MakeCastContext(const SDK::AIBaseClient& caster,
                                   const SDK::Events::ProcessSpellEventArgs& args,
                                   const Generated::SpellDataEntry& source,
                                   SpellLookupFn lookup) {
    CastContext context{ caster, args, source, lookup };
    context.Start3 = args.StartPosition;
    context.End3 = args.EndPosition;
    context.Start = context.Start3.To2D();
    context.End = context.End3.To2D();
    context.Direction = SafeDirection(context.Start, context.End, caster);
    return context;
}

inline void AddExtra(ProcessResult& result,
                     const Vector3& start,
                     const Vector3& end,
                     const Generated::SpellDataEntry& data,
                     int overrideStartTick = 0) {
    result.ExtraSpells.push_back({ start, end, data, overrideStartTick });
}

inline bool ProjectOnSegment(const Vec2& point,
                             const Vec2& segmentStart,
                             const Vec2& segmentEnd,
                             Vec2& segmentPoint) {
    const Vec2 segment = segmentEnd - segmentStart;
    const float lengthSqr = segment.LengthSqr();
    if (lengthSqr <= 0.0001f) {
        segmentPoint = segmentStart;
        return false;
    }

    const float t = (point - segmentStart).Dot(segment) / lengthSqr;
    segmentPoint = segmentStart + segment * std::clamp(t, 0.0f, 1.0f);
    return t >= 0.0f && t <= 1.0f;
}

inline void RefreshLineGeometry(SDK::Skillshot& skillshot) {
    skillshot.Direction = (skillshot.EndPosition - skillshot.StartPosition).Normalized();

    auto* line = dynamic_cast<SDK::SkillshotLine*>(&skillshot);
    if (!line) {
        return;
    }
    if (!line->Rectangle) {
        line->Rectangle = std::make_unique<SDK::RectanglePoly>(
            skillshot.StartPosition,
            skillshot.EndPosition,
            static_cast<float>(skillshot.SData.Radius));
    }

    line->Rectangle->Start = skillshot.StartPosition;
    line->Rectangle->End = skillshot.EndPosition;
    line->Rectangle->Width = static_cast<float>(skillshot.SData.Radius);
    line->Rectangle->UpdatePolygon();
    line->Path = line->Rectangle->ToClipperPath();
}

inline void RefreshSkillshotGeometry(SDK::Skillshot& skillshot) {
    skillshot.Direction = (skillshot.EndPosition - skillshot.StartPosition).Normalized();

    if (auto* circle = dynamic_cast<SDK::SkillshotCircle*>(&skillshot)) {
        if (!circle->Circle) {
            circle->Circle = std::make_unique<SDK::CirclePoly>(
                skillshot.EndPosition,
                static_cast<float>(skillshot.SData.Radius),
                20);
        }
        circle->Circle->Center = skillshot.EndPosition;
        circle->Circle->Radius = static_cast<float>(skillshot.SData.Radius);
        circle->Circle->UpdatePolygon();
        circle->Path = circle->Circle->ToClipperPath();
        return;
    }

    if (auto* cone = dynamic_cast<SDK::SkillshotCone*>(&skillshot)) {
        if (!cone->Sector) {
            cone->Sector = std::make_unique<SDK::SectorPoly>(
                skillshot.StartPosition,
                skillshot.EndPosition,
                static_cast<float>(skillshot.SData.Angle) * 3.14159265358979323846f / 180.0f,
                static_cast<float>(skillshot.SData.Range),
                20);
        }
        cone->Sector->Center = skillshot.StartPosition;
        cone->Sector->Direction = skillshot.Direction;
        cone->Sector->Angle =
            static_cast<float>(skillshot.SData.Angle) * 3.14159265358979323846f / 180.0f;
        cone->Sector->Radius = static_cast<float>(skillshot.SData.Range);
        cone->Sector->UpdatePolygon();
        cone->Path = cone->Sector->ToClipperPath();
        return;
    }

    RefreshLineGeometry(skillshot);
}

} // namespace Plugins::KuroEvade::SpecialSpells
