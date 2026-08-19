#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIEliseGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Elise {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttackEvent;
using ControllerHelpers::CaptureLocalAutoAttackEvent;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::SpellEventNameContainsAny;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Ready;
using ControllerHelpers::RuntimeNameContains;
using ControllerHelpers::PreserveAttack;

using ControllerHelpers::Now;

inline Menu* TacticsMenu = nullptr;
inline Menu* CocoonMenu = nullptr;
inline Menu* RappelMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline Form CurrentForm = Form::Unknown;
inline RappelPhase CurrentRappel = RappelPhase::Ready;
inline int RappelTargetId = 0;
inline int RappelExpireTick = 0;
inline int CocoonTargetId = 0;
inline int CocoonExpireTick = 0;
inline int SpiderlingCount = 0;
inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int PlayerOverrideUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int GapcloserTargetId = 0;
inline Vector3 GapcloserEndpoint{};
inline int GapcloserExpireTick = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline Vector3 LastRappelLanding{};

inline bool Throttle(int slot, int delay = 70) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           ControllerHelpers::HasSpellShieldOrImmunity(target) ||
           target.HasBuff("VladimirSanguinePool") || target.HasBuff("FizzEIcon") ||
           target.HasBuff("kindredrnodeathbuff") || target.HasBuff("ChronoShift");
}
inline bool HumanRuntime() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    return player.HasBuff("EliseHumanForm") || player.HasBuff("EliseHuman") ||
        RuntimeNameContains(0, "Neurotoxin") || RuntimeNameContains(1, "Volatile") ||
        RuntimeNameContains(2, "Cocoon");
}
inline bool SpiderRuntime() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    return player.HasBuff("EliseSpiderForm") || player.HasBuff("EliseSpider") ||
        RuntimeNameContains(0, "Venomous") || RuntimeNameContains(1, "Skittering") ||
        RuntimeNameContains(2, "Rappel");
}
inline void ReconcileForm() {
    if (SpiderRuntime()) CurrentForm = Form::Spider;
    else if (HumanRuntime()) CurrentForm = Form::Human;
    else if (CurrentForm == Form::Unknown) CurrentForm = Form::Human;
}
inline bool EnemySafeForDamage(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) && !Protected(target) && !target.IsDead();
}
inline bool SafeLanding(const Vector3& endpoint, bool defensive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !endpoint.IsValid() || endpoint.IsZero() ||
        SDK::NavMesh::IsWall(endpoint) || SDK::NavMesh::IsWallBetween(
            player.Position(), endpoint, 22.0f)) return false;
    if (!defensive && Engine::UnderEnemyTurret(endpoint) &&
        !Engine::UnderEnemyTurret(player.Position())) return false;
    const int maximum = Slider(RappelMenu, "MaxLandingEnemies", defensive ? 3 : 1);
    return Engine::CountEnemiesAt(endpoint, 250.0f) <= maximum;
}
inline bool RappelLandingAllowed(const Vector3& endpoint, bool defensive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const LandingSafety context{
        true,
        endpoint.IsValid() && !endpoint.IsZero() && !SDK::NavMesh::IsWall(endpoint),
        SDK::NavMesh::IsWallBetween(player.Position(), endpoint, 22.0f),
        !defensive && Engine::UnderEnemyTurret(endpoint) &&
            !Engine::UnderEnemyTurret(player.Position()),
        Engine::CountEnemiesAt(endpoint, 250.0f),
        Slider(RappelMenu, "MaxLandingEnemies", defensive ? 3 : 1),
        defensive};
    return ShouldDescend(context);
}
inline bool Lethal(const AIHeroClient& target, int slot) {
    return EnemySafeForDamage(target) && slot >= 0 && slot < 4 &&
        Engine::RuntimeSpells[slot] && Engine::RuntimeSpells[slot]->GetDamage(target) >=
            target.Health() + target.AllShield();
}
inline bool InBiteRange(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && EnemySafeForDamage(target) &&
        TargetIsInBiteRange({player.Position().x, player.Position().z},
                            {target.Position().x, target.Position().z}, target.BoundingRadius());
}

inline bool CastNeurotoxin(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || CurrentForm != Form::Human || !Ready(0, mode) ||
        !Throttle(0) || !EnemySafeForDamage(target) || PreserveAttack(reactive) ||
        player.Position().Distance2D(target.Position()) >
            kNeurotoxinRange + target.BoundingRadius()) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    LastCastTick[0] = Now();
    return true;
}
inline bool CastSpiderling(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || CurrentForm != Form::Human || !Ready(1, mode) ||
        !Throttle(1) || !EnemySafeForDamage(target) || PreserveAttack(reactive)) return false;
    Vector3 aim = PredictPosition(target, 0.25f);
    if (!aim.IsValid() || aim.IsZero()) aim = target.Position();
    if (player.Position().Distance2D(aim) > kVolatileSpiderlingRange + target.BoundingRadius() ||
        SDK::NavMesh::IsWallBetween(player.Position(), aim, 40.0f)) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    LastCastTick[1] = Now();
    return true;
}
inline bool CastCocoon(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || CurrentForm != Form::Human || !Ready(2, mode) ||
        !Throttle(2) || !EnemySafeForDamage(target) || PreserveAttack(reactive) ||
        player.Position().Distance2D(target.Position()) > kCocoonRange + target.BoundingRadius() ||
        !Engine::RuntimeSpells[2]) return false;
    const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    if (!prediction.CollisionObjects.empty() && !Engine::IsHardCrowdControlled(target)) return false;
    if (!reactive && prediction.Hitchance < SDK::HitChance::High && !target.IsDashing()) return false;
    Vector3 aim = prediction.GetCastPosition();
    if (!aim.IsValid() || aim.IsZero()) aim = PredictPosition(target, kCocoonDelay);
    if (!aim.IsValid() || aim.IsZero() || SDK::NavMesh::IsWallBetween(
            player.Position(), aim, kCocoonWidth * 0.5f) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(
            aim, kCocoonWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastCastTick[2] = Now();
    CocoonTargetId = static_cast<int>(target.NetworkId());
    CocoonExpireTick = Now() + kCocoonMarkMs;
    return true;
}
inline bool CastTransform(Mode mode, bool escape = false, bool reactive = false) {
    if (!Ready(3, mode) || !Throttle(3, 100) || PreserveAttack(reactive)) return false;
    const Form desired = CurrentForm == Form::Human ? Form::Spider : Form::Human;
    if (!ShouldTransform(CurrentForm, desired, true, escape, PlayerOverrideUntil > Now())) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    CurrentForm = desired;
    if (desired == Form::Human) {
        CurrentRappel = RappelPhase::Ready;
        RappelTargetId = 0;
        RappelExpireTick = 0;
    }
    return true;
}
inline bool CastVenomousBite(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || CurrentForm != Form::Spider || !Ready(0, mode) ||
        !Throttle(0) || !InBiteRange(target) || PreserveAttack(reactive)) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    LastCastTick[0] = Now();
    return true;
}
inline bool CastFrenzy(Mode mode, bool reactive = false) {
    if (CurrentForm != Form::Spider || !Ready(1, mode) || !Throttle(1) ||
        PreserveAttack(reactive)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    return true;
}
inline Vector3 RequestedLanding(const AIHeroClient& target, bool defensive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    if (defensive || !EnemySafeForDamage(target)) {
        Vector3 cursor = Game::CursorPos();
        if (!cursor.IsValid() || cursor.IsZero()) cursor = player.Position() +
            SharedGeometry::Direction2D(player.Position(), player.Position() +
                Vector3{1.0f, 0.0f, 0.0f}) * 500.0f;
        return player.Position() + SharedGeometry::Direction2D(player.Position(), cursor) *
            std::min(kRappelRange, player.Position().Distance2D(cursor));
    }
    return PredictPosition(target, 0.15f);
}
inline bool CastRappel(const AIHeroClient& target, Mode mode, bool defensive = false,
                       bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || CurrentForm != Form::Spider || !Ready(2, mode) ||
        !Throttle(2, defensive ? 45 : 90) || PreserveAttack(reactive)) return false;
    if (CurrentRappel == RappelPhase::Ready) {
        Vector3 aim = RequestedLanding(target, defensive);
        if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) >
            kRappelRange + 30.0f || (!defensive && Engine::UnderEnemyTurret(aim) &&
            !Engine::UnderEnemyTurret(player.Position()))) return false;
        if (!Engine::ControllerCastPosition(2, aim)) return false;
        LastCastTick[2] = Now();
        CurrentRappel = RappelPhase::Rising;
        RappelTargetId = EnemySafeForDamage(target) ? static_cast<int>(target.NetworkId()) : 0;
        RappelExpireTick = Now() + kRappelWindowMs;
        LastRappelLanding = aim;
        return true;
    }
    if (CurrentRappel != RappelPhase::Rising || Now() >= RappelExpireTick) return false;
    Vector3 endpoint = RequestedLanding(target, defensive);
    endpoint = {ClampRappelEndpoint(
        {player.Position().x, player.Position().z}, {endpoint.x, endpoint.z}).x,
        endpoint.y,
        ClampRappelEndpoint(
        {player.Position().x, player.Position().z}, {endpoint.x, endpoint.z}).y};
    if (!RappelLandingAllowed(endpoint, defensive) || !SafeLanding(endpoint, defensive)) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    LastCastTick[2] = Now();
    CurrentRappel = RappelPhase::Descending;
    LastRappelLanding = endpoint;
    return true;
}
inline bool TryFarm(Mode mode) {
    const bool jungle = mode == Mode::Jungle;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Engine::CountEnemiesAt(player.Position(), 1000.0f) > 0 &&
        Bool(FarmMenu, "StopNearEnemy", true)) return false;
    const auto& units = jungle ? GameObjects::Jungle() : GameObjects::EnemyMinions();
    AIBaseClient best{};
    float bestHealth = 0.0f;
    for (const auto& unit : units) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            player.Position().Distance2D(unit.Position()) > 700.0f) continue;
        if (unit.Health() > bestHealth) { best = AIBaseClient(unit.Handle()); bestHealth = unit.Health(); }
    }
    if (!best.IsValid()) return false;
    if (CurrentForm == Form::Human) {
        if (Ready(0, mode) && Throttle(0) && Engine::RuntimeSpells[0] &&
            Engine::RuntimeSpells[0]->GetDamage(best) >= best.Health() &&
            Engine::ControllerCastUnit(0, best)) { LastCastTick[0] = Now(); return true; }
        if (Ready(1, mode) && Throttle(1)) {
            const Vector3 aim = PredictPosition(best, 0.25f);
            if (aim.IsValid() && !aim.IsZero() && Engine::ControllerCastPosition(1, aim)) {
                LastCastTick[1] = Now(); return true;
            }
        }
        if (jungle && CastTransform(mode, false)) return true;
        return false;
    }
    if (jungle && CastFrenzy(mode)) return true;
    if (Ready(0, mode) && Throttle(0) && Engine::ControllerCastUnit(0, best)) {
        LastCastTick[0] = Now(); return true;
    }
    return false;
}
inline void ReconcileState() {
    ReconcileForm();
    const int now = Now();
    if (CocoonTargetId != 0 && now >= CocoonExpireTick) CocoonTargetId = CocoonExpireTick = 0;
    if (CurrentRappel != RappelPhase::Ready && now >= RappelExpireTick) {
        CurrentRappel = RappelPhase::Ready; RappelTargetId = RappelExpireTick = 0;
    }
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (PlayerOverrideUntil > Now()) return true;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return true;
    const AIHeroClient target = PreferredEnemyTarget(selected,
        CurrentForm == Form::Spider ? kRappelRange : kCocoonRange);
    const bool threatened = IncomingThreatUntil > Now() || IncomingHardCCUntil > Now();
    if (CurrentForm == Form::Spider && (mode == Mode::Flee || threatened ||
        player.HealthPercent() <= Slider(RappelMenu, "EmergencyHp", 30))) {
        if (CastRappel(NearestEnemyToPlayer(target, 900.0f), mode, true, true)) return true;
    }
    if (CurrentForm == Form::Human) {
        if (EnemySafeForDamage(target) && (mode == Mode::Combo || mode == Mode::Automatic) &&
            CastCocoon(target, mode, threatened)) return true;
        if (EnemySafeForDamage(target) && CastNeurotoxin(target, mode, Lethal(target, 0))) return true;
        if (EnemySafeForDamage(target) && (mode == Mode::Combo || mode == Mode::Harass) &&
            CastSpiderling(target, mode)) return true;
        if (EnemySafeForDamage(target) && (mode == Mode::Combo || mode == Mode::Automatic) &&
            InBiteRange(target) && CastTransform(mode)) return true;
    } else if (CurrentForm == Form::Spider) {
        if (EnemySafeForDamage(target) && CastVenomousBite(target, mode, Lethal(target, 0))) return true;
        if ((mode == Mode::Combo || mode == Mode::Harass || mode == Mode::Jungle) &&
            CastFrenzy(mode)) return true;
        if (EnemySafeForDamage(target) && Lethal(target, 0) &&
            CastRappel(target, mode, false, true)) return true;
        if (mode == Mode::Combo && Engine::CountEnemiesAt(player.Position(), 600.0f) == 0 &&
            CastTransform(mode)) return true;
    }
    switch (mode) {
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        return TryFarm(mode);
    case Mode::Flee:
        return CurrentForm == Form::Human ? CastTransform(mode, true, true) :
            CastRappel({}, mode, true, true);
    case Mode::Automatic:
        return threatened && CurrentForm == Form::Human && CastCocoon(target, mode, true);
    default:
        return false;
    }
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            if (!Engine::WasControllerCast(slot)) PlayerOverrideUntil = now +
                Slider(TacticsMenu, "ManualOwnershipMs", 560);
            LastCastTick[slot] = now;
        }
        if (SpellEventNameContainsAny(args, {"VenomousBite", "SkitteringFrenzy", "Rappel", "SpiderForm"})) {
            CurrentForm = Form::Spider;
        } else if (SpellEventNameContainsAny(args, {"Neurotoxin", "VolatileSpiderling", "Cocoon", "HumanForm"})) {
            CurrentForm = Form::Human;
        }
        if (SpellEventNameContainsAny(args, {"Rappel"})) {
            if (CurrentRappel == RappelPhase::Ready) CurrentRappel = RappelPhase::Rising;
            else CurrentRappel = RappelPhase::Descending;
            RappelExpireTick = now + kRappelWindowMs;
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
        IncomingThreatUntil = std::max(IncomingThreatUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
        if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(
            IncomingHardCCUntil, std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "EliseSpider")) CurrentForm = Form::Spider;
        if (Engine::TextContains(args.BuffName, "Rappel")) {
            CurrentRappel = RappelPhase::Rising; RappelExpireTick = Now() + kRappelWindowMs;
        }
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "EliseSpider")) {
        CurrentForm = Form::Human; CurrentRappel = RappelPhase::Ready;
    }
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    const auto target = Engine::EnemyByNetworkId(static_cast<int>(args.Target.NetworkId()));
    if (Engine::ValidEnemy(target) && Protected(target)) args.Process = false;
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    ControllerHelpers::CaptureGapcloser(args, GapcloserTargetId, GapcloserEndpoint,
                                        GapcloserExpireTick, 650.0f, 850);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    ControllerHelpers::CaptureInterruptable(args, InterruptTargetId, InterruptExpireTick, 900, 250, 5000);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() || !ControllerHelpers::ObjectEventIsAllied(args)) return;
    if (Engine::TextContains(args.Sender.Name, "EliseSpiderling") ||
        Engine::TextContains(args.Sender.CharacterName, "EliseSpiderling")) SpiderlingCount =
        std::min(3, SpiderlingCount + 1);
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (Engine::TextContains(args.Sender.Name, "EliseSpiderling") ||
        Engine::TextContains(args.Sender.CharacterName, "EliseSpiderling")) SpiderlingCount =
        std::max(0, SpiderlingCount - 1);
}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (Engine::TextContains(args.Sender.Name, "EliseHumanE") ||
        Engine::TextContains(args.Sender.CharacterName, "EliseHumanE") ||
        Engine::TextContains(args.MissileName, "Cocoon")) {
        CocoonExpireTick = std::max(CocoonExpireTick, Now() + 500);
    }
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (Engine::TextContains(args.Sender.Name, "EliseHumanE") ||
        Engine::TextContains(args.Sender.CharacterName, "EliseHumanE") ||
        Engine::TextContains(args.MissileName, "Cocoon")) {
        CocoonExpireTick = std::min(CocoonExpireTick, Now() + 80);
    }
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), CurrentForm == Form::Human ? kCocoonRange : kRappelRange,
                         0xFFB46CFFu, 1.5f, 42);
    if (!LastRappelLanding.IsZero()) Drawing::DrawCircle(LastRappelLanding,
        kRappelLandingRadius, 0xFFFFAA55u, 1.5f, 28);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("EliseOneTrick", "Elise form tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    CocoonMenu = TacticsMenu->AddSubMenu(new Menu("Cocoon", "Cocoon collision and prediction"));
    CocoonMenu->Add(new MenuBool("RequireHighHitchance", "Require high hitchance", true));
    RappelMenu = TacticsMenu->AddSubMenu(new Menu("Rappel", "Rappel landing safety"));
    RappelMenu->Add(new MenuSlider("MaxLandingEnemies", "Maximum landing enemies", 1, 0, 4));
    RappelMenu->Add(new MenuSlider("EmergencyHp", "Emergency Rappel HP", 30, 5, 80));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("EliseFarm", "Spiderling jungle farming"));
    FarmMenu->Add(new MenuBool("StopNearEnemy", "Stop farming near enemies", true));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("EliseCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Cocoon and Rappel ranges", false));
}
inline void OnLoad() {
    CurrentForm = Form::Unknown; CurrentRappel = RappelPhase::Ready;
    RappelTargetId = RappelExpireTick = CocoonTargetId = CocoonExpireTick = 0;
    SpiderlingCount = 0; std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = PlayerOverrideUntil = 0;
    IncomingThreatUntil = IncomingHardCCUntil = GapcloserTargetId = GapcloserExpireTick = 0;
    InterruptTargetId = InterruptExpireTick = 0; LastRappelLanding = {};
    ReconcileState();
}
inline void OnUnload() {
    TacticsMenu = CocoonMenu = RappelMenu = FarmMenu = CoachMenu = nullptr;
    CurrentForm = Form::Unknown; CurrentRappel = RappelPhase::Ready; SpiderlingCount = 0;
    LastRappelLanding = {};
}

inline constexpr const char* Scenarios[] = {
    "Pin Riot 26.15 and CommunityDragon 16.15 Summoner's Rift values",
    "Reconcile human and spider forms from runtime names, buffs, spell events and polling",
    "Preserve selected target before orbwalker and selector fallback",
    "Predict Cocoon with 1075 range, 55 width, collision object rejection and wall checks",
    "Use Neurotoxin current-health damage and Spider Bite missing-health execute gates",
    "Transform only for a justified bite, spiderling farm route, or safe disengage",
    "Track Rappel rising and descending phases with explicit target and landing endpoint",
    "Reject Rappel landings through walls, enemy turrets or excessive endpoint enemies",
    "Use Skittering Frenzy and spider Q for fast jungle clears without blind lane casts",
    "Preserve auto-attack windup and yield manual form or Rappel ownership",
    "Track Cocoon target expiry, Spiderling objects, enemy threat and interrupt windows",
    "Handle Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic modes",
    "Automatic mode is limited to Cocoon peel, lethal Bite and emergency Rappel",
    "Assign event and polling reconciliation callbacks without changing ChampionController ABI",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Elise;
    controller.ControllerId = "champion.kuroaio.ai.elise.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIElise.md";
    controller.ImplementationSummary =
        "Human Cocoon prediction and collision gates, current/missing-health Q form recasts, "
        "Spiderling object tracking and Rappel target/landing safety with jungle fast-farm policy.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &OnInterruptable;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Elise
