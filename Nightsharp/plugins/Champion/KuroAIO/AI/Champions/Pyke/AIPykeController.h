#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIPykeGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Pyke {

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

inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int ManualOverrideUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int QChargeStartTick = 0;
inline int ETrailUntil = 0;
inline int WStealthUntil = 0;
inline int LastRTargetId = 0;
inline bool WActive = false;
inline bool QCharging = false;
inline bool ETrailActive = false;
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
inline bool Throttle(int slot, int delay = 75) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
using ControllerHelpers::Protected;
using ControllerHelpers::PreserveAttack;
inline bool IsGhostwaterActive(const AIHeroClient& player) {
    return player.HasBuff("PykeW") || player.HasBuff("pykew") ||
        player.HasBuff("PykeWStealth");
}
inline bool IsGreyHealthAvailable(const AIHeroClient& player) {
    return player.HasBuff("PykePassive") || player.HasBuff("pykepassive") ||
        player.HasBuff("PykePassiveGreyHealth");
}
inline bool IsQCharging(const AIHeroClient& player) {
    return player.HasBuff("PykeQ") || player.HasBuff("pykeq") ||
        player.HasBuff("PykeQCharge");
}
inline bool IsRMarked(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) &&
        (target.HasBuff("PykeRExecute") || target.HasBuff("pykerexecute") ||
         target.HasBuff("PykeRMark"));
}
inline bool Lethal(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target) && Engine::RuntimeSpells[3] &&
        Engine::RuntimeSpells[3]->GetDamage(target) >= target.Health() + target.AllShield();
}
inline bool CursorAgrees(const Vector3& endpoint, float minimumDot = 0.05f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const Vector3 toEndpoint = SharedGeometry::Direction2D(player.Position(), endpoint);
    const Vector3 cursor = SharedGeometry::Direction2D(player.Position(), Game::CursorPos());
    return toEndpoint.IsZero() || cursor.IsZero() || toEndpoint.Dot(cursor) >= minimumDot;
}
inline bool SafeEndpoint(const Vector3& endpoint, bool defensive, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint)) return false;
    const bool safe = TurretEndpointSafe(
        Engine::UnderEnemyTurret(endpoint), Engine::UnderEnemyTurret(player.Position()),
        defensive || lethal, Engine::CountEnemiesAt(endpoint, 250.0f),
        Slider(EMenu, "MaxEndpointEnemies", 2));
    return safe && (defensive || CursorAgrees(endpoint));
}
inline Vector3 AimFor(const AIHeroClient& target, float delay) {
    if (!Engine::ValidEnemy(target)) return {};
    Vector3 aim = PredictPosition(target, delay);
    if (Engine::RuntimeSpells[0]) {
        const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
        if (prediction.Hitchance >= SDK::HitChance::High &&
            prediction.GetCastPosition().IsValid() && !prediction.GetCastPosition().IsZero())
            aim = prediction.GetCastPosition();
    }
    return aim;
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0, mode) || !Throttle(0) || Protected(target) ||
        PreserveAttack(reactive)) return false;
    const Vector3 aim = AimFor(target, 0.22f);
    if (!aim.IsValid() || aim.IsZero()) return false;
    const float distance = player.Position().Distance2D(aim);
    const bool charged = QCharging || QIsCharged((Now() - QChargeStartTick) / 1000.0f);
    const float range = charged ? kQThrowRange : kQTapRange;
    if (distance > range + target.BoundingRadius() ||
        !QLineHits(player.Position(), aim, target.Position(), target.BoundingRadius(), range) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    LastRTargetId = static_cast<int>(target.NetworkId());
    QCharging = false;
    QChargeStartTick = 0;
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1) || PreserveAttack(reactive)) return false;
    const bool enemyValid = Engine::ValidEnemy(target);
    const float distance = enemyValid ? player.Position().Distance2D(target.Position()) : 9999.0f;
    const bool attackedRecently = LastAutoTick > 0 && Now() - LastAutoTick < 1100;
    if (!WStealthTargetAllowed(distance, attackedRecently, enemyValid || reactive)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    WActive = true;
    WStealthUntil = Now() + Slider(WMenu, "StealthHoldMs", 2600);
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || !Throttle(2) || Protected(target) ||
        PreserveAttack(reactive)) return false;
    const Vector3 aim = PredictPosition(target, 0.18f);
    if (!aim.IsValid() || aim.IsZero()) return false;
    const Vector3 endpoint = ClampDashEndpoint(player.Position(), aim);
    if (!SafeEndpoint(endpoint, reactive, Lethal(target))) return false;
    if (!ETrailStuns(player.Position(), endpoint, target.Position(), target.BoundingRadius()) && !reactive)
        return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    LastCastTick[2] = Now();
    ETrailActive = true;
    ETrailUntil = Now() + 1000;
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false, bool manual = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 120) || Protected(target) ||
        PreserveAttack(reactive) || player.Position().Distance2D(target.Position()) > kRRange + target.BoundingRadius())
        return false;
    const Vector3 aim = PredictPosition(target, 0.20f);
    if (!aim.IsValid() || aim.IsZero() || Engine::UnderEnemyTurret(aim) && !Engine::UnderEnemyTurret(player.Position()))
        return false;
    const ExecuteContext context{
        true, Engine::ValidEnemy(target), Lethal(target), IsRMarked(target),
        Engine::CountAlliesAt(target.Position(), 650.0f) > 0,
        Bool(RMenu, "ShareExecute", true), manual, Protected(target)};
    if (!ShouldExecuteR(context)) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    LastCastTick[3] = Now();
    LastRTargetId = static_cast<int>(target.NetworkId());
    return true;
}
inline bool TryGreyHealth(const AIHeroClient& target, Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !IsGreyHealthAvailable(player) || player.HealthPercent() >
        Slider(WMenu, "GreyHealthBelow", 72)) return false;
    const float missing = std::max(0.0f, player.MaxHealth() - player.Health());
    const float recoverable = GreyHealthRecovered(missing, missing);
    if (recoverable <= 0.0f) return false;
    const bool nearby = Engine::CountEnemiesAt(player.Position(), kWDetectionRadius) > 0;
    if (!ShouldRecoverGreyHealth(missing, missing,
        player.Position().Distance2D(target.Position()), nearby)) return false;
    return CastW(target, mode, true);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (TryGreyHealth(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(WMenu, "HarassMana", 45)) return;
    if (TryGreyHealth(target, Mode::Harass)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastE(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (TryGreyHealth(target, Mode::Flee)) return;
    if (CastW(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastE(target, Mode::Flee, true);
}
inline bool Automatic(const AIHeroClient& target) {
    if (IncomingThreatUntil > Now() && CastW(target, Mode::Automatic, true)) return true;
    if (IncomingHardCCUntil > Now() && Engine::ValidEnemy(target) && CastE(target, Mode::Automatic, true)) return true;
    return Engine::ValidEnemy(target) && Lethal(target) && CastR(target, Mode::Automatic, true);
}
inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    WActive = IsGhostwaterActive(player) || (WActive && WStealthUntil > now);
    QCharging = player.Spellbook().IsCharging() ||
        IsQCharging(player) || (QCharging && QChargeStartTick + 1600 > now);
    ETrailActive = ETrailActive && ETrailUntil > now;
    if (!WActive) WStealthUntil = 0;
    if (!QCharging) QChargeStartTick = 0;
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    if (ManualOverrideUntil > Now()) return true;
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, mode == Mode::Flee ? 850.0f : kRRange);
    if (mode == Mode::Automatic) {
        (void)Automatic(target);
        return true;
    }
    switch (mode) {
    case Mode::Combo:
        Combo(target);
        break;
    case Mode::Harass:
        Harass(target);
        break;
    case Mode::Flee:
        Flee(NearestEnemyToPlayer(target, 850.0f));
        break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (GameObjects::Player().ManaPercent() >= Slider(FarmMenu, "Mana", 35))
            (void)Engine::TryFarm(mode);
        break;
    default:
        break;
    }
    return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot < 0 || slot > 3) return;
        if (!Engine::WasControllerCast(slot)) ManualOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
        LastCastTick[slot] = now;
        if (slot == 0 && Engine::TextContains(args.SpellName, "charge")) {
            QCharging = true;
            QChargeStartTick = now;
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(
        IncomingHardCCUntil, std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "PykeW") || Engine::TextContains(args.BuffName, "pykew")) WActive = true;
    if (Engine::TextContains(args.BuffName, "PykeQ") || Engine::TextContains(args.BuffName, "pykeq")) {
        QCharging = true; QChargeStartTick = Now();
    }
    if (Engine::TextContains(args.BuffName, "PykeE")) ETrailActive = true;
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "PykeW") || Engine::TextContains(args.BuffName, "pykew")) WActive = false;
    if (Engine::TextContains(args.BuffName, "PykeQ") || Engine::TextContains(args.BuffName, "pykeq")) QCharging = false;
    if (Engine::TextContains(args.BuffName, "PykeE")) ETrailActive = false;
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQThrowRange, 0xFF7744CCu, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFCC4455u, 1.5f, 40);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("PykeOneTrick", "Pyke ambush tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Bone Skewer"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Ghostwater"));
    WMenu->Add(new MenuSlider("GreyHealthBelow", "Use grey-health route below HP%", 72, 20, 95));
    WMenu->Add(new MenuSlider("StealthHoldMs", "Expected camouflage hold (ms)", 2600, 500, 6000));
    WMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 45, 10, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Phantom Undertow"));
    EMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum endpoint enemies", 2, 1, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Death from Below"));
    RMenu->Add(new MenuBool("ShareExecute", "Spend lethal R when an ally can share", true));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("PykeFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("PykeCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and R ranges", false));
}
inline void OnLoad() {
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = ManualOverrideUntil = IncomingThreatUntil = IncomingHardCCUntil = 0;
    QChargeStartTick = ETrailUntil = WStealthUntil = LastRTargetId = 0;
    WActive = QCharging = ETrailActive = false;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    WActive = QCharging = ETrailActive = false;
}
inline constexpr const char* Scenarios[] = {
    "Pin all mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Preserve selected target before orbwalker and selector fallback",
    "Reconcile grey-health passive, Ghostwater camouflage and Q charging from buffs and polling",
    "Use grey-health recovery only when missing health exists and nearby detection is safe",
    "Throw Bone Skewer through a predicted line with collision and tap/charge range distinction",
    "Never replace an ordinary attack windup with a nonreactive Q or E",
    "Use Phantom Undertow only when the dash endpoint is valid, cursor-consistent and stun trail reaches target",
    "Reject E endpoints through walls, enemy turrets and excessive enemy count",
    "Use Ghostwater targeting only outside detection or for reactive escape",
    "Use Death from Below only against a live lethal target",
    "Respect configured ally share policy and marked execute state",
    "Never execute protected, invulnerable or spell-shielded targets",
    "Automatic mode is restricted to defense, hard crowd control or lethal execute",
    "Combo sequences Q, E, Ghostwater and R without taking movement ownership",
    "Harass preserves mana and avoids unsolicited lethal dives",
    "LaneClear, Jungle and LastHit delegate to shared farm policy",
    "Flee prioritizes grey-health safety, Ghostwater and defensive Undertow",
    "Yield after observed manual Q, W, E or R ownership",
    "Expose Q, E and R ranges without changing gameplay decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Pyke;
    controller.ControllerId = "champion.kuroaio.ai.pyke.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIPyke.md";
    controller.ImplementationSummary =
        "Grey-health-aware Ghostwater routing, charged Bone Skewer, safe Undertow trail and conservative execute/share Death from Below loop.";
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

    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Pyke
