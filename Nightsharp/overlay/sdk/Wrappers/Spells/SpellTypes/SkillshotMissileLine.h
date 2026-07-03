#pragma once

#include "SkillshotMissile.h"

#include "../../../Math/Polygons/RectanglePoly.h"

#include <memory>

namespace SDK {

class SkillshotMissileLine : public SkillshotMissile {
public:
    std::unique_ptr<RectanglePoly> Rectangle;

    explicit SkillshotMissileLine(const std::string& spellName)
        : SkillshotMissile(spellName) {
    }

    explicit SkillshotMissileLine(const SpellDatabaseEntry& entry)
        : SkillshotMissile(entry) {
    }

    std::string ToString() const override {
        return "SkillshotMissileLine: Champion=" + SData.ChampionName +
               " SpellName=" + SData.SpellName;
    }

    void Game_OnUpdate() override {
        if (SData.MissileFollowsCaster && Caster.IsVisible()) {
            EndPosition = Caster.Position().To2D();
            Direction = (EndPosition - StartPosition).Normalized();
        }
        UpdatePolygon();
    }

protected:
    void UpdatePolygon() override {
        if (!Rectangle) {
            Rectangle = std::make_unique<RectanglePoly>(
                StartPosition,
                EndPosition,
                static_cast<float>(SData.Radius));
        }

        Rectangle->Start = GetMissilePosition(0);
        Rectangle->End = EndPosition;
        Rectangle->UpdatePolygon();
        UpdatePath();
    }

    void UpdatePath() override {
        if (Rectangle) {
            Path = Rectangle->ToClipperPath();
        }
    }
};

} // namespace SDK
