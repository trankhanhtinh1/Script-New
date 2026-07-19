#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIAhriGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Ahri {

using namespace Geometry;
using ControllerHelpers::ChampionIs;
using ControllerHelpers::EnemySpellReady;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MissileEventIsLocal;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;

enum class Sequence : int {
    None,
    HideCharmWithRush,
    RushThenCharm,
    CharmConfirmedBurst,
    QuickQThenW,
};

enum class Posture : int {
    Neutral,
    Assassinate,
    Peel,
    Escape,
};

enum class RushPurpose : int {
    None,
    CloseForCharm,
    HideCharm,
    RedirectReturnOrb,
    Execute,
    Peel,
    Exit,
};

inline Menu* TacticsMenu = nullptr;
inline Menu* OrbMenu = nullptr;
inline Menu* CharmMenu = nullptr;
inline Menu* FoxFireMenu = nullptr;
inline Menu* RushMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline Sequence ActiveSequence = Sequence::None;
inline Posture CurrentPosture = Posture::Neutral;
inline RushPurpose LastRushPurpose = RushPurpose::None;
inline int SequenceTargetId = 0;
inline int SequenceExpireTick = 0;

inline bool QActive = false;
inline bool QReturning = false;
inline int QMissileNetworkId = 0;
inline int QCastTick = 0;
inline int QLastSeenTick = 0;
inline int QTargetId = 0;
inline Vector3 QMissilePosition = {};
inline Vector3 QCastOrigin = {};
inline Vector3 QCastEnd = {};
inline float LastTipScore = 0.0f;

inline int EMissileNetworkId = 0;
inline int ECastTick = 0;
inline int ETargetId = 0;
inline int CharmTargetId = 0;
inline int CharmExpireTick = 0;

inline bool RActive = false;
inline int RWindowExpireTick = 0;
inline int RLastCastTick = 0;
inline int RCastsInWindow = 0;
inline int RObservedAmmo = -1;
inline int RObservedMaxAmmo = -1;
inline Vector3 LastRushDestination = {};
inline Vector3 PendingHiddenRushDestination = {};
inline Vector3 LastReturnCandidate = {};
inline float LastReturnImprovement = 0.0f;

inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int WLastCastTick = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int CommittedTargetId = 0;
inline int CommittedUntilTick = 0;
inline int IncomingLineThreatUntil = 0;

inline constexpr float kQRange = 970.0f;
inline constexpr float kWAcquireRange = 700.0f;
inline constexpr float kERange = 975.0f;
inline constexpr float kRDashRange = 450.0f;
inline constexpr int kRStaticLockoutMs = 1000;
inline constexpr int kRWindowMs = 15000;

inline bool CastThrottleReady(int index, bool fastFollowup = false) {
    return ControllerHelpers::CastThrottleReady(
        index, 55, fastFollowup ? 22 : -1);
}

inline bool HasCharm(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) &&
        (SDK::HasBuffOfType(target, SDK::BuffType::Charm) ||
         target.HasBuff("AhriSeduce") || target.HasBuff("ahriseduce"));
}

inline bool TargetBlocksCharm(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) ||
           ControllerHelpers::HasSpellShieldOrImmunity(target) ||
           target.HasBuff("FioraW") ||
           target.HasBuff("VladimirSanguinePool") ||
           target.HasBuff("FizzEIcon") ||
           target.HasBuff("KayleR") ||
           target.HasBuff("kindredrnodeathbuff");
}

inline bool HasPointClickLockdownAt(const Vector3& position) {
    return Bool(RushMenu, "RespectLockdown", true) &&
           ControllerHelpers::HasReadyPointClickThreatAt(position);
}

inline SDK::PredictionOutput PredictionFrom(int index,
                                            const AIHeroClient& target,
                                            const Vector3& source) {
    if (index < 0 || index >= 4 || !Engine::RuntimeSpells[index] ||
        !Engine::ValidEnemy(target) || !source.IsValid() || source.IsZero()) {
        return {};
    }
    SDK::Spell* spell = Engine::RuntimeSpells[index];
    const Vector3 oldFrom = spell->From;
    const Vector3 oldRangeCheck = spell->RangeCheckFrom;
    spell->From = source;
    spell->RangeCheckFrom = source;
    const SDK::PredictionOutput output = spell->GetPrediction(target);
    spell->From = oldFrom;
    spell->RangeCheckFrom = oldRangeCheck;
    return output;
}

inline bool PredictionAtLeast(const SDK::PredictionOutput& prediction,
                              SDK::HitChance chance) {
    return static_cast<int>(prediction.Hitchance) >= static_cast<int>(chance);
}

inline bool SafeRushDestination(const Vector3& destination,
                                const AIHeroClient& target,
                                bool aggressive,
                                bool allowLockdown = false) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !destination.IsValid() || destination.IsZero() ||
        SDK::NavMesh::IsWall(destination) ||
        player.Position().Distance2D(destination) > kRDashRange + 35.0f) {
        return false;
    }
    if (Engine::PositionDangerScore(
            destination, target, Engine::ResolvedSpecs[3]) <= -10000.0f) {
        return false;
    }
    if (Engine::UnderEnemyTurret(destination) && aggressive &&
        !Bool(Engine::ComboMenu, "AllowTurretDive", false)) {
        return false;
    }
    if (Engine::CountEnemiesAt(destination, 650.0f) >
        Slider(RushMenu, "MaxRushEnemies", 2)) {
        return false;
    }
    if (aggressive && !allowLockdown && HasPointClickLockdownAt(destination)) {
        return false;
    }
    if (aggressive && Bool(RushMenu, "RespectCursor", true)) {
        const float currentCursor = player.Position().Distance2D(Game::CursorPos());
        const float newCursor = destination.Distance2D(Game::CursorPos());
        if (newCursor > currentCursor + 360.0f && player.HealthPercent() < 70.0f) {
            return false;
        }
    }
    return true;
}

inline int RuntimeRushCharges() {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return 0;
    // Before the first cast Ahri's recast ammo field is build-dependent and
    // can legitimately read zero.  Readiness represents the initial three-
    // cast activation; only trust ammo once the recast window is active.
    if (!RActive) {
        return Engine::RuntimeSpells[3] && Engine::RuntimeSpells[3]->IsReady()
            ? 3
            : 0;
    }
    const auto instance = player.Spellbook().GetSpell(SDK::SpellSlot::R);
    if (instance.IsValid()) {
        const int ammo = instance.Ammo();
        const int maximum = instance.MaxAmmo();
        if (maximum > 0 && maximum <= 10 && ammo >= 0 && ammo <= maximum + 3) {
            RObservedAmmo = ammo;
            RObservedMaxAmmo = maximum;
            return ammo;
        }
    }
    if (RActive) {
        if (RObservedAmmo >= 0) return std::max(0, RObservedAmmo);
        return std::max(0, 3 - RCastsInWindow);
    }
    return 0;
}

inline int ReservedRushCharges() {
    return std::clamp(Slider(RushMenu, "ReserveCharges", 1), 0, 2);
}

inline bool RushStaticReady() {
    const int now = SDK::Variables::TickCount();
    return RLastCastTick <= 0 ||
           now - RLastCastTick >= kRStaticLockoutMs - std::min(120, Game::Ping() / 2);
}

inline bool CanSpendRush(bool allowReserved, Mode mode) {
    if (!SpellEnabled(3, mode) || !CastThrottleReady(3, true) ||
        !RushStaticReady()) {
        return false;
    }
    const int charges = RuntimeRushCharges();
    return charges > 0 &&
           (allowReserved || charges > ReservedRushCharges());
}

inline bool CastRush(const Vector3& destination,
                     const AIHeroClient& target,
                     RushPurpose purpose,
                     bool aggressive,
                     bool allowReserved,
                     Mode mode,
                     bool allowLockdown = false) {
    if (!CanSpendRush(allowReserved, mode) ||
        !SafeRushDestination(destination, target, aggressive, allowLockdown)) {
        return false;
    }
    if (Engine::ControllerCastPosition(3, destination)) {
        LastRushDestination = destination;
        LastRushPurpose = purpose;
        return true;
    }
    return false;
}

inline float QOutgoingDamage(const AIBaseClient& target) {
    return Engine::RuntimeSpells[0]
        ? Engine::RuntimeSpells[0]->GetDamage(target, SDK::DamageStage::Default)
        : 0.0f;
}

inline float QReturnDamage(const AIBaseClient& target) {
    return Engine::RuntimeSpells[0]
        ? Engine::RuntimeSpells[0]->GetDamage(target, SDK::DamageStage::WayBack)
        : 0.0f;
}

inline float ComboDamage(const AIHeroClient& target, bool includeReservedRush) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    const auto player = ObjectManager::Player();
    float damage = SDK::Damage::GetAutoAttackDamage(player, target, true);
    if (QActive && QReturning) {
        damage += QReturnDamage(target);
    } else if (!QActive && Engine::RuntimeSpells[0] &&
               Engine::RuntimeSpells[0]->IsReady()) {
        damage += QOutgoingDamage(target) + QReturnDamage(target);
    }
    if (Engine::RuntimeSpells[1] && Engine::RuntimeSpells[1]->IsReady()) {
        // First flame deals full damage; the two follow-ups are reduced.  A
        // conservative 1.6 multiplier avoids spending R on optimistic W math.
        damage += Engine::RuntimeSpells[1]->GetDamage(target) * 1.60f;
    }
    if (Engine::RuntimeSpells[2] && Engine::RuntimeSpells[2]->IsReady()) {
        damage += Engine::RuntimeSpells[2]->GetDamage(target);
    }
    if (Engine::RuntimeSpells[3] && Engine::RuntimeSpells[3]->IsReady()) {
        int offensiveCharges = RuntimeRushCharges();
        if (!includeReservedRush) {
            offensiveCharges = std::max(0, offensiveCharges - ReservedRushCharges());
        }
        damage += Engine::RuntimeSpells[3]->GetDamage(target) *
                  static_cast<float>(offensiveCharges);
    }
    return damage;
}

inline bool IsIsolated(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    int defenders = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) || enemy.NetworkId() == target.NetworkId()) continue;
        if (enemy.Position().DistanceSqr2D(target.Position()) <= 700.0f * 700.0f) {
            ++defenders;
        }
    }
    return defenders == 0;
}

inline AIHeroClient FindPeelTarget() {
    AIHeroClient best = {};
    float bestScore = -FLT_MAX;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally, 1200.0f)) continue;
        const float carryValue = ally.TotalAttackDamage() + ally.AP() * 0.72f +
                                 (100.0f - ally.HealthPercent()) * 1.8f;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!Engine::ValidEnemy(enemy, 1200.0f)) continue;
            const float distance = enemy.Position().Distance2D(ally.Position());
            if (distance > 390.0f) continue;
            const float score = carryValue + (390.0f - distance) * 1.2f +
                                (enemy.IsDashing() ? 180.0f : 0.0f);
            if (score > bestScore) {
                bestScore = score;
                best = enemy;
            }
        }
    }
    return best;
}

inline Posture DeterminePosture(const AIHeroClient& selected) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return Posture::Neutral;
    if (player.HealthPercent() <=
        static_cast<float>(Slider(RushMenu, "EscapeHp", 26))) {
        return Posture::Escape;
    }
    const auto peel = FindPeelTarget();
    if (Bool(CharmMenu, "PeelCarry", true) && Engine::ValidEnemy(peel)) {
        return Posture::Peel;
    }
    if (Engine::ValidEnemy(selected) && IsIsolated(selected) &&
        ComboDamage(selected, false) >= selected.Health() + selected.AllShield()) {
        return Posture::Assassinate;
    }
    return Posture::Neutral;
}

inline bool IsQMissileName(const char* spellName, const char* missileName) {
    return ControllerHelpers::TextContainsAny(missileName, {
               "AhriOrbMissile", "AhriQMissile", "AhriOrbReturn",
               "AhriQReturnMissile",
           }) ||
           ControllerHelpers::TextContainsAny(spellName, {
               "AhriOrbofDeception", "AhriQReturn",
           });
}

inline bool IsQReturnName(const char* spellName, const char* missileName) {
    return ControllerHelpers::TextContainsAny(
               missileName, { "return" }) ||
           ControllerHelpers::TextContainsAny(
               spellName, { "return", "OrbofDeception2" });
}

inline bool IsCharmMissileName(const char* spellName, const char* missileName) {
    return ControllerHelpers::TextContainsAny(
               missileName, { "AhriSeduce", "AhriEMissile" }) ||
           ControllerHelpers::TextContainsAny(
               spellName, { "AhriSeduce", "AhriE" });
}

inline void RefreshTrackedMissiles() {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return;
    bool foundQ = false;
    bool foundE = false;
    for (const auto& missile : GameObjects::Missiles()) {
        if (!missile.IsValid() || missile.CasterNetworkId() != player.NetworkId()) continue;
        const std::string spellName = missile.SpellName();
        const std::string missileName = missile.MissileName();
        if (IsQMissileName(spellName.c_str(), missileName.c_str())) {
            foundQ = true;
            QActive = true;
            QReturning = IsQReturnName(spellName.c_str(), missileName.c_str());
            QMissileNetworkId = missile.NetworkId();
            QMissilePosition = missile.Position();
            QLastSeenTick = SDK::Variables::TickCount();
        } else if (IsCharmMissileName(spellName.c_str(), missileName.c_str())) {
            foundE = true;
            EMissileNetworkId = missile.NetworkId();
        }
    }
    const int now = SDK::Variables::TickCount();
    if (!foundQ && QActive && QLastSeenTick > 0 && now - QLastSeenTick > 180 &&
        QCastTick > 0 && now - QCastTick > 500) {
        QActive = false;
        QReturning = false;
        QMissileNetworkId = 0;
        QMissilePosition = {};
    }
    if (!foundE && EMissileNetworkId != 0 && now - ECastTick > 850) {
        EMissileNetworkId = 0;
    }
}

inline int CharmWindowQuality(const AIHeroClient& target,
                              const SDK::PredictionOutput& prediction,
                              bool afterRush,
                              bool reactive) {
    const int now = SDK::Variables::TickCount();
    if (Engine::IsHardCrowdControlled(target)) return 6;
    if (target.IsDashing()) return reactive ? 6 : 5;
    if (static_cast<int>(target.NetworkId()) == CommittedTargetId &&
        now <= CommittedUntilTick) return 5;
    if (afterRush && ObjectManager::Player().Position().Distance2D(target.Position()) <=
                         static_cast<float>(Slider(CharmMenu, "RushCharmRange", 540))) {
        return 5;
    }
    if (prediction.Hitchance == SDK::HitChance::Immobile ||
        prediction.Hitchance == SDK::HitChance::Dash) return 6;
    if (PredictionAtLeast(prediction, SDK::HitChance::VeryHigh)) return 4;
    if (PredictionAtLeast(prediction, SDK::HitChance::High)) return 3;
    if (PredictionAtLeast(prediction, SDK::HitChance::Medium)) return 2;
    return 0;
}

inline bool CastCharm(const AIHeroClient& target,
                      Mode mode,
                      bool reactive,
                      bool afterRush,
                      bool planHiddenRush = false) {
    if (!Engine::ValidEnemy(target, kERange + 30.0f) ||
        !SpellEnabled(2, mode) || !CastThrottleReady(2, reactive) ||
        TargetBlocksCharm(target) ||
        (Orbwalker::IsWindingUp() &&
         Bool(Engine::HumanMenu, "PreserveAttacks", true) && !reactive)) {
        return false;
    }
    const auto player = ObjectManager::Player();
    const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    if (!prediction.CollisionObjects.empty()) return false;
    const int quality = CharmWindowQuality(target, prediction, afterRush, reactive);
    const int required = reactive
        ? Slider(CharmMenu, "ReactiveQuality", 2)
        : Slider(CharmMenu, "MinimumQuality", 4);
    const float distance = player.Position().Distance2D(prediction.GetUnitPosition());
    if (quality < required ||
        (!afterRush && !reactive && quality < 5 &&
         distance > static_cast<float>(Slider(CharmMenu, "RawMaxRange", 825)))) {
        return false;
    }
    ETargetId = static_cast<int>(target.NetworkId());
    if (Engine::ControllerCastPosition(2, prediction.GetCastPosition())) {
        ECastTick = SDK::Variables::TickCount();
        if (planHiddenRush) {
            ActiveSequence = Sequence::HideCharmWithRush;
            SequenceTargetId = ETargetId;
            SequenceExpireTick = ECastTick + 420;
        }
        return true;
    }
    return false;
}

inline bool CastQ(const AIHeroClient& target,
                  Mode mode,
                  bool guaranteed,
                  bool killSecure = false) {
    if (QActive || !Engine::ValidEnemy(target, kQRange + 35.0f) ||
        !SpellEnabled(0, mode) || !CastThrottleReady(0) ||
        (Orbwalker::IsWindingUp() &&
         Bool(Engine::HumanMenu, "PreserveAttacks", true) && !guaranteed)) {
        return false;
    }
    const auto player = ObjectManager::Player();
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const SDK::HitChance needed = guaranteed
        ? SDK::HitChance::Medium
        : (killSecure ? SDK::HitChance::VeryHigh : SDK::HitChance::High);
    if (!PredictionAtLeast(prediction, needed)) return false;
    Vector3 castPosition = prediction.GetCastPosition();
    if (!castPosition.IsValid() || castPosition.IsZero()) return false;
    const Vector3 predicted = prediction.GetUnitPosition();
    LastTipScore = TipDoubleHitScore(
        player.Position(), castPosition, predicted, target.BoundingRadius());
    if (Bool(OrbMenu, "TipDoubleHit", true) && LastTipScore >=
        static_cast<float>(Slider(OrbMenu, "TipConfidence", 62)) / 100.0f) {
        // Keep the cast collinear and at full travel length.  The target is at
        // the turnaround zone, causing outbound and return hits to nearly stack.
        const Vector3 direction = Direction2D(player.Position(), predicted);
        if (!direction.IsZero()) castPosition = player.Position() + direction * kQRange;
    }
    QTargetId = static_cast<int>(target.NetworkId());
    QCastOrigin = player.Position();
    QCastEnd = castPosition;
    if (Engine::ControllerCastPosition(0, castPosition)) {
        QCastTick = SDK::Variables::TickCount();
        QActive = true;
        QReturning = false;
        QLastSeenTick = QCastTick;
        return true;
    }
    return false;
}

inline bool WTargetIsPrioritized(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kWAcquireRange + 25.0f)) return false;
    const int now = SDK::Variables::TickCount();
    if (HasCharm(target) ||
        (LastAutoTargetId == static_cast<int>(target.NetworkId()) &&
         now - LastAutoTick <= 3000) ||
        (QTargetId == static_cast<int>(target.NetworkId()) &&
         now - QCastTick <= 950) ||
        ObjectManager::Player().Position().Distance2D(target.Position()) <= 360.0f) {
        return true;
    }
    return Engine::RuntimeSpells[1] &&
           Engine::RuntimeSpells[1]->GetDamage(target) * 1.60f >=
               target.Health() + target.AllShield();
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool movementOnly = false) {
    if (!SpellEnabled(1, mode) || !CastThrottleReady(1) ||
        (Orbwalker::IsWindingUp() &&
         Bool(Engine::HumanMenu, "PreserveAttacks", true))) {
        return false;
    }
    if (!movementOnly && !WTargetIsPrioritized(target)) return false;
    if (Engine::ControllerCastSelf(1)) {
        WLastCastTick = SDK::Variables::TickCount();
        return true;
    }
    return false;
}

inline void AddRushCandidate(std::array<Vector3, 64>& candidates,
                             std::size_t& count,
                             const Vector3& value) {
    if (count < candidates.size() && value.IsValid() && !value.IsZero()) {
        candidates[count++] = value;
    }
}

inline std::size_t BuildRushCandidates(std::array<Vector3, 64>& candidates,
                                       const AIHeroClient& target) {
    const auto player = ObjectManager::Player();
    std::size_t count = 0;
    AddRushCandidate(candidates, count,
        Engine::Extend(player.Position(), Game::CursorPos(), kRDashRange));
    if (Engine::ValidEnemy(target)) {
        const Vector3 predicted = PredictPosition(target, 0.30f);
        const Vector3 toward = Direction2D(player.Position(), predicted);
        if (!toward.IsZero()) {
            const Vector3 side{ -toward.z, 0.0f, toward.x };
            AddRushCandidate(candidates, count, player.Position() + toward * kRDashRange);
            AddRushCandidate(candidates, count, player.Position() + toward * 285.0f);
            AddRushCandidate(candidates, count,
                player.Position() + toward * 315.0f + side * 300.0f);
            AddRushCandidate(candidates, count,
                player.Position() + toward * 315.0f - side * 300.0f);
            AddRushCandidate(candidates, count, player.Position() - toward * kRDashRange);
        }
    }
    constexpr float pi = 3.14159265358979323846f;
    for (int ring = 0; ring < 2; ++ring) {
        const float radius = ring == 0 ? kRDashRange : 270.0f;
        for (int i = 0; i < 24; ++i) {
            const float angle = 2.0f * pi * static_cast<float>(i) / 24.0f;
            AddRushCandidate(candidates, count, {
                player.Position().x + std::cos(angle) * radius,
                player.Position().y,
                player.Position().z + std::sin(angle) * radius
            });
        }
    }
    return count;
}

inline bool FindCharmRushDestination(const AIHeroClient& target,
                                     Vector3& result,
                                     float& resultScore,
                                     bool allowLockdown) {
    if (!Engine::ValidEnemy(target)) return false;
    std::array<Vector3, 64> candidates = {};
    const std::size_t count = BuildRushCandidates(candidates, target);
    const auto player = ObjectManager::Player();
    result = {};
    resultScore = -FLT_MAX;
    for (std::size_t i = 0; i < count; ++i) {
        const Vector3& candidate = candidates[i];
        if (!SafeRushDestination(candidate, target, true, allowLockdown) ||
            candidate.Distance2D(target.Position()) > kERange) continue;
        const auto prediction = PredictionFrom(2, target, candidate);
        if (!prediction.CollisionObjects.empty() ||
            !PredictionAtLeast(prediction, SDK::HitChance::High)) continue;
        const float distance = candidate.Distance2D(prediction.GetUnitPosition());
        const float desired = static_cast<float>(Slider(CharmMenu, "RushCharmRange", 540));
        float score = static_cast<float>(static_cast<int>(prediction.Hitchance)) * 260.0f;
        score -= std::fabs(distance - desired) * 0.58f;
        score += Engine::PositionDangerScore(candidate, target, Engine::ResolvedSpecs[3]) * 0.20f;
        score -= candidate.Distance2D(Game::CursorPos()) * 0.08f;
        if (candidate.Distance2D(target.Position()) <= 600.0f) score += 90.0f;
        if (score > resultScore) {
            resultScore = score;
            result = candidate;
        }
    }
    return result.IsValid() && !result.IsZero();
}

inline bool FindReturnRushDestination(const AIHeroClient& target,
                                      Vector3& result,
                                      float& improvement) {
    const auto player = ObjectManager::Player();
    if (!QActive || !QReturning || !QMissilePosition.IsValid() ||
        QMissilePosition.IsZero() || !Engine::ValidEnemy(target)) {
        return false;
    }
    const Vector3 initialPrediction = PredictPosition(target,
        ReturnTravelSecondsToTarget(
            QMissilePosition, player.Position(), target.Position()));
    const float currentScore = ReturnHitScore(
        QMissilePosition, player.Position(), initialPrediction,
        target.BoundingRadius());
    if (currentScore >= static_cast<float>(Slider(OrbMenu, "KeepCurrentHit", 58)) /
                            100.0f) {
        return false;
    }
    std::array<Vector3, 64> candidates = {};
    const std::size_t count = BuildRushCandidates(candidates, target);
    float bestScore = currentScore;
    Vector3 best = {};
    for (std::size_t i = 0; i < count; ++i) {
        const Vector3& candidate = candidates[i];
        if (!SafeRushDestination(candidate, target, true, false)) continue;
        float travel = ReturnTravelSecondsToTarget(
            QMissilePosition, candidate, target.Position());
        Vector3 predicted = PredictPosition(target, travel);
        // One refinement accounts for the intersection moving with prediction.
        travel = ReturnTravelSecondsToTarget(QMissilePosition, candidate, predicted);
        predicted = PredictPosition(target, travel);
        const float hit = ReturnHitScore(
            QMissilePosition, candidate, predicted, target.BoundingRadius());
        if (hit <= 0.0f) continue;
        float score = hit;
        score += std::clamp(
            Engine::PositionDangerScore(candidate, target, Engine::ResolvedSpecs[3]) /
                5000.0f,
            -0.25f, 0.18f);
        score -= candidate.Distance2D(Game::CursorPos()) / 9000.0f;
        if (candidate.Distance2D(target.Position()) <= 650.0f) score += 0.04f;
        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }
    improvement = bestScore - currentScore;
    result = best;
    return result.IsValid() && !result.IsZero() &&
           bestScore >= static_cast<float>(Slider(OrbMenu, "RedirectHit", 55)) /
                            100.0f &&
           improvement >= static_cast<float>(Slider(OrbMenu, "RedirectGain", 22)) /
                              100.0f;
}

inline bool TryReturnRedirect(const AIHeroClient& fallback, Mode mode) {
    if (!Bool(OrbMenu, "RushRedirect", true) || !QActive || !QReturning ||
        !RActive || QTargetId == 0 || mode == Mode::LaneClear ||
        mode == Mode::LastHit || ActiveSequence == Sequence::HideCharmWithRush) {
        return false;
    }
    AIHeroClient target = Engine::EnemyByNetworkId(QTargetId);
    if (!Engine::ValidEnemy(target)) target = fallback;
    if (!Engine::ValidEnemy(target)) return false;
    const bool lethalReturn = QReturnDamage(target) +
        (Engine::RuntimeSpells[3] ? Engine::RuntimeSpells[3]->GetDamage(target) : 0.0f) >=
        target.Health() + target.AllShield();
    if (!CanSpendRush(lethalReturn, mode == Mode::None ? Mode::Automatic : mode)) {
        return false;
    }
    Vector3 destination = {};
    float improvement = 0.0f;
    if (!FindReturnRushDestination(target, destination, improvement)) return false;
    LastReturnCandidate = destination;
    LastReturnImprovement = improvement;
    return CastRush(destination, target, RushPurpose::RedirectReturnOrb,
                    true, lethalReturn,
                    mode == Mode::None ? Mode::Automatic : mode);
}

inline Vector3 BestEscapeRushPosition(const AIHeroClient& threat) {
    const auto player = ObjectManager::Player();
    std::array<Vector3, 64> candidates = {};
    const std::size_t count = BuildRushCandidates(candidates, threat);
    Vector3 best = {};
    float bestScore = -FLT_MAX;
    for (std::size_t i = 0; i < count; ++i) {
        const Vector3& candidate = candidates[i];
        if (!SafeRushDestination(candidate, threat, false, true)) continue;
        float nearest = 1400.0f;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (Engine::ValidEnemy(enemy)) {
                nearest = std::min(nearest, candidate.Distance2D(enemy.Position()));
            }
        }
        float score = nearest * 0.75f - candidate.Distance2D(Game::CursorPos()) * 0.35f;
        score += Engine::PositionDangerScore(candidate, threat, Engine::ResolvedSpecs[3]);
        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }
    return best;
}

inline bool TryRushClose(const AIHeroClient& target) {
    if (!Bool(RushMenu, "UseToGuaranteeCharm", true) ||
        !Engine::ValidEnemy(target, kERange + kRDashRange + 60.0f) ||
        !CanSpendRush(false, Mode::Combo) || TargetBlocksCharm(target)) {
        return false;
    }
    const float shielded = target.Health() + target.AllShield();
    const bool lethal = ComboDamage(target, false) >= shielded;
    const bool commit = lethal ||
        (IsIsolated(target) && target.HealthPercent() <=
             static_cast<float>(Slider(RushMenu, "IsolatedTargetHp", 72))) ||
        CurrentPosture == Posture::Assassinate;
    if (!commit) return false;

    Vector3 destination = {};
    float score = 0.0f;
    const bool allowLockdown = lethal &&
        Bool(RushMenu, "OverrideLockdownForLethal", false);
    if (!FindCharmRushDestination(target, destination, score, allowLockdown)) {
        return false;
    }

    const auto currentPrediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    const bool canHide = Bool(CharmMenu, "HideEWithR", true) &&
        ObjectManager::Player().Position().Distance2D(target.Position()) <= kERange &&
        currentPrediction.CollisionObjects.empty() &&
        PredictionAtLeast(currentPrediction, SDK::HitChance::High);
    if (canHide) {
        PendingHiddenRushDestination = destination;
        SequenceTargetId = static_cast<int>(target.NetworkId());
        if (CastCharm(target, Mode::Combo, false, false, true)) return true;
        PendingHiddenRushDestination = {};
    }

    if (CastRush(destination, target, RushPurpose::CloseForCharm,
                 true, false, Mode::Combo, allowLockdown)) {
        ActiveSequence = Sequence::RushThenCharm;
        SequenceTargetId = static_cast<int>(target.NetworkId());
        SequenceExpireTick = SDK::Variables::TickCount() + 780;
        return true;
    }
    return false;
}

inline bool TryRushExecute(const AIHeroClient& target) {
    if (!Bool(RushMenu, "Execute", true) || !Engine::ValidEnemy(target) ||
        !Engine::RuntimeSpells[3]) return false;
    const float damage = Engine::RuntimeSpells[3]->GetDamage(target);
    if (damage * 0.94f < target.Health() + target.AllShield()) return false;
    Vector3 destination = {};
    float score = 0.0f;
    if (!FindCharmRushDestination(target, destination, score, true)) {
        destination = Engine::Extend(
            ObjectManager::Player().Position(), target.Position(), kRDashRange);
    }
    if (destination.Distance2D(target.Position()) > 600.0f) return false;
    return CastRush(destination, target, RushPurpose::Execute,
                    true, true, Mode::Combo, true);
}

inline bool TrySequence(const AIHeroClient& fallback, Mode mode) {
    const int now = SDK::Variables::TickCount();
    if (ActiveSequence == Sequence::None) return false;
    if (now > SequenceExpireTick) {
        ActiveSequence = Sequence::None;
        PendingHiddenRushDestination = {};
        return false;
    }
    AIHeroClient target = Engine::EnemyByNetworkId(SequenceTargetId);
    if (!Engine::ValidEnemy(target)) target = fallback;

    if (ActiveSequence == Sequence::HideCharmWithRush) {
        if (!Engine::ValidEnemy(target) || PendingHiddenRushDestination.IsZero()) {
            ActiveSequence = Sequence::None;
            return false;
        }
        if (now - ECastTick < 25) return true;
        if (now - ECastTick > 300) {
            ActiveSequence = Sequence::None;
            PendingHiddenRushDestination = {};
            return false;
        }
        if (CastRush(PendingHiddenRushDestination, target,
                     RushPurpose::HideCharm, true, false, Mode::Combo)) {
            PendingHiddenRushDestination = {};
            ActiveSequence = Sequence::None;
        }
        return true;
    }

    if (ActiveSequence == Sequence::RushThenCharm) {
        const auto player = ObjectManager::Player();
        if (!Engine::ValidEnemy(target)) {
            ActiveSequence = Sequence::None;
            return false;
        }
        if (player.IsDashing() || now - RLastCastTick < 70) return true;
        if (CastCharm(target, Mode::Combo, true, true, false)) {
            ActiveSequence = Sequence::None;
        }
        return true;
    }

    if (ActiveSequence == Sequence::CharmConfirmedBurst) {
        if (!Engine::ValidEnemy(target)) {
            ActiveSequence = Sequence::None;
            return false;
        }
        if (CastQ(target, mode == Mode::Harass ? Mode::Harass : Mode::Combo, true)) {
            return true;
        }
        if (CastW(target, mode == Mode::Harass ? Mode::Harass : Mode::Combo)) {
            ActiveSequence = Sequence::None;
            return true;
        }
        if (!HasCharm(target) || now + 120 >= CharmExpireTick) {
            ActiveSequence = Sequence::None;
        }
        return true;
    }

    if (ActiveSequence == Sequence::QuickQThenW) {
        if (Engine::ValidEnemy(target) && now - QCastTick >= 80 &&
            CastW(target, mode)) {
            ActiveSequence = Sequence::None;
            return true;
        }
        return true;
    }
    return false;
}

inline bool TryInterrupt() {
    const int now = SDK::Variables::TickCount();
    if (!Bool(Engine::AutomaticMenu, "Interrupt", true) ||
        InterruptTargetId == 0 || now > InterruptExpireTick) return false;
    const auto target = Engine::EnemyByNetworkId(InterruptTargetId);
    if (!Engine::ValidEnemy(target, kERange + 20.0f)) return false;
    if (CastCharm(target, Mode::Automatic, true, false)) {
        InterruptTargetId = 0;
        return true;
    }
    return false;
}

inline bool TryAntiGapcloser() {
    const int now = SDK::Variables::TickCount();
    if (!Bool(Engine::AutomaticMenu, "AntiGapcloser", true) ||
        GapcloserTargetId == 0 || now > GapcloserExpireTick) return false;
    const auto target = Engine::EnemyByNetworkId(GapcloserTargetId);
    if (!Engine::ValidEnemy(target, kERange + 40.0f)) return false;
    if (CastCharm(target, Mode::Automatic, true, false)) {
        GapcloserTargetId = 0;
        return true;
    }
    return false;
}

inline bool TryIncomingThreatW(Mode mode) {
    const int now = SDK::Variables::TickCount();
    if (!Bool(FoxFireMenu, "DodgeWithW", true) ||
        now > IncomingLineThreatUntil ||
        (mode != Mode::Combo && mode != Mode::Harass && mode != Mode::Flee)) {
        return false;
    }
    return CastW({}, mode, true);
}

inline bool TryKillSecure() {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kERange + kRDashRange)) continue;
        const float health = enemy.Health() + enemy.AllShield();
        if (Engine::RuntimeSpells[2] && Engine::RuntimeSpells[2]->IsReady() &&
            Engine::RuntimeSpells[2]->GetDamage(enemy) * 0.95f >= health &&
            CastCharm(enemy, Mode::Automatic, false, false)) return true;
        if (!QActive && Engine::RuntimeSpells[0] &&
            Engine::RuntimeSpells[0]->IsReady() &&
            (QOutgoingDamage(enemy) + QReturnDamage(enemy)) * 0.94f >= health &&
            CastQ(enemy, Mode::Automatic, Engine::IsHardCrowdControlled(enemy), true)) {
            return true;
        }
        if (Engine::RuntimeSpells[1] && Engine::RuntimeSpells[1]->IsReady() &&
            WTargetIsPrioritized(enemy) &&
            Engine::RuntimeSpells[1]->GetDamage(enemy) * 1.55f >= health &&
            CastW(enemy, Mode::Automatic)) return true;
        if (TryRushExecute(enemy)) return true;
    }
    return false;
}

inline bool TryPeel(const AIHeroClient& peelTarget, Mode mode) {
    if (!Engine::ValidEnemy(peelTarget)) return false;
    if (CastCharm(peelTarget,
                  mode == Mode::None ? Mode::Automatic : mode,
                  true, false)) return true;
    if (CastW(peelTarget,
              mode == Mode::None ? Mode::Automatic : mode)) return true;
    if (CastQ(peelTarget,
              mode == Mode::None ? Mode::Automatic : mode,
              peelTarget.IsDashing() || Engine::IsHardCrowdControlled(peelTarget))) {
        return true;
    }
    return false;
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = ObjectManager::Player();
    const int now = SDK::Variables::TickCount();
    if (HasCharm(target)) {
        if (CastQ(target, Mode::Harass, true)) return true;
        if (CastW(target, Mode::Harass)) return true;
    }
    if (Bool(FoxFireMenu, "AAWAA", true) &&
        LastAutoTargetId == static_cast<int>(target.NetworkId()) &&
        now - LastAutoTick <= 340 && CastW(target, Mode::Harass)) {
        return true;
    }
    if (Bool(CharmMenu, "HarassOnlyGuaranteed", true) &&
        (Engine::IsHardCrowdControlled(target) || target.IsDashing() ||
         (CommittedTargetId == static_cast<int>(target.NetworkId()) &&
          now <= CommittedUntilTick)) &&
        CastCharm(target, Mode::Harass, false, false)) {
        return true;
    }
    if (CastQ(target, Mode::Harass, false)) {
        if (Bool(FoxFireMenu, "QWTrade", true)) {
            ActiveSequence = Sequence::QuickQThenW;
            SequenceTargetId = static_cast<int>(target.NetworkId());
            SequenceExpireTick = now + 520;
        }
        return true;
    }
    if (player.Position().Distance2D(target.Position()) <= 420.0f &&
        WTargetIsPrioritized(target) && CastW(target, Mode::Harass)) return true;
    return false;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (HasCharm(target)) {
        if (CastQ(target, Mode::Combo, true)) return true;
        if (CastW(target, Mode::Combo)) return true;
    }
    if (TryRushClose(target)) return true;
    if (CastCharm(target, Mode::Combo, false, false)) return true;
    if (CastQ(target, Mode::Combo, false)) return true;
    if (CastW(target, Mode::Combo)) return true;
    if (TryRushExecute(target)) return true;
    return false;
}

inline bool TryFlee(const AIHeroClient& selected) {
    AIHeroClient pursuer = selected;
    if (!Engine::ValidEnemy(pursuer, 1100.0f)) {
        float closest = FLT_MAX;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!Engine::ValidEnemy(enemy, 1100.0f)) continue;
            const float distance = ObjectManager::Player().Position().Distance2D(enemy.Position());
            if (distance < closest) {
                closest = distance;
                pursuer = enemy;
            }
        }
    }
    if (Engine::ValidEnemy(pursuer, kERange) &&
        CastCharm(pursuer, Mode::Flee, true, false)) return true;
    if (CastW(pursuer, Mode::Flee, true)) return true;
    if (Bool(RushMenu, "RushFlee", true) && CanSpendRush(true, Mode::Flee)) {
        const Vector3 destination = BestEscapeRushPosition(pursuer);
        if (CastRush(destination, pursuer, RushPurpose::Exit,
                     false, true, Mode::Flee, true)) return true;
    }
    return false;
}

inline bool TryExpiringWindowExit(const AIHeroClient& threat, Mode mode) {
    const int now = SDK::Variables::TickCount();
    if (!RActive || RWindowExpireTick <= 0 ||
        RWindowExpireTick - now > Slider(RushMenu, "ExitBeforeExpiryMs", 1350) ||
        RuntimeRushCharges() <= 0 || Engine::CountEnemiesAt(
            ObjectManager::Player().Position(), 800.0f) == 0) {
        return false;
    }
    const Vector3 destination = BestEscapeRushPosition(threat);
    return CastRush(destination, threat, RushPurpose::Exit,
                    false, true,
                    mode == Mode::None ? Mode::Automatic : mode, true);
}

struct FarmLine {
    Vector3 End = {};
    int Hits = 0;
    int DoubleHits = 0;
    int LastHits = 0;
    float Score = -FLT_MAX;
};

inline FarmLine BestQFarmLine(const std::vector<AIBaseClient>& units,
                              bool lastHitOnly) {
    FarmLine best{};
    const auto player = ObjectManager::Player();
    for (const auto& candidate : units) {
        if (!candidate.IsValid() || candidate.IsDead()) continue;
        const Vector3 candidatePrediction = PredictPosition(candidate, 0.25f);
        const Vector3 direction = Direction2D(player.Position(), candidatePrediction);
        if (direction.IsZero()) continue;
        const Vector3 end = player.Position() + direction * kQRange;
        int hits = 0;
        int doubles = 0;
        int kills = 0;
        for (const auto& unit : units) {
            if (!unit.IsValid() || unit.IsDead()) continue;
            const float outboundSeconds = 0.25f +
                player.Position().Distance2D(unit.Position()) / 1550.0f;
            const Vector3 predicted = PredictPosition(unit, outboundSeconds);
            const auto projection = ProjectPointToSegment2D(
                predicted, player.Position(), end);
            const float hitRadius = 100.0f + unit.BoundingRadius();
            if (projection.Distance > hitRadius) continue;
            ++hits;
            ++doubles; // stationary Ahri makes the return traverse this line again.
            if (lastHitOnly) {
                const int travelMs = static_cast<int>(outboundSeconds * 1000.0f);
                const float outboundHealth = SDK::HealthPrediction::GetPrediction(unit, travelMs);
                const float returnHealth = SDK::HealthPrediction::GetPrediction(unit, travelMs + 430);
                const float outgoing = QOutgoingDamage(unit);
                const float returning = QReturnDamage(unit);
                if ((outboundHealth > 0.0f && outgoing >= outboundHealth) ||
                    (returnHealth > 0.0f && outgoing + returning >= returnHealth)) {
                    ++kills;
                }
            }
        }
        const float score = static_cast<float>(hits) +
                            static_cast<float>(doubles) * 0.85f +
                            static_cast<float>(kills) * 4.5f;
        if (score > best.Score) {
            best.End = end;
            best.Hits = hits;
            best.DoubleHits = doubles;
            best.LastHits = kills;
            best.Score = score;
        }
    }
    return best;
}

inline bool TryWFarm(const std::vector<AIBaseClient>& units,
                     bool jungle,
                     bool lastHitOnly) {
    if (!Bool(FarmMenu, jungle ? "JungleW" : "LaneW", jungle) ||
        !SpellEnabled(1, lastHitOnly ? Mode::LastHit : Mode::LaneClear) ||
        !CastThrottleReady(1) ||
        Engine::CountEnemiesAt(ObjectManager::Player().Position(), 900.0f) > 0) {
        return false;
    }
    int killable = 0;
    for (const auto& unit : units) {
        if (!unit.IsValid() || unit.IsDead() ||
            ObjectManager::Player().Position().Distance2D(unit.Position()) > kWAcquireRange) {
            continue;
        }
        if (jungle) {
            ++killable;
        } else {
            const float damage = Engine::RuntimeSpells[1]->GetDamage(unit);
            const float health = SDK::HealthPrediction::GetPrediction(unit, 250);
            if (health > 0.0f && damage >= health) ++killable;
        }
    }
    const int required = jungle ? 1 :
        (lastHitOnly ? 1 : Slider(FarmMenu, "WLastHits", 2));
    return killable >= required && Engine::ControllerCastSelf(1);
}

inline bool TryFarm(bool lastHitOnly) {
    auto lane = Engine::ClearUnits(false);
    auto jungle = Engine::ClearUnits(true);
    const bool useJungle = lane.empty() && !jungle.empty();
    auto& units = useJungle ? jungle : lane;
    if (units.empty()) return false;
    const Mode mode = lastHitOnly ? Mode::LastHit : Mode::LaneClear;
    if (!QActive && SpellEnabled(0, mode) && CastThrottleReady(0)) {
        const FarmLine best = BestQFarmLine(units, lastHitOnly);
        const int minimum = useJungle ? 1 :
            (lastHitOnly ? 1 : Slider(FarmMenu, "QMinions", 3));
        if (best.Hits >= minimum && (!lastHitOnly || best.LastHits > 0) &&
            Engine::ControllerCastPosition(0, best.End)) {
            QTargetId = 0;
            QCastTick = SDK::Variables::TickCount();
            QActive = true;
            QReturning = false;
            QCastOrigin = ObjectManager::Player().Position();
            QCastEnd = best.End;
            return true;
        }
    }
    return TryWFarm(units, useJungle, lastHitOnly);
}

inline void RefreshState() {
    const auto player = ObjectManager::Player();
    const int now = SDK::Variables::TickCount();
    RefreshTrackedMissiles();
    const bool runtimeR = player.IsValid() &&
        (player.HasBuff("AhriTumble") ||
         Engine::TextContains(Engine::RuntimeSpellNames[3].c_str(), "AhriTumble"));
    RActive = RActive || runtimeR;
    if (RActive && RWindowExpireTick <= 0) {
        RWindowExpireTick = std::max(now + 1000, RLastCastTick + kRWindowMs);
    }
    if (RActive && !runtimeR && RWindowExpireTick > 0 && now > RWindowExpireTick) {
        RActive = false;
        RCastsInWindow = 0;
        RObservedAmmo = -1;
        RObservedMaxAmmo = -1;
    }
    (void)RuntimeRushCharges();
    if (CharmTargetId != 0 && now > CharmExpireTick) CharmTargetId = 0;
    if (CommittedTargetId != 0 && now > CommittedUntilTick) CommittedTargetId = 0;
    if (GapcloserTargetId != 0 && now > GapcloserExpireTick) GapcloserTargetId = 0;
    if (InterruptTargetId != 0 && now > InterruptExpireTick) InterruptTargetId = 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    RefreshState();
    CurrentPosture = DeterminePosture(selected);

    if (TrySequence(selected, mode)) return true;
    if (TryReturnRedirect(selected, mode)) return true;
    if (TryInterrupt() || TryAntiGapcloser() || TryIncomingThreatW(mode) ||
        TryKillSecure()) return true;

    if (mode == Mode::Flee) {
        (void)TryFlee(selected);
        return true;
    }

    if (CurrentPosture == Posture::Peel &&
        Bool(CharmMenu, "PeelCarry", true)) {
        const auto peel = FindPeelTarget();
        if (TryPeel(peel, mode)) return true;
    }

    // Run this before the combat-mode returns.  It is intentionally below
    // return-Q redirection and reactive peel, but above a fresh offensive
    // sequence so the reserved charge actually remains an exit resource.
    if ((mode == Mode::Combo || mode == Mode::Harass || mode == Mode::None) &&
        TryExpiringWindowExit(selected, mode)) {
        return true;
    }

    if (mode == Mode::Combo) {
        (void)TryCombo(selected);
        return true;
    }
    if (mode == Mode::Harass) {
        (void)TryHarass(selected);
        return true;
    }
    if (mode == Mode::LaneClear) {
        (void)TryFarm(false);
        return true;
    }
    if (mode == Mode::LastHit) {
        (void)TryFarm(true);
        return true;
    }
    if (Key(Engine::AutomaticMenu, "ManualR", false) &&
        Engine::ValidEnemy(selected)) {
        Vector3 destination = Engine::Extend(
            ObjectManager::Player().Position(), Game::CursorPos(), kRDashRange);
        (void)CastRush(destination, selected, RushPurpose::None,
                       false, true, Mode::Automatic, true);
    }
    (void)TryExpiringWindowExit(selected, mode);
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !args.Sender.IsValid()) return;
    const int now = SDK::Variables::TickCount();

    if (!IsLocalPlayer(args.Sender)) {
        const auto threat = ControllerHelpers::AnalyzeEnemyCast(
            args, 250.0f, 105.0f, 260, 250, 240, 1400, 380);
        if (!threat.Valid) return;
        if (threat.Committed) {
            CommittedTargetId = static_cast<int>(threat.Enemy.NetworkId());
            CommittedUntilTick = threat.CommitmentUntilTick;
        }
        if (threat.CrossesPlayer) {
            IncomingLineThreatUntil = threat.LineThreatUntilTick;
        }
        return;
    }

    if (args.Slot == 0) {
        QCastTick = now;
        QActive = true;
        QReturning = false;
        QLastSeenTick = now;
        QCastOrigin = args.StartPosition.IsValid() && !args.StartPosition.IsZero()
            ? args.StartPosition
            : player.Position();
        QCastEnd = args.CastPosition.IsValid() && !args.CastPosition.IsZero()
            ? args.CastPosition
            : args.EndPosition;
        const Mode observedMode = Engine::CurrentMode();
        if (QTargetId == 0 && observedMode != Mode::LaneClear &&
            observedMode != Mode::LastHit) {
            const auto target = Engine::SelectTarget(kQRange + 50.0f);
            if (Engine::ValidEnemy(target)) QTargetId = static_cast<int>(target.NetworkId());
        }
    } else if (args.Slot == 1) {
        WLastCastTick = now;
    } else if (args.Slot == 2) {
        ECastTick = now;
        if (ETargetId == 0) {
            const auto target = Engine::SelectTarget(kERange + 50.0f);
            if (Engine::ValidEnemy(target)) ETargetId = static_cast<int>(target.NetworkId());
        }
    } else if (args.Slot == 3) {
        const bool wasActive = RActive;
        RActive = true;
        RLastCastTick = now;
        if (!wasActive) {
            RCastsInWindow = 1;
            RWindowExpireTick = now + kRWindowMs;
        } else {
            ++RCastsInWindow;
        }
        if (RObservedAmmo > 0) --RObservedAmmo;
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int now = SDK::Variables::TickCount();
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "AhriTumble")) {
        RActive = true;
        if (args.EndTime > Game::Time()) {
            RWindowExpireTick = now +
                ControllerHelpers::RemainingMilliseconds(
                    args.EndTime, kRWindowMs, 500, 20000);
        } else if (RWindowExpireTick <= 0) {
            RWindowExpireTick = now + kRWindowMs;
        }
        return;
    }
    const auto enemy = Engine::EnemyByNetworkId(static_cast<int>(args.Sender.NetworkId));
    const bool isCharm = args.Type == static_cast<int>(SDK::BuffType::Charm) ||
                         Engine::TextContains(args.BuffName, "AhriSeduce") ||
                         Engine::TextContains(args.BuffName, "charm");
    if (Engine::ValidEnemy(enemy) && isCharm) {
        CharmTargetId = static_cast<int>(enemy.NetworkId());
        ETargetId = CharmTargetId;
        CharmExpireTick = now + ControllerHelpers::RemainingMilliseconds(
            args.EndTime, 1200, 250, 2600);
        ActiveSequence = Sequence::CharmConfirmedBurst;
        SequenceTargetId = CharmTargetId;
        SequenceExpireTick = CharmExpireTick;
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "AhriTumble")) {
        RActive = false;
        RWindowExpireTick = 0;
        RCastsInWindow = 0;
        RObservedAmmo = -1;
        RObservedMaxAmmo = -1;
        return;
    }
    if (static_cast<int>(args.Sender.NetworkId) == CharmTargetId &&
        (args.Type == static_cast<int>(SDK::BuffType::Charm) ||
         Engine::TextContains(args.BuffName, "AhriSeduce") ||
         Engine::TextContains(args.BuffName, "charm"))) {
        CharmTargetId = 0;
        CharmExpireTick = 0;
    }
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "AhriTumble") &&
        args.EndTime > Game::Time()) {
        RActive = true;
        RWindowExpireTick = SDK::Variables::TickCount() +
            ControllerHelpers::RemainingMilliseconds(
                args.EndTime, kRWindowMs, 250, 20000);
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    const int now = SDK::Variables::TickCount();
    if (CharmTargetId != 0 && now < CharmExpireTick &&
        CharmExpireTick - now <= 310 && Engine::RuntimeSpells[0] &&
        Engine::RuntimeSpells[0]->IsReady() && !QActive) {
        // A late AA would consume the guaranteed Charm window and lose both Q
        // passes.  This is the narrow exception to normal AA preservation.
        args.Process = false;
    }
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args)) return;
    const int now = SDK::Variables::TickCount();
    if (IsQMissileName(args.SpellName, args.MissileName)) {
        QActive = true;
        QReturning = IsQReturnName(args.SpellName, args.MissileName);
        QMissileNetworkId = args.MissileNetworkId != 0
            ? static_cast<int>(args.MissileNetworkId)
            : static_cast<int>(args.Sender.NetworkId);
        QMissilePosition = args.Sender.Position.IsValid()
            ? args.Sender.Position
            : args.StartPosition;
        QLastSeenTick = now;
        if (QCastTick <= 0) QCastTick = now;
    } else if (IsCharmMissileName(args.SpellName, args.MissileName)) {
        EMissileNetworkId = args.MissileNetworkId != 0
            ? static_cast<int>(args.MissileNetworkId)
            : static_cast<int>(args.Sender.NetworkId);
    }
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args)) return;
    const int id = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    if (IsQMissileName(args.SpellName, args.MissileName) &&
        (id == QMissileNetworkId || IsQReturnName(args.SpellName, args.MissileName))) {
        if (IsQReturnName(args.SpellName, args.MissileName) || QReturning) {
            QActive = false;
            QReturning = false;
            QMissileNetworkId = 0;
            QMissilePosition = {};
            QTargetId = 0;
            LastReturnCandidate = {};
            LastReturnImprovement = 0.0f;
        }
    }
    if (IsCharmMissileName(args.SpellName, args.MissileName) &&
        (id == EMissileNetworkId || EMissileNetworkId == 0)) {
        EMissileNetworkId = 0;
    }
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::Assassinate: return "assassinate";
    case Posture::Peel: return "peel";
    case Posture::Escape: return "escape";
    default: return "neutral";
    }
}

inline void OnDraw() {
    if (!CoachMenu || !ObjectManager::Player().IsValid()) return;
    const auto player = ObjectManager::Player();
    if (Bool(CoachMenu, "DrawOrbPath", true) && QActive &&
        QMissilePosition.IsValid() && !QMissilePosition.IsZero()) {
        Drawing::DrawCircle(QMissilePosition, 38.0f,
                            QReturning ? 0xFFFF77DDu : 0xFF77CCFFu, 2.0f, 40);
        if (QReturning) {
            Drawing::DrawLine(QMissilePosition, player.Position(),
                              0xAAFF77DDu, 2.0f);
        }
    }
    if (Bool(CoachMenu, "DrawRedirect", true) &&
        LastReturnCandidate.IsValid() && !LastReturnCandidate.IsZero() &&
        QReturning) {
        Drawing::DrawCircle(LastReturnCandidate, 52.0f, 0xFF77FF99u, 2.0f, 48);
        Drawing::DrawLine(QMissilePosition, LastReturnCandidate,
                          0xFF77FF99u, 2.0f);
    }
    if (Bool(CoachMenu, "DrawRush", true) &&
        LastRushDestination.IsValid() && !LastRushDestination.IsZero() &&
        SDK::Variables::TickCount() - RLastCastTick <= 1300) {
        Drawing::DrawCircle(LastRushDestination, 48.0f, 0xFFAA88FFu, 2.0f, 40);
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen = {};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char state[220] = {};
            _snprintf_s(state, sizeof(state), _TRUNCATE,
                        "Ahri one-trick | %s | Q %s | R %d (reserve %d) | seq %d | Q2 gain %.0f%%",
                        PostureName(CurrentPosture),
                        QReturning ? "return" : (QActive ? "out" : "ready"),
                        RuntimeRushCharges(), ReservedRushCharges(),
                        static_cast<int>(ActiveSequence),
                        LastReturnImprovement * 100.0f);
            Drawing::DrawText(screen.x - 145.0f, screen.y - 116.0f,
                              0xFFFFD9F5u, state);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "AhriOneTrick", "Ahri one-trick mechanics"));

    OrbMenu = TacticsMenu->AddSubMenu(new Menu(
        "OrbReturn", "Orb outbound/return routing"));
    OrbMenu->Add(new MenuBool("TipDoubleHit", "Aim Q turnaround for near-simultaneous Q1 + Q2", true));
    OrbMenu->Add(new MenuSlider("TipConfidence", "Minimum tip double-hit confidence (%)", 62, 35, 90));
    OrbMenu->Add(new MenuBool("RushRedirect", "Use R only when it converts return Q from miss to hit", true));
    OrbMenu->Add(new MenuSlider("KeepCurrentHit", "Do not R if current return hit score is at least (%)", 58, 35, 90));
    OrbMenu->Add(new MenuSlider("RedirectHit", "Required post-R return hit score (%)", 55, 35, 90));
    OrbMenu->Add(new MenuSlider("RedirectGain", "Minimum return-hit improvement from R (%)", 22, 8, 60));

    CharmMenu = TacticsMenu->AddSubMenu(new Menu(
        "CharmWindows", "Charm certainty and teamfight role"));
    CharmMenu->Add(new MenuSlider("MinimumQuality", "Minimum normal Charm window quality", 4, 2, 6));
    CharmMenu->Add(new MenuSlider("ReactiveQuality", "Minimum anti-dash/interrupt quality", 2, 1, 5));
    CharmMenu->Add(new MenuSlider("RawMaxRange", "Maximum raw E range without commitment/CC", 825, 550, 975));
    CharmMenu->Add(new MenuSlider("RushCharmRange", "Post-R distance used to guarantee E", 540, 325, 750));
    CharmMenu->Add(new MenuBool("HideEWithR", "Use E -> R to hide Charm animation when the line is already valid", true));
    CharmMenu->Add(new MenuBool("HarassOnlyGuaranteed", "Harass E only on CC, dash, or committed animation", true));
    CharmMenu->Add(new MenuBool("PeelCarry", "Peel a threatened carry before diving", true));

    FoxFireMenu = TacticsMenu->AddSubMenu(new Menu(
        "FoxFire", "Fox-Fire target marking and movement"));
    FoxFireMenu->Add(new MenuBool("AAWAA", "Use AA -> W -> AA short-trade branch", true));
    FoxFireMenu->Add(new MenuBool("QWTrade", "Use quick Q -> W/AA Electrocute-style trade", true));
    FoxFireMenu->Add(new MenuBool("DodgeWithW", "Use W movement speed for a detected incoming line", true));
    FoxFireMenu->Add(new MenuSeparator(
        "WPriority", "W waits for Charm, the recent AA target, Q target, close range, or lethal damage."));

    RushMenu = TacticsMenu->AddSubMenu(new Menu(
        "SpiritRush", "Spirit Rush charge economy"));
    RushMenu->Add(new MenuBool("UseToGuaranteeCharm", "Use R to create a collision-free high-confidence E", true));
    RushMenu->Add(new MenuSlider("ReserveCharges", "Reserve R charges for exit", 1, 0, 2));
    RushMenu->Add(new MenuSlider("IsolatedTargetHp", "Commit on isolated target below HP (%)", 72, 25, 100));
    RushMenu->Add(new MenuSlider("EscapeHp", "Switch to escape posture below own HP (%)", 26, 10, 60));
    RushMenu->Add(new MenuSlider("MaxRushEnemies", "Maximum enemies at R destination", 2, 1, 5));
    RushMenu->Add(new MenuSlider("ExitBeforeExpiryMs", "Use reserved exit R this long before expiry (ms)", 1350, 500, 2500));
    RushMenu->Add(new MenuBool("RespectLockdown", "Wait for nearby point-click lockdown cooldowns", true));
    RushMenu->Add(new MenuBool("RespectCursor", "Reject aggressive R far against player cursor", true));
    RushMenu->Add(new MenuBool("OverrideLockdownForLethal", "Allow lethal R through ready lockdown (risky)", false));
    RushMenu->Add(new MenuBool("Execute", "Spend a charge when R damage itself secures the kill", true));
    RushMenu->Add(new MenuBool("RushFlee", "Use any available R charge during explicit flee", true));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "AhriFarm", "Two-pass Orb farming"));
    FarmMenu->Add(new MenuSlider("QMinions", "Minimum lane minions crossed by both Q passes", 3, 1, 8));
    FarmMenu->Add(new MenuBool("LaneW", "Use W for confirmed lane last hits", false));
    FarmMenu->Add(new MenuSlider("WLastHits", "Minimum confirmed W lane last hits", 2, 1, 3));
    FarmMenu->Add(new MenuBool("JungleW", "Use W on jungle monsters", true));
    FarmMenu->Add(new MenuSeparator(
        "NoFarmR", "Spirit Rush is never spent on farming."));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "AhriCoach", "One-trick visual coaching"));
    CoachMenu->Add(new MenuBool("DrawOrbPath", "Draw live outbound/return Orb path", true));
    CoachMenu->Add(new MenuBool("DrawRedirect", "Draw a return-Q redirect destination", true));
    CoachMenu->Add(new MenuBool("DrawRush", "Draw the most recent R destination", true));
    CoachMenu->Add(new MenuBool("DrawState", "Draw posture, Q state, R charges, and sequence", true));
}

inline void OnLoad() {
    ActiveSequence = Sequence::None;
    CurrentPosture = Posture::Neutral;
    LastRushPurpose = RushPurpose::None;
    SequenceTargetId = 0;
    SequenceExpireTick = 0;
    QActive = false;
    QReturning = false;
    QMissileNetworkId = 0;
    QCastTick = 0;
    QLastSeenTick = 0;
    QTargetId = 0;
    QMissilePosition = {};
    QCastOrigin = {};
    QCastEnd = {};
    LastTipScore = 0.0f;
    EMissileNetworkId = 0;
    ECastTick = 0;
    ETargetId = 0;
    CharmTargetId = 0;
    CharmExpireTick = 0;
    RActive = ObjectManager::Player().HasBuff("AhriTumble");
    RWindowExpireTick = RActive ? SDK::Variables::TickCount() + kRWindowMs : 0;
    RLastCastTick = 0;
    RCastsInWindow = 0;
    RObservedAmmo = -1;
    RObservedMaxAmmo = -1;
    LastRushDestination = {};
    PendingHiddenRushDestination = {};
    LastReturnCandidate = {};
    LastReturnImprovement = 0.0f;
    LastAutoTargetId = 0;
    LastAutoTick = 0;
    WLastCastTick = 0;
    GapcloserTargetId = 0;
    GapcloserExpireTick = 0;
    GapcloserEnd = {};
    InterruptTargetId = 0;
    InterruptExpireTick = 0;
    CommittedTargetId = 0;
    CommittedUntilTick = 0;
    IncomingLineThreatUntil = 0;
    RefreshTrackedMissiles();
}

inline void OnUnload() {
    TacticsMenu = nullptr;
    OrbMenu = nullptr;
    CharmMenu = nullptr;
    FoxFireMenu = nullptr;
    RushMenu = nullptr;
    FarmMenu = nullptr;
    CoachMenu = nullptr;
    ActiveSequence = Sequence::None;
}

inline constexpr const char* Scenarios[] = {
    "Synchronize outbound Q from local cast and dedicated missile lifecycle",
    "Recognize both legacy and current outbound Orb missile names",
    "Transition to return-Q state without losing the missile on outbound deletion",
    "Recover Q state by scanning live missiles after a missed lifecycle callback",
    "Aim Q at the turnaround zone for a near-simultaneous two-pass hit",
    "Track the intended Q target across outbound and return travel",
    "Do not spend R when the current return line already hits",
    "Solve a lateral R destination that turns a Q2 miss into a hit",
    "Predict target movement at the return Orb's intersection time",
    "Reject a Q2 redirect that does not materially improve hit probability",
    "Reject a Q2 redirect into a wall, turret, excess enemies, or ready lockdown",
    "Preserve the last R charge while redirecting Q2 unless the sequence is lethal",
    "Cast E then R to hide Charm animation when the original E line is valid",
    "Cast R then E when a new angle removes minion collision",
    "Dash close enough to guarantee Charm rather than firing raw max-range E",
    "Wait for point-click lockdown cooldowns before crossing the frontline",
    "Override lockdown avoidance only through an explicit risky lethal setting",
    "Use Charm on hard CC, dash endpoint, or committed cast animation",
    "Hold ordinary long-range Charm below the configured certainty threshold",
    "Cancel a directed dash with Charm",
    "Interrupt a channel with collision-aware Charm",
    "Continue Q-W burst after a player-created E-Flash Charm",
    "Q then W after confirmed Charm without overwriting the AA windup",
    "Use AA-W-AA as the short lane trade",
    "Use quick Q-W/AA movement trade against a difficult ranged lane",
    "Use W movement speed only for a detected incoming spell line",
    "Wait for Charm, recent AA, Q mark, close range, or lethal before W",
    "Switch from assassination to peel when a valuable ally is being dived",
    "Reserve R3 as an exit in an ordinary all-in",
    "Spend reserved R when its own damage secures a takedown/reset",
    "Use the reserved charge to exit before the recast window expires under threat",
    "Use any safe R charge during explicit player flee mode",
    "Reject an aggressive R opposite the player's cursor at low health",
    "Re-plan after manual Q/E/R while preserving observed projectile and ammo state",
    "Compute Q1 magic plus Q2 true damage separately for lethal decisions",
    "Use conservative multi-flame W damage in all-in estimates",
    "Choose a line that crosses lane minions on both Q passes",
    "Use health prediction for outbound-versus-return Q last hits",
    "Never spend Spirit Rush on farming",
    "Use W on jungle while reserving lane W for confirmed last hits",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Ahri";
    controller.ControllerId = "champion.kuroaio.ai.ahri.onetrick";
    controller.KitRevision = "League 26.14 / CommunityDragon 16.14";
    controller.ResearchArtifact = "AI/Research/AIAhri.md";
    controller.ImplementationSummary =
        "Dedicated outbound/return Orb tracking, geometric R-to-Q2 interception, "
        "E-R and R-E Charm branches, charge-reserved Spirit Rush economy, "
        "marked Fox-Fire/AA trades, posture-aware peel, and two-pass farming.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast =
        &ControllerHelpers::CaptureLocalAutoAttackEvent<
            &LastAutoTargetId, &LastAutoTick>;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &OnBuffUpdate;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack =
        &ControllerHelpers::CaptureAfterAttackEvent<
            &LastAutoTargetId, &LastAutoTick>;
    controller.OnGapcloser =
        &ControllerHelpers::CaptureGapcloserEvent<
            &GapcloserTargetId, &GapcloserEnd,
            &GapcloserExpireTick, 475, 720>;
    controller.OnInterruptable =
        &ControllerHelpers::CaptureInterruptableEvent<
            &InterruptTargetId, &InterruptExpireTick>;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Ahri
