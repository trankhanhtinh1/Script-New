#pragma once
#include "GameObjects/GameObjects.h"
#include "GameObjects/Missile.h"
#include "Events/TurretAggro.h"
#include "Wrappers/Damages/DamageCalc.h"
#include "Game.h"
#include <algorithm>
#include <cfloat>

// ============================================================================
// TurretMissileManager - Tracks active turret shots and predicts turret damage
// Reference: LeagueChimera TurretMissileManager + NewOrbwalker turret farm flow
// ============================================================================

namespace SDK {

    class TurretMissileManager {
    public:
        struct ShotInfo {
            GameObject Turret;
            GameObject Target;
            Missile MissileObj;
            float Damage = 0.0f;
            float RemainingTimeMs = 0.0f;
            bool HasMissile = false;
            bool TurretHasAggro = false;

            bool IsValid() const {
                return Turret.IsValid() && Target.IsValid() && Damage > 0.0f && RemainingTimeMs >= 0.0f;
            }
        };

        static bool GetIncomingShot(const GameObject& target, ShotInfo& outInfo) {
            outInfo = ShotInfo();
            if (!target.IsValid() || !target.IsAlive()) {
                return false;
            }

            const int targetNetId = target.GetNetId();
            if (targetNetId == 0) {
                return false;
            }

            auto missiles = MissileManager::GetMissiles();
            float bestRemainingMs = FLT_MAX;

            for (auto& turret : GameObjects::AllTurrets) {
                if (!turret.IsValid() || !turret.IsAlive()) {
                    continue;
                }
                if (turret.GetTeam() == target.GetTeam()) {
                    continue;
                }
                if (turret.GetPosition().Distance2D(target.GetPosition()) > 950.0f + target.GetBoundingRadius()) {
                    continue;
                }

                Missile matchedMissile;
                for (auto& missile : missiles) {
                    if (!missile.IsValid() || !missile.IsTurretShot()) {
                        continue;
                    }
                    if (missile.GetCasterNetId() != turret.GetNetId()) {
                        continue;
                    }

                    const int missileTargetNetId = missile.GetTargetNetId();
                    if (missileTargetNetId != 0 && missileTargetNetId != targetNetId) {
                        continue;
                    }

                    Vec3 missileEnd = missile.GetEndPos();
                    if (!missileEnd.IsZero() &&
                        missileEnd.Distance2D(target.GetPosition()) > target.GetBoundingRadius() + 180.0f) {
                        continue;
                    }

                    matchedMissile = missile;
                    break;
                }

                const bool turretHasAggro = TurretAggro::IsTurretTargeting(turret, target);
                if (!matchedMissile.IsValid() && !turretHasAggro) {
                    continue;
                }

                const float remainingMs = matchedMissile.IsValid()
                    ? GetRemainingMissileTimeMs(turret, target, matchedMissile)
                    : GetCastToHitTimeMs(turret, target);

                if (remainingMs < 0.0f || remainingMs >= bestRemainingMs) {
                    continue;
                }

                bestRemainingMs = remainingMs;
                outInfo.Turret = turret;
                outInfo.Target = target;
                outInfo.MissileObj = matchedMissile;
                outInfo.HasMissile = matchedMissile.IsValid();
                outInfo.TurretHasAggro = turretHasAggro;
                outInfo.RemainingTimeMs = remainingMs;
                outInfo.Damage = DamageCalc::GetAutoAttackDamage(turret, target, false, false);
            }

            return outInfo.IsValid();
        }

        static bool HasIncomingShot(const GameObject& target) {
            ShotInfo shot;
            return GetIncomingShot(target, shot);
        }

        static float GetIncomingDamage(const GameObject& target, float windowMs,
                                       int* outShotCount = nullptr,
                                       ShotInfo* outFirstShot = nullptr,
                                       float minRemainingMs = 150.0f) {
            if (outShotCount) {
                *outShotCount = 0;
            }
            if (outFirstShot) {
                *outFirstShot = ShotInfo();
            }

            ShotInfo shot;
            if (!GetIncomingShot(target, shot)) {
                return 0.0f;
            }

            if (outFirstShot) {
                *outFirstShot = shot;
            }

            if (shot.RemainingTimeMs > windowMs) {
                return 0.0f;
            }

            const float attackIntervalMs = GetAttackIntervalMs(shot.Turret);
            const float firstHitMs = shot.RemainingTimeMs;

            float totalDamage = 0.0f;
            int shotCount = 0;

            for (float hitMs = firstHitMs; hitMs <= windowMs + 1.0f; hitMs += attackIntervalMs) {
                if (hitMs < minRemainingMs) {
                    continue;
                }

                totalDamage += shot.Damage;
                ++shotCount;

                // Only the first missile is guaranteed once aggro is lost.
                if (!shot.TurretHasAggro) {
                    break;
                }
            }

            if (outShotCount) {
                *outShotCount = shotCount;
            }

            return totalDamage;
        }

        static float GetTurretDamage(const GameObject& turret, const GameObject& target) {
            if (!turret.IsValid() || !target.IsValid()) {
                return 0.0f;
            }
            return DamageCalc::GetAutoAttackDamage(turret, target, false, false);
        }

    private:
        static float ResolveMissileSpeed(const GameObject& turret) {
            float speed = turret.GetBasicAttackMissileSpeed();
            if (speed < 100.0f || speed > 5000.0f) {
                speed = 1200.0f;
            }
            return speed;
        }

        static float GetRemainingMissileTimeMs(const GameObject& turret,
                                               const GameObject& target,
                                               const Missile& missile) {
            Vec3 from = missile.GetPosition();
            if (from.IsZero()) {
                from = turret.GetPosition();
            }

            const float speed = ResolveMissileSpeed(turret);
            const float distance = (std::max)(0.0f, from.Distance2D(target.GetPosition()) - target.GetBoundingRadius());
            return distance / speed * 1000.0f + Game::GetPing() * 0.5f;
        }

        static float GetCastToHitTimeMs(const GameObject& turret, const GameObject& target) {
            const float speed = ResolveMissileSpeed(turret);
            const float distance = (std::max)(0.0f, turret.DistanceTo(target) - turret.GetBoundingRadius());
            return turret.AttackCastDelay() * 1000.0f +
                   distance / speed * 1000.0f +
                   Game::GetPing() * 0.5f;
        }

        static float GetAttackIntervalMs(const GameObject& turret) {
            float delay = turret.GetAttackDelay() * 1000.0f;
            if (delay < 400.0f || delay > 2500.0f) {
                delay = 850.0f;
            }
            return delay;
        }
    };

} // namespace SDK
