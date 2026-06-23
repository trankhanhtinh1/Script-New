#pragma once

#include "Skillshot.h"

#include "../../../Math/Polygons/CirclePoly.h"

#include <memory>

namespace SDK {

class SkillshotCircle : public Skillshot {
public:
    std::unique_ptr<CirclePoly> Circle;

    explicit SkillshotCircle(const std::string& spellName)
        : Skillshot(spellName) {
    }

    explicit SkillshotCircle(const SpellDatabaseEntry& entry)
        : Skillshot(entry) {
    }

    std::string ToString() const override {
        return "SkillshotCircle: Champion=" + SData.ChampionName +
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
