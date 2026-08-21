#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../AIMarksmanControllerHelpers.h"
#include "AIViGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Vi {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Ready;
using ControllerHelpers::RemainingMilliseconds;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::UnitByNetworkId;
using MarksmanControllerHelpers::ClearTemporaryOrbwalkerFocus;
using MarksmanControllerHelpers::OwnedOrbwalkerFocus;
using MarksmanControllerHelpers::RedirectBeforeAttackToFocus;
using MarksmanControllerHelpers::SetTemporaryOrbwalkerFocus;

enum class Sequence : int {
  None,
  SafeQEntry,
  DentingFinish,
  AttackReset,
  LockOn,
  Peel,
  Escape,
  Jungle,
};
enum class Posture : int {
  Neutral,
  ShortTrade,
  AllIn,
  FinishDenting,
  Peel,
  Escape,
  Farm
};
enum class UltimateReason : int {
  None,
  Execute,
  Interrupt,
  Peel,
  Isolate
};

struct EnemyTrack {
  int NetworkId = 0;
  int DentingStacks = 0;
  int MarkerExpireTick = 0;
  int ArmorShredExpireTick = 0;
  int LastQualifyingHitTick = 0;
  bool ArmorShredded = false;
};
struct QPlan {
  Vector3 Aim = {}, Endpoint = {};
  int TargetId = 0, FirstCollisionId = 0, EnemiesAtEndpoint = 0;
  float Range = 0.0f;
  SDK::HitChance Hitchance = SDK::HitChance::None;
  bool CollisionOwned = false, EndpointSafe = false, Valid = false;
};
struct RPlan {
  Vector3 Endpoint = {};
  int TargetId = 0, PathEnemies = 0, EnemiesAtEndpoint = 0, AlliedFollowup = 0;
  bool UnderTurret = false, Safe = false, Valid = false;
};

inline Menu *TacticsMenu = nullptr;
inline Menu *ChargeMenu = nullptr;
inline Menu *DentingMenu = nullptr;
inline Menu *ShieldMenu = nullptr;
inline Menu *ForceMenu = nullptr;
inline Menu *UltimateMenu = nullptr;
inline Menu *FarmMenu = nullptr;
inline Menu *CoachMenu = nullptr;
inline Sequence ActiveSequence = Sequence::None;
inline Posture CurrentPosture = Posture::Neutral;
inline UltimateReason LastUltimateReason = UltimateReason::None;
inline Mode LastMode = Mode::None;
inline std::array<EnemyTrack, 16> EnemyTracks = {};
inline int OwnedFocusTargetId = 0, OwnedFocusUntil = 0;
inline bool QChargeObserved = false, QPlayerOwned = false;
inline int QChargeStartTick = 0, QTargetId = 0, LastQCastTick = 0;
inline Vector3 QStartPosition = {}, LastQAim = {}, LastQEndpoint = {};
inline QPlan LastQPlan = {};
inline bool PassiveReady = false, PassiveShieldActive = false;
inline int PassiveShieldExpireTick = 0, PassiveEstimatedReadyTick = 0;
inline int LastPassiveProcTick = 0, LastDentingProcTick = 0;
inline bool EArmed = false;
inline int EAmmo = 0, EMaximumAmmo = 2, EArmedExpireTick = 0, LastECastTick = 0;
inline int LastEmpoweredAttackTick = 0, LastAutoTargetId = 0, LastAutoTick = 0;
inline bool RActive = false;
inline int RTargetId = 0, RCastTick = 0, RLockUntil = 0;
inline RPlan LastRPlan = {};
inline int IncomingThreatUntil = 0, IncomingHardCcUntil = 0;
inline float IncomingDamage = 0.0f;
inline int GapcloserTargetId = 0, GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd = {};
inline int InterruptTargetId = 0, InterruptExpireTick = 0;
inline constexpr int kDentingDurationMs = 4000;
inline constexpr int kShieldDurationMs = 3000;
inline constexpr int kEArmDurationMs = 6000;

inline bool TargetProtected(const AIHeroClient &target) {
  return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
         HasSpellShieldOrImmunity(target) || target.HasBuff("FioraW") ||
         target.HasBuff("VladimirSanguinePool") || target.HasBuff("FizzE") ||
         target.HasBuff("EliseSpiderE") || target.HasBuff("BardRStasis");
}
inline EnemyTrack *FindTrack(int id, bool create = false) {
  if (!id)
    return nullptr;
  for (auto &t : EnemyTracks)
    if (t.NetworkId == id)
      return &t;
  if (!create)
    return nullptr;
  for (auto &t : EnemyTracks)
    if (!t.NetworkId ||
        (t.MarkerExpireTick < Now() && t.ArmorShredExpireTick < Now())) {
      t = {};
      t.NetworkId = id;
      return &t;
    }
  return nullptr;
}
inline int DentingStacks(const AIBaseClient &target) {
  if (!target.IsValid())
    return 0;
  const int observed = std::max({target.GetBuffCount("ViWMarker"),
                                 target.GetBuffCount("viwmarker"),
                                 target.GetBuffCount("ViWProc")});
  if (observed > 0)
    return std::clamp(observed, 0, 2);
  const auto *t = FindTrack(static_cast<int>(target.NetworkId()));
  return t && t->MarkerExpireTick >= Now() ? std::clamp(t->DentingStacks, 0, 2)
                                           : 0;
}
inline bool ArmorShredded(const AIBaseClient &target) {
  if (!target.IsValid())
    return false;
  if (target.HasBuff("ViWProc") || target.HasBuff("viwproc") ||
      target.HasBuff("ViWArmorShred"))
    return true;
  const auto *t = FindTrack(static_cast<int>(target.NetworkId()));
  return t && t->ArmorShredded && t->ArmorShredExpireTick >= Now();
}
inline int RuntimeEAmmo() {
  const auto p = GameObjects::Player();
  if (!p.IsValid())
    return 0;
  const auto spell = p.Spellbook().GetSpell(SDK::SpellSlot::E);
  if (spell.IsValid() && spell.MaxAmmo() > 0) {
    EMaximumAmmo = std::clamp(spell.MaxAmmo(), 1, 2);
    EAmmo = std::clamp(spell.Ammo(), 0, EMaximumAmmo);
  } else
    EAmmo = Ready(2) ? std::max(1, EAmmo) : 0;
  return EAmmo;
}
inline bool RuntimeQCharging() {
  const auto p = GameObjects::Player();
  return Engine::RuntimeSpells[0] &&
         (Engine::RuntimeSpells[0]->IsCharging() ||
          (p.IsValid() && (p.HasBuff("ViQ") || p.HasBuff("ViQLaunch") ||
                           p.Spellbook().IsCharging())));
}
inline float QElapsedSeconds() {
  return QChargeStartTick > 0 ? std::max(0, Now() - QChargeStartTick) / 1000.0f
                              : 0.0f;
}
inline float QDamage(const AIHeroClient &target, float elapsed) {
  const auto p = GameObjects::Player();
  return p.IsValid() && Engine::ValidEnemy(target)
             ? p.CalculatePhysicalDamage(
                   target,
                   QRawDamage(SpellRank(0), p.BonusAttackDamage(), elapsed))
             : 0.0f;
}
inline float RDamage(const AIHeroClient &target) {
  return Engine::RuntimeSpells[3] && Engine::ValidEnemy(target)
             ? Engine::RuntimeSpells[3]->GetDamage(target)
             : 0.0f;
}
inline float WProcDamage(const AIHeroClient &target) {
  const auto p = GameObjects::Player();
  return p.IsValid() && Engine::ValidEnemy(target)
             ? p.CalculatePhysicalDamage(
                   target,
                   DentingBlowsRawDamage(SpellRank(1), p.BonusAttackDamage(),
                                         target.MaxHealth()))
             : 0.0f;
}
using ControllerHelpers::Lethal;

inline std::vector<QCollisionCandidate> QCollisionCandidates(float delay) {
  std::vector<QCollisionCandidate> result;
  for (const auto &e : GameObjects::EnemyHeroes())
    if (Engine::ValidEnemy(e, 1100.0f))
      result.push_back({static_cast<int>(e.NetworkId()),
                        PredictPosition(e, delay), e.BoundingRadius(), true});
  const auto append = [&](const std::vector<AIBaseClient> &units) {
    for (const auto &u : units)
      if (u.IsValid() && !u.IsDead())
        result.push_back({static_cast<int>(u.NetworkId()),
                          PredictPosition(u, delay), u.BoundingRadius(), true});
  };
  append(Engine::ClearUnits(false));
  append(Engine::ClearUnits(true));
  return result;
}
inline bool QEndpointSafe(const Vector3 &endpoint, const AIHeroClient &target,
                          bool lethal, bool fleeing, int *count = nullptr) {
  if (!endpoint.IsValid() || endpoint.IsZero())
    return false;
  const int enemies = Engine::CountEnemiesAt(endpoint, 525.0f);
  if (count)
    *count = enemies;
  if (!QEndpointPolicy(!SDK::NavMesh::IsWall(endpoint),
                       Engine::UnderEnemyTurret(endpoint),
                       Bool(ChargeMenu, "RespectDashHazards", true) &&
                           HasReadyDashHazardAt(endpoint),
                       enemies, Slider(ChargeMenu, "MaxEndpointEnemies", 2),
                       lethal, fleeing))
    return false;
  return fleeing || !target.IsValid() ||
         Engine::PositionDangerScore(endpoint, target,
                                     Engine::ResolvedSpecs[0]) >=
             -static_cast<float>(
                 Slider(ChargeMenu, "MinimumEndpointScore", 1150));
}
inline QPlan BuildQPlan(const AIHeroClient &target, float elapsed,
                        bool fleeing = false) {
  QPlan plan{};
  const auto p = GameObjects::Player();
  if (!p.IsValid())
    return plan;
  plan.Range = QChargeRange(elapsed);
  plan.TargetId = target.IsValid() ? static_cast<int>(target.NetworkId()) : 0;
  Vector3 aim = Game::CursorPos();
  SDK::PredictionOutput prediction{};
  if (Engine::ValidEnemy(target, kQMaximumRange + 180.0f) &&
      Engine::RuntimeSpells[0]) {
    prediction =
        Engine::RuntimeSpells[0]->GetPrediction(target, false, plan.Range);
    aim = prediction.GetCastPosition();
    if (!aim.IsValid() || aim.IsZero())
      aim = PredictPosition(target, 0.10f + plan.Range / 1500.0f);
    plan.Hitchance = prediction.Hitchance;
  } else
    plan.Hitchance = SDK::HitChance::High;
  if (!aim.IsValid() || aim.IsZero())
    return plan;
  plan.Aim = aim;
  plan.Endpoint = QEndpoint(p.Position(), aim, elapsed);
  const auto collision =
      FirstQCollision(p.Position(), plan.Endpoint,
                      QCollisionCandidates(0.10f + plan.Range / 1500.0f));
  if (collision.Hit) {
    plan.FirstCollisionId = collision.Id;
    plan.Endpoint = collision.Contact;
  }
  plan.CollisionOwned = fleeing || !plan.TargetId ||
                        (collision.Hit && collision.Id == plan.TargetId);
  plan.EndpointSafe = QEndpointSafe(
      plan.Endpoint, target,
      target.IsValid() && Lethal(target, QDamage(target, elapsed)), fleeing,
      &plan.EnemiesAtEndpoint);
  plan.Valid =
      plan.CollisionOwned && plan.EndpointSafe &&
      (fleeing || Engine::IsHardCrowdControlled(target) || target.IsDashing() ||
       prediction.Hitchance >= SDK::HitChance::High);
  return plan;
}
inline bool StartQ(const AIHeroClient &target, Mode mode, bool reactive = false,
                   bool fleeing = false) {
  if (RuntimeQCharging() || !Engine::RuntimeSpells[0] ||
      !Engine::RuntimeSpells[0]->IsReady() || !SpellEnabled(0, mode) ||
      !CastThrottleReady(0, 36, reactive ? 0 : -1))
    return false;
  const auto p = GameObjects::Player();
  if (!p.IsValid() || ControllerHelpers::PlayerMobilityLocked() ||
      (!fleeing &&
       (!Engine::ValidEnemy(target, 875.0f) || TargetProtected(target))) ||
      (!reactive && Orbwalker::IsWindingUp()))
    return false;
  if (!fleeing && InAutoAttackRange(target) && DentingStacks(target) >= 2 &&
      Bool(DentingMenu, "FinishThirdHitBeforeQ", true))
    return false;
  const QPlan plan = BuildQPlan(target, kQFullChargeSeconds, fleeing);
  if (!plan.Valid)
    return false;
  Engine::ArmControllerCast(0);
  if (!Engine::RuntimeSpells[0]->StartCharging(plan.Aim)) {
    Engine::CancelControllerCast(0);
    return false;
  }
  Engine::MarkSuccessfulCast(0);
  QChargeObserved = true;
  QPlayerOwned = false;
  QChargeStartTick = LastQCastTick = Now();
  QTargetId = target.IsValid() ? static_cast<int>(target.NetworkId()) : 0;
  QStartPosition = p.Position();
  LastQAim = plan.Aim;
  LastQPlan = plan;
  ActiveSequence = fleeing ? Sequence::Escape
                           : (reactive ? Sequence::Peel : Sequence::SafeQEntry);
  return true;
}
inline bool ReleaseQ(const AIHeroClient &fallback, bool fleeing = false,
                     bool reactive = false) {
  if (!RuntimeQCharging() || QPlayerOwned || !Engine::RuntimeSpells[0])
    return false;
  AIHeroClient target = HeroByNetworkId(QTargetId);
  if (!Engine::ValidEnemy(target, 905.0f))
    target = fallback;
  const float elapsed = QElapsedSeconds();
  QPlan plan = BuildQPlan(target, elapsed, fleeing);
  LastQPlan = plan;
  if (!plan.Valid)
    return false;
  const float distance =
      target.IsValid()
          ? GameObjects::Player().Position().Distance2D(
                PredictPosition(target, 0.10f + plan.Range / 1500.0f))
          : GameObjects::Player().Position().Distance2D(Game::CursorPos());
  const bool inRange =
      distance <=
      plan.Range + (target.IsValid() ? target.BoundingRadius() : 0.0f);
  const bool expiring = elapsed >= 1.23f,
             controlled =
                 target.IsValid() &&
                 (Engine::IsHardCrowdControlled(target) || target.IsDashing());
  const float minimum =
      Slider(ChargeMenu, fleeing ? "FleeMinimumCharge" : "MinimumCharge", 28) /
      100.0f;
  if (!expiring && (!inRange || QChargeFraction(elapsed) < minimum) &&
      !controlled && !reactive)
    return false;
  if (!expiring && target.IsValid() &&
      !Lethal(target, QDamage(target, elapsed)) && DentingStacks(target) >= 2 &&
      !controlled)
    return false;
  Engine::ArmControllerCast(0);
  if (!Engine::RuntimeSpells[0]->ShootChargedSpell(plan.Aim)) {
    Engine::CancelControllerCast(0);
    return false;
  }
  Engine::MarkSuccessfulCast(0);
  LastQCastTick = Now();
  LastQAim = plan.Aim;
  LastQEndpoint = plan.Endpoint;
  QChargeObserved = false;
  QChargeStartTick = QTargetId = 0;
  return true;
}

inline bool CastE(Mode mode, const AIBaseClient &attacked, bool reset,
                  bool farm = false) {
  if (!attacked.IsValid() || RuntimeQCharging() || EArmed ||
      RuntimeEAmmo() <= 0 || !Ready(2) || !SpellEnabled(2, mode) ||
      !CastThrottleReady(2, 28, reset ? 0 : -1))
    return false;
  const auto p = GameObjects::Player();
  if (!EEmpoweredAttackInRange(p.Position().Distance2D(attacked.Position()),
                               p.AttackRange() + attacked.BoundingRadius()) ||
      (!reset && Orbwalker::IsWindingUp()))
    return false;
  if (attacked.IsHero()) {
    const AIHeroClient hero(attacked.Address());
    const bool proc = DentingStacks(hero) >= 2;
    const bool shield = PassiveReady && IncomingThreatUntil >= Now();
    const bool lethal = Engine::RuntimeSpells[2] &&
                        Lethal(hero, Engine::RuntimeSpells[2]->GetDamage(hero) +
                                         (proc ? WProcDamage(hero) : 0.0f));
    if (RuntimeEAmmo() <= 1 && Bool(ForceMenu, "ReserveLastCharge", true) &&
        !proc && !shield && !lethal && mode != Mode::Combo)
      return false;
  } else if (farm) {
    if (mode == Mode::LaneClear &&
        p.ManaPercent() < Slider(FarmMenu, "LaneMana", 48))
      return false;
    if (mode == Mode::LastHit && RuntimeEAmmo() <= 1)
      return false;
  }
  if (!Engine::ControllerCastSelf(2))
    return false;
  LastECastTick = Now();
  EArmed = true;
  EArmedExpireTick = Now() + kEArmDurationMs;
  EAmmo = std::max(0, RuntimeEAmmo() - 1);
  ActiveSequence = farm ? Sequence::Jungle : Sequence::AttackReset;
  return true;
}
inline RPlan BuildRPlan(const AIHeroClient &target, bool defensive) {
  RPlan plan{};
  const auto p = GameObjects::Player();
  if (!p.IsValid() || !Engine::ValidEnemy(target, kRRange) ||
      TargetProtected(target))
    return plan;
  plan.TargetId = static_cast<int>(target.NetworkId());
  const float travel =
      RTravelSeconds(p.Position().Distance2D(target.Position()));
  plan.Endpoint = PredictPosition(target, travel);
  if (!plan.Endpoint.IsValid() || plan.Endpoint.IsZero())
    plan.Endpoint = target.Position();
  for (const auto &e : GameObjects::EnemyHeroes())
    if (Engine::ValidEnemy(e, 1300.0f) && e.NetworkId() != target.NetworkId() &&
        RPathIntersects(p.Position(), plan.Endpoint,
                        PredictPosition(e, travel * 0.5f), e.BoundingRadius()))
      ++plan.PathEnemies;
  plan.EnemiesAtEndpoint = Engine::CountEnemiesAt(plan.Endpoint, 525.0f);
  plan.AlliedFollowup = CountAlliedFollowup(plan.Endpoint, 850.0f, false);
  plan.UnderTurret = Engine::UnderEnemyTurret(plan.Endpoint);
  RPathContext c{};
  c.TargetValid = true;
  c.EndpointWalkable = !SDK::NavMesh::IsWall(plan.Endpoint);
  c.EndpointUnderEnemyTurret = plan.UnderTurret;
  c.EndpointDashHazard = Bool(UltimateMenu, "RespectDashHazards", true) &&
                         HasReadyDashHazardAt(plan.Endpoint);
  c.PlayerUnderEnemyTurret = Engine::UnderEnemyTurret(p.Position());
  c.EnemiesAtEndpoint = plan.EnemiesAtEndpoint;
  c.PathEnemies = plan.PathEnemies;
  c.AlliedFollowup = plan.AlliedFollowup;
  c.MaximumEnemies = Slider(UltimateMenu, "MaxLandingEnemies", 2);
  c.TargetLethal = Lethal(target, RDamage(target));
  c.Defensive = defensive;
  plan.Safe = plan.Valid = RLockOnPathSafe(c);
  return plan;
}
inline bool CastR(const AIHeroClient &target, UltimateReason reason,
                  bool defensive = false) {
  const Mode mode = defensive ? Mode::Flee : Mode::Combo;
  if (!Ready(3) || !SpellEnabled(3, mode) || RuntimeQCharging() ||
      !CastThrottleReady(3, 36))
    return false;
  LastRPlan = BuildRPlan(target, defensive);
  if (!LastRPlan.Valid || !Engine::ControllerCastUnit(3, target))
    return false;
  RCastTick = Now();
  RTargetId = static_cast<int>(target.NetworkId());
  RLockUntil = Now() +
               static_cast<int>(
                   RTravelSeconds(GameObjects::Player().Position().Distance2D(
                       target.Position())) *
                   1000.0f) +
               1400;
  RActive = true;
  LastUltimateReason = reason;
  ActiveSequence =
      reason == UltimateReason::Peel ? Sequence::Peel : Sequence::LockOn;
  ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
  return true;
}

inline void RefreshEnemyTracks() {
  const int now = Now();
  for (auto &t : EnemyTracks) {
    if (!t.NetworkId)
      continue;
    const auto e = HeroByNetworkId(t.NetworkId);
    if (e.IsValid()) {
      const int count =
          std::max(e.GetBuffCount("ViWMarker"), e.GetBuffCount("viwmarker"));
      if (count > 0) {
        t.DentingStacks = std::clamp(count, 0, 2);
        t.MarkerExpireTick = now + kDentingDurationMs;
      }
      if (e.HasBuff("ViWProc") || e.HasBuff("viwproc") ||
          e.HasBuff("ViWArmorShred")) {
        t.ArmorShredded = true;
        t.ArmorShredExpireTick = now + kDentingDurationMs;
      }
    }
    if (t.MarkerExpireTick < now)
      t.DentingStacks = 0;
    if (t.ArmorShredExpireTick < now)
      t.ArmorShredded = false;
  }
}
inline void RefreshState() {
  const int now = Now();
  const auto p = GameObjects::Player();
  const bool charging = RuntimeQCharging();
  if (charging) {
    if (!QChargeStartTick)
      QChargeStartTick = now;
    QChargeObserved = true;
  } else if (QChargeObserved && now - LastQCastTick > 120) {
    QChargeObserved = QPlayerOwned = false;
    QChargeStartTick = QTargetId = 0;
  }
  if (p.IsValid()) {
    PassiveReady = p.HasBuff("ViPassiveReady") ||
                   (!PassiveShieldActive && PassiveEstimatedReadyTick <= now);
    PassiveShieldActive =
        p.HasBuff("ViPassiveBuff") ||
        (PassiveShieldExpireTick >= now && LastPassiveProcTick > 0);
    EArmed = p.HasBuff("ViE") || p.HasBuff("ViEAttack") ||
             p.HasBuff("ViEPunch") || (EArmed && EArmedExpireTick >= now);
    RActive = p.HasBuff("ViR") || p.HasBuff("ViRMissile") ||
              (RActive && RLockUntil >= now);
  }
  (void)RuntimeEAmmo();
  RefreshEnemyTracks();
  if (IncomingThreatUntil < now)
    IncomingDamage = 0.0f;
  if (GapcloserExpireTick < now)
    GapcloserTargetId = 0;
  if (InterruptExpireTick < now)
    InterruptTargetId = 0;
  if (RLockUntil < now) {
    RActive = false;
    RTargetId = 0;
  }
}
inline Posture ChoosePosture(Mode mode, const AIHeroClient &target) {
  if (mode == Mode::Flee)
    return Posture::Escape;
  if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit)
    return Posture::Farm;
  if (GapcloserTargetId || InterruptTargetId)
    return Posture::Peel;
  if (!Engine::ValidEnemy(target))
    return Posture::Neutral;
  if (DentingStacks(target) >= 2 &&
      InAutoAttackRange(target, kEExtraAttackRange))
    return Posture::FinishDenting;
  return mode == Mode::Combo ? Posture::AllIn : Posture::ShortTrade;
}
inline void RefreshDentingFocus(Mode mode) {
  if (mode != Mode::Combo && mode != Mode::Harass) {
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    return;
  }
  auto focus = OwnedOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil, 500.0f);
  if (focus.IsValid() && DentingStacks(focus) > 0 &&
      InAutoAttackRange(focus, kEExtraAttackRange))
    return;
  ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
  AIHeroClient best{};
  int stacks = 0;
  for (const auto &e : GameObjects::EnemyHeroes())
    if (Engine::ValidEnemy(e, 500.0f) &&
        InAutoAttackRange(e, kEExtraAttackRange)) {
      const int s = DentingStacks(e);
      if (s > stacks) {
        best = e;
        stacks = s;
      }
    }
  if (best.IsValid() && stacks > 0 &&
      Bool(DentingMenu, "PreserveTarget", true)) {
    (void)SetTemporaryOrbwalkerFocus(best, 500.0f, kDentingDurationMs,
                                     OwnedFocusTargetId, OwnedFocusUntil);
    ActiveSequence = Sequence::DentingFinish;
  }
}
inline bool TryReactive(const AIHeroClient &target) {
  auto interrupt = HeroByNetworkId(InterruptTargetId);
  if (InterruptExpireTick >= Now() && Engine::ValidEnemy(interrupt, 850.0f) &&
      Bool(ChargeMenu, "InterruptQ", true))
    return RuntimeQCharging() ? ReleaseQ(interrupt, false, true)
                              : StartQ(interrupt, Mode::Automatic, true);
  auto gap = HeroByNetworkId(GapcloserTargetId);
  if (GapcloserExpireTick >= Now() && Engine::ValidEnemy(gap, 750.0f) &&
      Bool(ChargeMenu, "AntiGapcloserQ", true))
    return RuntimeQCharging() ? ReleaseQ(gap, false, true)
                              : StartQ(gap, Mode::Automatic, true);
  if (IncomingHardCcUntil >= Now() && PassiveReady &&
      Engine::ValidEnemy(target, 300.0f) &&
      Bool(ShieldMenu, "PrimeVsHardCC", true))
    return CastE(Mode::Automatic, target, false);
  return false;
}
inline bool TryKillSecure(AIHeroClient target) {
  if (!Engine::ValidEnemy(target, 825.0f))
    target = Engine::SelectTarget(825.0f);
  if (!Engine::ValidEnemy(target) || TargetProtected(target))
    return false;
  if (RuntimeQCharging())
    return Lethal(target, QDamage(target, QElapsedSeconds())) &&
           ReleaseQ(target, false, true);
  if (Ready(0) && Lethal(target, QDamage(target, kQFullChargeSeconds)) &&
      StartQ(target, Mode::Automatic, true))
    return true;
  return Bool(UltimateMenu, "AutomaticExecute", false) && Ready(3) &&
         Lethal(target, RDamage(target)) &&
         CastR(target, UltimateReason::Execute);
}
inline bool TryCombo(const AIHeroClient &target) {
  if (!Engine::ValidEnemy(target) || TargetProtected(target))
    return false;
  if (RuntimeQCharging())
    return ReleaseQ(target);
  if (DentingStacks(target) >= 2 &&
      InAutoAttackRange(target, kEExtraAttackRange))
    return false;
  if (Bool(UltimateMenu, "AutomaticCombo", false) && Ready(3) &&
      (target.HealthPercent() <=
           Slider(UltimateMenu, "AutomaticTargetHP", 35) ||
       Engine::CountEnemiesAt(target.Position(), 600.0f) <= 1) &&
      CastR(target, UltimateReason::Isolate))
    return true;
  return Ready(0) && Bool(ChargeMenu, "UseComboQ", true) &&
         StartQ(target, Mode::Combo);
}
inline bool TryHarass(const AIHeroClient &target) {
  if (!Engine::ValidEnemy(target) || TargetProtected(target) ||
      GameObjects::Player().ManaPercent() <
          Slider(ChargeMenu, "HarassMana", 48))
    return false;
  if (RuntimeQCharging())
    return ReleaseQ(target);
  if (DentingStacks(target) > 0 &&
      InAutoAttackRange(target, kEExtraAttackRange))
    return false;
  return Bool(ChargeMenu, "UseHarassQ", false) && StartQ(target, Mode::Harass);
}
inline bool TryFlee(const AIHeroClient &fallback) {
  const auto pursuer = NearestEnemyToPlayer(fallback, 850.0f);
  return RuntimeQCharging() ? ReleaseQ(pursuer, true)
                            : (Bool(ChargeMenu, "UseFleeQ", true) &&
                               StartQ(pursuer, Mode::Flee, false, true));
}
inline bool TryJungleQ() {
  if (!Bool(FarmMenu, "JungleQ", true) || RuntimeQCharging() ||
      GameObjects::Player().ManaPercent() < Slider(FarmMenu, "JungleMana", 25))
    return false;
  AIBaseClient best{};
  float health = 0.0f;
  for (const auto &u : Engine::ClearUnits(true))
    if (u.IsValid() && !u.IsDead() && u.MaxHealth() > health) {
      best = u;
      health = u.MaxHealth();
    }
  const auto p = GameObjects::Player();
  if (!best.IsValid() ||
      !QEndpointSafe(
          QEndpoint(p.Position(), best.Position(), kQFullChargeSeconds), {},
          false, false) ||
      !Engine::RuntimeSpells[0] || !Engine::RuntimeSpells[0]->IsReady() ||
      !SpellEnabled(0, Mode::Jungle) || !CastThrottleReady(0))
    return false;
  Engine::ArmControllerCast(0);
  if (!Engine::RuntimeSpells[0]->StartCharging(best.Position())) {
    Engine::CancelControllerCast(0);
    return false;
  }
  Engine::MarkSuccessfulCast(0);
  QChargeObserved = true;
  QPlayerOwned = false;
  QChargeStartTick = Now();
  QTargetId = static_cast<int>(best.NetworkId());
  QStartPosition = p.Position();
  LastQAim = best.Position();
  ActiveSequence = Sequence::Jungle;
  return true;
}
inline bool OnUpdate(Mode mode, const AIHeroClient&) {
  LastMode = mode;
  RefreshState();
  const AIHeroClient target = Engine::SelectTarget(950.0f);
  CurrentPosture = ChoosePosture(mode, target);
  RefreshDentingFocus(mode);
  if (QPlayerOwned || RActive)
    return true;
  if (TryReactive(target) || TryKillSecure(target))
    return true;
  if (mode == Mode::Combo)
    (void)TryCombo(target);
  else if (mode == Mode::Harass)
    (void)TryHarass(target);
  else if (mode == Mode::Flee)
    (void)TryFlee(target);
  else if (mode == Mode::Jungle) {
    if (!RuntimeQCharging())
      (void)TryJungleQ();
    else {
      const auto u = UnitByNetworkId(QTargetId);
      if (u.IsValid() && QElapsedSeconds() >= 0.35f) {
        Engine::ArmControllerCast(0);
        if (Engine::RuntimeSpells[0]->ShootChargedSpell(u.Position())) {
          Engine::MarkSuccessfulCast(0);
          QChargeObserved = false;
          QChargeStartTick = QTargetId = 0;
        } else
          Engine::CancelControllerCast(0);
      }
    }
  }
  return true;
}

inline void ObserveQualifyingHit(int id, bool empowered) {
  auto *t = FindTrack(id, true);
  if (!t)
    return;
  const int old = t->MarkerExpireTick >= Now() ? t->DentingStacks : 0;
  const bool proc = DentingBlowsProcs(old, true);
  t->DentingStacks = NextDentingBlowsStacks(old, true);
  t->MarkerExpireTick = Now() + kDentingDurationMs;
  t->LastQualifyingHitTick = Now();
  if (proc) {
    t->ArmorShredded = true;
    t->ArmorShredExpireTick = Now() + kDentingDurationMs;
    LastDentingProcTick = Now();
    PassiveEstimatedReadyTick =
        std::max(Now(), PassiveEstimatedReadyTick - 4000);
  }
  if (empowered) {
    LastEmpoweredAttackTick = Now();
    EArmed = false;
    EArmedExpireTick = 0;
  }
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs &args) {
  if (!args.Sender.IsValid())
    return;
  const int now = Now();
  if (!IsLocalPlayer(args.Sender)) {
    const auto a =
        AnalyzeEnemyCast(args, 220.0f, 110.0f, 220, 280, 260, 1500, 450);
    if (!a.Valid)
      return;
    if (a.TargetsPlayer || a.CrossesPlayer) {
      IncomingThreatUntil = now + 1300;
      IncomingDamage = std::max(
          IncomingDamage, args.IsAutoAttack
                              ? SDK::Damage::GetAutoAttackDamage(
                                    a.Enemy, GameObjects::Player(), true)
                              : GameObjects::Player().MaxHealth() * 0.14f);
    }
    if (a.LikelyHardCrowdControl && (a.TargetsPlayer || a.CrossesPlayer))
      IncomingHardCcUntil = now + 800;
    return;
  }
  const int slot = args.Slot;
  const bool owned = slot >= 0 && slot < 4 && Engine::WasControllerCast(slot);
  int attackTarget = 0, attackTick = 0;
  if (CaptureLocalAutoAttack(args, attackTarget, attackTick)) {
    LastAutoTargetId = attackTarget;
    LastAutoTick = attackTick;
    const bool empowered = ControllerHelpers::TextContainsAny(
        args.SpellName, {"ViEAttack", "ViEPunch", "RelentlessForce"});
    ObserveQualifyingHit(attackTarget, empowered || EArmed);
    return;
  }
  if (slot == 0 || ControllerHelpers::TextContainsAny(
                       args.SpellName, {"ViQ", "ViQLaunch", "ViQDash"})) {
    LastQCastTick = now;
    if (!QChargeStartTick)
      QChargeStartTick = now;
    QChargeObserved = RuntimeQCharging();
    if (!owned)
      QPlayerOwned = true;
    if (args.EndPosition.IsValid() && !args.EndPosition.IsZero())
      LastQAim = args.EndPosition;
  } else if (slot == 2 || ControllerHelpers::TextContainsAny(
                              args.SpellName, {"ViE", "RelentlessForce"})) {
    LastECastTick = now;
    EArmed = true;
    EArmedExpireTick = now + kEArmDurationMs;
  } else if (slot == 3 ||
             ControllerHelpers::TextContainsAny(
                 args.SpellName, {"ViR", "ViRMissile", "ViRDunk"})) {
    RCastTick = now;
    RTargetId = static_cast<int>(args.TargetNetworkId ? args.TargetNetworkId
                                                      : args.Target.NetworkId);
    RActive = true;
    RLockUntil = now + 2600;
  }
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs &args) {
  if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack ||
      !ControllerHelpers::TextContainsAny(args.SpellName,
                                          {"ViEAttack", "ViEPunch"}))
    return;
  const int id = static_cast<int>(args.TargetNetworkId ? args.TargetNetworkId
                                                       : args.Target.NetworkId);
  if (id)
    ObserveQualifyingHit(id, true);
}
inline void UpdateBuffState(const SDK::Events::BuffEventArgs &args,
                            bool added) {
  const int now = Now(), id = static_cast<int>(args.Sender.NetworkId);
  if (IsLocalPlayer(args.Sender)) {
    if (ControllerHelpers::TextContainsAny(args.BuffName, {"ViPassiveReady"})) {
      PassiveReady = added;
      if (added)
        PassiveEstimatedReadyTick = now;
    } else if (ControllerHelpers::TextContainsAny(args.BuffName,
                                                  {"ViPassiveBuff"})) {
      PassiveShieldActive = added;
      PassiveReady = false;
      if (added) {
        LastPassiveProcTick = now;
        PassiveShieldExpireTick =
            now +
            RemainingMilliseconds(args.EndTime, kShieldDurationMs, 0, 3500);
        PassiveEstimatedReadyTick =
            now + 16000 -
            std::min(8, std::clamp(GameObjects::Player().Level(), 1, 18) - 1) *
                500;
      } else
        PassiveShieldExpireTick = 0;
    } else if (ControllerHelpers::TextContainsAny(args.BuffName,
                                                  {"ViQ", "ViQLaunch"})) {
      QChargeObserved = added;
      if (added && !QChargeStartTick)
        QChargeStartTick = now;
      if (!added) {
        QChargeStartTick = 0;
        QPlayerOwned = false;
      }
    } else if (ControllerHelpers::TextContainsAny(
                   args.BuffName, {"ViE", "ViEAttack", "ViEPunch"})) {
      EArmed = added;
      EArmedExpireTick =
          added ? now + RemainingMilliseconds(args.EndTime, kEArmDurationMs, 0,
                                              6500)
                : 0;
    } else if (ControllerHelpers::TextContainsAny(args.BuffName,
                                                  {"ViR", "ViRMissile"})) {
      RActive = added;
      if (!added)
        RTargetId = RLockUntil = 0;
    }
    return;
  }
  auto *t = FindTrack(id, added);
  if (!t)
    return;
  if (ControllerHelpers::TextContainsAny(args.BuffName,
                                         {"ViWMarker", "viwmarker"})) {
    t->DentingStacks = added ? std::clamp(args.Count, 1, 2) : 0;
    t->MarkerExpireTick =
        added ? now + RemainingMilliseconds(args.EndTime, kDentingDurationMs, 0,
                                            4500)
              : 0;
  } else if (ControllerHelpers::TextContainsAny(
                 args.BuffName, {"ViWProc", "viwproc", "ViWArmorShred"})) {
    t->ArmorShredded = added;
    t->ArmorShredExpireTick =
        added ? now + RemainingMilliseconds(args.EndTime, kDentingDurationMs, 0,
                                            4500)
              : 0;
    if (added) {
      t->DentingStacks = 0;
      LastDentingProcTick = now;
      PassiveEstimatedReadyTick =
          std::max(now, PassiveEstimatedReadyTick - 4000);
    }
  }
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs &args) {
  if (RuntimeQCharging() || RActive) {
    args.Process = false;
    return;
  }
  const auto focus =
      OwnedOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil, 500.0f);
  if (focus.IsValid() && DentingStacks(focus) > 0 &&
      RedirectBeforeAttackToFocus(args, focus, kEExtraAttackRange))
    return;
  if (!focus.IsValid())
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs &args) {
  if (!CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick))
    return;
  const AIBaseClient attacked(args.Target.Handle());
  if (!attacked.IsValid() || RuntimeQCharging() || RActive)
    return;
  if (attacked.IsHero() &&
      (LastMode == Mode::Combo || LastMode == Mode::Harass)) {
    (void)CastE(LastMode, attacked, true);
    return;
  }
  if (!attacked.IsHero() &&
      (LastMode == Mode::LaneClear || LastMode == Mode::Jungle ||
       LastMode == Mode::LastHit)) {
    const bool enabled = LastMode == Mode::Jungle
                             ? Bool(FarmMenu, "JungleE", true)
                             : Bool(FarmMenu, "LaneE", true);
    if (enabled)
      (void)CastE(LastMode, attacked, true, true);
  }
}
inline const char *PostureName(Posture p) {
  switch (p) {
  case Posture::ShortTrade:
    return "trade";
  case Posture::AllIn:
    return "all-in";
  case Posture::FinishDenting:
    return "third-hit";
  case Posture::Peel:
    return "peel";
  case Posture::Escape:
    return "escape";
  case Posture::Farm:
    return "farm";
  default:
    return "neutral";
  }
}
inline void OnDraw() {
  if (!CoachMenu)
    return;
  const auto p = GameObjects::Player();
  if (!p.IsValid())
    return;
  if (Bool(CoachMenu, "DrawQRange", false))
    Drawing::DrawCircle(p.Position(),
                        RuntimeQCharging() ? QChargeRange(QElapsedSeconds())
                                           : kQMaximumRange,
                        0xAAFF6FAFu, 1.7f, 64);
  if (Bool(CoachMenu, "DrawQEndpoint", false) && LastQPlan.Endpoint.IsValid() &&
      !LastQPlan.Endpoint.IsZero()) {
    Drawing::DrawLine(p.Position(), LastQPlan.Endpoint,
                      LastQPlan.EndpointSafe ? 0xFF63E6A7u : 0xFFFF5964u, 2.5f);
    Drawing::DrawCircle(LastQPlan.Endpoint, 55.0f,
                        LastQPlan.EndpointSafe ? 0xFF63E6A7u : 0xFFFF5964u,
                        2.0f, 40);
  }
  if (Bool(CoachMenu, "DrawRPath", false) && LastRPlan.Valid) {
    Drawing::DrawLine(p.Position(), LastRPlan.Endpoint, 0xFF9D7BFFu, 3.0f);
    Drawing::DrawCircle(LastRPlan.Endpoint, 100.0f, 0xFF9D7BFFu, 2.0f, 48);
  }
  if (Bool(CoachMenu, "DrawDenting", false))
    for (const auto &e : GameObjects::EnemyHeroes()) {
      const int s = DentingStacks(e);
      if (Engine::ValidEnemy(e, 1000.0f) && s > 0)
        Drawing::DrawCircle(e.Position(), e.BoundingRadius() + 35.0f,
                            s >= 2 ? 0xFFFFD45Eu : 0xAAFF82B7u,
                            s >= 2 ? 3.0f : 1.8f, 40);
    }
  if (Bool(CoachMenu, "DrawState", false)) {
    Vec2 screen{};
    if (Drawing::WorldToScreen(p.Position(), screen)) {
      char text[256]{};
      _snprintf_s(
          text, sizeof(text), _TRUNCATE,
          "Vi one-trick | %s | Q %s %.0f | P %s | E %d/%d | R %d/%d",
          PostureName(CurrentPosture),
          QPlayerOwned ? "player" : (RuntimeQCharging() ? "charge" : "ready"),
          QChargeFraction(QElapsedSeconds()) * 100.0f,
          PassiveShieldActive ? "shield"
                              : (PassiveReady ? "ready" : "cooldown"),
          EAmmo, EMaximumAmmo, LastRPlan.PathEnemies,
          LastRPlan.EnemiesAtEndpoint);
      Drawing::DrawText(screen.x - 245.0f, screen.y - 112.0f, 0xFFFFB4D2u,
                        text);
    }
  }
}

inline void BuildMenu(Menu *root) {
  if (!root)
    return;
  TacticsMenu =
      root->AddSubMenu(new Menu("ViOneTrick", "Vi one-trick mechanics"));
  ChargeMenu = TacticsMenu->AddSubMenu(
      new Menu("VaultBreaker", "Charge and endpoint safety"));
  ChargeMenu->Add(new MenuBool("UseComboQ", "Use Q in combo", true));
  ChargeMenu->Add(new MenuBool("UseHarassQ", "Use Q in harass", false));
  ChargeMenu->Add(
      new MenuBool("UseFleeQ", "Use Q toward cursor on flee", true));
  ChargeMenu->Add(new MenuBool("InterruptQ", "Q interrupt channels", true));
  ChargeMenu->Add(new MenuBool("AntiGapcloserQ", "Q peel gapclosers", true));
  ChargeMenu->Add(
      new MenuSlider("MinimumCharge", "Combat minimum charge (%)", 28, 0, 100));
  ChargeMenu->Add(new MenuSlider("FleeMinimumCharge", "Flee minimum charge (%)",
                                 12, 0, 100));
  ChargeMenu->Add(new MenuSlider("MaxEndpointEnemies",
                                 "Max enemies at Q endpoint", 2, 1, 5));
  ChargeMenu->Add(new MenuSlider("MinimumEndpointScore",
                                 "Reject danger below magnitude", 1150, 400,
                                 2500));
  ChargeMenu->Add(
      new MenuSlider("HarassMana", "Harass Q minimum mana (%)", 48, 20, 90));
  ChargeMenu->Add(
      new MenuBool("RespectDashHazards", "Avoid anti-dash endpoints", true));
  DentingMenu = TacticsMenu->AddSubMenu(
      new Menu("DentingBlows", "Third-hit and armor-shred rules"));
  DentingMenu->Add(
      new MenuBool("PreserveTarget", "Keep target through third hit", true));
  DentingMenu->Add(new MenuBool("FinishThirdHitBeforeQ",
                                "Do not Q away from third hit", true));
  DentingMenu->Add(new MenuSeparator(
      "Armor", "Armor shred and attack speed"));
  ShieldMenu =
      TacticsMenu->AddSubMenu(new Menu("BlastShield", "Passive shield timing"));
  ShieldMenu->Add(new MenuBool("PrimeVsHardCC",
                               "Prime E attack into incoming hard CC", true));
  ShieldMenu->Add(new MenuSeparator(
      "Cooldown",
      "Denting Blows refunds shield CD"));
  ForceMenu = TacticsMenu->AddSubMenu(
      new Menu("RelentlessForce", "Attack reset and charge economy"));
  ForceMenu->Add(
      new MenuBool("ReserveLastCharge", "Reserve last E outside all-in", true));
  ForceMenu->Add(new MenuSeparator(
      "Reset",
      "E follows attack or defense"));
  UltimateMenu = TacticsMenu->AddSubMenu(
      new Menu("CeaseAndDesist", "Lock-on and safe landing"));
  UltimateMenu->Add(
      new MenuBool("AutomaticCombo", "Allow automatic combo R", false));
  UltimateMenu->Add(
      new MenuBool("AutomaticExecute", "Allow automatic lethal R", false));
  UltimateMenu->Add(new MenuSlider(
      "AutomaticTargetHP", "Automatic combo R target HP (%)", 35, 10, 75));
  UltimateMenu->Add(
      new MenuSlider("MaxLandingEnemies", "Max enemies at R landing", 2, 1, 5));
  UltimateMenu->Add(new MenuBool("RespectDashHazards",
                                 "Avoid anti-dash landing zones", true));
  FarmMenu = TacticsMenu->AddSubMenu(
      new Menu("Farm", "E reset and jungle Q ownership"));
  FarmMenu->Add(new MenuBool("LaneE", "Use E reset in lane clear", true));
  FarmMenu->Add(new MenuBool("JungleE", "Use E reset in jungle", true));
  FarmMenu->Add(
      new MenuBool("JungleQ", "Use charged Q on largest monster", true));
  FarmMenu->Add(
      new MenuSlider("LaneMana", "Lane E minimum mana (%)", 48, 15, 90));
  FarmMenu->Add(
      new MenuSlider("JungleMana", "Jungle Q minimum mana (%)", 25, 0, 80));
  CoachMenu = TacticsMenu->AddSubMenu(
      new Menu("Coach", "Vi mechanics visual coaching"));
  CoachMenu->Add(new MenuBool("DrawQRange", "Draw current Q range", false));
  CoachMenu->Add(
      new MenuBool("DrawQEndpoint", "Draw Q endpoint safety", false));
  CoachMenu->Add(new MenuBool("DrawRPath", "Draw R path and landing", false));
  CoachMenu->Add(new MenuBool("DrawDenting", "Mark Denting stacks", false));
  CoachMenu->Add(new MenuBool("DrawState", "Draw state", false));
}
inline void OnLoad() {
  ActiveSequence = Sequence::None;
  CurrentPosture = Posture::Neutral;
  LastUltimateReason = UltimateReason::None;
  LastMode = Mode::None;
  EnemyTracks.fill({});
  OwnedFocusTargetId = OwnedFocusUntil = 0;
  QChargeObserved = QPlayerOwned = false;
  QChargeStartTick = QTargetId = LastQCastTick = 0;
  QStartPosition = LastQAim = LastQEndpoint = {};
  LastQPlan = {};
  PassiveReady = PassiveShieldActive = false;
  PassiveShieldExpireTick = PassiveEstimatedReadyTick = LastPassiveProcTick =
      LastDentingProcTick = 0;
  EArmed = false;
  EAmmo = 0;
  EMaximumAmmo = 2;
  EArmedExpireTick = LastECastTick = LastEmpoweredAttackTick =
      LastAutoTargetId = LastAutoTick = 0;
  RActive = false;
  RTargetId = RCastTick = RLockUntil = 0;
  LastRPlan = {};
  IncomingThreatUntil = IncomingHardCcUntil = 0;
  IncomingDamage = 0.0f;
  GapcloserTargetId = GapcloserExpireTick = 0;
  GapcloserEnd = {};
  InterruptTargetId = InterruptExpireTick = 0;
  RefreshState();
}
inline void OnUnload() {
  ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
  TacticsMenu = ChargeMenu = DentingMenu = ShieldMenu = ForceMenu =
      UltimateMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char *Scenarios[] = {
    "Pin mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Treat W as passive-only and never cast it",
    "Track and poll one/two Denting Blows stacks",
    "Keep orbwalker target through the third hit",
    "Apply Q R and attacks as qualifying W hits",
    "Track the four-second 20 percent armor shred separately",
    "Model current W max-health scaling and 300 monster cap",
    "Model Blast Shield as 12 percent max health for three seconds",
    "Refund four passive-cooldown seconds on W proc",
    "Prime shield only into observed danger and a real hit route",
    "Grow Q linearly from 250 to 725 over 1.25 seconds",
    "Grow Q damage from one to 2.5 times minimum",
    "Use current Q base and bonus-AD ratio",
    "Collect champion minion and monster Q collisions",
    "Require intended target as first collision",
    "Use collision contact as actual Q endpoint",
    "Reject wall hazard turret and over-numbered Q endpoints",
    "Reject unsafe position-danger score",
    "Hold Q until target enters current range",
    "Finish two-stack W target before charging away",
    "Never release a player-started Q",
    "Reconcile Q by event buff and polling",
    "Use same Q safety for interrupt anti-gap and flee",
    "Read E as two-charge post-attack reset",
    "Track E ammo and six-second armed state",
    "Reserve last E outside all-in",
    "Use explicit lane/jungle E and jungle Q modes",
    "Model E 50 range bonus and 535-range 35-degree cone",
    "Treat R as 800-range lock-on",
    "Reject R into protection or invalid landing",
    "Predict followed target at R arrival",
    "Sweep R path for secondary enemies",
    "Count landing enemies and allied follow-up",
    "Reject turret hazard over-numbered and unsupported R paths",
    "Keep automatic R disabled by default",
    "Stop attacks during Q and R movement",
    "Own Combo Harass LaneClear Jungle LastHit Flee Automatic",
    "Never issue movement or Flash input"};
inline constexpr ChampionController Controller = [] {
  ChampionController c{};
  c.ChampionId = SDK::ChampionId::Vi;
  c.ControllerId = "champion.kuroaio.ai.vi.onetrick";
  c.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
  c.ResearchArtifact = "AI/Research/AIVi.md";
  c.ImplementationSummary =
      "Event/poll Denting Blows and Blast Shield economy, first-collision "
      "charged-Q endpoint planning, post-attack two-charge E resets, and "
      "autonomous R path/landing safety across every mode.";
  c.Scenarios = Scenarios;
  c.ScenarioCount = std::size(Scenarios);
  c.OwnsDecisionLoop = true;
  c.OnLoad = &OnLoad;
  c.OnUnload = &OnUnload;
  c.BuildMenu = &BuildMenu;
  c.OnUpdate = &OnUpdate;
  c.OnDraw = &OnDraw;
  c.OnProcessSpell = &OnProcessSpell;
  c.OnDoCast = &OnDoCast;
  c.OnBuffAdd =
      &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
  c.OnBuffRemove =
      &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, false>;

  c.OnBeforeAttack = &OnBeforeAttack;
  c.OnAfterAttack = &OnAfterAttack;
  c.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<
      &GapcloserTargetId, &GapcloserEnd, &GapcloserExpireTick, 650, 900>;
  c.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<
      &InterruptTargetId, &InterruptExpireTick, 900, 120, 2400>;
  return c;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Vi
