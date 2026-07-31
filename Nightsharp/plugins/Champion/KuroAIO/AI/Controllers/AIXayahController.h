#pragma once

#include "../AIChampionEngine.h"
#include "../AIMarksmanControllerHelpers.h"
#include "AIXayahGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Xayah {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::ObjectEventIsAllied;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::RuntimeNameContains;

inline Menu* TacticsMenu = nullptr;
inline Menu* FeatherMenu = nullptr;
inline Menu* PostureMenu = nullptr;

inline std::array<Feather, kMaximumTrackedFeathers> Feathers{};
inline int NextFeatherId = 1;
inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline int PlayerOverrideUntil = 0;
inline Mode LastMode = Mode::None;
inline AIHeroClient LastSmartTarget = {};

inline bool FeatherObject(const SDK::Events::ObjectEventArgs& args) {
    return ControllerHelpers::AnyTextContains(
               {args.Sender.Name, args.Sender.CharacterName,
                args.SpellName, args.MissileName},
               {"xayahpassivefeather", "xayahfeather"}) &&
           ControllerHelpers::AnyTextContains(
               {args.Sender.Name, args.Sender.CharacterName,
                args.SpellName, args.MissileName},
               {"feather"});
}

inline bool LiveFeatherName(const char* value) {
    return value && value[0] &&
        (Engine::TextContains(value, "xayah") &&
         Engine::TextContains(value, "feather"));
}

inline int FindFeatherById(int id) {
    if (id == 0) return -1;
    for (std::size_t index = 0; index < Feathers.size(); ++index) {
        if (Feathers[index].Active && Feathers[index].NetworkId == id) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

inline void RecordFeather(const Vector3& position, int networkId = 0,
                          int spawnTick = 0) {
    if (!position.IsValid() || position.IsZero()) return;
    const int now = spawnTick > 0 ? spawnTick : Now();
    if (networkId != 0) {
        const int existing = FindFeatherById(networkId);
        if (existing >= 0) {
            Feathers[static_cast<std::size_t>(existing)].Position = position;
            Feathers[static_cast<std::size_t>(existing)].SpawnTick = now;
            return;
        }
    }
    for (auto& feather : Feathers) {
        if (feather.Active && feather.Position.Distance2D(position) <= 14.0f) {
            if (networkId != 0) feather.NetworkId = networkId;
            feather.Position = position;
            feather.SpawnTick = now;
            return;
        }
    }
    for (auto& feather : Feathers) {
        if (!feather.Active || now - feather.SpawnTick > kFeatherLifetimeMs) {
            feather = {position, networkId != 0 ? networkId : NextFeatherId++,
                       now, true};
            return;
        }
    }
}

inline void RemoveFeather(int networkId) {
    const int index = FindFeatherById(networkId);
    if (index >= 0) Feathers[static_cast<std::size_t>(index)] = {};
}

inline void ReconcileFeathers() {
    const int now = Now();
    for (auto& feather : Feathers) {
        if (!feather.Active || !feather.Position.IsValid() ||
            feather.Position.IsZero() || feather.SpawnTick <= 0 ||
            now - feather.SpawnTick > kFeatherLifetimeMs) {
            feather = {};
        }
    }
}

inline int FeatherHits(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0;
    return CountFeathersThrough(Feathers, player.Position(),
                                PredictPosition(target, 0.18f),
                                65.0f, Now());
}

inline bool RootReady(const AIHeroClient& target) {
    return RootAvailable(FeatherHits(target),
                         Slider(FeatherMenu, "RootThreshold", kRootThreshold));
}

inline bool IsWActive() {
    const auto player = GameObjects::Player();
    return player.IsValid() &&
        (player.HasBuff("XayahW") || player.HasBuff("XayahWAttackSpeed"));
}

inline bool TargetUsable(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) &&
           !ControllerHelpers::HasSpellShieldOrImmunity(target) &&
           !ControllerHelpers::IsCommonUntargetableOrImmune(target);
}

inline bool TurretSafe(bool urgent, bool defensive = false) {
    const auto player = GameObjects::Player();
    return player.IsValid() && (!Engine::UnderEnemyTurret(player.Position()) ||
                                urgent || defensive);
}

inline float BladecallerDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const int count = FeatherHits(target);
    const float raw = BladecallerRawDamage(
        ControllerHelpers::SpellRank(2), player.BonusAttackDamage(), count);
    return player.CalculatePhysicalDamage(target, raw);
}

inline bool AttackRoute(const AIHeroClient& target) {
    return OrbwalkerAttackRoute(target) && !OrbwalkerAttackProjectileBlocked(target);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (!TargetUsable(target) || !CanUse(0, mode, reactive) ||
        !CastThrottlePassed(LastQCastTick, 80)) return false;
    if (Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true) && !reactive) return false;
    SDK::PredictionOutput prediction{};
    if (!PredictionHits(0, target, SDK::HitChance::High, true, &prediction) ||
        PredictionProjectileWall(0, prediction, 36.0f)) return false;
    if (!TurretSafe(false, false) &&
        AutoDamage(target) < target.Health() + target.AllShield()) return false;
    if (!Engine::ControllerCastPosition(0, prediction.GetCastPosition())) return false;
    LastQCastTick = Now();
    const auto player = GameObjects::Player();
    const Vector3 origin = player.IsValid() ? player.Position() : Vector3{};
    const Vector3 endpoint = ClampFeatherEndpoint(origin,
                                                  prediction.GetCastPosition());
    if (!endpoint.IsZero()) {
        const Vec3 direction = Direction2D(origin, endpoint);
        const Vec3 side{-direction.z, 0.0f, direction.x};
        RecordFeather(endpoint + side * kQFeatherSpread);
        RecordFeather(endpoint - side * kQFeatherSpread);
    }
    return true;
}

inline BladecallerContext BuildEContext(const AIHeroClient& target,
                                        Mode mode,
                                        bool reactive = false) {
    BladecallerContext context{};
    context.Ready = CanUse(2, mode, reactive);
    context.TargetValid = TargetUsable(target);
    context.PredictionAccepted = context.TargetValid && FeatherHits(target) > 0;
    context.FeatherHits = FeatherHits(target);
    context.MinimumFeathers = Slider(FeatherMenu, "MinimumFeathers", 2);
    context.RootReady = RootReady(target);
    context.Lethal = context.TargetValid && BladecallerDamage(target) >=
        target.Health() + target.AllShield();
    context.AttackWindingUp = Orbwalker::IsWindingUp();
    context.TurretUnsafe = !TurretSafe(context.Lethal, reactive);
    context.ManualCast = PlayerOverrideUntil >= Now();
    return context;
}

inline bool CastE(const AIHeroClient& target, Mode mode,
                  bool reactive = false) {
    if (!TargetUsable(target) || !CanUse(2, mode, reactive) ||
        !CastThrottlePassed(LastECastTick, 55)) return false;
    const auto context = BuildEContext(target, mode, reactive);
    if (!ShouldBladecaller(context) || !Engine::ControllerCastSelf(2)) return false;
    LastECastTick = Now();
    for (auto& feather : Feathers) feather = {};
    return true;
}

inline WPostureContext BuildWContext(const AIHeroClient& target, Mode mode) {
    WPostureContext context{};
    const auto player = GameObjects::Player();
    context.Ready = CanUse(1, mode, false) && !IsWActive();
    context.TargetValid = TargetUsable(target);
    context.AttackRoute = AttackRoute(target);
    context.AttackWindingUp = Orbwalker::IsWindingUp();
    context.LethalWindow = context.TargetValid &&
        AutoDamage(target) * 2.0f >= target.Health() + target.AllShield();
    context.Defensive = player.IsValid() && player.HealthPercent() <=
        Slider(PostureMenu, "DefensiveHealth", 35);
    context.TurretUnsafe = player.IsValid() &&
        Engine::UnderEnemyTurret(player.Position()) && !context.LethalWindow;
    return context;
}

inline bool CastW(const AIHeroClient& target, Mode mode) {
    if (!TargetUsable(target) || !CanUse(1, mode, false) ||
        !CastThrottlePassed(LastWCastTick, 90)) return false;
    const auto context = BuildWContext(target, mode);
    if (!ShouldDeadlyPlumage(context) || !Engine::ControllerCastSelf(1)) return false;
    LastWCastTick = Now();
    return true;
}

inline RPostureContext BuildRContext(const AIHeroClient& target,
                                     bool defensive,
                                     bool manual = false) {
    RPostureContext context{};
    const auto player = GameObjects::Player();
    context.Ready = Engine::RuntimeSpells[3] && Engine::RuntimeSpells[3]->IsReady();
    context.TargetValid = TargetUsable(target);
    SDK::PredictionOutput prediction{};
    context.PredictionAccepted = context.TargetValid &&
        PredictionHits(3, target, SDK::HitChance::High, false, &prediction);
    context.ProjectileBlocked = context.PredictionAccepted &&
        PredictionProjectileWall(3, prediction, 42.0f);
    context.PlayerLow = player.IsValid() && player.HealthPercent() <=
        Slider(PostureMenu, "UltimateHealth", 32);
    context.MultiTarget = target.IsValid() &&
        Engine::CountEnemiesAt(target.Position(), 260.0f) >= 2;
    context.Lethal = context.TargetValid &&
        SpellDamage(3, target) + AutoDamage(target) >=
            target.Health() + target.AllShield();
    context.Defensive = defensive || manual || context.PlayerLow;
    context.TurretUnsafe = player.IsValid() &&
        Engine::UnderEnemyTurret(player.Position()) &&
        !context.Defensive && !context.Lethal;
    context.ChampionHits = context.MultiTarget ?
        Engine::CountEnemiesAt(target.Position(), 260.0f) : 1;
    context.MinimumHits = Slider(PostureMenu, "MinimumRTargets", 2);
    return context;
}

inline bool CastR(const AIHeroClient& target, Mode mode,
                  bool defensive = false, bool manual = false) {
    if (!TargetUsable(target) || !CanUse(3, mode, true) ||
        !CastThrottlePassed(LastRCastTick, 110)) return false;
    SDK::PredictionOutput prediction{};
    const auto context = BuildRContext(target, defensive, manual);
    if (!ShouldFeatherstorm(context) ||
        !PredictionHits(3, target, SDK::HitChance::High, false, &prediction) ||
        !Engine::ControllerCastPosition(3, prediction.GetCastPosition())) return false;
    LastRCastTick = Now();
    const auto player = GameObjects::Player();
    if (player.IsValid()) {
        const Vector3 endpoint = ClampFeatherEndpoint(
            player.Position(), prediction.GetCastPosition(), kRRange);
        const Vec3 direction = Direction2D(player.Position(), endpoint);
        if (!direction.IsZero()) {
            const Vec3 side{-direction.z, 0.0f, direction.x};
            for (int index = -2; index <= 2; ++index) {
                RecordFeather(endpoint + side * (22.0f * index));
            }
        }
    }
    return true;
}

inline MarksmanTargeting::TargetContext TargetFacts(
    const AIHeroClient& target, Mode mode) {
    const bool attack = AttackRoute(target);
    SDK::PredictionOutput qPrediction{};
    const bool q = TargetUsable(target) && CanUse(0, mode, false) &&
        PredictionHits(0, target, SDK::HitChance::High, true, &qPrediction) &&
        !PredictionProjectileWall(0, qPrediction, 36.0f);
    const int hits = FeatherHits(target);
    const bool e = TargetUsable(target) && CanUse(2, mode, true) && hits > 0;
    const bool w = TargetUsable(target) && CanUse(1, mode, false) && attack;
    const bool r = TargetUsable(target) && CanUse(3, mode, true) &&
        PredictionHits(3, target, SDK::HitChance::High, false);
    const std::array<bool, 4> reachable{q, w, e, r};
    auto context = BaseTargetContext(target, EstimatedDamage(
        target, reachable, attack ? 2 : 0));
    context.AutoReachable = attack;
    context.DirectSpellReachable = q || e;
    context.SetupReachable = w || r;
    context.ExecuteReachable = e &&
        BladecallerDamage(target) >= target.Health() + target.AllShield();
    context.ProjectileBlocked = !attack && !e &&
        (OrbwalkerAttackProjectileBlocked(target) ||
         (q && PredictionProjectileWall(0, qPrediction, 36.0f)));
    context.Priority += static_cast<float>(hits) * 42.0f;
    if (RootReady(target)) context.Priority += 125.0f;
    return context;
}

inline AIHeroClient SelectTarget(const AIHeroClient& preferred, Mode mode) {
    LastSmartTarget = ControllerHelpers::SelectReachableEnemy(
        preferred, kQRange + 75.0f,
        [mode](const AIHeroClient& enemy) { return TargetFacts(enemy, mode); });
    return LastSmartTarget;
}

inline bool TryAntiGapcloser() {
    if (GapcloserExpireTick < Now()) return false;
    const auto target = HeroByNetworkId(GapcloserTargetId);
    return TargetUsable(target) &&
        (CastR(target, Mode::Automatic, true) ||
         CastE(target, Mode::Automatic, true));
}

inline bool TryKillSecure(const AIHeroClient& preferred) {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    const auto target = SelectTarget(preferred, Mode::Automatic);
    if (!TargetUsable(target)) return false;
    if (BladecallerDamage(target) >= target.Health() + target.AllShield()) {
        return CastE(target, Mode::Automatic, true);
    }
    if (SpellDamage(0, target) + AutoDamage(target) >=
            target.Health() + target.AllShield()) {
        return CastQ(target, Mode::Automatic, true);
    }
    return false;
}

inline bool TryCombat(const AIHeroClient& target, Mode mode) {
    if (!TargetUsable(target)) return false;
    if (CastE(target, mode, false)) return true;
    if (mode == Mode::Combo && CastR(target, mode, false)) return true;
    if (CastW(target, mode)) return true;
    return CastQ(target, mode, false);
}

inline bool TryFlee(const AIHeroClient& preferred) {
    const auto target = ControllerHelpers::NearestEnemyToPlayer(preferred, kQRange);
    if (!TargetUsable(target)) return false;
    if (CastR(target, Mode::Flee, true)) return true;
    return CastE(target, Mode::Flee, true);
}

inline bool TryFarm(Mode mode) {
    const bool lastHit = mode == Mode::LastHit;
    const bool jungle = mode == Mode::Jungle;
    if (CanUse(2, mode, true) && Engine::TryFarmSpell(2, jungle, lastHit)) return true;
    return CanUse(0, mode, false) && Engine::TryFarmSpell(0, jungle, lastHit);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    LastMode = mode;
    ReconcileFeathers();
    if (PlayerOverrideUntil >= Now() && mode != Mode::Automatic) return false;
    if (TryAntiGapcloser()) return true;
    if (TryKillSecure(preferred)) return true;
    if (mode == Mode::Combo || mode == Mode::Harass) {
        return TryCombat(SelectTarget(preferred, mode), mode);
    }
    if (mode == Mode::Flee) return TryFlee(preferred);
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) return TryFarm(mode);
    return false;
}

inline void ObserveLocalSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender) || args.Slot < 0 || args.Slot > 3) return;
    const int now = Now();
    const bool ours = Engine::WasControllerCast(args.Slot);
    if (!ours) PlayerOverrideUntil = now + 520;
    if (args.IsAutoAttack) return;
    const auto player = GameObjects::Player();
    const Vector3 origin = args.StartPosition.IsValid() &&
            !args.StartPosition.IsZero()
        ? args.StartPosition : (player.IsValid() ? player.Position() : Vector3{});
    Vector3 aim = args.EndPosition.IsValid() && !args.EndPosition.IsZero()
        ? args.EndPosition : args.CastPosition;
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
        ControllerHelpers::SpellEventNameContainsAny(
            args, {"xayahq", "double daggers"})) {
        LastQCastTick = now;
        if (!aim.IsValid() || aim.IsZero()) aim = origin + Vector3{kQRange, 0, 0};
        const Vector3 endpoint = ClampFeatherEndpoint(origin, aim);
        const Vec3 direction = Direction2D(origin, endpoint);
        const Vec3 side{-direction.z, 0.0f, direction.x};
        if (!direction.IsZero()) {
            RecordFeather(endpoint + side * kQFeatherSpread);
            RecordFeather(endpoint - side * kQFeatherSpread);
        }
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"xayahw", "deadly plumage"})) {
        LastWCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"xayahe", "bladecaller"})) {
        LastECastTick = now;
        for (auto& feather : Feathers) feather = {};
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"xayahr", "featherstorm"})) {
        LastRCastTick = now;
        if (!aim.IsValid() || aim.IsZero()) aim = origin + Vector3{kRRange, 0, 0};
        const Vector3 endpoint = ClampFeatherEndpoint(origin, aim, kRRange);
        const Vec3 direction = Direction2D(origin, endpoint);
        const Vec3 side{-direction.z, 0.0f, direction.x};
        if (!direction.IsZero()) {
            for (int index = -2; index <= 2; ++index) {
                RecordFeather(endpoint + side * (22.0f * index));
            }
        }
    }
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) ObserveLocalSpell(args);
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    int targetId = 0;
    int castTick = 0;
    if (!ControllerHelpers::CaptureLocalAutoAttack(args, targetId, castTick)) return;
    LastAfterAttackTargetId = targetId;
    LastAfterAttackTick = castTick;
    const auto target = HeroByNetworkId(targetId);
    if (target.IsValid()) RecordFeather(target.Position(), 0, castTick);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    // Never cancel an attack: E/W/Q are inserted by OnUpdate after the
    // orbwalker's windup gate and the selected/orbwalker target remains owned
    // by the orbwalker itself.
    if (Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true)) return;
    (void)args;
}


inline void OnGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    Vector3 endpoint{};
    (void)CaptureGapcloser(args, GapcloserTargetId, endpoint,
                           GapcloserExpireTick, 900.0f, 900);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() || !ObjectEventIsAllied(args) ||
        !FeatherObject(args)) return;
    RecordFeather(args.Sender.Position,
                  static_cast<int>(args.Sender.NetworkId), Now());
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (id != 0) RemoveFeather(id);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("XayahMechanics", "Xayah Mechanics"));
    FeatherMenu = TacticsMenu->AddSubMenu(new Menu("FeatherLogic", "Feather / Bladecaller"));
    FeatherMenu->Add(new MenuSlider("MinimumFeathers", "Minimum feathers to recall", 2, 1, 8));
    FeatherMenu->Add(new MenuSlider("RootThreshold", "Feathers required for root", 3, 3, 8));
    PostureMenu = TacticsMenu->AddSubMenu(new Menu("PostureLogic", "W / R posture safety"));
    PostureMenu->Add(new MenuSlider("DefensiveHealth", "W defensive health", 35, 10, 70));
    PostureMenu->Add(new MenuSlider("UltimateHealth", "R defensive health", 32, 10, 70));
    PostureMenu->Add(new MenuSlider("MinimumRTargets", "Minimum R line targets", 2, 1, 4));
}

inline void OnLoad() {
    Feathers = {};
    NextFeatherId = 1;
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    GapcloserTargetId = GapcloserExpireTick = PlayerOverrideUntil = 0;
    LastMode = Mode::None;
    LastSmartTarget = {};
}

inline void OnUnload() {
    Feathers = {};
    TacticsMenu = FeatherMenu = PostureMenu = nullptr;
    LastMode = Mode::None;
    LastSmartTarget = {};
}

inline constexpr const char* Scenarios[] = {
    "Reconcile Q, R, passive and object-created feathers without duplicate records",
    "Expire feathers after their live six-second lifetime and remove deleted objects",
    "Prefer the selected target while retaining a reachable orbwalker attack route",
    "Recall E only when a predicted target has enough feather pass-throughs",
    "Root at the configured three-feather threshold unless an execute is available",
    "Preserve an AA windup and never cancel a selected/orbwalker attack",
    "Use W only for an actual attack posture or defensive lethal window",
    "Use R for invulnerability, peel, lethal setup or a verified multi-target line",
    "Reject Q/R predicted lines blocked by collision or projectile terrain",
    "Reject nonlethal casts while the player is under an enemy turret",
    "Use E reactively against a tracked gapcloser and flee pursuer",
    "Use Q/E conservatively in lane clear, jungle and last-hit modes",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Xayah";
    controller.ControllerId = "champion.kuroaio.ai.xayah.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIXayah.md";
    controller.ImplementationSummary =
        "Event and polling feather reconciliation, predictive Q/E return-root targeting, "
        "AA-safe W posture, defensive R featherstorm, collision checks and turret safety.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &OnDoCast;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAfterAttackTargetId, &LastAfterAttackTick>;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Xayah
