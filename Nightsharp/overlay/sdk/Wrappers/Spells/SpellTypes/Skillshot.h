#pragma once

#include "BaseSpell.h"

#include "../../../GameObjects/ObjectManager.h"
#include "../../../Math/Polygons/Polygon.h"
#include "../../../Utils/Logging.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace SDK {

class Skillshot : public BaseSpell {
public:
    Vector2 Direction = {};
    std::vector<Clipper::IntPoint> Path;

    explicit Skillshot(const std::string& spellName)
        : BaseSpell(spellName) {
    }

    explicit Skillshot(const SpellDatabaseEntry& entry)
        : BaseSpell(entry) {
    }

    std::string ToString() const override {
        return "Skillshot: Champion=" + SData.ChampionName +
               " SpellName=" + SData.SpellName;
    }

    virtual bool Process() {
        if (DetectionType == SkillshotDetectionType::ProcessSpell) {
            StartPosition = {};

            if (!SData.FromObject.empty()) {
                for (const auto& object : ObjectManager::Get<GameObject>()) {
                    if (object.Name().find(SData.FromObject) != std::string::npos) {
                        StartPosition = object.Position().To2D();
                        break;
                    }
                }

                if (StartPosition.IsZero()) {
                    Utils::Logging::Write()(LogLevel::Warn,
                        "[Skillshot] Couldn't find start position for %s FromObject=%s",
                        ToString().c_str(),
                        SData.FromObject.c_str());
                    return false;
                }
            } else if (Caster.IsValid()) {
                StartPosition = Caster.Position().To2D();
            }

            // TODO(SDK parity): port FromObjects and spell-specific source rewrites.
        }

        // TODO(SDK parity): add MissileCreate-specific position corrections.

        Direction = (EndPosition - StartPosition).Normalized();
        const float range = static_cast<float>(SData.Range);
        if (((!SData.AvoidMaxRangeReduction) &&
             StartPosition.DistanceSqr(EndPosition) > range * range) ||
            SData.FixedRange) {
            EndPosition = StartPosition + Direction * range;
        }

        if (SData.ExtraRange != 0) {
            const float extra = std::min(
                static_cast<float>(SData.ExtraRange),
                range - EndPosition.Distance(StartPosition));
            EndPosition = EndPosition + Direction * extra;
        }

        UpdatePolygon();
        return true;
    }

    void Draw(std::uint32_t /*color*/, std::uint32_t /*missileColor*/, int /*borderWidth*/ = 1) override {
        // TODO(SDK parity): existing Polygon drawing uses color-only; wire
        // this to the evade renderer once visual debug surfaces are ported.
    }

    static float AngleBetween(const Vector2& lhs, const Vector2& rhs) {
        const Vector2 a = lhs.Normalized();
        const Vector2 b = rhs.Normalized();
        if (a.IsZero() || b.IsZero()) {
            return 180.0f;
        }

        const float dot = std::clamp(a.Dot(b), -1.0f, 1.0f);
        return std::acos(dot) * 180.0f / 3.14159265358979323846f;
    }

protected:
    virtual void UpdatePolygon() {
    }

    virtual void UpdatePath() {
    }
};

} // namespace SDK
