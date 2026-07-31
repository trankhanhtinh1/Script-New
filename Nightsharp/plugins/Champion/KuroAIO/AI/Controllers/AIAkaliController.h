#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIAkaliGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Akali {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::ChampionIs;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::EnemySpellReady;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::RuntimeNameContains;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::UnitByNetworkId;
using ControllerHelpers::ValidHostileUnit;

// A deliberately explicit state machine.  Akali's damage does not come from
// casting four slots in order: every spell hit asks whether the player should
// cash a passive kama, spend W's only safety window, take E2, or preserve R2.
enum class Sequence : int {
    None,
    PassiveRingExit,
    PassiveKama,
    QTipToE,
    R1FastE,
    R1ThenQ,
    R1ThenWForEnergy,
    E2ArrivalBufferQ,
    E2ThenR1,
    ShroudAnchor,
    BackflipEntry,
    Cleanup,
};

enum class Posture : int {
    Neutral,
    Trade,
    Assassinate,
    Cleanup,
    Stall,
    Escape,
};

enum class EMarkKind : int {
    None,
    Enemy,
    Shroud,
    JungleObjective,
};

enum class R2Purpose : int {
    None,
    Execute,
    Exit,
    Reposition,
};

inline Menu* TacticsMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* EnergyMenu = nullptr;
inline Menu* ShroudMenu = nullptr;
inline Menu* FlipMenu = nullptr;
inline Menu* ExecutionMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline Sequence ActiveSequence = Sequence::None;
inline Posture CurrentPosture = Posture::Neutral;
inline EMarkKind CurrentEMark = EMarkKind::None;
inline R2Purpose LastR2Purpose = R2Purpose::None;
inline int SequenceTargetId = 0;
inline int SequenceExpireTick = 0;

// Passive is observed from the real buff whenever available.  The short
// candidate window lets the controller draw the intended ring immediately
// after a very-high-confidence hit, but it never hard-locks casts based on a
// prediction alone.
inline bool PassiveRingActive = false;
inline bool PassiveWeaponReady = false;
inline int PassiveTargetId = 0;
inline int PassiveRingExpireTick = 0;
inline int PassiveWeaponExpireTick = 0;
inline int PassiveCandidateTargetId = 0;
inline int PassiveCandidateExpireTick = 0;
inline int PassiveRingObjectId = 0;
inline Vector3 PassiveRingOrigin = {};
inline Vector3 PassiveExitCoachPoint = {};
inline float PassiveExitRemaining = 0.0f;

inline int QTargetId = 0;
inline int QCastTick = 0;
inline Vector3 QCastDirection = {};
inline bool LastQWasTip = false;

inline bool WActive = false;
inline int WCastTick = 0;
inline int WExpireTick = 0;
inline int WObjectNetworkId = 0;
inline Vector3 ShroudCenter = {};
inline Vector3 PendingShroudPosition = {};
inline int LastDefensiveWTick = 0;

inline int PendingETargetId = 0;
inline int EMarkTargetId = 0;
inline int E1CastTick = 0;
inline int E2CastTick = 0;
inline int EMarkExpireTick = 0;
inline bool PendingEToShroud = false;
inline Vector3 LastEBackflipDestination = {};
inline Vector3 LastE1CastPosition = {};
inline int EMissileNetworkId = 0;

inline bool RWindowActive = false;
inline int R1CastTick = 0;
inline int R2UnlockTick = 0;
inline int RWindowExpireTick = 0;
inline int RTargetId = 0;
inline Vector3 LastR1Landing = {};
inline Vector3 LastR2Destination = {};
inline int R2LastCastTick = 0;

inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int CommittedEnemyId = 0;
inline int CommittedEnemyUntil = 0;

inline constexpr float kQRange = 500.0f;
inline constexpr float kQEdgeRange = 555.0f;
inline constexpr float kERange = 825.0f;
inline constexpr float kEBackflipDistance = 400.0f;
inline constexpr float kR1Range = 675.0f;
inline constexpr float kR2Range = 800.0f;
inline constexpr int kPassiveRingMs = 4000;
inline constexpr int kPassiveWeaponMs = 4000;
inline constexpr int kEWindowMs = 3000;
inline constexpr int kR2LockMs = 2500;
inline constexpr int kRWindowMs = 10000;

inline bool CastThrottleReady(int index, bool fastFollowup = false) {
    return ControllerHelpers::CastThrottleReady(
        index, 48, fastFollowup ? 18 : -1);
}

inline bool ESecondCastAvailable() {
    return RuntimeNameContains(2, "AkaliEb");
}

inline bool RSecondCastAvailable() {
    return RuntimeNameContains(3, "AkaliRb");
}

inline float QCost() { return SpellCost(0); }
inline float ECost() { return SpellCost(2); }

inline bool HasPassiveWeapon() {
    const auto player = GameObjects::Player();
    return player.IsValid() &&
           (player.HasBuff("AkaliPWeapon") || player.HasBuff("akalipweapon") ||
            PassiveWeaponReady);
}

inline bool HasPassiveRingBuff() {
    const auto player = GameObjects::Player();
    return ControllerHelpers::HasAnyBuff(
        player, { "AkaliPZoneGround", "akalipzoneground" });
}

inline bool HasStealthShroudBuff() {
    const auto player = GameObjects::Player();
    return player.IsValid() &&
           (player.HasBuff("AkaliW") || player.HasBuff("AkaliWStealthTracker") ||
            player.HasBuff("akaliw") || player.HasBuff("akaliwstealthtracker"));
}

inline bool TargetCannotBeDamaged(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           target.HasBuff("SivirE") || target.HasBuff("NocturneShroudofDarkness") ||
           target.HasBuff("MorganaE") || target.HasBuff("BlackShield") ||
           target.HasBuff("BansheesVeil") || target.HasBuff("EdgeOfNight") ||
           target.HasBuff("FioraW") || target.HasBuff("VladimirSanguinePool") ||
           target.HasBuff("FizzEIcon") || target.HasBuff("KayleR") ||
           target.HasBuff("kindredrnodeathbuff") || target.HasBuff("ChronoShift");
}

// Do not put Akali's last escape (W/R2) into a point-click suppress/stun zone
// unless the player explicitly permits a lethal gamble.  The list is small on
// purpose: broad "any enemy R" heuristics make assassins unusably passive.
inline bool HasPointClickLockdownAt(const Vector3& position) {
    return Bool(ExecutionMenu, "RespectLockdown", true) &&
           ControllerHelpers::HasReadyPointClickThreatAt(position);
}

inline bool MordekaiserCanPunishShroud() {
    const auto player = GameObjects::Player();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ChampionIs(enemy, "Mordekaiser") &&
            enemy.Position().Distance2D(player.Position()) <= 750.0f &&
            EnemySpellReady(enemy, SDK::SpellSlot::R)) {
            return true;
        }
    }
    return false;
}

inline bool HasReliableRevealThreat() {
    const auto player = GameObjects::Player();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1200.0f)) continue;
        const float distance = enemy.Position().Distance2D(player.Position());
        if ((ChampionIs(enemy, "LeeSin") && distance <= 1100.0f &&
             EnemySpellReady(enemy, SDK::SpellSlot::Q)) ||
            (ChampionIs(enemy, "Lulu") && distance <= 650.0f &&
             EnemySpellReady(enemy, SDK::SpellSlot::E)) ||
            (ChampionIs(enemy, "Karma") && distance <= 700.0f &&
             EnemySpellReady(enemy, SDK::SpellSlot::W)) ||
            (ChampionIs(enemy, "Rengar") && EnemySpellReady(enemy, SDK::SpellSlot::R))) {
            return true;
        }
    }
    return false;
}

inline bool SafePoint(const Vector3& destination,
                      const AIHeroClient& target,
                      int spellIndex,
                      bool lethal = false,
                      bool escaping = false) {
    if (!destination.IsValid() || destination.IsZero() || SDK::NavMesh::IsWall(destination)) {
        return false;
    }
    if (Engine::UnderEnemyTurret(destination) && !lethal) return false;
    const int allowed = std::max(1, Slider(ExecutionMenu, "MaxCommitEnemies", 2));
    const int enemies = Engine::CountEnemiesAt(destination, 625.0f);
    const auto player = GameObjects::Player();
    if (enemies > allowed && !(lethal || escaping || player.HealthPercent() > 84.0f)) {
        return false;
    }
    if (!lethal && HasPointClickLockdownAt(destination)) return false;
    if (spellIndex >= 0 && spellIndex < 4) {
        SpellSpec spec = Engine::ResolvedSpecs[spellIndex];
        if (escaping) spec.Aim = AimPolicy::AwayFromThreat;
        const float score = Engine::PositionDangerScore(destination, target, spec);
        if (score <= -10000.0f && !lethal) return false;
    }
    return true;
}

inline float EffectivePassiveAaRange(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.AttackRange() * 2.0f + player.BoundingRadius() +
           target.BoundingRadius() + 20.0f;
}

inline bool TargetInPassiveAaRange(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return Engine::ValidEnemy(target) &&
           player.Position().Distance2D(target.Position()) <= EffectivePassiveAaRange(target);
}

inline bool CanSpendEnergyForQ(int followupQs, bool reserveE) {
    const float q = std::max(1.0f, QCost());
    const float reserve = reserveE && !ESecondCastAvailable() &&
        Engine::RuntimeSpells[2] && Engine::RuntimeSpells[2]->IsReady()
        ? ECost() : 0.0f;
    return CurrentResource() + 0.5f >= q * std::max(1, followupQs) + reserve;
}

inline bool WCanRestorePlannedCombo(int qCount, bool includeE) {
    if (!Engine::RuntimeSpells[1] || !Engine::RuntimeSpells[1]->IsReady()) return false;
    const float need = QCost() * std::max(1, qCount) + (includeE ? ECost() : 0.0f);
    // Live data grants 100 energy; leave a small buffer for regen/tick jitter.
    return CurrentResource() < need && CurrentResource() + 98.0f >= need;
}

inline float QDamage(const AIBaseClient& target) {
    return Engine::RuntimeSpells[0] && target.IsValid()
        ? std::max(0.0f, Engine::RuntimeSpells[0]->GetDamage(target))
        : 0.0f;
}

inline float E2Damage(const AIBaseClient& target) {
    return Engine::RuntimeSpells[2] && target.IsValid()
        ? std::max(0.0f, Engine::RuntimeSpells[2]->GetDamage(
              target, SDK::DamageStage::SecondCast))
        : 0.0f;
}

inline float R2Damage(const AIHeroClient& target) {
    if (!Engine::RuntimeSpells[3] || !target.IsValid()) return 0.0f;
    float damage = Engine::RuntimeSpells[3]->GetDamage(
        target, SDK::DamageStage::SecondCast);
    if (damage <= 1.0f) {
        // Old runtime spell data sometimes reports Rb's minimum cast in the
        // default stage.  The multiplier is live-kit based and capped at 3x.
        damage = Engine::RuntimeSpells[3]->GetDamage(target) *
                 R2ExecuteMultiplier(target.HealthPercent());
    }
    return std::max(0.0f, damage);
}

inline float ShortComboDamage(const AIHeroClient& target,
                              bool includeE2,
                              bool includeR2) {
    float damage = QDamage(target);
    if (HasPassiveWeapon()) damage += QDamage(target) * 0.72f;
    if (includeE2) damage += E2Damage(target);
    if (includeR2) damage += R2Damage(target);
    return damage;
}

inline bool LethalWith(const AIHeroClient& target, float damage) {
    return Engine::ValidEnemy(target) &&
           damage >= target.Health() + target.AllShield();
}

inline bool R2Unlocked() {
    const int now = SDK::Variables::TickCount();
    return RWindowActive && RSecondCastAvailable() && now >= R2UnlockTick &&
           now < RWindowExpireTick;
}

inline bool R1Available() {
    return Engine::RuntimeSpells[3] && Engine::RuntimeSpells[3]->IsReady() &&
           !RSecondCastAvailable() && !RWindowActive;
}

inline bool E1Available() {
    return Engine::RuntimeSpells[2] && Engine::RuntimeSpells[2]->IsReady() &&
           !ESecondCastAvailable();
}

inline bool CanCommitOn(const AIHeroClient& target, bool lethal) {
    const auto player = GameObjects::Player();
    if (!Engine::ValidEnemy(target) || TargetCannotBeDamaged(target)) return false;
    if (player.HealthPercent() < Slider(ExecutionMenu, "MinCommitHP", 30) && !lethal) {
        return false;
    }
    const int nearby = Engine::CountEnemiesAt(target.Position(), 725.0f);
    if (nearby > Slider(ExecutionMenu, "MaxCommitEnemies", 2) && !lethal) return false;
    if (HasPointClickLockdownAt(target.Position()) && !lethal) return false;
    return true;
}

inline Posture DeterminePosture(const AIHeroClient& selected) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return Posture::Neutral;
    if (player.HealthPercent() <= Slider(ExecutionMenu, "EscapeHP", 27) ||
        (RWindowActive && RWindowExpireTick - SDK::Variables::TickCount() < 1150 &&
         Engine::CountEnemiesAt(player.Position(), 800.0f) > 0)) {
        return Posture::Escape;
    }
    if (R2Unlocked() && Engine::ValidEnemy(selected) &&
        selected.HealthPercent() <= Slider(ExecutionMenu, "CleanupTargetHP", 54)) {
        return Posture::Cleanup;
    }
    if (Engine::ValidEnemy(selected)) {
        const int enemies = Engine::CountEnemiesAt(selected.Position(), 700.0f);
        if (enemies <= 1 && selected.HealthPercent() <=
            Slider(ExecutionMenu, "AssassinateTargetHP", 82)) {
            return Posture::Assassinate;
        }
        if (enemies >= 3 || HasReliableRevealThreat()) return Posture::Stall;
        return Posture::Trade;
    }
    return Posture::Neutral;
}

inline void StartPassiveCandidate(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    PassiveCandidateTargetId = static_cast<int>(target.NetworkId());
    PassiveCandidateExpireTick = SDK::Variables::TickCount() + 520;
}

inline void StartPassiveRing(const AIHeroClient& target, bool confirmed) {
    if (!Engine::ValidEnemy(target)) return;
    const auto player = GameObjects::Player();
    PassiveTargetId = static_cast<int>(target.NetworkId());
    if (PassiveRingObjectId == 0 || PassiveRingOrigin.IsZero()) {
        PassiveRingOrigin = Geometry::PassiveRingCenter(
            player.Position(), target.Position());
    }
    PassiveRingExpireTick = SDK::Variables::TickCount() + kPassiveRingMs;
    PassiveRingActive = true;
    if (confirmed) {
        ActiveSequence = Sequence::PassiveRingExit;
        SequenceTargetId = PassiveTargetId;
        SequenceExpireTick = PassiveRingExpireTick;
    }
}

inline void ClearPassiveRing() {
    PassiveRingActive = false;
    PassiveRingObjectId = 0;
    PassiveRingExpireTick = 0;
    PassiveRingOrigin = {};
    PassiveExitCoachPoint = {};
    PassiveExitRemaining = 0.0f;
    if (ActiveSequence == Sequence::PassiveRingExit) {
        ActiveSequence = Sequence::None;
        SequenceTargetId = 0;
        SequenceExpireTick = 0;
    }
}

inline void ClearEMark() {
    CurrentEMark = EMarkKind::None;
    PendingETargetId = 0;
    EMarkTargetId = 0;
    EMarkExpireTick = 0;
    PendingEToShroud = false;
}

inline void ClearRWindow() {
    RWindowActive = false;
    R1CastTick = 0;
    R2UnlockTick = 0;
    RWindowExpireTick = 0;
    RTargetId = 0;
}

inline bool CastQ(const AIHeroClient& target,
                  Mode mode,
                  bool guaranteed = false,
                  bool bufferDuringDash = false,
                  bool killSecure = false) {
    if (!Engine::ValidEnemy(target, kQEdgeRange + 35.0f) ||
        !SpellEnabled(0, mode) || !CastThrottleReady(0, bufferDuringDash) ||
        !CanSpendEnergyForQ(guaranteed ? 1 : 2, false) ||
        TargetCannotBeDamaged(target)) {
        return false;
    }
    if (Orbwalker::IsWindingUp() && Bool(Engine::HumanMenu, "PreserveAttacks", true) &&
        !bufferDuringDash && !killSecure) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (player.IsDashing() && !bufferDuringDash) {
        return false;
    }
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vector3 predicted = prediction.GetCastPosition().IsValid() &&
        !prediction.GetCastPosition().IsZero()
        ? prediction.GetCastPosition()
        : (prediction.GetUnitPosition().IsValid() && !prediction.GetUnitPosition().IsZero()
            ? prediction.GetUnitPosition()
            : PredictPosition(target, 0.22f));
    const auto aim = BestFivePointAim(player.Position(), predicted, target.BoundingRadius());
    if (aim.Direction.IsZero()) return false;
    const bool locked = Engine::IsHardCrowdControlled(target) ||
                        static_cast<int>(target.NetworkId()) == EMarkTargetId;
    SDK::HitChance minimum = guaranteed || locked
        ? SDK::HitChance::Medium
        : (aim.Hit.TipSlow ? SDK::HitChance::High : SDK::HitChance::VeryHigh);
    if (mode == Mode::Combo && !guaranteed && !locked) {
        minimum = aim.Hit.TipSlow ? SDK::HitChance::Medium : SDK::HitChance::High;
    }
    if (prediction.Hitchance < minimum && !locked && !target.IsDashing()) return false;
    const float targetDistance = player.Position().Distance2D(predicted);
    if (targetDistance > kQRange + target.BoundingRadius() &&
        !aim.Hit.TipSlow && !locked) return false;

    const Vector3 endpoint = player.Position() + aim.Direction * kQRange;
    QTargetId = static_cast<int>(target.NetworkId());
    QCastDirection = aim.Direction;
    LastQWasTip = aim.Hit.TipSlow;
    if (Engine::ControllerCastPosition(0, endpoint)) {
        QCastTick = SDK::Variables::TickCount();
        StartPassiveCandidate(target);
        if (aim.Hit.TipSlow && E1Available()) {
            ActiveSequence = Sequence::QTipToE;
            SequenceTargetId = QTargetId;
            SequenceExpireTick = QCastTick + 720;
        }
        return true;
    }
    return false;
}

inline Vector3 ChooseShroudPosition(const AIHeroClient& target, bool defensive) {
    const auto player = GameObjects::Player();
    Vector3 direction = defensive
        ? Direction2D(player.Position(), Game::CursorPos())
        : Direction2D(player.Position(), target.Position());
    if (direction.IsZero()) direction = { 1.0f, 0.0f, 0.0f };
    const float distance = defensive ? 75.0f : 125.0f;
    return player.Position() + direction * distance;
}

inline bool playerOrTargetClose(const AIHeroClient& target, float range) {
    const auto player = GameObjects::Player();
    return Engine::ValidEnemy(target) &&
           player.Position().Distance2D(target.Position()) <= range +
               player.BoundingRadius() + target.BoundingRadius();
}

inline bool CastW(const AIHeroClient& target,
                  Mode mode,
                  bool defensive,
                  bool energyRoute = false) {
    if (!Engine::RuntimeSpells[1] || !Engine::RuntimeSpells[1]->IsReady() ||
        !SpellEnabled(1, mode) || !CastThrottleReady(1) || WActive) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDashing()) return false;
    if (!defensive && MordekaiserCanPunishShroud() &&
        Bool(ShroudMenu, "SaveVsMordekaiser", true)) {
        return false;
    }
    if (!defensive && HasReliableRevealThreat() &&
        Bool(ShroudMenu, "DelayIntoReveal", true)) {
        return false;
    }
    if (!defensive && !energyRoute &&
        !Engine::IsHardCrowdControlled(target) &&
        playerOrTargetClose(target, 430.0f) == false) {
        return false;
    }
    PendingShroudPosition = ChooseShroudPosition(target, defensive);
    if (Engine::ControllerCastPosition(1, PendingShroudPosition)) {
        WCastTick = SDK::Variables::TickCount();
        const auto spell = player.Spellbook().GetSpell(SDK::SpellSlot::W);
        const int rank = spell.IsValid() ? std::clamp(spell.Level(), 1, 5) : 1;
        WExpireTick = WCastTick + 5000 + (rank - 1) * 500;
        WActive = true;
        ShroudCenter = PendingShroudPosition;
        if (defensive) LastDefensiveWTick = WCastTick;
        return true;
    }
    return false;
}

inline bool CastE1Raw(const Vector3& castPosition,
                      const AIHeroClient& target,
                      Mode mode,
                      bool intendedHit,
                      bool escapeBackflip,
                      bool shroudAnchor = false) {
    if (!E1Available() || !SpellEnabled(2, mode) || !CastThrottleReady(2) ||
        (intendedHit && TargetCannotBeDamaged(target))) {
        return false;
    }
    const auto player = GameObjects::Player();
    const Vector3 direction = Direction2D(player.Position(), castPosition);
    if (direction.IsZero()) return false;
    const Vector3 backflip = ShurikenBackflipEnd(
        player.Position(), direction, kEBackflipDistance);
    const bool lethal = Engine::ValidEnemy(target) &&
        LethalWith(target, E2Damage(target) + QDamage(target));
    if (!SafePoint(backflip, target, 2, lethal, escapeBackflip)) return false;
    if (intendedHit && Engine::ValidEnemy(target)) {
        const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
        if (!prediction.CollisionObjects.empty() ||
            (prediction.Hitchance < SDK::HitChance::High &&
             !Engine::IsHardCrowdControlled(target) && !target.IsDashing())) {
            return false;
        }
    }
    PendingETargetId = intendedHit && Engine::ValidEnemy(target)
        ? static_cast<int>(target.NetworkId()) : 0;
    PendingEToShroud = shroudAnchor;
    LastEBackflipDestination = backflip;
    LastE1CastPosition = castPosition;
    if (Engine::ControllerCastPosition(2, castPosition)) {
        E1CastTick = SDK::Variables::TickCount();
        if (shroudAnchor) {
            ActiveSequence = Sequence::ShroudAnchor;
            SequenceTargetId = 0;
            SequenceExpireTick = E1CastTick + 750;
        } else if (!intendedHit) {
            ActiveSequence = Sequence::BackflipEntry;
            SequenceTargetId = static_cast<int>(target.NetworkId());
            SequenceExpireTick = E1CastTick + 720;
        }
        return true;
    }
    return false;
}

inline bool CastE1AtTarget(const AIHeroClient& target,
                            Mode mode,
                            bool guaranteed = false) {
    if (!Engine::ValidEnemy(target, kERange + 25.0f)) return false;
    const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    if (!prediction.CollisionObjects.empty()) return false;
    const bool locked = guaranteed || Engine::IsHardCrowdControlled(target) ||
                        target.IsDashing() || LastQWasTip;
    SDK::HitChance needed = locked ? SDK::HitChance::Medium : SDK::HitChance::VeryHigh;
    if (mode == Mode::Combo && !locked) {
        needed = SDK::HitChance::High;
    }
    if (prediction.Hitchance < needed && !locked) return false;
    const Vector3 cast = prediction.GetCastPosition();
    return cast.IsValid() && !cast.IsZero() &&
           CastE1Raw(cast, target, mode, true, false);
}

inline bool CastEToShroud(const AIHeroClient& threat, Mode mode) {
    if (!WActive || ShroudCenter.IsZero() || !E1Available() ||
        !Bool(FlipMenu, "UseShroudAnchor", true)) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (player.Position().Distance2D(ShroudCenter) > kERange) return false;
    const Vector3 towardShroud = Direction2D(player.Position(), ShroudCenter);
    const Vector3 backflip = ShurikenBackflipEnd(player.Position(), towardShroud);
    // The route is valuable only when the backwards flip moves through the
    // opponent / toward a Q angle, not when it merely abandons a safe shroud.
    if (Engine::ValidEnemy(threat) &&
        backflip.Distance2D(threat.Position()) >= player.Position().Distance2D(threat.Position())) {
        return false;
    }
    return CastE1Raw(ShroudCenter, threat, mode, false, false, true);
}

inline bool E2DestinationSafe(const AIHeroClient& target, bool lethal) {
    if (!Engine::ValidEnemy(target) || !target.IsVisible() || !target.IsTargetable()) {
        return false;
    }
    const auto player = GameObjects::Player();
    const Vector3 destination = PredictPosition(target, 0.18f);
    const bool exitReady = R2Unlocked() || WActive ||
        (R1Available() && target.Position().Distance2D(player.Position()) <= kR1Range);
    if (!exitReady && !lethal &&
        Engine::CountEnemiesAt(destination, 650.0f) > 1) {
        return false;
    }
    if (!lethal && IncomingHardCCUntil > SDK::Variables::TickCount()) return false;
    return SafePoint(destination, target, 2, lethal, false);
}

inline bool TryE2(const AIHeroClient& fallback, Mode mode) {
    if (!ESecondCastAvailable() || !Engine::RuntimeSpells[2] ||
        !Engine::RuntimeSpells[2]->IsReady() || !SpellEnabled(2, mode) ||
        !CastThrottleReady(2, true)) {
        return false;
    }
    const int now = SDK::Variables::TickCount();
    if (EMarkExpireTick > 0 && now > EMarkExpireTick) {
        ClearEMark();
        return false;
    }
    if (CurrentEMark == EMarkKind::Shroud) {
        const auto player = GameObjects::Player();
        if (!ShroudCenter.IsZero() && SafePoint(ShroudCenter, fallback, 2, false, true) &&
            (mode == Mode::Flee || player.HealthPercent() <= Slider(ExecutionMenu, "EscapeHP", 27) ||
             Engine::CountEnemiesAt(player.Position(), 575.0f) >= 2)) {
            if (Engine::ControllerCastSelf(2)) {
                E2CastTick = now;
                ActiveSequence = Sequence::Cleanup;
                SequenceExpireTick = now + 650;
                return true;
            }
        }
        return false;
    }

    AIHeroClient target = HeroByNetworkId(EMarkTargetId);
    if (!Engine::ValidEnemy(target)) target = fallback;
    if (!Engine::ValidEnemy(target)) return false;
    const bool lethal = LethalWith(target, E2Damage(target) + QDamage(target) +
                                           (R2Unlocked() ? R2Damage(target) : 0.0f));
    const bool expiring = EMarkExpireTick > 0 && EMarkExpireTick - now <=
        Slider(FlipMenu, "TakeMarkBeforeExpiryMs", 480);
    if (!E2DestinationSafe(target, lethal) && !(expiring && lethal)) return false;

    // On high-confidence E marks, R1 first places Akali beyond the victim and
    // makes E2 a chase/return route rather than an automatic suicide dash.
    if (R1Available() && Bool(ExecutionMenu, "R1BeforeE2", true) &&
        Engine::ValidEnemy(target, kR1Range) && CanCommitOn(target, lethal) &&
        CurrentPosture != Posture::Escape) {
        ActiveSequence = Sequence::E2ThenR1;
        SequenceTargetId = static_cast<int>(target.NetworkId());
        SequenceExpireTick = now + 850;
        return false;
    }
    if (Engine::ControllerCastSelf(2)) {
        E2CastTick = now;
        ActiveSequence = Sequence::E2ArrivalBufferQ;
        SequenceTargetId = static_cast<int>(target.NetworkId());
        SequenceExpireTick = now + 950;
        return true;
    }
    return false;
}

inline bool CastR1(const AIHeroClient& target,
                   Mode mode,
                   bool lethal = false,
                   bool escapeRoute = false) {
    if (!R1Available() || !SpellEnabled(3, mode) || !CastThrottleReady(3) ||
        !Engine::ValidEnemy(target, kR1Range + 20.0f) || TargetCannotBeDamaged(target)) {
        return false;
    }
    const auto player = GameObjects::Player();
    const Vector3 predicted = PredictPosition(target, 0.08f);
    const Vector3 landing = R1LandingPoint(player.Position(), predicted);
    if (!SafePoint(landing, target, 3, lethal, escapeRoute)) return false;
    if (!lethal && !escapeRoute && !CanCommitOn(target, false)) return false;
    if (Engine::ControllerCastUnit(3, target)) {
        R1CastTick = SDK::Variables::TickCount();
        R2UnlockTick = R1CastTick + kR2LockMs;
        RWindowExpireTick = R1CastTick + kRWindowMs;
        RWindowActive = true;
        RTargetId = static_cast<int>(target.NetworkId());
        LastR1Landing = landing;
        if (E1Available()) {
            ActiveSequence = Sequence::R1FastE;
            SequenceTargetId = RTargetId;
            SequenceExpireTick = R1CastTick + 850;
        } else {
            ActiveSequence = Sequence::R1ThenQ;
            SequenceTargetId = RTargetId;
            SequenceExpireTick = R1CastTick + 900;
        }
        return true;
    }
    return false;
}

inline bool R2DestinationSafe(const Vector3& destination,
                              const AIHeroClient& target,
                              bool lethal,
                              bool escape) {
    if (!SafePoint(destination, target, 3, lethal, escape)) return false;
    if (!lethal && !escape && IncomingHardCCUntil > SDK::Variables::TickCount()) return false;
    return true;
}

struct R2Route {
    Vector3 Destination = {};
    float HitScore = 0.0f;
    float Safety = -FLT_MAX;
};

inline R2Route FindR2Route(const AIHeroClient& target,
                           bool requireHit,
                           bool lethal,
                           bool escape) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    const Vector3 source = player.Position();
    const Vector3 predicted = Engine::ValidEnemy(target)
        ? PredictPosition(target, 0.10f)
        : Game::CursorPos();
    const Vec3 direct = Direction2D(source, predicted);
    std::array<Vec3, 12> directions = {};
    std::size_t count = 0;
    if (!direct.IsZero()) {
        directions[count++] = direct;
        directions[count++] = Rotate2D(direct, 10.0f * kPi / 180.0f);
        directions[count++] = Rotate2D(direct, -10.0f * kPi / 180.0f);
        directions[count++] = Rotate2D(direct, 22.0f * kPi / 180.0f);
        directions[count++] = Rotate2D(direct, -22.0f * kPi / 180.0f);
    }
    const Vec3 cursor = Direction2D(source, Game::CursorPos());
    if (!cursor.IsZero() && count < directions.size()) directions[count++] = cursor;
    for (int i = 0; i < 5 && count < directions.size(); ++i) {
        const float angle = 2.0f * kPi * static_cast<float>(i) / 5.0f;
        directions[count++] = { std::cos(angle), 0.0f, std::sin(angle) };
    }
    R2Route best{};
    for (std::size_t i = 0; i < count; ++i) {
        if (directions[i].IsZero()) continue;
        const Vector3 destination = source + directions[i] * kR2Range;
        const float hit = Engine::ValidEnemy(target)
            ? DashLineHitScore(source, destination, predicted,
                               target.BoundingRadius())
            : 0.0f;
        if (requireHit && hit < 0.42f) continue;
        if (!R2DestinationSafe(destination, target, lethal, escape)) continue;
        const float safety = Engine::PositionDangerScore(
            destination, target, Engine::ResolvedSpecs[3]) -
            destination.Distance2D(Game::CursorPos()) * (escape ? 0.20f : 0.04f);
        const float score = hit * (escape ? 175.0f : 640.0f) + safety;
        if (score > best.Safety) {
            best = { destination, hit, score };
        }
    }
    return best;
}

inline bool CastR2(const AIHeroClient& target,
                   R2Purpose purpose,
                   bool requireHit,
                   bool lethal,
                   bool escape,
                   Mode mode) {
    if (!R2Unlocked() || !SpellEnabled(3, mode) || !CastThrottleReady(3, true)) {
        return false;
    }
    const R2Route route = FindR2Route(target, requireHit, lethal, escape);
    if (route.Destination.IsZero() || (requireHit && route.HitScore < 0.42f)) return false;
    if (Engine::ControllerCastPosition(3, route.Destination)) {
        LastR2Destination = route.Destination;
        LastR2Purpose = purpose;
        R2LastCastTick = SDK::Variables::TickCount();
        ClearRWindow();
        return true;
    }
    return false;
}

inline bool TryR2Execute(const AIHeroClient& target, Mode mode) {
    if (!R2Unlocked() || !Engine::ValidEnemy(target, 1150.0f) ||
        !Bool(ExecutionMenu, "UseR2Execute", true) || TargetCannotBeDamaged(target)) {
        return false;
    }
    const float damage = R2Damage(target);
    const bool lethal = LethalWith(target, damage);
    const int now = SDK::Variables::TickCount();
    const bool lowEnough = target.HealthPercent() <=
        Slider(ExecutionMenu, "R2HealthThreshold", 44);
    if (!lethal && !lowEnough) return false;
    // R2 is famous for its delayed execute.  Holding it through the lockout
    // is intentional; only spend nonlethal R2 when the window is ending.
    if (!lethal && RWindowExpireTick - now >
        Slider(ExecutionMenu, "NonlethalR2BeforeExpiryMs", 700)) return false;
    return CastR2(target, R2Purpose::Execute, true, lethal, false, mode);
}

inline bool TryR2Exit(const AIHeroClient& threat, Mode mode, bool forced = false) {
    if (!R2Unlocked()) return false;
    const auto player = GameObjects::Player();
    const int now = SDK::Variables::TickCount();
    const bool windowEnding = RWindowExpireTick - now <=
        Slider(ExecutionMenu, "ExitBeforeExpiryMs", 1200);
    const bool danger = player.HealthPercent() <= Slider(ExecutionMenu, "EscapeHP", 27) ||
        Engine::CountEnemiesAt(player.Position(), 700.0f) >= 2 ||
        IncomingHardCCUntil > now;
    if (!(forced || windowEnding || danger || mode == Mode::Flee)) return false;
    return CastR2(threat, R2Purpose::Exit, false, false, true, mode);
}

inline bool TryPassiveWeave(const AIHeroClient& fallback, Mode mode) {
    const int now = SDK::Variables::TickCount();
    AIHeroClient target = HeroByNetworkId(PassiveTargetId);
    if (!Engine::ValidEnemy(target)) target = fallback;
    const auto player = GameObjects::Player();

    if (HasPassiveWeapon()) {
        PassiveWeaponReady = true;
        if (PassiveWeaponExpireTick <= now) PassiveWeaponExpireTick = now + kPassiveWeaponMs;
        if (Engine::ValidEnemy(target) && TargetInPassiveAaRange(target)) {
            // Cooperation rule: do not overwrite the player's empowered AA
            // with Q/E.  Orbwalker remains free to attack and move.
            ActiveSequence = Sequence::PassiveKama;
            SequenceTargetId = static_cast<int>(target.NetworkId());
            SequenceExpireTick = PassiveWeaponExpireTick;
            return true;
        }
        if (PassiveWeaponExpireTick - now <= 440 && Engine::ValidEnemy(target)) {
            // The player owns the movement; show the range but do not waste a
            // spell to compensate for an unconfirmed attack path.
            return true;
        }
    }

    if (PassiveRingActive && Engine::ValidEnemy(target) &&
        now < PassiveRingExpireTick && !HasPassiveWeapon()) {
        PassiveExitCoachPoint = PassiveExitPoint(
            player.Position(), PassiveRingOrigin, Game::CursorPos());
        PassiveExitRemaining = PassiveExitDistance(player.Position(), PassiveRingOrigin);
        if (PassiveExitRemaining > 22.0f &&
            Bool(PassiveMenu, "HoldForRingExit", true) &&
            CurrentPosture != Posture::Escape && mode != Mode::Flee) {
            ActiveSequence = Sequence::PassiveRingExit;
            SequenceTargetId = static_cast<int>(target.NetworkId());
            SequenceExpireTick = PassiveRingExpireTick;
            return true;
        }
    }
    return false;
}

inline bool TryDefensiveW(const AIHeroClient& threat, Mode mode) {
    const auto player = GameObjects::Player();
    const int now = SDK::Variables::TickCount();
    const bool line = IncomingThreatUntil > now || IncomingHardCCUntil > now;
    const bool overwhelmed = Engine::CountEnemiesAt(player.Position(), 600.0f) >= 2;
    const bool low = player.HealthPercent() <= Slider(ShroudMenu, "DefensiveWHP", 39);
    const bool gap = GapcloserTargetId != 0 && now < GapcloserExpireTick;
    if (!(line || overwhelmed || low || gap || mode == Mode::Flee)) return false;
    if (!Bool(ShroudMenu, "UseDefensiveW", true)) return false;
    return CastW(threat, mode, true, false);
}

inline bool TryShroudEnergyRoute(const AIHeroClient& target, Mode mode) {
    if (!Bool(EnergyMenu, "UseWForEnergyRoute", true) ||
        !WCanRestorePlannedCombo(2, E1Available()) ||
        !Engine::ValidEnemy(target, 650.0f) || CurrentPosture == Posture::Escape) {
        return false;
    }
    // W becomes an offensive resource only after R1/mark/contact has created
    // a real commitment, never as a rote opener in neutral lane state.
    const bool committed = RWindowActive || ESecondCastAvailable() ||
        playerOrTargetClose(target, 415.0f) || Engine::IsHardCrowdControlled(target);
    return committed && CastW(target, mode, false, true);
}

inline bool TrySequence(const AIHeroClient& fallback, Mode mode) {
    const int now = SDK::Variables::TickCount();
    if (ActiveSequence == Sequence::None) return false;
    if (SequenceExpireTick > 0 && now > SequenceExpireTick) {
        ActiveSequence = Sequence::None;
        SequenceTargetId = 0;
        return false;
    }
    AIHeroClient target = HeroByNetworkId(SequenceTargetId);
    if (!Engine::ValidEnemy(target)) target = fallback;

    switch (ActiveSequence) {
    case Sequence::QTipToE:
        if (Engine::ValidEnemy(target) && now - QCastTick >= 125) {
            if (CastE1AtTarget(target, mode, true)) return true;
        }
        break;
    case Sequence::R1FastE:
        if (Engine::ValidEnemy(target) && now - R1CastTick >= 35 &&
            now - R1CastTick <= 720) {
            // If R1's pass changes the line, prediction is recalculated from
            // live position instead of replaying the original click vector.
            if (CastE1AtTarget(target, mode, true)) {
                if (WCanRestorePlannedCombo(1, false)) {
                    ActiveSequence = Sequence::R1ThenWForEnergy;
                    SequenceExpireTick = now + 800;
                }
                return true;
            }
        }
        break;
    case Sequence::R1ThenWForEnergy:
        if (Engine::ValidEnemy(target) && TryShroudEnergyRoute(target, mode)) return true;
        break;
    case Sequence::R1ThenQ:
        if (Engine::ValidEnemy(target) && now - R1CastTick >= 250) {
            if (TryShroudEnergyRoute(target, mode)) return true;
            if (CastQ(target, mode, true)) return true;
        }
        break;
    case Sequence::E2ThenR1:
        if (Engine::ValidEnemy(target) && R1Available() &&
            now - E1CastTick <= 900) {
            if (CastR1(target, mode, false, false)) return true;
        }
        if (TryE2(target, mode)) return true;
        break;
    case Sequence::E2ArrivalBufferQ:
        if (Engine::ValidEnemy(target) && now - E2CastTick >= 55 &&
            now - E2CastTick <= 780) {
            // League buffers Q during E2; this creates the famous E2-Q arrival
            // without pretending that Akali can issue a second movement order.
            if (CastQ(target, mode, true, true)) return true;
        }
        break;
    case Sequence::ShroudAnchor:
        if (ESecondCastAvailable() && CurrentEMark == EMarkKind::Shroud &&
            (mode == Mode::Flee || CurrentPosture == Posture::Escape)) {
            if (TryE2(target, mode)) return true;
        }
        break;
    case Sequence::BackflipEntry:
        if (Engine::ValidEnemy(target) && now - E1CastTick >= 230) {
            if (CastQ(target, mode, true)) return true;
        }
        break;
    default:
        break;
    }
    return false;
}

inline bool TryGapcloserPeel(const AIHeroClient& fallback, Mode mode) {
    const int now = SDK::Variables::TickCount();
    if (GapcloserTargetId == 0 || now > GapcloserExpireTick) return false;
    AIHeroClient threat = HeroByNetworkId(GapcloserTargetId);
    if (!Engine::ValidEnemy(threat)) threat = fallback;
    if (!Engine::ValidEnemy(threat, 825.0f)) return false;
    if (TryDefensiveW(threat, mode)) return true;
    // E aimed at the diver performs a defensive backwards flip and can leave a
    // mark for a later re-entry, but we never auto-take that mark under peel.
    if (E1Available() && SpellEnabled(2, Mode::Automatic)) {
        return CastE1Raw(threat.Position(), threat, Mode::Automatic, false, true);
    }
    if (Engine::ValidEnemy(threat, kQRange + 45.0f)) {
        return CastQ(threat, Mode::Automatic, true, false, false);
    }
    return false;
}

inline bool TryKillSecure(const AIHeroClient& target) {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true) ||
        !Engine::ValidEnemy(target) || TargetCannotBeDamaged(target)) {
        return false;
    }
    if (TryR2Execute(target, Mode::Automatic)) return true;
    if (Engine::ValidEnemy(target, kQEdgeRange) &&
        LethalWith(target, QDamage(target)) &&
        CastQ(target, Mode::Automatic, true, false, true)) {
        return true;
    }
    if (ESecondCastAvailable() &&
        LethalWith(target, E2Damage(target)) && E2DestinationSafe(target, true)) {
        return TryE2(target, Mode::Automatic);
    }
    return false;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || TargetCannotBeDamaged(target)) return false;
    const int now = SDK::Variables::TickCount();
    if (TryR2Execute(target, Mode::Combo)) return true;
    if (TryPassiveWeave(target, Mode::Combo)) return true;
    if (TryE2(target, Mode::Combo)) return true;
    if (TryShroudEnergyRoute(target, Mode::Combo)) return true;

    const bool lethal = LethalWith(target, ShortComboDamage(
        target, E1Available(), R1Available()));
    const float distance = GameObjects::Player().Position().Distance2D(target.Position());
    if (R1Available() && distance <= kR1Range + 18.0f &&
        Bool(ExecutionMenu, "UseR1Engage", true) &&
        (CurrentPosture == Posture::Assassinate || CurrentPosture == Posture::Cleanup ||
         lethal || (distance > kQRange - 40.0f && E1Available()))) {
        if (CastR1(target, Mode::Combo, lethal, false)) return true;
    }

    if (distance <= kQEdgeRange + target.BoundingRadius()) {
        if (CastQ(target, Mode::Combo, Engine::IsHardCrowdControlled(target))) return true;
    }

    // E1 after Q tip/CC is an actual hit route.  Otherwise it is withheld;
    // Akali mains do not donate Shuriken Flip into a free dodge.
    if (E1Available() && (LastQWasTip || Engine::IsHardCrowdControlled(target) ||
        target.IsDashing())) {
        if (CastE1AtTarget(target, Mode::Combo, true)) return true;
    }

    // Specific E-backflip entry: use the flip to move *toward* a target just
    // outside Q only when shroud/R remains available as an exit.
    if (E1Available() && Bool(FlipMenu, "UseBackflipEntry", true) &&
        distance > kQRange && distance <= 790.0f &&
        (WActive || (Engine::RuntimeSpells[1] && Engine::RuntimeSpells[1]->IsReady()) ||
         R1Available() || R2Unlocked()) &&
        !HasPointClickLockdownAt(target.Position())) {
        const Vector3 away = GameObjects::Player().Position() -
            Direction2D(GameObjects::Player().Position(), target.Position()) * 600.0f;
        if (CastE1Raw(away, target, Mode::Combo, false, false)) return true;
    }
    (void)now;
    return false;
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || TargetCannotBeDamaged(target)) return false;
    if (TryPassiveWeave(target, Mode::Harass)) return true;
    // Harass reserves energy for a real escape.  No automatic R, no W for a
    // small trade, and E only after tip slow, CC, dash endpoint, or a committed
    // enemy cast observed by the event stream.
    if (CurrentResource() < QCost() + std::max(ECost(), 30.0f)) return false;
    if (CastQ(target, Mode::Harass, Engine::IsHardCrowdControlled(target))) return true;
    const bool committed = static_cast<int>(target.NetworkId()) == CommittedEnemyId &&
        SDK::Variables::TickCount() <= CommittedEnemyUntil;
    if (E1Available() && Bool(FlipMenu, "HarassOnlyGuaranteedE", true) &&
        (LastQWasTip || Engine::IsHardCrowdControlled(target) || target.IsDashing() || committed)) {
        return CastE1AtTarget(target, Mode::Harass, true);
    }
    return false;
}

inline bool TryFlee(const AIHeroClient& fallback) {
    const auto player = GameObjects::Player();
    const AIHeroClient threat = NearestEnemyToPlayer(fallback, 1300.0f);
    if (TryDefensiveW(threat, Mode::Flee)) return true;
    if (ESecondCastAvailable() && CurrentEMark == EMarkKind::Shroud &&
        TryE2(threat, Mode::Flee)) return true;
    if (TryR2Exit(threat, Mode::Flee, true)) return true;
    if (E1Available() && Engine::ValidEnemy(threat, kERange)) {
        // Cast E at the pursuer, so Akali flips toward the cursor/away from it.
        if (CastE1Raw(threat.Position(), threat, Mode::Flee, false, true)) return true;
    }
    if (R1Available() && Engine::ValidEnemy(threat, kR1Range) &&
        Bool(ExecutionMenu, "UseR1Flee", true)) {
        return CastR1(threat, Mode::Flee, false, true);
    }
    (void)player;
    return false;
}

struct FarmAim {
    Vector3 Direction = {};
    int Hits = 0;
    int LastHits = 0;
    float Score = -FLT_MAX;
    AIBaseClient Primary = {};
};

inline FarmAim BestFarmQ(const std::vector<AIBaseClient>& units, bool lastHitOnly) {
    const auto player = GameObjects::Player();
    FarmAim best{};
    for (const auto& candidate : units) {
        if (!ValidHostileUnit(candidate, kQEdgeRange)) continue;
        const Vector3 predictedCandidate = PredictPosition(candidate, 0.22f);
        const Vec3 direct = Direction2D(player.Position(), predictedCandidate);
        if (direct.IsZero()) continue;
        constexpr float rotations[] = { 0.0f, 9.5f*kPi/180.0f, -9.5f*kPi/180.0f,
                                        5.0f*kPi/180.0f, -5.0f*kPi/180.0f };
        for (float rotation : rotations) {
            const Vec3 direction = Rotate2D(direct, rotation);
            int hits = 0;
            int lastHits = 0;
            float score = 0.0f;
            for (const auto& unit : units) {
                if (!ValidHostileUnit(unit, kQEdgeRange + 60.0f)) continue;
                const auto hit = FivePointHit(player.Position(), direction,
                    PredictPosition(unit, 0.22f), unit.BoundingRadius());
                if (!hit.Hits) continue;
                ++hits;
                score += 1.0f + (hit.TipSlow ? 0.22f : 0.0f);
                const float health = SDK::HealthPrediction::GetPrediction(unit, 310);
                if (health > 0.0f && QDamage(unit) >= health) {
                    ++lastHits;
                    score += 3.5f;
                }
            }
            if (lastHitOnly && lastHits == 0) continue;
            if (score > best.Score) {
                best = { direction, hits, lastHits, score, candidate };
            }
        }
    }
    return best;
}

inline AIBaseClient FindEpicObjective(const std::vector<AIBaseClient>& units) {
    AIBaseClient best = {};
    float bestMaxHealth = 0.0f;
    for (const auto& unit : units) {
        if (!ValidHostileUnit(unit, kERange + 30.0f) || !IsEpicMonster(unit)) continue;
        if (unit.MaxHealth() > bestMaxHealth) {
            best = unit;
            bestMaxHealth = unit.MaxHealth();
        }
    }
    return best;
}

inline bool TryObjectiveE1(const AIBaseClient& objective) {
    if (!Bool(FarmMenu, "ObjectiveE", true) || !E1Available() ||
        !SpellEnabled(2, Mode::Jungle) || !CastThrottleReady(2) ||
        !ValidHostileUnit(objective, kERange + 20.0f) || !IsEpicMonster(objective) ||
        objective.HealthPercent() > Slider(FarmMenu, "ObjectiveMarkHP", 38)) {
        return false;
    }
    const auto player = GameObjects::Player();
    const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(objective);
    if (!prediction.CollisionObjects.empty() ||
        prediction.Hitchance < SDK::HitChance::High) {
        return false;
    }
    const Vector3 castPosition = prediction.GetCastPosition();
    const Vector3 direction = Direction2D(player.Position(), castPosition);
    if (direction.IsZero()) return false;
    const Vector3 backflip = ShurikenBackflipEnd(player.Position(), direction);
    if (!SafePoint(backflip, AIHeroClient{}, 2, false, false)) return false;

    PendingETargetId = static_cast<int>(objective.NetworkId());
    EMarkTargetId = PendingETargetId;
    CurrentEMark = EMarkKind::JungleObjective;
    EMarkExpireTick = SDK::Variables::TickCount() + kEWindowMs;
    PendingEToShroud = false;
    LastEBackflipDestination = backflip;
    LastE1CastPosition = castPosition;
    if (Engine::ControllerCastPosition(2, castPosition)) {
        E1CastTick = SDK::Variables::TickCount();
        return true;
    }
    ClearEMark();
    return false;
}

inline bool TryObjectiveE2() {
    if (!Bool(FarmMenu, "ObjectiveE", true) ||
        CurrentEMark != EMarkKind::JungleObjective || !ESecondCastAvailable() ||
        !Engine::RuntimeSpells[2] || !Engine::RuntimeSpells[2]->IsReady() ||
        !SpellEnabled(2, Mode::Jungle) || !CastThrottleReady(2, true)) {
        return false;
    }
    const AIBaseClient objective = UnitByNetworkId(EMarkTargetId);
    if (!ValidHostileUnit(objective) || !IsEpicMonster(objective)) {
        ClearEMark();
        return false;
    }
    const auto player = GameObjects::Player();
    const int travelMs = std::clamp(
        250 + static_cast<int>(player.Position().Distance2D(objective.Position()) /
                               1500.0f * 1000.0f),
        300, 1800);
    const float predictedHealth = SDK::HealthPrediction::GetPrediction(
        objective, travelMs);
    const float damage = E2Damage(objective);
    if (predictedHealth <= 0.0f || damage < predictedHealth +
        static_cast<float>(Slider(FarmMenu, "ObjectiveDamageBuffer", 35))) {
        return false;
    }
    const int enemies = Engine::CountEnemiesAt(objective.Position(), 700.0f);
    const bool exitReady = WActive || R2Unlocked();
    if (enemies > Slider(FarmMenu, "ObjectiveMaxEnemies", 2) && !exitReady) {
        return false;
    }
    if (!SafePoint(objective.Position(), AIHeroClient{}, 2, true, false)) return false;
    if (Engine::ControllerCastSelf(2)) {
        E2CastTick = SDK::Variables::TickCount();
        ClearEMark();
        return true;
    }
    return false;
}

inline bool TryFarm(bool lastHitOnly) {
    const Mode mode = lastHitOnly ? Mode::LastHit : Mode::LaneClear;
    auto lane = Engine::ClearUnits(false);
    auto jungle = Engine::ClearUnits(true);
    const bool useJungle = lane.empty() && !jungle.empty();
    auto& units = useJungle ? jungle : lane;
    if (units.empty()) return false;

    if (useJungle && TryObjectiveE2()) return true;
    if (useJungle) {
        const AIBaseClient objective = FindEpicObjective(units);
        if (objective.IsValid() && TryObjectiveE1(objective)) return true;
    }
    if (!Engine::RuntimeSpells[0] || !Engine::RuntimeSpells[0]->IsReady() ||
        !SpellEnabled(0, mode) || !CastThrottleReady(0) ||
        CurrentResource() < QCost() + Slider(EnergyMenu, "FarmEnergyReserve", 30)) {
        return false;
    }
    const FarmAim best = BestFarmQ(units, lastHitOnly);
    const int minimum = useJungle ? 1 :
        (lastHitOnly ? 1 : Slider(FarmMenu, "QMinions", 3));
    if (best.Hits < minimum || best.Direction.IsZero() ||
        (lastHitOnly && best.LastHits == 0)) return false;
    const Vector3 endpoint = GameObjects::Player().Position() + best.Direction * kQRange;
    if (Engine::ControllerCastPosition(0, endpoint)) {
        QCastTick = SDK::Variables::TickCount();
        QTargetId = 0;
        LastQWasTip = false;
        return true;
    }
    return false;
}

inline void RefreshState() {
    const auto player = GameObjects::Player();
    const int now = SDK::Variables::TickCount();
    if (!player.IsValid()) return;

    const bool passiveWeapon = player.HasBuff("AkaliPWeapon") ||
        player.HasBuff("akalipweapon");
    if (passiveWeapon) {
        PassiveWeaponReady = true;
        if (PassiveWeaponExpireTick <= now) PassiveWeaponExpireTick = now + kPassiveWeaponMs;
    } else if (PassiveWeaponReady && now > PassiveWeaponExpireTick) {
        PassiveWeaponReady = false;
    }

    if (HasPassiveRingBuff()) {
        if (!PassiveRingActive) {
            AIHeroClient target = HeroByNetworkId(PassiveCandidateTargetId);
            if (!Engine::ValidEnemy(target)) target = HeroByNetworkId(QTargetId);
            if (Engine::ValidEnemy(target)) StartPassiveRing(target, true);
        }
    } else if (PassiveRingActive && now > PassiveRingExpireTick) {
        ClearPassiveRing();
    }
    if (PassiveCandidateExpireTick > 0 && now > PassiveCandidateExpireTick) {
        PassiveCandidateTargetId = 0;
        PassiveCandidateExpireTick = 0;
    }

    if (WActive && now > WExpireTick && !HasStealthShroudBuff()) {
        WActive = false;
        ShroudCenter = {};
        WObjectNetworkId = 0;
    }
    if (HasStealthShroudBuff() && !WActive) {
        WActive = true;
        if (WExpireTick <= now) WExpireTick = now + 2200;
        if (ShroudCenter.IsZero()) ShroudCenter = player.Position();
    }

    if (ESecondCastAvailable()) {
        if (CurrentEMark == EMarkKind::None) {
            if (PendingEToShroud) {
                CurrentEMark = EMarkKind::Shroud;
            } else if (PendingETargetId != 0) {
                CurrentEMark = EMarkKind::Enemy;
                EMarkTargetId = PendingETargetId;
            } else {
                CurrentEMark = EMarkKind::Enemy;
                EMarkTargetId = QTargetId;
            }
            EMarkExpireTick = now + kEWindowMs;
        }
    } else if (CurrentEMark != EMarkKind::None &&
               (EMarkExpireTick == 0 || now > EMarkExpireTick)) {
        ClearEMark();
    }

    const bool rRuntime = RSecondCastAvailable();
    if (rRuntime && !RWindowActive) {
        RWindowActive = true;
        if (R1CastTick <= 0) R1CastTick = now - kR2LockMs;
        if (R2UnlockTick <= 0) R2UnlockTick = R1CastTick + kR2LockMs;
        if (RWindowExpireTick <= now) RWindowExpireTick = R1CastTick + kRWindowMs;
    }
    if (RWindowActive && now > RWindowExpireTick) ClearRWindow();
    if (!rRuntime && RWindowActive && R1CastTick > 0 && now - R1CastTick > 1000 &&
        now < R2UnlockTick) {
        // The recast name can lag for a few frames after R1.  Do not clear the
        // window until the real expiry; R2's 2.5 sec static lock is expected.
    }

    if (GapcloserTargetId != 0 && now > GapcloserExpireTick) GapcloserTargetId = 0;
    if (InterruptTargetId != 0 && now > InterruptExpireTick) InterruptTargetId = 0;
    if (CommittedEnemyId != 0 && now > CommittedEnemyUntil) CommittedEnemyId = 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    RefreshState();
    CurrentPosture = DeterminePosture(selected);
    const AIHeroClient threat = NearestEnemyToPlayer(selected, 1300.0f);

    // Safety responses outrank damage branches.  A fleeing one-trick Akali
    // must retain the player-owned movement and choose a safe E/R route first.
    if (mode == Mode::Flee) {
        (void)TryFlee(selected);
        return true;
    }
    if (TryDefensiveW(threat, Mode::Automatic) || TryGapcloserPeel(selected, mode)) {
        return true;
    }
    if (TryR2Exit(threat, mode)) return true;
    if (TrySequence(selected, mode)) return true;
    if (TryKillSecure(selected)) return true;

    if (mode == Mode::Combo) {
        (void)TryCombo(selected);
        return true;
    }
    if (mode == Mode::Harass) {
        (void)TryHarass(selected);
        return true;
    }
    if (mode == Mode::LaneClear || mode == Mode::LastHit) {
        (void)TryFarm(mode == Mode::LastHit);
        return true;
    }
    if (Key(Engine::AutomaticMenu, "ManualR", false) && Engine::ValidEnemy(selected)) {
        if (R2Unlocked()) {
            (void)CastR2(selected, R2Purpose::Reposition, false, false, true,
                          Mode::Automatic);
        } else if (R1Available()) {
            (void)CastR1(selected, Mode::Automatic, false, false);
        }
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !args.Sender.IsValid()) return;
    const int now = SDK::Variables::TickCount();

    if (!IsLocalPlayer(args.Sender)) {
        const auto threat = ControllerHelpers::AnalyzeEnemyCast(
            args, 220.0f, 110.0f, 250, 280, 260, 1500, 450);
        if (!threat.Valid) return;
        if (threat.Committed) {
            CommittedEnemyId = static_cast<int>(threat.Enemy.NetworkId());
            CommittedEnemyUntil = threat.CommitmentUntilTick;
        }
        if (threat.CrossesPlayer) {
            IncomingThreatUntil = threat.LineThreatUntilTick;
            if (threat.LikelyHardCrowdControl) {
                IncomingHardCCUntil = now + 600;
            }
        }
        return;
    }

    if (args.Slot == 0) {
        QCastTick = now;
        const auto target = HeroByNetworkId(QTargetId);
        if (Engine::ValidEnemy(target)) StartPassiveCandidate(target);
    } else if (args.Slot == 1) {
        WCastTick = now;
        WActive = true;
        if (ShroudCenter.IsZero()) {
            ShroudCenter = args.CastPosition.IsValid() && !args.CastPosition.IsZero()
                ? args.CastPosition : PendingShroudPosition;
        }
        if (WExpireTick <= now) WExpireTick = now + 5000;
    } else if (args.Slot == 2) {
        const bool recast = Engine::TextContains(args.SpellName, "AkaliEb") ||
            ESecondCastAvailable();
        if (recast) {
            E2CastTick = now;
        } else {
            E1CastTick = now;
        }
    } else if (args.Slot == 3) {
        const bool recast = Engine::TextContains(args.SpellName, "AkaliRb") ||
            RSecondCastAvailable();
        if (recast) {
            R2LastCastTick = now;
            ClearRWindow();
        } else {
            RWindowActive = true;
            R1CastTick = now;
            R2UnlockTick = now + kR2LockMs;
            RWindowExpireTick = now + kRWindowMs;
            if (RTargetId == 0) {
                const auto target = Engine::SelectTarget(kR1Range + 50.0f);
                if (Engine::ValidEnemy(target)) RTargetId = static_cast<int>(target.NetworkId());
            }
        }
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int now = SDK::Variables::TickCount();
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "AkaliPWeapon")) {
            PassiveWeaponReady = true;
            PassiveWeaponExpireTick = now +
                ControllerHelpers::RemainingMilliseconds(
                    args.EndTime, kPassiveWeaponMs, 250, 5000);
            ClearPassiveRing();
            ActiveSequence = Sequence::PassiveKama;
            SequenceTargetId = PassiveTargetId;
            SequenceExpireTick = PassiveWeaponExpireTick;
        } else if (Engine::TextContains(args.BuffName, "AkaliPZoneGround")) {
            AIHeroClient target = HeroByNetworkId(PassiveCandidateTargetId);
            if (!Engine::ValidEnemy(target)) target = HeroByNetworkId(QTargetId);
            if (Engine::ValidEnemy(target)) StartPassiveRing(target, true);
        } else if (Engine::TextContains(args.BuffName, "AkaliW")) {
            WActive = true;
            if (WExpireTick <= now) WExpireTick = now + 3000;
        }
        return;
    }
    const auto enemy = Engine::EnemyByNetworkId(static_cast<int>(args.Sender.NetworkId));
    if (Engine::ValidEnemy(enemy) &&
        (Engine::TextContains(args.BuffName, "AkaliE") ||
         Engine::TextContains(args.BuffName, "akaliemark"))) {
        CurrentEMark = EMarkKind::Enemy;
        EMarkTargetId = static_cast<int>(enemy.NetworkId());
        PendingETargetId = EMarkTargetId;
        EMarkExpireTick = now + ControllerHelpers::RemainingMilliseconds(
            args.EndTime, kEWindowMs, 250, 4500);
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "AkaliPWeapon")) {
            PassiveWeaponReady = false;
            PassiveWeaponExpireTick = 0;
        } else if (Engine::TextContains(args.BuffName, "AkaliPZoneGround")) {
            ClearPassiveRing();
        } else if (Engine::TextContains(args.BuffName, "AkaliW")) {
            WActive = false;
            ShroudCenter = {};
        }
        return;
    }
    if (static_cast<int>(args.Sender.NetworkId) == EMarkTargetId &&
        (Engine::TextContains(args.BuffName, "AkaliE") ||
         Engine::TextContains(args.BuffName, "akaliemark"))) {
        if (!ESecondCastAvailable()) ClearEMark();
    }
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "AkaliPWeapon") &&
        args.EndTime > Game::Time()) {
        PassiveWeaponReady = true;
        PassiveWeaponExpireTick = SDK::Variables::TickCount() +
            ControllerHelpers::RemainingMilliseconds(
                args.EndTime, kPassiveWeaponMs, 150, 5000);
    }
    if (static_cast<int>(args.Sender.NetworkId) == EMarkTargetId &&
        Engine::TextContains(args.BuffName, "AkaliE") && args.EndTime > Game::Time()) {
        EMarkExpireTick = SDK::Variables::TickCount() +
            ControllerHelpers::RemainingMilliseconds(
                args.EndTime, kEWindowMs, 150, 4500);
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!HasPassiveWeapon() || !args.Target.IsValid()) return;
    const auto target = HeroByNetworkId(PassiveTargetId);
    if (!Engine::ValidEnemy(target) || !TargetInPassiveAaRange(target)) return;
    // Preserve a prepared kama for its marked champion instead of spending it
    // on a lane minion while the target is already in doubled attack range.
    if (static_cast<int>(args.Target.NetworkId()) != PassiveTargetId &&
        AIMinionClient(args.Target.Address()).IsMinion()) {
        args.Process = false;
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick)) return;
    if (HasPassiveWeapon() &&
        (PassiveTargetId == 0 || static_cast<int>(args.Target.NetworkId()) == PassiveTargetId)) {
        PassiveWeaponReady = false;
        PassiveWeaponExpireTick = 0;
        if (ActiveSequence == Sequence::PassiveKama) {
            ActiveSequence = Sequence::None;
        }
    }
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const bool passiveRing =
        IsPassiveRingSpawnObjectName(args.Sender.Name) ||
        IsPassiveRingSpawnObjectName(args.Sender.CharacterName);
    const bool passiveExpired =
        IsPassiveRingExpiredObjectName(args.Sender.Name) ||
        IsPassiveRingExpiredObjectName(args.Sender.CharacterName);
    const auto player = GameObjects::Player();
    const bool expirationMatchesTrackedRing = PassiveRingActive &&
        PassiveRingOrigin.IsValid() && !PassiveRingOrigin.IsZero() &&
        args.Sender.Position.IsValid() && !args.Sender.Position.IsZero() &&
        args.Sender.Position.Distance2D(PassiveRingOrigin) <= 250.0f;
    if (passiveExpired && expirationMatchesTrackedRing) {
        ClearPassiveRing();
    } else if (passiveRing && player.IsValid() && !HasPassiveWeapon() &&
               args.Sender.Position.IsValid() &&
               !args.Sender.Position.IsZero() &&
               args.Sender.Position.Distance2D(player.Position()) <= 900.0f) {
        PassiveRingObjectId = static_cast<int>(args.Sender.NetworkId);
        PassiveRingOrigin = args.Sender.Position;
        PassiveRingExpireTick = SDK::Variables::TickCount() + kPassiveRingMs;
        PassiveRingActive = true;
        AIHeroClient target = HeroByNetworkId(PassiveCandidateTargetId);
        if (!Engine::ValidEnemy(target)) target = HeroByNetworkId(QTargetId);
        if (Engine::ValidEnemy(target)) {
            PassiveTargetId = static_cast<int>(target.NetworkId());
            ActiveSequence = Sequence::PassiveRingExit;
            SequenceTargetId = PassiveTargetId;
            SequenceExpireTick = PassiveRingExpireTick;
        }
    }
    if ((Engine::TextContains(args.Sender.Name, "AkaliWSmoke") ||
         Engine::TextContains(args.Sender.CharacterName, "AkaliWSmoke")) &&
        SDK::Variables::TickCount() - WCastTick <= 1200) {
        WObjectNetworkId = static_cast<int>(args.Sender.NetworkId);
        if (args.Sender.Position.IsValid() && !args.Sender.Position.IsZero()) {
            ShroudCenter = args.Sender.Position;
        }
        WActive = true;
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    const int objectId = static_cast<int>(args.Sender.NetworkId);
    const bool namedPassiveRing =
        IsPassiveRingObjectName(args.Sender.Name) ||
        IsPassiveRingObjectName(args.Sender.CharacterName);
    const bool matchesTrackedRing = PassiveRingActive &&
        PassiveRingOrigin.IsValid() && !PassiveRingOrigin.IsZero() &&
        args.Sender.Position.IsValid() && !args.Sender.Position.IsZero() &&
        args.Sender.Position.Distance2D(PassiveRingOrigin) <= 250.0f;
    if ((PassiveRingObjectId != 0 && objectId == PassiveRingObjectId) ||
        (namedPassiveRing && matchesTrackedRing)) {
        ClearPassiveRing();
    }
    if (WObjectNetworkId != 0 &&
        objectId == WObjectNetworkId) {
        WObjectNetworkId = 0;
        if (!HasStealthShroudBuff()) {
            WActive = false;
            ShroudCenter = {};
        }
    }
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!ControllerHelpers::MissileEventIsLocal(args)) return;
    if (Engine::TextContains(args.SpellName, "AkaliE") ||
        Engine::TextContains(args.MissileName, "AkaliEMis")) {
        EMissileNetworkId = args.MissileNetworkId != 0
            ? static_cast<int>(args.MissileNetworkId)
            : static_cast<int>(args.Sender.NetworkId);
    }
    if (Engine::TextContains(args.SpellName, "AkaliWSmoke") ||
        Engine::TextContains(args.MissileName, "AkaliWSmoke")) {
        if (args.EndPosition.IsValid() && !args.EndPosition.IsZero()) {
            ShroudCenter = args.EndPosition;
        }
    }
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    if (id == EMissileNetworkId) EMissileNetworkId = 0;
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::Trade: return "trade";
    case Posture::Assassinate: return "assassinate";
    case Posture::Cleanup: return "cleanup";
    case Posture::Stall: return "stall";
    case Posture::Escape: return "escape";
    default: return "neutral";
    }
}

inline const char* EMarkName(EMarkKind mark) {
    switch (mark) {
    case EMarkKind::Enemy: return "enemy";
    case EMarkKind::Shroud: return "shroud";
    case EMarkKind::JungleObjective: return "objective";
    default: return "none";
    }
}

inline void OnDraw() {
    if (!CoachMenu || !GameObjects::Player().IsValid()) return;
    const auto player = GameObjects::Player();
    const int now = SDK::Variables::TickCount();
    if (Bool(CoachMenu, "DrawPassiveRing", true) && PassiveRingActive &&
        PassiveRingOrigin.IsValid() && !PassiveRingOrigin.IsZero()) {
        Drawing::DrawCircle(PassiveRingOrigin, 500.0f, 0xAA4BFFB0u, 1.65f, 72);
        if (PassiveExitCoachPoint.IsValid() && !PassiveExitCoachPoint.IsZero()) {
            Drawing::DrawLine(player.Position(), PassiveExitCoachPoint, 0xFF56FFC2u, 2.0f);
            Drawing::DrawCircle(PassiveExitCoachPoint, 30.0f, 0xFF56FFC2u, 2.0f, 24);
        }
    }
    if (Bool(CoachMenu, "DrawShroud", true) && WActive &&
        ShroudCenter.IsValid() && !ShroudCenter.IsZero()) {
        Drawing::DrawCircle(ShroudCenter, 340.0f, 0xAA8F63FFu, 1.6f, 64);
    }
    if (Bool(CoachMenu, "DrawEBackflip", true) &&
        LastEBackflipDestination.IsValid() && !LastEBackflipDestination.IsZero() &&
        now - E1CastTick <= 1200) {
        Drawing::DrawLine(player.Position(), LastEBackflipDestination, 0xFFFFB14Bu, 2.0f);
        Drawing::DrawCircle(LastEBackflipDestination, 35.0f, 0xFFFFB14Bu, 1.8f, 28);
    }
    if (Bool(CoachMenu, "DrawRRoute", true)) {
        if (LastR1Landing.IsValid() && !LastR1Landing.IsZero() && now - R1CastTick <= 1300) {
            Drawing::DrawCircle(LastR1Landing, 42.0f, 0xFFEC4BFFu, 2.0f, 32);
        }
        if (LastR2Destination.IsValid() && !LastR2Destination.IsZero() &&
            now - R2LastCastTick <= 1200) {
            Drawing::DrawCircle(LastR2Destination, 42.0f, 0xFFFF5489u, 2.0f, 32);
        }
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen = {};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char text[260] = {};
            _snprintf_s(text, sizeof(text), _TRUNCATE,
                "Akali one-trick | %s | energy %.0f | passive %s %.0f | E %s | R2 %s %dms | seq %d",
                PostureName(CurrentPosture), CurrentResource(),
                HasPassiveWeapon() ? "kama" : (PassiveRingActive ? "ring" : "none"),
                PassiveExitRemaining, EMarkName(CurrentEMark),
                R2Unlocked() ? "ready" : "hold",
                RWindowActive ? std::max(0, RWindowExpireTick - now) : 0,
                static_cast<int>(ActiveSequence));
            Drawing::DrawText(screen.x - 190.0f, screen.y - 122.0f,
                              0xFFC9FFE7u, text);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("AkaliOneTrick", "Akali one-trick mechanics"));

    PassiveMenu = TacticsMenu->AddSubMenu(new Menu(
        "PassiveRing", "Assassin's Mark ring and kama weaving"));
    PassiveMenu->Add(new MenuBool("HoldForRingExit", "Hold spells while player", true));
    PassiveMenu->Add(new MenuSeparator("PlayerMovement",
        "The AI coaches the ring-exit"));

    EnergyMenu = TacticsMenu->AddSubMenu(new Menu(
        "EnergyBudget", "Energy-aware Q and Shroud decisions"));
    EnergyMenu->Add(new MenuBool("UseWForEnergyRoute", "W after commitment when it", true));
    EnergyMenu->Add(new MenuSlider("FarmEnergyReserve", "Energy reserved after farm Q", 30, 0, 120));
    EnergyMenu->Add(new MenuSeparator("NoZeroEnergyAllIn",
        "Q is withheld when the route"));

    ShroudMenu = TacticsMenu->AddSubMenu(new Menu(
        "TwilightShroud", "One W: defensive timing and energy battery"));
    ShroudMenu->Add(new MenuBool("UseDefensiveW", "W for incoming line/CC,", true));
    ShroudMenu->Add(new MenuSlider("DefensiveWHP", "Defensive W at own HP (%)", 39, 15, 80));
    ShroudMenu->Add(new MenuBool("SaveVsMordekaiser", "Do not spend offensive W", true));
    ShroudMenu->Add(new MenuBool("DelayIntoReveal", "Do not spend offensive W", true));

    FlipMenu = TacticsMenu->AddSubMenu(new Menu(
        "ShurikenFlip", "E1/E2 route selection"));
    FlipMenu->Add(new MenuBool("HarassOnlyGuaranteedE", "Harass E only after Q tip,", true));
    FlipMenu->Add(new MenuBool("UseBackflipEntry", "Use safe E-backflip entry", true));
    FlipMenu->Add(new MenuBool("UseShroudAnchor", "E marks real routes", true));
    FlipMenu->Add(new MenuSlider("TakeMarkBeforeExpiryMs", "Safe E2 near expiry (ms)", 480, 150, 1100));
    FlipMenu->Add(new MenuSeparator("NoBlindE2",
        "E2 requires visibility, a"));

    ExecutionMenu = TacticsMenu->AddSubMenu(new Menu(
        "PerfectExecution", "R1 commitment and R2 economy"));
    ExecutionMenu->Add(new MenuBool("UseR1Engage", "R1 assassination entry", true));
    ExecutionMenu->Add(new MenuBool("R1BeforeE2", "R1 before a safe E2 when it", true));
    ExecutionMenu->Add(new MenuBool("UseR1Flee", "Use R1 through a threat on", true));
    ExecutionMenu->Add(new MenuBool("UseR2Execute", "R2 execute reserve", true));
    ExecutionMenu->Add(new MenuSlider("R2HealthThreshold", "Nonlethal R2 HP< (%)", 44, 10, 85));
    ExecutionMenu->Add(new MenuSlider("NonlethalR2BeforeExpiryMs", "Spend nonlethal R2 only this", 700, 200, 1800));
    ExecutionMenu->Add(new MenuSlider("ExitBeforeExpiryMs", "Reserve R2 exit this long", 1200, 450, 2500));
    ExecutionMenu->Add(new MenuSlider("AssassinateTargetHP", "Assassinate HP< (%)", 82, 25, 100));
    ExecutionMenu->Add(new MenuSlider("CleanupTargetHP", "Cleanup HP< (%)", 54, 10, 90));
    ExecutionMenu->Add(new MenuSlider("MinCommitHP", "Min HP nonlethal (%)", 30, 10, 80));
    ExecutionMenu->Add(new MenuSlider("EscapeHP", "Escape posture HP (%)", 27, 10, 70));
    ExecutionMenu->Add(new MenuSlider("MaxCommitEnemies", "Max enemies at dest", 2, 1, 5));
    ExecutionMenu->Add(new MenuBool("RespectLockdown", "Reject R/E landings into", true));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "AkaliFarm", "Five Point Strike cone farming"));
    FarmMenu->Add(new MenuSlider("QMinions", "Minimum lane minions hit by Q", 3, 1, 8));
    FarmMenu->Add(new MenuBool("ObjectiveE", "Use E1/E2 as a predicted", true));
    FarmMenu->Add(new MenuSlider("ObjectiveMarkHP", "Mark epic obj HP (%)", 38, 10, 75));
    FarmMenu->Add(new MenuSlider("ObjectiveDamageBuffer", "E2 dmg > predicted HP", 35, 0, 200));
    FarmMenu->Add(new MenuSlider("ObjectiveMaxEnemies", "Max enemies at objective", 2, 0, 5));
    FarmMenu->Add(new MenuSeparator("NoFarmWOrR",
        "W and Perfect Execution are"));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "AkaliCoach", "Live passive, shroud, E, and R route coaching"));
    CoachMenu->Add(new MenuBool("DrawPassiveRing", "Draw confirmed passive ring", false));
    CoachMenu->Add(new MenuBool("DrawShroud", "Draw tracked Shroud center", false));
    CoachMenu->Add(new MenuBool("DrawEBackflip", "Draw E1 backflip dest", false));
    CoachMenu->Add(new MenuBool("DrawRRoute", "Draw R1/R2 destinations", false));
    CoachMenu->Add(new MenuBool("DrawState", "Draw energy/state", false));
}

inline void OnLoad() {
    ActiveSequence = Sequence::None;
    CurrentPosture = Posture::Neutral;
    CurrentEMark = EMarkKind::None;
    LastR2Purpose = R2Purpose::None;
    SequenceTargetId = 0;
    SequenceExpireTick = 0;
    PassiveRingActive = false;
    PassiveWeaponReady = false;
    PassiveTargetId = 0;
    PassiveRingExpireTick = 0;
    PassiveWeaponExpireTick = 0;
    PassiveCandidateTargetId = 0;
    PassiveCandidateExpireTick = 0;
    PassiveRingObjectId = 0;
    PassiveRingOrigin = {};
    PassiveExitCoachPoint = {};
    PassiveExitRemaining = 0.0f;
    QTargetId = 0;
    QCastTick = 0;
    QCastDirection = {};
    LastQWasTip = false;
    WActive = false;
    WCastTick = 0;
    WExpireTick = 0;
    WObjectNetworkId = 0;
    ShroudCenter = {};
    PendingShroudPosition = {};
    LastDefensiveWTick = 0;
    ClearEMark();
    E1CastTick = 0;
    E2CastTick = 0;
    LastEBackflipDestination = {};
    LastE1CastPosition = {};
    EMissileNetworkId = 0;
    ClearRWindow();
    LastR1Landing = {};
    LastR2Destination = {};
    R2LastCastTick = 0;
    LastAutoTargetId = 0;
    LastAutoTick = 0;
    GapcloserTargetId = 0;
    GapcloserExpireTick = 0;
    GapcloserEnd = {};
    InterruptTargetId = 0;
    InterruptExpireTick = 0;
    IncomingThreatUntil = 0;
    IncomingHardCCUntil = 0;
    CommittedEnemyId = 0;
    CommittedEnemyUntil = 0;
    RefreshState();
}

inline void OnUnload() {
    TacticsMenu = nullptr;
    PassiveMenu = nullptr;
    EnergyMenu = nullptr;
    ShroudMenu = nullptr;
    FlipMenu = nullptr;
    ExecutionMenu = nullptr;
    FarmMenu = nullptr;
    CoachMenu = nullptr;
    ActiveSequence = Sequence::None;
}

inline constexpr const char* Scenarios[] = {
    "Read live AkaliPWeapon rather than assuming a passive attack exists",
    "Read the passive-zone buff and calculate the ring center 120 units toward Akali",
    "Use the local passive indicator object as the authoritative ring center when available",
    "Clear passive-ring state from indicator expiry and deletion events",
    "Draw a player-owned ring-exit vector without issuing movement orders",
    "Hold Q/E long enough for a confirmed passive kama in doubled AA range",
    "Preserve a prepared kama from being spent on a minion while its champion target is in range",
    "Release passive hold when the ring/weapon expires instead of freezing combat",
    "Aim Q through five candidate cone rays and use an outer knife only when it buys reach",
    "Recognize Q tip slow as a high-confidence E1 setup",
    "Reject an unconfirmed long-range Q/E against a moving target",
    "Budget energy for a second Q or E escape before an all-in",
    "Use W as a 100-energy battery only after contact, mark, R1, or CC commitment",
    "Never spend W as a rote neutral-lane combo opener",
    "Save offensive W against ready Mordekaiser R",
    "Delay offensive W into an immediate reveal threat",
    "Use W against incoming spell lines, hard CC, multi-enemy pressure, low HP, and flee",
    "Do not spend Q or W during an unrelated dash while preserving the intentional E2-Q buffer",
    "Track local shroud missile/object center for future E-to-shroud routes",
    "Use E1 with collision-aware high prediction after Q tip, CC, dash, or a committed cast",
    "Compute the 400-unit E backflip destination before casting E1",
    "Reject E1 backflips into a wall, turret, excess enemies, or point-click lockdown",
    "Use an intentional safe E-backflip entry only just outside Q range with an exit retained",
    "Mark a shroud with E only when the backflip creates a meaningful route",
    "Track E recast through runtime spell name, target buff, and recast expiry",
    "Never auto-take E2 solely because a target is marked",
    "Require E2 visibility, targetability, a safe landing, and a kill/exit route",
    "Use E2-to-shroud only for flee or genuine emergency return",
    "Buffer Q during E2 arrival for the researched E2-Q branch",
    "Use R1 before E2 only when it improves the return/chase geometry",
    "Calculate R1 landing beyond the target and reject unsafe passes",
    "Use the R1->E1 high-percentage route with fresh post-dash prediction",
    "Use R1->Q when E is unavailable rather than forcing a stale E line",
    "Hold R2 through the 2.5-second static lockout",
    "Use missing-health R2 damage for execution rather than pressing it on unlock",
    "Reject R2 into recently observed hooks, knockups, or point-click lockdown",
    "Select R2 dash candidates that both cross the target and land safely",
    "Reserve R2 as an exit before the 10-second recast window expires",
    "Use R2 toward player cursor only during an actual exit/reposition branch",
    "Allow explicit flee to use W, E-to-shroud, E-backflip, R2 exit, then R1-through-threat",
    "React to directed gapclosers with defensive W or an E backflip, not blind E2 pursuit",
    "Treat interruptable casts as commitment data because Akali lacks a reliable hard interrupt",
    "Use Q cone health prediction for wave and last-hit decisions",
    "Mark only Epic/Legendary monsters with objective E1 below the configured HP gate",
    "Recast objective E2 only when arrival-time health prediction is lethal with damage buffer",
    "Never spend W or either R cast on farming",
    "Re-plan after manual Q/W/E/R while retaining observed buffs, marks, objects, and recast state",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Akali";
    controller.ControllerId = "champion.kuroaio.ai.akali.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIAkali.md";
    controller.ImplementationSummary =
        "Passive-ring/kama cooperation, energy-aware Five Point cone aiming, "
        "one-W shroud economy, collision/safety-aware E routes, E2-Q buffering, "
        "and R1/R2 commitment/execute/exit state machines.";
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
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser =
        &ControllerHelpers::CaptureGapcloserEvent<
            &GapcloserTargetId, &GapcloserEnd,
            &GapcloserExpireTick, 475, 750>;
    // Akali has no reliable hard interrupt; retain the event only as a
    // commitment signal for E/R safety rather than claiming an interrupt.
    controller.OnInterruptable =
        &ControllerHelpers::CaptureInterruptableEvent<
            &InterruptTargetId, &InterruptExpireTick>;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Akali
