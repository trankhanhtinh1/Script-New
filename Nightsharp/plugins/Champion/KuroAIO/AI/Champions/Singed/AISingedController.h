#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AISinged.h"
#include "AISingedGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Singed {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellEnabled;
inline Menu* TacticsMenu = nullptr;
inline Menu* SafetyMenu = nullptr;
inline std::array<int, 4> LastCastTick{};
inline int ManualOverrideUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline int WTargetId = 0;
inline int WCastTick = 0;
inline int ETargetId = 0;
inline int ECastTick = 0;
inline int RExpireTick = 0;
inline bool PoisonActive = false;
inline bool PotionActive = false;
inline bool ControllerPoisonIntent = false;
inline bool ControllerPotionIntent = false;
inline Vector3 LastAdhesivePosition{};
inline Vector3 LastFlingEndpoint{};

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
        (reactive || ManualOverrideUntil <= Now());
}

inline bool PreserveAttack(bool reactive, bool allowDuringWindup = false) {
    return !reactive && !allowDuringWindup && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true);
}

inline bool IsPoisonBuffActive(const AIHeroClient& player) {
    return player.IsValid() && (player.HasBuff("PoisonTrail") ||
        player.HasBuff("poisontrail") || player.HasBuff("SingedQ"));
}

inline bool IsPotionBuffActive(const AIHeroClient& player) {
    return player.IsValid() && (player.HasBuff("InsanityPotion") ||
        player.HasBuff("insanitypotion") || player.HasBuff("SingedR"));
}

inline bool IsTargetOnRecentAdhesive(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) && WTargetId == static_cast<int>(target.NetworkId()) &&
        WCastTick > 0 && Now() - WCastTick <= 2100;
}

inline float FlingDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = FlingRawDamage(ControllerHelpers::SpellRank(2), player.AP(), target.MaxHealth());
    return player.CalculateMagicDamage(target, raw);
}

inline bool CanTogglePoison(Mode mode, const AIHeroClient& target,
                            bool farming = false, bool escaping = false,
                            bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0, mode, reactive) || PreserveAttack(reactive, true)) return false;
    const bool hostileInside = Engine::ValidEnemy(target, kPoisonRadius + target.BoundingRadius()) &&
        player.Position().Distance2D(target.Position()) <= kPoisonRadius + target.BoundingRadius();
    const bool lowMana = player.ManaPercent() <= Slider(TacticsMenu, "PoisonReserve", 24);
    const bool wanted = PoisonToggleWanted(PoisonActive, hostileInside, farming, lowMana, escaping);
    return wanted != PoisonActive;
}

inline bool CastPoison(Mode mode, const AIHeroClient& target,
                       bool farming = false, bool escaping = false,
                       bool reactive = false) {
    if (!CanTogglePoison(mode, target, farming, escaping, reactive)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    LastCastTick[0] = Now();
    ControllerPoisonIntent = !PoisonActive;
    PoisonActive = !PoisonActive;
    return true;
}

inline bool CastAdhesive(const AIHeroClient& target, Mode mode,
                         bool escaping = false, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode, reactive) ||
        PreserveAttack(reactive, escaping) ||
        (!escaping && !Engine::ValidEnemy(target, kAdhesiveRange + 80.0f))) return false;

    Vector3 aim = escaping ? Game::CursorPos() : PredictPosition(target, 0.375f);
    if (!aim.IsValid() || aim.IsZero()) aim = escaping ? Game::CursorPos() : target.Position();
    if (!AdhesiveLandingValid(player.Position(), aim) || SDK::NavMesh::IsWall(aim) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 8.0f)) return false;
    if (!escaping && Engine::RuntimeSpells[1]) {
        const auto prediction = Engine::RuntimeSpells[1]->GetPrediction(target);
        if (!prediction.GetCastPosition().IsZero() && !prediction.CollisionObjects.empty()) return false;
    }
    if (Engine::UnderEnemyTurret(aim) && !escaping &&
        Engine::CountEnemiesAt(aim, 600.0f) > Slider(SafetyMenu, "MaxEndpointEnemies", 2)) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    LastCastTick[1] = Now();
    WCastTick = LastCastTick[1];
    WTargetId = escaping ? 0 : static_cast<int>(target.NetworkId());
    LastAdhesivePosition = aim;
    return true;
}

inline bool CastFling(const AIHeroClient& target, Mode mode,
                      bool escaping = false, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kFlingRange + target.BoundingRadius()) ||
        !Ready(2, mode, reactive) || PreserveAttack(reactive, escaping)) return false;
    const Vector3 predicted = PredictPosition(target, 0.12f);
    if (!InFlingReach(player.Position(), predicted, target.BoundingRadius())) return false;
    const Vector3 endpoint = FlingEndpoint(player.Position(), predicted, target.BoundingRadius());
    if (!endpoint.IsValid() || endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint)) return false;
    const bool lethal = FlingLethal(FlingDamage(target), target.Health(), target.AllShield());
    const bool endpointTurret = Engine::UnderEnemyTurret(endpoint);
    const bool originTurret = Engine::UnderEnemyTurret(player.Position());
    const int nearby = Engine::CountEnemiesAt(endpoint, 550.0f);
    if (!FlingEndpointSafe(true, false, endpointTurret, originTurret, nearby,
                           Slider(SafetyMenu, "MaxEndpointEnemies", 2), lethal, escaping)) return false;

    const Vector3 pathEnd = target.PathEnd();
    const bool movingAway = pathEnd.IsValid() && !pathEnd.IsZero() &&
        pathEnd.Distance2D(player.Position()) > target.Position().Distance2D(player.Position()) + 35.0f;
    if (!AntiChaseAllowed(player.Position().Distance2D(target.Position()), target.MoveSpeed(),
                          movingAway, Engine::CountEnemiesAt(player.Position(), 650.0f),
                          endpointTurret, lethal, escaping)) return false;
    if (!escaping && !lethal && target.HealthPercent() > 72.0f && !IsTargetOnRecentAdhesive(target)) return false;
    if (!Engine::TryCast(Profiles::Singed.Spells[2], target, mode,
                         lethal ? StepRule::RequireTargetLow : StepRule::None, reactive)) return false;
    LastCastTick[2] = Now();
    ECastTick = LastCastTick[2];
    ETargetId = static_cast<int>(target.NetworkId());
    LastFlingEndpoint = endpoint;
    return true;
}

inline bool CastPotion(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PotionActive || !Ready(3, mode, reactive) ||
        PreserveAttack(reactive, true)) return false;
    const bool threat = IncomingThreatUntil >= Now();
    const bool targetReach = Engine::ValidEnemy(target, 650.0f);
    if (!PotionCommitAllowed(player.HealthPercent(), Engine::CountEnemiesAt(player.Position(), 700.0f),
                             targetReach, threat, PotionActive) && !reactive) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    RExpireTick = Now() + kPotionDurationMs;
    ControllerPotionIntent = true;
    PotionActive = true;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, 1100.0f)) return;
    if (CastPotion(target, Mode::Combo)) return;
    if (CastPoison(Mode::Combo, target)) return;
    if (IsTargetOnRecentAdhesive(target) && CastFling(target, Mode::Combo)) return;
    if (CastAdhesive(target, Mode::Combo)) return;
    (void)CastFling(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(TacticsMenu, "HarassMana", 42) ||
        !Engine::ValidEnemy(target, 1100.0f)) return;
    if (CastPoison(Mode::Harass, target)) return;
    if (IsTargetOnRecentAdhesive(target) && CastFling(target, Mode::Harass)) return;
    (void)CastAdhesive(target, Mode::Harass);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const bool jungle = mode == Mode::Jungle;
    const bool lastHit = mode == Mode::LastHit;
    if (player.ManaPercent() >= Slider(TacticsMenu, "FarmMana", 24))
        (void)CastPoison(mode, {}, true, false);
    // Poison is a self-centered persistent zone; basic attacks finish the units while it runs.
    (void)lastHit;
    (void)jungle;
}

inline void Flee(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (CastPotion(target, Mode::Flee, true)) return;
    if (CastPoison(Mode::Flee, target, false, true, true)) return;
    if (Engine::ValidEnemy(target, kAdhesiveRange) && CastAdhesive(target, Mode::Flee, true, true)) return;
    if (Engine::ValidEnemy(target, kFlingRange + target.BoundingRadius()))
        (void)CastFling(target, Mode::Flee, true, true);
}

inline void Automatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const auto threat = IncomingThreatUntil >= Now() ?
        Engine::EnemyByNetworkId(IncomingThreatTargetId) : target;
    if (Engine::ValidEnemy(threat, kFlingRange + threat.BoundingRadius()) &&
        (IncomingThreatUntil >= Now() || player.HealthPercent() < 38.0f)) {
        if (CastPotion(threat, Mode::Automatic, true)) return;
        if (CastAdhesive(threat, Mode::Automatic, false, true)) return;
        if (CastFling(threat, Mode::Automatic, false, true)) return;
    }
    if (Engine::ValidEnemy(target, 1000.0f)) {
        (void)CastPoison(Mode::Automatic, target, false, false, true);
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return true;
    PoisonActive = IsPoisonBuffActive(player);
    PotionActive = IsPotionBuffActive(player) || (RExpireTick > Now() && ControllerPotionIntent);
    if (PotionActive && RExpireTick > 0 && RExpireTick <= Now()) {
        PotionActive = false;
        ControllerPotionIntent = false;
    }
    if (IncomingThreatUntil < Now()) {
        IncomingThreatUntil = 0;
        IncomingThreatTargetId = 0;
    }
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, 1100.0f);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
    case Mode::Flee: Flee(target); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("Singed tactics"));
    SafetyMenu = root->AddSubMenu(new Menu("Singed endpoint safety"));
    TacticsMenu->Add(new MenuSlider("PoisonReserve", "Turn poison off below mana percent", 24, 0, 100));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 42, 0, 100));
    TacticsMenu->Add(new MenuSlider("FarmMana", "Farm mana percent", 24, 0, 100));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Manual cast ownership window", 560, 100, 1200));
    SafetyMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum enemies at fling endpoint", 2, 1, 5));
}

inline void OnLoad() {
    LastCastTick.fill(0);
    ManualOverrideUntil = 0;
    IncomingThreatUntil = 0;
    IncomingThreatTargetId = 0;
    WTargetId = 0;
    WCastTick = 0;
    ETargetId = 0;
    ECastTick = 0;
    RExpireTick = 0;
    PoisonActive = false;
    PotionActive = false;
    ControllerPoisonIntent = false;
    ControllerPotionIntent = false;
    LastAdhesivePosition = {};
    LastFlingEndpoint = {};
}

inline void OnUnload() {
    TacticsMenu = nullptr;
    SafetyMenu = nullptr;
    OnLoad();
}

inline void OnDraw() {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        // Fling and poison are deliberately woven around the observed attack rather than cancelling it.
        LastCastTick[0] = std::max(LastCastTick[0], Now() - 18);
    }
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        if (args.Slot >= 0 && args.Slot < 4) {
            LastCastTick[static_cast<std::size_t>(args.Slot)] = Now();
            if (!Engine::WasControllerCast(args.Slot))
                ManualOverrideUntil = Now() + Slider(TacticsMenu, "ManualOwnershipMs", 560);
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && analysis.Enemy.IsValid()) {
        IncomingThreatTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "poisontrail")) PoisonActive = true;
    if (Engine::TextContains(args.BuffName, "insanitypotion")) {
        PotionActive = true;
        RExpireTick = Now() + kPotionDurationMs;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "poisontrail")) PoisonActive = false;
    if (Engine::TextContains(args.BuffName, "insanitypotion")) {
        PotionActive = false;
        ControllerPotionIntent = false;
        RExpireTick = 0;
    }
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs&) {}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs&) {}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, IncomingThreatTargetId, LastFlingEndpoint,
                           IncomingThreatUntil, 700.0f, 1200);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, IncomingThreatTargetId, IncomingThreatUntil, 900, 250, 5000);
}

inline constexpr const char* Scenarios[] = {
    "Poison Trail toggle, mana reserve and four-tick damage-over-time linger",
    "Mega Adhesive prediction, grounded setup and projectile-wall rejection",
    "Fling real reach, max-health damage plus shield lethal gate",
    "Fling endpoint wall, turret, enemy-count and anti-chase posture",
    "Insanity Potion 25-second proxy or escape stat commitment",
    "combo and harass poison route with adhesive-to-fling priority",
    "lane clear, jungle and last-hit poison persistence",
    "flee adhesive escape, threat event and polling reconciliation",
    "manual cast windup protection, selected target and orbwalker policy",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Singed;
    controller.ControllerId = "champion.kuroaio.ai.singed.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AISinged.md";
    controller.ImplementationSummary =
        "Poison-route toggle, adhesive fling setup, safe endpoint and anti-chase checks, plus potion proxy/escape posture.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Singed
