#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AISwainGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Swain {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline SoulState Souls{};
inline TetherState Tether{};
inline DemonState Demon{};
inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastInterruptTargetId = 0;
inline int LastInterruptExpireTick = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline Vector3 LastQAim{};
inline Vector3 LastWAim{};
inline Vector3 LastEAim{};
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
using ControllerHelpers::Protected;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::AP;
using ControllerHelpers::Lethal;

inline bool Throttle(int slot, int delay = 72, bool fast = false) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, fast ? 0 : delay);
}

inline bool RRecastRuntime() {
    return ControllerHelpers::RuntimeNameContains(3, "SwainRSoulFlare") ||
           Engine::IsRuntimeRecast(3);
}

inline bool ERecastRuntime() {
    return ControllerHelpers::RuntimeNameContains(2, "SwainE2") ||
           Engine::IsRuntimeRecast(2);
}

inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target) && Engine::RuntimeSpells[0]
        ? player.CalculateMagicDamage(target, Engine::RuntimeSpells[0]->GetDamage(target)) : 0.0f;
}
inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target) && Engine::RuntimeSpells[1]
        ? player.CalculateMagicDamage(target, Engine::RuntimeSpells[1]->GetDamage(target)) : 0.0f;
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target) && Engine::RuntimeSpells[2]
        ? player.CalculateMagicDamage(target, Engine::RuntimeSpells[2]->GetDamage(target)) : 0.0f;
}
inline float DemonflareDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target) && Engine::RuntimeSpells[3]
        ? player.CalculateMagicDamage(target, Engine::RuntimeSpells[3]->GetDamage(target)) : 0.0f;
}
inline float DemonTickDamage(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) ? std::max(0.0f, DemonflareDamage(target) * 0.12f) : 0.0f;
}

inline bool SafeCommit(const Vector3& position, bool defensive, bool lethal) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !position.IsValid() || position.IsZero()) return false;
    if (!defensive && !lethal && Engine::UnderEnemyTurret(position)) return false;
    const int enemies = Engine::CountEnemiesAt(position, 650.0f);
    if (!defensive && !lethal && enemies > Slider(RMenu, "MaxCommitEnemies", 3)) return false;
    if (!defensive && ControllerHelpers::PlayerMobilityLocked()) return false;
    return true;
}

inline AIHeroClient Target(float range) {
    return Engine::SelectTarget(range);
}

inline bool CastERecast(const AIHeroClient& target, Mode mode, bool reactive = false,
                        bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Tether.PullReady || !ERecastRuntime() ||
        !Ready(2, mode) || !Throttle(2, 28, true) || Protected(target) ||
        static_cast<int>(target.NetworkId()) != Tether.TargetId ||
        PreserveAttack(reactive, true)) return false;
    const Vector3 predicted = PredictPosition(target, 0.08f);
    const Vector3 pullEndpoint = PullPosition(player.Position(), predicted);
    if (!predicted.IsValid() || !pullEndpoint.IsValid() ||
        player.Position().Distance2D(predicted) > kERecastRange + target.BoundingRadius() ||
        !CircleHits(predicted, predicted, kEExplosionRadius,
                    target.BoundingRadius()) && !lethal) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    Tether.PullReady = false;
    Tether.TargetId = 0;
    Tether.PullExpireTick = 0;
    Tether.RootExpireTick = Now() + static_cast<int>(kERootDuration * 1000.0f);
    LastCastTick[2] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kERange + 40.0f) ||
        Protected(target) || Tether.Outbound || Tether.PullReady ||
        !Ready(2, mode) || !Throttle(2) || PreserveAttack(reactive)) return false;
    auto aim = PredictPosition(target, 0.25f +
        player.Position().Distance2D(target.Position()) / kESpeed);
    if (Engine::RuntimeSpells[2]) {
        const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
        if (prediction.Hitchance < SDK::HitChance::High && !reactive && !lethal) return false;
        if (prediction.GetCastPosition().IsValid()) aim = prediction.GetCastPosition();
    }
    if (!aim.IsValid() || player.Position().Distance2D(aim) > kERange + target.BoundingRadius() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kEWidth * 0.5f) ||
        !LineHits(player.Position(), aim, PredictPosition(target, 0.15f), kERange,
                  kEWidth, target.BoundingRadius())) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    Tether = BeginNevermove(Tether, Now(), static_cast<int>(target.NetworkId()));
    LastEAim = aim;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + 35.0f) ||
        Protected(target) || !Ready(0, mode) || !Throttle(0) ||
        PreserveAttack(reactive, lethal)) return false;
    Vector3 aim = PredictPosition(target, kQDelay);
    if (Engine::RuntimeSpells[0]) {
        const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
        if (prediction.Hitchance < SDK::HitChance::High && !reactive && !lethal) return false;
        if (prediction.GetCastPosition().IsValid()) aim = prediction.GetCastPosition();
    }
    if (!ConeContains(player.Position(), aim, PredictPosition(target, kQDelay),
                      kQRange, kQHalfAngleDegrees, target.BoundingRadius())) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastQAim = aim;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kWRange + 45.0f) ||
        Protected(target) || !Ready(1, mode) || !Throttle(1) ||
        PreserveAttack(reactive, lethal)) return false;
    const Vector3 predicted = PredictPosition(target, kWDelay +
        player.Position().Distance2D(target.Position()) / 1800.0f);
    if (!predicted.IsValid() || player.Position().Distance2D(predicted) > kWRange + target.BoundingRadius() ||
        !CircleHits(predicted, PredictPosition(target, kWDelay), kWEffectRadius,
                    target.BoundingRadius())) return false;
    if (!Engine::ControllerCastPosition(1, predicted)) return false;
    LastWAim = predicted;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastDemonflare(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Demon.Active || !Demon.RecastReady ||
        !RRecastRuntime() || !Ready(3, mode) || !Throttle(3, 25, true)) return false;
    const bool lethal = Engine::ValidEnemy(target) &&
        Lethal(target, DemonflareDamage(target) + DemonTickDamage(target));
    const int enemies = Engine::CountEnemiesAt(player.Position(), kRRadius);
    const bool defensive = reactive || player.HealthPercent() <= Slider(RMenu, "DefensiveHealth", 36);
    const UltimateContext context{true, true, Demon.RecastReady,
        Engine::ValidEnemy(target) && CircleHits(player.Position(),
            PredictPosition(target, 0.10f), kRRadius, target.BoundingRadius()),
        lethal, defensive, Orbwalker::IsWindingUp(),
        Engine::UnderEnemyTurret(player.Position()), enemies,
        Slider(RMenu, "MinimumDetonationEnemies", 2)};
    if (!ShouldDemonflare(context)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    Demon.Active = false;
    Demon.RecastReady = false;
    Demon.Power = 0.0f;
    LastCastTick[3] = Now();
    return true;
}

inline bool CastDemon(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Demon.Active || !Ready(3, mode) || !Throttle(3) ||
        PreserveAttack(reactive, true)) return false;
    const AIHeroClient target = Target(kRRadius + 50.0f);
    const float sustain = DemonHealPerSecond(90.0f, AP(), player.MaxHealth());
    const int enemies = Engine::CountEnemiesAt(player.Position(), kRRadius);
    const bool defensive = reactive || IncomingHardCCUntil > Now() ||
        player.HealthPercent() <= Slider(RMenu, "DefensiveHealth", 36) ||
        (player.HealthPercent() < 52.0f && sustain > player.MaxHealth() * 0.01f);
    const bool lethal = Engine::ValidEnemy(target) &&
        Lethal(target, QDamage(target) + EDamage(target) + WDamage(target));
    const UltimateContext context{true, false, false,
        enemies > 0 || defensive, lethal, defensive,
        Orbwalker::IsWindingUp(), Engine::UnderEnemyTurret(player.Position()), enemies,
        Slider(RMenu, "MinimumStartEnemies", 2)};
    if (!ShouldCastDemon(context) || !SafeCommit(player.Position(), defensive, lethal)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    Demon = StartDemon(Demon, Now(), 0.0f);
    LastCastTick[3] = Now();
    return true;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Tether.PullReady && Lethal(target, EDamage(target)) && CastERecast(target, mode, true, true)) return true;
    if (Lethal(target, EDamage(target)) && CastE(target, mode, true, true)) return true;
    if (Lethal(target, QDamage(target)) && CastQ(target, mode, true, true)) return true;
    return Lethal(target, WDamage(target)) && CastW(target, mode, true, true);
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (Tether.PullReady && CastERecast(target, Mode::Combo)) return;
    if (!Demon.Active && CastE(target, Mode::Combo)) return;
    if (!Demon.Active && CastDemon(Mode::Combo)) return;
    if (Demon.Active && CastDemonflare(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    (void)CastW(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(WMenu, "HarassMana", 55)) return;
    if (Tether.PullReady && CastERecast(target, Mode::Harass)) return;
    if (CastE(target, Mode::Harass)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& target) {
    if (Demon.Active && Engine::ValidEnemy(target) && CastDemonflare(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target) && Tether.PullReady && CastERecast(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target) && CastW(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastE(target, Mode::Flee, true);
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    const int now = Now();
    if (!player.IsValid()) return;
    const bool rBuff = player.HasBuff("SwainR") || player.HasBuff("SwainRSoulBurn");
    if (rBuff && !Demon.Active) Demon = StartDemon(Demon, now, Demon.Power);
    if (!rBuff && Demon.Active && now - Demon.StartedTick > 6500) Demon = {};
    const bool pullBuff = player.HasBuff("SwainPPullReady") ||
        ControllerHelpers::RuntimeNameContains(2, "SwainE2");
    if (pullBuff && !Tether.PullReady && Tether.TargetId != 0)
        Tether = ArmPull(Tether, now, Tether.TargetId);
    Tether = ExpireTether(Tether, now);
    Demon = AdvanceDemon(Demon, now, Engine::CountEnemiesAt(player.Position(), kRRadius));
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    const auto player = GameObjects::Player();
    const float range = mode == Mode::Flee ? 1300.0f : kWRange;
    const AIHeroClient target = Engine::SelectTarget(range);
    if (Engine::ValidEnemy(target) && TryKillSecure(target, mode)) return true;
    if (Demon.Active && Engine::ValidEnemy(target) && CastDemonflare(target, mode)) return true;
    if (IncomingHardCCUntil > Now() && Engine::ValidEnemy(target) && CastW(target, mode, true)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1300.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (player.IsValid() && player.ManaPercent() >= Slider(FarmMenu, "Mana", 48))
            (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target) && IncomingThreatUntil > Now())
            (void)CastW(target, mode, true);
        else if (Engine::ValidEnemy(target) && Demon.Active)
            (void)CastDemonflare(target, mode, true);
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
        if (slot >= 0 && slot <= 3) LastCastTick[slot] = now;
        if (ControllerHelpers::SpellEventNameContains(args, "SwainE")) {
            if (ControllerHelpers::SpellEventNameContains(args, "SwainE2")) {
                Tether = ArmPull(Tether, now, Tether.TargetId);
            } else {
                const int id = args.TargetNetworkId != 0
                    ? static_cast<int>(args.TargetNetworkId) : Tether.TargetId;
                Tether = BeginNevermove(Tether, now, id);
            }
        } else if (ControllerHelpers::SpellEventNameContains(args, "SwainRSoulFlare")) {
            Demon = {};
        } else if (ControllerHelpers::SpellEventNameContains(args, "SwainR")) {
            if (!Demon.Active) Demon = StartDemon(Demon, now, 0.0f);
        } else if (ControllerHelpers::SpellEventNameContains(args, "SwainPassive")) {
            Souls = CollectSoul(Souls, now);
        }
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
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (Engine::TextContains(args.BuffName, "SwainSoulCounter") ||
        Engine::TextContains(args.BuffName, "SwainSoulCount")) {
        Souls = ReconcileSouls(Souls, std::max(0, args.Count), now);
    } else if (Engine::TextContains(args.BuffName, "SwainPPullReady")) {
        Tether = ArmPull(Tether, now, Tether.TargetId);
    } else if (Engine::TextContains(args.BuffName, "SwainR")) {
        if (!Demon.Active) Demon = StartDemon(Demon, now, Demon.Power);
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "SwainPPullReady")) {
        Tether.PullReady = false;
        Tether.PullExpireTick = 0;
    } else if (Engine::TextContains(args.BuffName, "SwainR") && Demon.Active &&
               Now() - Demon.StartedTick > 6500) {
        Demon = {};
    }
}

inline void OnInterruptable(
    const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    ControllerHelpers::CaptureInterruptable(args, LastInterruptTargetId,
        LastInterruptExpireTick, 900, 300, 5000);
    IncomingHardCCUntil = std::max(IncomingHardCCUntil, LastInterruptExpireTick);
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF8F5BFFu, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kRRadius, 0xFFCC66FFu, 1.3f, 40);
    if (LastWAim.IsValid() && Now() - LastCastTick[1] < 1800)
        Drawing::DrawCircle(LastWAim, kWEffectRadius, 0xFF66CCFFu, 1.2f, 32);
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    const int slot = static_cast<int>(args.Slot);
    if (slot >= 0 && slot <= 3) LastCastTick[slot] = Now();
    if (ControllerHelpers::SpellEventNameContains(args, "SwainE2")) {
        Tether = ArmPull(Tether, Now(), Tether.TargetId);
    } else if (ControllerHelpers::SpellEventNameContains(args, "SwainRSoulFlare")) {
        Demon = {};
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("SwainOneTrick", "Swain battlemage tactics"));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Death's Hand"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Vision of Empire"));
    WMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 55, 20, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Nevermove"));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Demonic Ascension"));
    RMenu->Add(new MenuSlider("DefensiveHealth", "Defensive health percent", 36, 10, 70));
    RMenu->Add(new MenuSlider("MinimumStartEnemies", "Minimum nearby enemies to start R", 2, 1, 5));
    RMenu->Add(new MenuSlider("MinimumDetonationEnemies", "Minimum enemies for Demonflare", 2, 1, 5));
    RMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies for nonlethal R", 3, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("SwainFarm", "Soul and mana farm"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 48, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("SwainCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/R/W zones", false));
}

inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    Souls = {};
    Tether = {};
    Demon = {};
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = LastInterruptTargetId = LastInterruptExpireTick = 0;
    IncomingThreatUntil = IncomingHardCCUntil = 0;
    LastQAim = LastWAim = LastEAim = {};
    LastMode = Mode::None;
}

inline void OnLoad() {
    Souls = {};
    Tether = {};
    Demon = {};
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = LastInterruptTargetId = LastInterruptExpireTick = 0;
    IncomingThreatUntil = IncomingHardCCUntil = 0;
    LastQAim = LastWAim = LastEAim = {};
    LastMode = Mode::None;
}

inline constexpr const char* Scenarios[] = {
    "Pin values to Riot 26.15 and the CommunityDragon 16.15 Swain champion JSON",
    "Reconcile soul fragment count and permanent fifteen health gains from buff and passive observations",
    "Track Nevermove outbound, rooted tether, pull-ready recast, target id and pull expiration",
    "Predict E return path, reject collision and projectile-wall blocked casts, and only pull the tethered target",
    "Use E pull explosion radius and real 290-unit pull endpoint rather than a generic recast",
    "Place global W vision zones with travel prediction, 7500 range and 325-unit impact radius",
    "Use Q's twenty-degree cone, 750 range, prediction and target-radius edge handling",
    "Start Demonic Ascension only with nearby pressure, defensive need, lethal setup or a multi-target commitment",
    "Advance Demon Power with ten-per-second drain and twenty-per-second enemy-contact regeneration",
    "Protect R movement channel and reconcile SwainR buff polling",
    "Detonate Demonflare after the two-second arming window on lethal, defensive or multi-target value",
    "Reject fresh nonlethal R commits under enemy turrets, excessive enemy count or unsafe mobility lock",
    "Use the autonomous engine-selected target while requiring reachable kit geometry",
    "Preserve AA windup for ordinary casts while allowing reactive, lethal and recast exceptions",
    "Run distinct Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic policies",
    "Use W peel on incoming hard crowd control and E pull as a verified tethered kill secure",
    "Reconcile local process-spell, soul, tether, ultimate and enemy threat events through polling",
    "Draw Q, R and last W geometry without changing gameplay decisions",
    "Never automate items, summoner spells, flash or movement ownership",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Swain;
    controller.ControllerId = "champion.kuroaio.ai.swain.battlemage";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AISwain.md";
    controller.ImplementationSummary =
        "Soul-fragment state, predictive Nevermove tether/pull, global Vision zones, cone Death's Hand and Demon Power-aware Demonic Ascension controller.";
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

    controller.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnDoCast = &OnDoCast;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Swain
