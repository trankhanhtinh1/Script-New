#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIIreliaGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>

namespace Plugins::KuroAIO::AI::Controllers::Irelia {
using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

struct MarkRecord {
  int NetworkId = 0;
  int ExpireTick = 0;
};
inline Menu *TacticsMenu = nullptr, *QMenu = nullptr, *WMenu = nullptr,
            *EMenu = nullptr, *RMenu = nullptr, *FarmMenu = nullptr,
            *CoachMenu = nullptr;
inline int PassiveStacks = 0, PassiveExpireTick = 0;
inline std::array<MarkRecord, 16> Marks{};
inline bool WCharging = false, WManual = false, EFirstPlaced = false,
            EManual = false;
inline int WChargeStartTick = 0, WTargetId = 0, EFirstExpireTick = 0,
           ETargetId = 0;
inline Vector3 EFirstPosition{}, LastEAim{}, LastRAim{};
inline BladeWall LastBladeWall{};
inline int BladeWallExpireTick = 0, FocusTargetId = 0, IncomingThreatId = 0,
           IncomingThreatUntil = 0, IncomingImpactTick = 0,
           GapcloserTargetId = 0, GapcloserExpireTick = 0,
           InterruptTargetId = 0, InterruptExpireTick = 0,
           PlayerOverrideUntil = 0, QCastTick = 0, WCastTick = 0, ECastTick = 0,
           RCastTick = 0, LastAutoTargetId = 0, LastAutoTick = 0;
inline Vector3 GapcloserEnd{};

using ControllerHelpers::Now;
inline bool Ready(int i, Mode m, bool windup = false) {
  return i >= 0 && i < 4 && Engine::RuntimeSpells[i] &&
         Engine::RuntimeSpells[i]->IsReady() && SpellEnabled(i, m) &&
         (windup || !Orbwalker::IsWindingUp());
}
inline bool Throttle(int tick, int delay) { return Now() - tick >= delay; }
inline int SliderValue(Menu *menu, const char *key, int fallback) {
  if (!menu)
    return fallback;
  const auto *item = menu->Get<MenuSlider>(key);
  return item ? item->Value : fallback;
}
inline bool BoolValue(Menu *menu, const char *key, bool fallback = true) {
  if (!menu)
    return fallback;
  const auto *item = menu->Get<MenuBool>(key);
  return item ? item->Value : fallback;
}
inline bool Marked(const AIBaseClient &unit) {
  if (!unit.IsValid())
    return false;
  if (unit.HasBuff("ireliamark") || unit.HasBuff("IreliaMark"))
    return true;
  for (const auto &mark : Marks)
    if (mark.NetworkId == static_cast<int>(unit.NetworkId()) &&
        mark.ExpireTick > Now())
      return true;
  return false;
}
inline void SetMark(int id, int expiry) {
  if (!id)
    return;
  for (auto &mark : Marks)
    if (mark.NetworkId == id || mark.NetworkId == 0 ||
        mark.ExpireTick <= Now()) {
      mark = {id, expiry};
      return;
    }
}
inline void ClearMark(int id) {
  for (auto &mark : Marks)
    if (mark.NetworkId == id)
      mark = {};
}
inline bool Rejected(const AIBaseClient &target) {
  return ControllerHelpers::IsCommonUntargetableOrImmune(target) ||
         ControllerHelpers::HasSpellShieldOrImmunity(target);
}

inline float QDamage(const AIBaseClient &target) {
  const auto p = GameObjects::Player();
  if (!p.IsValid() || !target.IsValid())
    return 0.0f;
  const bool lane =
      target.IsMinion() && !AIMinionClient(target.Address()).IsJungle();
  const float raw =
      lane ? QRawDamageToMinion(SpellRank(0), p.TotalAttackDamage())
           : QRawDamage(SpellRank(0), p.TotalAttackDamage());
  return p.CalculatePhysicalDamage(target, raw);
}
inline float WDamage(const AIBaseClient &target, int elapsed) {
  const auto p = GameObjects::Player();
  return p.IsValid() && target.IsValid()
             ? p.CalculatePhysicalDamage(
                   target, WRawDamage(SpellRank(1), p.TotalAttackDamage(),
                                      p.AP(), elapsed))
             : 0.0f;
}
inline float EDamage(const AIBaseClient &target) {
  static constexpr std::array<float, 6> base{0, 80, 125, 170, 215, 260};
  const auto p = GameObjects::Player();
  return p.IsValid() && target.IsValid()
             ? p.CalculateMagicDamage(target,
                                      base[std::clamp(SpellRank(2), 0, 5)] +
                                          0.80f * p.AP())
             : 0.0f;
}
inline float RDamage(const AIBaseClient &target) {
  static constexpr std::array<float, 4> base{0, 125, 250, 375};
  const auto p = GameObjects::Player();
  return p.IsValid() && target.IsValid()
             ? p.CalculateMagicDamage(target,
                                      base[std::clamp(SpellRank(3), 0, 3)] +
                                          0.70f * p.AP())
             : 0.0f;
}
using ControllerHelpers::Lethal;

inline void ReconcileState() {
  const auto p = GameObjects::Player();
  if (!p.IsValid())
    return;
  const int now = Now();
  const int observed = ControllerHelpers::MaximumBuffCount(
      p, {"ireliapassivestacks", "IreliaPassiveStacks", "ireliapassive"});
  if (observed > 0) {
    PassiveStacks = ClampPassiveStacks(observed);
    PassiveExpireTick = now + kPassiveDurationMs;
  } else if (PassiveExpireTick && now >= PassiveExpireTick) {
    PassiveStacks = 0;
    PassiveExpireTick = 0;
  }
  const bool runtimeW = Engine::RuntimeSpells[1] &&
                        (Engine::RuntimeSpells[1]->IsCharging() ||
                         p.HasBuff("ireliawdefense") || p.HasBuff("IreliaW"));
  if (runtimeW && !WCharging) {
    WCharging = true;
    WChargeStartTick = now;
  } else if (!runtimeW && WCharging && now - WChargeStartTick > 100) {
    WCharging = WManual = false;
    WChargeStartTick = WTargetId = 0;
  }
  if (ControllerHelpers::RuntimeNameContains(2, "IreliaE2") && !EFirstPlaced) {
    EFirstPlaced = true;
    EFirstExpireTick = now + kERecastWindowMs;
  }
  if (EFirstPlaced && EFirstExpireTick && now >= EFirstExpireTick) {
    EFirstPlaced = EManual = false;
    EFirstPosition = {};
    EFirstExpireTick = ETargetId = 0;
  }
  for (auto &mark : Marks)
    if (mark.ExpireTick <= now)
      mark = {};
  if (BladeWallExpireTick && now >= BladeWallExpireTick) {
    LastBladeWall = {};
    BladeWallExpireTick = 0;
  }
}

inline DashSafetyContext DashContext(const AIBaseClient &unit, bool reset,
                                     bool lethal, bool flee) {
  const auto p = GameObjects::Player();
  DashSafetyContext c{};
  if (!p.IsValid() || !unit.IsValid())
    return c;
  const Vector3 end = unit.Position();
  c.EndpointValid = end.IsValid() && !end.IsZero();
  c.EndpointWall = SDK::NavMesh::IsWall(end);
  c.NewEnemyTurret =
      Engine::UnderEnemyTurret(end) && !Engine::UnderEnemyTurret(p.Position());
  c.ResetExpected = reset;
  c.Lethal = lethal;
  c.Fleeing = flee;
  c.CursorProgress = end.Distance2D(Game::CursorPos()) + 35.0f <
                     p.Position().Distance2D(Game::CursorPos());
  c.EnemiesAtEndpoint = Engine::CountEnemiesAt(end, 575.0f);
  c.MaximumEnemies = SliderValue(TacticsMenu, "MaxDashEnemies", 2);
  return c;
}
inline bool CastQ(const AIBaseClient &target, Mode mode, bool flee = false,
                  bool forceLethal = false) {
  if (!target.IsValid() || Rejected(target) || !Ready(0, mode) ||
      !Throttle(QCastTick, 45))
    return false;
  const auto p = GameObjects::Player();
  if (!p.IsValid() ||
      p.Position().Distance2D(target.Position()) >
          kQRange + target.BoundingRadius() ||
      CurrentResource() < SpellCost(0))
    return false;
  const bool kill = Lethal(target, QDamage(target)), marked = Marked(target),
             reset = QWillReset({marked, kill, target.IsDead()}),
             lethal = forceLethal || (target.IsHero() && kill);
  if (!QDashSafe(DashContext(target, reset, lethal, flee)))
    return false;
  if (!Engine::ControllerCastUnit(0, target))
    return false;
  QCastTick = Now();
  if (marked)
    ClearMark(static_cast<int>(target.NetworkId()));
  if (target.IsHero())
    FocusTargetId = static_cast<int>(target.NetworkId());
  return true;
}

inline bool StartW(const AIHeroClient &target, Mode mode, bool defensive) {
  if (WCharging || !Ready(1, mode, true) || !Throttle(WCastTick, 80) ||
      !Engine::RuntimeSpells[1])
    return false;
  Vector3 aim = Engine::ValidEnemy(target, kWRange + 100)
                    ? PredictPosition(target, 0.25f)
                    : Game::CursorPos();
  if (!aim.IsValid() || aim.IsZero() ||
      (!defensive && Orbwalker::IsWindingUp()))
    return false;
  Engine::ArmControllerCast(1);
  if (!Engine::RuntimeSpells[1]->StartCharging(aim)) {
    Engine::CancelControllerCast(1);
    return false;
  }
  Engine::MarkSuccessfulCast(1);
  WCharging = true;
  WManual = false;
  WChargeStartTick = WCastTick = Now();
  WTargetId =
      Engine::ValidEnemy(target) ? static_cast<int>(target.NetworkId()) : 0;
  return true;
}
inline bool ReleaseW(const AIHeroClient &fallback, bool impact = false) {
  if (!WCharging || WManual || !Engine::RuntimeSpells[1])
    return false;
  AIHeroClient target = HeroByNetworkId(WTargetId);
  if (!Engine::ValidEnemy(target, kWRange + 150))
    target = fallback;
  Vector3 aim = Engine::ValidEnemy(target) ? PredictPosition(target, 0.18f)
                                           : Game::CursorPos();
  const auto p = GameObjects::Player();
  const int elapsed = std::max(0, Now() - WChargeStartTick);
  const bool valid = aim.IsValid() && !aim.IsZero(),
             inRange = valid && p.IsValid() &&
                       p.Position().Distance2D(aim) <= kWRange + 80;
  WReleaseContext c{true,
                    valid,
                    inRange,
                    impact,
                    !inRange,
                    Engine::ValidEnemy(target) &&
                        Lethal(target, WDamage(target, elapsed)),
                    elapsed};
  if (!ShouldReleaseW(c))
    return false;
  if (!valid)
    aim = p.Position().Extend(Game::CursorPos(), 100.0f);
  Engine::ArmControllerCast(1);
  if (!Engine::RuntimeSpells[1]->ShootChargedSpell(aim)) {
    Engine::CancelControllerCast(1);
    return false;
  }
  Engine::MarkSuccessfulCast(1);
  WCharging = false;
  WChargeStartTick = WTargetId = 0;
  WCastTick = Now();
  return true;
}

inline bool CastE(const AIHeroClient &target, Mode mode, bool interrupt = false,
                  bool defensive = false) {
  if (!Engine::ValidEnemy(target, kERange + 80) || !Ready(2, mode, true) ||
      !Throttle(ECastTick, 55) || Rejected(target))
    return false;
  const auto p = GameObjects::Player();
  const Vector3 predicted =
      PredictPosition(target, EFirstPlaced ? 0.18f : 0.32f);
  Vector3 place{};
  bool crosses = false;
  if (!EFirstPlaced) {
    const auto plan =
        BuildBladePlan(p.Position(), predicted, target.BoundingRadius());
    place = plan.First;
    crosses = plan.CrossesTarget;
  } else {
    const Vector3 first =
        EFirstPosition.IsZero() ? p.Position() : EFirstPosition;
    place = BuildSecondBlade(p.Position(), first, predicted);
    crosses = ELineHits(first, place, predicted, target.BoundingRadius());
  }
  ECastContext c{true,
                 place.IsValid() && !place.IsZero(),
                 place.IsValid() && SDK::NavMesh::IsWall(place),
                 true,
                 crosses,
                 ControllerHelpers::HasSpellShieldOrImmunity(target),
                 EFirstPlaced,
                 interrupt,
                 defensive};
  if (!MayCastE(c) || p.Position().Distance2D(place) > kERange + 1)
    return false;
  if (!Engine::ControllerCastPosition(2, place))
    return false;
  ECastTick = Now();
  LastEAim = place;
  ETargetId = static_cast<int>(target.NetworkId());
  EManual = false;
  if (!EFirstPlaced) {
    EFirstPlaced = true;
    EFirstPosition = place;
    EFirstExpireTick = Now() + kERecastWindowMs;
  } else {
    EFirstPlaced = false;
    EFirstPosition = {};
    EFirstExpireTick = 0;
  }
  return true;
}

inline bool CastR(const AIHeroClient &target, Mode mode) {
  if (!Engine::ValidEnemy(target, kRRange + 60) || !Ready(3, mode) ||
      !Throttle(RCastTick, 120) || Rejected(target))
    return false;
  const auto p = GameObjects::Player();
  const auto prediction = Engine::RuntimeSpells[3]->GetPrediction(target);
  Vector3 aim = prediction.GetCastPosition();
  if (!aim.IsValid() || aim.IsZero())
    aim = PredictPosition(target, 0.40f);
  if (!aim.IsValid() || aim.IsZero() ||
      p.Position().Distance2D(aim) > kRRange + target.BoundingRadius())
    return false;
  const bool lethal = Lethal(target, RDamage(target));
  const bool line = SharedGeometry::ProjectPointToSegment2D(target.Position(),
                                                            p.Position(), aim)
                        .Distance <= kRWidth * 0.5f + target.BoundingRadius();
  RContext c{
      true,
      static_cast<int>(prediction.Hitchance) >=
          static_cast<int>(SDK::HitChance::VeryHigh),
      line,
      ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kRWidth * 0.5f),
      Engine::RuntimeSpells[0] && Engine::RuntimeSpells[0]->IsReady(),
      QDashSafe(DashContext(target, true, lethal, false)),
      lethal,
      Engine::CountEnemiesAt(aim, 420) >=
          SliderValue(RMenu, "MinimumTargets", 2),
      target.IsMoving() && p.Position().Distance2D(target.PathEnd()) >
                               p.Position().Distance2D(target.Position()),
      PassiveStacks};
  if (!MayCastR(c) || !Engine::ControllerCastPosition(3, aim))
    return false;
  RCastTick = Now();
  LastRAim = aim;
  LastBladeWall = BuildBladeWall(p.Position(), aim);
  BladeWallExpireTick = Now() + 2500;
  FocusTargetId = static_cast<int>(target.NetworkId());
  return true;
}

inline AIBaseClient BestQMinion(Mode mode, const AIHeroClient &chase = {}) {
  const auto p = GameObjects::Player();
  if (!p.IsValid())
    return {};
  AIBaseClient best{};
  float bestScore = -FLT_MAX;
  const auto &units = mode == Mode::Jungle ? GameObjects::Jungle()
                                           : GameObjects::EnemyMinions();
  for (const auto &minion : units) {
    if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable() ||
        p.Position().Distance2D(minion.Position()) >
            kQRange + minion.BoundingRadius())
      continue;
    AIBaseClient unit(minion.Address());
    const bool kill = Lethal(unit, QDamage(unit)), marked = Marked(unit);
    if (!kill && !marked)
      continue;
    float score = (kill ? 800.0f : 350.0f) - minion.Health() * 0.05f;
    if (Engine::ValidEnemy(chase)) {
      const float before = p.Position().Distance2D(chase.Position()),
                  after = minion.Position().Distance2D(chase.Position());
      score += after + 75 < before ? (before - after) * 1.2f : -500.0f;
    }
    if (mode == Mode::Flee)
      score += minion.Position().Distance2D(Game::CursorPos()) + 35 <
                       p.Position().Distance2D(Game::CursorPos())
                   ? 700.0f
                   : -900.0f;
    if (!QDashSafe(DashContext(unit, true, false, mode == Mode::Flee)))
      continue;
    if (score > bestScore) {
      bestScore = score;
      best = unit;
    }
  }
  return best;
}
inline AIHeroClient ResolveTarget(const AIHeroClient &selected) {
  if (Engine::ValidEnemy(selected, kRRange + 100))
    return selected;
  const auto focus = HeroByNetworkId(FocusTargetId);
  if (Engine::ValidEnemy(focus, kRRange + 100) && Marked(focus))
    return focus;
  const auto orb = ControllerHelpers::OrbwalkerHeroTarget(kRRange + 100);
  return Engine::ValidEnemy(orb) ? orb : Engine::SelectTarget(kRRange + 100);
}
inline bool TryDefensive(const AIHeroClient &threat, Mode mode) {
  if (IncomingThreatUntil < Now() && GapcloserExpireTick < Now() &&
      GameObjects::Player().HealthPercent() >
          SliderValue(TacticsMenu, "EmergencyHP", 32))
    return false;
  if (WCharging)
    return ReleaseW(threat, IncomingImpactTick && Now() >= IncomingImpactTick);
  return BoolValue(WMenu, "BlockCommittedDamage", true) &&
         StartW(threat, mode == Mode::None ? Mode::Automatic : mode, true);
}
inline bool TryCombo(const AIHeroClient &t) {
  if (!Engine::ValidEnemy(t))
    return false;
  if (WCharging)
    return ReleaseW(t, IncomingImpactTick && Now() >= IncomingImpactTick);
  if (t.HealthPercent() <= SliderValue(RMenu, "TargetHP", 62) &&
      CastR(t, Mode::Combo))
    return true;
  if (EFirstPlaced && CastE(t, Mode::Combo))
    return true;
  if (!Marked(t) && CastE(t, Mode::Combo))
    return true;
  if (Marked(t) && CastQ(t, Mode::Combo))
    return true;
  if (PassiveStacks < kPassiveMaximumStacks && StartW(t, Mode::Combo, false))
    return true;
  if (Lethal(t, QDamage(t)) && CastQ(t, Mode::Combo, false, true))
    return true;
  const auto m = BoolValue(TacticsMenu, "GapcloseMinions", true)
                     ? BestQMinion(Mode::Combo, t)
                     : AIBaseClient{};
  return m.IsValid() && CastQ(m, Mode::Combo);
}
inline bool TryHarass(const AIHeroClient &t) {
  if (!Engine::ValidEnemy(t) || GameObjects::Player().ManaPercent() <
                                    SliderValue(FarmMenu, "HarassMana", 38))
    return false;
  if (WCharging)
    return ReleaseW(t);
  if (EFirstPlaced)
    return CastE(t, Mode::Harass);
  if (!Marked(t) && CastE(t, Mode::Harass))
    return true;
  return Marked(t) && CastQ(t, Mode::Harass);
}
inline bool TryFarm(Mode mode) {
  if (CurrentResource() <
      SpellCost(0) + SliderValue(FarmMenu, "ManaReserve", 45))
    return false;
  const auto unit = BestQMinion(mode);
  return unit.IsValid() && CastQ(unit, mode);
}
inline bool TryFlee(const AIHeroClient &t) {
  if (WCharging)
    return ReleaseW(t, IncomingImpactTick && Now() >= IncomingImpactTick);
  const auto m = BestQMinion(Mode::Flee);
  if (m.IsValid() && CastQ(m, Mode::Flee, true))
    return true;
  if (Engine::ValidEnemy(t) && CastE(t, Mode::Flee, false, true))
    return true;
  return Engine::ValidEnemy(t) && StartW(t, Mode::Flee, true);
}
inline bool OnUpdate(Mode mode, const AIHeroClient &selected) {
  ReconcileState();
  const auto target = ResolveTarget(selected);
  const auto threat = ControllerHelpers::NearestEnemyToPlayer(target, 1000);
  if (WCharging && WManual)
    return true;
  if (PlayerOverrideUntil > Now())
    return true;
  if (mode == Mode::Flee) {
    (void)TryFlee(threat);
    return true;
  }
  if (TryDefensive(threat, mode))
    return true;
  if (InterruptExpireTick >= Now() && BoolValue(EMenu, "Interrupt", true)) {
    const auto unit = HeroByNetworkId(InterruptTargetId);
    if (Engine::ValidEnemy(unit) && CastE(unit, Mode::Automatic, true, true))
      return true;
  }
  switch (mode) {
  case Mode::Combo:
    (void)TryCombo(target);
    break;
  case Mode::Harass:
    (void)TryHarass(target);
    break;
  case Mode::LaneClear:
  case Mode::Jungle:
  case Mode::LastHit:
    (void)TryFarm(mode);
    break;
  case Mode::Automatic:
    if (Engine::ValidEnemy(target) &&
        AutomaticAllowed(
            {false, false, Lethal(target, QDamage(target)), false}))
      (void)CastQ(target, Mode::Automatic, false, true);
    break;
  default:
    break;
  }
  return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs &args) {
  const auto p = GameObjects::Player();
  if (!p.IsValid() || !args.Sender.IsValid())
    return;
  const int now = Now();
  if (!IsLocalPlayer(args.Sender)) {
    const auto threat = ControllerHelpers::AnalyzeEnemyCast(
        args, 220, 115, 250, 280, 220, 1500, 500);
    if (threat.Valid && (threat.TargetsPlayer || threat.CrossesPlayer)) {
      IncomingThreatId = static_cast<int>(threat.Enemy.NetworkId());
      IncomingThreatUntil =
          std::max(threat.CommitmentUntilTick, threat.LineThreatUntilTick);
      IncomingImpactTick =
          now + std::clamp(ControllerHelpers::NormalizedCastDelayMs(
                               args.CastDelay, 300),
                           80, 1200);
    }
    return;
  }
  const int slot = args.Slot;
  const bool owned = slot >= 0 && slot < 4 && Engine::WasControllerCast(slot);
  if (!owned)
    PlayerOverrideUntil =
        now + SliderValue(TacticsMenu, "ManualOwnershipMs", 520);
  if (slot == 0)
    QCastTick = now;
  else if (slot == 1) {
    WCastTick = now;
    WCharging = true;
    WManual = !owned;
    if (!WChargeStartTick)
      WChargeStartTick = now;
  } else if (slot == 2) {
    ECastTick = now;
    const bool second = Engine::TextContains(args.SpellName, "IreliaE2") ||
                        Engine::TextContains(args.ScriptName, "IreliaE2");
    if (second) {
      EFirstPlaced = false;
      EFirstPosition = {};
      EFirstExpireTick = 0;
    } else {
      EFirstPlaced = true;
      EManual = !owned;
      EFirstPosition = args.EndPosition.IsValid() && !args.EndPosition.IsZero()
                           ? args.EndPosition
                           : args.CastPosition;
      EFirstExpireTick = now + kERecastWindowMs;
    }
  } else if (slot == 3) {
    RCastTick = now;
    const Vector3 impact =
        args.EndPosition.IsValid() && !args.EndPosition.IsZero()
            ? args.EndPosition
            : args.CastPosition;
    if (impact.IsValid() && !impact.IsZero()) {
      LastBladeWall = BuildBladeWall(p.Position(), impact);
      BladeWallExpireTick = now + 2500;
    }
  }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs &args) {
  const int now = Now();
  if (IsLocalPlayer(args.Sender)) {
    if (Engine::TextContains(args.BuffName, "ireliapassive")) {
      PassiveStacks = ClampPassiveStacks(std::max(1, args.Count));
      PassiveExpireTick =
          now + ControllerHelpers::RemainingMilliseconds(
                    args.EndTime, kPassiveDurationMs, 200, 6500);
    } else if (Engine::TextContains(args.BuffName, "ireliaw")) {
      WCharging = true;
      if (!WChargeStartTick)
        WChargeStartTick = now;
    }
    return;
  }
  if (Engine::TextContains(args.BuffName, "ireliamark"))
    SetMark(static_cast<int>(args.Sender.NetworkId),
            now + ControllerHelpers::RemainingMilliseconds(
                      args.EndTime, kMarkDurationMs, 200, 5500));
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs &args) {
  if (IsLocalPlayer(args.Sender)) {
    if (Engine::TextContains(args.BuffName, "ireliapassive")) {
      PassiveStacks = PassiveExpireTick = 0;
    } else if (Engine::TextContains(args.BuffName, "ireliaw")) {
      WCharging = WManual = false;
      WChargeStartTick = 0;
    }
  } else if (Engine::TextContains(args.BuffName, "ireliamark"))
    ClearMark(static_cast<int>(args.Sender.NetworkId));
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs &args) {
  const auto p = GameObjects::Player();
  if (!p.IsValid() || !args.Target.IsValid() || !FocusTargetId ||
      PassiveStacks < kPassiveMaximumStacks)
    return;
  const auto focus = HeroByNetworkId(FocusTargetId);
  if (Engine::ValidEnemy(focus, p.AttackRange() + focus.BoundingRadius()) &&
      args.Target.IsMinion() &&
      static_cast<int>(args.Target.NetworkId()) != FocusTargetId)
    args.Process = false;
}
inline void
OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs &args) {
  if (ControllerHelpers::CaptureGapcloser(args, GapcloserTargetId, GapcloserEnd,
                                          GapcloserExpireTick, 760, 950)) {
    IncomingThreatId = GapcloserTargetId;
    IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 750);
    IncomingImpactTick = Now() + 180;
  }
}
inline void OnDraw() {
  const auto p = GameObjects::Player();
  if (!p.IsValid() || !BoolValue(CoachMenu, "DrawRanges", false))
    return;
  Drawing::DrawCircle(p.Position(), kQRange, 0xFF58C9FFu, 1.6f, 48);
  if (EFirstPlaced && !EFirstPosition.IsZero())
    Drawing::DrawCircle(EFirstPosition, 45, 0xFFD7ECFFu, 2, 32);
  if (LastBladeWall.Valid && BladeWallExpireTick > Now())
    Drawing::DrawLine(LastBladeWall.Start, LastBladeWall.End, 0xFF75B9FFu, 3);
}

inline void BuildMenu(Menu *root) {
  if (!root)
    return;
  TacticsMenu = root->AddSubMenu(
      new Menu("IreliaOneTrick", "Irelia blade dancer mechanics"));
  TacticsMenu->Add(new MenuSlider(
      "ManualOwnershipMs", "Yield after player spell (ms)", 520, 180, 1200));
  TacticsMenu->Add(new MenuSlider("EmergencyHP", "Defensive W health threshold",
                                  32, 10, 75));
  TacticsMenu->Add(new MenuSlider("MaxDashEnemies",
                                  "Maximum enemies at Q endpoint", 2, 1, 5));
  TacticsMenu->Add(
      new MenuBool("GapcloseMinions", "Use reset minions toward target", true));
  QMenu =
      TacticsMenu->AddSubMenu(new Menu("IreliaQ", "Bladesurge reset policy"));
  QMenu->Add(new MenuBool("OnlyResetOrLethal",
                          "Require mark, kill or lethal dash", true));
  WMenu = TacticsMenu->AddSubMenu(new Menu("IreliaW", "Defiant Dance charge"));
  WMenu->Add(new MenuBool("BlockCommittedDamage",
                          "Charge into committed damage", true));
  EMenu =
      TacticsMenu->AddSubMenu(new Menu("IreliaE", "Flawless Duet geometry"));
  EMenu->Add(
      new MenuBool("Interrupt", "Use E on interruptible channels", true));
  RMenu =
      TacticsMenu->AddSubMenu(new Menu("IreliaR", "Vanguard's Edge policy"));
  RMenu->Add(new MenuSlider("TargetHP", "R target HP threshold", 62, 15, 100));
  RMenu->Add(
      new MenuSlider("MinimumTargets", "R minimum nearby targets", 2, 1, 5));
  FarmMenu =
      TacticsMenu->AddSubMenu(new Menu("IreliaFarm", "Reset-safe farming"));
  FarmMenu->Add(new MenuSlider("ManaReserve", "Mana reserve", 45, 0, 180));
  FarmMenu->Add(
      new MenuSlider("HarassMana", "Harass minimum mana percent", 38, 0, 100));
  CoachMenu = TacticsMenu->AddSubMenu(
      new Menu("IreliaCoach", "Blade route visualization"));
  CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q, E and blade wall", false));
}
inline void OnLoad() {
  PassiveStacks = PassiveExpireTick = 0;
  Marks = {};
  WCharging = WManual = EFirstPlaced = EManual = false;
  WChargeStartTick = WTargetId = EFirstExpireTick = ETargetId = 0;
  EFirstPosition = LastEAim = LastRAim = {};
  LastBladeWall = {};
  BladeWallExpireTick = FocusTargetId = IncomingThreatId = IncomingThreatUntil =
      IncomingImpactTick = GapcloserTargetId = GapcloserExpireTick =
          InterruptTargetId = InterruptExpireTick = PlayerOverrideUntil =
              QCastTick = WCastTick = ECastTick = RCastTick = LastAutoTargetId =
                  LastAutoTick = 0;
  GapcloserEnd = {};
  ReconcileState();
}
inline void OnUnload() {
  TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
  Marks = {};
  WCharging = EFirstPlaced = false;
  LastBladeWall = {};
}

inline constexpr const char *Scenarios[] = {
    "Track four passive stacks through buff events and polling",
    "Expire passive estimates after the live stack window",
    "Preserve full-stack attacks for the cooperating focus target",
    "Use 600-range targeted Q with rank-specific minion bonus damage",
    "Reset Q only from a live mark or verified kill",
    "Reject Q into terrain, a new turret or excessive enemies",
    "Use lethal reset minions as target-directed gapclose nodes",
    "Require reset and cursor progress for flee Q",
    "Preserve mana for follow-up abilities",
    "Start W before targeted, crossing or gapcloser impact",
    "Keep manually started W under player ownership",
    "Scale W damage across its full charge",
    "Model physical reduction and half magic reduction",
    "Track E1 from owned and manual cast positions",
    "Recover E2 state by runtime-name polling",
    "Place E blades on opposite sides of prediction",
    "Require the finite blade segment to cross the hitbox",
    "Reject terrain and spell-shield E routes",
    "Use E for interrupts and gapcloser peel",
    "Track enemy marks by identity and expiry",
    "Require very-high R prediction and line intersection",
    "Reject R through projectile walls",
    "Track the perpendicular R blade wall",
    "Re-evaluate marked Q safety after R",
    "Hold unsafe nonlethal R-to-Q commits",
    "Prefer selected, marked focus, orbwalker, then selector targets",
    "Combo orders setup before reset dashes",
    "Harass avoids unmarked all-in Q",
    "Farm modes Q only reset units",
    "Flee uses cursor-progress reset nodes",
    "Automatic allows defense, interrupt and lethal Q only",
    "Yield after manual spell input",
    "Never automate summoners or items",
    "Pin Riot 26.15 / CommunityDragon 16.15 data"};
inline constexpr ChampionController Controller = [] {
  ChampionController c{};
  c.ChampionId = SDK::ChampionId::Irelia;
  c.ControllerId = "champion.kuroaio.ai.irelia.onetrick";
  c.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
  c.ResearchArtifact = "AI/Research/AIIrelia.md";
  c.ImplementationSummary =
      "Passive-stack reconciliation, reset-safe Q routing, charged W "
      "ownership, finite E stun geometry, R blade-wall tracking and safe mark "
      "dashes.";
  c.Scenarios = Scenarios;
  c.ScenarioCount = std::size(Scenarios);
  c.OwnsDecisionLoop = true;
  c.OnLoad = &OnLoad;
  c.OnUnload = &OnUnload;
  c.BuildMenu = &BuildMenu;
  c.OnUpdate = &OnUpdate;
  c.OnDraw = &OnDraw;
  c.OnProcessSpell = &OnProcessSpell;
  c.OnDoCast =
      &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId,
                                                      &LastAutoTick>;
  c.OnBuffAdd = &OnBuffAdd;
  c.OnBuffRemove = &OnBuffRemove;

  c.OnBeforeAttack = &OnBeforeAttack;
  c.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
  c.OnGapcloser = &OnGapcloser;
  c.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 1900, 250, 5500>;
  return c;
}();
} // namespace Plugins::KuroAIO::AI::Controllers::Irelia
