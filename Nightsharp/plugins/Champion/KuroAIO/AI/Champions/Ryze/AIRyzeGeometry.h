#pragma once

// Deterministic Ryze mechanics and one-trick policy. Runtime prediction,
// buff/object discovery, NavMesh queries and casts belong to AIRyzeController.
// This layer owns Overload first-body interception, Rune/Flux bookkeeping,
// indirect E-Q bridges, branch-specific mana, auto-weave gates and safe
// player-authorized Realm Warp policy so the difficult decisions are testable.

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Ryze::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::RankValue;
using SharedGeometry::SolveMovingCircleContactTime2D;

inline constexpr float kQCastSeconds = 0.25f;
inline constexpr float kQRange = 1000.0f;
inline constexpr float kQMissileRadius = 55.0f;
inline constexpr float kQMissileSpeed = 1700.0f;
inline constexpr float kWRange = 550.0f;
inline constexpr float kWCastSeconds = 0.25f;
inline constexpr float kWCrowdControlSeconds = 1.5f;
inline constexpr float kWSlowPercent = 50.0f;
inline constexpr float kERange = 550.0f;
inline constexpr float kECastSeconds = 0.25f;
inline constexpr float kEMissileSpeed = 3500.0f;
inline constexpr float kFluxSeconds = 4.0f;
inline constexpr float kFluxSpreadRadius = 350.0f;
inline constexpr float kFluxSpreadRadiusLarge = 400.0f;
inline constexpr float kQFluxSpreadRadius = 350.0f;
inline constexpr float kQFluxSpreadRadiusLarge = 500.0f;
inline constexpr int kMaximumRunes = 2;
inline constexpr float kRuneSeconds = 4.0f;
inline constexpr float kRealmWarpMinimumRange = 1000.0f;
inline constexpr float kRealmWarpMaximumRange = 3000.0f;
inline constexpr float kRealmWarpRadius = 365.0f;
inline constexpr float kRealmWarpChargeSeconds = 2.10f;
inline constexpr float kRealmWarpTeleportSeconds = 0.65f;

inline float ArcaneMasteryMaximumMana(float unamplifiedMaximumMana,
                                      float abilityPower) {
    const float mana = std::max(0.0f, unamplifiedMaximumMana);
    const float ap = std::max(0.0f, abilityPower);
    return mana * (1.0f + 0.10f * ap / 100.0f);
}

inline float QRawDamage(int rank, float abilityPower, float bonusMana) {
    static constexpr std::array<float, 6> base = {
        0.0f, 75.0f, 95.0f, 115.0f, 135.0f, 155.0f,
    };
    if (rank <= 0) return 0.0f;
    return RankValue(base, rank) + 0.55f * std::max(0.0f, abilityPower) +
           0.02f * std::max(0.0f, bonusMana);
}

inline float WRawDamage(int rank, float abilityPower, float bonusMana) {
    static constexpr std::array<float, 6> base = {
        0.0f, 60.0f, 90.0f, 120.0f, 150.0f, 180.0f,
    };
    if (rank <= 0) return 0.0f;
    return RankValue(base, rank) + 0.60f * std::max(0.0f, abilityPower) +
           0.03f * std::max(0.0f, bonusMana);
}

inline float ERawDamage(int rank, float abilityPower, float bonusMana) {
    static constexpr std::array<float, 6> base = {
        0.0f, 60.0f, 90.0f, 120.0f, 150.0f, 180.0f,
    };
    if (rank <= 0) return 0.0f;
    return RankValue(base, rank) + 0.50f * std::max(0.0f, abilityPower) +
           0.02f * std::max(0.0f, bonusMana);
}

inline float FluxQBonusPercent(int realmWarpRank) {
    static constexpr std::array<float, 4> bonus = {
        25.0f, 50.0f, 75.0f, 100.0f,
    };
    return RankValue(bonus, realmWarpRank);
}

inline float FluxedQRawDamage(int qRank,
                              int realmWarpRank,
                              float abilityPower,
                              float bonusMana) {
    return QRawDamage(qRank, abilityPower, bonusMana) *
           (1.0f + FluxQBonusPercent(realmWarpRank) / 100.0f);
}

inline float TwoRuneMoveSpeedPercent(int qRank) {
    static constexpr std::array<float, 6> amount = {
        0.0f, 28.0f, 32.0f, 36.0f, 40.0f, 44.0f,
    };
    return qRank <= 0 ? 0.0f : RankValue(amount, qRank);
}

struct RuneLedger {
    int Stacks = 0;
    float ExpiresAt = 0.0f;
};

inline RuneLedger NormalizeRunes(RuneLedger ledger, float nowSeconds) {
    ledger.Stacks = std::clamp(ledger.Stacks, 0, kMaximumRunes);
    if (!std::isfinite(ledger.ExpiresAt) ||
        nowSeconds >= ledger.ExpiresAt) {
        ledger.Stacks = 0;
        ledger.ExpiresAt = 0.0f;
    }
    return ledger;
}

inline RuneLedger AddRune(RuneLedger ledger, float nowSeconds) {
    ledger = NormalizeRunes(ledger, nowSeconds);
    ledger.Stacks = std::min(kMaximumRunes, ledger.Stacks + 1);
    ledger.ExpiresAt = nowSeconds + kRuneSeconds;
    return ledger;
}

struct RuneSpend {
    RuneLedger After = {};
    int Consumed = 0;
    bool GrantsMoveSpeed = false;
};

inline RuneSpend SpendRunesWithQ(RuneLedger ledger, float nowSeconds) {
    ledger = NormalizeRunes(ledger, nowSeconds);
    RuneSpend spend{};
    spend.Consumed = ledger.Stacks;
    spend.GrantsMoveSpeed = ledger.Stacks >= kMaximumRunes;
    spend.After = {};
    return spend;
}

struct ManaCosts {
    float Q = 0.0f;
    float W = 0.0f;
    float E = 0.0f;
    float R = 0.0f;
};

enum class ComboBranch : std::uint8_t {
    None,
    ImmediateRootWQEQ,
    FastRootEWQ,
    RootedBurstQEWQ,
    MaximumDpsQEQWQEQ,
    TripleQNoRootQWQEQ,
    SlowSpeedWEQ,
    FastTradeQEQ,
    FluxBurstEQ,
    KiteEWQ,
    ClearEEQ,
};

struct BranchDefinition {
    std::array<int, 8> Slots = {};
    int Count = 0;
};

inline BranchDefinition DefinitionFor(ComboBranch branch) {
    BranchDefinition result{};
    switch (branch) {
    case ComboBranch::ImmediateRootWQEQ:
        result.Slots = { 1, 0, 2, 0, -1, -1, -1, -1 };
        result.Count = 4;
        break;
    case ComboBranch::FastRootEWQ:
    case ComboBranch::KiteEWQ:
        result.Slots = { 2, 1, 0, -1, -1, -1, -1, -1 };
        result.Count = 3;
        break;
    case ComboBranch::RootedBurstQEWQ:
        result.Slots = { 0, 2, 1, 0, -1, -1, -1, -1 };
        result.Count = 4;
        break;
    case ComboBranch::MaximumDpsQEQWQEQ:
        result.Slots = { 0, 2, 0, 1, 0, 2, 0, -1 };
        result.Count = 7;
        break;
    case ComboBranch::TripleQNoRootQWQEQ:
        result.Slots = { 0, 1, 0, 2, 0, -1, -1, -1 };
        result.Count = 5;
        break;
    case ComboBranch::SlowSpeedWEQ:
        result.Slots = { 1, 2, 0, -1, -1, -1, -1, -1 };
        result.Count = 3;
        break;
    case ComboBranch::FastTradeQEQ:
        result.Slots = { 0, 2, 0, -1, -1, -1, -1, -1 };
        result.Count = 3;
        break;
    case ComboBranch::FluxBurstEQ:
        result.Slots = { 2, 0, -1, -1, -1, -1, -1, -1 };
        result.Count = 2;
        break;
    case ComboBranch::ClearEEQ:
        result.Slots = { 2, 2, 0, -1, -1, -1, -1, -1 };
        result.Count = 3;
        break;
    default:
        result.Slots.fill(-1);
        break;
    }
    return result;
}

inline float BranchMana(ComboBranch branch, const ManaCosts& costs) {
    const BranchDefinition definition = DefinitionFor(branch);
    float total = 0.0f;
    for (int index = 0; index < definition.Count; ++index) {
        switch (definition.Slots[static_cast<std::size_t>(index)]) {
        case 0: total += std::max(0.0f, costs.Q); break;
        case 1: total += std::max(0.0f, costs.W); break;
        case 2: total += std::max(0.0f, costs.E); break;
        case 3: total += std::max(0.0f, costs.R); break;
        default: break;
        }
    }
    return total;
}

struct QBody {
    int Id = 0;
    Vec3 Position = {};
    Vec3 Velocity = {};
    float Radius = 0.0f;
    float Health = 1.0f;
    float MaximumHealth = 1.0f;
    float FluxExpiresAt = 0.0f;
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

inline bool FluxActive(const QBody& body, float nowSeconds) {
    return body.Valid && body.FluxExpiresAt > nowSeconds;
}

struct QContact {
    bool Hit = false;
    int BodyId = 0;
    int BodyIndex = -1;
    float ProjectileSeconds = FLT_MAX;
    float CastElapsedSeconds = FLT_MAX;
    float MissileDistance = FLT_MAX;
    Vec3 MissilePosition = {};
    Vec3 TargetPosition = {};
};

inline QContact ContactWithQBody(const Vec3& start,
                                 const Vec3& castPosition,
                                 const QBody& body,
                                 int bodyIndex = -1) {
    QContact result{};
    if (!start.IsValid() || !castPosition.IsValid() || !body.Valid ||
        !body.Targetable || !body.Hostile || body.Id == 0) {
        return result;
    }
    const Vec3 direction = Direction2D(start, castPosition);
    if (direction.IsZero()) return result;

    Vec3 relativePosition = body.PositionAt(kQCastSeconds) - start;
    relativePosition.y = 0.0f;
    Vec3 relativeVelocity = body.Velocity - direction * kQMissileSpeed;
    relativeVelocity.y = 0.0f;
    const float maximumFlight = kQRange / kQMissileSpeed;
    float contactSeconds = 0.0f;
    if (!SolveMovingCircleContactTime2D(
            relativePosition, relativeVelocity,
            kQMissileRadius + std::clamp(body.Radius, 0.0f, 220.0f),
            maximumFlight, contactSeconds)) {
        return result;
    }

    const Vec3 targetPosition = body.PositionAt(
        kQCastSeconds + contactSeconds);
    const Vec3 fromStart = targetPosition - start;
    const float longitudinal = fromStart.Dot(direction);
    const float lateral = std::fabs(
        fromStart.x * direction.z - fromStart.z * direction.x);
    const float bodyRadius = std::max(0.0f, body.Radius);
    if (longitudinal < -bodyRadius ||
        longitudinal > kQRange + bodyRadius + 0.01f ||
        lateral > kQMissileRadius + bodyRadius + 0.01f) {
        return result;
    }

    result.Hit = true;
    result.BodyId = body.Id;
    result.BodyIndex = bodyIndex;
    result.ProjectileSeconds = contactSeconds;
    result.CastElapsedSeconds = kQCastSeconds + contactSeconds;
    result.MissileDistance = std::min(
        kQRange, contactSeconds * kQMissileSpeed);
    result.MissilePosition = start + direction * result.MissileDistance;
    result.TargetPosition = targetPosition;
    return result;
}

inline QContact FirstQContact(const Vec3& start,
                              const Vec3& castPosition,
                              const std::vector<QBody>& bodies) {
    QContact best{};
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        const QContact candidate = ContactWithQBody(
            start, castPosition, bodies[index], static_cast<int>(index));
        if (!candidate.Hit) continue;
        if (!best.Hit ||
            candidate.ProjectileSeconds < best.ProjectileSeconds - 0.0001f ||
            (std::fabs(candidate.ProjectileSeconds - best.ProjectileSeconds) <=
                 0.0001f && candidate.BodyId < best.BodyId)) {
            best = candidate;
        }
    }
    return best;
}

inline bool QHitsIntendedFirst(const Vec3& start,
                               const Vec3& castPosition,
                               const std::vector<QBody>& bodies,
                               int intendedId,
                               QContact* contact = nullptr) {
    const QContact first = FirstQContact(start, castPosition, bodies);
    if (contact) *contact = first;
    return first.Hit && first.BodyId == intendedId;
}

inline const QBody* FindBody(const std::vector<QBody>& bodies, int id) {
    for (const auto& body : bodies) {
        if (body.Id == id && body.Valid) return &body;
    }
    return nullptr;
}

inline float EMarkRadius(const QBody& primary) {
    return primary.Large ? kFluxSpreadRadiusLarge : kFluxSpreadRadius;
}

inline float QFluxRadius(const QBody& primary) {
    return primary.Large ? kQFluxSpreadRadiusLarge : kQFluxSpreadRadius;
}

inline std::vector<int> SpellFluxMarkedIds(
    int primaryId,
    const std::vector<QBody>& bodies) {
    std::vector<int> result;
    const QBody* primary = FindBody(bodies, primaryId);
    if (!primary || !primary->Targetable || !primary->Hostile) return result;
    const float radius = EMarkRadius(*primary);
    for (const auto& body : bodies) {
        if (!body.Valid || !body.Targetable || !body.Hostile) continue;
        if (body.Id == primaryId ||
            body.Position.Distance2D(primary->Position) <=
                radius + std::max(0.0f, body.Radius)) {
            result.push_back(body.Id);
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

inline std::vector<int> FluxedQVictimIds(
    int primaryId,
    const std::vector<QBody>& bodies,
    float nowSeconds) {
    std::vector<int> result;
    const QBody* primary = FindBody(bodies, primaryId);
    if (!primary || !FluxActive(*primary, nowSeconds)) return result;
    const float radius = QFluxRadius(*primary);
    for (const auto& body : bodies) {
        if (!body.Valid || !body.Targetable || !body.Hostile ||
            !FluxActive(body, nowSeconds)) {
            continue;
        }
        if (body.Id == primaryId ||
            body.Position.Distance2D(primary->Position) <=
                radius + std::max(0.0f, body.Radius)) {
            result.push_back(body.Id);
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

inline bool ContainsId(const std::vector<int>& values, int id) {
    return std::find(values.begin(), values.end(), id) != values.end();
}

struct FluxBridgePlan {
    bool Valid = false;
    int EPrimaryId = 0;
    int QDetonationId = 0;
    int PriorityVictimId = 0;
    int MarkedUnits = 0;
    int QVictims = 0;
    float Score = -FLT_MAX;
};

inline FluxBridgePlan BestFluxBridge(const Vec3& playerPosition,
                                     int priorityVictimId,
                                     const std::vector<QBody>& bodies,
                                     float expectedEDamage,
                                     bool requireNonChampionPrimary = true) {
    FluxBridgePlan best{};
    const QBody* victim = FindBody(bodies, priorityVictimId);
    if (!victim || !victim->Champion) return best;

    for (const auto& primary : bodies) {
        if (!primary.Valid || !primary.Targetable || !primary.Hostile ||
            primary.Id == priorityVictimId ||
            (requireNonChampionPrimary && primary.Champion) ||
            playerPosition.Distance2D(primary.Position) >
                kERange + std::max(0.0f, primary.Radius)) {
            continue;
        }
        const std::vector<int> marked = SpellFluxMarkedIds(primary.Id, bodies);
        if (!ContainsId(marked, priorityVictimId)) continue;

        for (const int detonationId : marked) {
            const QBody* detonation = FindBody(bodies, detonationId);
            if (!detonation || detonation->Id == priorityVictimId ||
                detonation->Health <= expectedEDamage + 2.0f) {
                continue;
            }
            const float spread = QFluxRadius(*detonation);
            if (detonation->Position.Distance2D(victim->Position) >
                spread + std::max(0.0f, victim->Radius)) {
                continue;
            }
            int qVictims = 0;
            for (const int markedId : marked) {
                const QBody* markedBody = FindBody(bodies, markedId);
                if (markedBody &&
                    markedBody->Position.Distance2D(detonation->Position) <=
                        spread + std::max(0.0f, markedBody->Radius)) {
                    ++qVictims;
                }
            }
            float score = 700.0f + static_cast<float>(marked.size()) * 42.0f +
                          static_cast<float>(qVictims) * 58.0f;
            score -= playerPosition.Distance2D(primary.Position) * 0.10f;
            score -= playerPosition.Distance2D(detonation->Position) * 0.035f;
            if (primary.Large) score += 32.0f;
            if (detonation->Large) score += 45.0f;
            if (!best.Valid || score > best.Score ||
                (std::fabs(score - best.Score) <= 0.001f &&
                 primary.Id < best.EPrimaryId)) {
                best.Valid = true;
                best.EPrimaryId = primary.Id;
                best.QDetonationId = detonationId;
                best.PriorityVictimId = priorityVictimId;
                best.MarkedUnits = static_cast<int>(marked.size());
                best.QVictims = qVictims;
                best.Score = score;
            }
        }
    }
    return best;
}

struct WaveFluxPlan {
    bool Valid = false;
    int EPrimaryId = 0;
    int QDetonationId = 0;
    int MarkedUnits = 0;
    int QVictims = 0;
    int ExpectedELastHits = 0;
    bool PreservesLargeMinion = true;
    float Score = -FLT_MAX;
};

inline WaveFluxPlan BestWaveFluxPlan(const Vec3& playerPosition,
                                     const std::vector<QBody>& bodies,
                                     float expectedEDamage,
                                     int minimumVictims) {
    WaveFluxPlan best{};
    for (const auto& primary : bodies) {
        if (!primary.Valid || !primary.Targetable || !primary.Hostile ||
            (!primary.Minion && !primary.Monster) ||
            playerPosition.Distance2D(primary.Position) >
                kERange + std::max(0.0f, primary.Radius)) {
            continue;
        }
        const std::vector<int> marked = SpellFluxMarkedIds(primary.Id, bodies);
        for (const int detonationId : marked) {
            const QBody* detonation = FindBody(bodies, detonationId);
            if (!detonation || (!detonation->Minion && !detonation->Monster) ||
                detonation->Health <= expectedEDamage + 2.0f) {
                continue;
            }
            const float radius = QFluxRadius(*detonation);
            int victims = 0;
            for (const int id : marked) {
                const QBody* unit = FindBody(bodies, id);
                if (unit && (unit->Minion || unit->Monster) &&
                    unit->Position.Distance2D(detonation->Position) <=
                        radius + std::max(0.0f, unit->Radius)) {
                    ++victims;
                }
            }
            if (victims < std::max(1, minimumVictims)) continue;
            const int eLastHits = primary.Health <= expectedEDamage ? 1 : 0;
            const bool preservesLarge = !primary.Large || eLastHits > 0 ||
                primary.Health > expectedEDamage * 2.0f;
            float score = static_cast<float>(victims) * 130.0f +
                          static_cast<float>(marked.size()) * 35.0f +
                          static_cast<float>(eLastHits) * 90.0f;
            if (primary.Large && !preservesLarge) score -= 330.0f;
            if (primary.Epic) score += 440.0f;
            score -= playerPosition.Distance2D(primary.Position) * 0.04f;
            if (!best.Valid || score > best.Score ||
                (std::fabs(score - best.Score) <= 0.001f &&
                 primary.Id < best.EPrimaryId)) {
                best.Valid = true;
                best.EPrimaryId = primary.Id;
                best.QDetonationId = detonationId;
                best.MarkedUnits = static_cast<int>(marked.size());
                best.QVictims = victims;
                best.ExpectedELastHits = eLastHits;
                best.PreservesLargeMinion = preservesLarge;
                best.Score = score;
            }
        }
    }
    return best;
}

enum class QPurpose : std::uint8_t {
    Dps,
    FluxBurst,
    RootFollowup,
    SpeedExit,
    Harass,
    Kill,
    Peel,
    Interrupt,
    Clear,
    Objective,
    ManualResetAssist,
};

struct QContext {
    bool Ready = false;
    bool HasMana = false;
    bool TargetValid = false;
    bool IntendedFirstBody = false;
    bool ProjectileWallBlocked = false;
    bool TargetSpellShield = false;
    bool TargetImmune = false;
    bool TargetFluxed = false;
    bool TargetImmobile = false;
    bool TargetDashing = false;
    bool HighConfidence = false;
    bool Lethal = false;
    bool PlayerAttackWindingUp = false;
    bool Reactive = false;
    bool SpeedNeeded = false;
    bool PreserveTwoRuneSpeed = false;
    bool PriorityVictimHitByFluxSpread = false;
    bool CursorAgrees = true;
    int RuneStacks = 0;
    int FluxVictims = 1;
    int NearbyEnemies = 1;
    float CollisionConfidence = 1.0f;
    QPurpose Purpose = QPurpose::Dps;
};

struct CastEvaluation {
    bool Cast = false;
    float Score = -FLT_MAX;
    const char* Reason = "invalid";
};

inline CastEvaluation EvaluateQ(const QContext& context) {
    CastEvaluation result{};
    if (!context.Ready || !context.HasMana || !context.TargetValid) {
        result.Reason = "Q unavailable";
        return result;
    }
    if (!context.IntendedFirstBody || context.ProjectileWallBlocked) {
        result.Reason = "Q first body or projectile wall";
        return result;
    }
    if (context.TargetImmune || context.TargetSpellShield) {
        result.Reason = "Q denied by immunity or spell shield";
        return result;
    }
    const bool urgent = context.Reactive || context.Lethal ||
        context.Purpose == QPurpose::Peel ||
        context.Purpose == QPurpose::Interrupt ||
        context.Purpose == QPurpose::SpeedExit;
    if (context.PlayerAttackWindingUp && !urgent) {
        result.Reason = "preserve attack windup";
        return result;
    }
    if (!context.HighConfidence && !context.TargetImmobile &&
        !context.TargetDashing && context.Purpose != QPurpose::Clear &&
        context.Purpose != QPurpose::Objective) {
        result.Reason = "Q prediction below branch threshold";
        return result;
    }
    if (!context.CursorAgrees && !urgent &&
        context.Purpose != QPurpose::Clear) {
        result.Reason = "Q conflicts with player cursor";
        return result;
    }
    if (context.PreserveTwoRuneSpeed && context.RuneStacks >= 2 &&
        !context.SpeedNeeded && !context.Lethal &&
        context.Purpose == QPurpose::Harass) {
        result.Reason = "preserve charged movement-speed Q";
        return result;
    }

    float score = 210.0f +
        std::clamp(context.CollisionConfidence, 0.0f, 1.0f) * 150.0f;
    if (context.TargetFluxed) score += 180.0f;
    if (context.PriorityVictimHitByFluxSpread) score += 240.0f;
    if (context.TargetImmobile || context.TargetDashing) score += 120.0f;
    if (context.Lethal) score += 700.0f;
    if (context.RuneStacks >= 2 && context.SpeedNeeded) score += 260.0f;
    if (context.RuneStacks == 1 && context.Purpose == QPurpose::Harass)
        score -= 45.0f;
    score += static_cast<float>(std::max(0, context.FluxVictims - 1)) * 85.0f;
    score += static_cast<float>(std::max(0, context.NearbyEnemies - 1)) * 25.0f;
    switch (context.Purpose) {
    case QPurpose::Kill: score += 320.0f; break;
    case QPurpose::Peel:
    case QPurpose::Interrupt: score += 270.0f; break;
    case QPurpose::SpeedExit: score += 220.0f; break;
    case QPurpose::FluxBurst: score += 170.0f; break;
    case QPurpose::RootFollowup: score += 145.0f; break;
    case QPurpose::Objective: score += 90.0f; break;
    case QPurpose::ManualResetAssist: score += 115.0f; break;
    default: break;
    }
    result.Cast = score >= 300.0f;
    result.Score = score;
    result.Reason = result.Cast ? "authorized Q" : "Q value below threshold";
    return result;
}

enum class WPurpose : std::uint8_t {
    ImmediateRoot,
    RootSetup,
    SlowForSpeed,
    DpsReset,
    Peel,
    Interrupt,
    AntiGapcloser,
    JungleReset,
};

struct WContext {
    bool Ready = false;
    bool HasMana = false;
    bool TargetValid = false;
    bool InRange = false;
    bool TargetFluxed = false;
    bool TargetSpellShield = false;
    bool TargetImmune = false;
    bool TargetMobilityReady = false;
    bool TargetCommitted = false;
    bool TargetAlreadyHardCrowdControlled = false;
    bool PlayerAttackWindingUp = false;
    bool Reactive = false;
    bool FollowupQReady = false;
    bool EReady = false;
    bool RootRequired = false;
    bool Lethal = false;
    int RuneStacks = 0;
    WPurpose Purpose = WPurpose::DpsReset;
};

inline CastEvaluation EvaluateW(const WContext& context) {
    CastEvaluation result{};
    if (!context.Ready || !context.HasMana || !context.TargetValid ||
        !context.InRange) {
        result.Reason = "W unavailable or out of range";
        return result;
    }
    if (context.TargetSpellShield || context.TargetImmune) {
        result.Reason = "W denied by target state";
        return result;
    }
    if (context.RootRequired && !context.TargetFluxed) {
        result.Reason = "root branch requires Flux before W";
        return result;
    }
    const bool urgent = context.Reactive || context.Lethal ||
        context.Purpose == WPurpose::Peel ||
        context.Purpose == WPurpose::Interrupt ||
        context.Purpose == WPurpose::AntiGapcloser;
    if (context.PlayerAttackWindingUp && !urgent) {
        result.Reason = "preserve attack before W";
        return result;
    }
    if (context.TargetAlreadyHardCrowdControlled &&
        context.Purpose == WPurpose::RootSetup && !context.Lethal) {
        result.Reason = "do not overlap root without payoff";
        return result;
    }

    float score = 220.0f;
    if (context.TargetFluxed) score += 290.0f;
    if (context.TargetMobilityReady && context.TargetFluxed) score += 170.0f;
    if (context.TargetCommitted) score += 100.0f;
    if (context.FollowupQReady) score += 80.0f;
    if (context.RuneStacks == 1) score += 75.0f;
    if (context.Lethal) score += 500.0f;
    switch (context.Purpose) {
    case WPurpose::Interrupt: score += context.TargetFluxed ? 430.0f : 70.0f; break;
    case WPurpose::Peel:
    case WPurpose::AntiGapcloser: score += 320.0f; break;
    case WPurpose::ImmediateRoot: score += 270.0f; break;
    case WPurpose::SlowForSpeed: score += context.EReady ? 125.0f : -90.0f; break;
    case WPurpose::JungleReset: score += 55.0f; break;
    default: break;
    }
    result.Cast = score >= 300.0f;
    result.Score = score;
    result.Reason = result.Cast ? "authorized W" : "W value below threshold";
    return result;
}

enum class EPurpose : std::uint8_t {
    RootSetup,
    DamageReset,
    FluxBurst,
    FluxBridge,
    WaveSpread,
    RefreshFlux,
    PeelSetup,
    InterruptSetup,
    Jungle,
    Objective,
};

struct EContext {
    bool Ready = false;
    bool HasMana = false;
    bool TargetValid = false;
    bool InRange = false;
    bool TargetSpellShield = false;
    bool TargetImmune = false;
    bool TargetFluxed = false;
    bool WReady = false;
    bool QReady = false;
    bool TargetMobilityReady = false;
    bool TargetCommitted = false;
    bool TargetWillSurviveE = true;
    bool PriorityVictimWillBeMarked = false;
    bool PlayerAttackWindingUp = false;
    bool Reactive = false;
    bool Lethal = false;
    int MarkedUnits = 1;
    int RuneStacks = 0;
    EPurpose Purpose = EPurpose::DamageReset;
};

inline CastEvaluation EvaluateE(const EContext& context) {
    CastEvaluation result{};
    if (!context.Ready || !context.HasMana || !context.TargetValid ||
        !context.InRange) {
        result.Reason = "E unavailable or out of range";
        return result;
    }
    if (context.TargetSpellShield || context.TargetImmune) {
        result.Reason = "E denied by target state";
        return result;
    }
    const bool urgent = context.Reactive || context.Lethal ||
        context.Purpose == EPurpose::PeelSetup ||
        context.Purpose == EPurpose::InterruptSetup;
    if (context.PlayerAttackWindingUp && !urgent) {
        result.Reason = "preserve attack before E";
        return result;
    }
    if ((context.Purpose == EPurpose::FluxBridge ||
         context.Purpose == EPurpose::WaveSpread) &&
        (!context.PriorityVictimWillBeMarked || !context.TargetWillSurviveE)) {
        result.Reason = "E bridge has no surviving Q detonation";
        return result;
    }
    if (context.Purpose == EPurpose::RootSetup && !context.WReady) {
        result.Reason = "E root setup has no W";
        return result;
    }

    float score = 225.0f;
    if (context.QReady) score += 115.0f;
    if (context.WReady && !context.TargetFluxed) score += 90.0f;
    if (context.TargetMobilityReady && context.WReady) score += 105.0f;
    if (context.TargetCommitted) score += 80.0f;
    if (context.PriorityVictimWillBeMarked) score += 225.0f;
    if (context.Lethal) score += 500.0f;
    score += static_cast<float>(std::max(0, context.MarkedUnits - 1)) * 60.0f;
    switch (context.Purpose) {
    case EPurpose::RootSetup: score += 245.0f; break;
    case EPurpose::PeelSetup:
    case EPurpose::InterruptSetup: score += 310.0f; break;
    case EPurpose::FluxBridge: score += 260.0f; break;
    case EPurpose::WaveSpread: score += 100.0f; break;
    case EPurpose::Objective: score += 90.0f; break;
    default: break;
    }
    result.Cast = score >= 300.0f;
    result.Score = score;
    result.Reason = result.Cast ? "authorized E" : "E value below threshold";
    return result;
}

struct ComboContext {
    bool QReady = false;
    bool WReady = false;
    bool EReady = false;
    bool TargetValid = false;
    bool TargetFluxed = false;
    bool TargetCommitted = false;
    bool TargetMobilityReady = false;
    bool TargetHardCrowdControlled = false;
    bool CleanQ = false;
    bool SafeToCommit = false;
    bool PeelUrgent = false;
    bool InterruptUrgent = false;
    bool SpeedNeeded = false;
    bool FullDpsWindow = false;
    bool Harass = false;
    bool Clear = false;
    bool Lethal = false;
    int RuneStacks = 0;
    int NearbyEnemies = 1;
    float TargetDistance = FLT_MAX;
    float CurrentMana = 0.0f;
    float ReservedMana = 0.0f;
    ManaCosts Costs = {};
};

struct ComboDecision {
    ComboBranch Branch = ComboBranch::None;
    float RequiredMana = 0.0f;
    float Score = -FLT_MAX;
    bool RootBranch = false;
    bool PreservesW = false;
    const char* Reason = "no branch";
};

inline bool CanAffordBranch(const ComboContext& context,
                            ComboBranch branch,
                            float extraReserve = 0.0f) {
    return context.CurrentMana + 0.5f >=
        BranchMana(branch, context.Costs) +
        std::max(0.0f, context.ReservedMana) +
        std::max(0.0f, extraReserve);
}

inline ComboDecision SelectComboBranch(const ComboContext& context) {
    ComboDecision result{};
    if (!context.TargetValid) return result;

    const auto choose = [&](ComboBranch branch,
                            float score,
                            bool root,
                            bool preserveW,
                            const char* reason) {
        ComboDecision decision{};
        decision.Branch = branch;
        decision.RequiredMana = BranchMana(branch, context.Costs);
        decision.Score = score;
        decision.RootBranch = root;
        decision.PreservesW = preserveW;
        decision.Reason = reason;
        return decision;
    };

    if ((context.PeelUrgent || context.InterruptUrgent) &&
        context.TargetFluxed && context.WReady &&
        context.EReady && CanAffordBranch(
            context, ComboBranch::ImmediateRootWQEQ, 0.0f)) {
        return choose(ComboBranch::ImmediateRootWQEQ, 1250.0f, true, false,
                      "consume existing Flux for immediate root");
    }
    if ((context.PeelUrgent || context.InterruptUrgent ||
         context.SpeedNeeded) && context.EReady && context.WReady &&
        context.TargetDistance <= kERange + 80.0f &&
        CanAffordBranch(context, ComboBranch::KiteEWQ, 0.0f)) {
        return choose(ComboBranch::KiteEWQ, 1160.0f, true, false,
                      "E-W root then consume two runes for kite speed");
    }
    if (context.TargetFluxed && context.WReady &&
        context.EReady && context.TargetMobilityReady &&
        CanAffordBranch(context, ComboBranch::ImmediateRootWQEQ)) {
        return choose(ComboBranch::ImmediateRootWQEQ, 1040.0f, true, false,
                      "root mobile Flux target before it can escape");
    }
    if (context.FullDpsWindow && context.SafeToCommit && context.CleanQ &&
        context.QReady && context.WReady && context.EReady &&
        CanAffordBranch(context, ComboBranch::MaximumDpsQEQWQEQ)) {
        return choose(ComboBranch::MaximumDpsQEQWQEQ, 980.0f, false, false,
                      "committed target permits four-reset maximum DPS");
    }
    if (context.TargetMobilityReady && context.CleanQ && context.QReady &&
        context.EReady && context.WReady &&
        CanAffordBranch(context, ComboBranch::RootedBurstQEWQ)) {
        return choose(ComboBranch::RootedBurstQEWQ, 900.0f, true, false,
                      "lead Q then E-W so prison lands before travelling Q");
    }
    if (context.Lethal && context.CleanQ && context.QReady &&
        context.WReady && context.EReady &&
        CanAffordBranch(context, ComboBranch::TripleQNoRootQWQEQ)) {
        return choose(ComboBranch::TripleQNoRootQWQEQ, 875.0f, false, false,
                      "three-Q lethal is worth sacrificing root");
    }
    if (context.SpeedNeeded && context.WReady && context.EReady &&
        CanAffordBranch(context, ComboBranch::SlowSpeedWEQ)) {
        return choose(ComboBranch::SlowSpeedWEQ, 820.0f, false, false,
                      "instant W slow followed by two-rune speed exit");
    }
    if (context.Harass && context.CleanQ && context.QReady &&
        context.EReady && CanAffordBranch(context, ComboBranch::FastTradeQEQ)) {
        return choose(ComboBranch::FastTradeQEQ, 700.0f, false, true,
                      "short Q-E-Q trade preserves Rune Prison");
    }
    // E resets Overload, so an E-Q branch remains legal while Q itself is
    // cooling down. Requiring QReady here discards one of Ryze's defining
    // reset windows and makes the controller hesitate after a player Q.
    if (context.EReady &&
        CanAffordBranch(context, ComboBranch::FluxBurstEQ)) {
        return choose(ComboBranch::FluxBurstEQ, 620.0f, false, true,
                      "E-Q Flux burst with W held for control");
    }
    if (context.Clear && context.EReady &&
        CanAffordBranch(context, ComboBranch::ClearEEQ)) {
        return choose(ComboBranch::ClearEEQ, 560.0f, false, true,
                      "delayed E-E-Q wave compression");
    }
    return result;
}

struct AutoWeaveContext {
    bool TargetValid = false;
    bool InAttackRange = false;
    bool AttackReady = false;
    bool Safe = false;
    bool TargetCanInstantEscape = false;
    bool TargetRooted = false;
    bool BufferWindowActive = false;
    bool NextSpellLethal = false;
    bool NextSpellCriticalPeel = false;
    bool PlayerIssuedAttack = false;
    int MillisecondsUntilNextReset = 0;
};

inline bool ShouldWeaveAuto(const AutoWeaveContext& context) {
    if (!context.TargetValid || !context.InAttackRange ||
        !context.AttackReady || !context.Safe) return false;
    if (context.BufferWindowActive || context.NextSpellLethal ||
        context.NextSpellCriticalPeel) return false;
    if (context.TargetCanInstantEscape && !context.TargetRooted) return false;
    if (context.MillisecondsUntilNextReset <= 180 &&
        !context.PlayerIssuedAttack) return false;
    return true;
}

enum class WarpPurpose : std::uint8_t {
    ManualCursor,
    EmergencyEscape,
    ObjectiveTransfer,
    WaveTransfer,
};

struct WarpContext {
    bool Ready = false;
    bool HasMana = false;
    bool ManualAuthorized = false;
    bool AutomaticEmergencyOptIn = false;
    bool OriginValid = false;
    bool DestinationValid = false;
    bool DestinationNavigable = false;
    bool DestinationUnderEnemyTurret = false;
    bool DestinationHasVision = false;
    bool PlayerRootedOrGrounded = false;
    bool IncomingInterruptLikely = false;
    bool AllyChannelWouldBeBroken = false;
    bool CursorAgrees = true;
    bool AllowUnsafeManual = false;
    bool PlayerInLethalDanger = false;
    int AlliesAtDestination = 0;
    int EnemiesAtDestination = 0;
    int AlliesInPortal = 1;
    float Distance = 0.0f;
    WarpPurpose Purpose = WarpPurpose::ManualCursor;
};

inline Vec3 ClampWarpDestination(const Vec3& origin,
                                 const Vec3& requested) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    const float distance = origin.Distance2D(requested);
    if (distance + 0.01f < kRealmWarpMinimumRange) return {};
    Vec3 result = origin + direction * std::min(
        kRealmWarpMaximumRange, distance);
    result.y = requested.y;
    return result;
}

inline bool PortalContains(const Vec3& portalCenter,
                           const Vec3& unitPosition,
                           float unitRadius = 0.0f) {
    return portalCenter.IsValid() && unitPosition.IsValid() &&
        portalCenter.Distance2D(unitPosition) <=
            kRealmWarpRadius + std::max(0.0f, unitRadius);
}

inline CastEvaluation EvaluateWarp(const WarpContext& context) {
    CastEvaluation result{};
    if (!context.Ready || !context.HasMana || !context.OriginValid ||
        !context.DestinationValid) {
        result.Reason = "Realm Warp unavailable";
        return result;
    }
    const bool emergencyAuthorized = context.Purpose ==
        WarpPurpose::EmergencyEscape && context.AutomaticEmergencyOptIn &&
        context.PlayerInLethalDanger;
    if (!context.ManualAuthorized && !emergencyAuthorized) {
        result.Reason = "Realm Warp remains player-authorized";
        return result;
    }
    if (context.Distance < kRealmWarpMinimumRange - 0.01f ||
        context.Distance > kRealmWarpMaximumRange + 0.01f ||
        !context.DestinationNavigable || context.PlayerRootedOrGrounded) {
        result.Reason = "invalid Realm Warp geometry or player state";
        return result;
    }
    if (context.IncomingInterruptLikely && !context.PlayerInLethalDanger) {
        result.Reason = "portal likely cancelled by incoming crowd control";
        return result;
    }
    if (context.AllyChannelWouldBeBroken && context.AlliesInPortal > 1) {
        result.Reason = "do not abduct an allied protected channel";
        return result;
    }
    if (!context.AllowUnsafeManual &&
        (context.DestinationUnderEnemyTurret ||
         context.EnemiesAtDestination > context.AlliesAtDestination + 1 ||
         (!context.DestinationHasVision && context.EnemiesAtDestination > 0))) {
        result.Reason = "unsafe or blind Realm Warp arrival";
        return result;
    }
    if (!context.CursorAgrees && context.ManualAuthorized) {
        result.Reason = "manual endpoint no longer agrees with cursor";
        return result;
    }

    float score = context.ManualAuthorized ? 700.0f : 540.0f;
    score += static_cast<float>(context.AlliesAtDestination) * 95.0f;
    score -= static_cast<float>(context.EnemiesAtDestination) * 130.0f;
    score += context.DestinationHasVision ? 90.0f : -120.0f;
    if (context.PlayerInLethalDanger) score += 320.0f;
    if (context.DestinationUnderEnemyTurret)
        score -= context.AllowUnsafeManual ? 150.0f : 800.0f;
    switch (context.Purpose) {
    case WarpPurpose::ObjectiveTransfer: score += 130.0f; break;
    case WarpPurpose::EmergencyEscape: score += 240.0f; break;
    case WarpPurpose::WaveTransfer: score += 45.0f; break;
    default: break;
    }
    result.Cast = score >= 520.0f;
    result.Score = score;
    result.Reason = result.Cast
        ? "player-authorized safe Realm Warp"
        : "Realm Warp value below threshold";
    return result;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Ryze::Geometry
