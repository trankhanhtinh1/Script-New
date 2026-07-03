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

            if (!SData.FromObject.empty() || !SData.FromObjects.empty()) {
                const auto matchesSourceObject = [this](const std::string& objectName) {
                    if (!SData.FromObject.empty() &&
                        objectName.find(SData.FromObject) != std::string::npos) {
                        return true;
                    }

                    return std::any_of(
                        SData.FromObjects.begin(),
                        SData.FromObjects.end(),
                        [&objectName](const std::string& sourceName) {
                            return !sourceName.empty() &&
                                   objectName.find(sourceName) != std::string::npos;
                        });
                };

                for (const auto& object : ObjectManager::Get<GameObject>()) {
                    if (matchesSourceObject(object.Name())) {
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
        }

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

    void Draw(std::uint32_t color, std::uint32_t /*missileColor*/, int borderWidth = 1) override {
        if (Path.empty()) {
            return;
        }

        const float height = GameObjects::PlayerPosition().y;
        const std::uint32_t drawColor = color != 0 ? color : 0xFFFFFFFFu;
        const float lineWidth = static_cast<float>(std::max(1, borderWidth));

        for (std::size_t i = 0; i < Path.size(); ++i) {
            const auto& startPoint = Path[i];
            const auto& endPoint = Path[(i + 1 == Path.size()) ? 0 : i + 1];
            const Vector3 startWorld(
                static_cast<float>(startPoint.X),
                height,
                static_cast<float>(startPoint.Y));
            const Vector3 endWorld(
                static_cast<float>(endPoint.X),
                height,
                static_cast<float>(endPoint.Y));

            Vector2 startScreen = {};
            Vector2 endScreen = {};
            if (!Drawing::WorldToScreen(startWorld, startScreen) ||
                !Drawing::WorldToScreen(endWorld, endScreen)) {
                continue;
            }

            Drawing::DrawLine(
                startScreen.x,
                startScreen.y,
                endScreen.x,
                endScreen.y,
                lineWidth,
                drawColor,
                true);
        }
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
