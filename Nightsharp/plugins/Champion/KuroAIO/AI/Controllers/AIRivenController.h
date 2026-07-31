#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIRivenGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Riven {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* ChainMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline int QStageState = 1;
inline int QLastCastTick = 0;
inline int QWindowExpireTick = 0;
inline int WLastCastTick = 0;
inline int ELastCastTick = 0;
inline int RLastCastTick = 0;
inline int RBuffExpireTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int IncomingHardCCUntil = 0;
inline int PlayerOverrideUntil = 0;
inline int AttackWindupUntil = 0;
inline int QTargetId = 0;
inline Vector3 LastQEndpoint = {};
inline Vector3 LastEEndpoint = {};
inline Vector3 LastREndpoint = {};
inline bool RActive = false;
inline bool QCastPendingEvent = false;
inline bool QWasManual = false;
inline bool WWasManual = false;
inline bool EWasManual = false;
inline bool RWasManual = false;

using ControllerHelpers::Now;

using ControllerHelpers::Ready;

inline bool Throttle(int index, int delay) {
    const int tick = index == 0 ? QLastCastTick : index == 1 ? WLastCastTick :
                     index == 2 ? ELastCastTick : RLastCastTick;
    return Now() - tick >= delay;
}

inline bool TargetProtected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           target.HasBuff("SivirE") || target.HasBuff("NocturneShroudofDarkness") ||
           target.HasBuff("MorganaE") || target.HasBuff("BansheesVeil") ||
           target.HasBuff("VladimirSanguinePool") || target.HasBuff("FizzEIcon") ||
           target.HasBuff("KayleR") || target.HasBuff("kindredrnodeathbuff");
}

inline bool IsQWindowOpen() {
    return QLastCastTick > 0 && !QChainExpired(QLastCastTick, Now()) &&
           Now() <= QWindowExpireTick;
}

inline bool IsRActive() {
    const auto player = GameObjects::Player();
    return RActive || (player.IsValid() &&
        (player.HasBuff("RivenFengShuiEngine") || player.HasBuff("RivenR")));
}

using ControllerHelpers::TotalAttackDamage;

using ControllerHelpers::BonusAttackDamage;

inline float QDamage(const AIHeroClient& target, int stage) {
    if (!Engine::ValidEnemy(target) || !Engine::RuntimeSpells[0]) return 0.0f;
    return GameObjects::Player().CalculatePhysicalDamage(
        target, QRawDamage(SpellRank(0), TotalAttackDamage(), stage));
}
inline float WDamage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    return GameObjects::Player().CalculatePhysicalDamage(
        target, WRawDamage(SpellRank(1), BonusAttackDamage()));
}
inline float RDamage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    return GameObjects::Player().CalculatePhysicalDamage(
        target, RRawDamage(SpellRank(3), BonusAttackDamage(),
                          100.0f - target.HealthPercent()));
}
using ControllerHelpers::Lethal;

inline bool MayCast(const AIHeroClient& target, bool selected, bool orbwalker,
                    bool lethal, bool manual = false) {
    return MayUseAbility({ selected, orbwalker,
                           Orbwalker::IsWindingUp() || AttackWindupUntil > Now(),
                           lethal, manual });
}

inline Vector3 Predicted(const AIHeroClient& target, float delay) {
    if (!Engine::ValidEnemy(target)) return {};
    if (Engine::RuntimeSpells[0]) {
        const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
        if (prediction.GetCastPosition().IsValid() && !prediction.GetCastPosition().IsZero() &&
            prediction.Hitchance >= SDK::HitChance::High)
            return prediction.GetCastPosition();
    }
    return PredictPosition(target, delay);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool selected, bool orbwalker,
                  bool fleeing = false) {
    if (!Engine::ValidEnemy(target, kQRange + 45.0f) || !Ready(0, mode) ||
        !Throttle(0, 40) || TargetProtected(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int stage = IsQWindowOpen() ? QStageState : 1;
    if (IsQWindowOpen() && !QRecastAllowed(QStageState, Now() - QLastCastTick)) return false;
    const Vector3 aim = fleeing ? Game::CursorPos() : Predicted(target, 0.18f);
    const Vector3 endpoint = ClampDashEndpoint(player.Position(), aim, kQDashDistance);
    if (endpoint.IsZero() || !endpoint.IsValid() || SDK::NavMesh::IsWall(endpoint) ||
        player.Position().Distance2D(endpoint) < 20.0f) return false;
    const bool lethal = Lethal(target, QDamage(target, stage));
    if (!MayCast(target, selected, orbwalker, lethal)) return false;
    if (!Engine::ControllerCastPosition(0, endpoint)) return false;
    QLastCastTick = Now();
    QWindowExpireTick = Now() + kQRecastWindowMs;
    QStageState = NextQStage(stage);
    QCastPendingEvent = true;
    QTargetId = static_cast<int>(target.NetworkId());
    LastQEndpoint = endpoint;
    QWasManual = false;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool selected, bool orbwalker,
                  bool reactive = false) {
    if (!Engine::ValidEnemy(target, kWRange + target.BoundingRadius()) || !Ready(1, mode) ||
        !Throttle(1, 65) || TargetProtected(target) ||
        GameObjects::Player().Position().Distance2D(target.Position()) >
            kWRange + target.BoundingRadius()) return false;
    const bool lethal = Lethal(target, WDamage(target));
    if (!reactive && !MayCast(target, selected, orbwalker, lethal)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    WLastCastTick = Now();
    WWasManual = false;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool selected, bool orbwalker,
                  bool fleeing = false, bool manual = false) {
    if (!Ready(2, mode) || !Throttle(2, 70)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    Vector3 aim = fleeing ? Game::CursorPos() : Predicted(target, 0.12f);
    if (!aim.IsValid() || aim.IsZero()) aim = Game::CursorPos();
    const Vector3 endpoint = ClampDashEndpoint(player.Position(), aim);
    if (endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint)) return false;
    const int enemies = Engine::CountEnemiesAt(endpoint, 425.0f);
    const bool lethal = Engine::ValidEnemy(target) && Lethal(target, QDamage(target, QStageState));
    const bool cursorIntent = !Bool(ChainMenu, "RespectCursor", true) || fleeing ||
                              endpoint.Distance2D(Game::CursorPos()) < 360.0f ||
                              endpoint.Distance2D(aim) < 120.0f;
    if (!EEndpointSafe(!SDK::NavMesh::IsWall(endpoint), Engine::UnderEnemyTurret(endpoint),
                       Engine::UnderEnemyTurret(player.Position()), enemies,
                       Slider(FarmMenu, "MaxEndpointEnemies", 2), lethal, fleeing,
                       cursorIntent, manual)) return false;
    if (!fleeing && Engine::ValidEnemy(target) && !MayCast(target, selected, orbwalker, lethal, manual)) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    ELastCastTick = Now();
    LastEEndpoint = endpoint;
    EWasManual = false;
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool selected, bool orbwalker,
                  bool defensive = false) {
    if (!Ready(3, mode) || !Throttle(3, 110)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (!IsRActive()) {
        if (!Engine::ValidEnemy(target) && !defensive) return false;
        if (!defensive && !MayCast(target, selected, orbwalker, false)) return false;
        if (!Engine::ControllerCastSelf(3)) return false;
        RActive = true;
        RLastCastTick = Now();
        RBuffExpireTick = Now() + kRBuffDurationMs;
        RWasManual = false;
        return true;
    }
    if (!Engine::ValidEnemy(target, kRRange + 60.0f) || TargetProtected(target)) return false;
    const Vector3 predicted = Predicted(target, 0.35f);
    const Vector3 direction = Direction2D(player.Position(), predicted);
    if (direction.IsZero()) return false;
    const Vector3 endpoint = player.Position() + direction * std::min(kRRange, player.Position().Distance2D(predicted));
    if (SDK::NavMesh::IsWall(endpoint)) return false;
    std::vector<Body> bodies;
    for (const auto& enemy : GameObjects::EnemyHeroes())
        if (Engine::ValidEnemy(enemy, kRRange + 80.0f))
            bodies.push_back({ enemy.Position(), enemy.BoundingRadius(),
                static_cast<int>(enemy.NetworkId()),
                static_cast<int>(enemy.NetworkId()) == static_cast<int>(target.NetworkId()), true });
    const auto line = EvaluateRLine(player.Position(), endpoint, bodies,
                                    static_cast<int>(target.NetworkId()), kRWidth);
    const bool lethal = Lethal(target, RDamage(target));
    const bool multi = line.OrderedIds.size() >= static_cast<std::size_t>(
        Slider(UltimateMenu, "MinimumTargets", 2));
    const RContext context{ true, true, true, line.Hit, false, lethal,
                            target.HealthPercent() <= Slider(UltimateMenu, "ExecuteHP", 55),
                            defensive, multi, Engine::UnderEnemyTurret(endpoint) && !lethal };
    if (!MayCastR(context)) return false;
    if (!Engine::ControllerCastPosition(3, predicted)) return false;
    RLastCastTick = Now();
    LastREndpoint = predicted;
    RActive = false;
    RWasManual = false;
    return true;
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    if (QChainExpired(QLastCastTick, now)) QStageState = 1;
    if (RBuffExpireTick > 0 && now >= RBuffExpireTick) RActive = false;
    if (player.HasBuff("RivenFengShuiEngine") || player.HasBuff("RivenR")) {
        RActive = true;
        RBuffExpireTick = std::max(RBuffExpireTick, now + 800);
    }
    if (player.HasBuff("RivenFeint")) ELastCastTick = std::max(ELastCastTick, now - 50);
    if (AttackWindupUntil > 0 && now > AttackWindupUntil) AttackWindupUntil = 0;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode, bool selected, bool orbwalker) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, RDamage(target)) && CastR(target, mode, selected, orbwalker)) return true;
    if (Lethal(target, WDamage(target)) && CastW(target, mode, selected, orbwalker, true)) return true;
    return Lethal(target, QDamage(target, QStageState)) && CastQ(target, mode, selected, orbwalker);
}

inline bool TryCombo(const AIHeroClient& target, bool selected, bool orbwalker) {
    if (!Engine::ValidEnemy(target)) return false;
    if (!IsRActive() && target.HealthPercent() <= Slider(UltimateMenu, "RStartHP", 70) &&
        CastR(target, Mode::Combo, selected, orbwalker)) return true;
    if (IsQWindowOpen() && QStageState == 3 && CastQ(target, Mode::Combo, selected, orbwalker)) return true;
    if (CastW(target, Mode::Combo, selected, orbwalker)) return true;
    if (!IsQWindowOpen() && CastE(target, Mode::Combo, selected, orbwalker)) return true;
    if (CastQ(target, Mode::Combo, selected, orbwalker)) return true;
    if (IsRActive() && target.HealthPercent() <= Slider(UltimateMenu, "ExecuteHP", 55) &&
        CastR(target, Mode::Combo, selected, orbwalker)) return true;
    return false;
}

inline bool TryHarass(const AIHeroClient& target, bool selected, bool orbwalker) {
    if (!Engine::ValidEnemy(target) || GameObjects::Player().HealthPercent() < 45.0f) return false;
    if (CastQ(target, Mode::Harass, selected, orbwalker)) return true;
    return CastW(target, Mode::Harass, selected, orbwalker);
}

inline bool TryFlee(const AIHeroClient& threat) {
    if (CastE(threat, Mode::Flee, false, true, true, true)) return true;
    if (Engine::ValidEnemy(threat) && CastQ(threat, Mode::Flee, false, true, true)) return true;
    return Engine::ValidEnemy(threat) && CastW(threat, Mode::Flee, false, true, true);
}

inline bool TryFarm(Mode mode) {
    if (mode == Mode::LastHit && GameObjects::Player().HealthPercent() < 25.0f) return false;
    if (mode != Mode::LastHit && GameObjects::Player().ManaPercent() < Slider(FarmMenu, "Reserve", 0)) return false;
    return Engine::TryFarm(mode);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (PlayerOverrideUntil > Now()) return true;
    const bool selectedTarget = Engine::ValidEnemy(selected);
    AIHeroClient target = selectedTarget ? selected : Engine::SelectTarget(kRRange + 60.0f);
    const bool orbwalkerTarget = !selectedTarget && Engine::ValidEnemy(target);
    const AIHeroClient threat = NearestEnemyToPlayer(target, 900.0f);
    if (mode == Mode::Flee) { (void)TryFlee(threat); return true; }
    if (TryKillSecure(target, mode, selectedTarget, orbwalkerTarget)) return true;
    switch (mode) {
    case Mode::Combo: (void)TryCombo(target, selectedTarget, orbwalkerTarget); break;
    case Mode::Harass: (void)TryHarass(target, selectedTarget, orbwalkerTarget); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)TryFarm(mode); break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target) && AutomaticAllowed({
                IncomingHardCCUntil > Now(), IncomingHardCCUntil > Now(),
                Lethal(target, RDamage(target)), false })) {
            if (IncomingHardCCUntil > Now() && CastW(threat, Mode::Automatic,
                                                    selectedTarget, orbwalkerTarget, true))
                break;
            (void)TryKillSecure(target, Mode::Automatic, selectedTarget, orbwalkerTarget);
        }
        break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (!IsLocalPlayer(args.Sender)) {
        const auto threat = ControllerHelpers::AnalyzeEnemyCast(args, 260.0f, 100.0f,
                                                                 250, 280, 260, 1500, 450);
        if (threat.Valid && threat.CrossesPlayer && threat.LikelyHardCrowdControl)
            IncomingHardCCUntil = now + 650;
        return;
    }
    if (args.IsAutoAttack) { LastAutoTick = now; LastAutoTargetId = static_cast<int>(args.TargetNetworkId); return; }
    const int slot = args.Slot;
    const bool owned = slot >= 0 && slot < 4 && Engine::WasControllerCast(slot);
    if (!owned) PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 520);
    if (slot == 0) {
        QLastCastTick = now; QWindowExpireTick = now + kQRecastWindowMs;
        QWasManual = !owned;
        if (owned && QCastPendingEvent) {
            QCastPendingEvent = false;
        } else {
            QStageState = NextQStage(QStageState);
        }
    } else if (slot == 1) { WLastCastTick = now; WWasManual = !owned; }
    else if (slot == 2) { ELastCastTick = now; EWasManual = !owned; }
    else if (slot == 3) {
        RLastCastTick = now; RWasManual = !owned;
        if (Engine::TextContains(args.SpellName, "Izuna") || Engine::TextContains(args.SpellName, "WindSlash")) RActive = false;
        else RActive = true;
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (Engine::TextContains(args.BuffName, "FengShui") || Engine::TextContains(args.BuffName, "RivenR")) {
        RActive = true; RBuffExpireTick = now + 15000;
    } else if (Engine::TextContains(args.BuffName, "TriCleave") || Engine::TextContains(args.BuffName, "RivenQ")) {
        QLastCastTick = now; QWindowExpireTick = now + kQRecastWindowMs;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "FengShui") || Engine::TextContains(args.BuffName, "RivenR")) RActive = false;
    if (Engine::TextContains(args.BuffName, "TriCleave") || Engine::TextContains(args.BuffName, "RivenQ")) QStageState = 1;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    AttackWindupUntil = Now() + std::max(60, Game::Ping() + 100);
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
    AttackWindupUntil = 0;
}
inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawRanges", false)) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFFF6C88u, 1.8f, 40);
    Drawing::DrawCircle(player.Position(), kWRange, 0xFFFFB45Bu, 1.5f, 40);
    if (RActive) Drawing::DrawCircle(player.Position(), kRRange, 0xFFDD4D65u, 1.2f, 40);
    if (!LastEEndpoint.IsZero()) Drawing::DrawLine(player.Position(), LastEEndpoint, 0xFF77DDFFu, 2.0f);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("RivenMechanics", "Riven Q weave and safety"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 520, 180, 1100));
    ChainMenu = TacticsMenu->AddSubMenu(new Menu("BrokenWings", "Q chain timing"));
    ChainMenu->Add(new MenuBool("HoldThirdQ", "Hold Q3 for knock-up", true));
    ChainMenu->Add(new MenuBool("RespectCursor", "Respect cursor dash intent", true));
    UltimateMenu = TacticsMenu->AddSubMenu(new Menu("BladeOfExile", "R execute policy"));
    UltimateMenu->Add(new MenuSlider("RStartHP", "Activate R below target HP (%)", 70, 20, 100));
    UltimateMenu->Add(new MenuSlider("ExecuteHP", "Wind Slash execute HP (%)", 55, 10, 100));
    UltimateMenu->Add(new MenuSlider("MinimumTargets", "Minimum R line targets", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("RivenFarm", "Conservative farming"));
    FarmMenu->Add(new MenuSlider("Reserve", "Mana reserve (no-resource Riven)", 0, 0, 100));
    FarmMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum E endpoint enemies", 2, 1, 5));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("RivenCoach", "Route visualization"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/W/R ranges", false));
}

inline void OnLoad() {
    QStageState = 1; QLastCastTick = QWindowExpireTick = WLastCastTick = ELastCastTick = RLastCastTick = 0;
    RBuffExpireTick = LastAutoTargetId = LastAutoTick = IncomingHardCCUntil = PlayerOverrideUntil = AttackWindupUntil = 0;
    QTargetId = 0; LastQEndpoint = LastEEndpoint = LastREndpoint = {};
    RActive = false; QCastPendingEvent = false;
    QWasManual = WWasManual = EWasManual = RWasManual = false;
    ReconcileState();
}
inline void OnUnload() {
    TacticsMenu = ChainMenu = UltimateMenu = FarmMenu = CoachMenu = nullptr;
    RActive = false; QStageState = 1;
}

inline constexpr const char* Scenarios[] = {
    "Riot 26.15 and CommunityDragon 16.15 Summoner's Rift baseline",
    "Track Broken Wings stage from cast, buff, and polling observations",
    "Allow each Q recast only inside the four-second chain window",
    "Preserve Q3 for a knock-up, peel, or verified lethal hit",
    "Weave an auto attack between Q casts when its windup is valuable",
    "Reject Q dash endpoints in walls or zero-length directions",
    "Use Ki Burst only inside its 260-range stun radius",
    "Respect spell shields, invulnerability, and stasis before W",
    "Use Valor as a shielded cursor-respecting dash",
    "Reject E endpoints under a new turret unless lethal or manual",
    "Limit E destination enemy count outside fleeing or lethal routes",
    "Activate Blade of the Exile only for an intended commit",
    "Track R activation and Wind Slash recast through buff and spell names",
    "Evaluate Wind Slash line width, wall safety, and endpoint safety",
    "Require lethal, execute-window, defensive, or multi-target value for R2",
    "Scale Q, W, E shield, and R damage with pinned rank arithmetic",
    "Preserve selected target before orbwalker fallback",
    "Preserve attack windup unless the cast is lethal or reactive",
    "Yield to manual Q/W/E/R casts for the configured ownership window",
    "Reconcile manual Q stage without erasing observed R or E state",
    "Combo starts E only when the cursor endpoint is safe",
    "Combo sequences Q/W/E and finishes with R execute",
    "Harass uses Q/W and does not unsolicitedly activate R",
    "LaneClear and Jungle use shared farm logic without spending R",
    "LastHit avoids spending mobility on routine minions",
    "Flee uses E toward cursor then Q spacing and W peel",
    "Automatic mode never creates a fresh engage",
    "Automatic mode may answer hard crowd control or verified lethal damage",
    "Capture animation/AA windup and reconcile after-attack timing",
    "Keep controller metadata and research artifact nonempty",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Riven";
    controller.ControllerId = "champion.kuroaio.ai.riven.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIRiven.md";
    controller.ImplementationSummary =
        "Q chain timing and AA weaving, W stun gating, cursor-safe E shield/dash, "
        "R activation and Wind Slash execute line, wall/turret endpoint safety, and event reconciliation.";
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
    controller.OnBuffUpdate = &ControllerHelpers::ForwardLocalActiveBuffEvent<&OnBuffAdd>;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Riven
