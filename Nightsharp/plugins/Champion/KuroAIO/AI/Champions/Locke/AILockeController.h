#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AILockeGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Locke {
using namespace Geometry;

using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellRank;
using ControllerHelpers::SpellEnabled;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline int QMarkTargetId = 0;
inline int QMarkStacks = 0;
inline int QMarkExpireTick = 0;
inline int WCastTick = 0;
inline int WExpireTick = 0;
inline bool WActive = false;
inline int ECastTick = 0;
inline Vector3 EAnchor{};
inline int ETargetId = 0;
inline int RMarkTargetId = 0;
inline int RMarkExpireTick = 0;
inline int SealedChampions = 0;
inline int RCastTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd{};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
inline bool Throttle(int slot, int delay = 70, bool reactive = false) {
    return ControllerHelpers::CastThrottleReady(slot, delay, reactive ? 0 : -1);
}
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Protected;
inline bool HasQMark(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) && (static_cast<int>(target.NetworkId()) == QMarkTargetId ||
        target.HasBuff("LockeQMark") || target.HasBuff("LockeSoulNails"));
}
inline bool HasRMark(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) && (static_cast<int>(target.NetworkId()) == RMarkTargetId ||
        target.HasBuff("LockeRMark") || target.HasBuff("LockePurgatoryMark"));
}
inline Vector3 Aim(const AIHeroClient& target, float seconds, int slot) {
    if (!Engine::ValidEnemy(target)) return {};
    Vector3 result = PredictPosition(target, seconds);
    if (slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot]) {
        const auto prediction = Engine::RuntimeSpells[slot]->GetPrediction(target);
        if (static_cast<int>(prediction.Hitchance) >= static_cast<int>(SDK::HitChance::High) &&
            prediction.GetCastPosition().IsValid() && !prediction.GetCastPosition().IsZero())
            result = prediction.GetCastPosition();
    }
    return result;
}
inline bool QBlocked(const AIHeroClient& target, const Vector3& aim) {
    if (!aim.IsValid() || aim.IsZero()) return true;
    std::vector<Blocker> blockers;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable()) continue;
        blockers.push_back({minion.Position(), std::max(20.0f, minion.BoundingRadius()),
                            static_cast<int>(minion.NetworkId())});
    }
    const auto first = FirstQCollision(GameObjects::Player().Position(), aim,
                                       target.BoundingRadius(), blockers);
    return first.Blocked && first.NetworkId != static_cast<int>(target.NetworkId());
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(0, mode) || !Throttle(0, reactive) ||
        PreserveAttack(reactive)) return false;
    const Vector3 aim = Aim(target, 0.25f, 0);
    if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) > kQRange + target.BoundingRadius() ||
        QBlocked(target, aim) || ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQHalfWidth)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    QMarkTargetId = static_cast<int>(target.NetworkId());
    QMarkStacks = std::min(3, QMarkStacks + 1);
    QMarkExpireTick = Now() + 3500;
    return true;
}
inline bool CastW(Mode mode, bool reactive = false, bool recast = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1, reactive)) return false;
    if (recast) {
        if (!WActive || !Engine::ControllerCastSelf(1)) return false;
        WActive = false; WExpireTick = 0; return true;
    }
    const AIHeroClient target = Engine::SelectTarget(kWRadius + 120.0f);
    const WContext context{true, WActive, GapcloserTargetId != 0,
        player.HealthPercent() > Slider(WMenu, "MinimumHealth", 32),
        player.ManaPercent() > Slider(WMenu, "MinimumMana", 20),
        Engine::ValidEnemy(target, kWRadius + 120.0f)};
    if (!ShouldIgnite(context) || PreserveAttack(reactive) || !Engine::ControllerCastSelf(1)) return false;
    WActive = true; WCastTick = Now(); WExpireTick = WCastTick + static_cast<int>(kWDuration * 1000.0f);
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool defensive = false, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || !Throttle(2, reactive) || PlayerMobilityLocked() ||
        PreserveAttack(reactive)) return false;
    Vector3 requested = defensive ? Engine::Extend(player.Position(), Game::CursorPos(), kERange)
                                  : Aim(target, 0.15f, 2);
    if (requested.IsZero()) requested = Game::CursorPos();
    const Vector3 endpoint = ClampEEndpoint(player.Position(), requested);
    if (!SafeBlink(endpoint, SDK::NavMesh::IsWall(endpoint),
                   Engine::UnderEnemyTurret(endpoint) && !Engine::UnderEnemyTurret(player.Position()),
                   Engine::CountEnemiesAt(endpoint, 250.0f), Slider(EMenu, "MaxEnemiesAtBlink", 2))) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    ECastTick = Now(); EAnchor = endpoint;
    ETargetId = Engine::ValidEnemy(target) ? static_cast<int>(target.NetworkId()) : 0;
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(3, mode) || !Throttle(3, reactive) || PreserveAttack(reactive)) return false;
    const Vector3 aim = Aim(target, kRDelay, 3);
    if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) > kRRange + target.BoundingRadius() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kRRadius)) return false;
    const bool execute = ExecuteEligible(target.HealthPercent() / 100.0f, SealedChampions);
    const RContext context{true, target.IsTargetable(), false,
        target.Health() + target.AllShield() <= RBaseDamage(SpellRank(3), player.AP()), execute,
        reactive, Engine::UnderEnemyTurret(aim), Engine::CountEnemiesAt(aim, kRRadius)};
    if (!ShouldCastPurgatory(context, Slider(RMenu, "MinimumTargets", 1))) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    RCastTick = Now(); RMarkTargetId = static_cast<int>(target.NetworkId()); RMarkExpireTick = Now() + 5000;
    return true;
}
inline bool TryFarm(bool lastHitOnly, bool jungle) {
    if (!Ready(0, jungle ? Mode::Jungle : (lastHitOnly ? Mode::LastHit : Mode::LaneClear)) ||
        !Throttle(0) || PreserveAttack(false)) return false;
    Vector3 best{}; float bestHealth = FLT_MAX;
    if (jungle) {
        for (const auto& monster : GameObjects::Jungle()) {
            if (!monster.IsValid() || monster.IsDead() || !monster.IsTargetable()) continue;
            if (monster.Position().Distance2D(GameObjects::Player().Position()) > kQRange) continue;
            if (monster.Health() < bestHealth) { bestHealth = monster.Health(); best = monster.Position(); }
        }
    } else {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable()) continue;
            if (minion.Position().Distance2D(GameObjects::Player().Position()) > kQRange) continue;
            if (!lastHitOnly || minion.Health() < 120.0f) {
                if (minion.Health() < bestHealth) { bestHealth = minion.Health(); best = minion.Position(); }
            }
        }
    }
    return !best.IsZero() && Engine::ControllerCastPosition(0, best);
}
inline bool TryFlee() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const Vector3 cursor = Game::CursorPos();
    if (Ready(2, Mode::Flee) && Throttle(2, 80, true)) {
        const Vector3 endpoint = ClampEEndpoint(player.Position(), cursor);
        const ThreatRoute route{endpoint, !endpoint.IsZero(), endpoint.Distance2D(cursor) < player.Position().Distance2D(cursor),
            Engine::UnderEnemyTurret(endpoint), Engine::CountEnemiesAt(endpoint, 250.0f)};
        if (SafeFleeRoute(route, Slider(EMenu, "MaxEnemiesAtBlink", 2)) && CastE({}, Mode::Flee, true, true)) return true;
    }
    return false;
}
inline bool TryAutomatic() {
    const auto player = GameObjects::Player();
    if (GapcloserTargetId != 0 && GapcloserExpireTick >= Now()) {
        const auto threat = Engine::EnemyByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(threat, 700.0f)) return CastE(threat, Mode::Automatic, true, true);
    }
    if (InterruptTargetId != 0 && InterruptExpireTick >= Now()) {
        const auto threat = Engine::EnemyByNetworkId(InterruptTargetId);
        if (Engine::ValidEnemy(threat, kRRange)) return CastR(threat, Mode::Automatic, true);
    }
    if (player.IsValid() && player.HealthPercent() < Slider(WMenu, "EmergencyHealth", 24)) return CastW(Mode::Automatic, true);
    return false;
}
inline void ReconcileObservedState() {
    const auto player = GameObjects::Player();
    const int now = Now();
    if (player.IsValid() && player.HasBuff("LockeW")) {
        WActive = true;
        if (WExpireTick <= now) WExpireTick = now + 4000;
    } else if (WActive && WExpireTick > 0 && now >= WExpireTick) {
        WActive = false;
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        if (enemy.HasBuff("LockeQMark") || enemy.HasBuff("LockeSoulNails")) {
            QMarkTargetId = static_cast<int>(enemy.NetworkId());
            QMarkExpireTick = now + 3500;
        }
        if (enemy.HasBuff("LockeRMark") || enemy.HasBuff("LockePurgatoryMark")) {
            RMarkTargetId = static_cast<int>(enemy.NetworkId());
            RMarkExpireTick = now + 5000;
        }
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    const int now = Now(); LastMode = mode;
    ReconcileObservedState();
    if (now > QMarkExpireTick) { QMarkTargetId = 0; QMarkStacks = 0; }
    if (now > RMarkExpireTick) RMarkTargetId = 0;
    if (WActive && now >= WExpireTick) WActive = false;
    if (mode == Mode::Flee) return TryFlee() || CastW(mode, true);
    if (mode == Mode::Automatic) return TryAutomatic();
    const AIHeroClient target = Engine::SelectTarget(mode == Mode::Harass ? 850.0f : kRRange);
    if (!Engine::ValidEnemy(target)) {
        if (mode == Mode::LaneClear) return TryFarm(false, false);
        if (mode == Mode::LastHit) return TryFarm(true, false);
        if (mode == Mode::Jungle) return TryFarm(false, true);
        return false;
    }
    if (mode == Mode::Combo) {
        if (CastR(target, mode) || CastQ(target, mode) || CastE(target, mode) || CastW(mode)) return true;
    } else if (mode == Mode::Harass) {
        if (CastQ(target, mode) || CastW(mode)) return true;
    } else if (mode == Mode::Jungle || mode == Mode::LaneClear || mode == Mode::LastHit) {
        return TryFarm(mode == Mode::LastHit, mode == Mode::Jungle);
    }
    return false;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (!IsLocalPlayer(args.Sender)) {
        const auto threat = ControllerHelpers::AnalyzeEnemyCast(args, 250.0f, 105.0f, 260, 250, 240, 1400, 380);
        if (threat.Valid && threat.Committed) { GapcloserTargetId = static_cast<int>(threat.Enemy.NetworkId()); GapcloserExpireTick = threat.CommitmentUntilTick; }
        return;
    }
    if (args.Slot == 0) { QMarkExpireTick = now + 3500; }
    else if (args.Slot == 1) { WCastTick = now; WActive = !WActive; WExpireTick = WActive ? now + 4000 : 0; }
    else if (args.Slot == 2) { ECastTick = now; EAnchor = args.EndPosition.IsValid() ? args.EndPosition : args.CastPosition; }
    else if (args.Slot == 3) { RCastTick = now; RMarkExpireTick = now + 5000; }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId); const int now = Now();
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "LockeW")) {
        WActive = true; WCastTick = now; WExpireTick = args.EndTime > Game::Time() ? now + ControllerHelpers::RemainingMilliseconds(args.EndTime, 4000, 250, 6000) : now + 4000;
    } else if (Engine::ValidEnemy(Engine::EnemyByNetworkId(id)) && Engine::TextContains(args.BuffName, "LockeQ")) {
        QMarkTargetId = id; QMarkStacks = std::min(3, QMarkStacks + 1); QMarkExpireTick = now + 3500;
    } else if (Engine::ValidEnemy(Engine::EnemyByNetworkId(id)) && Engine::TextContains(args.BuffName, "LockeR")) {
        RMarkTargetId = id; RMarkExpireTick = now + 5000;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "LockeW")) { WActive = false; WExpireTick = 0; }
    if (id == QMarkTargetId && Engine::TextContains(args.BuffName, "LockeQ")) { QMarkTargetId = 0; QMarkStacks = 0; }
    if (id == RMarkTargetId && Engine::TextContains(args.BuffName, "LockeR")) RMarkTargetId = 0;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    const int now = Now();
    if (QMarkTargetId != 0 && now < QMarkExpireTick && QMarkExpireTick - now < 300 &&
        Engine::RuntimeSpells[0] && Engine::RuntimeSpells[0]->IsReady()) args.Process = false;
}
inline void OnDraw() {
    if (!CoachMenu || !GameObjects::Player().IsValid()) return;
    if (Bool(CoachMenu, "DrawRanges", false)) {
        Drawing::DrawCircle(GameObjects::Player().Position(), kQRange, 0x557788FFu, 1.0f, 48);
        Drawing::DrawCircle(GameObjects::Player().Position(), kERange, 0x55FFAA77u, 1.0f, 40);
    }
    if (Bool(CoachMenu, "DrawState", false)) {
        Vec2 screen{}; if (Drawing::WorldToScreen(GameObjects::Player().Position(), screen)) {
            char state[180]{}; _snprintf_s(state, sizeof(state), _TRUNCATE,
                "Locke | W %s | Q marks %d | seals %d", WActive ? "active" : "ready", QMarkStacks, SealedChampions);
            Drawing::DrawText(screen.x - 80.0f, screen.y - 100.0f, 0xFFFFCC88u, state);
        }
    }
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("LockeOneTrick", "Locke Ashen Exorcist mechanics"));
    QMenu = TacticsMenu->AddSubMenu(new Menu("RitualNails", "Q marks and collision"));
    QMenu->Add(new MenuBool("Combo", "Use Q in combo", true)); QMenu->Add(new MenuBool("Harass", "Use Q in harass", true));
    WMenu = TacticsMenu->AddSubMenu(new Menu("SoulIgnition", "W self-drain and recast"));
    WMenu->Add(new MenuSlider("MinimumHealth", "Minimum health (%)", 32, 10, 80)); WMenu->Add(new MenuSlider("EmergencyHealth", "Emergency W health (%)", 24, 8, 60)); WMenu->Add(new MenuSlider("MinimumMana", "Minimum mana (%)", 20, 0, 80));
    EMenu = TacticsMenu->AddSubMenu(new Menu("AshenPursuit", "Blink endpoint safety")); EMenu->Add(new MenuSlider("MaxEnemiesAtBlink", "Max enemies at blink", 2, 0, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("Purgatory", "Execute and seal policy")); RMenu->Add(new MenuSlider("MinimumTargets", "Minimum R targets", 1, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("LockeFarm", "Lane, jungle, and last hit")); FarmMenu->Add(new MenuBool("UseQ", "Use Q to farm", true));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("LockeCoach", "Visual state coaching")); CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/E ranges", false)); CoachMenu->Add(new MenuBool("DrawState", "Draw state", false));
}
inline void OnLoad() {
    QMarkTargetId = QMarkStacks = QMarkExpireTick = WCastTick = WExpireTick = 0; WActive = false;
    ECastTick = ETargetId = RMarkTargetId = RMarkExpireTick = SealedChampions = RCastTick = 0;
    LastAutoTargetId = LastAutoTick = GapcloserTargetId = GapcloserExpireTick = InterruptTargetId = InterruptExpireTick = 0;
    EAnchor = GapcloserEnd = {}; LastMode = Mode::None;
}
inline void OnUnload() { TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr; WActive = false; }
inline constexpr const char* Scenarios[] = {
    "Pin Locke to Riot 26.15 and CommunityDragon 16.15 champion id 805",
    "Use Q's 950 range and first-body collision before releasing Soul Nails",
    "Reconcile Q marks from cast events, buff events, and expiry polling",
    "Keep reachable targets stable while validating the nail sequence",
    "Preserve an AA windup unless a nearly expired Q mark has a legal follow-up",
    "Ignite W only with sufficient health and mana for its current-health drain",
    "Recast W early when health becomes unsafe or the nearby target leaves range",
    "Use E blink only on a valid navmesh endpoint with bounded enemy count",
    "Treat E's next attack dash as a pending target action, not a free teleport",
    "Use E defensively toward cursor only when the route increases separation",
    "Aim Purgatory from prediction and reject a wall-blocked projectile",
    "Reserve Purgatory for an execute window, lethal damage, or configured AoE",
    "Track R marks and sealed-champion stacks through buff lifecycle callbacks",
    "Use gapcloser and interrupt callbacks for automatic defensive responses",
    "Support Combo, Harass, LaneClear, Jungle, LastHit, Flee, and Automatic modes",
    "Reconcile local casts without clearing autonomous target state",
    "Never turret dive through an unverified lethal calculation",
    "Keep tooltip placeholder values conservative until numeric payloads are published",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Locke;
    controller.ControllerId = "champion.kuroaio.ai.locke.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AILocke.md";
    controller.ImplementationSummary = "Soul-Nail collision and mark tracker, self-drain W safety/recast policy, E blink-plus-attack routing, and Purgatory execution planner with full mode/event reconciliation.";
    controller.Scenarios = Scenarios; controller.ScenarioCount = std::size(Scenarios); controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad; controller.OnUnload = &OnUnload; controller.BuildMenu = &BuildMenu; controller.OnUpdate = &OnUpdate; controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnBuffAdd = &OnBuffAdd; controller.OnBuffRemove = &OnBuffRemove; controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEnd, &GapcloserExpireTick, 425, 700>;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick>;
    return controller;
}();
}
