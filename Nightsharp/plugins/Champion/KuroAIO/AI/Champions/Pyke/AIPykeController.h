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
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int QChargeStartTick = 0;
inline int QReleaseAttemptTick = 0;
inline int QRuntimeMissingSince = 0;
inline int QTargetId = 0;
inline int ETrailUntil = 0;
inline int WStealthUntil = 0;
inline int LastRTargetId = 0;
inline Vector3 QLastAim{};
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
inline bool RuntimeQCharging() {
    const auto player = GameObjects::Player();
    return Engine::RuntimeSpells[0] &&
        (Engine::RuntimeSpells[0]->IsCharging() ||
         (player.IsValid() &&
          (player.Spellbook().IsCharging() || IsQCharging(player))));
}
inline float QChargeElapsedSeconds() {
    return QChargeStartTick > 0
        ? static_cast<float>(std::max(0, Now() - QChargeStartTick)) / 1000.0f
        : 0.0f;
}
inline void ClearQState() {
    QChargeStartTick = 0;
    QReleaseAttemptTick = 0;
    QRuntimeMissingSince = 0;
    QTargetId = 0;
    QLastAim = {};
    QCharging = false;
}
inline bool IsRMarked(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) &&
        (target.HasBuff("PykeRExecute") || target.HasBuff("pykerexecute") ||
         target.HasBuff("PykeRMark"));
}
inline bool Lethal(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) && Engine::RuntimeSpells[3] &&
        Engine::RuntimeSpells[3]->GetDamage(target) >= target.Health();
}
inline bool SafeEndpoint(const Vector3& endpoint, bool defensive, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint)) return false;
    const bool safe = TurretEndpointSafe(
        Engine::UnderEnemyTurret(endpoint), Engine::UnderEnemyTurret(player.Position()),
        defensive || lethal, Engine::CountEnemiesAt(endpoint, 250.0f),
        Slider(EMenu, "MaxEndpointEnemies", 2));
    return safe;
}
inline bool BuildQAim(const AIHeroClient& target, float range,
                      Vector3& aim, Vector3& predictedUnit,
                      bool& collisionFree) {
    aim = {};
    predictedUnit = {};
    collisionFree = false;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::RuntimeSpells[0] ||
        !Engine::ValidEnemy(target, kQMaxRange + target.BoundingRadius())) {
        return false;
    }
    const auto prediction =
        Engine::RuntimeSpells[0]->GetPrediction(target, false, range);
    aim = prediction.GetCastPosition();
    predictedUnit = prediction.GetUnitPosition();
    if (!predictedUnit.IsValid() || predictedUnit.IsZero()) {
        predictedUnit = PredictPosition(target, 0.25f);
    }
    if (!aim.IsValid() || aim.IsZero()) aim = predictedUnit;
    collisionFree = prediction.CollisionObjects.empty();
    return aim.IsValid() && !aim.IsZero() &&
           predictedUnit.IsValid() && !predictedUnit.IsZero() &&
           prediction.Hitchance >= SDK::HitChance::High &&
           QLineHits(player.Position(), aim, predictedUnit,
                     target.BoundingRadius(), range) &&
           !ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth);
}
inline bool StartQ(const AIHeroClient& target, Mode mode,
                   bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || QCharging || RuntimeQCharging() ||
        !Engine::ValidEnemy(target, kQMaxRange + target.BoundingRadius()) ||
        !Ready(0, mode) || !Throttle(0) || Protected(target) ||
        PreserveAttack(reactive) || !Engine::RuntimeSpells[0]) {
        return false;
    }
    Vector3 aim{};
    Vector3 predicted{};
    bool collisionFree = false;
    if (!BuildQAim(target, kQMaxRange, aim, predicted, collisionFree) ||
        !collisionFree) {
        return false;
    }
    Engine::ArmControllerCast(0);
    if (!Engine::RuntimeSpells[0]->StartCharging(aim)) {
        Engine::CancelControllerCast(0);
        return false;
    }
    Engine::MarkSuccessfulCast(0);
    QCharging = true;
    QChargeStartTick = LastCastTick[0] = Now();
    QTargetId = static_cast<int>(target.NetworkId());
    QLastAim = aim;
    return true;
}
inline bool ReleaseQ(const AIHeroClient& fallback) {
    if (!QCharging || !Engine::RuntimeSpells[0] ||
        Now() - QReleaseAttemptTick < 25) {
        return false;
    }
    auto target = ControllerHelpers::HeroByNetworkId(QTargetId);
    if (!Engine::ValidEnemy(target, kQMaxRange + 100.0f)) target = fallback;
    const float elapsed = QChargeElapsedSeconds();
    const float range = QRangeFromCharge(elapsed);
    Vector3 aim = QLastAim;
    Vector3 predicted{};
    bool collisionFree = false;
    const bool predictionHits = Engine::ValidEnemy(target) &&
        BuildQAim(target, range, aim, predicted, collisionFree);
    const bool inCurrentRange = Engine::ValidEnemy(target) &&
        GameObjects::Player().Position().Distance2D(predicted) <=
            range + target.BoundingRadius();
    const QReleaseContext context{
        true, Engine::ValidEnemy(target), predictionHits, collisionFree,
        inCurrentRange, Bool(QMenu, "FullPullOnly", false), elapsed};
    if (!ShouldReleaseQ(context)) return false;
    if ((!aim.IsValid() || aim.IsZero()) &&
        QMustRelease(elapsed)) {
        aim = QLastAim.IsValid() && !QLastAim.IsZero()
            ? QLastAim : Game::CursorPos();
    }
    if (!aim.IsValid() || aim.IsZero()) return false;
    QLastAim = aim;
    QReleaseAttemptTick = Now();
    Engine::ArmControllerCast(0);
    if (!Engine::RuntimeSpells[0]->ShootChargedSpell(aim)) {
        Engine::CancelControllerCast(0);
        return false;
    }
    Engine::MarkSuccessfulCast(0);
    LastCastTick[0] = Now();
    LastRTargetId = Engine::ValidEnemy(target)
        ? static_cast<int>(target.NetworkId()) : 0;
    ClearQState();
    return true;
}
inline bool CastQ(const AIHeroClient& target, Mode mode,
                  bool reactive = false) {
    return StartQ(target, mode, reactive);
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || QCharging || RuntimeQCharging() ||
        !Ready(1, mode) || !Throttle(1) || PreserveAttack(reactive)) {
        return false;
    }
    const bool enemyValid = Engine::ValidEnemy(target, kWChaseRange);
    const float distance = enemyValid
        ? player.Position().Distance2D(target.Position()) : 9999.0f;
    const bool attackedRecently = LastAutoTick > 0 && Now() - LastAutoTick < 1100;
    if ((!reactive &&
         distance < static_cast<float>(Slider(WMenu, "EngageDistance", 1000))) ||
        !WStealthTargetAllowed(distance, attackedRecently,
                               enemyValid || reactive)) {
        return false;
    }
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    WActive = true;
    WStealthUntil = Now() + Slider(WMenu, "StealthHoldMs", 2600);
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || QCharging || RuntimeQCharging() ||
        !Ready(2, mode) || !Throttle(2) || Protected(target) ||
        PreserveAttack(reactive)) {
        return false;
    }
    const Vector3 dashAim = PredictPosition(target, 0.18f);
    const Vector3 returnTarget = PredictPosition(target, kEReturnDelay);
    if (!dashAim.IsValid() || dashAim.IsZero() ||
        !returnTarget.IsValid() || returnTarget.IsZero()) {
        return false;
    }
    const Vector3 endpoint = ClampDashEndpoint(player.Position(), dashAim);
    if (!SafeEndpoint(endpoint, reactive, Lethal(target)) ||
        (!reactive &&
         (!ETrailStuns(player.Position(), endpoint, returnTarget,
                       target.BoundingRadius()) ||
          !EFollowupReachable(endpoint, returnTarget)))) {
        return false;
    }
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    LastCastTick[2] = Now();
    ETrailActive = true;
    ETrailUntil = Now() + 1000;
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 120) || Protected(target) ||
        PreserveAttack(reactive) || player.Position().Distance2D(target.Position()) > kRRange + target.BoundingRadius())
        return false;
    const auto prediction = Engine::RuntimeSpells[3]->GetPrediction(target);
    const Vector3 aim = prediction.GetCastPosition();
    if (prediction.Hitchance < SDK::HitChance::High ||
        !aim.IsValid() || aim.IsZero() ||
        (Engine::UnderEnemyTurret(aim) &&
         !Engine::UnderEnemyTurret(player.Position()))) {
        return false;
    }
    const ExecuteContext context{
        true, Engine::ValidEnemy(target), Lethal(target), IsRMarked(target),
        Engine::CountAlliesAt(target.Position(), 650.0f) > 0,
        Bool(RMenu, "ShareExecute", true), Protected(target)};
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
    if (CastE(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
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
inline bool TryAutoExecute() {
    if (!Bool(RMenu, "AutoExecute", true)) return false;
    AIHeroClient best{};
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kRRange) || !Lethal(enemy) ||
            Protected(enemy)) {
            continue;
        }
        if (!best.IsValid() || enemy.Health() < best.Health()) best = enemy;
    }
    return best.IsValid() &&
        CastR(best, Mode::Automatic, true);
}
inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    WActive = IsGhostwaterActive(player) || (WActive && WStealthUntil > now);
    const bool runtimeCharging = RuntimeQCharging();
    if (runtimeCharging) {
        QCharging = true;
        QRuntimeMissingSince = 0;
        if (QChargeStartTick == 0) QChargeStartTick = now;
    } else if (QCharging) {
        if (QRuntimeMissingSince == 0) QRuntimeMissingSince = now;
        if (now - QChargeStartTick > 180 &&
            now - QRuntimeMissingSince > 120) {
            ClearQState();
        }
    }
    ETrailActive = ETrailActive && ETrailUntil > now;
    if (!WActive) WStealthUntil = 0;
}
inline AIHeroClient AutonomousTarget(float range) {
    const auto orbwalker = ControllerHelpers::OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orbwalker, range)) return orbwalker;
    return Engine::SelectTarget(range);
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    LastMode = mode;
    ReconcileState();
    const AIHeroClient target = AutonomousTarget(
        mode == Mode::Flee ? 850.0f : kWChaseRange);
    if (QCharging || RuntimeQCharging()) {
        (void)ReleaseQ(target);
        return true;
    }
    if (TryAutoExecute()) return true;
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
        LastCastTick[slot] = now;
        if (slot == 0) {
            if (RuntimeQCharging()) {
                QCharging = true;
                if (QChargeStartTick == 0) QChargeStartTick = now;
            } else if (!QCharging) {
                QCharging = true;
                QChargeStartTick = now;
            } else if (now - QChargeStartTick > 100) {
                ClearQState();
            }
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
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "PykeW") ||
        Engine::TextContains(args.BuffName, "pykew")) {
        WActive = true;
    }
    if (Engine::TextContains(args.BuffName, "PykeQ") ||
        Engine::TextContains(args.BuffName, "pykeq")) {
        QCharging = true;
        if (QChargeStartTick == 0) QChargeStartTick = Now();
    }
    if (Engine::TextContains(args.BuffName, "PykeE")) ETrailActive = true;
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "PykeW") ||
        Engine::TextContains(args.BuffName, "pykew")) {
        WActive = false;
    }
    if (Engine::TextContains(args.BuffName, "PykeQ") ||
        Engine::TextContains(args.BuffName, "pykeq")) {
        if (!RuntimeQCharging()) ClearQState();
    }
    if (Engine::TextContains(args.BuffName, "PykeE")) ETrailActive = false;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (QCharging || RuntimeQCharging()) {
        args.Process = false;
        return;
    }
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQMaxRange, 0xFF7744CCu, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFCC4455u, 1.5f, 40);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("PykeOneTrick", "Pyke ambush tactics"));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Bone Skewer"));
    QMenu->Add(new MenuBool(
        "FullPullOnly", "Charge Q fully before pulling", false));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Ghostwater"));
    WMenu->Add(new MenuSlider("GreyHealthBelow", "Use grey-health route below HP%", 72, 20, 95));
    WMenu->Add(new MenuSlider("StealthHoldMs", "Expected camouflage hold (ms)", 2600, 500, 6000));
    WMenu->Add(new MenuSlider("EngageDistance", "Use W beyond this distance", 1000, 600, 1800));
    WMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 45, 10, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Phantom Undertow"));
    EMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum endpoint enemies", 2, 1, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Death from Below"));
    RMenu->Add(new MenuBool("AutoExecute", "Auto-execute killable enemies", true));
    RMenu->Add(new MenuBool("ShareExecute", "Share lethal R with allies", true));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("PykeFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("PykeCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and R ranges", false));
}
inline void OnLoad() {
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = IncomingThreatUntil =
        IncomingHardCCUntil = 0;
    ETrailUntil = WStealthUntil = LastRTargetId = 0;
    ClearQState();
    WActive = ETrailActive = false;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    ClearQState();
    WActive = ETrailActive = false;
}
inline constexpr const char* Scenarios[] = {
    "Pin all mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Use autonomous orbwalker and engine target policy",
    "Reconcile grey-health passive, Ghostwater camouflage and Q charging from local buffs and runtime state",
    "Start Bone Skewer as a real charged spell and release it once predicted range reaches the target",
    "Force-release Bone Skewer at the maximum hold time so charging Q never remains stuck",
    "Block attacks, Ghostwater and Undertow while Bone Skewer is charging",
    "Use the current 400-to-1100 linear Bone Skewer range with collision and projectile-wall checks",
    "Never replace an ordinary attack windup with a nonreactive Q or E",
    "Predict the returning Undertow trail and reject dashes that leave no reachable follow-up",
    "Reject E endpoints through walls, enemy turrets and excessive enemy count",
    "Use Ghostwater for distant approaches or reactive escape, not inside immediate engage range",
    "Automatically scan for lethal Death from Below targets in every orbwalker mode",
    "Use Death from Below only against a live lethal target",
    "Respect configured ally share policy and marked execute state",
    "Never execute protected, invulnerable or spell-shielded targets",
    "LaneClear, Jungle and LastHit delegate to shared farm policy",
    "Flee prioritizes grey-health safety, Ghostwater and defensive Undertow",
    "Reconcile observed Q, W, E and R events",
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
    controller.OnBeforeAttack = &OnBeforeAttack;

    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Pyke
