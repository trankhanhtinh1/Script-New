#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIRammusGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Rammus {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PlayerManaPercent;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline PowerballState Ball{};
inline bool CurlActive = false;
inline int TauntTargetId = 0;
inline int LandingTick = 0;
inline int RAftershockUntil = 0;
inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int GapcloserTargetId = 0;
inline Vector3 GapcloserEnd{};
inline int GapcloserUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int InterruptUntil = 0;
inline int PlayerOverrideUntil = 0;
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
using ControllerHelpers::Protected;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Lethal;
using ControllerHelpers::Ready;

inline bool Throttle(int slot, int delay = 70) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
inline bool HasManaFor(int slot, float reserve = 0.0f) {
    return ControllerHelpers::CurrentResource() + 0.5f >=
        SpellCost(slot) + std::max(0.0f, reserve);
}
inline bool TargetIsTaunted(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) &&
        static_cast<int>(target.NetworkId()) == TauntTargetId;
}
inline bool RammusUnderTurretTransition(const Vector3& destination) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::UnderEnemyTurret(destination) &&
        !Engine::UnderEnemyTurret(player.Position());
}
inline bool SafeEndpoint(const Vector3& endpoint, bool defensive, bool lethal) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !endpoint.IsValid() || endpoint.IsZero() ||
        SDK::NavMesh::IsWall(endpoint)) return false;
    return SafeLanding(endpoint, SDK::NavMesh::IsWallBetween(player.Position(), endpoint, 0.0f),
        RammusUnderTurretTransition(endpoint), Engine::CountEnemiesAt(endpoint, 400.0f),
        Slider(RMenu, "MaximumLandingEnemies", 3), defensive || lethal);
}
inline float PassiveDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculateMagicDamage(target,
        PassiveBonusAttackDamage(player.Armor(), player.SpellBlock()));
}
inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculateMagicDamage(target, QRawDamage(SpellRank(0), player.AP()));
}
inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculateMagicDamage(target,
        WReturnRawDamage(SpellRank(1), player.BonusArmor(), player.BonusSpellBlock()));
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculateMagicDamage(target, EMonsterRawDamage(SpellRank(2), player.AP()));
}
inline float RDamage(const AIHeroClient& target, bool powerballCenter = false,
                     float travelDistance = 800.0f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    float raw = RInitialRawDamage(SpellRank(3), player.AP()) *
        SoaringSlamLandingDamageMultiplier(travelDistance) +
        RPulseRawDamage(SpellRank(3), player.AP()) * static_cast<float>(kRPulses);
    if (powerballCenter) raw += RPowerballBonusRawDamage(SpellRank(0), player.AP());
    return player.CalculateMagicDamage(target, raw);
}
inline bool EnemyAtPowerballImpact(const AIHeroClient& target, const Vector3& aim) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target) &&
        PowerballImpactHits(player.Position(), aim, PredictPosition(target, 0.20f),
                            target.BoundingRadius());
}
inline Vector3 AimFor(const AIHeroClient& target, float delay) {
    if (!Engine::ValidEnemy(target)) return {};
    Vector3 aim = PredictPosition(target, delay);
    if (Engine::RuntimeSpells[0]) {
        const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
        if (prediction.Hitchance >= SDK::HitChance::High &&
            prediction.GetCastPosition().IsValid()) aim = prediction.GetCastPosition();
    }
    return aim;
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool cancel = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0, mode) || !Throttle(0) || !HasManaFor(0) ||
        (!reactive && ControllerHelpers::PlayerMobilityLocked()) ||
        PreserveAttack(reactive)) return false;
    if (Ball.Active) {
        if (!cancel && (!Engine::ValidEnemy(target) || !EnemyAtPowerballImpact(target, Ball.Aim)))
            return false;
        if (!Engine::ControllerCastSelf(0)) return false;
        Ball.Impacted = cancel;
        Ball.Active = false;
        LastCastTick[0] = Now();
        return true;
    }
    if (Protected(target)) return false;
    const Vector3 aim = ClampPowerballAim(player.Position(), AimFor(target, 0.25f));
    if (aim.IsZero() || !PowerballCollisionObserved(player.Position(), aim,
        SDK::NavMesh::IsWallBetween(player.Position(), aim, 0.0f), target.BoundingRadius())) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    Ball = {true, false, player.Position(), aim, Now(), 0};
    LastCastTick[0] = Now();
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1, 110) || !HasManaFor(1) ||
        PreserveAttack(reactive)) return false;
    const bool threat = Engine::ValidEnemy(target, kWRadius) &&
        (target.IsDashing() || IncomingThreatUntil > Now() ||
         Engine::IsHardCrowdControlled(target));
    const bool low = player.HealthPercent() <= Slider(WMenu, "DefensiveHealth", 42);
    if (!reactive && !threat && !low && mode == Mode::Harass) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    CurlActive = true;
    LastCastTick[1] = Now();
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool killSecure = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(2, mode) || !Throttle(2) ||
        !HasManaFor(2) || PreserveAttack(reactive) || !TauntReachable(
            player.Position(), PredictPosition(target, 0.20f), target.BoundingRadius())) return false;
    if (!reactive && !killSecure && mode == Mode::Harass && !target.IsDashing()) return false;
    if (!Engine::ControllerCastUnit(2, target)) return false;
    TauntTargetId = static_cast<int>(target.NetworkId());
    LastCastTick[2] = Now();
    return true;
}
inline int PredictedLandingEnemies(const Vector3& center) {
    int hits = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && LandingHits(center,
            PredictPosition(enemy, 0.45f), enemy.BoundingRadius())) ++hits;
    }
    return hits;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool manual = false, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) || !Ready(3, mode) ||
        !Throttle(3, 130) || !HasManaFor(3) ||
        (!reactive && !defensive && ControllerHelpers::PlayerMobilityLocked()) ||
        PreserveAttack(reactive) || Protected(target)) return false;
    const Vector3 aim = PredictPosition(target, 0.45f);
    const float reach = SoaringSlamReach(player.MoveSpeed());
    if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) > reach + target.BoundingRadius()) return false;
    const bool wall = false;
    const bool lethal = Lethal(target, RDamage(target, Ball.Active,
        player.Position().Distance2D(aim)));
    const bool center = Ball.Active && CenterKnockupHits(aim, PredictPosition(target, 0.45f), target.BoundingRadius());
    const bool landingSafe = SafeEndpoint(aim, defensive, lethal);
    const LandingContext context{true, true, wall, landingSafe, RammusUnderTurretTransition(aim),
        defensive || player.HealthPercent() <= Slider(RMenu, "DefensiveHealth", 42), lethal,
        center, PredictedLandingEnemies(aim), Slider(RMenu, "MinimumTargets", 2)};
    if (!manual && !ShouldLandSoaringSlam(context)) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    LandingTick = Now() + 450;
    RAftershockUntil = LandingTick + static_cast<int>(kRPulseDuration * 1000.0f);
    LastCastTick[3] = Now();
    return true;
}
inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, RDamage(target, Ball.Active)) && CastR(target, mode, true, false, false)) return true;
    if (Lethal(target, EDamage(target)) && CastE(target, mode, true, true)) return true;
    if (Lethal(target, QDamage(target)) && CastQ(target, mode, true)) return true;
    return Lethal(target, PassiveDamage(target)) && CastW(target, mode, true);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (Ball.Active && CastR(target, Mode::Combo, false, false, false)) return;
    if (!Ball.Active && CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo, false, Lethal(target, EDamage(target)))) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || PlayerManaPercent() < Slider(QMenu, "HarassMana", 55)) return;
    if (!Ball.Active && CastQ(target, Mode::Harass)) return;
    if (CastE(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& threat) {
    if (!Engine::ValidEnemy(threat)) return;
    if (CastW(threat, Mode::Flee, true)) return;
    if (Ball.Active && CastQ(threat, Mode::Flee, true, true)) return;
    if (CastE(threat, Mode::Flee, true)) return;
    (void)CastR(threat, Mode::Flee, true, true, true);
}
inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const bool ballBuff = player.HasBuff("PowerBall") || player.HasBuff("PowerBallBuff") ||
        player.HasBuff("RammusQ");
    Ball.Active = Ball.Active || ballBuff;
    if (Ball.Active && Ball.StartTick <= 0) {
        Ball.StartTick = now;
        Ball.Origin = player.Position();
    }
    if (Ball.Active && !PowerballChargeValid(Ball, now) && now > Ball.StartTick + 6100) Ball = {};
    CurlActive = player.HasBuff("DefensiveBallCurl") || player.HasBuff("RammusW");
    if (!CurlActive && LastCastTick[1] > 0 && now - LastCastTick[1] > 7500) CurlActive = false;
    if (TauntTargetId != 0 && now > IncomingThreatUntil + 2200) TauntTargetId = 0;
    if (RAftershockUntil > 0 && now > RAftershockUntil) {
        LandingTick = 0;
        RAftershockUntil = 0;
    }
    if (GapcloserUntil <= now) GapcloserTargetId = 0;
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const auto target = ControllerHelpers::PreferredEnemyTarget(selected,
        mode == Mode::Flee ? 1050.0f : SoaringSlamReach(player.MoveSpeed()));
    if (PlayerOverrideUntil > Now()) return true;
    if (Engine::ValidEnemy(target) && GapcloserTargetId != 0 &&
        static_cast<int>(target.NetworkId()) == GapcloserTargetId &&
        CastE(target, mode, true)) return true;
    if (Engine::ValidEnemy(target) && IncomingHardCCUntil > Now() && CastW(target, mode, true)) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1050.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (PlayerManaPercent() >= Slider(FarmMenu, "Mana", 40)) (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic: {
        const bool defensive = IncomingThreatUntil > Now() || IncomingHardCCUntil > Now() ||
            player.HealthPercent() <= Slider(WMenu, "DefensiveHealth", 42);
        const bool antiGap = GapcloserTargetId != 0;
        const bool interrupt = InterruptUntil > Now();
        const bool kill = Engine::ValidEnemy(target) && Lethal(target, RDamage(target, Ball.Active));
        if (AutomaticAllowed({defensive, antiGap, interrupt, kill, false, PlayerOverrideUntil > Now()})) {
            if (Engine::ValidEnemy(target) && (antiGap || interrupt) && CastE(target, mode, true)) return true;
            if (Engine::ValidEnemy(target) && defensive && CastW(target, mode, true)) return true;
            if (Engine::ValidEnemy(target)) (void)CastR(target, mode, true, false, defensive);
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
        if (slot < 0 || slot > 3) return;
        if (!Engine::WasControllerCast(slot)) PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
        LastCastTick[slot] = now;
        if (slot == 0) {
            if (!Ball.Active) Ball = {true, false, GameObjects::Player().Position(), args.EndPosition, now, 0};
            else Ball.Active = false;
        } else if (slot == 1) {
            CurlActive = !CurlActive;
        } else if (slot == 2) {
            TauntTargetId = args.TargetNetworkId != 0 ? static_cast<int>(args.TargetNetworkId) : TauntTargetId;
        } else if (slot == 3) {
            LandingTick = now + 450;
            RAftershockUntil = LandingTick + static_cast<int>(kRPulseDuration * 1000.0f);
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(IncomingHardCCUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "PowerBall") || Engine::TextContains(args.BuffName, "RammusQ")) {
        Ball.Active = true; Ball.Impacted = false; Ball.StartTick = Now();
        Ball.Origin = GameObjects::Player().Position();
    }
    if (Engine::TextContains(args.BuffName, "DefensiveBallCurl") || Engine::TextContains(args.BuffName, "RammusW")) CurlActive = true;
    if (Engine::TextContains(args.BuffName, "Tremors") || Engine::TextContains(args.BuffName, "RammusR")) {
        LandingTick = Now(); RAftershockUntil = Now() + static_cast<int>(kRPulseDuration * 1000.0f);
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "PowerBall") || Engine::TextContains(args.BuffName, "RammusQ")) Ball = {};
    if (Engine::TextContains(args.BuffName, "DefensiveBallCurl") || Engine::TextContains(args.BuffName, "RammusW")) CurlActive = false;
    if (Engine::TextContains(args.BuffName, "Tremors") || Engine::TextContains(args.BuffName, "RammusR")) {
        LandingTick = 0; RAftershockUntil = 0;
    }
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) TauntTargetId = static_cast<int>(args.Target.NetworkId());
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) LastAutoTick = Now();
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (CaptureGapcloser(args, GapcloserTargetId, GapcloserEnd, GapcloserUntil, 700.0f, 900)) {
        IncomingThreatUntil = std::max(IncomingThreatUntil, GapcloserUntil);
    }
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs&) {
    InterruptUntil = Now() + 650;
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (Engine::TextContains(args.SpellName, "PowerBall") || Engine::TextContains(args.MissileName, "PowerBall")) Ball.Active = true;
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (Engine::TextContains(args.SpellName, "PowerBall") || Engine::TextContains(args.MissileName, "PowerBall")) Ball = {};
}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) { OnObjectCreate(args); }
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) { OnObjectDelete(args); }
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFCC8844u, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kWRadius, 0xFF66AAFFu, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kERange, 0xFFFFAA55u, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), SoaringSlamReach(player.MoveSpeed()), 0xFFAA66FFu, 1.0f, 48);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("RammusMechanics", "Rammus armor posture"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 560, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Powerball", "Powerball charge and impact"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 55, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("BallCurl", "Defensive Ball Curl posture"));
    WMenu->Add(new MenuSlider("DefensiveHealth", "Defensive health percent", 42, 10, 80));
    EMenu = TacticsMenu->AddSubMenu(new Menu("Taunt", "Frenzying Taunt peel"));
    EMenu->Add(new MenuBool("AntiGapcloser", "Taunt gapclosers", true));
    RMenu = TacticsMenu->AddSubMenu(new Menu("SoaringSlam", "Landing and aftershocks"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum nonlethal landing targets", 2, 1, 5));
    RMenu->Add(new MenuSlider("MaximumLandingEnemies", "Maximum enemies at landing", 3, 0, 5));
    RMenu->Add(new MenuSlider("DefensiveHealth", "Defensive landing health percent", 42, 10, 80));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Farm", "Farm resource reserve"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 40, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("Coach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/W/E/R ranges", false));
}
inline void OnLoad() {
    Ball = {}; CurlActive = false; TauntTargetId = LandingTick = RAftershockUntil = 0;
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = GapcloserTargetId = GapcloserUntil = 0;
    GapcloserEnd = {}; IncomingThreatUntil = IncomingHardCCUntil = InterruptUntil = PlayerOverrideUntil = 0;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    Ball = {}; CurlActive = false;
}
inline constexpr const char* Scenarios[] = {
    "Pin all spell arithmetic to Riot 26.15 and CommunityDragon 16.15",
    "Track Powerball charge origin, six-second roll lifetime and impact/recast transitions",
    "Predict Q impact with the live 300 range and 200 impact radius",
    "Reject Powerball paths crossing observed terrain and preserve AA windup unless reactive",
    "Use Defensive Ball Curl as an armor/MR posture against attack and hard-CC threats",
    "Track Curl activation and recast through events plus polling reconciliation",
    "Treat Frenzying Taunt as a 325-range point-click peel and monster control spell",
    "Intercept an observed gapcloser before selecting offensive casts",
    "Apply Spiked Shell bonus attack damage from live armor and magic resist",
    "Require selected target before orbwalker fallback and reject protected targets",
    "Compute Soaring Slam reach from movement speed and clamp it to 1700",
    "Predict landing collision, reject walls and unsafe enemy-turret destinations",
    "Require lethal, defensive, Powerball-center or configured multi-target R value",
    "Track R landing timing and all three aftershock pulses from buffs and polling",
    "Use center landing while Powerball is active for knockup and bonus impact damage",
    "Combo prioritizes Powerball impact, Taunt, Curl and safe Soaring Slam",
    "Harass spends mana on Q/E and uses Curl only for an observed threat",
    "LaneClear Jungle and LastHit delegate to shared farm policy",
    "Flee uses Curl, cancel-safe Powerball, Taunt peel and manual-assist landing",
    "Automatic mode permits only defense, anti-gapcloser, interruption or kill secure",
    "Yield after observed manual Q W E or R ownership and reconcile spell buffs",
    "Reject invulnerable, spell-shielded and invalid targets",
    "Never automate items, summoner spells or movement ownership",
    "Draw live Q W E and speed-scaled R reach without changing decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Rammus;
    controller.ControllerId = "champion.kuroaio.ai.rammus.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIRammus.md";
    controller.ImplementationSummary =
        "Powerball impact and curl posture controller with taunt gapcloser peel, armor-scaled passive damage, and speed-scaled Soaring Slam landing/aftershock safety.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &OnDoCast;
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

} // namespace Plugins::KuroAIO::AI::Controllers::Rammus
