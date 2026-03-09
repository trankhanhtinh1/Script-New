#pragma once
#include "../IPlugin.h"
#include "sdk/UI/MenuUI.h"
#include "sdk/GameObjects/GameObjects.h"
#include "sdk/GameObjects/AiManager.h"
#include "sdk/GameObjects/BuffManager.h"
#include "sdk/UI/Drawing.h"
#include "sdk/Game.h"
#include "sdk/Math/Prediction.h"
#include "sdk/Math/Collisions.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <memory>
#include <vector>

// ============================================================================
// PredictionPlugin - Plugin-level Prediction.cs style engine
// Keeps SDK::Prediction untouched and provides a selectable ported engine.
// ============================================================================

namespace Plugins {

    class PredictionPlugin : public IPlugin {
    public:
        const char* GetName() const override { return "Prediction"; }
        const char* GetAuthor() const override { return "NightSharp"; }
        PluginCategory GetCategory() const override { return PluginCategory::CorePlugin; }

        void OnLoad() override {
            s_instance = this;
            m_menu = SDK::MenuUI::Menu::Create("PredictionCore", "Prediction");

            m_menu->Add<SDK::MenuUI::MenuList>(
                "Engine",
                "Engine",
                std::vector<std::string>{ "Prediction.cs Port", "SDK Legacy" },
                0);

            auto settings = m_menu->AddSubMenu("settings", "Settings");
            settings->Add<SDK::MenuUI::MenuBool>("UseFt", "Use Ping/Server Tick Compensation", true);
            settings->Add<SDK::MenuUI::MenuBool>("UseCollision", "Check Collision", true);
            settings->Add<SDK::MenuUI::MenuBool>("DrawDebug", "Draw Debug Prediction", false);
            settings->Add<SDK::MenuUI::MenuSlider>("DebugRange", "Debug Range", 1200, 300, 2500);
        }

        void OnUnload() override {
            SDK::MenuUI::Menu::Remove("PredictionCore");
            m_menu.reset();
            s_instance = nullptr;
        }

        void OnUpdate() override {
            if (!m_menu) return;
            m_menu->UpdateKeyBinds();
        }

        void OnRender() override {
            if (!m_menu) return;
            auto* drawDebug = m_menu->Get<SDK::MenuUI::MenuBool>("DrawDebug");
            if (!drawDebug || !drawDebug->Enabled) return;

            auto& player = SDK::GameObjects::Player;
            if (!player.IsValid() || !player.IsAlive()) return;

            float range = 1200.0f;
            if (auto* r = m_menu->Get<SDK::MenuUI::MenuSlider>("DebugRange")) {
                range = (float)r->Value;
            }

            SDK::GameObject best;
            float bestDist = FLT_MAX;
            for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive() || !hero.IsVisible()) continue;
                float d = player.DistanceTo(hero);
                if (d <= range && d < bestDist) {
                    best = hero;
                    bestDist = d;
                }
            }

            if (!best.IsValid()) return;

            SDK::PredictionInput input;
            input.From = player.GetPosition();
            input.RangeCheckFrom = player.GetPosition();
            input.Range = range;
            input.Delay = 0.25f;
            input.Speed = 1800.0f;
            input.Width = 60.0f;
            input.Type = SDK::SkillshotType::Line;
            input.CollisionCheck = false;

            auto pred = Predict(best, input);

            ImU32 col = IM_COL32(255, 90, 90, 220);
            if (pred.Hitchance >= SDK::HitChance::VeryHigh) col = IM_COL32(80, 255, 120, 230);
            else if (pred.Hitchance >= SDK::HitChance::High) col = IM_COL32(130, 230, 255, 230);
            else if (pred.Hitchance >= SDK::HitChance::Medium) col = IM_COL32(255, 200, 80, 230);

            SDK::Drawing::DrawLine3D(player.GetPosition(), pred.CastPosition, col, 2.0f);
            SDK::Drawing::DrawCircle(pred.CastPosition, 65.0f, col, 2.0f);

            Vec3 txtPos = pred.CastPosition;
            txtPos.y += 80.0f;
            SDK::Drawing::DrawTextCentered(txtPos, HitChanceToString(pred.Hitchance), col);
        }

        void OnMenu() override {
            if (m_menu) m_menu->Draw();
        }

        SDK::MenuUI::Menu* GetMenuRoot() override {
            return m_menu.get();
        }

        static SDK::PredictionResult Predict(const SDK::GameObject& target, const SDK::PredictionInput& input) {
            if (!target.IsValid()) {
                return SDK::PredictionResult();
            }

            if (!s_instance || !s_instance->m_menu || !s_instance->IsLoaded() || !s_instance->IsEnabled()) {
                return SDK::Prediction::GetPrediction(target, input);
            }

            auto* mode = s_instance->m_menu->Get<SDK::MenuUI::MenuList>("Engine");
            if (!mode || mode->Index == 1) {
                return SDK::Prediction::GetPrediction(target, input);
            }

            bool useFt = true;
            if (auto* ft = s_instance->m_menu->Get<SDK::MenuUI::MenuBool>("UseFt")) {
                useFt = ft->Enabled;
            }

            bool useCollision = true;
            if (auto* c = s_instance->m_menu->Get<SDK::MenuUI::MenuBool>("UseCollision")) {
                useCollision = c->Enabled;
            }

            return s_instance->PredictPort(target, input, useFt, useCollision);
        }

    private:
        std::shared_ptr<SDK::MenuUI::Menu> m_menu;
        static inline PredictionPlugin* s_instance = nullptr;

        SDK::PredictionResult PredictPort(const SDK::GameObject& target,
                                          SDK::PredictionInput input,
                                          bool ft,
                                          bool checkCollision) const {
            SDK::PredictionResult result;
            result.Hitchance = SDK::HitChance::Impossible;

            if (!target.IsValid() || !target.IsAlive()) {
                return result;
            }

            input.From = ResolveFrom(input);
            const Vec3 rangeCheckFrom = ResolveRangeCheckFrom(input);
            const float range = (input.Range > 0.0f) ? input.Range : FLT_MAX;

            if (ft) {
                input.Delay += SDK::Game::GetPing() / 2000.0f + 0.06f;
            }

            if (range < FLT_MAX) {
                const float maxRange = range * 1.5f;
                if (rangeCheckFrom.DistanceSqr2D(target.GetPosition()) > maxRange * maxRange) {
                    result.Hitchance = SDK::HitChance::OutOfRange;
                    return result;
                }
            }

            bool usedSpecial = false;
            if (target.IsDashing()) {
                result = GetDashingPrediction(target, input);
                usedSpecial = true;
            } else {
                const float immobileT = UnitIsImmobileUntil(target);
                if (immobileT >= 0.0f) {
                    result = GetImmobilePrediction(target, input, immobileT);
                    usedSpecial = true;
                }
            }

            if (!usedSpecial || result.Hitchance == SDK::HitChance::Impossible) {
                result = GetStandardPrediction(target, input);
            }

            if (range < FLT_MAX) {
                const float realRadius = GetRealRadius(input, target);
                if ((int)result.Hitchance >= (int)SDK::HitChance::High) {
                    const float edgeRange = range + realRadius * 0.75f;
                    if (rangeCheckFrom.DistanceSqr2D(target.GetPosition()) > edgeRange * edgeRange) {
                        result.Hitchance = SDK::HitChance::Medium;
                    }
                }

                const float extra = (input.Type == SDK::SkillshotType::Circle) ? realRadius : 0.0f;
                const float maxUnitRange = range + extra;
                if (rangeCheckFrom.DistanceSqr2D(result.UnitPosition) > maxUnitRange * maxUnitRange) {
                    result.Hitchance = SDK::HitChance::OutOfRange;
                }

                if (rangeCheckFrom.DistanceSqr2D(result.CastPosition) > range * range) {
                    if (result.Hitchance != SDK::HitChance::OutOfRange) {
                        Vec3 dir = (result.UnitPosition - rangeCheckFrom).Normalized2D();
                        result.CastPosition = rangeCheckFrom + dir * range;
                    } else {
                        result.Hitchance = SDK::HitChance::OutOfRange;
                    }
                }
            }

            if (checkCollision && input.CollisionCheck && (int)result.Hitchance >= (int)SDK::HitChance::Medium) {
                std::vector<Vec3> positions = { result.UnitPosition, result.CastPosition, target.GetPosition() };
                if (HasCollision(target, input, positions)) {
                    result.Hitchance = SDK::HitChance::Collision;
                }
            }

            return result;
        }

        static SDK::PredictionResult GetImmobilePrediction(const SDK::GameObject& target,
                                                           const SDK::PredictionInput& input,
                                                           float remainingImmobileT) {
            SDK::PredictionResult result;
            const float moveSpeed = std::max(target.GetMoveSpeed(), 1.0f);
            const float dist = target.GetPosition().Distance2D(input.From);
            const float travel = IsInstantSpeed(input.Speed) ? 0.0f : dist / input.Speed;
            const float reachTime = input.Delay + travel;
            const float realRadius = GetRealRadius(input, target);

            result.CastPosition = target.GetServerPosition();
            result.UnitPosition = target.GetPosition();
            result.Hitchance = (reachTime <= remainingImmobileT + realRadius / moveSpeed)
                ? SDK::HitChance::Immobile
                : SDK::HitChance::High;
            return result;
        }

        static SDK::PredictionResult GetDashingPrediction(const SDK::GameObject& target,
                                                          const SDK::PredictionInput& input) {
            SDK::PredictionResult result;

            SDK::AiManager ai(target.address);
            const Vec3 curPos = target.GetServerPosition();
            Vec3 dashEnd = ai.GetPathEnd();
            if (dashEnd.IsZero()) {
                dashEnd = target.GetPathEnd();
            }
            if (dashEnd.IsZero()) {
                dashEnd = curPos;
            }

            float dashSpeed = ai.GetDashSpeed();
            if (dashSpeed < 100.0f) {
                dashSpeed = std::max(target.GetMoveSpeed(), 1000.0f);
            }

            std::vector<Vec3> path = { curPos, dashEnd };
            auto dashPred = GetPositionOnPath(target, input, path, dashSpeed);

            if ((int)dashPred.Hitchance >= (int)SDK::HitChance::High) {
                const float distToLine = Geometry::PointToSegmentDistance(
                    dashPred.UnitPosition.To2D(),
                    curPos.To2D(),
                    dashEnd.To2D());
                if (distToLine < 200.0f) {
                    dashPred.CastPosition = dashPred.UnitPosition;
                    dashPred.Hitchance = SDK::HitChance::Dashing;
                    return dashPred;
                }
            }

            const float dashDistance = curPos.Distance2D(dashEnd);
            if (dashDistance > 200.0f) {
                const float timeToPoint = input.Delay * 0.5f +
                    GetTravelTime(input.From, dashEnd, input.Speed) - 0.25f;
                const float arrivalTime = dashDistance / std::max(dashSpeed, 1.0f) +
                    GetRealRadius(input, target) / std::max(target.GetMoveSpeed(), 1.0f);

                if (timeToPoint <= arrivalTime) {
                    result.CastPosition = dashEnd;
                    result.UnitPosition = dashEnd;
                    result.Hitchance = SDK::HitChance::Dashing;
                    return result;
                }
            }

            result.CastPosition = dashEnd;
            result.UnitPosition = dashEnd;
            result.Hitchance = SDK::HitChance::Medium;
            return result;
        }

        static SDK::PredictionResult GetStandardPrediction(const SDK::GameObject& target,
                                                           const SDK::PredictionInput& input) {
            float speed = target.GetMoveSpeed();
            if (target.GetPosition().DistanceSqr2D(input.From) < 200.0f * 200.0f) {
                speed /= 1.5f;
            }

            auto path = target.GetWaypoints();
            if (path.empty()) {
                path.push_back(target.GetServerPosition());
            }
            return GetPositionOnPath(target, input, path, speed);
        }

        static SDK::PredictionResult GetPositionOnPath(const SDK::GameObject& target,
                                                       const SDK::PredictionInput& input,
                                                       std::vector<Vec3> path,
                                                       float speed = -1.0f) {
            SDK::PredictionResult result;
            const float targetSpeed = (speed < 0.0f) ? target.GetMoveSpeed() : speed;

            if (path.size() <= 1) {
                const Vec3 pos = path.empty() ? target.GetServerPosition() : path[0];
                result.CastPosition = pos;
                result.UnitPosition = pos;
                result.Hitchance = SDK::HitChance::VeryHigh;
                return result;
            }

            const float realRadius = GetRealRadius(input, target);
            const float pathLength = Geometry::PathLength(path);
            const float triggerDistance = std::max(0.0f, input.Delay * targetSpeed - realRadius);

            if (pathLength >= triggerDistance && IsInstantSpeed(input.Speed)) {
                float remaining = triggerDistance;
                for (size_t i = 0; i + 1 < path.size(); i++) {
                    const Vec3 a = path[i];
                    const Vec3 b = path[i + 1];
                    const float segLen = a.Distance2D(b);

                    if (segLen >= remaining) {
                        const Vec3 dir = (b - a).Normalized2D();
                        const Vec3 castPos = a + dir * remaining;
                        const float push = (i == path.size() - 2)
                            ? std::min(remaining + realRadius, segLen)
                            : (remaining + realRadius);
                        const Vec3 unitPos = a + dir * push;

                        result.CastPosition = castPos;
                        result.UnitPosition = unitPos;
                        result.Hitchance = GetPathHitchance(target);
                        return result;
                    }

                    remaining -= segLen;
                }
            }

            if (pathLength >= triggerDistance && !IsInstantSpeed(input.Speed) && input.Speed > 0.0f) {
                float d = triggerDistance;
                if ((input.Type == SDK::SkillshotType::Line || input.Type == SDK::SkillshotType::Cone) &&
                    input.From.DistanceSqr2D(path[0]) < 200.0f * 200.0f) {
                    d = input.Delay * targetSpeed;
                }

                path = CutPath(path, std::max(0.0f, d));
                float tT = 0.0f;

                for (size_t i = 0; i + 1 < path.size(); i++) {
                    Vec3 a = path[i];
                    const Vec3 b = path[i + 1];
                    const float segLen = a.Distance2D(b);
                    const float segTime = segLen / std::max(targetSpeed, 1.0f);
                    const Vec3 dir = (b - a).Normalized2D();

                    a = a - dir * (targetSpeed * tT);
                    auto solution = VectorMovementCollision(a, b, targetSpeed, input.From, input.Speed, tT);
                    const float t = solution.first;
                    const Vec3 pos = solution.second;

                    if (pos.IsValid() && t >= tT && t <= tT + segTime) {
                        if (pos.Distance2D(b) < 20.0f) {
                            break;
                        }

                        result.CastPosition = pos;
                        result.UnitPosition = pos + dir * realRadius;
                        result.Hitchance = GetPathHitchance(target);
                        return result;
                    }

                    tT += segTime;
                }
            }

            result.CastPosition = path.back();
            result.UnitPosition = path.back();
            result.Hitchance = SDK::HitChance::Medium;
            return result;
        }

        static std::vector<Vec3> CutPath(const std::vector<Vec3>& path, float distance) {
            if (path.size() <= 1 || distance <= 0.0f) {
                return path;
            }

            std::vector<Vec3> result;
            float remaining = distance;

            for (size_t i = 0; i + 1 < path.size(); i++) {
                const float segLen = path[i].Distance2D(path[i + 1]);
                if (remaining > segLen) {
                    remaining -= segLen;
                    continue;
                }

                const Vec3 startPoint = path[i].Extend(path[i + 1], remaining);
                result.push_back(startPoint);
                for (size_t j = i + 1; j < path.size(); j++) {
                    result.push_back(path[j]);
                }
                break;
            }

            if (result.empty()) {
                result.push_back(path.back());
            }
            return result;
        }

        static std::pair<float, Vec3> VectorMovementCollision(const Vec3& startPos1,
                                                              const Vec3& endPos1,
                                                              float speed1,
                                                              const Vec3& startPos2,
                                                              float speed2,
                                                              float delay = 0.0f) {
            if (speed1 <= 0.0f || speed2 <= 0.0f) {
                return { -1.0f, Vec3() };
            }

            const float sP1x = startPos1.x;
            const float sP1y = startPos1.z;
            const float eP1x = endPos1.x;
            const float eP1y = endPos1.z;
            const float sP2x = startPos2.x;
            const float sP2y = startPos2.z;

            const float d = eP1x - sP1x;
            const float e = eP1y - sP1y;
            const float dist = sqrtf(d * d + e * e);
            if (dist < 0.0001f) {
                return { -1.0f, Vec3() };
            }

            const float t1 = dist / speed1;
            const float S = speed1;
            const float K = d / dist;
            const float L = e / dist;

            const float a = (S * S) - (speed2 * speed2);
            const float b = -2.0f * (sP1x * S * K - sP2x * S * K + sP1y * S * L - sP2y * S * L);
            const float c = (sP1x - sP2x) * (sP1x - sP2x) + (sP1y - sP2y) * (sP1y - sP2y);

            if (fabsf(a) < 0.0001f) {
                if (fabsf(b) > 0.0001f) {
                    const float t = -c / b;
                    if (t >= delay && t <= t1) {
                        return { t, Vec3(sP1x + t * S * K, startPos1.y, sP1y + t * S * L) };
                    }
                }
                return { -1.0f, Vec3() };
            }

            const float disc = b * b - 4.0f * a * c;
            if (disc < 0.0f) {
                return { -1.0f, Vec3() };
            }

            const float sqrtDisc = sqrtf(disc);
            const float t1Sol = (-b - sqrtDisc) / (2.0f * a);
            const float t2Sol = (-b + sqrtDisc) / (2.0f * a);
            const float t = (t1Sol >= delay && t1Sol <= t1)
                ? t1Sol
                : ((t2Sol >= delay && t2Sol <= t1) ? t2Sol : -1.0f);

            if (t >= 0.0f) {
                return { t, Vec3(sP1x + t * S * K, startPos1.y, sP1y + t * S * L) };
            }
            return { -1.0f, Vec3() };
        }

        static float UnitIsImmobileUntil(const SDK::GameObject& target) {
            const float now = SDK::Game::GetTime();
            float maxEnd = -1.0f;
            SDK::BuffManager bm(target.address);

            bm.ForEach([&](SDK::Buff& buff) {
                if (!buff.IsActive()) return;

                const SDK::BuffType t = buff.GetType();
                if (t != SDK::BuffType::Charm &&
                    t != SDK::BuffType::Knockup &&
                    t != SDK::BuffType::Stun &&
                    t != SDK::BuffType::Suppression &&
                    t != SDK::BuffType::Snare) {
                    return;
                }

                maxEnd = std::max(maxEnd, buff.GetEndTime());
            });

            return maxEnd - now;
        }

        static SDK::HitChance GetPathHitchance(const SDK::GameObject& target) {
            const auto path = SDK::GamePath::PathTracker::GetCurrentPath(target);
            const float age = path.Time(SDK::Game::GetTime());
            return age < 0.1f ? SDK::HitChance::VeryHigh : SDK::HitChance::High;
        }

        static bool HasCollision(const SDK::GameObject& target,
                                 const SDK::PredictionInput& input,
                                 const std::vector<Vec3>& positions) {
            const Vec3 from = ResolveFrom(input);
            const float width = (input.Width > 0.0f) ? input.Width : (input.Radius * 2.0f);
            int flags = ToCollisionFlags(input.CollisionFlags);
            if (flags == 0) {
                flags = SDK::Collisions::CheckMinions | SDK::Collisions::CheckProjectileBlockers;
            }

            for (const auto& p : positions) {
                auto col = SDK::Collisions::GetCollision(from, p, width, input.Speed, input.Delay, flags, target);
                if (col.HasCollision) {
                    return true;
                }
            }
            return false;
        }

        static int ToCollisionFlags(int inputFlags) {
            int flags = 0;
            if (inputFlags & SDK::CollisionMinions)     flags |= SDK::Collisions::CheckMinions;
            if (inputFlags & SDK::CollisionHeroes)      flags |= SDK::Collisions::CheckHeroes;
            if (inputFlags & SDK::CollisionWalls)       flags |= SDK::Collisions::CheckWalls;
            if (inputFlags & SDK::CollisionYasuoWall)   flags |= SDK::Collisions::CheckProjectileBlockers;
            if (inputFlags & SDK::CollisionBraumShield) flags |= SDK::Collisions::CheckProjectileBlockers;
            return flags;
        }

        static bool IsInstantSpeed(float speed) {
            return speed <= 0.0f || speed >= FLT_MAX * 0.5f;
        }

        static float GetTravelTime(const Vec3& from, const Vec3& to, float speed) {
            return IsInstantSpeed(speed) ? 0.0f : from.Distance2D(to) / speed;
        }

        static Vec3 ResolveFrom(const SDK::PredictionInput& input) {
            if (!input.From.IsZero()) {
                return input.From;
            }
            if (SDK::GameObjects::Player.IsValid()) {
                return SDK::GameObjects::Player.GetPosition();
            }
            return Vec3();
        }

        static Vec3 ResolveRangeCheckFrom(const SDK::PredictionInput& input) {
            if (!input.RangeCheckFrom.IsZero()) {
                return input.RangeCheckFrom;
            }
            return ResolveFrom(input);
        }

        static float GetRealRadius(const SDK::PredictionInput& input, const SDK::GameObject& target) {
            return input.RealRadius() + target.GetBoundingRadius();
        }

        static const char* HitChanceToString(SDK::HitChance hc) {
            switch (hc) {
            case SDK::HitChance::Collision: return "Collision";
            case SDK::HitChance::OutOfRange: return "OutOfRange";
            case SDK::HitChance::Impossible: return "Impossible";
            case SDK::HitChance::Low: return "Low";
            case SDK::HitChance::Medium: return "Medium";
            case SDK::HitChance::High: return "High";
            case SDK::HitChance::VeryHigh: return "VeryHigh";
            case SDK::HitChance::Dashing: return "Dashing";
            case SDK::HitChance::Immobile: return "Immobile";
            default: return "Unknown";
            }
        }
    };

} // namespace Plugins
