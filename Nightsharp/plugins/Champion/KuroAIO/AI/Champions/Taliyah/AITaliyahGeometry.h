#pragma once

// Deterministic Taliyah mechanics and one-trick policy.
//
// Runtime prediction, NavMesh/turret queries, game-object discovery and casts
// live in AITaliyahController.  This layer owns the mechanics that are easy to
// get subtly wrong: accelerating versus fixed-speed Q interception, first-body
// AoE splash, Worked Ground creation/consumption, the six-row 22-mine E field,
// W displacement direction, E-W versus W-E timing, branch mana and manual R
// wall split safety.  Keeping these rules pure makes the controller auditable
// without a running League client.

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Taliyah::Geometry {

using SharedGeometry::Cross2D;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;

inline constexpr float kQCastSeconds = 0.25f;
inline constexpr float kQRange = 1000.0f;
inline constexpr float kQMissileRadius = 50.0f;
inline constexpr float kQNormalAoeRadius = 175.0f;
inline constexpr float kQBigAoeRadius = 225.0f;
inline constexpr float kQInitialSpeed = 3600.0f;
inline constexpr float kQMinimumSpeed = 1500.0f;
inline constexpr float kQDeceleration = 5000.0f;
inline constexpr float kQBigSpeed = 2000.0f;
inline constexpr float kQVolleyMultiplier = 2.60f;
inline constexpr float kQBigMultiplier = 1.80f;
inline constexpr float kQBigSlowSeconds = 1.50f;
inline constexpr float kWorkedGroundRadius = 400.0f;
inline constexpr float kWorkedGroundSeconds = 30.0f;
inline constexpr float kWorkedGroundCooldownMultiplier = 0.50f;
inline constexpr float kWorkedGroundMinimumCooldown = 0.75f;

inline constexpr float kWCastSeconds = 0.25f;
inline constexpr float kWKnockupDelaySeconds = 0.50f;
inline constexpr float kWImpactSeconds =
    kWCastSeconds + kWKnockupDelaySeconds;
inline constexpr float kWRange = 900.0f;
inline constexpr float kWRadius = 225.0f;
inline constexpr float kWThrowDistance = 400.0f;
inline constexpr float kWDisplacementSeconds = 0.50f;

inline constexpr float kECastSeconds = 0.25f;
inline constexpr float kERange = 950.0f;
inline constexpr float kEMineRadius = 85.0f;
inline constexpr float kEMineLifetimeSeconds = 4.0f;
inline constexpr float kEDelayBetweenRows = 0.17f;
inline constexpr int kERows = 6;
inline constexpr int kEMines = 22;
inline constexpr int kEMaximumDamagingMines = 4;
inline constexpr float kEMineDamageFalloff = 0.25f;
inline constexpr float kEStunSeconds = 0.75f;
inline constexpr float kEMaximumStunSeconds = 2.0f;
inline constexpr float kESlowPercent = 20.0f;
inline constexpr float kEMonsterMultiplier = 2.25f;

inline constexpr float kRMinimumUsefulRange = 900.0f;
inline constexpr float kRWallDurationSeconds = 4.0f;
inline constexpr float kRChannelSeconds = 1.0f;
inline constexpr float kRMissileSpeed = 2000.0f;
inline constexpr float kRWallHalfWidth = 60.0f;
inline constexpr float kRDamageLockoutSeconds = 3.0f;

inline float QRockRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 55.0f, 72.5f, 90.0f, 107.5f, 125.0f,
    };
    if (rank <= 0) return 0.0f;
    return RankValue(base, rank) + 0.50f * std::max(0.0f, abilityPower);
}

inline float QVolleyRawDamage(int rank, float abilityPower) {
    return QRockRawDamage(rank, abilityPower) * kQVolleyMultiplier;
}

inline float QBigRawDamage(int rank, float abilityPower) {
    return QRockRawDamage(rank, abilityPower) * kQBigMultiplier;
}

inline float QMonsterFlatDamage(int rank) {
    static constexpr std::array<float, 6> amount = {
        0.0f, 20.0f, 25.0f, 30.0f, 35.0f, 40.0f,
    };
    return rank <= 0 ? 0.0f : RankValue(amount, rank);
}

inline float QMonsterVolleyRawDamage(int rank, float abilityPower) {
    if (rank <= 0) return 0.0f;
    // Riot labels the jungle modifier as flat damage per rock.  The ordinary
    // five-rock base/AP payload is 1 + 4*0.4 = 2.6, while all five missiles
    // receive the flat monster payload.
    return QVolleyRawDamage(rank, abilityPower) +
           5.0f * QMonsterFlatDamage(rank);
}

inline float QMonsterBigRawDamage(int rank, float abilityPower) {
    if (rank <= 0) return 0.0f;
    return (QRockRawDamage(rank, abilityPower) +
            QMonsterFlatDamage(rank)) * kQBigMultiplier;
}

inline float QBigSlowPercent(int rank) {
    static constexpr std::array<float, 6> amount = {
        0.0f, 20.0f, 25.0f, 30.0f, 35.0f, 40.0f,
    };
    return rank <= 0 ? 0.0f : RankValue(amount, rank);
}

inline float ERawInitialDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 60.0f, 105.0f, 150.0f, 195.0f, 240.0f,
    };
    if (rank <= 0) return 0.0f;
    return RankValue(base, rank) + 0.60f * std::max(0.0f, abilityPower);
}

inline float ERawMineDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 25.0f, 40.0f, 55.0f, 70.0f, 85.0f,
    };
    if (rank <= 0) return 0.0f;
    return RankValue(base, rank) + 0.30f * std::max(0.0f, abilityPower);
}

inline float EMineDamageMultiplier(int contacts) {
    const int count = std::clamp(contacts, 0, kEMaximumDamagingMines);
    float result = 0.0f;
    for (int index = 0; index < count; ++index) {
        result += std::max(0.25f, 1.0f -
            kEMineDamageFalloff * static_cast<float>(index));
    }
    return result;
}

inline float ERawDetonationDamage(int rank,
                                  float abilityPower,
                                  int contacts) {
    return ERawMineDamage(rank, abilityPower) *
           EMineDamageMultiplier(contacts);
}

inline float ERawTotalDamage(int rank,
                             float abilityPower,
                             int contacts,
                             bool monster = false) {
    float result = ERawInitialDamage(rank, abilityPower) +
        ERawDetonationDamage(rank, abilityPower, contacts);
    if (monster) result *= kEMonsterMultiplier;
    return result;
}

inline float QCooldownSeconds(int rank) {
    static constexpr std::array<float, 6> amount = {
        0.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f,
    };
    return rank <= 0 ? 0.0f : RankValue(amount, rank);
}

inline float WorkedGroundQCooldownSeconds(int rank) {
    if (rank <= 0) return 0.0f;
    return std::max(kWorkedGroundMinimumCooldown,
                    QCooldownSeconds(rank) *
                        kWorkedGroundCooldownMultiplier);
}

inline float RRange(int rank) {
    static constexpr std::array<float, 4> amount = {
        0.0f, 2500.0f, 4500.0f, 6500.0f,
    };
    return rank <= 0 ? 0.0f : RankValue(amount, rank);
}

// Distance measured after the missile is released (the 0.25 second cast is
// intentionally separate).  Normal Q decelerates from 3600 toward 1500;
// within its 1000 range it remains in the deceleration phase.
inline float QProjectileDistance(float projectileSeconds, bool bigRock) {
    const float seconds = std::max(0.0f, projectileSeconds);
    if (bigRock) return std::min(kQRange, kQBigSpeed * seconds);
    const float decelerationSeconds =
        (kQInitialSpeed - kQMinimumSpeed) / kQDeceleration;
    if (seconds <= decelerationSeconds) {
        return std::min(
            kQRange,
            kQInitialSpeed * seconds -
                0.5f * kQDeceleration * seconds * seconds);
    }
    const float acceleratedDistance =
        kQInitialSpeed * decelerationSeconds -
        0.5f * kQDeceleration *
            decelerationSeconds * decelerationSeconds;
    return std::min(kQRange,
                    acceleratedDistance +
                        kQMinimumSpeed *
                            (seconds - decelerationSeconds));
}

inline float QProjectileTravelSeconds(float distance, bool bigRock) {
    const float clamped = std::clamp(distance, 0.0f, kQRange);
    if (bigRock) return clamped / kQBigSpeed;
    const float decelerationSeconds =
        (kQInitialSpeed - kQMinimumSpeed) / kQDeceleration;
    const float acceleratedDistance =
        kQInitialSpeed * decelerationSeconds -
        0.5f * kQDeceleration *
            decelerationSeconds * decelerationSeconds;
    if (clamped <= acceleratedDistance) {
        const float discriminant = std::max(
            0.0f,
            kQInitialSpeed * kQInitialSpeed -
                2.0f * kQDeceleration * clamped);
        return (kQInitialSpeed - std::sqrt(discriminant)) /
               kQDeceleration;
    }
    return decelerationSeconds +
           (clamped - acceleratedDistance) / kQMinimumSpeed;
}

struct QBody {
    int Id = 0;
    Vec3 Position = {};
    Vec3 Velocity = {};
    float Radius = 0.0f;
    float Health = 1.0f;
    float MaximumHealth = 1.0f;
    bool Valid = true;
    bool Targetable = true;
    bool Hostile = true;
    bool Champion = false;
    bool Minion = false;
    bool Monster = false;
    bool Large = false;
    bool Epic = false;

    Vec3 PositionAt(float seconds) const {
        Vec3 result = Position + Velocity * std::max(0.0f, seconds);
        result.y = Position.y;
        return result;
    }
};

struct QContact {
    bool Hit = false;
    int BodyId = 0;
    std::size_t BodyIndex = 0;
    float ProjectileSeconds = 0.0f;
    float CastElapsedSeconds = 0.0f;
    float MissileDistance = 0.0f;
    Vec3 MissilePosition = {};
    Vec3 BodyPosition = {};
};

inline bool ValidQBody(const QBody& body) {
    return body.Id != 0 && body.Valid && body.Targetable && body.Hostile &&
           body.Health > 0.0f && body.MaximumHealth > 0.0f &&
           body.Position.IsValid();
}

inline float QContactSeparationSquared(const Vec3& origin,
                                       const Vec3& direction,
                                       const QBody& body,
                                       float projectileSeconds,
                                       bool bigRock) {
    const Vec3 missile = origin +
        direction * QProjectileDistance(projectileSeconds, bigRock);
    const Vec3 target = body.PositionAt(
        kQCastSeconds + projectileSeconds);
    const float radius = kQMissileRadius + std::max(0.0f, body.Radius);
    return missile.DistanceSqr2D(target) - radius * radius;
}

inline QContact ContactWithQBody(const Vec3& origin,
                                 const Vec3& aim,
                                 const QBody& body,
                                 bool bigRock = false) {
    QContact result{};
    if (!origin.IsValid() || !aim.IsValid() || !ValidQBody(body)) {
        return result;
    }
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return result;
    const float maximumSeconds = QProjectileTravelSeconds(kQRange, bigRock);

    // A 480 Hz bracket is comfortably smaller than the time needed for the
    // fastest projectile to cross the smallest legal capsule.  Bisection then
    // recovers the first entry instead of merely the closest sample.
    constexpr int samples = 256;
    float previousTime = 0.0f;
    float previousValue = QContactSeparationSquared(
        origin, direction, body, previousTime, bigRock);
    bool bracketed = previousValue <= 0.0f;
    float low = 0.0f;
    float high = 0.0f;
    for (int index = 1; index <= samples && !bracketed; ++index) {
        const float time = maximumSeconds *
            static_cast<float>(index) / static_cast<float>(samples);
        const float value = QContactSeparationSquared(
            origin, direction, body, time, bigRock);
        if (value <= 0.0f) {
            low = previousTime;
            high = time;
            bracketed = true;
            break;
        }
        previousTime = time;
        previousValue = value;
    }
    if (!bracketed) return result;
    if (high > low) {
        for (int iteration = 0; iteration < 24; ++iteration) {
            const float middle = (low + high) * 0.5f;
            if (QContactSeparationSquared(
                    origin, direction, body, middle, bigRock) <= 0.0f) {
                high = middle;
            } else {
                low = middle;
            }
        }
    }
    const float contactTime = high > 0.0f ? high : 0.0f;
    const float missileDistance = QProjectileDistance(contactTime, bigRock);
    if (missileDistance > kQRange + 0.01f) return result;
    result.Hit = true;
    result.BodyId = body.Id;
    result.ProjectileSeconds = contactTime;
    result.CastElapsedSeconds = kQCastSeconds + contactTime;
    result.MissileDistance = missileDistance;
    result.MissilePosition = origin + direction * missileDistance;
    result.BodyPosition = body.PositionAt(result.CastElapsedSeconds);
    return result;
}

inline QContact FirstQContact(const Vec3& origin,
                              const Vec3& aim,
                              const std::vector<QBody>& bodies,
                              bool bigRock = false) {
    QContact best{};
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        QContact candidate = ContactWithQBody(
            origin, aim, bodies[index], bigRock);
        if (!candidate.Hit) continue;
        candidate.BodyIndex = index;
        const bool earlier = !best.Hit ||
            candidate.ProjectileSeconds < best.ProjectileSeconds - 0.0001f;
        const bool tie = best.Hit && std::fabs(
            candidate.ProjectileSeconds - best.ProjectileSeconds) <= 0.0001f;
        if (earlier || (tie && candidate.BodyId < best.BodyId)) {
            best = candidate;
        }
    }
    return best;
}

inline const QBody* FindBody(const std::vector<QBody>& bodies, int id) {
    for (const auto& body : bodies) {
        if (body.Id == id) return &body;
    }
    return nullptr;
}

inline bool QContactSplashesBody(const QContact& contact,
                                 const QBody& body,
                                 bool bigRock) {
    if (!contact.Hit || !ValidQBody(body)) return false;
    if (contact.BodyId == body.Id) return true;
    const Vec3 position = body.PositionAt(contact.CastElapsedSeconds);
    const float radius = (bigRock ? kQBigAoeRadius : kQNormalAoeRadius) +
        std::max(0.0f, body.Radius);
    return contact.BodyPosition.DistanceSqr2D(position) <= radius * radius;
}

inline std::vector<int> QSplashVictimIds(const QContact& contact,
                                         const std::vector<QBody>& bodies,
                                         bool bigRock) {
    std::vector<int> result;
    if (!contact.Hit) return result;
    for (const auto& body : bodies) {
        if (QContactSplashesBody(contact, body, bigRock)) {
            result.push_back(body.Id);
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

inline bool QHitsIntended(const Vec3& origin,
                          const Vec3& aim,
                          const std::vector<QBody>& bodies,
                          int intendedId,
                          bool bigRock,
                          bool allowAoeBridge,
                          QContact* contactOut = nullptr) {
    const QBody* intended = FindBody(bodies, intendedId);
    if (!intended) return false;
    const QContact contact = FirstQContact(origin, aim, bodies, bigRock);
    if (contactOut) *contactOut = contact;
    if (!contact.Hit) return false;
    return contact.BodyId == intendedId ||
        (allowAoeBridge && QContactSplashesBody(contact, *intended, bigRock));
}

struct WorkedGroundZone {
    int Id = 0;
    Vec3 Center = {};
    float CreatedAt = 0.0f;
    float ExpiresAt = 0.0f;
    bool Consumed = false;
    bool Confirmed = false;
};

inline bool ActiveWorkedGround(const WorkedGroundZone& zone,
                               float nowSeconds) {
    return zone.Id != 0 && zone.Center.IsValid() && !zone.Consumed &&
           std::isfinite(zone.ExpiresAt) && zone.ExpiresAt > nowSeconds;
}

inline void NormalizeWorkedGround(std::vector<WorkedGroundZone>& zones,
                                  float nowSeconds) {
    zones.erase(std::remove_if(
        zones.begin(), zones.end(),
        [nowSeconds](const WorkedGroundZone& zone) {
            return !ActiveWorkedGround(zone, nowSeconds);
        }), zones.end());
}

inline int WorkedGroundAt(const std::vector<WorkedGroundZone>& zones,
                          const Vec3& position,
                          float nowSeconds,
                          float playerRadius = 0.0f) {
    if (!position.IsValid()) return 0;
    int bestId = 0;
    float bestDistance = FLT_MAX;
    const float allowed = kWorkedGroundRadius +
        std::max(0.0f, playerRadius);
    for (const auto& zone : zones) {
        if (!ActiveWorkedGround(zone, nowSeconds)) continue;
        const float distance = zone.Center.Distance2D(position);
        if (distance <= allowed &&
            (distance < bestDistance - 0.01f ||
             (std::fabs(distance - bestDistance) <= 0.01f &&
              zone.Id < bestId))) {
            bestId = zone.Id;
            bestDistance = distance;
        }
    }
    return bestId;
}

inline bool ConsumeWorkedGround(std::vector<WorkedGroundZone>& zones,
                                int id) {
    for (auto& zone : zones) {
        if (zone.Id == id && !zone.Consumed) {
            zone.Consumed = true;
            return true;
        }
    }
    return false;
}

inline int AddWorkedGround(std::vector<WorkedGroundZone>& zones,
                           const Vec3& center,
                           float nowSeconds,
                           int nextId,
                           bool confirmed = false) {
    if (!center.IsValid() || center.IsZero() || nextId == 0) return 0;
    WorkedGroundZone zone{};
    zone.Id = nextId;
    zone.Center = center;
    zone.CreatedAt = nowSeconds;
    zone.ExpiresAt = nowSeconds + kWorkedGroundSeconds;
    zone.Confirmed = confirmed;
    zones.push_back(zone);
    return nextId;
}

enum class QForm : std::uint8_t {
    Volley,
    Boulder,
};

struct QCastTransition {
    QForm Form = QForm::Volley;
    int ZoneId = 0;
    int CreatedZoneId = 0;
};

inline QCastTransition ApplyQCastToWorkedGround(
    std::vector<WorkedGroundZone>& zones,
    const Vec3& playerPosition,
    float playerRadius,
    float nowSeconds,
    int nextZoneId) {
    NormalizeWorkedGround(zones, nowSeconds);
    QCastTransition result{};
    result.ZoneId = WorkedGroundAt(
        zones, playerPosition, nowSeconds, playerRadius);
    if (result.ZoneId != 0) {
        result.Form = QForm::Boulder;
        ConsumeWorkedGround(zones, result.ZoneId);
    } else {
        result.Form = QForm::Volley;
        result.CreatedZoneId = AddWorkedGround(
            zones, playerPosition, nowSeconds, nextZoneId, false);
    }
    return result;
}

struct EMine {
    int Index = 0;
    int Row = 0;
    int Column = 0;
    Vec3 Position = {};
    float SpawnAt = 0.0f;
    float ExpiresAt = 0.0f;
};

struct Minefield {
    Vec3 Origin = {};
    Vec3 Direction = {};
    float CastAt = 0.0f;
    std::array<EMine, kEMines> Mines = {};
    int Count = 0;
    bool Valid = false;
};

// Riot exposes 22 mines in six rows (2 in the first, then 4 in each of five
// rows), 85 radius and 0.17 seconds between rows, but the server-side lateral
// offsets are not in the client bin.  These centers reconstruct the visible
// trapezoid conservatively: exact row count/timing/radius are live data, while
// the increasing half-width prevents the planner from claiming contacts in
// gaps near the sides.  Runtime W casts still require prediction confidence.
inline Minefield BuildMinefield(const Vec3& origin,
                                const Vec3& castPosition,
                                float castAtSeconds = 0.0f) {
    Minefield field{};
    const Vec3 direction = Direction2D(origin, castPosition);
    if (!origin.IsValid() || direction.IsZero()) return field;
    const Vec3 perpendicular{ -direction.z, 0.0f, direction.x };
    static constexpr std::array<float, kERows> forward = {
        130.0f, 280.0f, 430.0f, 580.0f, 730.0f, 880.0f,
    };
    static constexpr std::array<float, kERows> halfWidth = {
        72.0f, 145.0f, 176.0f, 207.0f, 238.0f, 269.0f,
    };
    field.Origin = origin;
    field.Direction = direction;
    field.CastAt = castAtSeconds;
    int mineIndex = 0;
    for (int row = 0; row < kERows; ++row) {
        const int columns = row == 0 ? 2 : 4;
        for (int column = 0; column < columns; ++column) {
            float lateral = 0.0f;
            if (columns == 2) {
                lateral = column == 0 ? -halfWidth[row] : halfWidth[row];
            } else {
                const float normalized = -1.0f +
                    2.0f * static_cast<float>(column) / 3.0f;
                lateral = normalized * halfWidth[row];
            }
            EMine mine{};
            mine.Index = mineIndex;
            mine.Row = row;
            mine.Column = column;
            mine.Position = origin + direction * forward[row] +
                perpendicular * lateral;
            mine.Position.y = origin.y;
            mine.SpawnAt = castAtSeconds + kECastSeconds +
                static_cast<float>(row) * kEDelayBetweenRows;
            mine.ExpiresAt = mine.SpawnAt + kEMineLifetimeSeconds;
            field.Mines[static_cast<std::size_t>(mineIndex++)] = mine;
        }
    }
    field.Count = mineIndex;
    field.Valid = mineIndex == kEMines;
    return field;
}

inline bool PointInMinefieldEnvelope(const Minefield& field,
                                     const Vec3& point,
                                     float radius = 0.0f) {
    if (!field.Valid || !point.IsValid()) return false;
    Vec3 relative = point - field.Origin;
    relative.y = 0.0f;
    const Vec3 perpendicular{ -field.Direction.z, 0.0f, field.Direction.x };
    const float forward = relative.Dot(field.Direction);
    const float lateral = std::fabs(relative.Dot(perpendicular));
    const float extra = std::max(0.0f, radius);
    if (forward < -extra || forward > kERange + extra) return false;
    const float halfWidth = 95.0f + 0.205f *
        std::clamp(forward, 0.0f, kERange);
    return lateral <= halfWidth + extra;
}

inline bool SegmentTouchesMine(const Vec3& start,
                               const Vec3& end,
                               const EMine& mine,
                               float targetRadius,
                               float movementStartSeconds,
                               float movementDurationSeconds,
                               float* contactT = nullptr) {
    const auto projection = ProjectPointToSegment2D(
        mine.Position, start, end);
    const float radius = kEMineRadius + std::max(0.0f, targetRadius);
    if (projection.Distance > radius) return false;
    const float time = movementStartSeconds +
        projection.T * std::max(0.0f, movementDurationSeconds);
    if (time + 0.0001f < mine.SpawnAt || time >= mine.ExpiresAt) {
        return false;
    }
    if (contactT) *contactT = projection.T;
    return true;
}

struct MineContactSummary {
    int Contacts = 0;
    int DistinctRows = 0;
    float DamageMultiplier = 0.0f;
    float FirstPathT = 1.0f;
    float LastPathT = 0.0f;
    std::array<int, kEMaximumDamagingMines> MineIndices = {
        -1, -1, -1, -1,
    };
};

inline MineContactSummary CountMineContacts(
    const Minefield& field,
    const Vec3& start,
    const Vec3& end,
    float targetRadius,
    float movementStartSeconds,
    float movementDurationSeconds) {
    MineContactSummary result{};
    if (!field.Valid || !start.IsValid() || !end.IsValid() ||
        start.DistanceSqr2D(end) <= 0.001f) return result;
    struct Candidate {
        int MineIndex = -1;
        int Row = -1;
        float T = 0.0f;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(kEMines);
    for (int index = 0; index < field.Count; ++index) {
        float t = 0.0f;
        const EMine& mine = field.Mines[static_cast<std::size_t>(index)];
        if (SegmentTouchesMine(start, end, mine, targetRadius,
                               movementStartSeconds,
                               movementDurationSeconds, &t)) {
            candidates.push_back({ index, mine.Row, t });
        }
    }
    std::sort(candidates.begin(), candidates.end(),
        [](const Candidate& left, const Candidate& right) {
            if (std::fabs(left.T - right.T) > 0.0001f) {
                return left.T < right.T;
            }
            return left.MineIndex < right.MineIndex;
        });
    std::array<bool, kERows> rows = {};
    for (const Candidate& candidate : candidates) {
        // Multiple overlapping circles in one row are a single path event for
        // a capsule moving through the lane.  Count distinct rows first; this
        // avoids exaggerating a lateral graze into the 2.5x damage cap.
        if (rows[static_cast<std::size_t>(candidate.Row)]) continue;
        rows[static_cast<std::size_t>(candidate.Row)] = true;
        if (result.Contacts < kEMaximumDamagingMines) {
            result.MineIndices[static_cast<std::size_t>(result.Contacts)] =
                candidate.MineIndex;
            result.FirstPathT = std::min(result.FirstPathT, candidate.T);
            result.LastPathT = std::max(result.LastPathT, candidate.T);
            ++result.Contacts;
        }
    }
    for (bool touched : rows) {
        if (touched) ++result.DistinctRows;
    }
    result.DamageMultiplier = EMineDamageMultiplier(result.Contacts);
    return result;
}

inline Vec3 WDestination(const Vec3& center,
                         const Vec3& desiredDirection) {
    Vec3 direction = desiredDirection;
    direction.y = 0.0f;
    const float length = direction.Length2D();
    if (length <= 0.001f || !std::isfinite(length)) return {};
    direction = direction / length;
    Vec3 result = center + direction * kWThrowDistance;
    result.y = center.y;
    return result;
}

enum class QPurpose : std::uint8_t {
    Poke,
    Combo,
    BoulderSetup,
    Kill,
    Peel,
    LastHit,
    Wave,
    Jungle,
    Objective,
};

enum class WPurpose : std::uint8_t {
    MinefieldCatch,
    FastFollowup,
    BigQCatch,
    Interrupt,
    PeelPlayer,
    PeelAlly,
    Gapcloser,
};

enum class EPurpose : std::uint8_t {
    ComboSetup,
    DashPunish,
    Peel,
    Choke,
    Wave,
    Jungle,
    Objective,
};

enum class ComboBranch : std::uint8_t {
    None,
    BoulderWEQ,
    FastWEQ,
    ControlledEWQ,
    DashPunishEWQ,
    EQPoke,
    QPoke,
};

struct CastEvaluation {
    bool Cast = false;
    float Score = -FLT_MAX;
    const char* Reason = "invalid";
};

struct QContext {
    bool Ready = false;
    bool HasMana = false;
    bool TargetValid = false;
    bool InRange = false;
    bool CleanContact = false;
    bool AoeBridge = false;
    bool ProjectileWallBlocked = false;
    bool TargetSpellShield = false;
    bool TargetImmune = false;
    bool HighConfidence = false;
    bool TargetImmobile = false;
    bool TargetDashing = false;
    bool Lethal = false;
    bool Reactive = false;
    bool PlayerAttackWindingUp = false;
    bool ComboFollowupReady = false;
    bool FullVolleyPreferred = false;
    bool PreserveGroundForSetup = false;
    bool CursorAgrees = true;
    QForm Form = QForm::Volley;
    QPurpose Purpose = QPurpose::Poke;
    int AoeVictims = 1;
    float CollisionConfidence = 0.0f;
};

inline CastEvaluation EvaluateQ(const QContext& context) {
    CastEvaluation result{};
    if (!context.Ready || !context.HasMana || !context.TargetValid ||
        !context.InRange) {
        result.Reason = "Q unavailable";
        return result;
    }
    if ((!context.CleanContact && !context.AoeBridge) ||
        context.ProjectileWallBlocked) {
        result.Reason = "Q first body misses";
        return result;
    }
    if (context.TargetSpellShield || context.TargetImmune) {
        result.Reason = "Q denied";
        return result;
    }
    if (!context.HighConfidence && !context.TargetImmobile &&
        !context.TargetDashing && !context.Reactive) {
        result.Reason = "Q prediction too weak";
        return result;
    }
    if (context.PlayerAttackWindingUp && !context.Lethal &&
        !context.Reactive) {
        result.Reason = "preserve attack";
        return result;
    }
    if (context.Form == QForm::Boulder && context.FullVolleyPreferred &&
        !context.Lethal && context.Purpose != QPurpose::BoulderSetup &&
        context.Purpose != QPurpose::Peel) {
        result.Reason = "preserve full volley DPS";
        return result;
    }
    if (!context.CursorAgrees && !context.Lethal && !context.Reactive &&
        (context.Purpose == QPurpose::Poke ||
         context.Purpose == QPurpose::Combo)) {
        result.Reason = "player direction disagrees";
        return result;
    }
    result.Cast = true;
    result.Score = 480.0f + context.CollisionConfidence * 260.0f +
        static_cast<float>(std::max(1, context.AoeVictims)) * 42.0f;
    if (context.Form == QForm::Boulder) result.Score += 80.0f;
    if (context.AoeBridge) result.Score += 95.0f;
    if (context.ComboFollowupReady && context.Form == QForm::Boulder) {
        result.Score += 180.0f;
    }
    if (context.PreserveGroundForSetup && context.Form == QForm::Volley) {
        result.Score += 45.0f;
    }
    if (context.TargetImmobile || context.TargetDashing) result.Score += 90.0f;
    if (context.Lethal) result.Score += 760.0f;
    if (context.Reactive) result.Score += 220.0f;
    switch (context.Purpose) {
    case QPurpose::Kill: result.Score += 320.0f; break;
    case QPurpose::BoulderSetup: result.Score += 180.0f; break;
    case QPurpose::Peel: result.Score += 150.0f; break;
    case QPurpose::Objective: result.Score += 110.0f; break;
    case QPurpose::LastHit: result.Score += 65.0f; break;
    default: break;
    }
    result.Reason = context.Form == QForm::Boulder
        ? "worked-ground boulder" : "five-rock volley";
    return result;
}

struct WContext {
    bool Ready = false;
    bool HasMana = false;
    bool TargetValid = false;
    bool InRange = false;
    bool CenterValid = false;
    bool DirectionValid = false;
    bool HighConfidence = false;
    bool TargetImmobile = false;
    bool TargetSlowedByBoulder = false;
    bool TargetCommitted = false;
    bool TargetMobilitySpent = false;
    bool TargetSpellShield = false;
    bool TargetImmune = false;
    bool DestinationTerrain = false;
    bool PushesTowardEnemySafety = false;
    bool PushesThreatTowardCarry = false;
    bool ImprovesPeelDistance = false;
    bool CursorAgrees = true;
    bool Reactive = false;
    bool LethalCombo = false;
    int MineContacts = 0;
    int AlliedFollowup = 0;
    int EnemiesNearDestination = 0;
    float Distance = FLT_MAX;
    WPurpose Purpose = WPurpose::MinefieldCatch;
};

inline CastEvaluation EvaluateW(const WContext& context) {
    CastEvaluation result{};
    if (!context.Ready || !context.HasMana || !context.TargetValid ||
        !context.InRange || !context.CenterValid || !context.DirectionValid) {
        result.Reason = "W unavailable";
        return result;
    }
    if (context.TargetSpellShield || context.TargetImmune) {
        result.Reason = "W denied";
        return result;
    }
    if (context.DestinationTerrain) {
        result.Reason = "W destination terrain";
        return result;
    }
    if (context.PushesThreatTowardCarry &&
        context.Purpose != WPurpose::PeelAlly) {
        result.Reason = "W endangers carry";
        return result;
    }
    if (context.PushesTowardEnemySafety && !context.LethalCombo &&
        context.Purpose != WPurpose::PeelPlayer &&
        context.Purpose != WPurpose::PeelAlly &&
        context.Purpose != WPurpose::Interrupt) {
        result.Reason = "W saves target";
        return result;
    }
    const bool peel = context.Purpose == WPurpose::PeelPlayer ||
        context.Purpose == WPurpose::PeelAlly ||
        context.Purpose == WPurpose::Gapcloser;
    if (peel && !context.ImprovesPeelDistance) {
        result.Reason = "W fails peel direction";
        return result;
    }
    const bool reliable = context.HighConfidence || context.TargetImmobile ||
        context.TargetSlowedByBoulder || context.TargetCommitted ||
        context.TargetMobilitySpent || context.Reactive;
    if (!reliable) {
        result.Reason = "hold W for commitment";
        return result;
    }
    if ((context.Purpose == WPurpose::MinefieldCatch ||
         context.Purpose == WPurpose::FastFollowup ||
         context.Purpose == WPurpose::BigQCatch) &&
        context.MineContacts <= 0 && !context.LethalCombo) {
        result.Reason = "W misses mine conversion";
        return result;
    }
    if (!context.CursorAgrees && !peel && !context.Reactive &&
        context.AlliedFollowup <= 0) {
        result.Reason = "player direction disagrees";
        return result;
    }
    result.Cast = true;
    result.Score = 540.0f +
        static_cast<float>(context.MineContacts) * 190.0f +
        static_cast<float>(context.AlliedFollowup) * 80.0f;
    if (context.TargetSlowedByBoulder) result.Score += 170.0f;
    if (context.TargetImmobile) result.Score += 150.0f;
    if (context.TargetMobilitySpent) result.Score += 90.0f;
    if (context.ImprovesPeelDistance) result.Score += 210.0f;
    if (context.LethalCombo) result.Score += 450.0f;
    if (context.Reactive) result.Score += 260.0f;
    if (context.EnemiesNearDestination >= 2 && !peel) result.Score -= 110.0f;
    switch (context.Purpose) {
    case WPurpose::Interrupt: result.Score += 360.0f; break;
    case WPurpose::Gapcloser: result.Score += 300.0f; break;
    case WPurpose::PeelAlly: result.Score += 260.0f; break;
    case WPurpose::BigQCatch: result.Score += 180.0f; break;
    default: break;
    }
    result.Reason = peel ? "peel displacement" : "mine displacement";
    return result;
}

struct EContext {
    bool Ready = false;
    bool HasMana = false;
    bool TargetValid = false;
    bool InRange = false;
    bool CastPositionValid = false;
    bool TargetSpellShield = false;
    bool TargetImmune = false;
    bool TargetDashing = false;
    bool TargetHasReadyDash = false;
    bool TargetCommitted = false;
    bool WReady = false;
    bool QReady = false;
    bool WWillCrossMines = false;
    bool ChokePoint = false;
    bool Reactive = false;
    bool HoldForChampion = false;
    int ExpectedInitialHits = 1;
    int ExpectedMineContacts = 0;
    int NearbyEnemies = 1;
    float ManaPercent = 100.0f;
    EPurpose Purpose = EPurpose::ComboSetup;
};

inline CastEvaluation EvaluateE(const EContext& context) {
    CastEvaluation result{};
    if (!context.Ready || !context.HasMana || !context.TargetValid ||
        !context.InRange || !context.CastPositionValid) {
        result.Reason = "E unavailable";
        return result;
    }
    if (context.TargetImmune) {
        result.Reason = "E target immune";
        return result;
    }
    if (context.HoldForChampion &&
        (context.Purpose == EPurpose::Wave ||
         context.Purpose == EPurpose::Jungle)) {
        result.Reason = "reserve E for contest";
        return result;
    }
    const bool farm = context.Purpose == EPurpose::Wave ||
        context.Purpose == EPurpose::Jungle ||
        context.Purpose == EPurpose::Objective;
    if (context.Purpose == EPurpose::Wave &&
        context.ExpectedInitialHits < 2) {
        result.Reason = "E farm value too low";
        return result;
    }
    if (!farm && !context.TargetDashing && !context.TargetHasReadyDash &&
        !context.TargetCommitted && !context.WReady &&
        !context.ChokePoint && !context.Reactive) {
        result.Reason = "hold E denial";
        return result;
    }
    if (context.Purpose == EPurpose::ComboSetup && context.WReady &&
        !context.WWillCrossMines && !context.TargetDashing) {
        result.Reason = "E has no W path";
        return result;
    }
    if (context.TargetSpellShield && !context.TargetDashing &&
        context.Purpose != EPurpose::Choke) {
        result.Reason = "E would only feed spell shield";
        return result;
    }
    result.Cast = true;
    result.Score = 420.0f +
        static_cast<float>(context.ExpectedInitialHits) * 65.0f +
        static_cast<float>(context.ExpectedMineContacts) * 145.0f;
    if (context.TargetDashing) result.Score += 480.0f;
    if (context.TargetHasReadyDash) result.Score += 135.0f;
    if (context.TargetCommitted) result.Score += 100.0f;
    if (context.WReady && context.WWillCrossMines) result.Score += 230.0f;
    if (context.ChokePoint) result.Score += 180.0f;
    if (context.Reactive) result.Score += 250.0f;
    if (context.ManaPercent < 25.0f && !context.Reactive && !farm) {
        result.Score -= 220.0f;
    }
    switch (context.Purpose) {
    case EPurpose::DashPunish: result.Score += 340.0f; break;
    case EPurpose::Peel: result.Score += 260.0f; break;
    case EPurpose::Objective: result.Score += 140.0f; break;
    default: break;
    }
    result.Reason = context.TargetDashing
        ? "dash punishment" : "minefield setup";
    return result;
}

struct ManaCosts {
    float Q = 0.0f;
    float W = 0.0f;
    float E = 0.0f;
    float R = 0.0f;
};

struct BranchDefinition {
    std::array<int, 6> Slots = { -1, -1, -1, -1, -1, -1 };
    int Count = 0;
};

inline BranchDefinition DefinitionFor(ComboBranch branch) {
    BranchDefinition result{};
    switch (branch) {
    case ComboBranch::BoulderWEQ:
        result.Slots = { 0, 1, 2, 0, -1, -1 };
        result.Count = 4;
        break;
    case ComboBranch::FastWEQ:
        result.Slots = { 1, 2, 0, -1, -1, -1 };
        result.Count = 3;
        break;
    case ComboBranch::ControlledEWQ:
    case ComboBranch::DashPunishEWQ:
        result.Slots = { 2, 1, 0, -1, -1, -1 };
        result.Count = 3;
        break;
    case ComboBranch::EQPoke:
        result.Slots = { 2, 0, -1, -1, -1, -1 };
        result.Count = 2;
        break;
    case ComboBranch::QPoke:
        result.Slots = { 0, -1, -1, -1, -1, -1 };
        result.Count = 1;
        break;
    default:
        break;
    }
    return result;
}

inline float BranchMana(ComboBranch branch,
                        const ManaCosts& costs,
                        bool firstQIsBoulder = false) {
    const BranchDefinition definition = DefinitionFor(branch);
    float result = 0.0f;
    for (int index = 0; index < definition.Count; ++index) {
        const int slot = definition.Slots[static_cast<std::size_t>(index)];
        if (slot == 0) {
            result += firstQIsBoulder && index == 0
                ? 10.0f : std::max(0.0f, costs.Q);
        } else if (slot == 1) {
            result += std::max(0.0f, costs.W);
        } else if (slot == 2) {
            result += std::max(0.0f, costs.E);
        } else if (slot == 3) {
            result += std::max(0.0f, costs.R);
        }
    }
    return result;
}

struct ComboContext {
    bool TargetValid = false;
    bool QReady = false;
    bool WReady = false;
    bool EReady = false;
    bool OnWorkedGround = false;
    bool TargetDashing = false;
    bool TargetHasReadyDash = false;
    bool TargetCommitted = false;
    bool TargetImmobile = false;
    bool BoulderCanHit = false;
    bool WCanHit = false;
    bool ECanHit = false;
    bool Safe = false;
    bool Lethal = false;
    bool FastFollowupWindow = false;
    bool PreserveWForPeel = false;
    float CurrentMana = 0.0f;
    ManaCosts Costs = {};
};

inline ComboBranch ChooseComboBranch(const ComboContext& context) {
    if (!context.TargetValid || !context.Safe) return ComboBranch::None;
    if (context.TargetDashing && context.EReady && context.ECanHit &&
        context.CurrentMana + 0.5f >= BranchMana(
            ComboBranch::DashPunishEWQ, context.Costs)) {
        return ComboBranch::DashPunishEWQ;
    }
    if (context.QReady && context.OnWorkedGround &&
        context.BoulderCanHit && context.WReady && context.EReady &&
        context.WCanHit && context.ECanHit &&
        !context.PreserveWForPeel &&
        context.CurrentMana + 0.5f >= BranchMana(
            ComboBranch::BoulderWEQ, context.Costs, true)) {
        return ComboBranch::BoulderWEQ;
    }
    if (context.WReady && context.EReady && context.WCanHit &&
        context.ECanHit && !context.PreserveWForPeel &&
        (context.FastFollowupWindow || context.TargetImmobile ||
         context.TargetCommitted) &&
        context.CurrentMana + 0.5f >= BranchMana(
            ComboBranch::FastWEQ, context.Costs)) {
        return ComboBranch::FastWEQ;
    }
    if (context.EReady && context.WReady && context.ECanHit &&
        context.WCanHit && !context.PreserveWForPeel &&
        (context.TargetHasReadyDash || context.TargetCommitted ||
         context.Lethal) &&
        context.CurrentMana + 0.5f >= BranchMana(
            ComboBranch::ControlledEWQ, context.Costs)) {
        return ComboBranch::ControlledEWQ;
    }
    if (context.EReady && context.QReady && context.ECanHit &&
        context.CurrentMana + 0.5f >= BranchMana(
            ComboBranch::EQPoke, context.Costs)) {
        return ComboBranch::EQPoke;
    }
    if (context.QReady && context.CurrentMana + 0.5f >=
        std::max(0.0f, context.Costs.Q)) {
        return ComboBranch::QPoke;
    }
    return ComboBranch::None;
}

inline float SignedWallSide(const Vec3& start,
                            const Vec3& end,
                            const Vec3& point) {
    Vec3 wall = end - start;
    wall.y = 0.0f;
    Vec3 relative = point - start;
    relative.y = 0.0f;
    return Cross2D(wall, relative);
}

struct WallUnit {
    Vec3 Position = {};
    bool Ally = false;
    bool Enemy = false;
    bool Protected = false;
    bool Priority = false;
    bool Channeling = false;
};

struct WallSplit {
    int AlliesLeft = 0;
    int AlliesRight = 0;
    int EnemiesLeft = 0;
    int EnemiesRight = 0;
    int ProtectedAlliesNearWall = 0;
    int ChannelingAlliesNearWall = 0;
    int EnemiesKnockedAside = 0;
    bool PriorityEnemySeparated = false;
};

inline WallSplit AnalyzeWallSplit(const Vec3& start,
                                  const Vec3& end,
                                  const std::vector<WallUnit>& units,
                                  float nearWallRadius = 180.0f) {
    WallSplit result{};
    bool priorityLeft = false;
    bool priorityRight = false;
    for (const WallUnit& unit : units) {
        if (!unit.Position.IsValid()) continue;
        const float side = SignedWallSide(start, end, unit.Position);
        const auto projection = ProjectPointToSegment2D(
            unit.Position, start, end);
        const bool closeToWall = projection.Distance <= nearWallRadius;
        if (unit.Ally) {
            if (side >= 0.0f) ++result.AlliesLeft;
            else ++result.AlliesRight;
            if (closeToWall && unit.Protected) {
                ++result.ProtectedAlliesNearWall;
            }
            if (closeToWall && unit.Channeling) {
                ++result.ChannelingAlliesNearWall;
            }
        }
        if (unit.Enemy) {
            if (side >= 0.0f) ++result.EnemiesLeft;
            else ++result.EnemiesRight;
            if (closeToWall) ++result.EnemiesKnockedAside;
            if (unit.Priority) {
                if (side >= 0.0f) priorityLeft = true;
                else priorityRight = true;
            }
        }
    }
    result.PriorityEnemySeparated =
        (priorityLeft && result.AlliesRight > 0) ||
        (priorityRight && result.AlliesLeft > 0);
    return result;
}

enum class WallPurpose : std::uint8_t {
    Rotation,
    Cutoff,
    Objective,
    Escape,
};

struct WallContext {
    bool Ready = false;
    bool HasMana = false;
    bool ManualAuthorized = false;
    bool OriginValid = false;
    bool EndpointValid = false;
    bool PlayerRecentlyDamaged = false;
    bool PlayerImmobilized = false;
    bool InterruptThreat = false;
    bool CursorAgrees = false;
    bool RouteNavigable = false;
    bool ObjectiveSecuredSide = false;
    bool EscapeSeparatesPursuers = false;
    float Distance = 0.0f;
    WallSplit Split = {};
    WallPurpose Purpose = WallPurpose::Rotation;
};

inline CastEvaluation EvaluateWall(const WallContext& context) {
    CastEvaluation result{};
    if (!context.Ready || !context.HasMana || !context.ManualAuthorized ||
        !context.OriginValid || !context.EndpointValid ||
        !context.CursorAgrees || !context.RouteNavigable ||
        context.Distance < kRMinimumUsefulRange) {
        result.Reason = "R not player-authorized";
        return result;
    }
    if (context.PlayerRecentlyDamaged || context.PlayerImmobilized ||
        context.InterruptThreat) {
        result.Reason = "R channel unsafe";
        return result;
    }
    if (context.Split.ChannelingAlliesNearWall > 0 ||
        context.Split.ProtectedAlliesNearWall > 0) {
        result.Reason = "R would disrupt ally";
        return result;
    }
    if (context.Purpose == WallPurpose::Objective &&
        !context.ObjectiveSecuredSide) {
        result.Reason = "R fails objective partition";
        return result;
    }
    if (context.Purpose == WallPurpose::Escape &&
        !context.EscapeSeparatesPursuers) {
        result.Reason = "R fails escape partition";
        return result;
    }
    result.Cast = true;
    result.Score = 520.0f + context.Distance * 0.025f +
        static_cast<float>(context.Split.EnemiesKnockedAside) * 80.0f;
    if (context.Split.PriorityEnemySeparated) result.Score += 220.0f;
    if (context.ObjectiveSecuredSide) result.Score += 260.0f;
    if (context.EscapeSeparatesPursuers) result.Score += 300.0f;
    switch (context.Purpose) {
    case WallPurpose::Objective: result.Score += 210.0f; break;
    case WallPurpose::Escape: result.Score += 180.0f; break;
    case WallPurpose::Cutoff: result.Score += 130.0f; break;
    default: break;
    }
    result.Reason = "manual safe wall";
    return result;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Taliyah::Geometry
