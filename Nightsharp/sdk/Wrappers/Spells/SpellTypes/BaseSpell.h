#pragma once

#include "../Database/SpellDatabase.h"

#include "../../../Core/Game.h"
#include "../../../Core/Objects.h"
#include "../../../Core/Variables.h"
#include "../../../Enumerations/SkillshotDetectionType.h"
#include "../../../Utils/Logging.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace SDK {

class BaseSpell {
public:
    SpellDatabaseEntry SData;
    SkillshotDetectionType DetectionType = SkillshotDetectionType::ProcessSpell;
    AIBaseClient Caster;
    Vector2 StartPosition = {};
    Vector2 EndPosition = {};
    int StartTime = 0;

    explicit BaseSpell(const std::string& spellName) {
        if (const auto* entry = SpellDatabase::GetByName(spellName)) {
            SData = *entry;
        }
    }

    explicit BaseSpell(const SpellDatabaseEntry& entry)
        : SData(entry) {
    }

    virtual ~BaseSpell() = default;

    virtual bool HasMissile() const {
        return false;
    }

    virtual std::string ToString() const {
        return "BaseSpell: Champion=" + SData.ChampionName +
               " SpellName=" + SData.SpellName;
    }

    virtual void PrintSpellData() const {
        Utils::Logging::Write()(LogLevel::Info,
            "ChampionName=%s SpellName=%s Range=%d Radius=%d Delay=%d MissileSpeed=%d CanBeRemoved=%d Angle=%d FixedRange=%d",
            SData.ChampionName.c_str(),
            SData.SpellName.c_str(),
            SData.Range,
            SData.Radius,
            SData.Delay,
            SData.MissileSpeed,
            SData.CanBeRemoved ? 1 : 0,
            SData.Angle,
            SData.FixedRange ? 1 : 0);
    }

    virtual bool HasExpired() const {
        if (SData.MissileAccel != 0) {
            return Variables::TickCount() >= StartTime + 5000;
        }

        const float missileSpeed = std::max(1.0f, static_cast<float>(SData.MissileSpeed));
        return Variables::TickCount() >
               StartTime + SData.Delay +
               static_cast<int>(1000.0f * (StartPosition.Distance(EndPosition) / missileSpeed));
    }

    virtual bool IsAboutToHit(const AIBaseClient& /*unit*/, int /*afterTime*/) const {
        // Matches EnsoulSharp.SDK: evade hit prediction is still a placeholder.
        return false;
    }

    virtual bool IsAboutToHit(const Vector3& /*position*/, int /*afterTime*/) const {
        // Matches EnsoulSharp.SDK: evade hit prediction is still a placeholder.
        return false;
    }

    virtual void Game_OnUpdate() {
    }

    virtual void Draw(std::uint32_t /*color*/, std::uint32_t /*missileColor*/, int /*borderWidth*/ = 1) {
    }
};

} // namespace SDK
