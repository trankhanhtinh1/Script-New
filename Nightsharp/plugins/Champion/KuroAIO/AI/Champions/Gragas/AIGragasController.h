#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIGragasGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Gragas {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::PlayerManaPercent;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::CursorDirectionAgrees;

inline Menu* TacticsMenu = nullptr;
inline Menu* BarrelMenu = nullptr;
inline Menu* SlamMenu = nullptr;
inline Menu* CaskMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline BarrelState Barrel{};
inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int PassiveCooldownUntil = 0;
inline int WEmpoweredUntil = 0;
inline int PlayerOverrideUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
        (reactive || Engine::ModeEnabled(Engine::ResolvedSpecs[slot], mode));
}
inline bool Throttle(int slot, int delay = 80) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
using ControllerHelpers::Protected;
using ControllerHelpers::PreserveAttack;
inline bool HasManaFor(int slot, float reserve = 0.0f) {
    return ControllerHelpers::CurrentResource() + 0.5f >=
        SpellCost(slot) + std::max(0.0f, reserve);
}
inline bool PassiveReadyNow() {
    return Now() >= PassiveCooldownUntil;
}
inline void ObservePassiveProc() {
    const auto player = GameObjects::Player();
    const int level = player.IsValid() ? player.Level() : 1;
    PassiveCooldownUntil = Now() + PassiveCooldownMs(level);
}
inline bool TurretSafe(const Vector3& endpoint, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !endpoint.IsValid() || endpoint.IsZero() ||
        SDK::NavMesh::IsWall(endpoint)) return false;
    return !Engine::UnderEnemyTurret(endpoint) ||
        Engine::UnderEnemyTurret(player.Position()) || lethal;
}
inline bool SafeCaskAim(const Vector3& aim, const AIHeroClient& target,
                        bool defensive, bool lethal) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !aim.IsValid() || aim.IsZero() ||
        player.Position().Distance2D(aim) > kRRange + target.BoundingRadius() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 45.0f)) return false;
    if (!defensive && !lethal && Engine::UnderEnemyTurret(aim) &&
        !Engine::UnderEnemyTurret(player.Position())) return false;
    return true;
}
inline float QDamage(const AIHeroClient& target, float multiplier = 1.0f) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target,
            QRawDamage(SpellRank(0), player.AP(), multiplier)) : 0.0f;
}
inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target,
            WRawDamage(SpellRank(1), target.MaxHealth(), player.AP())) : 0.0f;
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target,
            ERawDamage(SpellRank(2), player.AP())) : 0.0f;
}
inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target,
            RRawDamage(SpellRank(3), player.AP())) : 0.0f;
}
using ControllerHelpers::Lethal;
inline bool CastQ(const AIHeroClient& target, Mode mode,
                  bool reactive = false, bool forceRelease = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || PreserveAttack(reactive) ||
        !Ready(0, mode, reactive) || !Throttle(0)) return false;
    const int now = Now();
    if (Barrel.Active) {
        if (!forceRelease && !BarrelCanDetonate(Barrel, now)) return false;
        if (!Engine::ControllerCastPosition(0, Barrel.Position)) return false;
        Barrel = {};
        LastCastTick[0] = now;
        ObservePassiveProc();
        return true;
    }
    const Vector3 aim = PredictPosition(target, kQDelay);
    if (!aim.IsValid() || aim.IsZero() ||
        player.Position().Distance2D(aim) > kQRange + target.BoundingRadius() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 35.0f)) return false;
    const Vector3 barrelPos = ClampBarrel(player.Position(), aim);
    if (!Engine::ControllerCastPosition(0, barrelPos)) return false;
    RecordBarrel(Barrel, barrelPos, now);
    LastCastTick[0] = now;
    ObservePassiveProc();
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode,
                  bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PreserveAttack(reactive) ||
        !Ready(1, mode, reactive) || !Throttle(1) ||
        !HasManaFor(1, SpellCost(3) * 0.18f)) return false;
    if (Engine::ValidEnemy(target) &&
        player.Position().Distance2D(target.Position()) >
            player.AttackRange() + target.BoundingRadius() + 85.0f) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    WEmpoweredUntil = Now() + 550;
    LastCastTick[1] = Now();
    ObservePassiveProc();
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode,
                  bool reactive = false, bool lethal = false,
                  bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || PreserveAttack(reactive) ||
        !Ready(2, mode, reactive) || !Throttle(2) || !HasManaFor(2)) return false;
    const Vector3 aim = PredictPosition(target, 0.20f);
    const Vector3 endpoint = ClampDash(player.Position(), aim);
    if (!aim.IsValid() || aim.IsZero() || endpoint.IsZero()) return false;
    if (!reactive && !defensive && mode != Mode::Combo &&
        !CursorDirectionAgrees(aim, -0.20f)) return false;
    const bool collision = BodySlamHits(player.Position(), endpoint,
                                        target.Position(), target.BoundingRadius());
    const bool endpointTurret = Engine::UnderEnemyTurret(endpoint) &&
        !Engine::UnderEnemyTurret(player.Position());
    const bool safe = SafeDashEndpoint(endpoint, SDK::NavMesh::IsWall(endpoint),
        endpointTurret, Engine::UnderEnemyTurret(player.Position()),
        Engine::CountEnemiesAt(endpoint, 250.0f),
        Slider(SlamMenu, "MaximumEnemies", 2), lethal);
    BodySlamContext context{true, collision, safe, endpointTurret,
        Engine::CountEnemiesAt(endpoint, 250.0f),
        Slider(SlamMenu, "MaximumEnemies", 2), defensive, lethal};
    if (!ShouldBodySlam(context)) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastCastTick[2] = Now();
    ObservePassiveProc();
    return true;
}
inline int PredictedCaskHits(const Vector3& aim) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    int hits = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && CaskHits(aim,
            PredictPosition(enemy, kRDelay), enemy.BoundingRadius())) ++hits;
    }
    return hits;
}
inline bool CastR(const AIHeroClient& target, Mode mode,
                  bool reactive = false, bool manual = false,
                  bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(3, mode, reactive) ||
        !Throttle(3, 120) || !HasManaFor(3) || PreserveAttack(reactive)) return false;
    const bool lethal = Lethal(target, RDamage(target));
    const Vector3 aim = defensive
        ? player.Position() + Direction2D(target.Position(), player.Position()) * 250.0f
        : PredictPosition(target, kRDelay);
    if (!SafeCaskAim(aim, target, defensive, lethal)) return false;
    if (!manual && !defensive && mode != Mode::Combo &&
        !CursorDirectionAgrees(aim, -0.20f)) return false;
    CaskContext context{true, Engine::ValidEnemy(target), true,
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 45.0f),
        lethal, defensive, manual, PredictedCaskHits(aim),
        Slider(CaskMenu, "MinimumTargets", 2)};
    if (!ShouldCastCask(context)) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    LastCastTick[3] = Now();
    ObservePassiveProc();
    return true;
}
inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, RDamage(target)) && CastR(target, mode, true, false, false)) return true;
    if (Lethal(target, EDamage(target)) && CastE(target, mode, true, true, false)) return true;
    if (Lethal(target, QDamage(target, kQMaxDamageMultiplier)) &&
        CastQ(target, mode, true, true)) return true;
    return Lethal(target, WDamage(target)) && CastW(target, mode, true);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (Barrel.Active && BarrelCanDetonate(Barrel, Now()) &&
        BarrelHits(Barrel.Position, PredictPosition(target, 0.15f), target.BoundingRadius()) &&
        CastQ(target, Mode::Combo, false, true)) return;
    if (!Barrel.Active && CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || PlayerManaPercent() < Slider(BarrelMenu, "HarassMana", 55)) return;
    if (Barrel.Active && BarrelCanDetonate(Barrel, Now()) && CastQ(target, Mode::Harass, false, true)) return;
    if (!Barrel.Active && CastQ(target, Mode::Harass)) return;
    if (InAutoAttackRange(target) && CastW(target, Mode::Harass)) (void)CastE(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& threat) {
    if (!Engine::ValidEnemy(threat)) return;
    (void)CastR(threat, Mode::Flee, true, false, true);
    (void)CastE(threat, Mode::Flee, true, false, true);
}
inline void ReconcileState() {
    const int now = Now();
    ExpireBarrel(Barrel, now);
    if (WEmpoweredUntil <= now) WEmpoweredUntil = 0;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if ((player.HasBuff("GragasQ") || player.HasBuff("gragasq")) &&
        Barrel.Position.IsValid() && !Barrel.Position.IsZero()) {
        Barrel.Active = true;
    }
    if (player.HasBuff("GragasW") || player.HasBuff("gragasw")) WEmpoweredUntil = std::max(WEmpoweredUntil, now + 250);
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    const auto target = ControllerHelpers::PreferredEnemyTarget(selected, kRRange);
    if (PlayerOverrideUntil > Now()) return true;
    if (IncomingThreatUntil > Now() && Engine::ValidEnemy(target)) {
        if (CastE(target, mode, true, false, true)) return true;
        if (CastR(target, mode, true, false, true)) return true;
    }
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 950.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (PlayerManaPercent() >= Slider(FarmMenu, "Mana", 38))
            (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
        if (AutomaticAllowed({IncomingThreatUntil > Now(), IncomingHardCCUntil > Now(),
            Engine::ValidEnemy(target) && Lethal(target, RDamage(target)),
            false, PlayerOverrideUntil > Now()})) {
            (void)CastR(target, mode, true, false, IncomingThreatUntil > Now());
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
        LastCastTick[slot] = now;
        if (slot == 0 && !Barrel.Active) {
            const Vector3 observed = args.EndPosition.IsValid() &&
                !args.EndPosition.IsZero() ? args.EndPosition : args.CastPosition;
            if (observed.IsValid() && !observed.IsZero())
                RecordBarrel(Barrel, observed, now);
        }
        if (slot == 1) WEmpoweredUntil = now + 550;
        ObservePassiveProc();
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(
        IncomingHardCCUntil, std::max(analysis.CommitmentUntilTick,
                                      analysis.LineThreatUntilTick));
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if ((Engine::TextContains(args.BuffName, "GragasQ") ||
         Engine::TextContains(args.BuffName, "gragasq")) &&
        Barrel.Position.IsValid() && !Barrel.Position.IsZero()) {
        Barrel.Active = true;
    }
    if (Engine::TextContains(args.BuffName, "GragasW") ||
        Engine::TextContains(args.BuffName, "gragasw")) WEmpoweredUntil = Now() + 550;
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "GragasQ") ||
        Engine::TextContains(args.BuffName, "gragasq")) Barrel = {};
    if (Engine::TextContains(args.BuffName, "GragasW") ||
        Engine::TextContains(args.BuffName, "gragasw")) WEmpoweredUntil = 0;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || (LastMode != Mode::Combo && LastMode != Mode::Harass)) return;
    const AIHeroClient target(args.Target.Handle());
    if (Engine::ValidEnemy(target) && WEmpoweredUntil <= Now())
        (void)CastW(target, LastMode, true);
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFCC8844u, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kERange, 0xFF8844CCu, 1.0f, 36);
    if (Barrel.Active) Drawing::DrawCircle(Barrel.Position, kQRadius, 0xFFFFAA55u, 1.5f, 36);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("GragasMechanics", "Gragas barrel tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 560, 180, 1200));
    BarrelMenu = TacticsMenu->AddSubMenu(new Menu("Barrel", "Barrel roll charge/release"));
    BarrelMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 55, 10, 90));
    SlamMenu = TacticsMenu->AddSubMenu(new Menu("BodySlam", "Body Slam collision safety"));
    SlamMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies at dash endpoint", 2, 0, 5));
    CaskMenu = TacticsMenu->AddSubMenu(new Menu("Cask", "Explosive Cask displacement"));
    CaskMenu->Add(new MenuSlider("MinimumTargets", "Minimum cask targets", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Farm", "Farm resource reserve"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 38, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("Coach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw barrel and dash ranges", false));
}
inline void OnLoad() {
    Barrel = {};
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = PassiveCooldownUntil = WEmpoweredUntil = 0;
    PlayerOverrideUntil = IncomingThreatUntil = IncomingHardCCUntil = 0;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = BarrelMenu = SlamMenu = CaskMenu = FarmMenu = CoachMenu = nullptr;
    Barrel = {};
}
inline constexpr const char* Scenarios[] = {
    "Pin all spell and passive arithmetic to Riot 26.15 and CommunityDragon 16.15",
    "Track the Q barrel position, arm delay and four-second expiry from events and polling",
    "Place Q inside the observed 850 range and release only after the arm delay",
    "Charge Q for a high-value area hit, then release early for a fleeing target",
    "Use W before a real attack and preserve the empowered hit through the windup",
    "Track Happy Hour's five-point-five percent max-health heal and level-scaled 12/10/8/6-second cooldown",
    "Reserve enough live mana for the next defensive Body Slam or Cask",
    "Require Body Slam collision prediction before spending the dash",
    "Clamp Body Slam endpoint to 600 and reject walls and unsafe enemy-turret endpoints",
    "Reject Body Slam endpoints with excessive enemy density unless the hit is lethal or defensive",
    "Use Explosive Cask displacement to peel divers away from the player",
    "Use Cask for lethal kill-secure and reject blocked or uncertain projectile paths",
    "Require configured multi-target value before a nonlethal offensive Cask",
    "Preserve selected target before orbwalker and selector fallback",
    "Preserve AA windup unless the cast is reactive or kill-secure",
    "Yield after manual Q W E or R ownership and reconcile manual state",
    "Combo branches Q setup, W weave, collision E and cask displacement",
    "Harass uses Q/W while respecting the configured mana floor and never opens R casually",
    "LaneClear Jungle and LastHit delegate to shared farm policy",
    "Flee uses defensive Cask peel and only safe Body Slam",
    "Automatic mode permits only defense, interrupt or lethal Cask",
    "Reject invulnerable, spell-shielded and invalid targets",
    "Never automate movement, items or summoner spells",
    "Draw observed barrel and spell ranges without changing gameplay decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Gragas;
    controller.ControllerId = "champion.kuroaio.ai.gragas.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIGragas.md";
    controller.ImplementationSummary =
        "Charge-aware barrel release, W passive-heal/resource reconciliation, collision-gated Body Slam and conservative Cask displacement/execute policy.";
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
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffEvent<OnBuffAdd>;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Gragas
