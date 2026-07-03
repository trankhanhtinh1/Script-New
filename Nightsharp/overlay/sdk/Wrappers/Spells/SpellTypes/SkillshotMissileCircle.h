#pragma once

#include "SkillshotMissile.h"

#include "../../../Math/Polygons/CirclePoly.h"

#include <memory>

namespace SDK {

class SkillshotMissileCircle : public SkillshotMissile {
public:
    std::unique_ptr<CirclePoly> Circle;

    explicit SkillshotMissileCircle(const std::string& spellName)
        : SkillshotMissile(spellName) {
    }

    explicit SkillshotMissileCircle(const SpellDatabaseEntry& entry)
        : SkillshotMissile(entry) {
    }

    std::string ToString() const override {
        return "SkillshotMissileCircle: Champion=" + SData.ChampionName +
               " SpellName=" + SData.SpellName;
    }

protected:
    void UpdatePolygon() override {
        if (!Circle) {
            Circle = std::make_unique<CirclePoly>(
                EndPosition,
                static_cast<float>(SData.Radius),
                20);
            UpdatePath();
        }
    }

    void UpdatePath() override {
        if (Circle) {
            Path = Circle->ToClipperPath();
        }
    }
};

} // namespace SDK
