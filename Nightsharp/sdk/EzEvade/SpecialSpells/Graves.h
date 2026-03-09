#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Graves : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "GravesQLineSpell")) {
            return;
        }
        if (s_registered) {
            return;
        }

        SpellDetector::RegisterOnProcessSpecialSpell(
            [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs&) {
                if (!SpellNameIs(spellData, "GravesQLineSpell")) {
                    return;
                }

                auto newData = std::make_shared<SpellData>(spellData->Clone());
                newData->isPerpendicular = true;
                newData->secondaryRadius = 255.0f;
                newData->updatePosition = false;
                newData->extraEndTime = 1300.0f;

                Vec3 start = args.StartPos;
                Vec3 end = ClipEndByRange(start, args.EndPos, newData->range);
                if (start.Distance2D(end) < newData->range) {
                    end = start.Extend(args.EndPos, newData->range);
                }

                const Vec3 wallPoint = Graves::FindNearWallPoint(start, end);
                if (wallPoint.IsValid() && !wallPoint.IsZero()) {
                    end = wallPoint;
                }

                SpellDetector::CreateSpellData(args.Sender, args.Sender.GetServerPosition(), end, newData);
            });

        s_registered = true;
    }

private:
    static Vec3 FindNearWallPoint(const Vec3& start, const Vec3& end, float sampleStep = 35.0f) {
        const float totalDist = start.Distance2D(end);
        if (totalDist <= 1.0f) {
            return Vec3();
        }

        const float step = std::max(sampleStep, 5.0f);
        const int samples = std::max(2, (int)(totalDist / step));
        for (int i = 1; i <= samples; ++i) {
            const float t = (float)i / (float)samples;
            const Vec3 probe = Vec3::Lerp(start, end, t);
            if (SDK::GameObject::IsWallAt(probe)) {
                return probe;
            }
        }

        return Vec3();
    }

    static inline bool s_registered = false;
};

} // namespace SpecialSpells
} // namespace EzEvade
