#pragma once
#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIJarvanIVGeometry.h"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::JarvanIV {
using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::EnemyFlashReady;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;

struct PassiveRecord {
  int Id = 0;
  int ReadyTick = 0;
};
struct FlagRecord {
  int Id = 0;
  Vector3 Position = {};
  int SpawnTick = 0;
  int ExpireTick = 0;
  bool Confirmed = false;
};
inline Menu *TacticsMenu = nullptr;
inline Menu *ComboMenu = nullptr;
inline Menu *ShieldMenu = nullptr;
inline Menu *UltimateMenu = nullptr;
inline Menu *FarmMenu = nullptr;
inline Menu *CoachMenu = nullptr;
inline std::array<PassiveRecord, 20> PassiveTargets = {};
inline FlagRecord Standard = {};
inline Vector3 PendingFlag = {};
inline int PendingFlagReady = 0;
inline int PendingFlagExpire = 0;
inline int QTick = 0, WTick = 0, ETick = 0, RTick = 0, LastAutoId = 0,
           LastAutoTick = 0;
inline int ThreatUntil = 0, HardCCUntil = 0, ManualUntil = 0;
inline bool ArenaActive = false;
inline Vector3 ArenaCenter = {}, ArenaLanding = {};
inline int ArenaExpire = 0, ArenaTargetId = 0;
inline constexpr int kManualMs = 520, kFlagMs = 8000, kArenaMs = 3500;
using ControllerHelpers::Now;
inline int Rank(int i) {
  return i >= 0 && i < 4 && Engine::RuntimeSpells[i]
             ? std::max(0, Engine::RuntimeSpells[i]->Level())
             : 0;
}
using ControllerHelpers::Ready;
inline bool Throttle(int i, int ms) {
  const int tick = i == 0 ? QTick : i == 1 ? WTick : i == 2 ? ETick : RTick;
  return Now() - tick >= ms;
}
inline bool CannotDamage(const AIHeroClient &t) {
  return !Engine::ValidEnemy(t) || t.IsInvulnerable() ||
         HasSpellShieldOrImmunity(t) || t.HasBuff("VladimirSanguinePool") ||
         t.HasBuff("FizzEIcon") || t.HasBuff("FioraW") || t.HasBuff("KayleR") ||
         t.HasBuff("kindredrnodeathbuff") || t.HasBuff("ChronoShift");
}
inline PassiveRecord *PassiveFor(int id, bool create = false) {
  if (!id)
    return nullptr;
  PassiveRecord *empty = nullptr;
  PassiveRecord *oldest = &PassiveTargets[0];
  for (auto &r : PassiveTargets) {
    if (r.Id == id)
      return &r;
    if (!r.Id && !empty)
      empty = &r;
    if (r.ReadyTick < oldest->ReadyTick)
      oldest = &r;
  }
  if (!create)
    return nullptr;
  auto *result = empty ? empty : oldest;
  *result = {id, 0};
  return result;
}
inline bool PassiveReady(const AIBaseClient &t) {
  if (!t.IsValid())
    return false;
  const auto *r = PassiveFor(static_cast<int>(t.NetworkId()));
  return !r || r->ReadyTick <= Now();
}
inline void SpendPassive(int id) {
  const auto p = GameObjects::Player();
  if (!p.IsValid() || !id)
    return;
  PassiveFor(id, true)->ReadyTick =
      Now() + static_cast<int>(PassiveCooldownSeconds(p.Level()) * 1000.0f);
}
inline float QDamage(const AIHeroClient &t) {
  const auto p = GameObjects::Player();
  return p.IsValid() && t.IsValid()
             ? p.CalculatePhysicalDamage(
                   t, QRawDamage(Rank(0), p.BonusAttackDamage()))
             : 0.0f;
}
inline float EDamage(const AIHeroClient &t) {
  const auto p = GameObjects::Player();
  return p.IsValid() && t.IsValid()
             ? p.CalculateMagicDamage(t, ERawDamage(Rank(2), p.AP()))
             : 0.0f;
}
inline float RDamage(const AIHeroClient &t) {
  const auto p = GameObjects::Player();
  return p.IsValid() && t.IsValid()
             ? p.CalculatePhysicalDamage(
                   t, RRawDamage(Rank(3), p.BonusAttackDamage()))
             : 0.0f;
}
inline float PassiveDamage(const AIHeroClient &t) {
  const auto p = GameObjects::Player();
  return p.IsValid() && t.IsValid() && PassiveReady(t)
             ? p.CalculatePhysicalDamage(t, PassiveRawDamage(t.Health()))
             : 0.0f;
}
using ControllerHelpers::Lethal;
inline bool FlagUsable() {
  return Standard.Position.IsValid() && !Standard.Position.IsZero() &&
         Standard.ExpireTick > Now();
}
inline bool IsStandard(const char *a, const char *b) {
  return (Engine::TextContains(a, "Jarvan") ||
          Engine::TextContains(b, "Jarvan")) &&
         (Engine::TextContains(a, "Standard") ||
          Engine::TextContains(b, "Standard") ||
          Engine::TextContains(a, "Flag") || Engine::TextContains(b, "Flag"));
}
inline bool IsArena(const char *a, const char *b) {
  return (Engine::TextContains(a, "Jarvan") ||
          Engine::TextContains(b, "Jarvan")) &&
         (Engine::TextContains(a, "Cataclysm") ||
          Engine::TextContains(b, "Cataclysm") ||
          Engine::TextContains(a, "Arena") || Engine::TextContains(b, "Arena"));
}
inline void Reconcile() {
  const int now = Now();
  if (Standard.ExpireTick && now >= Standard.ExpireTick)
    Standard = {};
  if (!FlagUsable() && PendingFlagReady && now >= PendingFlagReady &&
      now < PendingFlagExpire)
    Standard = {0, PendingFlag, ETick, PendingFlagExpire, false};
  if (PendingFlagExpire && now >= PendingFlagExpire) {
    PendingFlag = {};
    PendingFlagReady = PendingFlagExpire = 0;
  }
  if (ArenaActive && now >= ArenaExpire) {
    ArenaActive = false;
    ArenaCenter = ArenaLanding = {};
    ArenaTargetId = 0;
  }
  for (auto &r : PassiveTargets)
    if (r.Id && r.ReadyTick + 1000 < now)
      r = {};
}
inline int WalkableSamples(const Vector3 &c) {
  static const std::array<Vec3, 8> d{{{1, 0, 0},
                                          {-1, 0, 0},
                                          {0, 0, 1},
                                          {0, 0, -1},
                                          {.707f, 0, .707f},
                                          {-.707f, 0, .707f},
                                          {.707f, 0, -.707f},
                                          {-.707f, 0, -.707f}}};
  int n = 0;
  for (const auto &v : d)
    if (!SDK::NavMesh::IsWall(c + v * 210.0f))
      ++n;
  return n;
}
inline int TrappedAllies(const Vector3 &c) {
  const auto p = GameObjects::Player();
  int n = 0;
  if (!p.IsValid())
    return n;
  for (const auto &a : GameObjects::AllyHeroes())
    if (Engine::ValidAlly(a) && a.NetworkId() != p.NetworkId() &&
        ArenaContains(c, a.Position(), a.BoundingRadius()))
      ++n;
  return n;
}
inline bool SafeEndpoint(const Vector3 &e, const AIHeroClient &t, bool lethal,
                         bool flee) {
  const auto p = GameObjects::Player();
  if (!p.IsValid() || !e.IsValid() || e.IsZero() || SDK::NavMesh::IsWall(e))
    return false;
  const bool underTurret = Engine::UnderEnemyTurret(e);
  const bool startedUnderTurret = Engine::UnderEnemyTurret(p.Position());
  const bool threatened =
      HasReadyPointClickThreatAt(e) || HasReadyDashHazardAt(e);
  const int nearbyEnemies = Engine::CountEnemiesAt(e, 650);
  const int maximumEnemies = Slider(ComboMenu, "MaxEQEnemies", 3);
  const bool exitAvailable =
      flee || nearbyEnemies <= 1 || Engine::CountAlliesAt(e, 750) > 0;
  if (underTurret && !startedUnderTurret && !lethal) return false;
  if (threatened && !lethal && !flee) return false;
  if (nearbyEnemies > maximumEnemies && !lethal && !flee) return false;
  return exitAvailable &&
         (lethal || Engine::PositionDangerScore(
                        e, t, Engine::ResolvedSpecs[0]) > -10000.0f);
}
inline bool CastQTarget(const AIHeroClient &t, Mode m, bool defensive = false) {
  if (!Ready(0, m) || !Throttle(0, 70) ||
      !Engine::ValidEnemy(t, kQRange + 35) || CannotDamage(t))
    return false;
  const auto p = GameObjects::Player();
  const bool lethal = Lethal(t, QDamage(t));
  if (Orbwalker::IsWindingUp() &&
      Bool(Engine::HumanMenu, "PreserveAttacks", true) && !lethal && !defensive)
    return false;
  if (!defensive && !lethal && PassiveReady(t) && InAutoAttackRange(t, 35))
    return false;
  const auto pred = Engine::RuntimeSpells[0]->GetPrediction(t);
  const auto aim =
      pred.GetCastPosition().IsValid() && !pred.GetCastPosition().IsZero()
          ? pred.GetCastPosition()
          : PredictPosition(t, kQCastDelaySeconds);
  if (!aim.IsValid() || aim.IsZero() ||
      pred.Hitchance <
          (defensive ? SDK::HitChance::Medium : SDK::HitChance::High) ||
      p.Position().Distance2D(aim) > kQRange + t.BoundingRadius())
    return false;
  if (!Engine::ControllerCastPosition(0, aim))
    return false;
  QTick = Now();
  return true;
}
inline bool CastQFlag(const AIHeroClient &t, Mode m, bool flee = false) {
  if (!FlagUsable() || !Ready(0, m) || !Throttle(0, 70))
    return false;
  const auto p = GameObjects::Player();
  const auto aim = QEndpointThroughFlag(p.Position(), Standard.Position);
  if (aim.IsZero() || !QHitsFlag(p.Position(), aim, Standard.Position))
    return false;
  const auto predicted =
      Engine::ValidEnemy(t)
          ? PredictPosition(t,
                            kQCastDelaySeconds +
                                EQDashSeconds(p.Position(), Standard.Position))
          : Vector3{};
  const bool hit = predicted.IsValid() && !predicted.IsZero() &&
                   EQKnocksUp(p.Position(), Standard.Position, predicted,
                              t.BoundingRadius());
  if (!flee && !hit)
    return false;
  const bool lethal = hit && Lethal(t, QDamage(t) + EDamage(t));
  if (!SafeEndpoint(Standard.Position, t, lethal, flee))
    return false;
  if (!Engine::ControllerCastPosition(0, aim))
    return false;
  QTick = Now();
  return true;
}
inline Vector3 ComboFlag(const AIHeroClient &t) {
  const auto p = GameObjects::Player();
  const float travel =
      kFlagCastDelaySeconds +
      p.Position().Distance2D(t.Position()) / kFlagMissileSpeed;
  return ClampFlagPosition(p.Position(), PredictPosition(t, travel), 842);
}
inline bool CastE(const Vector3 &desired, const AIHeroClient &t, Mode m,
                  bool flee) {
  if (!Ready(2, m) || !Throttle(2, 90) ||
      CurrentResource() + .5f < SpellCost(2))
    return false;
  const auto p = GameObjects::Player();
  const auto flag = ClampFlagPosition(p.Position(), desired);
  if (!flag.IsValid() || flag.IsZero() || SDK::NavMesh::IsWall(flag) ||
      p.Position().Distance2D(flag) > kFlagCastRange + 5)
    return false;
  const bool lethal =
      Engine::ValidEnemy(t) && Lethal(t, EDamage(t) + QDamage(t));
  if (!SafeEndpoint(flag, t, lethal, flee) ||
      !Engine::ControllerCastPosition(2, flag))
    return false;
  ETick = Now();
  PendingFlag = flag;
  PendingFlagReady = ETick + 90;
  PendingFlagExpire = ETick + kFlagMs;
  return true;
}
inline bool CastW(const AIHeroClient &t, Mode m, bool defensive = false) {
  if (!Ready(1, m) || !Throttle(1, 90) ||
      CurrentResource() + .5f < SpellCost(1))
    return false;
  const auto p = GameObjects::Player();
  const int enemies = Engine::CountEnemiesAt(p.Position(), kWRadius);
  if (!enemies)
    return false;
  const bool pressure = ThreatUntil > Now() || HardCCUntil > Now() ||
                        defensive ||
                        p.HealthPercent() <= Slider(ShieldMenu, "ShieldHP", 62);
  const bool stick =
      Engine::ValidEnemy(t, kWRadius) && !InAutoAttackRange(t, 20);
  if (!pressure && !stick && enemies < Slider(ShieldMenu, "MinimumEnemies", 2))
    return false;
  if (!Engine::ControllerCastSelf(1))
    return false;
  WTick = Now();
  return true;
}
inline bool RSafe(const AIHeroClient &t, bool lethal, bool defensive) {
  const auto p = GameObjects::Player();
  const auto center = PredictPosition(t, .25f);
  const auto landing = CataclysmLandingEndpoint(
      p.Position(), center, p.BoundingRadius(), t.BoundingRadius());
  ArenaSafetyContext c{};
  c.CenterValid =
      center.IsValid() && !center.IsZero() && !SDK::NavMesh::IsWall(center);
  c.LandingWalkable =
      landing.IsValid() && !landing.IsZero() && !SDK::NavMesh::IsWall(landing);
  c.LandingUnderEnemyTurret = Engine::UnderEnemyTurret(landing);
  c.StartingUnderEnemyTurret = Engine::UnderEnemyTurret(p.Position());
  c.PointClickThreat = HasReadyPointClickThreatAt(landing);
  c.DashHazard = HasReadyDashHazardAt(landing);
  c.Lethal = lethal;
  c.Defensive = defensive;
  c.NearbyEnemies = Engine::CountEnemiesAt(center, kArenaRadius + 250);
  c.NearbyAllies = Engine::CountAlliesAt(center, kArenaRadius + 350);
  c.TrappedAllies = TrappedAllies(center);
  c.WalkableInteriorSamples = WalkableSamples(center);
  c.MaximumEnemies = Slider(UltimateMenu, "MaxArenaEnemies", 3);
  if (!CataclysmSafe(c))
    return false;
  ArenaCenter = center;
  ArenaLanding = landing;
  return true;
}
inline bool CastR(const AIHeroClient &t, Mode m, bool defensive = false) {
  if (ArenaActive || !Ready(3, m) || !Throttle(3, 150) ||
      !Engine::ValidEnemy(t, kRCastRange + 35) || CannotDamage(t))
    return false;
  const bool lethal = Lethal(t, RDamage(t));
  const int hits = Engine::CountEnemiesAt(t.Position(), kArenaRadius);
  const bool multi = hits >= Slider(UltimateMenu, "MinimumTargets", 2);
  const bool allin =
      t.HealthPercent() <= Slider(UltimateMenu, "TargetHP", 55) &&
      Lethal(t, QDamage(t) + EDamage(t) + RDamage(t) + PassiveDamage(t));
  if (!lethal && !multi && !defensive && !allin)
    return false;
  if (EnemyFlashReady(t) && hits <= 1 && !lethal && !defensive)
    return false;
  if (!RSafe(t, lethal, defensive) || !Engine::ControllerCastUnit(3, t))
    return false;
  RTick = Now();
  ArenaExpire = RTick + kArenaMs;
  ArenaTargetId = static_cast<int>(t.NetworkId());
  ArenaActive = true;
  return true;
}
inline bool AllyNeedsExit() {
  if (!ArenaActive)
    return false;
  for (const auto &a : GameObjects::AllyHeroes())
    if (Engine::ValidAlly(a) &&
        ArenaContains(ArenaCenter, a.Position(), a.BoundingRadius()) &&
        a.HealthPercent() < 24 && Engine::CountEnemiesAt(a.Position(), 500) > 0)
      return true;
  return false;
}
inline bool Collapse(Mode m, bool flee) {
  if (!ArenaActive || !Ready(3, m) || Now() - RTick < 250)
    return false;
  const auto t = HeroByNetworkId(ArenaTargetId);
  const bool contained =
      Engine::ValidEnemy(t) &&
      ArenaContains(ArenaCenter, t.Position(), t.BoundingRadius());
  const bool killable =
      Engine::ValidEnemy(t) &&
      t.Health() + t.AllShield() <= QDamage(t) + PassiveDamage(t);
  if (!ShouldCollapseArena(
          true, Now() - RTick, flee, AllyNeedsExit(), contained, killable,
          Engine::CountEnemiesAt(ArenaCenter, kArenaRadius + 200)))
    return false;
  if (!Engine::ControllerCastSelf(3))
    return false;
  ArenaActive = false;
  ArenaExpire = 0;
  return true;
}
inline bool KillSecure(const AIHeroClient &t, Mode m) {
  return Engine::ValidEnemy(t) &&
         ((Lethal(t, QDamage(t)) && CastQTarget(t, m)) ||
          (Lethal(t, RDamage(t)) && CastR(t, m)));
}
inline bool Combo(const AIHeroClient &t) {
  if (!Engine::ValidEnemy(t))
    return false;
  if (ArenaActive && Collapse(Mode::Combo, false))
    return true;
  if (CastW(t, Mode::Combo))
    return true;
  if (PassiveReady(t) && InAutoAttackRange(t, 35))
    return false;
  if (FlagUsable() && CastQFlag(t, Mode::Combo))
    return true;
  const auto p = GameObjects::Player();
  if (Ready(0, Mode::Combo) && Ready(2, Mode::Combo) &&
      p.Position().Distance2D(t.Position()) >
          p.AttackRange() + t.BoundingRadius() &&
      CastE(ComboFlag(t), t, Mode::Combo, false))
    return true;
  if (CastQTarget(t, Mode::Combo))
    return true;
  return CastR(t, Mode::Combo);
}
inline bool Harass(const AIHeroClient &t) {
  if (!Engine::ValidEnemy(t) ||
      CurrentResource() < SpellCost(0) + Slider(FarmMenu, "ManaReserve", 75))
    return false;
  if (PassiveReady(t) && InAutoAttackRange(t, 35))
    return false;
  return CastQTarget(t, Mode::Harass) || CastW(t, Mode::Harass);
}
inline bool Flee(const AIHeroClient &t) {
  if (ArenaActive && Collapse(Mode::Flee, true))
    return true;
  if (FlagUsable() && CastQFlag(t, Mode::Flee, true))
    return true;
  const auto p = GameObjects::Player();
  if (p.IsValid() && Ready(0, Mode::Flee) && Ready(2, Mode::Flee) &&
      CastE(ClampFlagPosition(p.Position(), Game::CursorPos(), 825), t,
            Mode::Flee, true))
    return true;
  return CastW(t, Mode::Flee, true) ||
         (Engine::ValidEnemy(t) && CastQTarget(t, Mode::Flee, true));
}
inline bool OnUpdate(Mode mode, const AIHeroClient &selected) {
  Reconcile();
  if (ManualUntil > Now())
    return true;
  auto target = selected;
  if (!Engine::ValidEnemy(target))
    target = Engine::SelectTarget(kFlagCastRange + 80);
  const auto threat = NearestEnemyToPlayer(target, 1200);
  if (mode == Mode::Flee) {
    (void)Flee(threat);
    return true;
  }
  if (ThreatUntil > Now() && CastW(threat, Mode::Automatic, true))
    return true;
  if (KillSecure(target, mode))
    return true;
  switch (mode) {
  case Mode::Combo:
    (void)Combo(target);
    break;
  case Mode::Harass:
    (void)Harass(target);
    break;
  case Mode::LaneClear:
  case Mode::Jungle:
  case Mode::LastHit:
    if (CurrentResource() >= Slider(FarmMenu, "ManaReserve", 75))
      (void)Engine::TryFarm(mode);
    break;
  case Mode::Automatic:
    if (ArenaActive)
      (void)Collapse(mode, false);
    else if (Engine::ValidEnemy(target) &&
             (ThreatUntil > Now() || HardCCUntil > Now() ||
              Lethal(target, QDamage(target))))
      (void)KillSecure(target, mode);
    break;
  default:
    break;
  }
  return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs &a) {
  const auto p = GameObjects::Player();
  if (!p.IsValid() || !a.Sender.IsValid())
    return;
  const int now = Now();
  if (!IsLocalPlayer(a.Sender)) {
    const auto threat = ControllerHelpers::AnalyzeEnemyCast(
        a, 220, 110, 250, 300, 260, 1500, 450);
    if (threat.Valid && threat.CrossesPlayer) {
      ThreatUntil = std::max(ThreatUntil, threat.LineThreatUntilTick);
      if (threat.LikelyHardCrowdControl)
        HardCCUntil = std::max(HardCCUntil, now + 700);
    }
    return;
  }
  const bool owned =
      a.Slot >= 0 && a.Slot < 4 && Engine::WasControllerCast(a.Slot);
  if (!owned && a.Slot >= 0 && a.Slot < 4)
    ManualUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", kManualMs);
  if (a.Slot == 0)
    QTick = now;
  else if (a.Slot == 1)
    WTick = now;
  else if (a.Slot == 2) {
    ETick = now;
    if (!owned && a.CastPosition.IsValid()) {
      PendingFlag = a.CastPosition;
      PendingFlagReady = now + 90;
      PendingFlagExpire = now + kFlagMs;
    }
  } else if (a.Slot == 3) {
    RTick = now;
    if (ArenaActive) {
      ArenaActive = false;
      ArenaExpire = 0;
    } else {
      ArenaActive = true;
      ArenaExpire = now + kArenaMs;
      ArenaCenter = a.CastPosition;
    }
  }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs &a) {
  if (!a.Sender.IsValid())
    return;
  if (Engine::TextContains(a.BuffName, "MartialCadenceCheck"))
    SpendPassive(static_cast<int>(a.Sender.NetworkId));
  if (Engine::TextContains(a.BuffName, "Cataclysm") &&
      IsLocalPlayer(a.Sender)) {
    ArenaActive = true;
    ArenaExpire = Now() + ControllerHelpers::RemainingMilliseconds(
                              a.EndTime, kArenaMs, 250, 5000);
  }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs &a) {
  if (!a.Sender.IsValid())
    return;
  if (Engine::TextContains(a.BuffName, "MartialCadenceCheck")) {
    auto *r = PassiveFor(static_cast<int>(a.Sender.NetworkId), true);
    r->ReadyTick = Now();
  }
  if (Engine::TextContains(a.BuffName, "Cataclysm") &&
      IsLocalPlayer(a.Sender)) {
    ArenaActive = false;
    ArenaExpire = 0;
  }
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs &a) {
  if (CaptureAfterAttack(a, LastAutoId, LastAutoTick))
    SpendPassive(LastAutoId);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs &a) {
  if (!a.Sender.IsValid())
    return;
  if (IsStandard(a.Sender.Name, a.Sender.CharacterName) &&
      ControllerHelpers::ObjectEventIsAllied(a)) {
    Standard = {static_cast<int>(a.Sender.NetworkId), a.Sender.Position, Now(),
                Now() + kFlagMs, true};
    return;
  }
  if (ArenaActive && IsArena(a.Sender.Name, a.Sender.CharacterName) &&
      a.Sender.Position.IsValid())
    ArenaCenter = a.Sender.Position;
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs &a) {
  if (a.Sender.IsValid() && Standard.Id &&
      static_cast<int>(a.Sender.NetworkId) == Standard.Id)
    Standard = {};
}
inline void OnDraw() {
  const auto p = GameObjects::Player();
  if (!p.IsValid() || !Bool(CoachMenu, "DrawRanges", false))
    return;
  Drawing::DrawCircle(p.Position(), kQRange, 0x33E7C65Bu, 1, 64);
  Drawing::DrawCircle(p.Position(), kFlagCastRange, 0x3339599Bu, 1, 64);
  if (FlagUsable()) {
    Drawing::DrawCircle(Standard.Position, kEQKnockupRadius,
                        Standard.Confirmed ? 0xAAE7C65Bu : 0x667A7A7Au, 2, 40);
    Drawing::DrawLine(p.Position(), Standard.Position, 0xAAE7C65Bu, 2);
  }
  if (ArenaActive && !ArenaCenter.IsZero())
    Drawing::DrawCircle(ArenaCenter, kArenaRadius, 0xAA30549Bu, 2, 64);
}
inline void BuildMenu(Menu *root) {
  if (!root)
    return;
  TacticsMenu = root->AddSubMenu(
      new Menu("JarvanIVOneTrick", "Jarvan IV one-trick mechanics"));
  TacticsMenu->Add(new MenuSlider(
      "ManualOwnershipMs", "Yield after player spell (ms)", 520, 180, 1100));
  ComboMenu = TacticsMenu->AddSubMenu(
      new Menu("FlagCombo", "Demacian Standard / Dragon Strike"));
  ComboMenu->Add(new MenuSlider("MaxEQEnemies",
                                "Maximum enemies at E-Q endpoint", 3, 1, 5));
  ShieldMenu = TacticsMenu->AddSubMenu(
      new Menu("GoldenAegis", "Nearby pressure shield and slow"));
  ShieldMenu->Add(
      new MenuSlider("ShieldHP", "Defensive shield below HP", 62, 10, 95));
  ShieldMenu->Add(new MenuSlider("MinimumEnemies",
                                 "Proactive shield enemy count", 2, 1, 5));
  UltimateMenu = TacticsMenu->AddSubMenu(
      new Menu("Cataclysm", "Arena endpoint and collapse policy"));
  UltimateMenu->Add(
      new MenuSlider("TargetHP", "All-in target HP threshold", 55, 10, 100));
  UltimateMenu->Add(
      new MenuSlider("MinimumTargets", "Minimum arena targets", 2, 1, 5));
  UltimateMenu->Add(
      new MenuSlider("MaxArenaEnemies", "Maximum enemies at landing", 3, 1, 5));
  FarmMenu = TacticsMenu->AddSubMenu(
      new Menu("JarvanFarm", "Passive-first farm delegation"));
  FarmMenu->Add(
      new MenuSlider("ManaReserve", "Mana reserved after farm", 75, 0, 220));
  CoachMenu = TacticsMenu->AddSubMenu(
      new Menu("JarvanCoach", "Flag line and arena coaching"));
  CoachMenu->Add(
      new MenuBool("DrawRanges", "Draw Q/E and arena geometry", false));
}
inline void OnLoad() {
  PassiveTargets = {};
  Standard = {};
  PendingFlag = ArenaCenter = ArenaLanding = {};
  PendingFlagReady = PendingFlagExpire = QTick = WTick = ETick = RTick =
      LastAutoId = LastAutoTick = ThreatUntil = HardCCUntil = ManualUntil =
          ArenaExpire = ArenaTargetId = 0;
  ArenaActive = false;
}
inline void OnUnload() {
  TacticsMenu = ComboMenu = ShieldMenu = UltimateMenu = FarmMenu = CoachMenu =
      nullptr;
  Standard = {};
  PassiveTargets = {};
  ArenaActive = false;
}
inline constexpr const char *Scenarios[] = {
    "Read Riot 26.15 and CommunityDragon PC 16.15 baseline",
    "Track Martial Cadence per target and level-scaled cooldown",
    "Use passive current-health damage, minimum and monster cap",
    "Yield spells to an in-range passive attack",
    "Use Q damage and armor-shred data",
    "Predict direct Q without false unit collision",
    "Track allied Standard objects and reconcile pending casts",
    "Expire flags after eight seconds",
    "Require Q segment contact with the flag pickup radius",
    "Aim Q through rather than at the flag",
    "Predict E-Q knock-up at dash arrival",
    "Reject E-Q missing the 180-radius corridor",
    "Reject wall, turret, lockdown, dash-hazard and outnumbered endpoints",
    "Use W only with nearby enemy pressure",
    "Scale W shield for nearby champions",
    "Use R landing endpoint from collision radii",
    "Reject R center or endpoint in wall terrain",
    "Sample arena interior against existing terrain",
    "Account for allies trapped in the arena",
    "Reject new turret exposure and unsafe landing threats",
    "Use R for lethal, multi-target or verified all-in outcomes",
    "Respect Flash for nonlethal single-target R",
    "Track and expire the 3.5-second arena",
    "Collapse R for flee, ally exit or escaped target",
    "Preserve initial arena cast window",
    "Preserve selected target and orbwalker attacks",
    "Combo owns passive, E-Q, W and R economy",
    "Harass avoids unsolicited E-Q dive",
    "LaneClear, Jungle and LastHit preserve mana",
    "Flee uses cursor flag then Q",
    "Automatic only defends, kill-secures or cleans arena",
    "Yield after manual Q W E or R",
    "Never automate summoners or items",
    "Keep profile and controller responsibilities separate"};
inline constexpr ChampionController Controller = [] {
  ChampionController c{};
  c.ChampionId = SDK::ChampionId::JarvanIV;
  c.ControllerId = "champion.kuroaio.ai.jarvaniv.onetrick";
  c.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
  c.ResearchArtifact = "AI/Research/AIJarvanIV.md";
  c.ImplementationSummary =
      "Per-target passive economy, reconciled flag tracking, prediction and "
      "safety-gated E-Q, pressure shielding and terrain-safe Cataclysm "
      "ownership.";
  c.Scenarios = Scenarios;
  c.ScenarioCount = std::size(Scenarios);
  c.OwnsDecisionLoop = true;
  c.OnLoad = &OnLoad;
  c.OnUnload = &OnUnload;
  c.BuildMenu = &BuildMenu;
  c.OnUpdate = &OnUpdate;
  c.OnDraw = &OnDraw;
  c.OnProcessSpell = &OnProcessSpell;
  c.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoId,
                                                               &LastAutoTick>;
  c.OnBuffAdd = &OnBuffAdd;
  c.OnBuffRemove = &OnBuffRemove;

  c.OnAfterAttack = &OnAfterAttack;
  c.OnObjectCreate = &OnObjectCreate;
  c.OnObjectDelete = &OnObjectDelete;
  return c;
}();
} // namespace Plugins::KuroAIO::AI::Controllers::JarvanIV
