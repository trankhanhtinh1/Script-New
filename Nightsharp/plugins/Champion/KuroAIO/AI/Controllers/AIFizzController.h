#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIFizzGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Fizz {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Lethal;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline std::array<int, 4> LastCastTick{};
inline int ManualOwnershipUntil = 0;
inline int LastAutoTargetId = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline Vector3 IncomingThreatEndpoint{};
inline EState EPhase{};
inline Vector3 EFirstEndpoint{};
inline int WArmedUntil = 0;
inline int WTargetId = 0;
inline int RTargetId = 0;
inline int RImpactTick = 0;
inline SharkSize RSize = SharkSize::Small;
inline float RTravelDistance = 0.0f;

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && ControllerHelpers::SpellEnabled(slot, mode) &&
        (reactive || LastCastTick[static_cast<std::size_t>(slot)] + 45 <= Now());
}

inline bool HasManaFor(int slot, float reserve = 0.0f) {
    return ControllerHelpers::CurrentResource() + 0.5f >= SpellCost(slot) + std::max(0.0f, reserve);
}

inline bool PreserveAttack(bool reactive, bool lethal = false) {
    return !reactive && !lethal && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true);
}

inline bool TargetBlocked(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || IsCommonUntargetableOrImmune(target) ||
        target.IsInvulnerable() || target.HasBuff("FizzE") || target.HasBuff("FizzEIcon") ||
        target.HasBuff("VladimirSanguinePool") || target.HasBuff("BansheesVeil") ||
        target.HasBuff("EdgeOfNight") || target.HasBuff("KayleR");
}

inline void ReconcileState() {
    const int now = Now();
    if (EWindowExpired(EPhase, now)) {
        EPhase = {};
        EFirstEndpoint = {};
    }
    if (WArmedUntil <= now) {
        WArmedUntil = 0;
        WTargetId = 0;
    }
    if (RImpactTick > 0 && RImpactTick <= now) {
        RImpactTick = 0;
        RTargetId = 0;
        RTravelDistance = 0.0f;
    }
    if (IncomingThreatUntil <= now) {
        IncomingThreatUntil = 0;
        IncomingThreatTargetId = 0;
        IncomingThreatEndpoint = {};
    }
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.HasBuff("FizzE") || player.HasBuff("FizzEIcon")) {
        if (!EPhase.OnPole) {
            EPhase.OnPole = true;
            EPhase.HopStartTick = now;
        }
        EPhase.RecastExpireTick = std::max(EPhase.RecastExpireTick, now + 80);
        EPhase.UntargetableExpireTick = std::max(EPhase.UntargetableExpireTick, now + 50);
    }
    if (player.HasBuff("FizzWActive") || player.HasBuff("FizzW"))
        WArmedUntil = std::max(WArmedUntil, now + 800);
}

inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, QRawDamage(SpellRank(0), player.AP(),
            player.TotalAttackDamage())) : 0.0f;
}
inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, WPassiveDamage(SpellRank(1), player.AP()) +
            WActiveDamage(SpellRank(1), player.AP())) : 0.0f;
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, ERawDamage(SpellRank(2), player.AP())) : 0.0f;
}
inline float RDamage(const AIHeroClient& target, float distance = -1.0f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float travel = distance >= 0.0f ? distance : player.Position().Distance2D(target.Position());
    return player.CalculateMagicDamage(target, SharkDamage(SpellRank(3), player.AP(), travel));
}

inline std::vector<RBody> BuildRChampions(float delaySeconds) {
    std::vector<RBody> bodies;
    bodies.reserve(GameObjects::EnemyHeroes().size());
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) || IsCommonUntargetableOrImmune(enemy)) continue;
        bodies.push_back({PredictPosition(enemy, delaySeconds),
            std::max(25.0f, enemy.BoundingRadius()), static_cast<int>(enemy.NetworkId()), true,
            enemy.IsTargetable() && !enemy.IsInvulnerable()});
    }
    return bodies;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) ||
        player.Position().Distance2D(target.Position()) > kQRange + target.BoundingRadius() ||
        !Ready(0, mode, reactive) || PreserveAttack(reactive, lethal) ||
        !HasManaFor(0, lethal ? 0.0f : Slider(TacticsMenu, "ManaReserve", 20))) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) ||
        player.Position().Distance2D(target.Position()) > kWRange + target.BoundingRadius() ||
        !Ready(1, mode, reactive) || !HasManaFor(1, lethal ? 0.0f : 8.0f)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    WArmedUntil = Now() + 900;
    WTargetId = static_cast<int>(target.NetworkId());
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool defensive = false, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode, reactive) || !HasManaFor(2, defensive ? 0.0f : 10.0f))
        return false;
    const bool recast = ERecastAvailable(EPhase, Now());
    if (!recast && PreserveAttack(reactive, lethal)) return false;
    Vector3 requested{};
    if (defensive || mode == Mode::Flee) requested = Game::CursorPos();
    else if (Engine::ValidEnemy(target, kERange + target.BoundingRadius()))
        requested = PredictPosition(target, kEDelay);
    else requested = Game::CursorPos();
    const Vector3 endpoint = EReturnEndpoint(player.Position(), requested);
    if (endpoint.IsZero()) return false;
    const int enemies = Engine::CountEnemiesAt(endpoint, kEImpactRadius);
    const bool underTurret = Engine::UnderEnemyTurret(endpoint);
    if (!SafeEEndpoint({true, SDK::NavMesh::IsWall(endpoint), underTurret,
                        Engine::UnderEnemyTurret(player.Position()), defensive, lethal, false,
                        reactive && IncomingThreatUntil > Now(), enemies,
                        Slider(TacticsMenu, "MaxEEnemies", 2)})) return false;
    if (!recast && ControllerHelpers::ProjectileWallBlocksFromPlayer(endpoint, 25.0f)) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    LastCastTick[2] = Now();
    if (recast) {
        EPhase = {};
        EFirstEndpoint = {};
    } else {
        EPhase = {true, Now(), Now() + kERecastWindowMs, Now() + kEUntargetableMs};
        EFirstEndpoint = endpoint;
    }
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) || !Engine::ValidEnemy(target, kRRange) ||
        !Ready(3, mode, reactive) || PreserveAttack(reactive, lethal) ||
        !HasManaFor(3, lethal ? 0.0f : Slider(UltimateMenu, "ManaReserve", 15))) return false;
    const auto prediction = Engine::RuntimeSpells[3]->GetPrediction(target);
    const Vector3 aim = prediction.GetCastPosition().IsValid() && !prediction.GetCastPosition().IsZero()
        ? prediction.GetCastPosition() : PredictPosition(target, kRDelay);
    if (aim.IsZero() || player.Position().Distance2D(aim) > kRRange + target.BoundingRadius() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, SharkRadius(player.Position().Distance2D(aim)))) return false;
    const Vec3 direction = Direction2D(player.Position(), aim);
    if (direction.IsZero()) return false;
    const auto bodies = BuildRChampions(kRDelay);
    const int first = FirstRChampionCollisionIndex(player.Position(), direction, bodies,
                                                   kRRange, SharkRadius(player.Position().Distance2D(aim)));
    const int intended = static_cast<int>(target.NetworkId());
    if (first < 0 || bodies[static_cast<std::size_t>(first)].Id != intended) return false;
    const float distance = player.Position().Distance2D(bodies[static_cast<std::size_t>(first)].Position);
    const float damage = RDamage(target, distance);
    if (!lethal && mode == Mode::Harass && target.HealthPercent() >
        Slider(UltimateMenu, "HarassTargetHP", 70) && damage < target.Health() * 0.22f) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    LastCastTick[3] = Now();
    RTargetId = intended;
    RTravelDistance = distance;
    RSize = SharkSizeForDistance(distance);
    RImpactTick = Now() + static_cast<int>(RTravelSeconds(distance) * 1000.0f);
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastR(target, Mode::Combo, false, Lethal(target, RDamage(target)))) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastE(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || GameObjects::Player().ManaPercent() <
        Slider(TacticsMenu, "HarassMana", 55)) return;
    if (CastW(target, Mode::Harass)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastE(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (ERecastAvailable(EPhase, Now())) {
        (void)CastE(target, Mode::Flee, true, true);
        return;
    }
    if (IncomingThreatUntil > Now()) (void)CastE(target, Mode::Flee, true, true);
    else if (Engine::ValidEnemy(target, kQRange)) (void)CastQ(target, Mode::Flee, true);
}
inline void Automatic(const AIHeroClient& target) {
    if (IncomingThreatUntil > Now()) {
        (void)CastE(target, Mode::Automatic, true, true);
        return;
    }
    if (Engine::ValidEnemy(target) && Lethal(target, RDamage(target)))
        (void)CastR(target, Mode::Automatic, true, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected,
        mode == Mode::Flee ? 900.0f : kRRange);
    if (ERecastAvailable(EPhase, Now()) &&
        (!EUntargetable(EPhase, Now()) || mode == Mode::Flee ||
         IncomingThreatUntil > Now())) {
        (void)CastE(target, mode, mode == Mode::Automatic || mode == Mode::Flee,
                    mode == Mode::Flee || IncomingThreatUntil > Now());
        return true;
    }
    else if (mode == Mode::Automatic) Automatic(target);
    else if (mode == Mode::Combo) Combo(target);
    else if (mode == Mode::Harass) Harass(target);
    else if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) {
        if (GameObjects::Player().ManaPercent() >= Slider(FarmMenu,
            mode == Mode::Jungle ? "JungleMana" : "LaneMana", 40)) (void)Engine::TryFarm(mode);
    }
    return true;
}

inline void OnLoad() {
    TacticsMenu = nullptr; FarmMenu = nullptr; UltimateMenu = nullptr;
    LastCastTick = {}; ManualOwnershipUntil = LastAutoTargetId = 0;
    IncomingThreatUntil = IncomingThreatTargetId = 0; IncomingThreatEndpoint = {};
    EPhase = {}; EFirstEndpoint = {}; WArmedUntil = WTargetId = 0;
    RTargetId = RImpactTick = 0; RSize = SharkSize::Small; RTravelDistance = 0.0f;
}
inline void OnUnload() { OnLoad(); }

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            LastCastTick[static_cast<std::size_t>(slot)] = Now();
            if (!Engine::WasControllerCast(slot))
                ManualOwnershipUntil = Now() + Slider(TacticsMenu, "ManualOwnershipMs", 560);
            if (slot == 1) WArmedUntil = Now() + 900;
            if (slot == 2 && !EPhase.OnPole)
                EPhase = {true, Now(), Now() + kERecastWindowMs, Now() + kEUntargetableMs};
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args, 220.0f, 110.0f, 250, 260, 260, 1500, 450);
    if (analysis.Valid && analysis.Enemy.IsValid()) {
        IncomingThreatTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
    }
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack && args.Target.IsValid())
        LastAutoTargetId = static_cast<int>(args.Target.NetworkId);
    if (IsLocalPlayer(args.Sender) && args.Slot == static_cast<int>(SDK::SpellSlot::W))
        WArmedUntil = Now() + 250;
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "FizzE") || Engine::TextContains(args.BuffName, "FizzEIcon"))
            EPhase = {true, Now(), Now() + kERecastWindowMs, Now() + kEUntargetableMs};
        if (Engine::TextContains(args.BuffName, "FizzW")) WArmedUntil = Now() + 900;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "FizzE") || Engine::TextContains(args.BuffName, "FizzEIcon"))
        EPhase = {};
    if (Engine::TextContains(args.BuffName, "FizzW")) WArmedUntil = 0;
}
inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (args.EndTime <= Game::Time()) OnBuffRemove(args); else OnBuffAdd(args);
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) {
        LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
        if (WTargetId == LastAutoTargetId) { WArmedUntil = 0; WTargetId = 0; }
    }
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, IncomingThreatTargetId, IncomingThreatEndpoint, IncomingThreatUntil,
                           900.0f, 1100);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    int interruptTarget = 0;
    int interruptUntil = 0;
    CaptureInterruptable(args, interruptTarget, interruptUntil, 900, 250, 5000);
    if (interruptUntil > Now()) {
        IncomingThreatTargetId = interruptTarget;
        IncomingThreatUntil = interruptUntil;
    }
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() && Engine::TextContains(args.Sender.Name, "FizzRMissile"))
        RImpactTick = Now() + static_cast<int>(RTravelSeconds(RTravelDistance) * 1000.0f);
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() && Engine::TextContains(args.Sender.Name, "FizzRMissile"))
        RImpactTick = 0;
}
inline void OnDraw() {}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("FizzTactics", "Urchin Strike, Trident and Trickster policy"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 560, 150, 1200));
    TacticsMenu->Add(new MenuSlider("ManaReserve", "Mana reserve percent", 20, 0, 80));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 55, 0, 100));
    TacticsMenu->Add(new MenuSlider("MaxEEnemies", "Maximum enemies at E endpoint", 2, 1, 5));
    FarmMenu = root->AddSubMenu(new Menu("FizzFarm", "Lane and jungle farming"));
    FarmMenu->Add(new MenuSlider("LaneMana", "Lane clear mana percent", 40, 0, 100));
    FarmMenu->Add(new MenuSlider("JungleMana", "Jungle mana percent", 30, 0, 100));
    UltimateMenu = root->AddSubMenu(new Menu("FizzUltimate", "Chum projectile collision and shark size"));
    UltimateMenu->Add(new MenuSlider("ManaReserve", "R mana reserve", 15, 0, 80));
    UltimateMenu->Add(new MenuSlider("HarassTargetHP", "Harass R target health percent", 70, 1, 100));
}

inline constexpr const char* Scenarios[] = {
    "Q Urchin Strike targeted dash with mana, prediction target precedence and attack-windup ownership",
    "W Seastone Trident active reset, empowered next attack and buff/attack polling reconciliation",
    "E Playful first hop, 750ms untargetable window, recast timing and explicit return endpoint",
    "E evasive endpoint wall, turret and nearby-enemy safety with threat-gated defensive escape",
    "R Chum projectile prediction, first enemy-champion collision and wall rejection",
    "R shark travel distance size tiers, impact clock, collision target and shield-aware lethal gate",
    "Combo Q-W-E weave, Harass poke, lane/jungle/LastHit farm and Flee Trickster route",
    "Automatic gapcloser/interrupt threat response and manual spell ownership handoff",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Fizz";
    controller.ControllerId = "champion.kuroaio.ai.fizz.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIFizz.md";
    controller.ImplementationSummary =
        "Fizz-specific Q dash and W reset state, E timed untargetability/recast endpoint safety, and first-champion R shark size/collision planning.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Fizz
