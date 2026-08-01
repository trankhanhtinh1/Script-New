#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AIBrand.h"
#include "AIBrandGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Brand {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::PredictionAtLeast;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MissileEventIsLocal;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContains;

inline Menu* TacticsMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline std::array<int, 24> AblazeIds{};
inline std::array<int, 24> AblazeStacks{};
inline std::array<int, 24> AblazeExpiry{};
inline int PendingAblazeTargetId = 0;
inline int PendingAblazeUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastCastTick = 0;
inline std::array<int, 4> LastSlotCast{};
inline int RTargetId = 0;
inline int RMissileId = 0;
inline bool RActive = false;
inline int RCastTick = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserUntil = 0;
inline Vector3 GapcloserEndpoint{};
inline int InterruptTargetId = 0;
inline int InterruptUntil = 0;

inline int IndexFor(int networkId) {
    for (std::size_t i = 0; i < AblazeIds.size(); ++i)
        if (AblazeIds[i] == networkId) return static_cast<int>(i);
    for (std::size_t i = 0; i < AblazeIds.size(); ++i) {
        if (AblazeIds[i] == 0) { AblazeIds[i] = networkId; return static_cast<int>(i); }
    }
    return -1;
}

inline int StackCount(const AIHeroClient& target) {
    if (!target.IsValid()) return 0;
    const int id = static_cast<int>(target.NetworkId());
    const int index = IndexFor(id);
    if (index < 0) return target.HasBuff("BrandAblaze") ? 1 : 0;
    if (!target.HasBuff("BrandAblaze")) {
        AblazeStacks[static_cast<std::size_t>(index)] = 0;
        AblazeExpiry[static_cast<std::size_t>(index)] = 0;
    }
    return std::clamp(AblazeStacks[static_cast<std::size_t>(index)], 0, 3);
}

inline bool Ablaze(const AIHeroClient& target) {
    return target.IsValid() && target.HasBuff("BrandAblaze");
}

inline void AddStack(const AIHeroClient& target) {
    if (!target.IsValid()) return;
    const int index = IndexFor(static_cast<int>(target.NetworkId()));
    if (index < 0) return;
    AblazeStacks[static_cast<std::size_t>(index)] =
        NextAblazeStacks(AblazeStacks[static_cast<std::size_t>(index)]);
    AblazeExpiry[static_cast<std::size_t>(index)] = Now() + kAblazeDurationMs;
}

inline void ClearStacks(int networkId) {
    const int index = IndexFor(networkId);
    if (index >= 0) {
        AblazeStacks[static_cast<std::size_t>(index)] = 0;
        AblazeExpiry[static_cast<std::size_t>(index)] = 0;
    }
}
inline void ReconcileAblaze(int now) {
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!enemy.IsValid()) continue;
        const int index = IndexFor(static_cast<int>(enemy.NetworkId()));
        if (index < 0) continue;
        if (enemy.HasBuff("BrandAblaze")) {
            AblazeStacks[static_cast<std::size_t>(index)] =
                std::max(1, AblazeStacks[static_cast<std::size_t>(index)]);
            AblazeExpiry[static_cast<std::size_t>(index)] =
                std::max(AblazeExpiry[static_cast<std::size_t>(index)],
                         now + kAblazeDurationMs);
        } else {
            AblazeStacks[static_cast<std::size_t>(index)] = 0;
            AblazeExpiry[static_cast<std::size_t>(index)] = 0;
        }
    }
}


inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
           Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
           CastThrottleReady(slot, reactive);
}

inline bool CanAct(Mode mode, bool reactive) {
    (void)mode;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Engine::IsPlayerCrowdControlled(player)) return false;
    return reactive || !ControllerHelpers::PreserveAttack(false);
}

inline bool SafeCommit(const Vector3& position, bool lethal = false,
                       int maximumEnemies = 3) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !position.IsValid()) return false;
    if (!lethal && Engine::UnderEnemyTurret(position)) return false;
    if (!lethal && Engine::CountEnemiesAt(position, 700.0f) >
                         std::max(1, maximumEnemies)) return false;
    return player.HealthPercent() > Slider(TacticsMenu, "MinCommitHealth", 24);
}

inline float DamageFor(const AIHeroClient& target, int slot,
                       bool empowered = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = SpellRawDamage(slot, Engine::RuntimeSpells[slot]
        ? static_cast<int>(Engine::RuntimeSpells[slot]->Level()) : 0,
        player.AP(), empowered);
    return player.CalculateMagicDamage(target, raw);
}

inline float ComboDamage(const AIHeroClient& target, bool includeR = true) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    float damage = 0.0f;
    const int stacks = StackCount(target);
    damage += DamageFor(target, 1, stacks > 0);
    damage += DamageFor(target, 2);
    if (QStunAllowed(stacks, PendingAblazeTargetId == static_cast<int>(target.NetworkId()) &&
                    PendingAblazeUntil >= Now())) damage += DamageFor(target, 0);
    if (includeR) damage += DamageFor(target, 3);
    if (AblazeDetonationReady(stacks))
        damage += GameObjects::Player().CalculateMagicDamage(
            target, BlazeRawDamage(GameObjects::Player().Level(), target.MaxHealth(),
                                   GameObjects::Player().AP()));
    return damage;
}

inline bool LethalWithoutR(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) &&
        ComboDamage(target, false) >= target.Health() + target.AllShield();
}

inline bool PredictionAndCollision(int slot, const AIHeroClient& target,
                                   Vector3& aim, SDK::HitChance chance = SDK::HitChance::High) {
    if (!Engine::RuntimeSpells[slot] || !Engine::ValidEnemy(target)) return false;
    const auto prediction = Engine::RuntimeSpells[slot]->GetPrediction(target);
    aim = prediction.GetCastPosition();
    if (!aim.IsValid() || aim.IsZero() ||
        !PredictionAtLeast(prediction, chance) ||
        !prediction.CollisionObjects.empty()) return false;
    if (slot == 0 && !SearLineHits(GameObjects::Player().Position(), aim,
                                   PredictPosition(target, kCastDelay),
                                   target.BoundingRadius())) return false;
    return true;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() ||
        !Engine::ValidEnemy(target, kSearRange + 50.0f) ||
        !InRealReach(player.Position(), target.Position(), kSearRange,
                     player.BoundingRadius(), target.BoundingRadius()) ||
        !Ready(0, mode, reactive) || !CanAct(mode, reactive) ||
        IsCommonUntargetableOrImmune(target) ||
        (!reactive && Orbwalker::IsWindingUp() &&
         Bool(Engine::HumanMenu, "PreserveAttacks", true))) return false;
    const int stacks = StackCount(target);
    const bool pending = PendingAblazeTargetId == static_cast<int>(target.NetworkId()) &&
                         PendingAblazeUntil >= Now() + 35;
    if (!QStunAllowed(stacks, pending)) return false;
    Vector3 aim{};
    if (!PredictionAndCollision(0, target, aim, SDK::HitChance::High) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kSearWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastSlotCast[0] = LastCastTick = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() ||
        !Engine::ValidEnemy(target, kPillarRange + 50.0f) ||
        !InRealReach(player.Position(), target.Position(), kPillarRange,
                     player.BoundingRadius(), target.BoundingRadius()) ||
        !Ready(1, mode, reactive) || !CanAct(mode, reactive) ||
        (!reactive && Orbwalker::IsWindingUp() &&
         Bool(Engine::HumanMenu, "PreserveAttacks", true))) return false;
    const Vector3 aim = PredictPosition(target, kCastDelay);
    if (!PillarHits(aim, PredictPosition(target, kCastDelay), target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kPillarRadius)) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    PendingAblazeTargetId = static_cast<int>(target.NetworkId());
    PendingAblazeUntil = Now() + 850;
    LastSlotCast[1] = LastCastTick = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() ||
        !Engine::ValidEnemy(target, kConflagrationRange + 50.0f) ||
        !InRealReach(player.Position(), target.Position(), kConflagrationRange,
                     player.BoundingRadius(), target.BoundingRadius()) ||
        !Ready(2, mode, reactive) || !CanAct(mode, reactive) ||
        IsCommonUntargetableOrImmune(target) ||
        (!reactive && Orbwalker::IsWindingUp() &&
         Bool(Engine::HumanMenu, "PreserveAttacks", true))) return false;
    const bool spread = Ablaze(target) &&
        Engine::CountEnemiesAt(target.Position(), kConflagrationRadius) >= 2;
    const bool lethal = DamageFor(target, 2) +
        (AblazeDetonationReady(StackCount(target), false) ?
            GameObjects::Player().CalculateMagicDamage(target,
                BlazeRawDamage(GameObjects::Player().Level(), target.MaxHealth(),
                               GameObjects::Player().AP())) : 0.0f) >=
        target.Health() + target.AllShield();
    if (!spread && !lethal && mode == Mode::Harass && target.HealthPercent() > 68.0f) return false;
    if (!Engine::ControllerCastUnit(2, target)) return false;
    PendingAblazeTargetId = static_cast<int>(target.NetworkId());
    PendingAblazeUntil = Now() + 850;
    LastSlotCast[2] = LastCastTick = Now();
    return true;
}

inline std::vector<BounceCandidate> BounceCandidates() {
    std::vector<BounceCandidate> result;
    result.reserve(GameObjects::EnemyHeroes().size());
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1000.0f) || IsCommonUntargetableOrImmune(enemy)) continue;
        result.push_back({static_cast<int>(enemy.NetworkId()), enemy.Position(),
                          enemy.BoundingRadius(), StackCount(enemy), false, true});
    }
    return result;
}

inline bool CastR(const AIHeroClient& selected, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() ||
        !Engine::ValidEnemy(selected, kPyroclasmRange + 50.0f) ||
        !InRealReach(player.Position(), selected.Position(), kPyroclasmRange,
                     player.BoundingRadius(), selected.BoundingRadius()) ||
        !Ready(3, mode, reactive) || RActive ||
        IsCommonUntargetableOrImmune(selected)) return false;
    auto candidates = BounceCandidates();
    for (auto& candidate : candidates)
        candidate.IsSelected = candidate.NetworkId == static_cast<int>(selected.NetworkId());
    BounceCandidate first{static_cast<int>(selected.NetworkId()), selected.Position(),
                          selected.BoundingRadius(), StackCount(selected), true, true};
    const auto route = BuildBounceRoute(first, candidates,
        Slider(TacticsMenu, "MinimumBounces", 2));
    const bool lethal = ComboDamage(selected, true) >= selected.Health() + selected.AllShield();
    if (!route.Safe || !DetonationClusterSafe(selected.Position(),
            Engine::CountEnemiesAt(selected.Position(), kPyroclasmBounceRadius),
            Slider(TacticsMenu, "MaxRClusterEnemies", 3), lethal, true) ||
        (!lethal && !SafeCommit(selected.Position(), false,
                                Slider(TacticsMenu, "MaxCommitEnemies", 3)))) return false;
    if (!Engine::ControllerCastUnit(3, selected)) return false;
    RTargetId = static_cast<int>(selected.NetworkId());
    RCastTick = LastSlotCast[3] = LastCastTick = Now();
    RActive = true;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kSearRange + 50.0f)) return;
    if (CastR(target, Mode::Combo, false)) return;
    if (StackCount(target) == 0 && CastW(target, Mode::Combo, false)) return;
    if (CastE(target, Mode::Combo, false)) return;
    if (CastQ(target, Mode::Combo, false)) return;
    (void)CastW(target, Mode::Combo, false);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(TacticsMenu, "HarassMana", 42)) return;
    if (StackCount(target) > 0 && CastQ(target, Mode::Harass, false)) return;
    if (CastW(target, Mode::Harass, false)) return;
    (void)CastE(target, Mode::Harass, false);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(TacticsMenu, "ClearMana", 35)) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target, 500.0f) && Ablaze(target)) {
        (void)CastQ(target, Mode::Flee, true);
        return;
    }
    if (Engine::ValidEnemy(target, kPillarRange) &&
        Engine::CountEnemiesAt(GameObjects::Player().Position(), 600.0f) > 0)
        (void)CastW(target, Mode::Flee, true);
}

inline void Automatic(const AIHeroClient& target) {
    if (InterruptTargetId != 0 && InterruptUntil >= Now()) {
        const auto threat = HeroByNetworkId(InterruptTargetId);
        if (Engine::ValidEnemy(threat, kSearRange) && Ablaze(threat) &&
            CastQ(threat, Mode::Automatic, true)) return;
    }
    if (GapcloserTargetId != 0 && GapcloserUntil >= Now()) {
        const auto threat = HeroByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(threat, kSearRange) && CastQ(threat, Mode::Automatic, true)) return;
    }
    if (Engine::ValidEnemy(target, kPyroclasmRange)) (void)CastR(target, Mode::Automatic, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    const int now = Now();
    for (std::size_t i = 0; i < AblazeIds.size(); ++i) {
        if (AblazeIds[i] != 0 && AblazeExpiry[i] > 0 && AblazeExpiry[i] < now) {
            AblazeStacks[i] = 0; AblazeExpiry[i] = 0;
        }
    }
    if (PendingAblazeUntil < now) { PendingAblazeTargetId = 0; PendingAblazeUntil = 0; }
    if (RActive && now - RCastTick > 1800) RActive = false;
    ReconcileAblaze(now);
    if (GapcloserUntil < now) GapcloserTargetId = 0;
    if (InterruptUntil < now) InterruptTargetId = 0;
    const auto target = PreferredEnemyTarget(selected, kSearRange + 50.0f);
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
    TacticsMenu = root->AddSubMenu(new Menu("Brand tactics"));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 42, 0, 100));
    TacticsMenu->Add(new MenuSlider("ClearMana", "Clear mana percent", 35, 0, 100));
    TacticsMenu->Add(new MenuSlider("MinimumBounces", "Minimum Pyroclasm bounces", 2, 1, 5));
    TacticsMenu->Add(new MenuSlider("MaxRClusterEnemies", "Maximum enemies near R detonation", 3, 1, 5));
    TacticsMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies at commit", 3, 1, 5));
    TacticsMenu->Add(new MenuSlider("MinCommitHealth", "Minimum health to commit", 24, 1, 100));
    PassiveMenu = root->AddSubMenu(new Menu("Brand passive tracking"));
    PassiveMenu->Add(new MenuBool("RequireAblazeQ", "Require Blaze for Q stun", true));
}

inline void OnLoad() {
    AblazeIds.fill(0); AblazeStacks.fill(0); AblazeExpiry.fill(0);
    LastSlotCast.fill(0); PendingAblazeTargetId = PendingAblazeUntil = 0;
    LastAutoTargetId = LastAutoTick = RTargetId = RMissileId = 0;
    RActive = false; GapcloserTargetId = GapcloserUntil = InterruptTargetId = InterruptUntil = 0;
}
inline void OnUnload() { TacticsMenu = nullptr; PassiveMenu = nullptr; OnLoad(); }

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (Engine::WasControllerCast(slot)) LastSlotCast[std::clamp(slot, 0, 3)] = Now();
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && analysis.Enemy.IsValid()) {
        InterruptTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        InterruptUntil = std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
    }
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId); LastAutoTick = Now();
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (Engine::TextContains(args.BuffName, "brandablaze")) AddStack(HeroByNetworkId(static_cast<int>(args.Sender.NetworkId)));
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (args.Sender.IsValid() && Engine::TextContains(args.BuffName, "brandablaze"))
        ClearStacks(static_cast<int>(args.Sender.NetworkId));
}
inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (args.Sender.IsValid() && Engine::TextContains(args.BuffName, "brandablaze")) {
        const int id = static_cast<int>(args.Sender.NetworkId);
        const int index = IndexFor(id);
        if (index >= 0) AblazeExpiry[static_cast<std::size_t>(index)] = Now() + kAblazeDurationMs;
    }
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) { LastAutoTargetId = static_cast<int>(args.Target.NetworkId()); LastAutoTick = Now(); }
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) { LastAutoTargetId = static_cast<int>(args.Target.NetworkId()); LastAutoTick = Now(); }
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, GapcloserTargetId, GapcloserEndpoint, GapcloserUntil, 900, 1200);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, InterruptTargetId, InterruptUntil, 1050, 250, 6000);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args)) return;
    if (Engine::TextContains(args.SpellName, "BrandR") ||
        Engine::TextContains(args.MissileName, "BrandWildfire")) {
        RMissileId = args.MissileNetworkId != 0 ? static_cast<int>(args.MissileNetworkId) :
                     static_cast<int>(args.Sender.NetworkId);
        RActive = true;
    }
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = args.MissileNetworkId != 0 ? static_cast<int>(args.MissileNetworkId) :
                   static_cast<int>(args.Sender.NetworkId);
    if (id == RMissileId) { RMissileId = 0; RActive = false; }
}
inline void OnDraw() {}

inline constexpr const char* Scenarios[] = {
    "Ablaze stack lifecycle and passive detonation threshold",
    "Sear prediction, collision and conditional Ablaze stun",
    "Pillar of Flame delayed circle and projectile-wall safety",
    "Conflagration target selection and 315-radius spread",
    "Pyroclasm five-bounce Ablaze/cluster routing",
    "detonation damage, shield and lethal gates",
    "turret, enemy-count and low-health commit safety",
    "combo, harass, lane clear, jungle, last-hit, flee and automatic modes",
    "manual cast ownership, missile lifecycle and polling reconciliation",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Brand;
    controller.ControllerId = "champion.kuroaio.ai.brand.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIBrand.md";
    controller.ImplementationSummary =
        "Blaze stack tracking, conditional Sear stun, delayed pillar/spread geometry and safe Pyroclasm bounce routing.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Brand
