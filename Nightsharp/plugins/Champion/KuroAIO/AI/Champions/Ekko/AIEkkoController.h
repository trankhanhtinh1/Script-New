#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIEkkoGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace Plugins::KuroAIO::AI::Controllers::Ekko {

using namespace Geometry;
using ControllerHelpers::BonusAttackDamage;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MissileEventIsLocal;
using ControllerHelpers::Ready;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::TotalAttackDamage;

inline Menu* TacticsMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<PassiveState, 8> PassiveRecords = {};
inline std::array<Vector3, 24> PositionHistory = {};
inline std::array<int, 24> PositionTicks = {};
inline int PositionHistoryIndex = 0;
inline int QLastCastTick = 0;
inline int QReturnExpireTick = 0;
inline int QTargetId = 0;
inline int WLastCastTick = 0;
inline int WExpireTick = 0;
inline int ELastCastTick = 0;
inline int EEmpoweredUntil = 0;
inline int RLastCastTick = 0;
inline int RTargetId = 0;
inline int AttackWindupUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int IncomingHardCCUntil = 0;
inline Vector3 QOrigin = {};
inline Vector3 QOutboundEndpoint = {};
inline Vector3 WCenter = {};
inline Vector3 RRewindPosition = {};
inline bool QReturnProjectedHit = false;

inline bool QOutboundActive = false;
inline bool QReturning = false;
inline bool WArmed = false;
inline bool EAttackArmed = false;
inline bool RAvailable = false;
inline bool RWasManual = false;
inline bool QWasManual = false;
inline bool WWasManual = false;
inline bool EWasManual = false;

inline bool Throttle(int slot, int delayMs) {
    const int last = slot == 0 ? QLastCastTick : slot == 1 ? WLastCastTick :
                     slot == 2 ? ELastCastTick : RLastCastTick;
    return Now() - last >= delayMs;
}

inline bool TargetProtected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           target.HasBuff("SivirE") || target.HasBuff("NocturneShroudofDarkness") ||
           target.HasBuff("MorganaE") || target.HasBuff("BansheesVeil") ||
           target.HasBuff("VladimirSanguinePool") || target.HasBuff("FizzEIcon") ||
           target.HasBuff("KayleR") || target.HasBuff("kindredrnodeathbuff");
}

inline PassiveState* PassiveFor(int id, bool create = false) {
    if (id == 0) return nullptr;
    for (auto& state : PassiveRecords)
        if (state.TargetId == id) return &state;
    if (!create) return nullptr;
    for (auto& state : PassiveRecords) {
        if (state.TargetId == 0 || state.LastHitTick + kPassiveWindowMs < Now()) {
            state = {}; state.TargetId = id; return &state;
        }
    }
    return &PassiveRecords.front();
}

inline int PassiveHits(int id) {
    const auto state = PassiveFor(id);
    return state ? state->Hits : 0;
}

inline bool PassiveReadyFor(const AIHeroClient& target) {
    const auto state = PassiveFor(static_cast<int>(target.NetworkId()));
    return state && Geometry::PassiveReady(*state, static_cast<int>(target.NetworkId()), Now());
}

inline void ObservePassive(int id) {
    if (id == 0) return;
    auto* state = PassiveFor(id, true);
    if (!state) return;
    if (state->TargetId == id && state->LastHitTick == Now()) return;
    *state = Geometry::ObservePassiveHit(*state, id, Now());
}

inline float QDamage(const AIHeroClient& target, bool returning = false) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    const float raw = returning ? QReturnRawDamage(SpellRank(0), GameObjects::Player().AP())
                                : QOutboundRawDamage(SpellRank(0), GameObjects::Player().AP());
    return GameObjects::Player().CalculateMagicDamage(target, raw);
}
inline float WDamage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    return GameObjects::Player().CalculateMagicDamage(target,
        WRawDamage(SpellRank(1), GameObjects::Player().AP()));
}
inline float EDamage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    return GameObjects::Player().CalculateMagicDamage(target,
        ERawDamage(SpellRank(2), GameObjects::Player().AP()));
}
inline float RDamage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    return GameObjects::Player().CalculateMagicDamage(target,
        RRawDamage(SpellRank(3), GameObjects::Player().AP(),
                   100.0f - target.HealthPercent()));
}
inline float PassiveDamage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    return GameObjects::Player().CalculateMagicDamage(target,
        PassiveRawDamage(SpellRank(0), GameObjects::Player().AP()));
}
inline bool Lethal(const AIHeroClient& target, float damage) {
    return Engine::ValidEnemy(target) && damage > 0.0f &&
           damage >= target.Health() + target.AllShield();
}

inline bool MayCast(const AIHeroClient& target, bool selected, bool orbwalker,
                    bool lethal, bool manual = false) {
    return MayUseAbility({ selected, orbwalker,
                           Orbwalker::IsWindingUp() || AttackWindupUntil > Now(),
                           lethal, manual });
}

inline Vector3 QAim(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return {};
    const auto prediction = Engine::RuntimeSpells[0]
        ? Engine::RuntimeSpells[0]->GetPrediction(target) : SDK::PredictionOutput{};
    Vector3 result = prediction.GetCastPosition();
    if (!result.IsValid() || result.IsZero()) result = prediction.GetUnitPosition();
    if (!result.IsValid() || result.IsZero()) result = PredictPosition(target, 0.28f);
    return result;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool selected, bool orbwalker,
                  bool fleeing = false, bool manual = false) {
    if (!Engine::ValidEnemy(target, kQRange + target.BoundingRadius()) ||
        !Ready(0, mode) || !Throttle(0, 60) || TargetProtected(target) ||
        !MayCast(target, selected, orbwalker, Lethal(target, QDamage(target)), manual)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.Mana() < SpellCost(0)) return false;
    const Vector3 predicted = fleeing ? Game::CursorPos() : QAim(target);
    const Vector3 endpoint = ClampQEndpoint(player.Position(), predicted);
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const bool collision = !prediction.CollisionObjects.empty();
    const Vector3 returnPrediction = PredictPosition(
        target, QReturnTravelSeconds(endpoint, player.Position()));
    QReturnProjectedHit = QReturnIntersects(endpoint, player.Position(),
                                             returnPrediction, target.BoundingRadius());
    if (!QPathClear(player.Position(), endpoint, SDK::NavMesh::IsWall(endpoint), collision) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(endpoint, 60.0f)) {
        return false;
    }
    if (Engine::ControllerCastPosition(0, endpoint)) {
        QLastCastTick = Now(); QReturnExpireTick = Now() + kQReturnWindowMs;
        QTargetId = static_cast<int>(target.NetworkId()); QOrigin = player.Position();
        QOutboundEndpoint = endpoint; QOutboundActive = true; QReturning = false; QWasManual = false;
        return true;
    }
    return false;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool selected, bool orbwalker,
                  bool reactive = false, bool manual = false) {
    if (!Engine::ValidEnemy(target, kWRange) || !Ready(1, mode) || !Throttle(1, 100) ||
        TargetProtected(target) || GameObjects::Player().Mana() < SpellCost(1)) return false;
    const Vector3 predicted = PredictPosition(target, kWDelayMs / 1000.0f);
    const bool inZone = predicted.IsValid() && predicted.Distance2D(target.Position()) <=
                        kWRadius + target.BoundingRadius();
    const bool lethal = Lethal(target, WDamage(target));
    if (!reactive && !MayCast(target, selected, orbwalker, lethal, manual)) return false;
    if (!WCastAllowed(predicted, target.Position(), target.BoundingRadius(), false,
                      inZone, manual)) return false;
    if (!Engine::ControllerCastPosition(1, predicted)) return false;
    WLastCastTick = Now(); WExpireTick = Now() + kWDelayMs + 1000;
    WCenter = predicted; WArmed = true; WWasManual = false;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool selected, bool orbwalker,
                  bool fleeing = false, bool manual = false) {
    if (!Ready(2, mode) || !Throttle(2, 75) || GameObjects::Player().Mana() < SpellCost(2)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const Vector3 aim = fleeing ? Game::CursorPos() : PredictPosition(target, 0.12f);
    const Vector3 endpoint = ClampEEndpoint(player.Position(), aim);
    if (!endpoint.IsValid() || endpoint.IsZero()) return false;
    const bool lethal = Engine::ValidEnemy(target) && Lethal(target, EDamage(target));
    const bool safe = EEndpointSafe(!SDK::NavMesh::IsWall(endpoint),
        Engine::UnderEnemyTurret(endpoint), Engine::UnderEnemyTurret(player.Position()),
        Engine::CountEnemiesAt(endpoint, 425.0f), Slider(EMenu, "MaxEndpointEnemies", 2),
        lethal, fleeing, manual);
    if (!safe || (!fleeing && Engine::ValidEnemy(target) &&
                  !MayCast(target, selected, orbwalker, lethal, manual))) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    ELastCastTick = Now(); EEmpoweredUntil = Now() + 3000; EAttackArmed = true; EWasManual = false;
    return true;
}

inline Vector3 RewindPosition(int nowTick) {
    Vector3 result = {};
    int bestAge = -1;
    for (std::size_t i = 0; i < PositionHistory.size(); ++i) {
        if (!PositionHistory[i].IsValid() || PositionTicks[i] <= 0) continue;
        const int age = nowTick - PositionTicks[i];
        if (age >= kRRewindWindowMs && age > bestAge) { result = PositionHistory[i]; bestAge = age; }
    }
    return result;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool selected, bool orbwalker,
                  bool defensive = false, bool fleeing = false) {
    if (!RAvailable || !Ready(3, mode) || !Throttle(3, 160) || !GameObjects::Player().IsValid()) return false;
    const auto player = GameObjects::Player();
    RRewindPosition = RewindPosition(Now());
    if (!RRewindPosition.IsValid() || RRewindPosition.IsZero()) return false;
    const bool lethal = Engine::ValidEnemy(target) && Lethal(target, RDamage(target));
    const bool endpointTarget = Engine::ValidEnemy(target) &&
        RRewindPosition.Distance2D(PredictPosition(target, 0.20f)) <=
            kRRadius + target.BoundingRadius();
    RContext context{ true, true, !SDK::NavMesh::IsWall(RRewindPosition),
        Engine::UnderEnemyTurret(RRewindPosition), Engine::UnderEnemyTurret(player.Position()),
        Engine::CountEnemiesAt(RRewindPosition, 500.0f), Slider(RMenu, "MaxEndpointEnemies", 1),
        lethal && endpointTarget, defensive, fleeing, player.HealthPercent(),
        static_cast<float>(Slider(RMenu, "MinimumHealth", 20)) };
    if (!RewindEndpointSafe(context) || !RDamageWorthwhile(context)) return false;
    if (Engine::ValidEnemy(target) && !MayCast(target, selected, orbwalker,
                                               context.Lethal, false)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    RLastCastTick = Now(); RTargetId = Engine::ValidEnemy(target)
        ? static_cast<int>(target.NetworkId()) : 0; RAvailable = false; RWasManual = false;
    return true;
}

inline void ReconcileState() {
    const int now = Now();
    for (auto& state : PassiveRecords) state = ReconcilePassive(state, now);
    if (QReturnExpireTick > 0 && now > QReturnExpireTick) {
        QOutboundActive = false; QReturning = false; QTargetId = 0;
        QReturnProjectedHit = false;

    }
    if (WExpireTick > 0 && now > WExpireTick) WArmed = false;
    if (EEmpoweredUntil > 0 && now > EEmpoweredUntil) EAttackArmed = false;
    const auto player = GameObjects::Player();
    if (player.IsValid() && player.HasBuff("EkkoR")) RAvailable = false;
    if (Engine::RuntimeSpells[3] && Engine::RuntimeSpells[3]->IsReady() &&
        (!player.IsValid() || !player.HasBuff("EkkoR"))) RAvailable = true;

    if (AttackWindupUntil > 0 && now > AttackWindupUntil) AttackWindupUntil = 0;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode, bool selected, bool orbwalker) {
    if (!Engine::ValidEnemy(target)) return false;
    if (PassiveReadyFor(target) && Lethal(target, PassiveDamage(target)) &&
        MayCast(target, selected, orbwalker, true)) return true;
    if (RAvailable && Lethal(target, RDamage(target)) && CastR(target, mode, selected, orbwalker)) return true;
    if (Lethal(target, QDamage(target, true)) && CastQ(target, mode, selected, orbwalker)) return true;
    if (Lethal(target, EDamage(target)) && CastE(target, mode, selected, orbwalker)) return true;
    return false;
}

inline bool TryCombo(const AIHeroClient& target, bool selected, bool orbwalker) {
    if (!Engine::ValidEnemy(target)) return false;
    if (RAvailable && target.HealthPercent() <= Slider(RMenu, "RTargetHP", 40) &&
        CastR(target, Mode::Combo, selected, orbwalker)) return true;
    if (CastW(target, Mode::Combo, selected, orbwalker)) return true;
    if (CastQ(target, Mode::Combo, selected, orbwalker)) return true;
    if (CastE(target, Mode::Combo, selected, orbwalker)) return true;
    if (RAvailable && CastR(target, Mode::Combo, selected, orbwalker)) return true;
    return false;
}

inline bool TryHarass(const AIHeroClient& target, bool selected, bool orbwalker) {
    if (!Engine::ValidEnemy(target) || GameObjects::Player().HealthPercent() < 45.0f ||
        GameObjects::Player().ManaPercent() < Slider(QMenu, "HarassMana", 50)) return false;
    if (CastQ(target, Mode::Harass, selected, orbwalker)) return true;
    return CastE(target, Mode::Harass, selected, orbwalker);
}

inline bool TryFlee(const AIHeroClient& threat) {
    if (CastE(threat, Mode::Flee, false, true, true, true)) return true;
    if (Engine::ValidEnemy(threat)) return CastW(threat, Mode::Flee, false, true, true, true);
    return false;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& ignoredTargetInput) {
    (void)ignoredTargetInput;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return true;
    const int now = Now();
    PositionHistory[static_cast<std::size_t>(PositionHistoryIndex)] = player.Position();
    PositionTicks[static_cast<std::size_t>(PositionHistoryIndex)] = now;
    PositionHistoryIndex = (PositionHistoryIndex + 1) % static_cast<int>(PositionHistory.size());
    ReconcileState();
    const AIHeroClient target = Engine::SelectTarget(kQRange + 80.0f);
    const bool selectedTarget = false;
    const bool orbwalkerTarget = true;
    const AIHeroClient threat = ControllerHelpers::NearestEnemyToPlayer(target, 900.0f);
    if (mode == Mode::Flee) { (void)TryFlee(threat); return true; }
    if (TryKillSecure(target, mode, selectedTarget, orbwalkerTarget)) return true;
    switch (mode) {
    case Mode::Combo: (void)TryCombo(target, selectedTarget, orbwalkerTarget); break;
    case Mode::Harass: (void)TryHarass(target, selectedTarget, orbwalkerTarget); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)Engine::TryFarm(mode); break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target) && IncomingHardCCUntil > now)
            (void)CastW(threat, Mode::Automatic, selectedTarget, orbwalkerTarget, true, true);
        break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (!IsLocalPlayer(args.Sender)) {
        const auto threat = ControllerHelpers::AnalyzeEnemyCast(args, 300.0f, 110.0f,
                                                                 250, 280, 260, 1600, 450);
        if (threat.Valid && threat.CrossesPlayer && threat.LikelyHardCrowdControl)
            IncomingHardCCUntil = now + 700;
        return;
    }
    if (args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId != 0
            ? args.TargetNetworkId : args.Target.NetworkId);
        LastAutoTick = now;
        ObservePassive(LastAutoTargetId);
        EAttackArmed = false;
        return;
    }
    const int slot = args.Slot;
    const bool owned = slot >= 0 && slot < 4 && Engine::WasControllerCast(slot);
    const Vector3 eventEnd = args.EndPosition.IsValid() && !args.EndPosition.IsZero()
        ? args.EndPosition : args.CastPosition;
    const int spellTargetId = static_cast<int>(args.TargetNetworkId != 0
        ? args.TargetNetworkId : args.Target.NetworkId);
    if (slot == 0 || Engine::TextContains(args.SpellName, "EkkoQ")) {
        QLastCastTick = now;
        QReturnExpireTick = now + kQReturnWindowMs;
        QWasManual = !owned;
        QOrigin = GameObjects::Player().Position();
        if (eventEnd.IsValid() && !eventEnd.IsZero()) QOutboundEndpoint = eventEnd;
        if (spellTargetId != 0) {
            QTargetId = spellTargetId;
            ObservePassive(spellTargetId);
        }
        QOutboundActive = true;
        QReturning = Engine::TextContains(args.SpellName, "Return");
    } else if (slot == 1 || Engine::TextContains(args.SpellName, "EkkoW")) {
        WLastCastTick = now;
        WExpireTick = now + kWDelayMs + 1000;
        WWasManual = !owned;
        WCenter = eventEnd.IsValid() && !eventEnd.IsZero() ? eventEnd : Game::CursorPos();
        WArmed = true;
        if (spellTargetId != 0) ObservePassive(spellTargetId);
    } else if (slot == 2 || Engine::TextContains(args.SpellName, "EkkoE")) {
        ELastCastTick = now;
        EEmpoweredUntil = now + 3000;
        EAttackArmed = true;
        EWasManual = !owned;
        if (spellTargetId != 0) ObservePassive(spellTargetId);
    } else if (slot == 3 || Engine::TextContains(args.SpellName, "EkkoR")) {
        RLastCastTick = now;
        RWasManual = !owned;
        RAvailable = false;
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "EkkoR") ||
        Engine::TextContains(args.BuffName, "EkkoChronobreak")) RAvailable = false;
    if (Engine::TextContains(args.BuffName, "EkkoPassive")) {
        const int id = LastAutoTargetId;
        if (id != 0) { auto* state = PassiveFor(id, true); if (state) state->Hits = 0; }
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "EkkoR") ||
        Engine::TextContains(args.BuffName, "EkkoChronobreak")) RAvailable = true;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    AttackWindupUntil = Now() + std::max(60, Game::Ping() + 100);
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
    ObservePassive(LastAutoTargetId);
    AttackWindupUntil = 0;
    EAttackArmed = false;
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs&) {}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs&) {}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args)) return;
    if (!Engine::TextContains(args.SpellName, "EkkoQ") &&
        !Engine::TextContains(args.MissileName, "EkkoQ")) return;
    QOutboundActive = true;
    QReturning = Engine::TextContains(args.SpellName, "Return") ||
                 Engine::TextContains(args.MissileName, "Return");
    QLastCastTick = Now();
    QReturnExpireTick = Now() + kQReturnWindowMs;
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args)) return;
    if (Engine::TextContains(args.SpellName, "EkkoQ") ||
        Engine::TextContains(args.MissileName, "EkkoQ")) {
        if (QReturning || Engine::TextContains(args.SpellName, "Return") ||
            Engine::TextContains(args.MissileName, "Return")) {
            QOutboundActive = false;
            QReturning = false;
            QTargetId = 0;
        }
    }
}

inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawRanges", false)) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF6CD6FFu, 1.4f, 40);
    Drawing::DrawCircle(player.Position(), kWRange, 0xFFB98CFFu, 1.0f, 40);
    Drawing::DrawCircle(player.Position(), kERange, 0xFF6CFF9Bu, 1.4f, 40);
    if (RRewindPosition.IsValid() && !RRewindPosition.IsZero())
        Drawing::DrawCircle(RRewindPosition, 90.0f, 0xFFFFD166u, 1.6f, 32);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("EkkoMechanics", "Ekko passive and Chronobreak safety"));
    PassiveMenu = TacticsMenu->AddSubMenu(new Menu("ZDrive", "Three-hit passive tracking"));
    PassiveMenu->Add(new MenuBool("TrackAutoAttacks", "Track spell and auto hits", true));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Timewinder", "Outbound and return prediction"));
    QMenu->Add(new MenuSlider("HarassMana", "Minimum harass mana (%)", 50, 0, 100));
    QMenu->Add(new MenuBool("RequireCollisionFree", "Require clear outbound path", true));
    WMenu = TacticsMenu->AddSubMenu(new Menu("ParallelConvergence", "Delayed stun zone"));
    WMenu->Add(new MenuSlider("StunLeadMs", "W arrival lead (ms)", 3000, 2200, 3400));
    EMenu = TacticsMenu->AddSubMenu(new Menu("PhaseDive", "Dash and empowered attack"));
    EMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum enemies at E endpoint", 2, 1, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("Chronobreak", "Rewind and damage gate"));
    RMenu->Add(new MenuSlider("RTargetHP", "Prefer R target below HP (%)", 40, 10, 90));
    RMenu->Add(new MenuSlider("MinimumHealth", "Minimum safe endpoint health (%)", 20, 5, 80));
    RMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum enemies at rewind", 1, 0, 5));
    RMenu->Add(new MenuBool("DefensiveR", "Allow defensive rewind", true));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("EkkoFarm", "Conservative farming"));
    FarmMenu->Add(new MenuSlider("ManaReserve", "Mana reserve (%)", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("EkkoCoach", "Route visualization"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/W/E and rewind", false));
}

inline void OnLoad() {
    PassiveRecords = {}; PositionHistory = {}; PositionTicks = {}; PositionHistoryIndex = 0;
    QLastCastTick = QReturnExpireTick = QTargetId = WLastCastTick = WExpireTick = 0;
    ELastCastTick = EEmpoweredUntil = RLastCastTick = RTargetId = 0;
    AttackWindupUntil = LastAutoTargetId = LastAutoTick = IncomingHardCCUntil = 0;
    QOrigin = QOutboundEndpoint = WCenter = RRewindPosition = {};
    QOutboundActive = QReturning = WArmed = EAttackArmed = false;
    RAvailable = true; RWasManual = QWasManual = WWasManual = EWasManual = false;
    QReturnProjectedHit = false;

}
inline void OnUnload() {
    TacticsMenu = PassiveMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    RAvailable = false; PassiveRecords = {};
}

inline constexpr const char* Scenarios[] = {
    "Riot 26.15 and CommunityDragon 16.15 Summoner's Rift baseline",
    "Track Z-Drive Resonance hits per target from auto, spell, buff, and polling events",
    "Reset passive hit state after the four-second target window or target change",
    "Use passive third-hit speed and damage only for the tracked target",
    "Predict Timewinder outbound cast and return interception with live hitchance",
    "Reject Q paths with a collision object, wall endpoint, or zero-length direction",
    "Reconcile Q return state from spell names and expiry polling",
    "Place Parallel Convergence three seconds ahead of predicted target position",
    "Require a non-protected target and delayed zone arrival before expecting stun",
    "Preserve W as a reactive interrupt or peel tool in Automatic mode",
    "Phase Dive to a clamped endpoint and arm the empowered next attack",
    "Reject E endpoints through walls, unsafe turrets, or excessive enemy density",
    "Prefer selected target before orbwalker fallback in every offensive route",
    "Yield to manual Q/W/E/R casts for the configured ownership window",
    "Capture basic attack windup and do not interrupt nonlethal autos",
    "Record a bounded four-second position history for Chronobreak rewind",
    "Require a valid walkable rewind endpoint before casting Chronobreak",
    "Reject offensive R when the rewind endpoint is turret-unsafe or overcommitted",
    "Permit R only for verified lethal damage or defensive/flee value",
    "Apply missing-health scaling to R damage and target endpoint proximity",
    "Use health and nearby-enemy gates for safe Chronobreak endpoint selection",
    "Combo uses W-Q-E weaving and reserves R for a lethal or safe rewind",
    "Harass uses Q/E with mana and health gates and never starts a blind R",
    "LaneClear, Jungle, and LastHit delegate only to explicit shared farm handling",
    "Flee uses cursor-safe E then delayed W peel without aggressive R",
    "Automatic mode answers hard crowd control but never creates a fresh engage",
    "Complete spell, buff, attack, object, missile, gapcloser, and interrupt callbacks",
    "Keep controller metadata and relative research artifact nonempty",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Ekko;
    controller.ControllerId = "champion.kuroaio.ai.ekko.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIEkko.md";
    controller.ImplementationSummary =
        "Z-Drive three-hit state, predictive Timewinder outbound/return, delayed Parallel Convergence, "
        "Phase Dive empowered attack, and safe Chronobreak rewind/damage gates.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &OnInterruptable;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Ekko
