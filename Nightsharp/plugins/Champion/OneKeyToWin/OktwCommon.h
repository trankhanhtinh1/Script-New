#pragma once
// ============================================================================
// OktwCommon.h — Port of OneKeyToWin_AIO_Sebby.OktwCommon / SebbyLib helpers.
//
// Provides the utility helpers that every OKTW champion depends on:
//   ValidUlt / CanMove / CanHarras / GetKsDamage / GetIncomingDamage
//   IsSpellHeroCollision / CollisionYasuo / CirclePoints / GetPassiveTime
//   GetBuffCount / GetEchoLudenDamage / IsMovingInSameDirection
//   GetMinions / CountEnemyMinions / DrawLineRectangle / DrawTriangleOKTW
//   blockAttack / blockSpells / blockMove (mutable module state)
// ============================================================================

#include "../../sdk/SDK.h"
#include "../../sdk/Core/Objects.h"
#include "../../sdk/Extensions/Unit.h"
#include "../../sdk/GameObjects/GameObjects.h"
#include "../../sdk/GameObjects/ObjectManager.h"
#include "../../sdk/Math/HealthPrediction.h"
#include "../../sdk/Math/Prediction/Movement.h"
#include "../../sdk/UI/Drawing.h"
#include "../../sdk/Wrappers/Damages/Damage.h"
#include "../../sdk/Wrappers/Spells/Spell.h"
#include "../../core/CoreBuffs.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Plugins { namespace OKTW {

// ---------------------------------------------------------------------------
// Global blocking flags (used by Program.cs positioning helper + activator).
// ---------------------------------------------------------------------------
inline bool blockAttack = false;
inline bool blockMove   = false;
inline bool blockSpells = false;

namespace OktwCommon {

// ---------------------------------------------------------------------------
// Numeric helpers
// ---------------------------------------------------------------------------
inline float SafeDivide(float value, float divisor, float fallback = 0.0f) {
    return divisor > 0.0001f ? value / divisor : fallback;
}

inline float Clamp01(float value) {
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

// ---------------------------------------------------------------------------
// Buff helpers (parity with LeagueSharp.Common OktwCommon).
// GetPassiveTime — return remaining time for a named buff.
// ---------------------------------------------------------------------------
inline float GetPassiveTime(const SDK::AIBaseClient& unit, const char* buffName) {
    if (!unit.IsValid() || !buffName || !buffName[0]) {
        return 0.0f;
    }
    const auto buff = CoreBuffs::FindByName(unit.Address(), buffName);
    if (!buff.IsValid()) {
        return 0.0f;
    }
    return buff.GetRemainingTime(SDK::Game::Time());
}

inline int GetBuffCount(const SDK::AIBaseClient& unit, const char* buffName) {
    return (unit.IsValid() && buffName && buffName[0])
        ? unit.GetBuffCount(buffName)
        : 0;
}

inline bool HasBuffOfType(const SDK::AIBaseClient& unit, int type) {
    return unit.IsValid() && CoreBuffs::HasBuffType(unit.Address(), type);
}

// ---------------------------------------------------------------------------
// CanMove — enemy is immobile if stun/snare/charm/fear/taunt/knockup/knockback.
// ---------------------------------------------------------------------------
inline bool CanMove(const SDK::AIBaseClient& unit) {
    using namespace SDK::Prediction::BuffType;
    if (!unit.IsValid() || unit.IsDead()) {
        return false;
    }
    if (unit.MoveSpeed() < 25.0f) {
        return false;
    }
    if (HasBuffOfType(unit, Stun)        ||
        HasBuffOfType(unit, Snare)       ||
        HasBuffOfType(unit, Charm)       ||
        HasBuffOfType(unit, Fear)        ||
        HasBuffOfType(unit, Taunt)       ||
        HasBuffOfType(unit, Knockup)     ||
        HasBuffOfType(unit, Knockback)   ||
        HasBuffOfType(unit, Suppression) ||
        HasBuffOfType(unit, Asleep)) {
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// ValidUlt — target should be worth ulting (alive, targetable, not zombie/invuln).
// ---------------------------------------------------------------------------
inline bool ValidUlt(const SDK::AIHeroClient& target) {
    if (!target.IsValid() || target.IsDead() || target.IsZombie()) {
        return false;
    }
    if (target.IsInvulnerable()) {
        return false;
    }
    if (target.HasBuff("kindredrnodeathbuff")   ||
        target.HasBuff("JudicatorIntervention") ||
        target.HasBuff("bansheesveil")          ||
        target.HasBuff("sivire")                ||
        target.HasBuff("kindredrdeathmark")) {
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// GetIncomingDamage — approximate incoming damage via health-prediction diff.
// ---------------------------------------------------------------------------
inline float GetIncomingDamage(const SDK::AIBaseClient& target, float delaySeconds = 1.0f) {
    if (!target.IsValid()) {
        return 0.0f;
    }
    const float currentHp = target.Health();
    const int   timeMs    = static_cast<int>(std::max(0.0f, delaySeconds) * 1000.0f);
    const float predicted = SDK::HealthPrediction::GetPrediction(target, timeMs, 0);
    const float diff      = currentHp - predicted;
    return diff > 0.0f ? diff : 0.0f;
}

// ---------------------------------------------------------------------------
// GetKsDamage — damage a spell would deal accounting for incoming damage.
// ---------------------------------------------------------------------------
inline float GetKsDamage(const SDK::AIBaseClient& target, const SDK::Spell& spell) {
    if (!target.IsValid()) {
        return 0.0f;
    }
    const float rawDamage = spell.GetDamage(target);
    return rawDamage - GetIncomingDamage(target);
}

// ---------------------------------------------------------------------------
// GetEchoLudenDamage — approximated Ludens/Echo bonus (60 + 10% AP).
// ---------------------------------------------------------------------------
inline float GetEchoLudenDamage(const SDK::AIBaseClient& target) {
    if (!target.IsValid()) {
        return 0.0f;
    }
    const auto player = SDK::ObjectManager::Player();
    if (!player.IsValid()) {
        return 0.0f;
    }
    // Luden's Echo là id 6655; id 3285 (Luden's Tempest cũ) không còn tồn tại
    // trong ItemData nên nhánh cũ là code chết, không bao giờ cộng damage.
    if (!player.HasItem(6655) && !player.HasItem(3504)) {
        return 0.0f;
    }
    const float ap  = player.AP();
    const float raw = 100.0f + ap * 0.1f;
    return player.CalculateMagicDamage(target, raw);
}

// ---------------------------------------------------------------------------
// CanHarras — safe to harass right now? (approximation of original)
// ---------------------------------------------------------------------------
inline bool CanHarras() {
    const auto player = SDK::ObjectManager::Player();
    if (!player.IsValid()) {
        return false;
    }
    if (player.HealthPercent() < 30.0f) {
        return false;
    }
    if (player.ManaPercent() < 40.0f) {
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// IsMovingInSameDirection — heuristic used by Varus R and others.
// ---------------------------------------------------------------------------
inline bool IsMovingInSameDirection(const SDK::AIBaseClient& source,
                                    const SDK::AIBaseClient& target) {
    if (!source.IsValid() || !target.IsValid()) {
        return false;
    }
    const auto sourcePath = source.Path();
    const auto targetPath = target.Path();
    if (sourcePath.size() < 2 || targetPath.size() < 2) {
        return false;
    }
    const Vector3 sourceDir = (sourcePath.back() - sourcePath.front()).Normalized();
    const Vector3 targetDir = (targetPath.back() - targetPath.front()).Normalized();
    const float dot = sourceDir.x * targetDir.x + sourceDir.z * targetDir.z;
    return dot > 0.5f;
}

// ---------------------------------------------------------------------------
// IsSpellHeroCollision — is there an ally hero between source and target?
// ---------------------------------------------------------------------------
inline bool IsSpellHeroCollision(const SDK::AIBaseClient& target,
                                 const SDK::Spell& spell) {
    if (!target.IsValid()) {
        return true;
    }
    const auto player = SDK::ObjectManager::Player();
    if (!player.IsValid()) {
        return true;
    }
    const Vector3 from = player.ServerPosition();
    const Vector3 to   = target.ServerPosition();
    const float   widthSqr = (spell.Width + 30.0f) * (spell.Width + 30.0f);

    for (const auto& hero : SDK::GameObjects::EnemyHeroes()) {
        if (!hero.IsValid() || hero.NetworkId() == target.NetworkId()) {
            continue;
        }
        const Vector3 heroPos = hero.ServerPosition();
        const Vector3 ab = to - from;
        const Vector3 ap = heroPos - from;
        const float abLenSqr = ab.x * ab.x + ab.z * ab.z;
        if (abLenSqr < 0.001f) {
            continue;
        }
        const float t = Clamp01((ab.x * ap.x + ab.z * ap.z) / abLenSqr);
        const Vector3 projection = { from.x + ab.x * t, from.y, from.z + ab.z * t };
        const float dx = heroPos.x - projection.x;
        const float dz = heroPos.z - projection.z;
        if (dx * dx + dz * dz < widthSqr) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// CollisionYasuo — is there a Yasuo wall blocking the path? (SDK stub)
// ---------------------------------------------------------------------------
inline bool CollisionYasuo(const Vector3& /*from*/, const Vector3& /*to*/) {
    return false;
}

// ---------------------------------------------------------------------------
// CirclePoints — sample `pointsCount` uniform points on a circle.
// ---------------------------------------------------------------------------
inline std::vector<Vector3> CirclePoints(int pointsCount, float radius, const Vector3& center) {
    std::vector<Vector3> points;
    if (pointsCount <= 0 || radius <= 0.0f) {
        return points;
    }
    points.reserve(pointsCount);
    const float step = 2.0f * 3.14159265358979323846f / static_cast<float>(pointsCount);
    for (int i = 0; i < pointsCount; ++i) {
        const float angle = step * static_cast<float>(i);
        points.push_back({
            center.x + std::cos(angle) * radius,
            center.y,
            center.z + std::sin(angle) * radius,
        });
    }
    return points;
}

// ---------------------------------------------------------------------------
// GetTrapPos — helper used by Caitlyn to compute a valid trap position along
// the enemy's move direction.
// ---------------------------------------------------------------------------
inline Vector3 GetTrapPos(const SDK::AIHeroClient& target, float distance) {
    if (!target.IsValid()) {
        return {};
    }
    const auto path = target.Path();
    if (path.size() < 2) {
        return target.ServerPosition();
    }
    const Vector3 first = path.front();
    const Vector3 last  = path.back();
    const Vector3 dir   = (last - first).Normalized();
    return { first.x + dir.x * distance, first.y, first.z + dir.z * distance };
}

// ---------------------------------------------------------------------------
// GetMinions — enemy minions (optionally jungle mobs) within `range` of `pos`.
// Mirrors OKTW.Cache.GetMinions signature used by champion ports.
// ---------------------------------------------------------------------------
inline std::vector<SDK::AIMinionClient> GetMinions(const Vector3& pos,
                                                   float range,
                                                   bool  /*includeAlly*/ = false,
                                                   bool  jungleOnly = false) {
    std::vector<SDK::AIMinionClient> out;
    const auto& list = SDK::GameObjects::EnemyMinions();
    out.reserve(list.size());
    const float rangeSqr = range * range;
    for (const auto& minion : list) {
        if (!minion.IsValid() || minion.IsDead()) continue;
        if (!minion.IsTargetable()) continue;
        if (jungleOnly) {
            const std::string name = minion.CharacterName();
            const bool isJungle = name.rfind("SRU_", 0) == 0 || name.rfind("Sru_", 0) == 0;
            if (!isJungle) continue;
        }
        const Vector3 mp = minion.Position();
        const float dx = mp.x - pos.x;
        const float dz = mp.z - pos.z;
        if (dx * dx + dz * dz <= rangeSqr) {
            out.push_back(minion);
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// CountEnemiesInRange — number of enemy heroes within `range` of `position`.
// ---------------------------------------------------------------------------
inline int CountEnemiesInRange(const Vector3& position, float range) {
    int count = 0;
    const float rangeSqr = range * range;
    for (const auto& hero : SDK::GameObjects::EnemyHeroes()) {
        if (!hero.IsValid() || hero.IsDead()) continue;
        const Vector3 hp = hero.Position();
        const float dx = hp.x - position.x;
        const float dz = hp.z - position.z;
        if (dx * dx + dz * dz <= rangeSqr) ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
// CountAlliesInRange — number of ally heroes within `range` of `position`.
// ---------------------------------------------------------------------------
inline int CountAlliesInRange(const Vector3& position, float range) {
    int count = 0;
    const float rangeSqr = range * range;
    const auto player = SDK::ObjectManager::Player();
    if (!player.IsValid()) return 0;
    const auto myTeam = player.Team();
    for (const auto& hero : SDK::ObjectManager::Get<SDK::AIHeroClient>()) {
        if (!hero.IsValid() || hero.IsDead()) continue;
        if (hero.Team() != myTeam) continue;
        const Vector3 hp = hero.Position();
        const float dx = hp.x - position.x;
        const float dz = hp.z - position.z;
        if (dx * dx + dz * dz <= rangeSqr) ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
// CountEnemyMinions — number of enemy minions within `range` of `position`.
// ---------------------------------------------------------------------------
inline int CountEnemyMinions(const Vector3& position, float range) {
    int count = 0;
    const float rangeSqr = range * range;
    for (const auto& minion : SDK::GameObjects::EnemyMinions()) {
        if (!minion.IsValid() || minion.IsDead()) {
            continue;
        }
        const Vector3 mp = minion.Position();
        const float dx = mp.x - position.x;
        const float dz = mp.z - position.z;
        if (dx * dx + dz * dz <= rangeSqr) {
            ++count;
        }
    }
    return count;
}

// ---------------------------------------------------------------------------
// CanHitSkillShot — quick prediction shortcut used by a few champions.
// ---------------------------------------------------------------------------
inline bool CanHitSkillShot(const SDK::AIBaseClient& target,
                            const SDK::Spell& spell,
                            SDK::HitChance minHitChance = SDK::HitChance::High) {
    if (!target.IsValid()) {
        return false;
    }
    const auto out = spell.GetPrediction(target);
    return static_cast<int>(out.Hitchance) >= static_cast<int>(minHitChance);
}

// ---------------------------------------------------------------------------
// Drawing helpers (rectangle + triangle debug shapes).
// ---------------------------------------------------------------------------
inline void DrawLineRectangle(const Vector3& start,
                              const Vector3& end,
                              int width,
                              int thickness,
                              std::uint32_t color) {
    if (!SDK::Drawing::IsEnabled()) {
        return;
    }
    const Vector3 dir = (end - start).Normalized();
    const Vector3 perp = { -dir.z, 0.0f, dir.x };
    const float half = static_cast<float>(width) / 2.0f;

    const Vector3 p1 = { start.x + perp.x * half, start.y, start.z + perp.z * half };
    const Vector3 p2 = { start.x - perp.x * half, start.y, start.z - perp.z * half };
    const Vector3 p3 = { end.x   - perp.x * half, end.y,   end.z   - perp.z * half };
    const Vector3 p4 = { end.x   + perp.x * half, end.y,   end.z   + perp.z * half };

    Vec2 s1, s2, s3, s4;
    if (!SDK::Drawing::WorldToScreen(p1, s1) ||
        !SDK::Drawing::WorldToScreen(p2, s2) ||
        !SDK::Drawing::WorldToScreen(p3, s3) ||
        !SDK::Drawing::WorldToScreen(p4, s4)) {
        return;
    }
    SDK::Drawing::DrawLine(s1, s4, static_cast<float>(thickness), color);
    SDK::Drawing::DrawLine(s4, s3, static_cast<float>(thickness), color);
    SDK::Drawing::DrawLine(s3, s2, static_cast<float>(thickness), color);
    SDK::Drawing::DrawLine(s2, s1, static_cast<float>(thickness), color);
}

inline void DrawTriangleOKTW(float size, const Vector3& position, std::uint32_t color) {
    if (!SDK::Drawing::IsEnabled()) {
        return;
    }
    const float half = size * 0.5f;
    const Vector3 top     = { position.x,        position.y, position.z + half };
    const Vector3 bottomL = { position.x - half, position.y, position.z - half };
    const Vector3 bottomR = { position.x + half, position.y, position.z - half };

    Vec2 s1, s2, s3;
    if (!SDK::Drawing::WorldToScreen(top,     s1) ||
        !SDK::Drawing::WorldToScreen(bottomL, s2) ||
        !SDK::Drawing::WorldToScreen(bottomR, s3)) {
        return;
    }
    SDK::Drawing::DrawLine(s1, s2, 1.5f, color);
    SDK::Drawing::DrawLine(s2, s3, 1.5f, color);
    SDK::Drawing::DrawLine(s3, s1, 1.5f, color);
}

} // namespace OktwCommon
} } // namespace Plugins::OKTW
