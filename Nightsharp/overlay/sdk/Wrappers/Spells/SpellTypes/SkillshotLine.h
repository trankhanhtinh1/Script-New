#pragma once

#include "SkillshotMissile.h"

#include "../../../Math/Polygons/RectanglePoly.h"

#include <memory>

namespace SDK {

class SkillshotLine : public SkillshotMissile {
public:
    std::unique_ptr<RectanglePoly> Rectangle;

    explicit SkillshotLine(const std::string& spellName)
        : SkillshotMissile(spellName) {
    }

    explicit SkillshotLine(const SpellDatabaseEntry& entry)
        : SkillshotMissile(entry) {
    }

    std::string ToString() const override {
        return "SkillshotLine: Champion=" + SData.ChampionName +
               " SpellName=" + SData.SpellName;
    }

protected:
    void UpdatePolygon() override {
        if (!Rectangle) {
            Rectangle = std::make_unique<RectanglePoly>(
                StartPosition,
                EndPosition,
                static_cast<float>(SData.Radius));
            UpdatePath();
        }
    }

    void UpdatePath() override {
        if (Rectangle) {
            Path = Rectangle->ToClipperPath();
        }
    }
};

} // namespace SDK
