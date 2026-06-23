#pragma once

#include "SkillshotMissile.h"

#include "../../../Math/Polygons/SectorPoly.h"

#include <memory>

namespace SDK {

class SkillshotMissileCone : public SkillshotMissile {
public:
    std::unique_ptr<SectorPoly> Sector;

    explicit SkillshotMissileCone(const std::string& spellName)
        : SkillshotMissile(spellName) {
    }

    explicit SkillshotMissileCone(const SpellDatabaseEntry& entry)
        : SkillshotMissile(entry) {
    }

    std::string ToString() const override {
        return "SkillshotMissileCone: Champion=" + SData.ChampionName +
               " SpellName=" + SData.SpellName;
    }

protected:
    void UpdatePolygon() override {
        // EnsoulSharp source keeps SkillshotMissileCone empty; use the same
        // sector geometry as SkillshotCone so MissileCone entries can still
        // participate in evade until a champion-specific override is ported.
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
