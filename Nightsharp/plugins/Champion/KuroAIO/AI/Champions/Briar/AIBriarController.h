#pragma once
#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIBriarGeometry.h"
#include <algorithm>
#include <array>
#include <cstddef>
namespace Plugins::KuroAIO::AI::Controllers::Briar {
using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptableEvent;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellEnabled;
inline Menu *TacticsMenu = nullptr;
inline Menu *QMenu = nullptr;
inline Menu *WMenu = nullptr;
inline Menu *EMenu = nullptr;
inline Menu *RMenu = nullptr;
inline Menu *FarmMenu = nullptr;
inline Menu *CoachMenu = nullptr;
inline QState CurrentQState = QState::Ready;
inline WState CurrentWState = WState::Ready;
inline EState CurrentEState = EState::Ready;
inline RState CurrentRState = RState::Ready;
inline FrenzyState CurrentFrenzy = FrenzyState::Calm;
inline std::array<int, 4> LastCastTick{};
inline int QCastTick = 0, WCastTick = 0, ECastTick = 0, RCastTick = 0,
           QTargetId = 0, WTargetId = 0, RTargetId = 0, LastAutoTargetId = 0,
           LastAutoTick = 0, ManualOwnershipUntil = 0, IncomingThreatUntil = 0,
           IncomingThreatTargetId = 0, InterruptTargetId = 0,
           InterruptExpireTick = 0, RMissileId = 0;
inline Vector3 IncomingThreatEndpoint{};
inline Mode LastMode = Mode::None;
inline bool Ready(int slot, Mode mode, bool reactive = false) {
  return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
         Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
         (reactive ||
          LastCastTick[static_cast<std::size_t>(slot)] + 45 <= Now());
}
inline bool PreserveAttack(bool reactive) {
  return !reactive && Orbwalker::IsWindingUp() &&
         Bool(Engine::HumanMenu, "PreserveAttacks", true);
}
inline bool Protected(const AIHeroClient &t) {
  return !Engine::ValidEnemy(t) || t.IsInvulnerable() ||
         HasSpellShieldOrImmunity(t);
}
inline bool RuntimeFrenzy() {
  const auto p = GameObjects::Player();
  return p.IsValid() &&
         (p.HasBuff("BriarW") || p.HasBuff("BriarWAttackSpeed") ||
          p.HasBuff("BriarWFrenzyBuff") || p.HasBuff("BriarWFrenzyStateBuff") ||
          p.HasBuff("BriarR") || p.HasBuff("BriarRFrenzyStateBuff"));
}
inline bool RuntimeTaunt() {
  const auto p = GameObjects::Player();
  return p.IsValid() &&
         (p.HasBuff("BriarWTaunt") || p.HasBuff("BriarWTargetLock"));
}
inline bool RuntimeECharge() {
  const auto p = GameObjects::Player();
  return (Engine::RuntimeSpells[2] && Engine::RuntimeSpells[2]->IsCharging()) ||
         (p.IsValid() && (p.HasBuff("BriarE") || p.HasBuff("BriarEDR")));
}
inline bool RuntimeRBerserk() {
  const auto p = GameObjects::Player();
  return p.IsValid() && (p.HasBuff("BriarR") || p.HasBuff("BriarRVictim") ||
                         p.HasBuff("BriarRFrenzyStateBuff"));
}
inline bool Lethal(const AIHeroClient &t, int slot) {
  return Engine::ValidEnemy(t) && slot >= 0 && slot < 4 &&
         Engine::RuntimeSpells[slot] &&
         Engine::RuntimeSpells[slot]->GetDamage(t) >=
             t.Health() + t.AllShield();
}
inline bool Throttle(int slot, int gap = 70) {
  return slot >= 0 && slot < 4 &&
         LastCastTick[static_cast<std::size_t>(slot)] + gap <= Now();
}
inline bool HealthGate(bool emergency = false) {
  const auto p = GameObjects::Player();
  return p.IsValid() &&
         (emergency ||
          p.HealthPercent() > Slider(TacticsMenu, "MinimumHealth", 24));
}
inline bool CastQ(const AIHeroClient &t, Mode m, bool reactive = false) {
  const auto p = GameObjects::Player();
  if (!p.IsValid() || !Engine::ValidEnemy(t, kQRange + 70) ||
      !InRange(p.Position(), t.Position(), kQRange, t.BoundingRadius()) ||
      !QCastAllowed(CurrentQState, CurrentFrenzy, true, Protected(t),
                    ManualOwnershipUntil > Now(), Orbwalker::IsWindingUp()) ||
      !Ready(0, m, reactive) || !Throttle(0) || PreserveAttack(reactive))
    return false;
  if (!Engine::ControllerCastUnit(0, t))
    return false;
  CurrentQState = QState::LeapPending;
  QTargetId = static_cast<int>(t.NetworkId());
  QCastTick = LastCastTick[0] = Now();
  return true;
}
inline bool StartW(const AIHeroClient &t, Mode m, bool reactive = false) {
  const auto p = GameObjects::Player();
  if (!p.IsValid() || !Engine::ValidEnemy(t, kWRange + 80) ||
      !InRange(p.Position(), PredictPosition(t, .10f), kWRange,
               t.BoundingRadius()) ||
      !Ready(1, m, reactive) || !Throttle(1) || PreserveAttack(reactive) ||
      !HealthGate(reactive))
    return false;
  const int n = Engine::CountEnemiesAt(PredictPosition(t, .10f), 260);
  if (!FrenzyCommitSafe(CurrentFrenzy, true, Protected(t),
                        Engine::UnderEnemyTurret(t.Position()), n,
                        Slider(TacticsMenu, "MaximumFrenzyEnemies", 2),
                        Lethal(t, 1), m == Mode::Flee, p.HealthPercent() <= 40))
    return false;
  if (!Engine::ControllerCastUnit(1, t))
    return false;
  CurrentWState = WState::Frenzy;
  CurrentFrenzy = FrenzyState::Frenzy;
  WTargetId = static_cast<int>(t.NetworkId());
  WCastTick = LastCastTick[1] = Now();
  return true;
}
inline bool RecastW(const AIHeroClient &t, Mode m, bool reactive = false) {
  const auto p = GameObjects::Player();
  if (!p.IsValid() || !Engine::ValidEnemy(t, kWRange + 80) ||
      !InRange(p.Position(), t.Position(), kWRange, t.BoundingRadius()) ||
      CurrentWState != WState::Frenzy || !Ready(1, m, reactive) ||
      !Throttle(1, 90) || PreserveAttack(reactive))
    return false;
  if (!WRecastAllowed(
          CurrentWState, Now() - WCastTick, p.HealthPercent(), Lethal(t, 1),
          IncomingThreatUntil > Now() || p.HealthPercent() <= 55, true))
    return false;
  if (!Engine::ControllerCastUnit(1, t))
    return false;
  CurrentWState = WState::RecastPending;
  LastCastTick[1] = Now();
  return true;
}
inline bool CastE(const AIHeroClient &t, Mode m, bool reactive = false,
                  bool fleeing = false) {
  const auto p = GameObjects::Player();
  if (!p.IsValid() || PreserveAttack(reactive))
    return false;
  if (CurrentEState == EState::Charging) {
    const int e = Now() - ECastTick;
    if (!EChargeAllowed(CurrentEState, e, ManualOwnershipUntil > Now(),
                        reactive || IncomingThreatUntil > Now(),
                        Engine::ValidEnemy(t, kERange + 100)) ||
        !EReleaseSafe(CurrentEState, e, false,
                      Engine::UnderEnemyTurret(p.Position()),
                      Engine::CountEnemiesAt(p.Position(), 320),
                      Slider(TacticsMenu, "MaximumReleaseEnemies", 3),
                      reactive || p.HealthPercent() <= 42, fleeing))
      return false;
    if (!Engine::ControllerCastSelf(2))
      return false;
    CurrentEState = EState::ReleasePending;
    LastCastTick[2] = Now();
    return true;
  }
  if (CurrentEState != EState::Ready || !Ready(2, m, reactive) ||
      !Throttle(2, 100) ||
      (!reactive && !fleeing && !Engine::ValidEnemy(t, kERange + 100)))
    return false;
  if (!Engine::ControllerCastSelf(2))
    return false;
  CurrentEState = EState::Charging;
  ECastTick = LastCastTick[2] = Now();
  return true;
}
inline bool FirstRHitIsTarget(const Vec3 &origin, const Vec3 &aim,
                              const AIHeroClient &target) {
  std::array<RCollisionTarget, 8> candidates{};
  int count = 0;
  for (const auto &enemy : GameObjects::EnemyHeroes()) {
    if (count >= static_cast<int>(candidates.size()) ||
        !Engine::ValidEnemy(enemy, kRGlobalRange + 100.0f))
      continue;
    candidates[static_cast<std::size_t>(count++)] = {
        static_cast<int>(enemy.NetworkId()), PredictPosition(enemy, 0.40f),
        enemy.BoundingRadius(), true};
  }
  return FirstRHit(origin, aim, candidates) ==
         static_cast<int>(target.NetworkId());
}
inline bool CastR(const AIHeroClient &t, Mode m, bool reactive = false,
                  bool fleeing = false) {
  const auto p = GameObjects::Player();
  if (!p.IsValid() || !Engine::ValidEnemy(t) ||
      CurrentRState != RState::Ready || !Ready(3, m, reactive) ||
      !Throttle(3, 180) || Protected(t) || PreserveAttack(reactive))
    return false;
  const Vec3 a = PredictPosition(t, .40f);
  if (!a.IsValid() || a.IsZero() ||
      p.Position().Distance2D(a) > kRGlobalRange + t.BoundingRadius() ||
      ControllerHelpers::ProjectileWallBlocksFromPlayer(a, kRWidth * .5f) ||
      SDK::NavMesh::IsWallBetween(p.Position(), a, kRWidth * .5f) ||
      !FirstRHitIsTarget(p.Position(), a, t))
    return false;
  if (!RCommitAllowed(true, false, false, Engine::UnderEnemyTurret(a),
                      Engine::CountEnemiesAt(a, 300),
                      Slider(RMenu, "MaximumBerserkEnemies", 2), Lethal(t, 3),
                      fleeing, p.HealthPercent() <= 36))
    return false;
  if (!Engine::ControllerCastPosition(3, a))
    return false;
  CurrentRState = RState::Traveling;
  RTargetId = static_cast<int>(t.NetworkId());
  RCastTick = LastCastTick[3] = Now();
  return true;
}
inline void Combo(const AIHeroClient &t) {
  if (!Engine::ValidEnemy(t))
    return;
  if (CurrentWState == WState::Frenzy && RecastW(t, Mode::Combo))
    return;
  if (CastQ(t, Mode::Combo))
    return;
  if (StartW(t, Mode::Combo))
    return;
  if (CastE(t, Mode::Combo))
    return;
  (void)CastR(t, Mode::Combo);
}
inline void Harass(const AIHeroClient &t) {
  if (!Engine::ValidEnemy(t))
    return;
  if (CurrentWState == WState::Frenzy && RecastW(t, Mode::Harass))
    return;
  if (CastQ(t, Mode::Harass))
    return;
  (void)StartW(t, Mode::Harass);
}
inline void Farm(Mode m, const AIHeroClient &t) {
  const auto p = GameObjects::Player();
  if (!p.IsValid() || p.HealthPercent() < Slider(FarmMenu, "MinimumHealth", 38))
    return;
  if (Engine::ValidEnemy(t, kQRange) && CastQ(t, m))
    return;
  (void)Engine::TryFarm(m);
}
inline void Flee(const AIHeroClient &t) {
  if (CastE(t, Mode::Flee, true, true))
    return;
  if (CurrentWState == WState::Frenzy && Engine::ValidEnemy(t) &&
      RecastW(t, Mode::Flee, true))
    return;
  if (Engine::ValidEnemy(t))
    (void)CastR(t, Mode::Flee, true, true);
}
inline void Automatic(const AIHeroClient &t) {
  if (IncomingThreatUntil > Now() && CastE(t, Mode::Automatic, true))
    return;
  const auto p = GameObjects::Player();
  if (p.IsValid() && p.HealthPercent() <= 36 &&
      CurrentWState == WState::Frenzy && RecastW(t, Mode::Automatic, true))
    return;
  if (Engine::ValidEnemy(t)) {
    if (CurrentWState == WState::Frenzy && RecastW(t, Mode::Automatic, true))
      return;
    (void)StartW(t, Mode::Automatic, true);
  }
}
inline void ReconcileState() {
  const auto p = GameObjects::Player();
  if (!p.IsValid())
    return;
  const int n = Now();
  if (RuntimeTaunt())
    CurrentFrenzy = FrenzyState::Taunt;
  else if (RuntimeFrenzy())
    CurrentFrenzy = FrenzyState::Frenzy;
  else if (CurrentFrenzy != FrenzyState::Calm &&
           n > WCastTick + kWFrenzyDurationMs + 500)
    CurrentFrenzy = FrenzyState::Calm;
  if (RuntimeFrenzy() && CurrentWState == WState::Ready)
    CurrentWState = WState::Frenzy;
  if (CurrentWState != WState::Ready && n > WCastTick + kWRecastWindowMs + 400)
    CurrentWState = WState::Ready;
  if (RuntimeECharge() && CurrentEState == EState::Ready) {
    CurrentEState = EState::Charging;
    ECastTick = n;
  }
  if (!RuntimeECharge() && CurrentEState != EState::Ready &&
      n > ECastTick + kEChargeMaxMs + 500)
    CurrentEState = EState::Ready;
  if (RuntimeRBerserk())
    CurrentRState = RState::Berserk;
  else if (CurrentRState != RState::Ready &&
           n > RCastTick + kRTravelTimeoutMs + kRBerserkDurationMs)
    CurrentRState = RState::Ready;
  if (CurrentQState != QState::Ready && n > QCastTick + kQFollowWindowMs + 250)
    CurrentQState = QState::Ready;
  if (IncomingThreatUntil <= n) {
    IncomingThreatUntil = 0;
    IncomingThreatTargetId = 0;
    IncomingThreatEndpoint = {};
  }
  if (InterruptExpireTick <= n)
    InterruptTargetId = 0;
}
inline bool OnUpdate(Mode m, const AIHeroClient &selected) {
  LastMode = m;
  ReconcileState();
  if (ManualOwnershipUntil > Now())
    return true;
  const auto t = ControllerHelpers::PreferredEnemyTarget(
      selected, m == Mode::Flee ? kRGlobalRange : kWRange + 80);
  if (InterruptTargetId && InterruptExpireTick > Now() &&
      Engine::ValidEnemy(t) && CastQ(t, m, true))
    return true;
  switch (m) {
  case Mode::Combo:
    Combo(t);
    break;
  case Mode::Harass:
    Harass(t);
    break;
  case Mode::LaneClear:
  case Mode::Jungle:
  case Mode::LastHit:
    Farm(m, t);
    break;
  case Mode::Flee:
    Flee(t);
    break;
  case Mode::Automatic:
    Automatic(t);
    break;
  default:
    break;
  }
  return true;
}
inline void BuildMenu(Menu *root) {
  if (!root)
    return;
  TacticsMenu =
      root->AddSubMenu(new Menu("BriarFrenzy", "Briar frenzy and safety"));
  QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Head Rush"));
  WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Blood Frenzy / Snack Attack"));
  EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Chilling Scream"));
  RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Certain Death"));
  FarmMenu = TacticsMenu->AddSubMenu(new Menu("Farm", "Health-safe farming"));
  CoachMenu = TacticsMenu->AddSubMenu(new Menu("Coach", "Visual coaching"));
  TacticsMenu->Add(new MenuSlider("ManualOwnershipMs",
                                  "Manual cast protection (ms)", 650, 0, 2000));
  TacticsMenu->Add(new MenuSlider("MaximumFrenzyEnemies",
                                  "Maximum enemies for W frenzy", 2, 0, 5));
  TacticsMenu->Add(new MenuSlider("MaximumReleaseEnemies",
                                  "Maximum enemies for E release", 3, 0, 5));
  TacticsMenu->Add(
      new MenuSlider("MinimumHealth", "Minimum health percent", 24, 5, 80));
  RMenu->Add(new MenuSlider("MaximumBerserkEnemies", "Maximum enemies after R",
                            2, 0, 5));
  FarmMenu->Add(
      new MenuSlider("MinimumHealth", "Farm health percent", 38, 10, 90));
  CoachMenu->Add(new MenuBool("DrawRanges", "Draw Briar ranges", false));
}
inline void OnLoad() {
  CurrentQState = QState::Ready;
  CurrentWState = WState::Ready;
  CurrentEState = EState::Ready;
  CurrentRState = RState::Ready;
  CurrentFrenzy = FrenzyState::Calm;
  QCastTick = WCastTick = ECastTick = RCastTick = 0;
  QTargetId = WTargetId = RTargetId = 0;
  LastAutoTargetId = LastAutoTick = ManualOwnershipUntil = 0;
  IncomingThreatUntil = IncomingThreatTargetId = InterruptTargetId =
      InterruptExpireTick = 0;
  IncomingThreatEndpoint = {};
  RMissileId = 0;
  LastMode = Mode::None;
  LastCastTick.fill(0);
}
inline void OnUnload() { OnLoad(); }
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs &a) {
  if (!a.Sender.IsValid())
    return;
  const int n = Now();
  if (IsLocalPlayer(a.Sender)) {
    if (a.Slot < 0 || a.Slot > 3)
      return;
    if (!Engine::WasControllerCast(a.Slot))
      ManualOwnershipUntil =
          n + static_cast<int>(Slider(TacticsMenu, "ManualOwnershipMs", 650));
    LastCastTick[static_cast<std::size_t>(a.Slot)] = n;
    if (a.Slot == 0) {
      CurrentQState = QState::LeapPending;
      QCastTick = n;
    } else if (a.Slot == 1) {
      CurrentWState = CurrentWState == WState::Frenzy ? WState::RecastPending
                                                      : WState::Frenzy;
      CurrentFrenzy = FrenzyState::Frenzy;
      WCastTick = n;
    } else if (a.Slot == 2) {
      CurrentEState = CurrentEState == EState::Charging ? EState::ReleasePending
                                                        : EState::Charging;
      ECastTick = n;
    } else if (a.Slot == 3) {
      CurrentRState = RState::Traveling;
      RCastTick = n;
    }
    return;
  }
  const auto x = AnalyzeEnemyCast(a);
  if (x.Valid && (x.TargetsPlayer || x.CrossesPlayer)) {
    IncomingThreatTargetId = static_cast<int>(a.Sender.NetworkId);
    IncomingThreatUntil =
        std::max(x.CommitmentUntilTick, x.LineThreatUntilTick);
    IncomingThreatEndpoint = a.EndPosition;
  }
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs &a) {
  (void)CaptureLocalAutoAttack(a, LastAutoTargetId, LastAutoTick);
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs &a) {
  if (!a.Sender.IsValid() || !IsLocalPlayer(a.Sender))
    return;
  if (Engine::TextContains(a.BuffName, "BriarW") ||
      Engine::TextContains(a.BuffName, "BriarR")) {
    CurrentFrenzy = RuntimeTaunt() ? FrenzyState::Taunt : FrenzyState::Frenzy;
    CurrentWState = WState::Frenzy;
  }
  if (Engine::TextContains(a.BuffName, "BriarE")) {
    CurrentEState = EState::Charging;
    ECastTick = Now();
  }
  if (Engine::TextContains(a.BuffName, "BriarR")) {
    CurrentRState = RState::Berserk;
    RCastTick = Now();
  }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs &a) {
  if (!a.Sender.IsValid() || !IsLocalPlayer(a.Sender))
    return;
  if (Engine::TextContains(a.BuffName, "BriarW")) {
    CurrentWState = WState::Ready;
    CurrentFrenzy = FrenzyState::Calm;
  }
  if (Engine::TextContains(a.BuffName, "BriarE"))
    CurrentEState = EState::Ready;
  if (Engine::TextContains(a.BuffName, "BriarR"))
    CurrentRState = RState::Ready;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs &a) {
  if (!a.Target.IsValid())
    return;
  LastAutoTargetId = static_cast<int>(a.Target.NetworkId());
  LastAutoTick = Now();
  if (CurrentFrenzy == FrenzyState::Taunt &&
      Bool(WMenu, "PreserveTauntTarget", true))
    a.Process = false;
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs &a) {
  (void)CaptureAfterAttack(a, LastAutoTargetId, LastAutoTick);
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs &a) {
  (void)CaptureGapcloser(a, IncomingThreatTargetId, IncomingThreatEndpoint,
                         IncomingThreatUntil, kERange + 80, 1000);
}
inline void OnInterruptable(
    const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs &a) {
  CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 900, 250,
                            5000>(a);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs &) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs &) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs &a) {
  if (ControllerHelpers::AnyTextContains({a.SpellName, a.MissileName},
                                         {"briarr", "certaindeath"})) {
    RMissileId = a.MissileNetworkId ? static_cast<int>(a.MissileNetworkId)
                                    : static_cast<int>(a.Sender.NetworkId);
    CurrentRState = RState::Traveling;
    RCastTick = Now();
  }
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs &a) {
  const int id = a.MissileNetworkId ? static_cast<int>(a.MissileNetworkId)
                                    : static_cast<int>(a.Sender.NetworkId);
  if (id == RMissileId)
    RMissileId = 0;
}
inline void OnDraw() {
  if (!Bool(CoachMenu, "DrawRanges", false))
    return;
  const auto p = GameObjects::Player();
  if (!p.IsValid())
    return;
  Drawing::DrawCircle(p.Position(), kQRange, 0xFFDD5577u, 1.2f, 40);
  Drawing::DrawCircle(p.Position(), kWRange, 0xFFCC8844u, 1.2f, 40);
  Drawing::DrawCircle(p.Position(), kERange, 0xFF55AADDu, 1.2f, 40);
}
inline constexpr const char *Scenarios[] = {
    "Reconcile Crimson Curse health model and manual spell ownership",
    "Track Blood Frenzy, taunt target lock and Snack Attack recast healing",
    "Use Head Rush leap reach, attack reset and stun timing",
    "Preserve ordinary attack windup unless reactive stun is justified",
    "Charge Chilling Scream for damage reduction and release knockback",
    "Apply E wall, turret and enemy-count safety boundaries",
    "Launch Certain Death only through predicted global collision and target "
    "gates",
    "Reject unsafe berserk commits into turrets or excessive enemies",
    "Use health, cooldown, protected-target and lethal resource gates",
    "Support Combo Harass LaneClear Jungle LastHit Flee and Automatic modes",
    "Reconcile Q/W/E/R state through process-spell, buff, missile and polling "
    "callbacks",
    "Keep geometry formulas and boundaries SDK-independent"};
inline constexpr ChampionController Controller = [] {
  ChampionController c{};
  c.ChampionId = SDK::ChampionId::Briar;
  c.ControllerId = "champion.kuroaio.ai.briar.onetrick";
  c.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
  c.ResearchArtifact = "AI/Research/AIBriar.md";
  c.ImplementationSummary =
      "Briar frenzy and taunt reconciliation, Q leap/stun, W2 healing, E "
      "charge reduction/knockback, and safe global R berserk.";
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
  c.OnBuffAdd = &OnBuffAdd;
  c.OnBuffRemove = &OnBuffRemove;

  c.OnBeforeAttack = &OnBeforeAttack;
  c.OnAfterAttack = &OnAfterAttack;
  c.OnGapcloser = &OnGapcloser;
  c.OnInterruptable = &OnInterruptable;
  c.OnObjectCreate = &OnObjectCreate;
  c.OnObjectDelete = &OnObjectDelete;
  c.OnMissileCreate = &OnMissileCreate;
  c.OnMissileDelete = &OnMissileDelete;
  return c;
}();
} // namespace Plugins::KuroAIO::AI::Controllers::Briar
