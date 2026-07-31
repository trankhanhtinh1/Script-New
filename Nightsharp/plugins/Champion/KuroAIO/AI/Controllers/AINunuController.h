#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "../Profiles/AINunu.h"
#include "AINunuGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Nunu {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HeroHasSmite;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::Ready;
using ControllerHelpers::RuntimeNameContains;
using ControllerHelpers::SelectJungleTarget;
using ControllerHelpers::SpellEnabled;

inline Menu* TacticsMenu = nullptr;
inline Menu* ObjectiveMenu = nullptr;
inline Menu* SnowballMenu = nullptr;
inline Menu* BarrageMenu = nullptr;
inline Menu* AbsoluteZeroMenu = nullptr;

inline std::array<int, 4> LastCastTick{};
inline int LastAttackTargetId = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline int IncomingInterruptUntil = 0;
inline int IncomingGapcloserUntil = 0;
inline bool WCharging = false;
inline int WChargeStartTick = 0;
inline int WTargetId = 0;
inline Vector3 WEndpoint{};
inline int EStacks = 0;
inline int ELastStackTick = 0;
inline int ERootUntil = 0;
inline int ETargetId = 0;
inline bool RChanneling = false;
inline int RChannelStartTick = 0;
inline int RChannelEndTick = 0;
inline int RTargetId = 0;
inline bool RPlayerOwned = false;

inline bool ReadyFor(int index, Mode mode, bool reactive = false) {
    return index >= 0 && index < 4 && Ready(index, mode) &&
           (reactive || LastCastTick[static_cast<std::size_t>(index)] + 45 <= Now());
}

inline float ConsumeDamage() {
    static constexpr float damage[] = {0.0f, 200.0f, 400.0f, 600.0f, 800.0f, 1000.0f, 1200.0f};
    const int rank = std::clamp(ControllerHelpers::SpellRank(0), 0, 6);
    return damage[rank];
}

inline float SmiteStyleDamage() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !HeroHasSmite(player)) return 0.0f;
    return 900.0f;
}

inline bool SafeEndpoint(const Vector3& endpoint,
                         int maximumEnemies,
                         bool allowTurret = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !endpoint.IsValid() || endpoint.IsZero() ||
        SDK::NavMesh::IsWall(endpoint)) return false;
    if (!allowTurret && Engine::UnderEnemyTurret(endpoint)) return false;
    if (Engine::CountEnemiesAt(endpoint, 575.0f) > maximumEnemies) return false;
    if (ControllerHelpers::HasReadyDashHazardAt(endpoint)) return false;
    return !ProjectileWallBlocksFromPlayer(endpoint, SnowballWidthForCharge(5.0f));
}

inline const AIMinionClient ObjectiveTarget() {
    return SelectJungleTarget(kConsumeRange + 120.0f, 0.20f, 100000.0f);
}

inline bool ObjectiveQAllowed(const AIMinionClient& monster) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !monster.IsValid() || monster.IsDead() ||
        !monster.IsTargetable() || player.Position().Distance2D(monster.Position()) >
            kConsumeRange + monster.BoundingRadius()) return false;
    const bool epic = IsEpicMonster(monster);
    const int enemies = Engine::CountEnemiesAt(monster.Position(), 900.0f);
    const float smite = SmiteStyleDamage();
    const bool secure = ConsumeSecure(ConsumeDamage(), monster.Health(), smite,
                                      epic, enemies > 0);
    const float objectiveHp = monster.MaxHealth() > 0.0f
        ? (monster.Health() / monster.MaxHealth()) * 100.0f : 100.0f;
    if (!ConsumeHealthy(player.HealthPercent(), objectiveHp, epic)) return false;
    if (epic && enemies > 0 && !secure) return false;
    if (!epic && monster.Health() > ConsumeDamage() && player.HealthPercent() > 42.0f) {
        return false;
    }
    return secure || !epic;
}

inline bool CastConsumeMonster(const AIMinionClient& monster, Mode mode,
                               bool reactive = false) {
    if (!monster.IsValid() || !ReadyFor(0, mode, reactive) ||
        !SpellEnabled(0, mode) || !ObjectiveQAllowed(monster) ||
        PlayerMobilityLocked() || !Engine::CanAct(reactive)) return false;
    if (!Engine::ControllerCastUnit(0, AIBaseClient(monster.Handle()))) return false;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastConsumeChampion(const AIHeroClient& target, Mode mode,
                                bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kConsumeRange) ||
        !ReadyFor(0, mode, reactive) || PlayerMobilityLocked() ||
        !Engine::CanAct(reactive)) return false;
    const float raw = 20.0f + 60.0f * std::clamp(
        static_cast<float>(ControllerHelpers::SpellRank(0)), 0.0f, 5.0f) +
        0.65f * player.AP() + 0.05f * player.BonusHealth();
    if (mode == Mode::Harass && raw < target.Health() + target.AllShield()) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    LastCastTick[0] = Now();
    return true;
}

inline Vector3 SnowballAim(const AIHeroClient& target, float chargeSeconds) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return {};
    const Vector3 predicted = PredictPosition(target,
        SnowballImpactSeconds(player.Position().Distance2D(target.Position()), chargeSeconds));
    if (!predicted.IsValid() || predicted.IsZero()) return {};
    const float distance = player.Position().Distance2D(predicted);
    if (distance < kSnowballMinimumDistance) {
        return Engine::Extend(player.Position(), predicted, kSnowballMinimumDistance);
    }
    return Engine::Extend(player.Position(), predicted,
                          std::min(distance, kSnowballMaximumDistance));
}

inline bool FinishSnowball(bool reactive = false) {
    if (!WCharging || !Engine::RuntimeSpells[1]) return false;
    const auto player = GameObjects::Player();
    const float elapsed = static_cast<float>(std::max(0, Now() - WChargeStartTick)) / 1000.0f;
    if (elapsed < 0.25f && !reactive) return false;
    if (!SafeEndpoint(WEndpoint, reactive ? 2 : 1)) {
        WCharging = false;
        WTargetId = 0;
        WEndpoint = {};
        return false;
    }
    Engine::ArmControllerCast(1);
    if (!Engine::RuntimeSpells[1]->ShootChargedSpell(WEndpoint)) {
        Engine::CancelControllerCast(1);
        return false;
    }
    Engine::MarkSuccessfulCast(1);
    LastCastTick[1] = Now();
    WCharging = false;
    WTargetId = 0;
    WEndpoint = {};
    (void)player;
    return true;
}

inline bool StartSnowball(const AIHeroClient& target, Mode mode,
                          bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, 2500.0f) ||
        !ReadyFor(1, mode, reactive) || PlayerMobilityLocked() ||
        !Engine::CanAct(reactive) ||
        (!reactive && Orbwalker::IsWindingUp() &&
         ControllerHelpers::Bool(Engine::HumanMenu, "PreserveAttacks", true))) return false;
    const float charge = std::clamp(
        player.Position().Distance2D(target.Position()) / 350.0f, 0.0f, 5.0f);
    const Vector3 endpoint = SnowballAim(target, charge);
    if (!SnowballPathValid(player.Position(), endpoint, target.BoundingRadius()) ||
        !SafeEndpoint(endpoint, reactive ? 2 : 1)) return false;
    const auto prediction = Engine::RuntimeSpells[1]->GetPrediction(target);
    if (!prediction.GetCastPosition().IsValid() ||
        !prediction.CollisionObjects.empty() ||
        ProjectileWallBlocksFromPlayer(endpoint, SnowballWidthForCharge(charge))) return false;
    Engine::ArmControllerCast(1);
    if (!Engine::RuntimeSpells[1]->StartCharging(endpoint)) {
        Engine::CancelControllerCast(1);
        return false;
    }
    Engine::MarkSuccessfulCast(1);
    WCharging = true;
    WChargeStartTick = Now();
    WTargetId = static_cast<int>(target.NetworkId());
    WEndpoint = endpoint;
    return true;
}

inline bool CastBarrage(const AIHeroClient& target, Mode mode,
                        bool reactive = false) {
    if (EStacks >= 3 && ERootUntil < Now()) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kSnowballTargetRange) ||
        !ReadyFor(2, mode, reactive) || PlayerMobilityLocked() ||
        !Engine::CanAct(reactive)) return false;
    if (!reactive && Orbwalker::IsWindingUp() &&
        ControllerHelpers::Bool(Engine::HumanMenu, "PreserveAttacks", true)) return false;
    const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    if (!prediction.GetCastPosition().IsValid() ||
        prediction.GetCastPosition().IsZero() || !prediction.CollisionObjects.empty() ||
        ProjectileWallBlocksFromPlayer(prediction.GetCastPosition(), 80.0f)) return false;
    const SDK::HitChance needed = (reactive || target.IsDashing() ||
        Engine::IsHardCrowdControlled(target)) ? SDK::HitChance::Medium : SDK::HitChance::High;
    if (static_cast<int>(prediction.Hitchance) < static_cast<int>(needed)) return false;
    if (!Engine::ControllerCastPredicted(2, target, needed)) return false;
    LastCastTick[2] = Now();
    ETargetId = static_cast<int>(target.NetworkId());
    EStacks = std::clamp(EStacks + 1, 1, 3);
    ELastStackTick = Now();
    if (EStacks >= 3) ERootUntil = Now() + 1000;
    return true;
}

inline bool AbsoluteZeroAllowed(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || RChanneling || PlayerMobilityLocked() ||
        Engine::UnderEnemyTurret(player.Position())) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), kAbsoluteZeroRadius);
    const bool rooted = ERootUntil >= Now() ||
        (Engine::ValidEnemy(target) && Engine::IsHardCrowdControlled(target));
    return AbsoluteZeroCommitAllowed(player.HealthPercent(), enemies, rooted,
                                     IncomingInterruptUntil >= Now(), false);
}

inline bool CastAbsoluteZero(const AIHeroClient& target, Mode mode,
                             bool reactive = false) {
    if (!ReadyFor(3, mode, reactive) || !Engine::CanAct(reactive) ||
        !AbsoluteZeroAllowed(target)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    RChanneling = true;
    RPlayerOwned = true;
    RChannelStartTick = Now();
    RChannelEndTick = Now() + 3000;
    RTargetId = target.IsValid() ? static_cast<int>(target.NetworkId()) : 0;
    return true;
}

inline bool ReleaseAbsoluteZero(bool reactive = true) {
    if (!RChanneling || !Engine::RuntimeSpells[3]) return false;
    if (!reactive && Now() < RChannelStartTick + 300) return false;
    Engine::ArmControllerCast(3);
    if (!Engine::RuntimeSpells[3]->Cast()) {
        Engine::CancelControllerCast(3);
        return false;
    }
    Engine::MarkSuccessfulCast(3);
    RChanneling = false;
    RPlayerOwned = false;
    RTargetId = 0;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, 2500.0f)) return;
    if (WCharging) {
        if (FinishSnowball(false)) return;
        return;
    }
    if (CastBarrage(target, Mode::Combo, false)) return;
    if (StartSnowball(target, Mode::Combo, false)) return;
    if (CastAbsoluteZero(target, Mode::Combo, false)) return;
    (void)CastConsumeChampion(target, Mode::Combo, false);
}

inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, 900.0f)) return;
    if (WCharging) {
        (void)FinishSnowball(false);
        return;
    }
    if (CastBarrage(target, Mode::Harass, false)) return;
    if (StartSnowball(target, Mode::Harass, false)) return;
    (void)CastConsumeChampion(target, Mode::Harass, false);
}

inline void Farm(Mode mode) {
    const auto monster = ObjectiveTarget();
    if (mode == Mode::Jungle && monster.IsValid() && IsEpicMonster(monster)) {
        if (CastConsumeMonster(monster, mode, false)) return;
        if (Engine::ValidEnemy(NearestEnemyToPlayer({}, 850.0f), 850.0f)) return;
        if (ReadyFor(0, mode, false)) return;
    }
    if (mode == Mode::Jungle && monster.IsValid() && CastConsumeMonster(monster, mode, false)) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& selected) {
    const AIHeroClient threat = NearestEnemyToPlayer(selected, 1000.0f);
    if (Engine::ValidEnemy(threat, kSnowballTargetRange) &&
        CastBarrage(threat, Mode::Flee, true)) return;
    if (WCharging) {
        (void)FinishSnowball(true);
        return;
    }
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PlayerMobilityLocked()) return;
    const Vector3 cursor = Game::CursorPos();
    if (FleeSnowballSafe(player.Position(), cursor,
                         SDK::NavMesh::IsWall(cursor), Engine::UnderEnemyTurret(cursor),
                         Engine::CountEnemiesAt(cursor, 575.0f), 1) &&
        threat.IsValid()) {
        (void)StartSnowball(threat, Mode::Flee, true);
        return;
    }
    if (Engine::ValidEnemy(threat, kAbsoluteZeroRadius) &&
        Engine::CountEnemiesAt(player.Position(), kAbsoluteZeroRadius) >= 2) {
        (void)CastAbsoluteZero(threat, Mode::Flee, true);
    }
}

inline void Automatic(const AIHeroClient& selected) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (RChanneling) {
        const AIHeroClient target = ControllerHelpers::HeroByNetworkId(RTargetId);
        const bool noTargets = Engine::CountEnemiesAt(player.Position(), kAbsoluteZeroRadius) <= 0;
        if (AbsoluteZeroReleaseNeeded(
                static_cast<float>(Now() - RChannelStartTick) / 1000.0f,
                IncomingInterruptUntil >= Now(), noTargets,
                Engine::UnderEnemyTurret(player.Position()))) {
            (void)ReleaseAbsoluteZero(true);
        }
        (void)target;
        return;
    }
    if (IncomingInterruptUntil >= Now()) {
        const AIHeroClient threat = ControllerHelpers::HeroByNetworkId(IncomingThreatTargetId);
        if (Engine::ValidEnemy(threat, kSnowballTargetRange) &&
            CastBarrage(threat, Mode::Automatic, true)) return;
    }
    if (player.HealthPercent() < 40.0f) {
        const AIHeroClient threat = NearestEnemyToPlayer(selected, 900.0f);
        if (Engine::ValidEnemy(threat, kAbsoluteZeroRadius)) {
            (void)CastAbsoluteZero(threat, Mode::Automatic, true);
        }
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    if (EStacks > 0 && !EStackWindowActive(EStacks, Now(), ELastStackTick)) {
        EStacks = 0;
        ETargetId = 0;
        ERootUntil = 0;
    }
    if (WCharging && Now() - WChargeStartTick > 6000) (void)FinishSnowball(true);
    if (RChanneling) {
        const auto player = GameObjects::Player();
        const bool noTargets = !player.IsValid() ||
            Engine::CountEnemiesAt(player.Position(), kAbsoluteZeroRadius) <= 0;
        if (Now() >= RChannelEndTick || AbsoluteZeroReleaseNeeded(
                static_cast<float>(Now() - RChannelStartTick) / 1000.0f,
                IncomingInterruptUntil >= Now(), noTargets,
                player.IsValid() && Engine::UnderEnemyTurret(player.Position()))) {
            (void)ReleaseAbsoluteZero(true);
        }
        return true;
    }
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(
        selected, kSnowballTargetRange);
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
    TacticsMenu = root->AddSubMenu(new Menu("Nunu tactics"));
    ObjectiveMenu = TacticsMenu->AddSubMenu(new Menu("Consume objectives"));
    ObjectiveMenu->Add(new MenuBool("SecureEpic", "Reserve Consume for epic objective secure", true));
    ObjectiveMenu->Add(new MenuBool("AllowContest", "Allow Q plus Smite contest window", true));
    SnowballMenu = TacticsMenu->AddSubMenu(new Menu("Snowball path"));
    SnowballMenu->Add(new MenuSlider("MaxEnemies", "Maximum enemies at W endpoint", 1, 0, 3));
    SnowballMenu->Add(new MenuBool("RespectWalls", "Reject wall-blocked snowballs", true));
    BarrageMenu = TacticsMenu->AddSubMenu(new Menu("Barrage stacks"));
    BarrageMenu->Add(new MenuSlider("RootStacks", "Snowball stacks before root", 3, 1, 3));
    AbsoluteZeroMenu = TacticsMenu->AddSubMenu(new Menu("Absolute Zero"));
    AbsoluteZeroMenu->Add(new MenuSlider("MinEnemies", "Minimum enemies for channel", 1, 1, 4));
    AbsoluteZeroMenu->Add(new MenuBool("InterruptOnThreat", "Release on interrupt threat", true));
}

inline void OnLoad() {
    LastCastTick.fill(0);
    LastAttackTargetId = 0;
    IncomingThreatUntil = IncomingThreatTargetId = IncomingInterruptUntil = IncomingGapcloserUntil = 0;
    WCharging = false;
    WChargeStartTick = 0;
    WTargetId = 0;
    WEndpoint = {};
    EStacks = 0;
    ELastStackTick = ERootUntil = ETargetId = 0;
    RChanneling = false;
    RChannelStartTick = RChannelEndTick = RTargetId = 0;
    RPlayerOwned = false;
}

inline void OnUnload() {
    TacticsMenu = ObjectiveMenu = SnowballMenu = BarrageMenu = AbsoluteZeroMenu = nullptr;
    OnLoad();
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        if (!Engine::WasControllerCast(static_cast<int>(args.Slot))) return;
        if (args.Slot == static_cast<int>(SDK::SpellSlot::W)) {
            WTargetId = static_cast<int>(args.TargetNetworkId);
        } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E)) {
            ELastStackTick = Now();
        } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R)) {
            RChanneling = true;
            RChannelStartTick = Now();
            RChannelEndTick = Now() + 3000;
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && analysis.Enemy.IsValid()) {
        IncomingThreatTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick,
                                       analysis.LineThreatUntilTick);
        if (analysis.LikelyHardCrowdControl) IncomingInterruptUntil = IncomingThreatUntil;
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAttackTargetId = static_cast<int>(args.TargetNetworkId);
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "nunuestackmarker") ||
        Engine::TextContains(args.BuffName, "nunuesnowballfightbuff")) {
        EStacks = std::clamp(EStacks + 1, 1, 3);
        ELastStackTick = Now();
        if (EStacks >= 3) ERootUntil = Now() + 1000;
    }
    if (Engine::TextContains(args.BuffName, "nunuabsolutezero") ||
        Engine::TextContains(args.BuffName, "nunur")) {
        RChanneling = true;
        RChannelStartTick = Now();
        RChannelEndTick = Now() + 3000;
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "nunuestackmarker") ||
        Engine::TextContains(args.BuffName, "nunuesnowballfightbuff")) {
        EStacks = 0;
        ERootUntil = 0;
    }
    if (Engine::TextContains(args.BuffName, "nunuabsolutezero") ||
        Engine::TextContains(args.BuffName, "nunur")) {
        RChanneling = false;
        RPlayerOwned = false;
    }
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "nunuabsolutezero") ||
        Engine::TextContains(args.BuffName, "nunur")) {
        RChanneling = args.EndTime > Game::Time();
        if (RChanneling) RChannelEndTick = Now() + 3000;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAttackTargetId = static_cast<int>(args.Target.NetworkId());
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAttackTargetId = static_cast<int>(args.Target.NetworkId());
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, IncomingThreatTargetId, WEndpoint,
                           IncomingGapcloserUntil, kAbsoluteZeroRadius, 1200);
    IncomingInterruptUntil = std::max(IncomingInterruptUntil, IncomingGapcloserUntil);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, IncomingThreatTargetId, IncomingInterruptUntil, 900, 250, 5000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() && Engine::TextContains(args.Sender.Name, "nunu") &&
        Engine::TextContains(args.Sender.Name, "snowball")) {
        WEndpoint = args.Sender.Position;
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (WEndpoint.IsValid() && args.Sender.IsValid() &&
        args.Sender.Position.Distance2D(WEndpoint) < 100.0f) {
        WEndpoint = {};
        WCharging = false;
    }
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    OnObjectCreate(args);
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    OnObjectDelete(args);
}

inline void OnDraw() {}

inline constexpr const char* Scenarios[] = {
    "Consume epic objective priority with Q plus Smite secure window",
    "Consume champion damage and missing-health sustain gate",
    "Snowball charge tier, path distance and wall/collision prediction",
    "Snowball endpoint turret, enemy-density and dash-hazard safety",
    "Snowball Barrage three-stack slow-to-root lifecycle",
    "Absolute Zero three-second slow/channel and early interrupt release",
    "jungle objective sequencing and non-epic sustain clear",
    "lane last-hit, harass mana and selected/orbwalker target policy",
    "flee Barrage peel and safe-cursor snowball escape",
    "manual cast, auto windup protection and event/poll reconciliation",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Nunu";
    controller.ControllerId = "champion.kuroaio.ai.nunu.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AINunu.md";
    controller.ImplementationSummary =
        "Consume objective secure sequencing, charged Snowball path prediction, E stack/root tracking and interrupt-safe Absolute Zero channeling.";
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
    controller.OnBuffUpdate = &OnBuffUpdate;
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

} // namespace Plugins::KuroAIO::AI::Controllers::Nunu
