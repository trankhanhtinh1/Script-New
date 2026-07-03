#pragma once

#include "Skillshot.h"

#include <algorithm>
#include <cmath>

namespace SDK {

class SkillshotMissile : public Skillshot {
public:
    MissileClient Missile;
    bool MissileDestroyed = false;

    explicit SkillshotMissile(const std::string& spellName)
        : Skillshot(spellName) {
    }

    explicit SkillshotMissile(const SpellDatabaseEntry& entry)
        : Skillshot(entry) {
    }

    bool HasMissile() const override {
        return true;
    }

    std::string ToString() const override {
        return "SkillshotMissile: Champion=" + SData.ChampionName +
               " SpellName=" + SData.SpellName;
    }

    virtual Vector2 GetMissilePosition(int afterTime) const {
        const int elapsed = std::max(
            0,
            Variables::TickCount() + afterTime - StartTime - SData.Delay);

        int distance = 0;
        if (SData.MissileAccel == 0) {
            distance = elapsed * SData.MissileSpeed / 1000;
        } else {
            const float accel = static_cast<float>(SData.MissileAccel);
            const float terminal =
                (SData.MissileAccel > 0 ? SData.MissileMaxSpeed : SData.MissileMinSpeed - SData.MissileSpeed) *
                1000.0f / accel;

            if (elapsed <= terminal) {
                const float t = static_cast<float>(elapsed) / 1000.0f;
                distance = static_cast<int>(
                    elapsed * SData.MissileSpeed / 1000.0f +
                    0.5f * accel * t * t);
            } else {
                const float t1 = terminal / 1000.0f;
                distance = static_cast<int>(
                    terminal * SData.MissileSpeed / 1000.0f +
                    0.5f * accel * t1 * t1 +
                    (elapsed - terminal) / 1000.0f *
                    (SData.MissileAccel < 0 ? SData.MissileMaxSpeed : SData.MissileMinSpeed));
            }
        }

        return StartPosition + Direction * static_cast<float>(distance);
    }
};

} // namespace SDK
