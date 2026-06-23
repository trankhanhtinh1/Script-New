#pragma once

#include "SkillshotMissile.h"

#include "../../../Math/Polygons/RingPoly.h"

#include <memory>

namespace SDK {

class SkillshotRing : public SkillshotMissile {
public:
    std::unique_ptr<RingPoly> Ring;

    explicit SkillshotRing(const std::string& spellName)
        : SkillshotMissile(spellName) {
    }

    explicit SkillshotRing(const SpellDatabaseEntry& entry)
        : SkillshotMissile(entry) {
    }

    std::string ToString() const override {
        return "SkillshotRing: Champion=" + SData.ChampionName +
               " SpellName=" + SData.SpellName;
    }

protected:
    void UpdatePolygon() override {
        if (!Ring) {
            Ring = std::make_unique<RingPoly>(
                EndPosition,
                static_cast<float>(SData.Radius),
                static_cast<float>(SData.RingRadius),
                20);
            UpdatePath();
        }
    }

    void UpdatePath() override {
        if (Ring) {
            Path = Ring->ToClipperPath();
        }
    }
};

} // namespace SDK
