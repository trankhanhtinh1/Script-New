#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIPoppyGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Poppy {

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

inline BucklerState Buckler{};
inline UltimateStage RStage = UltimateStage::Idle;
inline Vector3 LastQAim{};
inline Vector3 LastEAim{};
inline Vector3 LastRAim{};
inline int LastCastTick[4]{};
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int GapcloserUntil = 0;
inline int GapcloserTargetId = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
inline bool Throttle(int slot, int delay = 90) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
using ControllerHelpers::Protected;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Lethal;
using ControllerHelpers::BonusAttackDamage;
inline float QDamage(const AIHeroClient& target, bool zone = false) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, QRawDamage(SpellRank(0),
            BonusAttackDamage(), target.MaxHealth(), zone)) : 0.0f;
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, ERawDamage(SpellRank(2), BonusAttackDamage())) : 0.0f;
}
inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, RRawDamage(SpellRank(3), BonusAttackDamage())) : 0.0f;
}
inline bool SafeEndpoint(const Vector3& point, bool defensive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || point.IsZero() || !point.IsValid() || SDK::NavMesh::IsWall(point)) return false;
    if (!defensive && Engine::UnderEnemyTurret(point) && !Engine::UnderEnemyTurret(player.Position())) return false;
    return Engine::CountEnemiesAt(point, 240.0f) <= Slider(EMenu, "MaxEndpointEnemies", 2);
}
inline Vector3 AimFor(const AIHeroClient& target, float delay) {
    if (!Engine::ValidEnemy(target)) return {};
    Vector3 aim = PredictPosition(target, delay);
    if (Engine::RuntimeSpells[0]) {
        const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
        if (prediction.Hitchance >= SDK::HitChance::High && prediction.GetCastPosition().IsValid())
            aim = prediction.GetCastPosition();
    }
    return aim;
}
inline AIHeroClient AutonomousTarget(float range) {
    const auto orbwalker = ControllerHelpers::OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orbwalker, range)) return orbwalker;
    return Engine::SelectTarget(range);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0, mode) || !Throttle(0) || Protected(target) || PreserveAttack(reactive)) return false;
    const Vector3 aim = AimFor(target, kQDelay);
    if (!HammerShockHits(player.Position(), aim, target.Position(), target.BoundingRadius()) ||
        SDK::NavMesh::IsWallBetween(player.Position(), aim, kQWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastQAim = aim;
    LastCastTick[0] = Now();
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1, 150) || PreserveAttack(reactive)) return false;
    const bool threateningDash = Engine::ValidEnemy(target, kWRadius) && target.IsDashing();
    const bool urgent = reactive || threateningDash || IncomingHardCCUntil > Now() ||
        player.HealthPercent() <= Slider(WMenu, "DefensiveHealth", 42);
    if (!urgent && mode == Mode::Harass) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    return true;
}
inline bool WallBehindTarget(const AIHeroClient& target, const Vector3& origin, Vector3& wallPoint) {
    if (!Engine::ValidEnemy(target)) return false;
    const Vector3 direction = Direction2D(origin, target.Position());
    if (direction.IsZero()) return false;
    wallPoint = target.Position() + direction * (target.BoundingRadius() + 70.0f);
    return SDK::NavMesh::IsWallBetween(target.Position(), wallPoint, 0.0f) ||
        SDK::NavMesh::IsWall(wallPoint);
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || !Throttle(2) || Protected(target) || PreserveAttack(reactive)) return false;
    const Vector3 endpoint = ClampHeroicChargeEndpoint(player.Position(), PredictPosition(target, 0.15f));
    Vector3 wallPoint{};
    if (!WallImpactObserved(player.Position(), endpoint, WallBehindTarget(target, player.Position(), wallPoint), target.BoundingRadius()) ||
        !WallStunCanHit(player.Position(), target.Position(), wallPoint, target.BoundingRadius())) return false;
    const WallDashContext context{true, true, SafeEndpoint(endpoint, reactive), true, true,
        Engine::UnderEnemyTurret(endpoint) && !Engine::UnderEnemyTurret(player.Position()), reactive,
        lethal || Lethal(target, EDamage(target))};
    if (!ShouldHeroicCharge(context)) return false;
    if (!Engine::ControllerCastUnit(2, target)) return false;
    LastEAim = endpoint;
    LastCastTick[2] = Now();
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kRRange) || Protected(target) ||
        !Ready(3, mode) || !Throttle(3, 140) || PreserveAttack(reactive)) return false;
    const Vector3 aim = AimFor(target, 0.35f);
    if (aim.IsZero() || player.Position().Distance2D(aim) > kRRange + target.BoundingRadius() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kRWidth * 0.5f)) return false;
    int hitCount = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && SegmentHits(player.Position(), aim, PredictPosition(enemy, 0.35f),
            kRWidth * 0.5f, enemy.BoundingRadius())) ++hitCount;
    }
    const bool low = player.HealthPercent() <= Slider(RMenu, "DefensiveHealth", 42);
    const bool defensive = reactive || low || mode == Mode::Flee;
    const Vector3 away = Direction2D(player.Position(), aim);
    const UltimateContext context{true, true, true, false, RStage == UltimateStage::Charging,
        !away.IsZero(), defensive, Lethal(target, RDamage(target)), low,
        hitCount, Slider(RMenu, "MinimumTargets", 2)};
    if (!ShouldReleaseUltimate(context)) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    LastRAim = aim;
    LastCastTick[3] = Now();
    RStage = RStage == UltimateStage::Charging ? UltimateStage::Idle : UltimateStage::Charging;
    return true;
}
inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, EDamage(target)) && CastE(target, mode, false, true)) return true;
    if (Lethal(target, QDamage(target, true)) && CastQ(target, mode)) return true;
    return Lethal(target, RDamage(target)) && CastR(target, mode);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastE(target, Mode::Combo, false, Lethal(target, EDamage(target)))) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(QMenu, "HarassMana", 50)) return;
    if (CastQ(target, Mode::Harass)) return;
    if (Engine::ValidEnemy(target) && target.IsDashing()) (void)CastW(target, Mode::Harass, true);
}
inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) && CastW(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastR(target, Mode::Flee, true);
}
inline void ReconcileState() {
    const auto player = GameObjects::Player();
    const int now = Now();
    if (!player.IsValid()) return;
    Buckler.Ready = player.HasBuff("PoppyPassive") || player.HasBuff("PoppyPassiveReady");
    if (player.HasBuff("PoppyRCharge") || player.HasBuff("PoppyRCharging")) RStage = UltimateStage::Charging;
    if (RStage != UltimateStage::Idle && !player.HasBuff("PoppyRCharge") &&
        !player.HasBuff("PoppyRCharging") && now - LastCastTick[3] > 2500) RStage = UltimateStage::Idle;
    if (GapcloserUntil < now) GapcloserTargetId = 0;
}
inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    LastMode = mode;
    ReconcileState();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const AIHeroClient target = AutonomousTarget(
        mode == Mode::Flee ? 1000.0f : kRRange);
    if (Engine::ValidEnemy(target) &&
        ShouldInterceptDash({Ready(1, mode), target.IsDashing(),
            target.Position().Distance2D(player.Position()) <= kWRadius,
            target.IsDashing() || IncomingThreatUntil > Now(),
            player.HealthPercent() <= Slider(WMenu, "DefensiveHealth", 42)}) &&
        CastW(target, mode, true)) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1000.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (player.ManaPercent() >= Slider(FarmMenu, "Mana", 35)) (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic: {
        const bool defense = IncomingThreatUntil > Now() || IncomingHardCCUntil > Now() ||
            player.HealthPercent() <= Slider(WMenu, "DefensiveHealth", 42);
        const bool antiGap = GapcloserUntil > Now();
        const bool kill = Engine::ValidEnemy(target) && Lethal(target, QDamage(target, true));
        if (AutomaticAllowed({defense, antiGap, IncomingHardCCUntil > Now(), kill,
            false})) {
            if ((antiGap || defense) && CastW(target, mode, true)) return true;
            if (Engine::ValidEnemy(target) && CastE(target, mode, true, kill)) return true;
            if (Engine::ValidEnemy(target)) (void)CastR(target, mode, true);
        }
        break;
    }
    default: break;
    }
    return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            LastCastTick[slot] = now;
            if (slot == 3) RStage =
                RStage == UltimateStage::Charging ? UltimateStage::RecastReady : UltimateStage::Charging;
        }
        if (args.IsAutoAttack) {
            Buckler.Ready = false;
            Buckler.ShieldOnGround = true;
            Buckler.ShieldPosition = args.EndPosition.IsValid() && !args.EndPosition.IsZero()
                ? args.EndPosition : args.Target.Position;
            Buckler.ThrowTick = now;
            Buckler.PickupExpireTick =
                now + static_cast<int>(kPassiveShieldDuration * 1000.0f);
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(IncomingHardCCUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.Enemy.IsDashing()) {
        GapcloserUntil = std::max(GapcloserUntil, now + 650);
        GapcloserTargetId = static_cast<int>(analysis.Enemy.NetworkId());
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "PoppyPassive")) Buckler.Ready = false;
    if (Engine::TextContains(args.BuffName, "PoppyRCharge")) RStage = UltimateStage::Charging;
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "PoppyRCharge")) RStage = UltimateStage::Idle;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) Buckler.Ready = Buckler.Ready ||
        GameObjects::Player().HasBuff("PoppyPassive");
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
    if (Buckler.Ready) Buckler.Ready = false;
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFCC8844u, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kWRadius, 0xFF66AAFFu, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kERange, 0xFFFFAA55u, 1.5f, 40);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("PoppyOneTrick", "Poppy vanguard tactics"));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Hammer Shock"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 50, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Steadfast Presence"));
    WMenu->Add(new MenuSlider("DefensiveHealth", "Defensive health percent", 42, 10, 80));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Heroic Charge"));
    EMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum endpoint enemies", 2, 1, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Keeper's Verdict"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum nonlethal targets", 2, 1, 5));
    RMenu->Add(new MenuSlider("DefensiveHealth", "Defensive health percent", 42, 10, 80));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("PoppyFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("PoppyCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/W/E ranges", false));
}
inline void OnLoad() {
    Buckler = {};
    RStage = UltimateStage::Idle;
    LastQAim = LastEAim = LastRAim = {};
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    IncomingThreatUntil = IncomingHardCCUntil = GapcloserUntil = 0;
    GapcloserTargetId = LastAutoTargetId = LastAutoTick = 0;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    Buckler = {};
}
inline constexpr const char* Scenarios[] = {
    "Pin all mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Track Iron Ambassador buckler readiness through passive buffs, attacks and polling",
    "Preserve a thrown buckler pickup unless terrain, turret or enemy count makes it unsafe",
    "Use live Q range, line width, delay, delayed zone and target max-health damage",
    "Reject Q through observed walls and preserve attack windup unless reactive",
    "Activate W only for an observed dash, hard crowd-control threat or defensive posture",
    "Treat Steadfast Presence as anti-gapcloser control rather than a generic damage cast",
    "Require an observed terrain wall behind the target before Heroic Charge",
    "Apply E dash endpoint safety, turret rejection and wall stun radius",
    "Intercept enemy dashes with W before selecting an offensive sequence",
    "Track Keeper's Verdict charging and release posture from spell and buff events",
    "Use tap or charged R only with a valid predicted line and no projectile wall",
    "Require a knockback-away direction for charged R release",
    "Reserve nonlethal R for configured multi-target value, defense, interruption or flee",
    "Use autonomous orbwalker and engine target policy",
    "Combo prioritizes wall stun, Q damage and defensive W before verdict",
    "Harass spends mana on Q and uses W only for an observed dash",
    "LaneClear Jungle and LastHit delegate to shared farm policy",
    "Flee uses W peel and safe verdict knockback",
    "Automatic mode permits defense, anti-gapclose, interrupt or kill secure only",
    "Reconcile observed Q W E or R events",
    "Reject protected, invulnerable and spell-shielded targets",
    "Never issue items, summoner spells or movement",
    "Keep profile metadata separate from the decision loop",
    "Draw ranges without changing gameplay decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Poppy;
    controller.ControllerId = "champion.kuroaio.ai.poppy.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIPoppy.md";
    controller.ImplementationSummary =
        "Buckler-aware defensive loop with W dash denial, terrain-gated E stun and charged R knockback posture.";
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

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Poppy
