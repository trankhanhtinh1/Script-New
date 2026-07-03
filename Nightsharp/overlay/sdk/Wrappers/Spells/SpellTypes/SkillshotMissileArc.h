#pragma once

#include "SkillshotMissile.h"

#include "../../../Math/Polygons/ArcPoly.h"

#include <memory>

namespace SDK {

class SkillshotMissileArc : public SkillshotMissile {
public:
    std::unique_ptr<ArcPoly> Arc;

    explicit SkillshotMissileArc(const std::string& spellName)
        : SkillshotMissile(spellName) {
    }

    explicit SkillshotMissileArc(const SpellDatabaseEntry& entry)
        : SkillshotMissile(entry) {
    }

    std::string ToString() const override {
        return "SkillshotMissileArc: Champion=" + SData.ChampionName +
               " SpellName=" + SData.SpellName;
    }

protected:
    void UpdatePolygon() override {
        if (!Arc) {
            Arc = std::make_unique<ArcPoly>(
                StartPosition,
                EndPosition,
                static_cast<float>(SData.ArcAngle),
                static_cast<float>(SData.Radius),
                20);
            UpdatePath();
        }
    }

    void UpdatePath() override {
        if (Arc) {
            Path = Arc->ToClipperPath();
        }
    }
};

} // namespace SDK
