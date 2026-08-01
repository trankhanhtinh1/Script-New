#pragma once
#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AICassiopeiaGeometry.h"
#include <algorithm>
#include <array>
#include <cfloat>
#include <vector>
namespace Plugins::KuroAIO::AI::Controllers::Cassiopeia {
using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
inline Menu *TacticsMenu = nullptr;
inline Menu *PoisonMenu = nullptr;
inline Menu *UltimateMenu = nullptr;
inline Menu *FarmMenu = nullptr;
inline Menu *CoachMenu = nullptr;
inline int PassiveStacks = 0;
inline int QCastTick = 0;
inline int WCastTick = 0;
inline int ECastTick = 0;
inline int RCastTick = 0;
inline int IncomingHardCCUntil = 0;
inline int PlayerOverrideUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline Vector3 LastQPosition = {};
inline Vector3 LastWPosition = {};
inline Vector3 LastRPosition = {};
using ControllerHelpers::Now;
using ControllerHelpers::Ready;
inline bool Poisoned(const AIHeroClient &target) {
  return Engine::ValidEnemy(target) && (target.HasBuff("CassiopeiaQPoison") ||
                                        target.HasBuff("CassiopeiaWPoison") ||
                                        target.HasBuff("cassiopeiaqpoison") ||
                                        target.HasBuff("cassiopeiawpoison"));
}
inline bool TargetCannotBeDamaged(const AIHeroClient &t) {
  return !Engine::ValidEnemy(t) || t.IsInvulnerable() || t.HasBuff("SivirE") ||
         t.HasBuff("NocturneShroudofDarkness") || t.HasBuff("MorganaE") ||
         t.HasBuff("BlackShield") || t.HasBuff("BansheesVeil") ||
         t.HasBuff("EdgeOfNight") || t.HasBuff("VladimirSanguinePool") ||
         t.HasBuff("KayleR");
}
using ControllerHelpers::AP;
inline float QDamage(const AIHeroClient &t) {
  return Engine::ValidEnemy(t)
             ? QRawDamage(std::clamp(Engine::RuntimeSpells[0]->Level(), 1, 5),
                          AP())
             : 0.0f;
}
inline float WDamage(const AIHeroClient &t) {
  return Engine::ValidEnemy(t)
             ? WRawDamage(std::clamp(Engine::RuntimeSpells[1]->Level(), 1, 5),
                          AP(), 1.0f)
             : 0.0f;
}
inline float EDamage(const AIHeroClient &t) {
  return Engine::ValidEnemy(t)
             ? ERawDamage(std::clamp(Engine::RuntimeSpells[2]->Level(), 1, 5),
                          AP(), t.HealthPercent())
             : 0.0f;
}
inline float RDamage(const AIHeroClient &t) {
  return Engine::ValidEnemy(t)
             ? RRawDamage(std::clamp(Engine::RuntimeSpells[3]->Level(), 1, 3),
                          AP())
             : 0.0f;
}
using ControllerHelpers::Lethal;
inline bool HasWall(const Vector3 &a, const Vector3 &b) {
  return SDK::NavMesh::IsWallBetween(a, b, 25.0f);
}
inline void ReconcileState() {
  const auto p = GameObjects::Player();
  if (!p.IsValid())
    return;
  if (p.HasBuff("CassiopeiaPassive"))
    PassiveStacks = std::max(PassiveStacks, 1);
  if (PassiveStacks > 500)
    PassiveStacks = 500;
}
inline bool CastQ(const AIHeroClient &target, Mode mode) {
  if (!Engine::ValidEnemy(target, kQRange) || !Ready(0, mode) ||
      TargetCannotBeDamaged(target))
    return false;
  const auto p = GameObjects::Player();
  const Vector3 aim = PredictPosition(target, 0.75f);
  if (!aim.IsValid() || aim.IsZero() ||
      p.Position().Distance2D(aim) > kQRange + target.BoundingRadius() ||
      HasWall(p.Position(), aim))
    return false;
  if (!Engine::ControllerCastPosition(0, aim))
    return false;
  QCastTick = Now();
  LastQPosition = aim;
  return true;
}
inline bool CastW(const AIHeroClient &target, Mode mode, bool fleeing = false) {
  if (!Ready(1, mode))
    return false;
  const auto p = GameObjects::Player();
  if (!p.IsValid())
    return false;
  const Vector3 aim = Engine::ValidEnemy(target)
                          ? PredictPosition(target, 0.35f)
                          : Game::CursorPos();
  ZoneContext c{aim.IsValid() && !aim.IsZero(),
                !SDK::NavMesh::IsWall(aim),
                Engine::UnderEnemyTurret(aim),
                Engine::ValidEnemy(target),
                !Poisoned(target),
                fleeing};
  if (!ZoneSafe(c))
    return false;
  if (!Engine::ControllerCastPosition(1, aim))
    return false;
  WCastTick = Now();
  LastWPosition = aim;
  return true;
}
inline bool CastE(const AIHeroClient &target, Mode mode) {
  if (!Engine::ValidEnemy(target, kERange) || !Ready(2, mode) ||
      TargetCannotBeDamaged(target))
    return false;
  if (!Poisoned(target) && !Lethal(target, EDamage(target)))
    return false;
  if (!Engine::ControllerCastUnit(2, target))
    return false;
  ECastTick = Now();
  return true;
}
inline bool CastR(const AIHeroClient &target, Mode mode,
                  bool defensive = false) {
  if (!Engine::ValidEnemy(target, kRRange) || !Ready(3, mode) ||
      TargetCannotBeDamaged(target))
    return false;
  const auto p = GameObjects::Player();
  const Vector3 aim = PredictPosition(target, 0.55f);
  const Vector3 dir = Direction2D(p.Position(), aim);
  if (dir.IsZero() || HasWall(p.Position(), aim))
    return false;
  int hits = Engine::CountEnemiesAt(aim, 240.0f);
  const RContext c{
      true,
      true,
      RHits(p.Position().Distance2D(aim), 0.0f, target.BoundingRadius()),
      HasWall(p.Position(), aim),
      Lethal(target, RDamage(target)),
      hits >= 2,
      defensive};
  if (!MayCastR(c))
    return false;
  if (!Engine::ControllerCastPosition(3, aim))
    return false;
  RCastTick = Now();
  LastRPosition = aim;
  return true;
}
inline bool TryDefensive(const AIHeroClient &t) {
  if (!Engine::ValidEnemy(t, 900.0f))
    return false;
  if (IncomingHardCCUntil > Now() || GameObjects::Player().HealthPercent() <=
                                         Slider(TacticsMenu, "EmergencyHP", 32))
    return CastW(t, Mode::Flee, true) || CastR(t, Mode::Flee, true);
  return false;
}
inline bool TryKill(const AIHeroClient &t, Mode m) {
  if (Lethal(t, EDamage(t)) && CastE(t, m))
    return true;
  if (Lethal(t, QDamage(t) + EDamage(t)) && CastQ(t, m))
    return true;
  return Lethal(t, RDamage(t)) && CastR(t, m);
}
inline bool TryCombo(const AIHeroClient &t) {
  if (CastW(t, Mode::Combo))
    return true;
  if (CastQ(t, Mode::Combo))
    return true;
  if (CastE(t, Mode::Combo))
    return true;
  return t.HealthPercent() <= Slider(UltimateMenu, "RTargetHP", 45) &&
         CastR(t, Mode::Combo);
}
inline bool TryHarass(const AIHeroClient &t) {
  if (CastQ(t, Mode::Harass))
    return true;
  return Poisoned(t) && CastE(t, Mode::Harass);
}
inline bool TryFarm(Mode m) {
  if (!Ready(2, m) ||
      GameObjects::Player().ManaPercent() < Slider(FarmMenu, "ManaPercent", 35))
    return false;
  return Engine::TryFarm(m);
}
inline bool OnUpdate(Mode mode, const AIHeroClient &selected) {
  ReconcileState();
  if (PlayerOverrideUntil > Now())
    return true;
  AIHeroClient t = selected;
  if (!Engine::ValidEnemy(t))
    t = Engine::SelectTarget(kRRange);
  const AIHeroClient threat = NearestEnemyToPlayer(t, 1000.0f);
  if (mode == Mode::Flee) {
    (void)CastW(threat, Mode::Flee, true);
    return true;
  }
  if (TryDefensive(threat) || TryKill(t, mode))
    return true;
  switch (mode) {
  case Mode::Combo:
    (void)TryCombo(t);
    break;
  case Mode::Harass:
    (void)TryHarass(t);
    break;
  case Mode::LaneClear:
  case Mode::Jungle:
  case Mode::LastHit:
    (void)TryFarm(mode);
    break;
  case Mode::Automatic:
    if (Engine::ValidEnemy(t) &&
        AutomaticAllowed({IncomingHardCCUntil > Now(),
                          IncomingHardCCUntil > Now(), Lethal(t, EDamage(t)),
                          false}))
      (void)TryKill(t, Mode::Automatic);
    break;
  default:
    break;
  }
  return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs &a) {
  if (!a.Sender.IsValid())
    return;
  const int now = Now();
  if (!IsLocalPlayer(a.Sender)) {
    const auto threat = ControllerHelpers::AnalyzeEnemyCast(
        a, 220.0f, 110.0f, 250, 280, 260, 1500, 450);
    if (threat.Valid && threat.CrossesPlayer && threat.LikelyHardCrowdControl)
      IncomingHardCCUntil = now + 650;
    return;
  }
  const bool owned =
      a.Slot >= 0 && a.Slot < 4 && Engine::WasControllerCast(a.Slot);
  if (!owned)
    PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 520);
  if (a.Slot == 0)
    QCastTick = now;
  else if (a.Slot == 1)
    WCastTick = now;
  else if (a.Slot == 2)
    ECastTick = now;
  else if (a.Slot == 3)
    RCastTick = now;
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs &a) {
  if (!IsLocalPlayer(a.Sender))
    return;
  if (Engine::TextContains(a.BuffName, "CassiopeiaPassive"))
    PassiveStacks = std::max(PassiveStacks, 1);
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs &a) {
  if (!IsLocalPlayer(a.Sender))
    return;
}
inline void OnBuffUpdate(const SDK::Events::BuffEventArgs &a) {
  if (IsLocalPlayer(a.Sender) && a.EndTime > Game::Time())
    OnBuffAdd(a);
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs &a) {
  if (!a.Target.IsValid())
    return;
}
inline void OnDraw() {
  const auto p = GameObjects::Player();
  if (!p.IsValid() || !Bool(CoachMenu, "DrawRanges", false))
    return;
  Drawing::DrawCircle(p.Position(), kQRange, 0xFF8555EEu, 1.6f, 40);
  if (!LastWPosition.IsZero())
    Drawing::DrawCircle(LastWPosition, kWRadius, 0xFF55D6C8u, 1.8f, 32);
  if (!LastRPosition.IsZero())
    Drawing::DrawLine(p.Position(), LastRPosition, 0xFFFF6699u, 2.0f);
}
inline void BuildMenu(Menu *root) {
  if (!root)
    return;
  TacticsMenu = root->AddSubMenu(
      new Menu("CassiopeiaOneTrick", "Cassiopeia poison mechanics"));
  TacticsMenu->Add(new MenuSlider(
      "ManualOwnershipMs", "Yield after player spell (ms)", 520, 180, 1100));
  TacticsMenu->Add(new MenuSlider("EmergencyHP", "Emergency HP", 32, 10, 70));
  PoisonMenu = TacticsMenu->AddSubMenu(new Menu("Poison", "Poison tracking"));
  PoisonMenu->Add(new MenuBool("UseMiasma", "Use Miasma", true));
  UltimateMenu =
      TacticsMenu->AddSubMenu(new Menu("PetrifyingGaze", "R policy"));
  UltimateMenu->Add(
      new MenuSlider("RTargetHP", "R target HP threshold", 45, 10, 100));
  FarmMenu = TacticsMenu->AddSubMenu(new Menu("CassiopeiaFarm", "Farm policy"));
  FarmMenu->Add(
      new MenuSlider("ManaPercent", "Minimum mana percent", 35, 0, 100));
  CoachMenu =
      TacticsMenu->AddSubMenu(new Menu("CassiopeiaCoach", "Visualization"));
  CoachMenu->Add(new MenuBool("DrawRanges", "Draw ranges", false));
}
inline void OnLoad() {
  PassiveStacks = 0;
  QCastTick = WCastTick = ECastTick = RCastTick = 0;
  IncomingHardCCUntil = PlayerOverrideUntil = 0;
  LastAutoTargetId = LastAutoTick = 0;
  LastQPosition = LastWPosition = LastRPosition = {};
  ReconcileState();
}
inline void OnUnload() {
  TacticsMenu = PoisonMenu = UltimateMenu = FarmMenu = CoachMenu = nullptr;
  PassiveStacks = 0;
}
inline constexpr const char *Scenarios[] = {
    "Read Riot 26.15 and CommunityDragon 16.15 as the pinned Summoner's Rift "
    "baseline",
    "Track passive state with events and polling reconciliation",
    "Preserve passive resource stacks and never invent a missing passive event",
    "Use Q 850 range and current AP scaling",
    "Reject Q through wall or prediction uncertainty",
    "Use W 700 range and 160 radius miasma zone",
    "Reject W under a new turret outside flee posture",
    "Use W to apply poison and deny enemy movement",
    "Use E only on poisoned or lethal targets",
    "Preserve Twin Fang reset timing and AA cooperation",
    "Use E execute scaling below the live health threshold",
    "Use R 825 range and directional cone geometry",
    "Require R high prediction and target contact",
    "Reject R through projectile wall",
    "Reserve nonlethal single-target R without peel or multi-target value",
    "Use R for defensive hard crowd-control response",
    "Preserve selected target before orbwalker fallback",
    "Combo starts Q/W poison before repeated E",
    "Harass avoids unsolicited R and preserves mana",
    "LaneClear, Jungle and LastHit use shared farm path",
    "Flee uses W peel and defensive R",
    "Automatic mode rejects unsolicited engage",
    "Automatic mode allows defensive, interrupt and kill-secure actions",
    "Preserve AA windup before nonlethal spell casts",
    "Yield after manual Q/W/E/R and re-plan from observed state",
    "Never automate Flash, Ignite, Smite or item actives",
    "Keep profile metadata separate from the owned decision loop",
};
inline constexpr ChampionController Controller = [] {
  ChampionController controller{};
  controller.ChampionId = SDK::ChampionId::Cassiopeia;
  controller.ControllerId = "champion.kuroaio.ai.cassiopeia.onetrick";
  controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
  controller.ResearchArtifact = "AI/Research/AICassiopeia.md";
  controller.ImplementationSummary =
      "Poison-state Q/W setup, Twin Fang reset discipline, directional R "
      "safety and event/poll reconciliation.";
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
      &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId,
                                                      &LastAutoTick>;
  controller.OnBuffAdd = &OnBuffAdd;
  controller.OnBuffRemove = &OnBuffRemove;
  controller.OnBuffUpdate = &OnBuffUpdate;
  controller.OnBeforeAttack = &OnBeforeAttack;
  controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
  return controller;
}();
} // namespace Plugins::KuroAIO::AI::Controllers::Cassiopeia
