#pragma once

#include "SkillshotMissile.h"

#include "../../../Math/Polygons/SectorPoly.h"

#include <memory>

namespace SDK {

class SkillshotCone : public SkillshotMissile {
public:
    std::unique_ptr<SectorPoly> Sector;

    explicit SkillshotCone(const std::string& spellName)
        : SkillshotMissile(spellName) {
    }

    explicit SkillshotCone(const SpellDatabaseEntry& entry)
        : SkillshotMissile(entry) {
    }

    std::string ToString() const override {
        return "SkillshotCone: Champion=" + SData.ChampionName +
               " SpellName=" + SData.SpellName;
    }

protected:
    void UpdatePolygon() override {
        if (!Sector) {
            Sector = std::make_unique<SectorPoly>(
                StartPosition,
                EndPosition,
                static_cast<float>(SData.Angle) * 3.14159265358979323846f / 180.0f,
                static_cast<float>(SData.Range),
                20);
            UpdatePath();
        }
    }

    void UpdatePath() override {
        if (Sector) {
            Path = Sector->ToClipperPath();
        }
    }
};

} // namespace SDK
