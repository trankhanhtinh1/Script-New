#pragma once

#include "EvadeHelper.h"
#include "SpellData.h"

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <climits>
#include <memory>

namespace Plugins::KuroEvade {

struct Spell {
    int DangerLevel = 1;
    int StartTime = 0;
    int EndTime = 0;
    float Radius = 0.0f;
    Vec2 StartPosition;
    Vec2 EndPosition;
    Vec2 CurrentPosition;
    SDK::SpellType Type = SDK::SpellType::SkillshotLine;
    Vec2 direction;
    int helpTime = 0;
    Vec2 helpPos;
    float height = 0.0f;
    int heroID = 0;
    int projectileID = 0;
    SpellData info;
    int spellID = 0;
    SDK::GameObject spellObject;
    Vec2 cnLeft;
    Vec2 cnRight;
    Vec2 cnStart;
    Vec2 currentNegativePosition;
    Vec2 predictedEndPos;
    float evadeTime = -FLT_MAX;
    float spellHitTime = -FLT_MAX;

    Spell() = default;

    explicit Spell(const SDK::Skillshot& skillshot)
        : DangerLevel(EvadeHelper::DangerValue(skillshot)),
          StartTime(skillshot.StartTime),
          Radius(static_cast<float>(skillshot.SData.Radius)),
          StartPosition(skillshot.StartPosition),
          EndPosition(skillshot.EndPosition),
          CurrentPosition(skillshot.StartPosition),
          Type(skillshot.SData.SpellType),
          direction(skillshot.Direction),
          heroID(skillshot.Caster.NetworkId()) {
        info.sdk = skillshot.SData;
        if (const auto* missile = dynamic_cast<const SDK::SkillshotMissile*>(&skillshot)) {
            CurrentPosition = missile->GetMissilePosition(0);
            if (missile->Missile.IsValid()) {
                projectileID = missile->Missile.NetworkId();
                spellObject = missile->Missile;
            }
        }
    }

    float Delay() const { return static_cast<float>(info.sdk.Delay); }
    float Range() const { return static_cast<float>(info.sdk.Range); }
    const std::string& ChampionName() const { return info.sdk.ChampionName; }
    const std::string& SpellName() const { return info.sdk.SpellName; }
    SDK::SpellSlot Slot() const { return info.sdk.Slot; }

    Vec2 GetSpellEndPosition() const {
        return predictedEndPos.IsZero() ? EndPosition : predictedEndPos;
    }

    float GetSpellHitTime(const Vec2& position) const {
        const int now = SDK::Variables::TickCount();
        if (SDK::IsLineSpellType(Type) && info.sdk.MissileSpeed > 0 &&
            info.sdk.MissileSpeed != INT_MAX) {
            const float speed = static_cast<float>(std::max(1, info.sdk.MissileSpeed));
            return 1000.0f * CurrentPosition.Distance(position) / speed;
        }
        return std::max(0.0f, static_cast<float>(StartTime + info.sdk.Delay - now));
    }

    bool IsInside(const Vec2& position, float hitRadius, bool predictCollision = true) const {
        (void)predictCollision;
        SDK::SpellDatabaseEntry entry = info.sdk;
        SDK::SkillshotLine adapter(entry);
        adapter.StartPosition = StartPosition;
        adapter.EndPosition = EndPosition;
        adapter.Direction = direction;
        return EvadeHelper::InSkillShot(adapter, position, Radius + hitRadius);
    }

    bool HasProjectile() const {
        return info.sdk.MissileSpeed > 0 && info.sdk.MissileSpeed != INT_MAX;
    }
};

} // namespace Plugins::KuroEvade
