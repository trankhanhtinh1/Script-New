#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIKSanteGeometry.h"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::KSante {
using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::OrbwalkerHeroTarget;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::RuntimeNameContains;
using ControllerHelpers::SpellEnabled;

inline Menu *TacticsMenu = nullptr;
inline Menu *WMenu = nullptr;
inline Menu *EMenu = nullptr;
inline Menu *RMenu = nullptr;
inline Stance CurrentStance = Stance::Tank;
inline int AllOutExpireTick = 0;
inline int QStacks = 0;
inline int QStackExpireTick = 0;
inline int WChargeStartTick = 0;
inline int WTargetId = 0;
inline WPurpose WCurrentPurpose = WPurpose::Combo;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;

struct TerrainBlocksCacheEntry {
  int Tick = 0;
  Vector3 Start = {};
  Vector3 End = {};
  bool Blocks = false;
};

inline std::array<TerrainBlocksCacheEntry, 8> TerrainBlocksCaches = {};
inline std::size_t TerrainBlocksCacheCursor = 0;
using ControllerHelpers::Now;
inline bool AllOut() { return CurrentStance == Stance::AllOut; }
inline bool Ready(int i, Mode m, bool windup = false) {
  return i >= 0 && i < 4 && Engine::RuntimeSpells[i] &&
         Engine::RuntimeSpells[i]->IsReady() && SpellEnabled(i, m) &&
         (windup || !Orbwalker::IsWindingUp() || i == 1);
}

inline bool Throttled(int tick) { return Now() - tick < 55; }

inline bool TargetBlocked(const AIHeroClient &t) {
  return !Engine::ValidEnemy(t) || !t.IsTargetable() || t.IsInvulnerable() ||
         t.HasBuff("SivirE") || t.HasBuff("NocturneShroudofDarkness") ||
         t.HasBuff("MorganaE") || t.HasBuff("BlackShield") ||
         t.HasBuff("BansheesVeil") || t.HasBuff("EdgeOfNight") ||
         t.HasBuff("FioraW") || t.HasBuff("VladimirSanguinePool") ||
         t.HasBuff("FizzEIcon") || t.HasBuff("KayleR") ||
         t.HasBuff("kindredrnodeathbuff");
}

inline bool Unstoppable(const AIHeroClient &t) {
  return t.HasBuff("OlafRagnarok") || t.HasBuff("SionR") ||
         t.HasBuff("MalphiteR") || t.HasBuff("WarwickR") ||
         t.HasBuff("VolibearR") || t.HasBuff("OrnnW") || t.HasBuff("UdyrE2") ||
         t.HasBuff("SettR") || t.HasBuff("BriarE") || t.HasBuff("GalioE");
}


inline bool TerrainBlocks(const Vector3 &a, const Vector3 &b) {
  const Vec3 d = Direction2D(a, b);
  const float n = a.Distance2D(b);
  if (d.IsZero())
    return false;
  const int now = Now();
  for (const TerrainBlocksCacheEntry &entry : TerrainBlocksCaches) {
    if (entry.Tick <= 0 || now < entry.Tick || now - entry.Tick > 56 ||
        entry.Start.Distance2D(a) > 6.0f ||
        entry.End.Distance2D(b) > 6.0f) {
      continue;
    }
    return entry.Blocks;
  }
  bool blocks = false;
  for (float x = 35; x < n - 25; x += 24) {
    if (SDK::NavMesh::IsWall(a + d * x)) {
      blocks = true;
      break;
    }
  }
  TerrainBlocksCacheEntry &cache =
      TerrainBlocksCaches[TerrainBlocksCacheCursor % TerrainBlocksCaches.size()];
  TerrainBlocksCacheCursor =
      (TerrainBlocksCacheCursor + 1) % TerrainBlocksCaches.size();
  cache.Tick = now;
  cache.Start = a;
  cache.End = b;
  cache.Blocks = blocks;
  return blocks;
}

inline void ReconcileState() {
  const auto me = GameObjects::Player();
  if (!me.IsValid())
    return;
  const int now = Now();
  const bool r = me.HasBuff("KsanteRTransform") ||
                 me.HasBuff("KSanteRTransform") || me.HasBuff("KSanteRBuff");
  if (r) {
    CurrentStance = Stance::AllOut;
    if (AllOutExpireTick <= now)
      AllOutExpireTick = now + kAllOutDurationMs;
  } else if ((AllOutExpireTick > 0 && now >= AllOutExpireTick) ||
             (AllOut() && !RuntimeNameContains(1, "AllOut") &&
              now > LastRCastTick + 900)) {
    CurrentStance = Stance::Tank;
    AllOutExpireTick = 0;
  }
  const bool q3 = me.HasBuff("KSanteQ3") || RuntimeNameContains(0, "KSanteQ3");
  QStacks = ReconcileQStacks(-1, q3, QStacks,
                             QStackExpireTick > 0 && now >= QStackExpireTick);
  if (QStacks == 0 && QStackExpireTick <= now)
    QStackExpireTick = 0;
  const bool charging =
      Engine::RuntimeSpells[1] && Engine::RuntimeSpells[1]->IsCharging();
  if (!charging && now > LastWCastTick + 180) {
    WChargeStartTick = WTargetId = 0;
  } else if (charging && WChargeStartTick == 0) {
    WChargeStartTick = now;
  }
}

inline AIHeroClient ResolveTarget(float range = 950) {
  const auto orb = OrbwalkerHeroTarget(range);
  return Engine::ValidEnemy(orb, range) ? orb : Engine::SelectTarget(range);
}

inline int SpellRank(int i) {
  return i >= 0 && i < 4 && Engine::RuntimeSpells[i]
             ? std::clamp(Engine::RuntimeSpells[i]->Level(), 1, 5)
             : 1;
}

inline bool QLethal(const AIHeroClient &t) {
  const auto me = GameObjects::Player();
  return me.IsValid() && t.IsValid() &&
         QRawDamage(SpellRank(0), me.TotalAttackDamage(), CurrentStance) >=
             t.Health() + t.AllShield();
}

inline bool Q3Collision(const AIHeroClient &t, const Vector3 &aim) {
  if (!IsQ3(QStacks))
    return false;
  const auto me = GameObjects::Player();
  for (const auto &e : GameObjects::EnemyHeroes()) {
    if (!Engine::ValidEnemy(e, kQ3Range + 100) ||
        e.NetworkId() == t.NetworkId())
      continue;
    const auto p =
        ProjectPointToSegment2D(PredictPosition(e, .3f), me.Position(), aim);
    if (p.T < .98f && p.Distance <= kQHalfWidth + e.BoundingRadius())
      return true;
  }
  return false;
}

inline bool CastQ(const AIHeroClient &t, Mode m, bool defensive = false) {
  if (TargetBlocked(t) || !Ready(0, m) || Throttled(LastQCastTick))
    return false;
  const auto me = GameObjects::Player();
  const auto pred = Engine::RuntimeSpells[0]->GetPrediction(t);
  Vector3 aim = pred.GetCastPosition();
  if (!aim.IsValid() || aim.IsZero())
    aim = PredictPosition(t, IsQ3(QStacks) ? .45f : .35f);
  const float range = QRange(QStacks);
  if (!aim.IsValid() ||
      me.Position().Distance2D(aim) > range + t.BoundingRadius() ||
      pred.Hitchance <
          (IsQ3(QStacks) ? SDK::HitChance::High : SDK::HitChance::Medium) ||
      TerrainBlocks(me.Position(), aim) || Q3Collision(t, aim))
    return false;
  ModeContext c{};
  c.AttackWindingUp = Orbwalker::IsWindingUp();
  c.Defensive = defensive;
  c.KillSecure = QLethal(t);
  if (!MayUseAbility(c))
    return false;
  const bool q3 = IsQ3(QStacks);
  if (!Engine::ControllerCastPosition(0, aim))
    return false;
  LastQCastTick = Now();
  QStacks = QStacksAfterCast(QStacks, q3);
  QStackExpireTick = QStacks ? Now() + kQStackDurationMs : 0;
  return true;
}

inline bool EndpointSafe(const Vector3 &p, const AIHeroClient &t,
                         bool defensive, bool lethal, bool ally = false,
                         bool allyAlive = true, bool allyRange = true) {
  const auto me = GameObjects::Player();
  DashSafetyContext c{};
  c.EndpointValid = p.IsValid() && !p.IsZero();
  c.EndpointWalkable = c.EndpointValid && !SDK::NavMesh::IsWall(p);
  c.EnemyTurret = Engine::UnderEnemyTurret(p);
  c.StartedUnderEnemyTurret = Engine::UnderEnemyTurret(me.Position());
  c.PointClickThreat = ControllerHelpers::HasReadyPointClickThreatAt(p);
  c.DashHazard = ControllerHelpers::HasReadyDashHazardAt(p);
  c.AllyTarget = ally;
  c.AllyAlive = allyAlive;
  c.AllyInRange = allyRange;
  c.Lethal = lethal;
  c.Defensive = defensive;
  c.EnemiesAtEndpoint = Engine::CountEnemiesAt(p, 625);
  c.AlliesAtEndpoint = Engine::CountAlliesAt(p, 750);
  c.MaximumEnemies = Slider(EMenu, "MaxEndpointEnemies", 2);
  return DashSafe(c) &&
         (defensive || lethal || !t.IsValid() ||
          Engine::PositionDangerScore(p, t, Engine::ResolvedSpecs[2]) > -10000);
}

inline bool StartW(const AIHeroClient &t, Mode m, WPurpose purpose) {
  if (TargetBlocked(t) || !Ready(1, m, true) || Throttled(LastWCastTick) ||
      (Engine::RuntimeSpells[1] && Engine::RuntimeSpells[1]->IsCharging()))
    return false;
  const auto pred = Engine::RuntimeSpells[1]->GetPrediction(t);
  Vector3 aim = pred.GetCastPosition();
  if (!aim.IsValid())
    aim = PredictPosition(t, .45f);
  if (!aim.IsValid() || pred.Hitchance < SDK::HitChance::High ||
      TerrainBlocks(GameObjects::Player().Position(), aim) ||
      (Orbwalker::IsWindingUp() && purpose == WPurpose::Combo && !QLethal(t)))
    return false;
  Engine::ArmControllerCast(1);
  if (!Engine::RuntimeSpells[1]->StartCharging(aim)) {
    Engine::CancelControllerCast(1);
    return false;
  }
  Engine::MarkSuccessfulCast(1);
  WChargeStartTick = LastWCastTick = Now();
  WTargetId = (int)t.NetworkId();
  WCurrentPurpose = purpose;
  return true;
}

inline bool ReleaseW(const AIHeroClient &fallback) {
  if (!Engine::RuntimeSpells[1] || !Engine::RuntimeSpells[1]->IsCharging() ||
      !WChargeStartTick)
    return false;
  auto t = HeroByNetworkId(WTargetId);
  if (!Engine::ValidEnemy(t, 900))
    t = fallback;
  if (!Engine::ValidEnemy(t, 900))
    return false;
  const int elapsed = Now() - WChargeStartTick;
  const float range = WRange(elapsed, CurrentStance);
  const auto pred = Engine::RuntimeSpells[1]->GetPrediction(t);
  Vector3 aim = pred.GetCastPosition();
  if (!aim.IsValid())
    aim = PredictPosition(t, .25f);
  const auto me = GameObjects::Player();
  const Vec3 d = Direction2D(me.Position(), aim);
  if (d.IsZero())
    return false;
  const Vector3 end =
      me.Position() + d * std::min(range, me.Position().Distance2D(aim));
  const bool def = WCurrentPurpose != WPurpose::Combo;
  WReleaseContext c{};
  c.Charging = true;
  c.PredictionAccepted = pred.Hitchance >= SDK::HitChance::High &&
                         !TerrainBlocks(me.Position(), aim);
  c.TargetInCurrentRange =
      me.Position().Distance2D(aim) <= range + t.BoundingRadius();
  c.EndpointSafe = EndpointSafe(end, t, def, QLethal(t));
  c.Interrupt = WCurrentPurpose == WPurpose::Interrupt;
  c.Peel =
      WCurrentPurpose == WPurpose::Peel || WCurrentPurpose == WPurpose::Flee;
  c.Lethal = QLethal(t) && t.HealthPercent() < 22;
  c.Expiring = elapsed >= kWMaximumHoldMs - 55;
  c.AttackWindingUp = Orbwalker::IsWindingUp();
  c.ElapsedMs = elapsed;
  c.CurrentStance = CurrentStance;
  if (!MayReleaseW(c) || !LineHits(me.Position(), me.Position() + d * range,
                                   aim, t.BoundingRadius(), kWHalfWidth))
    return false;
  Engine::ArmControllerCast(1);
  if (!Engine::RuntimeSpells[1]->ShootChargedSpell(aim)) {
    Engine::CancelControllerCast(1);
    return false;
  }
  Engine::MarkSuccessfulCast(1);
  LastWCastTick = Now();
  WChargeStartTick = WTargetId = 0;
  return true;
}

inline AIHeroClient BestDashAlly() {
  const auto me = GameObjects::Player();
  AIHeroClient best{};
  float score = -FLT_MAX;
  for (const auto &a : GameObjects::AllyHeroes()) {
    if (!Engine::ValidAlly(a, kEAllyRange + 40) ||
        a.NetworkId() == me.NetworkId() ||
        !EndpointSafe(a.Position(), {}, true, false, true, true, true))
      continue;
    const float s = Engine::CountAlliesAt(a.Position(), 650) * 110.f -
                    Engine::CountEnemiesAt(a.Position(), 600) * 170.f;
    if (s > score) {
      best = a;
      score = s;
    }
  }
  return best;
}

inline bool CastEAlly(const AIHeroClient &a, Mode m) {
  const auto me = GameObjects::Player();
  if (!Ready(2, m, true) || Throttled(LastECastTick) ||
      !Engine::ValidAlly(a, kEAllyRange + 35) ||
      a.NetworkId() == me.NetworkId() ||
      !EndpointSafe(a.Position(), {}, true, false, true, true,
                    me.Position().Distance2D(a.Position()) <=
                        kEAllyRange + a.BoundingRadius()))
    return false;
  if (!Engine::ControllerCastUnit(2, a))
    return false;
  LastECastTick = Now();
  return true;
}

inline bool CastESelf(const Vector3 &requested, Mode m, const AIHeroClient &t,
                      bool defensive, bool lethal = false) {
  if (!Ready(2, m, defensive) || Throttled(LastECastTick))
    return false;
  const auto me = GameObjects::Player();
  Vector3 end = ClampDashEndpoint(me.Position(), requested,
                                  AllOut() ? kEAllOutSelfRange : kESelfRange);
  if (!end.IsValid())
    return false;
  end.y = SDK::NavMesh::GetHeightForPosition(end);
  if (!EndpointSafe(end, t, defensive, lethal) ||
      !Engine::ControllerCastPosition(2, end))
    return false;
  LastECastTick = Now();
  return true;
}

inline IsolationResult IsolationFor(const AIHeroClient &t) {
  IsolationContext c{};
  c.Ready = Ready(3, Mode::Combo);
  c.TargetValid = !TargetBlocked(t) && Engine::ValidEnemy(t, kRRange + 30);
  c.TargetUnstoppable = Unstoppable(t);
  c.TargetSpellShielded = t.HasBuff("SivirE") || t.HasBuff("MorganaE") ||
                          t.HasBuff("BansheesVeil") || t.HasBuff("EdgeOfNight");
  if (!c.TargetValid)
    return EvaluateIsolation(c);
  const auto me = GameObjects::Player();
  const Vec3 d = Direction2D(me.Position(), t.Position());
  Vector3 landing = t.Position() + d * 450;
  bool wall = false;
  for (float x = 45; x <= 430; x += 28) {
    const Vector3 sample = t.Position() + d * x;
    if (SDK::NavMesh::IsWall(sample)) {
      wall = true;
      landing = sample + d * 220;
    }
  }
  landing.y = SDK::NavMesh::GetHeightForPosition(landing);
  c.WallBehindTarget = wall;
  c.LandingWalkable =
      landing.IsValid() && !landing.IsZero() && !SDK::NavMesh::IsWall(landing);
  c.LandingUnderEnemyTurret = Engine::UnderEnemyTurret(landing);
  c.PlayerUnderEnemyTurret = Engine::UnderEnemyTurret(me.Position());
  c.PlayerExitAvailable = Ready(2, Mode::Combo, true) ||
                          Engine::CountAlliesAt(landing, 850) > 0 ||
                          Engine::CountEnemiesAt(landing, 650) <= 1;
  c.Lethal = t.HealthPercent() <= Slider(RMenu, "LethalHp", 24);
  c.EnemiesBefore = Engine::CountEnemiesAt(t.Position(), 750);
  c.EnemiesAfter = Engine::CountEnemiesAt(landing, 750);
  c.AlliesAfter = Engine::CountAlliesAt(landing, 900);
  c.SeparationGain = wall ? 450.f : 0.f;
  return EvaluateIsolation(c);
}

inline bool CastR(const AIHeroClient &t, Mode m) {
  if (AllOut() || !Ready(3, m) || Throttled(LastRCastTick) || TargetBlocked(t))
    return false;
  const auto result = IsolationFor(t);
  if (!result.Cast ||
      result.Score < Slider(RMenu, "MinimumIsolationScore", 250) ||
      !Engine::ControllerCastUnit(3, t))
    return false;
  LastRCastTick = Now();
  CurrentStance = Stance::AllOut;
  AllOutExpireTick = Now() + kAllOutDurationMs;
  return true;
}

inline bool TryReactive(const AIHeroClient &fallback, Mode m) {
  auto t = HeroByNetworkId(InterruptTargetId);
  if (InterruptExpireTick > Now() && Engine::ValidEnemy(t, 850)) {
    if (Engine::RuntimeSpells[1] && Engine::RuntimeSpells[1]->IsCharging())
      return ReleaseW(t);
    if (StartW(t, m, WPurpose::Interrupt) ||
        (IsQ3(QStacks) && CastQ(t, m, true)))
      return true;
  }
  t = HeroByNetworkId(GapcloserTargetId);
  if (GapcloserExpireTick > Now() && Engine::ValidEnemy(t, 850)) {
    if (StartW(t, m, WPurpose::Peel) ||
        CastESelf(t.Position(), m, t, true))
      return true;
  }
  if (IncomingHardCCUntil > Now() || IncomingThreatUntil > Now()) {
    const auto a = BestDashAlly();
    if (a.IsValid() && CastEAlly(a, m))
      return true;
    return CastESelf(fallback.Position(), m, fallback, true);
  }
  return false;
}

inline bool Combo(const AIHeroClient &t) {
  if (!Engine::ValidEnemy(t))
    return false;
  if (Engine::RuntimeSpells[1] && Engine::RuntimeSpells[1]->IsCharging())
    return ReleaseW(t);
  if (!AllOut() && CastR(t, Mode::Combo))
    return true;
  if (IsQ3(QStacks) && CastQ(t, Mode::Combo))
    return true;
  const float d = GameObjects::Player().Position().Distance2D(t.Position());
  if (d > 340 && CastESelf(t.Position(), Mode::Combo, t, false, QLethal(t)))
    return true;
  if (CastQ(t, Mode::Combo))
    return true;
  return d <= 500 && StartW(t, Mode::Combo, WPurpose::Combo);
}

inline bool Flee(const AIHeroClient &t) {
  if (Engine::RuntimeSpells[1] && Engine::RuntimeSpells[1]->IsCharging())
    return ReleaseW(t);
  const auto a = BestDashAlly();
  if (a.IsValid() && CastEAlly(a, Mode::Flee))
    return true;
  if (CastESelf(Game::CursorPos(), Mode::Flee, t, true))
    return true;
  if (Engine::ValidEnemy(t, 600) && StartW(t, Mode::Flee, WPurpose::Flee))
    return true;
  return Engine::ValidEnemy(t) && IsQ3(QStacks) && CastQ(t, Mode::Flee, true);
}

inline bool OnUpdate(Mode m, const AIHeroClient &) {
  ReconcileState();
  const auto t = ResolveTarget(kQ3Range + 100);
  if (Engine::RuntimeSpells[1] && Engine::RuntimeSpells[1]->IsCharging()) {
    (void)ReleaseW(t);
    return true;
  }
  if (TryReactive(t, m == Mode::None ? Mode::Automatic : m))
    return true;
  switch (m) {
  case Mode::Combo:
    (void)Combo(t);
    break;
  case Mode::Harass:
    if (!AllOut())
      (void)CastQ(t, m);
    break;
  case Mode::LaneClear:
    (void)Engine::TryFarm(m);
    break;
  case Mode::Jungle:
    (void)Engine::TryFarm(m);
    break;
  case Mode::LastHit:
    (void)Engine::TryFarm(m);
    break;
  case Mode::Flee:
    (void)Flee(t);
    break;
  case Mode::Automatic: {
    ModeContext c{};
    c.Defensive = IncomingThreatUntil > Now() || GapcloserExpireTick > Now();
    c.Interrupt = InterruptExpireTick > Now();
    c.KillSecure = Engine::ValidEnemy(t) && QLethal(t);
    if (AutomaticAllowed(c) && c.KillSecure)
      (void)CastQ(t, m);
    break;
  }
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
        a, 220, 110, 250, 280, 260, 1500, 450);
    if (threat.Valid && threat.CrossesPlayer)
      IncomingThreatUntil = now + 650;
    if (threat.Valid && threat.CrossesPlayer && threat.LikelyHardCrowdControl)
      IncomingHardCCUntil = now + 750;
    return;
  }
  const int s = a.Slot;
  if (s == 0) {
    LastQCastTick = now;
    const bool q3 = Engine::TextContains(a.SpellName, "Q3");
    QStacks = QStacksAfterCast(QStacks, q3);
    QStackExpireTick = QStacks ? now + kQStackDurationMs : 0;
  } else if (s == 1) {
    LastWCastTick = now;
    if (WChargeStartTick == 0)
      WChargeStartTick = now;
  } else if (s == 2)
    LastECastTick = now;
  else if (s == 3) {
    LastRCastTick = now;
    if (Engine::TextContains(a.SpellName, "EndEarly")) {
      CurrentStance = Stance::Tank;
      AllOutExpireTick = 0;
    } else {
      CurrentStance = Stance::AllOut;
      AllOutExpireTick = now + kAllOutDurationMs;
    }
  }
}

inline void UpdateBuff(const SDK::Events::BuffEventArgs &a, bool add) {
  if (!IsLocalPlayer(a.Sender))
    return;
  const int now = Now();
  if (Engine::TextContains(a.BuffName, "KsanteRTransform") ||
      Engine::TextContains(a.BuffName, "KSanteRTransform")) {
    CurrentStance = add ? Stance::AllOut : Stance::Tank;
    AllOutExpireTick = add ? now + ControllerHelpers::RemainingMilliseconds(
                                       a.EndTime, kAllOutDurationMs, 500, 17000)
                           : 0;
  }
  if (Engine::TextContains(a.BuffName, "KSanteQ3")) {
    QStacks = add ? 2 : 0;
    QStackExpireTick = add ? now + ControllerHelpers::RemainingMilliseconds(
                                       a.EndTime, kQStackDurationMs, 300, 8000)
                           : 0;
  } else if (add && Engine::TextContains(a.BuffName, "KSanteQ")) {
    QStacks = std::min(2, QStacks + 1);
    QStackExpireTick = now + ControllerHelpers::RemainingMilliseconds(
                                 a.EndTime, kQStackDurationMs, 300, 8000);
  }
  if (Engine::TextContains(a.BuffName, "KSanteW")) {
    if (add && WChargeStartTick == 0)
      WChargeStartTick = now;
    if (!add) {
      WChargeStartTick = WTargetId = 0;
    }
  }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs &a) {
  if (Engine::RuntimeSpells[1] && Engine::RuntimeSpells[1]->IsCharging())
    a.Process = false;
}

inline void OnDraw() {
  if (!Bool(TacticsMenu, "DrawRanges", false))
    return;
  const auto me = GameObjects::Player();
  if (!me.IsValid())
    return;
  Drawing::DrawCircle(me.Position(), QRange(QStacks),
                      IsQ3(QStacks) ? 0xFFFFA040u : 0xFF55B8FFu, 1.5f, 40);
  Drawing::DrawCircle(me.Position(), AllOut() ? kEAllOutSelfRange : kESelfRange,
                      0xFF70E0A0u, 1.f, 32);
}

inline void BuildMenu(Menu *root) {
  if (!root)
    return;
  TacticsMenu =
      root->AddSubMenu(new Menu("KSanteOneTrick", "K'Sante one-trick tactics"));
  TacticsMenu->Add(new MenuBool("DrawRanges", "Draw live Q/E ranges", false));
  WMenu = TacticsMenu->AddSubMenu(new Menu("PathMaker", "Path Maker charge"));
  WMenu->Add(new MenuSeparator("ChargeTiming",
                               "Path Maker charge timing"));
  EMenu = TacticsMenu->AddSubMenu(new Menu("Footwork", "Footwork safety"));
  EMenu->Add(new MenuSlider("MaxEndpointEnemies",
                            "Maximum enemies at E endpoint", 2, 1, 5));
  RMenu = TacticsMenu->AddSubMenu(new Menu("AllOut", "All Out isolation"));
  RMenu->Add(new MenuSlider("MinimumIsolationScore", "Minimum isolation score",
                            250, 100, 900));
  RMenu->Add(
      new MenuSlider("LethalHp", "Allow lethal R below HP (%)", 24, 5, 60));
}

inline void OnLoad() {
  CurrentStance = Stance::Tank;
  AllOutExpireTick = QStacks = QStackExpireTick = 0;
  WChargeStartTick = WTargetId = 0;
  IncomingThreatUntil = IncomingHardCCUntil = 0;
  GapcloserTargetId = GapcloserExpireTick = 0;
  GapcloserEndpoint = {};
  InterruptTargetId = InterruptExpireTick = 0;
  LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick =
      LastAutoTargetId = LastAutoTick = 0;
  TerrainBlocksCaches.fill({});
  TerrainBlocksCacheCursor = 0;
}

inline void OnUnload() {
  TacticsMenu = WMenu = EMenu = RMenu = nullptr;
  TerrainBlocksCaches.fill({});
  TerrainBlocksCacheCursor = 0;
}

inline constexpr const char *Scenarios[] = {
    "Reconcile Tank and All Out stance from R events, buffs, runtime names and "
    "expiry polling",
    "Reconcile Q stacks from casts, Q3 buff and six-second expiry",
    "Use Q3 live range with high prediction, terrain and champion-collision "
    "rejection",
    "Preserve orbwalker windup except defensive or lethal casts",
    "Own W start, target, purpose, charge threshold and release",
    "Release an observed W charge after minimum timing, prediction and safety checks",
    "Respect 400 ms Tank and 750 ms All Out W minimum charge",
    "Release W for interrupt, peel, lethal, full charge or expiry",
    "Capture gapclosers and interruptible channels for W/Q3 peel",
    "Reject W/E endpoints into walls, turrets, point-click threats or "
    "outnumbered pockets",
    "Use E self range 250 Tank and 400 All Out",
    "Use ally E only on a living in-range ally with a safe endpoint",
    "Reject R into unstoppable or spell-shielded targets",
    "Sample terrain behind R target and require measurable isolation",
    "Require R exit, landing safety, numerical safety and terrain separation",
    "Combo builds Q3 and commits W/E/R only through safety gates",
    "Harass preserves All Out, W and E",
    "LaneClear, Jungle and LastHit use resource-aware farm execution",
    "Flee prioritizes ally E, self E, W peel and Q3",
    "Automatic allows defensive, interrupt and kill-secure reactions only",
    "Use autonomous target selection and resume after observed spell events",
    "Never automate summoners, items or movement orders"};

inline constexpr ChampionController Controller = [] {
  ChampionController c{};
  c.ChampionId = SDK::ChampionId::KSante;
  c.ControllerId = "champion.kuroaio.ai.ksante.onetrick";
  c.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
  c.ResearchArtifact = "AI/Research/AIKSante.md";
  c.ImplementationSummary =
      "Event/poll reconciled All Out and Q stacks; W charge timing and interrupt; "
      "safe self/ally E; terrain-scored R isolation; mode and orbwalker safety.";
  c.Scenarios = Scenarios;
  c.ScenarioCount = std::size(Scenarios);
  c.OwnsDecisionLoop = true;
  c.OnLoad = &OnLoad;
  c.OnUnload = &OnUnload;
  c.BuildMenu = &BuildMenu;
  c.OnUpdate = &OnUpdate;
  c.OnDraw = &OnDraw;
  c.OnProcessSpell = &OnProcessSpell;
  c.OnBuffAdd = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuff, true>;
  c.OnBuffRemove = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuff, false>;

  c.OnBeforeAttack = &OnBeforeAttack;
  c.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
  c.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, 700, 950>;
  c.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 1400, 250, 5000>;
  return c;
}();
} // namespace Plugins::KuroAIO::AI::Controllers::KSante
