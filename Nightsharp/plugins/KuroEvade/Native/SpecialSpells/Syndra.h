#pragma once

#include "SpecialSpellCommon.h"

#include <utility>

namespace Plugins::KuroEvade::SpecialSpells {

struct Syndra {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        CleanupSpots();

        if (EqualsSpell(context.Source, "SyndraQ") || EqualsSpell(context.Source, "SyndraWCast")) {
            Vector3 spot = context.End3;
            if (context.Start3.Distance(spot) > static_cast<float>(context.Source.Runtime.Range)) {
                spot = From2D(
                    context.Start + context.Direction * static_cast<float>(context.Source.Runtime.Range),
                    context.Start3.y);
            }

            QSpots().push_back({ SDK::Variables::TickCount(), spot });
            return false;
        }

        if (!EqualsSpell(context.Source, "SyndraE")) {
            return false;
        }

        const Vec2 pushStart = context.Start;
        const Vec2 pushEnd = context.Start + context.Direction * 800.0f;
        const Vec2 casterPos = context.Caster.ServerPosition().To2D();
        auto addPushedSphere = [&](const Vector3& spherePos, float radius) {
            Vec2 projection;
            if (!ProjectOnSegment(spherePos.To2D(), pushStart, pushEnd, projection) ||
                spherePos.To2D().Distance(projection) > radius) {
                return;
            }

            const Vec2 sphere2D = spherePos.To2D();
            const Vec2 direction = (sphere2D - casterPos).Normalized();
            if (direction.IsZero()) {
                return;
            }

            Database::SpellData pushed = context.Source;
            const float speed = context.Source.Runtime.MissileSpeed > 100.0f
                ? static_cast<float>(context.Source.Runtime.MissileSpeed)
                : 2000.0f;
            const float range = context.Source.Runtime.Range > 100.0f
                ? static_cast<float>(context.Source.Runtime.Range)
                : 1250.0f;
            const float sphereRadius = context.Source.Runtime.Radius > 10.0f
                ? static_cast<float>(context.Source.Runtime.Radius)
                : 100.0f;

            const float distFromCaster = spherePos.Distance(context.Caster.ServerPosition());
            pushed.Runtime.Delay = 250 + std::clamp(static_cast<int>((distFromCaster / speed) * 1000.0f), 0, 500);
            pushed.Runtime.MissileSpeed = static_cast<int>(speed);
            pushed.Runtime.Range = static_cast<int>(range);
            pushed.Runtime.Radius = static_cast<int>(sphereRadius);
            pushed.Type = Database::SkillShotType::SkillshotLine;
            pushed.DangerValue = 3;

            const Vector3 spellEnd = From2D(
                casterPos + direction * range,
                spherePos.y);
            AddExtra(result, spherePos, spellEnd, pushed);
        };

        for (const auto& sphere : SDK::GameObjects::EnemyMinions()) {
            if (!IsSphere(sphere)) {
                continue;
            }

            const Vector3 position = sphere.Position();
            RemoveSpotsNear(position);
            addPushedSphere(position, sphere.BoundingRadius() + 155.0f);
        }

        for (const auto& spot : QSpots()) {
            addPushedSphere(spot.second, 155.0f);
        }

        result.NoProcess = true;
        return true;
    }

private:
    static std::vector<std::pair<int, Vector3>>& QSpots() {
        static std::vector<std::pair<int, Vector3>> spots;
        return spots;
    }

    static bool IsSphere(const SDK::AIMinionClient& minion) {
        if (!minion.IsValid() || minion.IsDead() || !minion.IsEnemy()) {
            return false;
        }
        const std::string name = minion.CharacterName();
        return _stricmp(name.c_str(), "syndrasphere") == 0;
    }

    static void CleanupSpots() {
        const int now = SDK::Variables::TickCount();
        auto& spots = QSpots();
        spots.erase(
            std::remove_if(spots.begin(), spots.end(), [&](const auto& entry) {
                return now - entry.first >= 720;
            }),
            spots.end());
    }

    static void RemoveSpotsNear(const Vector3& position) {
        auto& spots = QSpots();
        spots.erase(
            std::remove_if(spots.begin(), spots.end(), [&](const auto& entry) {
                return entry.second.Distance(position) <= 30.0f;
            }),
            spots.end());
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
