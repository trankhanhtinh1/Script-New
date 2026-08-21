#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AINamiGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Nami {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MissileEventIsLocal;
using ControllerHelpers::Now;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContains;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<int, 4> CastTicks{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int ProtectedAllyId = 0;
inline int TargetedAllyThreatId = 0;
inline int TargetedAllyThreatUntil = 0;
inline int GapcloserTargetId = 0;
inline Vector3 GapcloserEndpoint = {};
inline int GapcloserUntil = 0;
inline int InterruptTargetId = 0;
inline int InterruptUntil = 0;
inline int QPendingUntil = 0;
inline int EEmpowerHits = 0;
inline int WPendingUntil = 0;
inline int EEmpowerUntil = 0;
inline int EEmpowerAllyId = 0;
inline int RActiveUntil = 0;
inline int RMissileId = 0;

inline int SpellRank(int slot) {
    if (slot < 0 || slot >= 4 || !Engine::RuntimeSpells[slot]) return 1;
    return std::clamp(Engine::RuntimeSpells[slot]->Level(), 1,
                      slot == 3 ? 3 : 5);
}

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
           Engine::RuntimeSpells[slot]->IsReady() &&
           SpellEnabled(slot, mode) && CastThrottleReady(slot, reactive);
}

inline bool HasMana(int slot, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const float reserve = reactive ? 0.0f :
        (mode == Mode::Harass ? Slider(WMenu, "ManaReserve", 80) :
         mode == Mode::LaneClear || mode == Mode::Jungle ?
             Slider(FarmMenu, "ManaReserve", 100) : 0.0f);
    return CurrentResource() >= SpellCost(slot) + reserve;
}

inline bool TargetBlocked(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           target.HasBuff("SivirE") || target.HasBuff("NocturneShroudofDarkness") ||
           target.HasBuff("MorganaE") || target.HasBuff("BansheesVeil") ||
           target.HasBuff("EdgeOfNight") || target.HasBuff("VladimirSanguinePool") ||
           target.HasBuff("FizzEIcon");
}

inline bool SafeCommit(const Vector3& position, bool lethal = false,
                       bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !position.IsValid() || SDK::NavMesh::IsWall(position)) return false;
    if (!fleeing && !lethal && Engine::UnderEnemyTurret(position)) return false;
    if (!fleeing && !lethal && Engine::CountEnemiesAt(position, 650.0f) >
        Slider(RMenu, "MaxCommitEnemies", 2)) return false;
    return fleeing || lethal || player.HealthPercent() >=
        Slider(TacticsMenu, "MinCommitHealth", 30);
}

inline AIHeroClient ProtectedAlly() {
    const auto player = GameObjects::Player();
    AIHeroClient ally = SelectProtectionAlly(1000.0f,
        TargetedAllyThreatId, TargetedAllyThreatUntil);
    if (Engine::ValidAlly(ally, 1000.0f)) {
        ProtectedAllyId = static_cast<int>(ally.NetworkId());
        return ally;
    }
    return player;
}

inline AIHeroClient AllyById(int networkId) {
    if (networkId <= 0) return {};
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (static_cast<int>(ally.NetworkId()) == networkId && Engine::ValidAlly(ally)) return ally;
    }
    return {};
}

inline bool AllyNeedsHeal(const AIHeroClient& ally) {
    return Engine::ValidAlly(ally) && ally.HealthPercent() <=
        Slider(WMenu, "HealBelow", 78);
}

inline bool AllyThreatened(const AIHeroClient& ally) {
    if (!Engine::ValidAlly(ally)) return false;
    if (static_cast<int>(ally.NetworkId()) == TargetedAllyThreatId &&
        TargetedAllyThreatUntil >= Now()) return true;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy, 900.0f) &&
            enemy.Position().Distance2D(ally.Position()) < 475.0f) return true;
    }
    return Engine::IsHardCrowdControlled(ally);
}

inline float WDamage(const AIHeroClient& target, int bounce = 0) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculateMagicDamage(target, WRawAmount(
        SpellRank(1), player.AP(), bounce, false));
}

inline float WHeal(const AIHeroClient& ally, int bounce = 0) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidAlly(ally)) return 0.0f;
    return WRawAmount(SpellRank(1), player.AP(), bounce, true);
}

inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculateMagicDamage(target,
        EEmpoweredDamage(SpellRank(2), player.AP()));
}

inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    static constexpr std::array<float, 4> base = {0.0f, 150.0f, 250.0f, 350.0f};
    const float raw = base[static_cast<std::size_t>(SpellRank(3))] + player.AP() * 0.60f;
    return player.CalculateMagicDamage(target, raw);
}

inline bool CanAct(bool reactive, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Engine::IsPlayerCrowdControlled(player)) return false;
    if (reactive || lethal) return true;
    return PreserveAaWindup(Orbwalker::IsWindingUp(), lethal, reactive) ||
           !Bool(Engine::HumanMenu, "PreserveAttacks", true);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) ||
        !Engine::ValidEnemy(target, kQRange + target.BoundingRadius()) ||
        !Ready(0, mode, reactive) || !HasMana(0, mode, reactive) ||
        !CanAct(reactive)) return false;
    auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    Vector3 aim = prediction.GetCastPosition();
    if (!aim.IsValid() || aim.IsZero()) aim = QPredictedCenter(
        player.Position(), PredictPosition(target, kQDelaySeconds), {}, kQDelaySeconds);
    const Vector3 predicted = PredictPosition(target, kQDelaySeconds);
    if (!WithinRange(player.Position(), aim, kQRange, target.BoundingRadius()) ||
        !BubbleHits(aim, predicted, target.BoundingRadius()) ||
        (!prediction.CollisionObjects.empty() && !reactive) ||
        (prediction.Hitchance < SDK::HitChance::High && !reactive) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQRadius)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    CastTicks[0] = Now();
    QPendingUntil = Now() + 1250;
    return true;
}

inline bool CastW(const AIHeroClient& enemy, const AIHeroClient& ally,
                  Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode, reactive) ||
        !HasMana(1, mode, reactive) || !CanAct(reactive)) return false;
    AIHeroClient target = {};
    bool healing = false;
    if (AllyNeedsHeal(ally) && Geometry::WCanBounce(player.Position(), ally.Position())) {
        target = ally; healing = true;
    } else if (Engine::ValidEnemy(enemy, kWRange + enemy.BoundingRadius()) &&
               !TargetBlocked(enemy)) {
        target = enemy;
    }
    if (!target.IsValid()) return false;
    if (!healing && Orbwalker::IsWindingUp() && !reactive &&
        WDamage(target, 0) < target.Health() + target.AllShield() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true)) return false;
    if (!Engine::ControllerCastUnit(1, target)) return false;
    CastTicks[1] = Now();
    WPendingUntil = Now() + 900;
    return true;
}

inline bool CastE(const AIHeroClient& preferred, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode, reactive) ||
        !HasMana(2, mode, reactive) || !CanAct(reactive)) return false;
    AIHeroClient ally = preferred;
    if (!Engine::ValidAlly(ally, kERange)) ally = ProtectedAlly();
    if (!Engine::ValidAlly(ally, kERange)) return false;
    if (!reactive && !AllyThreatened(ally) && mode == Mode::Harass &&
        !Bool(EMenu, "EmpowerHarass", true)) return false;
    if (!SafeAllyEmpower(player.Position(), ally.Position(), kERange,
                         AllyThreatened(ally), Engine::UnderEnemyTurret(ally.Position()))) return false;
    if (!Engine::ControllerCastUnit(2, ally)) return false;
    CastTicks[2] = Now();
    EEmpowerAllyId = static_cast<int>(ally.NetworkId());
    EEmpowerUntil = Now() + 6000;
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) ||
        !Engine::ValidEnemy(target, kRRange + target.BoundingRadius()) ||
        !Ready(3, mode, reactive) || !HasMana(3, mode, reactive) ||
        !CanAct(reactive)) return false;
    const Vector3 predicted = PredictPosition(target, RTravelSeconds(
        player.Position().Distance2D(target.Position())));
    const Vector3 endpoint = REndpoint(player.Position(), predicted - player.Position(),
                                       std::min(kRRange, player.Position().Distance2D(predicted)));
    if (!endpoint.IsValid() || SDK::NavMesh::IsWall(endpoint) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(endpoint, kRWidth * 0.5f)) return false;
    const auto nearest = NearestEnemyToPlayer(target, kRRange);
    if (nearest.IsValid() && static_cast<int>(nearest.NetworkId()) !=
        static_cast<int>(target.NetworkId()) &&
        !FirstCollisionOwnsTarget(player.Position(), endpoint, predicted,
                                  nearest.Position(), target.BoundingRadius())) return false;
    const bool lethal = RDamage(target) >= target.Health() + target.AllShield();
    if (!SafeCommit(endpoint, lethal, mode == Mode::Flee || reactive)) return false;
    if (!reactive && mode == Mode::Combo &&
        Engine::CountEnemiesAt(endpoint, 300.0f) < Slider(RMenu, "MinimumTargets", 2) &&
        !lethal) return false;
    if (!Engine::ControllerCastPosition(3, endpoint)) return false;
    CastTicks[3] = Now();
    RActiveUntil = Now() + 2600;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    const auto ally = ProtectedAlly();
    if (Engine::ValidEnemy(target, kRRange) && CastR(target, Mode::Combo)) return;
    if (Engine::ValidEnemy(target, kQRange) && CastQ(target, Mode::Combo)) return;
    if (CastE(ally, Mode::Combo)) return;
    (void)CastW(target, ally, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto ally = ProtectedAlly();
    if (Engine::ValidEnemy(target, kQRange) && CastQ(target, Mode::Harass)) return;
    (void)CastW(target, ally, Mode::Harass);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "MinimumMana", 42)) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    const auto ally = ProtectedAlly();
    if (CastE(ally, Mode::Flee, true)) return;
    if (CastW(target, ally, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target, kQRange)) (void)CastQ(target, Mode::Flee, true);
    else if (Engine::ValidEnemy(target, kRRange)) (void)CastR(target, Mode::Flee, true);
}

inline void Automatic(const AIHeroClient& target) {
    const auto ally = ProtectedAlly();
    if (GapcloserTargetId && GapcloserUntil >= Now()) {
        const auto threat = HeroByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(threat, kQRange) && CastQ(threat, Mode::Automatic, true)) return;
        if (Engine::ValidEnemy(threat, kRRange) && CastR(threat, Mode::Automatic, true)) return;
    }
    if (InterruptTargetId && InterruptUntil >= Now()) {
        const auto threat = HeroByNetworkId(InterruptTargetId);
        if (Engine::ValidEnemy(threat, kQRange) && CastQ(threat, Mode::Automatic, true)) return;
    }
    if (CastE(ally, Mode::Automatic, true)) return;
    if (CastW(target, ally, Mode::Automatic, true)) return;
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    const int now = Now();
    if (QPendingUntil && QPendingUntil < now) QPendingUntil = 0;
    if (WPendingUntil && WPendingUntil < now) WPendingUntil = 0;
    if (EEmpowerUntil && EEmpowerUntil < now) {
        EEmpowerUntil = 0; EEmpowerAllyId = 0; EEmpowerHits = 0;
    }
    if (RActiveUntil && RActiveUntil < now) { RActiveUntil = 0; RMissileId = 0; }
    if (GapcloserUntil < now) GapcloserTargetId = 0;
    if (InterruptUntil < now) InterruptTargetId = 0;
    const auto target = Engine::SelectTarget(kRRange);
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
    TacticsMenu = root->AddSubMenu(new Menu("Nami tactics"));
    TacticsMenu->Add(new MenuSlider("MinCommitHealth", "Minimum health to commit", 30, 1, 100));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Aqua Prison"));
    QMenu->Add(new MenuBool("RequireHighHitchance", "Require high hitchance", true));
    WMenu = TacticsMenu->AddSubMenu(new Menu("Ebb and Flow"));
    WMenu->Add(new MenuSlider("ManaReserve", "Mana reserve", 80, 0, 250));
    WMenu->Add(new MenuSlider("HealBelow", "Heal allies below percent", 78, 1, 100));
    EMenu = TacticsMenu->AddSubMenu(new Menu("Tidecaller's Blessing"));
    EMenu->Add(new MenuBool("EmpowerHarass", "Empower harass ally", true));
    RMenu = TacticsMenu->AddSubMenu(new Menu("Tidal Wave"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum combo wave targets", 2, 1, 5));
    RMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies at endpoint", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Nami farming"));
    FarmMenu->Add(new MenuSlider("MinimumMana", "Minimum mana", 42, 0, 100));
    FarmMenu->Add(new MenuSlider("ManaReserve", "Farm mana reserve", 100, 0, 300));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("Nami coach"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw spell ranges", false));
}

inline void OnLoad() {
    CastTicks.fill(0);
    LastAutoTargetId = LastAutoTick = ProtectedAllyId = 0;
    TargetedAllyThreatId = TargetedAllyThreatUntil = GapcloserTargetId = GapcloserUntil = 0;
    GapcloserEndpoint = {};
    InterruptTargetId = InterruptUntil = 0;
    QPendingUntil = WPendingUntil = EEmpowerUntil = EEmpowerAllyId = EEmpowerHits = 0;
    RActiveUntil = RMissileId = 0;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    OnLoad();
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        const int slot = args.Slot;
        if (slot >= 0 && slot < 4) {
            CastTicks[static_cast<std::size_t>(slot)] = Now();
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || !analysis.Enemy.IsValid()) return;
    const int enemyId = static_cast<int>(analysis.Enemy.NetworkId());
    const int until = std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
    InterruptTargetId = enemyId;
    InterruptUntil = until;
}
inline void ConsumeEmpowerAttack(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack || EEmpowerUntil <= Now() ||
        EEmpowerAllyId != static_cast<int>(GameObjects::Player().NetworkId())) return;
    EEmpowerHits = std::min(3, EEmpowerHits + 1);
    if (EEmpowerHits >= 3) { EEmpowerUntil = 0; EEmpowerAllyId = 0; }
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
        LastAutoTick = Now();
    }
    ConsumeEmpowerAttack(args);
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !Engine::TextContains(args.BuffName, "NamiE")) return;
    const AIHeroClient sender(args.Sender.Ptr);
    if (Engine::ValidAlly(sender)) {
        EEmpowerUntil = std::max(EEmpowerUntil, Now() + 6000);
        EEmpowerAllyId = static_cast<int>(args.Sender.NetworkId);
        EEmpowerHits = 0;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (args.Sender.IsValid() && Engine::TextContains(args.BuffName, "NamiE") &&
        static_cast<int>(args.Sender.NetworkId) == EEmpowerAllyId) {
        EEmpowerUntil = 0; EEmpowerAllyId = 0; EEmpowerHits = 0;
    }
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) {
        LastAutoTargetId = static_cast<int>(args.Target.NetworkId()); LastAutoTick = Now();
    }
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, GapcloserTargetId, GapcloserEndpoint,
                            GapcloserUntil, 900, 1200);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, InterruptTargetId, InterruptUntil, 1050, 250, 6000);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (MissileEventIsLocal(args) && Engine::TextContains(args.SpellName, "NamiR")) {
        RMissileId = args.MissileNetworkId ? static_cast<int>(args.MissileNetworkId) :
            static_cast<int>(args.Sender.NetworkId);
        RActiveUntil = Now() + 2600;
    }
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = args.MissileNetworkId ? static_cast<int>(args.MissileNetworkId) :
        static_cast<int>(args.Sender.NetworkId);
    if (id == RMissileId) { RMissileId = 0; RActiveUntil = 0; }
}
inline void OnDraw() {}

inline constexpr const char* Scenarios[] = {
    "Tidecaller ally movement empower is reconciled from E buff events and polling",
    "Aqua Prison predicts moving targets with cast delay, projectile wall and collision safety",
    "Ebb and Flow chooses heal-first or damage-first bounce routes with 3-target scaling",
    "Tidecaller's Blessing selects a threatened or attacking ally and tracks three hits",
    "Tidal Wave projects a 2550-range 325-width wave with first-collision ownership",
    "Tidal Wave endpoint rejects walls, turrets and unsafe enemy piles except peel or lethal",
    "resource, cooldown, damage, healing and shield gates protect every cast",
    "Use autonomous Engine target selection for combat decisions",
    "orbwalker AA windup is preserved unless lethal or reactive peel is required",
    "Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic have distinct policies",
    "Events and polling reconcile buffs, threats and R missile lifecycle",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Nami;
    controller.ControllerId = "champion.kuroaio.ai.nami.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AINami.md";
    controller.ImplementationSummary =
        "Tidecaller-aware bubble prediction, alternating W bounces, ally E empower and safe Tidal Wave peel/engage.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Nami
