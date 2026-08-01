#pragma once
#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIJayceGeometry.h"
#include <algorithm>
#include <cfloat>
#include <vector>
namespace Plugins::KuroAIO::AI::Controllers::Jayce {
using namespace Geometry;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
inline Menu *MenuRoot = nullptr;
inline int Form = 0;
inline int ManualUntil = 0;
inline int ThreatUntil = 0;
inline int LastTarget = 0;
inline int LastTick = 0;
using ControllerHelpers::Now;
using ControllerHelpers::Ready;
inline bool Hammer() { return Form == 1; }
inline bool Blocked(const AIHeroClient &t) {
  return !Engine::ValidEnemy(t) || t.IsInvulnerable() || t.HasBuff("SivirE") ||
         t.HasBuff("NocturneShroudofDarkness") || t.HasBuff("MorganaE") ||
         t.HasBuff("BansheesVeil");
}
inline bool CastGate(const AIHeroClient &t, Mode m) {
  if (!Ready(2, m))
    return false;
  const Vector3 p = PredictPosition(t, 0.3f);
  if (!p.IsValid() || SDK::NavMesh::IsWall(p))
    return false;
  return Engine::ControllerCastPosition(2, p);
}
inline bool CastQ(const AIHeroClient &t, Mode m) {
  if (Blocked(t) || !Ready(0, m))
    return false;
  const auto p = GameObjects::Player();
  const auto aim = PredictPosition(t, 0.25f);
  if (!aim.IsValid() ||
      p.Position().Distance2D(aim) > kCannonQRange + t.BoundingRadius() ||
      SDK::NavMesh::IsWallBetween(p.Position(), aim, 25.0f) ||
      ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 35.0f))
    return false;
  return Engine::ControllerCastPosition(0, aim);
}
inline bool CastHammerQ(const AIHeroClient &t, Mode m) {
  if (Blocked(t) || !Ready(0, m) || !Hammer())
    return false;
  const auto p = GameObjects::Player();
  const auto aim = PredictPosition(t, 0.2f);
  if (!HammerLeapSafe(!SDK::NavMesh::IsWall(aim), Engine::UnderEnemyTurret(aim),
                      t.HealthPercent() < 35.0f, false) ||
      p.Position().Distance2D(aim) > kHammerQRange + t.BoundingRadius())
    return false;
  return Engine::ControllerCastUnit(0, t);
}
inline bool CastForm(Mode m) {
  if (!Ready(3, m) || !CanTransform(true, false, ThreatUntil > Now()))
    return false;
  if (!Engine::ControllerCastSelf(3))
    return false;
  Form = Hammer() ? 0 : 1;
  return true;
}
inline bool CastW(Mode m) {
  return Ready(1, m) && Engine::ControllerCastSelf(1);
}
inline bool CastE(const AIHeroClient &t, Mode m) {
  if (Blocked(t) || !Ready(2, m))
    return false;
  return Hammer() ? Engine::ControllerCastUnit(2, t) : CastGate(t, m);
}
inline bool Combo(const AIHeroClient &t) {
  if (Hammer()) {
    if (CastHammerQ(t, Mode::Combo))
      return true;
    if (CastE(t, Mode::Combo))
      return true;
    if (CastForm(Mode::Combo))
      return true;
  } else {
    if (CastGate(t, Mode::Combo) && CastQ(t, Mode::Combo))
      return true;
    if (CastQ(t, Mode::Combo))
      return true;
    if (t.HealthPercent() < 45.0f && CastForm(Mode::Combo))
      return true;
  }
  return CastW(Mode::Combo);
}
inline bool OnUpdate(Mode mode, const AIHeroClient &selected) {
  if (ManualUntil > Now())
    return true;
  auto t = selected;
  if (!Engine::ValidEnemy(t))
    t = Engine::SelectTarget(kCannonQRange);
  if (mode == Mode::Flee) {
    if (Hammer())
      (void)CastE(t, Mode::Flee);
    else
      (void)CastForm(Mode::Flee);
    return true;
  }
  if (Engine::ValidEnemy(t) && Blocked(t))
    return true;
  if (mode == Mode::Combo)
    (void)Combo(t);
  else if (mode == Mode::Harass) {
    if (!Hammer())
      (void)CastQ(t, mode);
    else
      (void)CastForm(mode);
  } else if (mode == Mode::LaneClear) {
    (void)Engine::TryFarm(Mode::LaneClear);
  } else if (mode == Mode::Jungle) {
    (void)Engine::TryFarm(Mode::Jungle);
  } else if (mode == Mode::LastHit) {
    (void)Engine::TryFarm(Mode::LastHit);
  } else if (mode == Mode::Automatic &&
             AutomaticAllowed({ThreatUntil > Now(), ThreatUntil > Now(),
                               t.IsValid() && t.HealthPercent() < 20.0f,
                               false}))
    (void)Combo(t);
  return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs &a) {
  if (!a.Sender.IsValid())
    return;
  if (!IsLocalPlayer(a.Sender)) {
    if (a.Slot >= 0)
      ThreatUntil = Now() + 500;
    return;
  }
  if (a.Slot == 3)
    Form = Hammer() ? 0 : 1;
  else
    ManualUntil = Now() + 500;
}
inline void OnDraw() {
  if (!Bool(MenuRoot, "DrawRanges", false))
    return;
  const auto p = GameObjects::Player();
  if (p.IsValid())
    Drawing::DrawCircle(p.Position(), Hammer() ? kHammerQRange : kCannonQRange,
                        0xFF55AAEEu, 1.5f, 40);
}
inline void BuildMenu(Menu *root) {
  if (!root)
    return;
  MenuRoot =
      root->AddSubMenu(new Menu("JayceOneTrick", "Jayce dual-form tactics"));
  MenuRoot->Add(new MenuBool("DrawRanges", "Draw ranges", false));
}
inline void OnLoad() {
  Form = 0;
  ManualUntil = ThreatUntil = LastTarget = LastTick = 0;
}
inline void OnUnload() { MenuRoot = nullptr; }
inline constexpr const char *Scenarios[] = {
    "Read Riot 26.15 and CommunityDragon 16.15 baseline",
    "Track cannon and hammer form state with event and polling reconciliation",
    "Use gate-accelerated shock blast geometry",
    "Reject Q through wall or uncertain prediction",
    "Use hammer leap only at safe walkable endpoints",
    "Use hammer E for peel and displacement",
    "Preserve selected target and orbwalker cooperation",
    "Combo uses ranged poke before safe hammer conversion",
    "Harass preserves mana and avoids unsolicited dive",
    "LaneClear and Jungle preserve form economy",
    "Flee uses hammer peel or ranged retreat",
    "Automatic mode allows defensive and kill-secure reactions only",
    "Preserve AA windup and manual form ownership",
    "Never automate summoner or item actives",
    "Keep profile metadata separate from owned decision loop"};
inline constexpr ChampionController Controller = [] {
  ChampionController controller{};
  controller.ChampionId = SDK::ChampionId::Jayce;
  controller.ControllerId = "champion.kuroaio.ai.jayce.onetrick";
  controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
  controller.ResearchArtifact = "AI/Research/AIJayce.md";
  controller.ImplementationSummary =
      "Dual-form gate shock, hammer safety and manual form reconciliation.";
  controller.Scenarios = Scenarios;
  controller.ScenarioCount = std::size(Scenarios);
  controller.OwnsDecisionLoop = true;
  controller.OnLoad = &OnLoad;
  controller.OnUnload = &OnUnload;
  controller.BuildMenu = &BuildMenu;
  controller.OnUpdate = &OnUpdate;
  controller.OnDraw = &OnDraw;
  controller.OnProcessSpell = &OnProcessSpell;
  controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastTarget, &LastTick>;
  return controller;
}();
} // namespace Plugins::KuroAIO::AI::Controllers::Jayce
