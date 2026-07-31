#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AINidaleeGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Nidalee {

using namespace Geometry;
using Vec3 = ::Vec3;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::RuntimeNameContains;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContainsAny;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* SpearMenu = nullptr;
inline Menu* HuntMenu = nullptr;
inline Menu* CougarMenu = nullptr;
inline Menu* HealMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline Form CurrentForm = Form::Unknown;
inline HuntMark ActiveHunt{};
inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int PlayerOverrideUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline Vec3 LastPounceEndpoint{};

using ControllerHelpers::Now;
inline bool Throttle(int slot, int delay = 80) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           ControllerHelpers::HasSpellShieldOrImmunity(target) ||
           target.HasBuff("VladimirSanguinePool") || target.HasBuff("FizzEIcon") ||
           target.HasBuff("kindredrnodeathbuff") || target.HasBuff("ChronoShift");
}
inline bool CougarRuntime() {
    const auto player = GameObjects::Player();
    return player.IsValid() &&
        (player.HasBuff("NidaleeCougarForm") || player.HasBuff("NidaleeAspectOfTheCougar") ||
         RuntimeNameContains(0, "Takedown") || RuntimeNameContains(1, "Pounce") ||
         RuntimeNameContains(2, "Swipe"));
}
inline void ReconcileForm() {
    CurrentForm = CougarRuntime() ? Form::Cougar : Form::Human;
}
using ControllerHelpers::Ready;
using ControllerHelpers::PreserveAttack;
inline bool Marked(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    const int id = static_cast<int>(target.NetworkId());
    return HuntActive(ActiveHunt, id, Now()) || target.HasBuff("NidaleeHunted") ||
           target.HasBuff("NidaleePassiveHunted") || target.HasBuff("NidaleePassiveMark");
}
inline bool SafeEndpoint(const Vector3& endpoint, bool defensive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !endpoint.IsValid() || endpoint.IsZero() ||
        SDK::NavMesh::IsWall(endpoint)) return false;
    if (!defensive && Engine::UnderEnemyTurret(endpoint) &&
        !Engine::UnderEnemyTurret(player.Position())) return false;
    return Engine::CountEnemiesAt(endpoint, 250.0f) <=
        Slider(CougarMenu, "MaxPounceEnemies", 2);
}
inline bool CastHumanQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || CurrentForm != Form::Human || !Ready(0, mode) ||
        !Throttle(0) || Protected(target) || PreserveAttack(reactive)) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    Vector3 aim = prediction.GetCastPosition();
    if (!aim.IsValid() || aim.IsZero()) aim = PredictPosition(target, kJavelinDelay);
    if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) >
        kJavelinRange + target.BoundingRadius() ||
        SDK::NavMesh::IsWallBetween(player.Position(), aim, kJavelinWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    ActiveHunt = {static_cast<int>(target.NetworkId()), Now() + kHuntDurationMs};
    return true;
}
inline bool CastHumanW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || CurrentForm != Form::Human || !Ready(1, mode) ||
        !Throttle(1) || !Bool(SpearMenu, "UseTrap", true) || PreserveAttack(reactive)) return false;
    Vector3 aim = Engine::ValidEnemy(target) ? PredictPosition(target, 0.25f) : Game::CursorPos();
    if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) >
        kTrapRange + (Engine::ValidEnemy(target) ? target.BoundingRadius() : 0.0f) ||
        SDK::NavMesh::IsWall(aim)) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    LastCastTick[1] = Now();
    return true;
}
inline bool CastHeal(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || CurrentForm != Form::Human || !Ready(2, mode) ||
        !Throttle(2) || PreserveAttack(reactive) || player.HealthPercent() >
        Slider(HealMenu, "SelfHealHp", 62)) return false;
    if (!Engine::ControllerCastUnit(2, player)) return false;
    LastCastTick[2] = Now();
    return true;
}
inline bool CastCougarQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || CurrentForm != Form::Cougar || !Ready(0, mode) ||
        !Throttle(0) || Protected(target) || PreserveAttack(reactive) ||
        player.Position().Distance2D(target.Position()) > kTakedownRange + target.BoundingRadius()) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    LastCastTick[0] = Now();
    return true;
}
inline bool CastPounce(const AIHeroClient& target, Mode mode, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || CurrentForm != Form::Cougar || !Ready(1, mode) ||
        !Throttle(1) || PreserveAttack(defensive)) return false;
    const bool hunted = Engine::ValidEnemy(target) && Marked(target);
    Vector3 requested = Engine::ValidEnemy(target) ? PredictPosition(target, 0.15f) : Game::CursorPos();
    const Vector3 direction = SharedGeometry::Direction2D(player.Position(), requested);
    if (direction.IsZero()) return false;
    const Vector3 endpoint = player.Position() + direction *
        std::min(hunted ? kHuntedPounceRange : kPounceRange,
                 player.Position().Distance2D(requested));
    if (!endpoint.IsValid() || endpoint.IsZero() || !SafeEndpoint(endpoint, defensive)) return false;
    const LeapSafety context{true, true, SDK::NavMesh::IsWallBetween(
        player.Position(), endpoint, kPounceRadius * 0.12f),
        !defensive && Engine::UnderEnemyTurret(endpoint) &&
            !Engine::UnderEnemyTurret(player.Position()),
        Engine::CountEnemiesAt(endpoint, 250.0f), Slider(CougarMenu, "MaxPounceEnemies", 2),
        defensive, hunted};
    if (!ShouldPounce(context)) return false;
    if (!Engine::ControllerCastPosition(1, endpoint)) return false;
    LastCastTick[1] = Now();
    LastPounceEndpoint = endpoint;
    return true;
}
inline bool CastSwipe(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || CurrentForm != Form::Cougar || !Ready(2, mode) ||
        !Throttle(2) || PreserveAttack(reactive) ||
        (!Engine::ValidEnemy(target) || player.Position().Distance2D(target.Position()) >
         kSwipeRadius + target.BoundingRadius())) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    LastCastTick[2] = Now();
    return true;
}
inline bool CastTransform(Form desired, Mode mode, bool reactive = false) {
    if (CurrentForm == desired || !Ready(3, mode) || !Throttle(3, 120) ||
        PreserveAttack(reactive)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    CurrentForm = desired;
    return true;
}
inline bool Lethal(const AIHeroClient& target, int slot) {
    return Engine::ValidEnemy(target) && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->GetDamage(target) >= target.Health() + target.AllShield();
}
inline bool CougarCombo(const AIHeroClient& target, Mode mode) {
    if (CastCougarQ(target, mode, Lethal(target, 0))) return true;
    if (CastSwipe(target, mode)) return true;
    if (CastPounce(target, mode)) return true;
    return false;
}
inline bool HumanCombo(const AIHeroClient& target, Mode mode) {
    if (CastHumanQ(target, mode)) return true;
    if (CastHumanW(target, mode)) return true;
    if (Marked(target) && CastTransform(Form::Cougar, mode)) return true;
    if (CastHeal(mode)) return true;
    return false;
}
inline bool Flee(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (CurrentForm == Form::Human && CastTransform(Form::Cougar, Mode::Flee, true)) return true;
    if (CurrentForm == Form::Cougar && CastPounce({}, Mode::Flee, true)) return true;
    return CurrentForm == Form::Human && CastHeal(Mode::Flee, true);
}
inline bool TryFarm(Mode mode) {
    if (!GameObjects::Player().IsValid() ||
        GameObjects::Player().ManaPercent() < Slider(FarmMenu, "ManaReserve", 28)) return false;
    return Engine::TryFarm(mode);
}
inline void ReconcileState() {
    ReconcileForm();
    const int now = Now();
    if (ActiveHunt.TargetId != 0 && now >= ActiveHunt.ExpireTick) ActiveHunt = {};
    const auto player = GameObjects::Player();
    if (player.IsValid() && player.HasBuff("NidaleeHunted")) {
        ActiveHunt.ExpireTick = std::max(ActiveHunt.ExpireTick, now + kHuntDurationMs);
    }
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (PlayerOverrideUntil > Now()) return true;
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, CurrentForm == Form::Cougar ?
        kHuntedPounceRange : kJavelinRange);
    const bool threatened = IncomingThreatUntil > Now() || IncomingHardCCUntil > Now();
    if (CurrentForm == Form::Human && (threatened ||
        GameObjects::Player().HealthPercent() <= Slider(HealMenu, "EmergencyHp", 38)) &&
        CastHeal(mode, true)) return true;
    if (Engine::ValidEnemy(target) && CurrentForm == Form::Cougar &&
        Lethal(target, 0) && CastCougarQ(target, mode, true)) return true;
    switch (mode) {
    case Mode::Combo:
        return CurrentForm == Form::Human ? HumanCombo(target, mode) : CougarCombo(target, mode);
    case Mode::Harass:
        if (CurrentForm == Form::Human) return CastHumanQ(target, mode) || CastHumanW(target, mode);
        return CastCougarQ(target, mode) || CastPounce(target, mode);
    case Mode::Flee:
        return Flee(NearestEnemyToPlayer(target, 1000.0f));
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        return TryFarm(mode);
    case Mode::Automatic:
        if (threatened && CurrentForm == Form::Cougar) return CastPounce({}, mode, true);
        if (threatened && CurrentForm == Form::Human) return CastHeal(mode, true);
        return Engine::ValidEnemy(target) && Lethal(target, 0) &&
            (CurrentForm == Form::Cougar ? CastCougarQ(target, mode, true) : CastHumanQ(target, mode, true));
    default: return false;
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
        if (SpellEventNameContainsAny(args, {"Takedown", "Pounce", "Swipe", "Cougar"}))
            CurrentForm = Form::Cougar;
        else if (SpellEventNameContainsAny(args, {"JavelinToss", "Bushwhack", "PrimalSurge"}))
            CurrentForm = Form::Human;
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
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "Cougar")) CurrentForm = Form::Cougar;
        return;
    }
    if (Engine::TextContains(args.BuffName, "Hunted")) {
        const auto enemy = Engine::EnemyByNetworkId(id);
        if (Engine::ValidEnemy(enemy)) ActiveHunt = {id, Now() + kHuntDurationMs};
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (static_cast<int>(args.Sender.NetworkId) == ActiveHunt.TargetId &&
        Engine::TextContains(args.BuffName, "Hunted")) ActiveHunt = {};
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "Cougar"))
        ReconcileForm();
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    const auto target = Engine::EnemyByNetworkId(static_cast<int>(args.Target.NetworkId()));
    if (Engine::ValidEnemy(target) && Protected(target)) args.Process = false;
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), CurrentForm == Form::Cougar ?
        kHuntedPounceRange : kJavelinRange, 0xFF7BCBFFu, 1.5f, 42);
    if (!LastPounceEndpoint.IsZero()) Drawing::DrawCircle(LastPounceEndpoint,
        kPounceRadius, 0xFFFFAA55u, 1.5f, 28);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("NidaleeOneTrick", "Nidalee form tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    SpearMenu = TacticsMenu->AddSubMenu(new Menu("Javelin", "Spear and trap setup"));
    SpearMenu->Add(new MenuBool("UseTrap", "Use Bushwhack setup", true));
    HuntMenu = TacticsMenu->AddSubMenu(new Menu("Hunt", "Hunt mark tracking"));
    HuntMenu->Add(new MenuBool("RequireMarkForPounce", "Require Hunt for long Pounce", true));
    CougarMenu = TacticsMenu->AddSubMenu(new Menu("Cougar", "Cougar leap safety"));
    CougarMenu->Add(new MenuSlider("MaxPounceEnemies", "Maximum enemies at Pounce endpoint", 2, 0, 5));
    HealMenu = TacticsMenu->AddSubMenu(new Menu("Heal", "Primal Surge"));
    HealMenu->Add(new MenuSlider("SelfHealHp", "Self-heal below HP", 62, 10, 95));
    HealMenu->Add(new MenuSlider("EmergencyHp", "Emergency heal HP", 38, 5, 80));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("NidaleeFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("ManaReserve", "Minimum mana percent", 28, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("NidaleeCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw spear and leap ranges", false));
}
inline void OnLoad() {
    CurrentForm = Form::Unknown;
    ActiveHunt = {};
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = PlayerOverrideUntil = 0;
    IncomingThreatUntil = IncomingHardCCUntil = 0;
    LastPounceEndpoint = {};
    ReconcileState();
}
inline void OnUnload() {
    TacticsMenu = SpearMenu = HuntMenu = CougarMenu = HealMenu = FarmMenu = CoachMenu = nullptr;
    ActiveHunt = {};
    LastPounceEndpoint = {};
}
inline constexpr const char* Scenarios[] = {
    "Pin Riot 26.15 and CommunityDragon 16.15 Summoner's Rift values",
    "Reconcile human/cougar form from spell names, form buffs, events and polling",
    "Preserve selected target before orbwalker and selector fallback",
    "Track Hunt marks by enemy network id, buff callbacks and four-second expiry polling",
    "Use high-confidence predicted Javelin with 1500 range, collision and wall rejection",
    "Use Bushwhack as a conservative setup/trap placement rather than blind combat damage",
    "Heal self only from observed low-health or incoming-threat states",
    "Use Cougar Takedown at melee range and prioritize lethal missing-health damage",
    "Use Swipe only when the target is inside the observed cougar cone radius",
    "Clamp Pounce endpoints to 375 normal or 750 Hunted reach",
    "Reject Pounce through walls, enemy turret destinations and excessive endpoint enemies",
    "Require Hunt evidence before spending the long Pounce path when configured",
    "Transform only when a target or escape route justifies a form transition",
    "Preserve auto-attack windup unless reactive or lethal action requires interruption",
    "Yield manual spell ownership and reconcile manual form transitions",
    "Handle Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic modes",
    "Automatic mode permits only emergency sustain, defensive leap or kill secure",
    "Never automate items, summoner spells or movement ownership",
    "Draw range and last leap endpoint telemetry without changing decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Nidalee";
    controller.ControllerId = "champion.kuroaio.ai.nidalee.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AINidalee.md";
    controller.ImplementationSummary =
        "Human/cougar form reconciliation, Hunt-aware spear and pounce sequencing, "
        "Primal Surge sustain and conservative leap endpoint safety.";
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
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffEvent<OnBuffAdd>;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Nidalee
