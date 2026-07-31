#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIGalioGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Galio {

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

inline bool WCharging = false;
inline int WStartTick = 0;
inline int WExpireTick = 0;
inline int PassiveReadyTick = 0;
inline int LastCastTick[4]{};
inline int PlayerOverrideUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastRAllyId = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline Vector3 LastQAim{};
inline Vector3 LastEAim{};
inline Vector3 LastRLanding{};
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
inline bool Throttle(int slot, int delay = 90) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
using ControllerHelpers::Protected;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::AP;
inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, QRawDamage(SpellRank(0), AP())) : 0.0f;
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, ERawDamage(SpellRank(2), AP())) : 0.0f;
}
using ControllerHelpers::Lethal;
inline bool PassiveWeaveWouldBeLost(const AIHeroClient& target, bool reactive) {
    const auto player = GameObjects::Player();
    return !reactive && PassiveReady(Now(), PassiveReadyTick) &&
        player.IsValid() && Engine::ValidEnemy(target) &&
        player.Position().Distance2D(target.Position()) <=
            player.AttackRange() + target.BoundingRadius() + 25.0f;
}
inline Vector3 QAim(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return {};
    Vector3 aim = PredictPosition(target, kQDelay);
    if (Engine::RuntimeSpells[0]) {
        const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
        if (prediction.Hitchance >= SDK::HitChance::High &&
            prediction.GetCastPosition().IsValid()) aim = prediction.GetCastPosition();
    }
    return aim;
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0, mode) || !Throttle(0) || Protected(target) ||
        PreserveAttack(reactive) || PassiveWeaveWouldBeLost(target, reactive)) return false;
    const Vector3 aim = QAim(target);
    if (!aim.IsValid() || player.Position().Distance2D(aim) >
        kQRange + target.BoundingRadius() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastQAim = aim;
    LastCastTick[0] = Now();
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool interrupt = false, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1, 100) ||
        PreserveAttack(reactive, interrupt || WCharging)) return false;
    if (WCharging) {
        const Vector3 predicted = Engine::ValidEnemy(target)
            ? PredictPosition(target, 0.10f) : player.Position();
        const float charge = std::clamp(static_cast<float>(Now() - WStartTick) /
            (kWMaxCharge * 1000.0f), 0.0f, 1.0f);
        const WContext context{true, true, Engine::ValidEnemy(target),
            Engine::ValidEnemy(target) && CircleHits(player.Position(), predicted,
                kWRadius, target.BoundingRadius()), Orbwalker::IsWindingUp(),
            reactive, interrupt, lethal, false, charge};
        if (!ShouldReleaseW(context)) return false;
        if (!Engine::ControllerCastSelf(1)) return false;
        WCharging = false;
        WExpireTick = 0;
        LastCastTick[1] = Now();
        return true;
    }
    if (!Engine::ValidEnemy(target) || Protected(target)) return false;
    const Vector3 predicted = PredictPosition(target, 0.25f);
    if (!CircleHits(player.Position(), predicted, kWRadius, target.BoundingRadius())) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    WCharging = true;
    WStartTick = Now();
    WExpireTick = WStartTick + static_cast<int>(kWMaxCharge * 1000.0f);
    LastCastTick[1] = WStartTick;
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || !Throttle(2) || Protected(target) ||
        PreserveAttack(reactive)) return false;
    const Vector3 requested = PredictPosition(target, 0.20f);
    const Vector3 endpoint = ClampRange(player.Position(), requested, kERange);
    if (endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(requested, kEWidth * 0.5f)) return false;
    const bool defensive = reactive || IncomingHardCCUntil > Now();
    if (!SafeEEndpoint(endpoint, Engine::UnderEnemyTurret(endpoint) &&
        !Engine::UnderEnemyTurret(player.Position()), false,
        Engine::CountEnemiesAt(endpoint, 300.0f),
        Engine::CountAlliesAt(endpoint, 500.0f), Slider(EMenu, "MaxEndpointEnemies", 3),
        defensive, lethal)) return false;
    if (!Engine::ControllerCastPosition(2, requested)) return false;
    LastEAim = requested;
    LastCastTick[2] = Now();
    return true;
}
inline AIHeroClient BestAllyForR(bool defensive, Vector3* landing = nullptr) {
    AIHeroClient best{};
    float bestScore = -1000000.0f;
    const auto player = GameObjects::Player();
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally, kRRange) || !player.IsValid() ||
            ally.NetworkId() == player.NetworkId()) continue;
        const Vector3 predicted = PredictPosition(ally, kRDelay);
        if (!predicted.IsValid() || predicted.IsZero() || SDK::NavMesh::IsWall(predicted)) continue;
        const int enemies = Engine::CountEnemiesAt(predicted, kRRadius);
        const int allies = Engine::CountAlliesAt(predicted, kRRadius);
        const bool threatened = enemies > 0 && (ally.HealthPercent() < 70.0f || enemies >= allies);
        if (defensive && !threatened) continue;
        const bool safe = SafeRLanding({true, true, threatened, true,
            !SDK::NavMesh::IsWall(predicted),
            Engine::UnderEnemyTurret(predicted) && !Engine::UnderEnemyTurret(player.Position()),
            false, defensive, false, enemies, allies, Slider(RMenu, "MaxLandingEnemies", 3)});
        if (!safe) continue;
        const float score = (threatened ? 900.0f : 0.0f) +
            (100.0f - ally.HealthPercent()) * 8.0f +
            static_cast<float>(allies) * 80.0f - static_cast<float>(enemies) * 120.0f -
            predicted.Distance2D(Game::CursorPos()) * 0.05f;
        if (score > bestScore) {
            best = ally;
            bestScore = score;
            if (landing) *landing = predicted;
        }
    }
    return best;
}
inline bool CastR(Mode mode, bool reactive = false, bool manual = false) {
    if (!Ready(3, mode) || !Throttle(3, 120)) return false;
    Vector3 landing{};
    const AIHeroClient ally = BestAllyForR(reactive, &landing);
    if (!ally.IsValid()) return false;
    const bool threatened = Engine::CountEnemiesAt(landing, kRRadius) > 0;
    const RLandingContext context{true, true, threatened, landing.IsValid(),
        !SDK::NavMesh::IsWall(landing),
        Engine::UnderEnemyTurret(landing) && !Engine::UnderEnemyTurret(GameObjects::Player().Position()),
        manual, reactive, false, Engine::CountEnemiesAt(landing, kRRadius),
        Engine::CountAlliesAt(landing, kRRadius), Slider(RMenu, "MaxLandingEnemies", 3)};
    if (!SafeRLanding(context) || !Engine::ControllerCastUnit(3, ally)) return false;
    LastRAllyId = static_cast<int>(ally.NetworkId());
    LastRLanding = landing;
    LastCastTick[3] = Now();
    return true;
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo, false, Lethal(target, EDamage(target)))) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastR(Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(WMenu, "HarassMana", 52)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) && CastW(target, Mode::Flee, true, true)) return;
    if (Engine::ValidEnemy(target) && CastE(target, Mode::Flee, true)) return;
    (void)CastR(Mode::Flee, true, true);
}
inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, EDamage(target)) && CastE(target, mode, true, true)) return true;
    return Lethal(target, QDamage(target)) && CastQ(target, mode, true);
}
inline void ReconcileState() {
    const auto player = GameObjects::Player();
    const int now = Now();
    if (!player.IsValid()) return;
    const bool buff = player.HasBuff("GalioW") || player.HasBuff("GalioWChannel") ||
        player.HasBuff("GalioWActive");
    if (buff) {
        WCharging = true;
        if (WStartTick <= 0) WStartTick = now;
        WExpireTick = WStartTick + static_cast<int>(kWMaxCharge * 1000.0f);
    } else if (WCharging && now > WExpireTick + 250) {
        WCharging = false;
        WStartTick = WExpireTick = 0;
    }
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, mode == Mode::Flee ? 1000.0f : kQRange);
    if (PlayerOverrideUntil > Now()) return true;
    const bool defensive = IncomingThreatUntil > Now() ||
        (GameObjects::Player().IsValid() && GameObjects::Player().HealthPercent() <=
            Slider(TacticsMenu, "DefensiveHealth", 38));
    if (defensive && Engine::ValidEnemy(target) && CastW(target, mode, true, true)) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1000.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (GameObjects::Player().IsValid() && GameObjects::Player().ManaPercent() >=
            Slider(FarmMenu, "Mana", 35)) (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
        if (AutomaticAllowed({defensive, IncomingHardCCUntil > Now(),
            Engine::ValidEnemy(target) && Lethal(target, QDamage(target)), false,
            PlayerOverrideUntil > Now()})) {
            if (defensive && Engine::ValidEnemy(target) && CastW(target, mode, true, true)) return true;
            (void)CastR(mode, true);
        }
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
        if (!Engine::WasControllerCast(slot)) PlayerOverrideUntil = now +
            Slider(TacticsMenu, "ManualOwnershipMs", 560);
        if (slot == 1) {
            if (!WCharging) {
                WCharging = true;
                WStartTick = now;
            } else if (now - WStartTick > 150) {
                WCharging = false;
                WStartTick = WExpireTick = 0;
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
    if (Engine::TextContains(args.BuffName, "GalioW")) {
        WCharging = true;
        if (WStartTick <= 0) WStartTick = Now();
        WExpireTick = WStartTick + static_cast<int>(kWMaxCharge * 1000.0f);
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "GalioW")) {
        WCharging = false;
        WStartTick = WExpireTick = 0;
    }
}
inline void OnInterruptable(
    const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    ControllerHelpers::CaptureInterruptable(args, InterruptTargetId,
        InterruptExpireTick, 900, 300, 5000);
    IncomingHardCCUntil = std::max(IncomingHardCCUntil, InterruptExpireTick);
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
    if (args.Target.IsValid() && PassiveReady(Now(), PassiveReadyTick))
        PassiveReadyTick = PassiveResetTick(Now());
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    const int slot = static_cast<int>(args.Slot);
    if (slot >= 0 && slot <= 2 && PassiveReadyTick > Now())
        PassiveReadyTick = std::max(Now(), PassiveReadyTick - 4000);
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFB77BFFu, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kWRadius, 0xFFB77BFFu, 1.2f, 32);
    if (!LastRLanding.IsZero() && Now() - LastCastTick[3] < 1800)
        Drawing::DrawCircle(LastRLanding, kRRadius, 0xFF66DDFFu, 1.8f, 40);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("GalioOneTrick", "Galio vanguard tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    TacticsMenu->Add(new MenuSlider("DefensiveHealth", "Defensive health percent", 38, 10, 70));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Winds of War"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Shield of Durand"));
    WMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 52, 10, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Justice Punch"));
    EMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum endpoint enemies", 3, 1, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Hero's Entrance"));
    RMenu->Add(new MenuSlider("MaxLandingEnemies", "Maximum R landing enemies", 3, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("GalioFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("GalioCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/W/R safety ranges", false));
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    WCharging = false;
    WStartTick = WExpireTick = PassiveReadyTick = PlayerOverrideUntil = 0;
    IncomingThreatUntil = IncomingHardCCUntil = InterruptTargetId = InterruptExpireTick = 0;
    LastAutoTargetId = LastAutoTick = LastRAllyId = 0;
    LastQAim = LastEAim = LastRLanding = {};
    LastMode = Mode::None;
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
}
inline void OnLoad() {
    WCharging = false;
    WStartTick = WExpireTick = PassiveReadyTick = PlayerOverrideUntil = 0;
    IncomingThreatUntil = IncomingHardCCUntil = InterruptTargetId = InterruptExpireTick = 0;
    LastAutoTargetId = LastAutoTick = LastRAllyId = 0;
    LastQAim = LastEAim = LastRLanding = {};
    LastMode = Mode::None;
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
}
inline constexpr const char* Scenarios[] = {
    "Pin every value and behavior to Riot 26.15 / CommunityDragon 16.15",
    "Reset passive cooldown after observed Q or E spell ownership and preserve an available empowered attack",
    "Use Q's 825 range, 0.25 second delay, projectile prediction and wall rejection",
    "Use Q gusts for combo, harass, lane clear, jungle and last-hit policies",
    "Start W as a channel and reconcile GalioW buff state through events and polling",
    "Release W only with a valid predicted taunt contact or a defensive/interruption emergency",
    "Scale W release value by observed charge duration without inventing hidden telemetry",
    "Use E prediction, dash range, knockup corridor, safe endpoint and turret rejection",
    "Allow E to peel incoming hard crowd control and to secure a verified lethal target",
    "Select R allies by threat, health, predicted landing, follow-up and enemy count",
    "Reject R landings in walls, fresh enemy turrets or over-committed enemy numbers",
    "Use R for defensive rescue and interrupt response; ordinary engage remains conservative",
    "Preserve selected target before orbwalker and selector fallback",
    "Preserve AA windup unless W release, interruption or lethal response justifies commitment",
    "Reconcile manual Q W E R ownership and yield before synthetic casts",
    "Reconcile enemy process-spell threats for automatic defensive and interrupt rules",
    "Automatic mode permits only defense, interrupt or kill secure, never fresh engage",
    "Flee uses W taunt peel, E disengage and manual-assist R ally landing",
    "LaneClear Jungle and LastHit delegate to shared farm policy after mana reserve",
    "Reject protected, invulnerable and spell-shielded targets",
    "Draw range and last R landing without changing gameplay decisions",
    "Never automate items, summoner spells, flash or movement ownership",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Galio";
    controller.ControllerId = "champion.kuroaio.ai.galio.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIGalio.md";
    controller.ImplementationSummary =
        "Passive-reset-aware Q prediction, W charge/release taunt, safe E knockup and ally-threat Hero's Entrance controller.";
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
    controller.OnInterruptable = &OnInterruptable;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffEvent<OnBuffAdd>;
    controller.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnDoCast = &OnDoCast;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Galio
