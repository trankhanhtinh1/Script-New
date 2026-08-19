#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIZiggsGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Ziggs {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline Vector3 LastQAim{};
inline Vector3 LastWAim{};
inline Vector3 LastEAim{};
inline Vector3 LastRAim{};
inline int LastCastTick[4]{};
inline int EExpireTick = 0;
inline int WExpireTick = 0;
inline int PlayerOverrideUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
inline bool Throttle(int slot, int delay = 90) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
using ControllerHelpers::Protected;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::AP;
inline bool Lethal(const AIHeroClient& target, float rawDamage) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target) &&
           player.CalculateMagicDamage(target, rawDamage) >=
               target.Health() + target.AllShield();
}
inline Vector3 Predicted(const AIHeroClient& target, int slot, float delay) {
    if (!Engine::ValidEnemy(target)) return {};
    Vector3 aim = PredictPosition(target, delay);
    if (slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot]) {
        const auto prediction = Engine::RuntimeSpells[slot]->GetPrediction(target);
        if (prediction.Hitchance >= SDK::HitChance::High &&
            prediction.GetCastPosition().IsValid() &&
            !prediction.GetCastPosition().IsZero()) {
            aim = prediction.GetCastPosition();
        }
    }
    return aim;
}
inline bool SafeEndpoint(const Vector3& endpoint, bool defensive, bool lethal) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || endpoint.IsZero() || !endpoint.IsValid() ||
        SDK::NavMesh::IsWall(endpoint)) return false;
    if (!defensive && !lethal && Engine::UnderEnemyTurret(endpoint) &&
        !Engine::UnderEnemyTurret(player.Position())) return false;
    return Engine::CountEnemiesAt(endpoint, kWSatchelRadius) <=
        Slider(WMenu, "MaxEndpointEnemies", 1) || defensive || lethal;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0, mode) || !Throttle(0) || Protected(target) ||
        PreserveAttack(reactive)) return false;
    const Vector3 aim = Predicted(target, 0, kQDelay);
    if (aim.IsZero() || player.Position().Distance2D(aim) > kQRange + target.BoundingRadius() ||
        !QHits(player.Position(), aim, PredictPosition(target, kQDelay), target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastQAim = aim;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool defensive = false, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1, reactive ? 35 : 100) ||
        PreserveAttack(reactive)) return false;
    Vector3 aim = Engine::ValidEnemy(target) ? Predicted(target, 1, kWDelay) : Game::CursorPos();
    aim = ClampPosition(player.Position(), aim, kWRange);
    if (aim.IsZero()) return false;
    const SatchelSafetyContext context{
        true, SafeEndpoint(aim, defensive, lethal),
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 0.0f),
        SDK::NavMesh::IsWall(aim), Engine::UnderEnemyTurret(aim),
        Engine::CountEnemiesAt(aim, kWSatchelRadius),
        Slider(WMenu, "MaxEndpointEnemies", 1), defensive, lethal};
    if (!ShouldSatchel(context) || !Engine::ControllerCastPosition(1, aim)) return false;
    LastWAim = aim;
    LastCastTick[1] = Now();
    WExpireTick = Now() + 4000;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool defensive = false, bool objective = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || !Throttle(2, reactive ? 35 : 100) ||
        PreserveAttack(reactive)) return false;
    const Vector3 aim = Engine::ValidEnemy(target) ? Predicted(target, 2, kEDelay) : Game::CursorPos();
    if (aim.IsZero() || player.Position().Distance2D(aim) > kERange + 30.0f) return false;
    const MinefieldContext context{
        true, !SDK::NavMesh::IsWall(aim),
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 0.0f),
        Engine::UnderEnemyTurret(aim), defensive, objective,
        Engine::CountEnemiesAt(aim, kEMinefieldRadius),
        Slider(EMenu, "MinimumTargets", objective ? 1 : 2)};
    if (!ShouldMinefield(context) || !Engine::ControllerCastPosition(2, aim)) return false;
    LastEAim = aim;
    LastCastTick[2] = Now();
    EExpireTick = Now() + static_cast<int>(kEDurationSeconds * 1000.0f);
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool manual = false, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(3, mode) ||
        !Throttle(3, reactive ? 50 : 140) || PreserveAttack(reactive)) return false;
    const Vector3 aim = Predicted(target, 3, kRDelay);
    if (aim.IsZero() || player.Position().Distance2D(aim) > kRRange + kRRadius ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 0.0f)) return false;
    int predictedTargets = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && ROuterHit(aim, PredictPosition(enemy, kRDelay), enemy.BoundingRadius()))
            ++predictedTargets;
    }
    const bool lethal = Lethal(target, RRawDamage(SpellRank(3), AP()));
    const UltimateContext context{
        true, true, true,
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 0.0f),
        lethal, defensive, manual, Orbwalker::IsWindingUp(), predictedTargets,
        Slider(RMenu, "MinimumTargets", 2)};
    if (!ShouldCastMega(context) || !Engine::ControllerCastPosition(3, aim)) return false;
    LastRAim = aim;
    LastCastTick[3] = Now();
    return true;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, QRawDamage(SpellRank(0), AP())) && CastQ(target, mode)) return true;
    if (Lethal(target, ERawDamage(SpellRank(2), AP())) && CastE(target, mode)) return true;
    return Lethal(target, RRawDamage(SpellRank(3), AP())) && CastR(target, mode);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo, false, false, false)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(WMenu, "HarassMana", 48)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastE(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (CastW(target, Mode::Flee, true, true, false)) return;
    if (CastE(target, Mode::Flee, true, true, false)) return;
    if (Engine::ValidEnemy(target)) (void)CastQ(target, Mode::Flee, true);
}
inline void ReconcileState() {
    const int now = Now();
    if (EExpireTick > 0 && now > EExpireTick) { EExpireTick = 0; LastEAim = {}; }
    if (WExpireTick > 0 && now > WExpireTick) { WExpireTick = 0; LastWAim = {}; }
    const auto player = GameObjects::Player();
    if (player.IsValid() && player.HasBuff("ZiggsW")) WExpireTick = std::max(WExpireTick, now + 400);
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, kRRange);
    if (PlayerOverrideUntil > Now()) return true;
    if (IncomingThreatUntil > Now() && Engine::ValidEnemy(target) &&
        CastW(target, mode, true, true, false)) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1100.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (GameObjects::Player().ManaPercent() >= Slider(FarmMenu, "Mana", 35))
            (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
        if (AutomaticAllowed({IncomingThreatUntil > Now(), IncomingHardCCUntil > Now(),
            Engine::ValidEnemy(target) && Lethal(target, RRawDamage(SpellRank(3), AP())),
            PlayerOverrideUntil > Now()}))
            (void)CastR(target, mode, true, false, IncomingThreatUntil > Now());
        break;
    default: break;
    }
    return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot < 0 || slot > 3) return;
        if (!Engine::WasControllerCast(slot))
            PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
        LastCastTick[slot] = now;
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl)
        IncomingHardCCUntil = std::max(IncomingHardCCUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "ZiggsW")) WExpireTick = Now() + 500;
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "ZiggsW")) WExpireTick = 0;
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFEEAA44u, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFCC55DDu, 1.0f, 40);
    if (EExpireTick > Now() && !LastEAim.IsZero())
        Drawing::DrawCircle(LastEAim, kEMinefieldRadius, 0xFFFF8844u, 1.5f, 36);
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs&) {
    IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 450);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs&) {
    IncomingHardCCUntil = std::max(IncomingHardCCUntil, Now() + 650);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("ZiggsTactics", "Ziggs artillery tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Bouncing Bomb"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Satchel Charge"));
    WMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum endpoint enemies", 1, 0, 4));
    WMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 48, 10, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Minefield"));
    EMenu->Add(new MenuSlider("MinimumTargets", "Minimum predicted targets", 2, 1, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Mega Inferno Bomb"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum nonlethal targets", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("ZiggsFarm", "Waveclear and objectives"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("ZiggsCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw artillery and minefield ranges", false));
}
inline void OnLoad() {
    LastQAim = LastWAim = LastEAim = LastRAim = {};
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    EExpireTick = WExpireTick = PlayerOverrideUntil = IncomingThreatUntil = IncomingHardCCUntil = 0;
    LastAutoTargetId = LastAutoTick = 0;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    LastQAim = LastWAim = LastEAim = LastRAim = {};
}
inline constexpr const char* Scenarios[] = {
    "Pin all mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Use Q prediction, initial range, bounce explosion radius and projectile wall checks",
    "Reject Q casts when collision or low-confidence prediction makes contact uncertain",
    "Use W Satchel only at a valid, non-wall endpoint with bounded enemy exposure",
    "Allow defensive Satchel displacement while preserving cursor and movement ownership",
    "Reject Satchel endpoint turrets unless defensive or lethal policy allows it",
    "Use E Minefield to layer slow zones, deny routes and control objective approaches",
    "Reject E through walls and require predicted target count outside turret danger",
    "Use R center and outer radius with prediction, collision and wall safety checks",
    "Reserve nonlethal R for configured multi-target value; permit lethal execute",
    "Preserve selected target before orbwalker and selector fallback",
    "Preserve AA windup unless casts are reactive or lethal",
    "Reconcile Satchel and Minefield state from spell, buff and polling observations",
    "Yield after observed manual Q W E or R ownership",
    "Combo layers E then Q, uses safe W displacement and reserves R for value",
    "Harass uses Q and E with a mana reserve and no unsolicited artillery engage",
    "LaneClear, Jungle and LastHit use shared farm policy with Ziggs waveclear intents",
    "Jungle mode may use E and R only through observed objective-safe target policy",
    "Flee uses defensive Satchel first, Minefield peel and Q only as a safe follow-up",
    "Automatic mode permits defense, interrupt or kill secure only",
    "Never automate items, summoner spells or movement ownership",
    "Draw artillery ranges and active minefield state without changing decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Ziggs;
    controller.ControllerId = "champion.kuroaio.ai.ziggs.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIZiggs.md";
    controller.ImplementationSummary =
        "Prediction- and zone-aware artillery loop with safe Satchel displacement, Minefield route denial, execute-gated Mega Inferno Bomb and reconciled manual ownership.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;

    controller.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &OnInterruptable;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Ziggs
