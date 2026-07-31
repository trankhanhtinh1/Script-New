#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIShenGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>

namespace Plugins::KuroAIO::AI::Controllers::Shen {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MissileEventIsLocal;
using ControllerHelpers::Now;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContains;
using ControllerHelpers::SpellCost;
using ControllerHelpers::CurrentResource;

inline Menu* TacticsMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* BladeMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline Menu* FarmMenu = nullptr;

inline bool PassiveReady = false;
inline float PassiveShieldAmount = 0.0f;
inline int PassiveShieldUntil = 0;
inline int BladeTargetId = 0;
inline Vector3 SpiritBlade = {};
inline int BladeLastSeenTick = 0;
inline bool WActive = false;
inline Vector3 WCenter = {};
inline int WExpireTick = 0;
inline int ECastTick = 0;
inline Vector3 EEndpoint = {};
inline int ETargetId = 0;
inline bool RChannelActive = false;
inline int RAllyId = 0;
inline int RChannelUntil = 0;
inline bool RInterrupted = false;
inline int GapcloserTargetId = 0;
inline Vector3 GapcloserEndpoint = {};
inline int GapcloserUntil = 0;
inline int InterruptTargetId = 0;
inline int InterruptUntil = 0;
inline int TargetedAllyId = 0;
inline int TargetedAllyUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline std::array<int, 4> LastCastTick{};

inline constexpr float kQRange = 475.0f;
inline constexpr float kWRadius = 60.0f;
inline constexpr float kERange = 600.0f;
inline constexpr int kBladeGraceMs = 1600;
inline constexpr int kRChannelMs = 3000;

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
           Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
           CastThrottleReady(slot, reactive ? 12 : 48);
}

inline bool CanAct(bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Engine::IsPlayerCrowdControlled(player)) return false;
    return reactive || !ControllerHelpers::PreserveAttack(false);
}

inline bool SafeCommit(const Vector3& position, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !position.IsValid()) return false;
    const int maximum = ControllerHelpers::Slider(TacticsMenu, "MaxCommitEnemies", 3);
    if (!lethal && Engine::UnderEnemyTurret(position)) return false;
    if (!lethal && Engine::CountEnemiesAt(position, 650.0f) > maximum) return false;
    return lethal || player.HealthPercent() >=
        static_cast<float>(ControllerHelpers::Slider(TacticsMenu, "MinCommitHealth", 30));
}

inline AIHeroClient ProtectedAlly() {
    return SelectProtectionAlly(25000.0f, TargetedAllyId, TargetedAllyUntil,
                                300.0f, 850.0f);
}

inline bool AllyNeedsStandUnited(const AIHeroClient& ally) {
    if (!Engine::ValidAlly(ally)) return false;
    const float threat = static_cast<float>(Engine::CountEnemiesAt(ally.Position(), 700.0f));
    return ally.HealthPercent() <= ControllerHelpers::Slider(UltimateMenu, "AllyHealth", 58) ||
           threat >= 2 || static_cast<int>(ally.NetworkId()) == TargetedAllyId;
}

inline bool QTargetReachable(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target, kQRange + 55.0f) &&
           CanReachTarget(player.Position(), target.Position(), kQRange,
                          target.BoundingRadius());
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !QTargetReachable(target) || !Ready(0, mode, reactive) ||
        !CanAct(reactive) || IsCommonUntargetableOrImmune(target)) return false;
    if (!reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true)) return false;
    if (CurrentResource() + 0.5f < SpellCost(0)) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    BladeTargetId = static_cast<int>(target.NetworkId());
    SpiritBlade = SpiritBladePosition(player.Position(), target.Position());
    BladeLastSeenTick = Now();
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode, reactive) || !CanAct(reactive) ||
        CurrentResource() + 0.5f < SpellCost(1)) return false;
    Vector3 center = SpiritBlade.IsValid() && Now() - BladeLastSeenTick <= kBladeGraceMs
        ? SpiritBlade : player.Position();
    const bool threatened = player.HealthPercent() <=
        ControllerHelpers::Slider(PassiveMenu, "ShieldHealth", 72) ||
        Engine::CountEnemiesAt(center, 220.0f) > 0 || Engine::ValidEnemy(target, 350.0f);
    if (!reactive && !threatened) return false;
    if (!Engine::ControllerCastPosition(1, center)) return false;
    WActive = true;
    WCenter = center;
    WExpireTick = Now() + ControllerHelpers::Slider(TacticsMenu, "WDuration", 1800);
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kERange + 60.0f) ||
        !Ready(2, mode, reactive) || !CanAct(reactive) ||
        CurrentResource() + 0.5f < SpellCost(2) ||
        IsCommonUntargetableOrImmune(target)) return false;
    const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    const Vector3 aim = prediction.GetCastPosition();
    if (!aim.IsValid() || aim.IsZero() || prediction.Hitchance < SDK::HitChance::Medium ||
        !prediction.CollisionObjects.empty() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 55.0f)) return false;
    const Vector3 endpoint = DashEndpoint(player.Position(), aim, kERange);
    const bool lethal = SDK::Damage::GetSpellDamage(
        player, target, SDK::SpellSlot::E, SDK::DamageStage::Default) >=
        target.Health() + target.AllShield();
    if (!DashEndpointSafe(endpoint, Engine::UnderEnemyTurret(endpoint),
                          Engine::CountEnemiesAt(endpoint, 650.0f),
                          ControllerHelpers::Slider(TacticsMenu, "MaxDashEnemies", 2),
                          player.HealthPercent(),
                          static_cast<float>(ControllerHelpers::Slider(TacticsMenu, "MinDashHealth", 28)),
                          lethal)) return false;
    if (!Engine::ControllerCastVector(2, player.Position(), endpoint)) return false;
    ECastTick = LastCastTick[2] = Now();
    EEndpoint = endpoint;
    ETargetId = static_cast<int>(target.NetworkId());
    return true;
}

inline bool CastR(const AIHeroClient& ally, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidAlly(ally) || !Ready(3, mode, reactive) ||
        !CanAct(reactive) || RChannelActive || RInterrupted ||
        CurrentResource() + 0.5f < SpellCost(3)) return false;
    if (!AllyNeedsStandUnited(ally) && !reactive) return false;
    if (Engine::IsPlayerCrowdControlled(player)) return false;
    if (!Engine::ControllerCastUnit(3, ally)) return false;
    RChannelActive = true;
    RAllyId = static_cast<int>(ally.NetworkId());
    RChannelUntil = Now() + kRChannelMs;
    RInterrupted = false;
    LastCastTick[3] = Now();
    return true;
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    const bool passiveBuff = player.HasBuff("ShenPassiveShield") ||
        player.HasBuff("ShenPassiveShieldMarker") || player.HasBuff("ShenPShield");
    PassiveReady = passiveBuff || (PassiveReady && now <= PassiveShieldUntil);
    if (PassiveReady && PassiveShieldUntil == 0) PassiveShieldUntil = now + 4000;
    if (!passiveBuff && now > PassiveShieldUntil) {
        PassiveReady = false;
        PassiveShieldAmount = 0.0f;
    }
    if (passiveBuff && PassiveShieldAmount <= 0.0f) {
        PassiveShieldAmount = PassiveShield(player.Level(), player.BonusHealth()).Amount;
    }
    WActive = WActive && (player.HasBuff("ShenW") || now <= WExpireTick);
    if (now > WExpireTick) { WActive = false; WCenter = {}; }
    if (BladeTargetId != 0 && now - BladeLastSeenTick > kBladeGraceMs) {
        BladeTargetId = 0; SpiritBlade = {};
    }
    if (RChannelActive) {
        if (now >= RChannelUntil || Engine::IsPlayerCrowdControlled(player)) {
            RChannelActive = false;
            if (Engine::IsPlayerCrowdControlled(player)) RInterrupted = true;
        } else {
            const auto ally = HeroByNetworkId(RAllyId);
            const auto decision = EvaluateTeleport(
                ally.IsValid() ? ally.HealthPercent() : 100.0f,
                0.0f,
                static_cast<float>(RChannelUntil - now) / 1000.0f,
                false,
                ally.IsValid() && Engine::CountEnemiesAt(ally.Position(), 700.0f) >= 3,
                false);
            if (decision == TeleportDecision::Interrupt) RInterrupted = true;
        }
    }
    if (now > GapcloserUntil) GapcloserTargetId = 0;
    if (now > InterruptUntil) InterruptTargetId = 0;
    if (now > TargetedAllyUntil) TargetedAllyId = 0;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kERange + 50.0f)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    (void)CastW(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Engine::ManaPercent(player) <
        ControllerHelpers::Slider(TacticsMenu, "HarassEnergy", 40)) return;
    if (Bool(TacticsMenu, "HarassUseE", false) &&
        CastE(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Engine::ManaPercent(player) <
        ControllerHelpers::Slider(FarmMenu, "ClearEnergy", 35)) return;
    if (mode == Mode::Jungle &&
        Bool(TacticsMenu, "UseObjectiveW", true) &&
        Engine::CountEnemiesAt(player.Position(), 500.0f) > 0 &&
        CastW(AIHeroClient{}, mode, true)) return;
    (void)Engine::TryFarm(mode);
}

inline void LastHit(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target, kQRange) && CastQ(target, Mode::LastHit)) return;
    (void)Engine::TryFarm(Mode::LastHit);
}

inline void Flee(const AIHeroClient& target) {
    if (CastW(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target, kERange) && CastE(target, Mode::Flee, true)) return;
    const auto ally = ProtectedAlly();
    (void)CastR(ally, Mode::Flee, true);
}

inline void Automatic(const AIHeroClient& selected) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (InterruptTargetId != 0 && InterruptUntil >= Now()) {
        const auto threat = HeroByNetworkId(InterruptTargetId);
        if (Engine::ValidEnemy(threat, kERange) && CastE(threat, Mode::Automatic, true)) return;
    }
    if (GapcloserTargetId != 0 && GapcloserUntil >= Now()) {
        const auto threat = HeroByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(threat, kERange) && CastE(threat, Mode::Automatic, true)) return;
    }
    const auto ally = ProtectedAlly();
    if (Engine::ValidAlly(ally) && AllyNeedsStandUnited(ally) && CastR(ally, Mode::Automatic, true)) return;
    if (Engine::ValidEnemy(selected, kQRange)) (void)CastW(selected, Mode::Automatic, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const auto target = PreferredEnemyTarget(selected, kERange + 50.0f);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle: Farm(mode); break;
    case Mode::LastHit: LastHit(target); break;
    case Mode::Flee: Flee(target); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("Shen tactics"));
    TacticsMenu->Add(new MenuSlider("MinCommitHealth", "Minimum commit health", 30, 1, 100));
    TacticsMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies at commit", 3, 1, 5));
    TacticsMenu->Add(new MenuSlider("MaxDashEnemies", "Maximum enemies at taunt endpoint", 2, 0, 5));
    TacticsMenu->Add(new MenuSlider("MinDashHealth", "Minimum taunt endpoint health", 28, 1, 100));
    TacticsMenu->Add(new MenuSlider("WDuration", "Spirit's Refuge tracking ms", 1800, 500, 2500));
    TacticsMenu->Add(new MenuSlider("HarassEnergy", "Harass energy percent", 40, 0, 100));
    TacticsMenu->Add(new MenuBool("HarassUseE", "Use taunt during harass", false));
    TacticsMenu->Add(new MenuBool("UseObjectiveW", "Use Refuge for objectives", true));
    PassiveMenu = root->AddSubMenu(new Menu("Shen passive"));
    PassiveMenu->Add(new MenuSlider("ShieldHealth", "Shield trigger health percent", 72, 1, 100));
    BladeMenu = root->AddSubMenu(new Menu("Spirit Blade"));
    BladeMenu->Add(new MenuBool("TrackTarget", "Track Q target blade", true));
    UltimateMenu = root->AddSubMenu(new Menu("Stand United"));
    UltimateMenu->Add(new MenuSlider("AllyHealth", "Ally health threshold", 58, 1, 100));
    UltimateMenu->Add(new MenuBool("InterruptUnsafeChannel", "Stop policy on channel interrupt", true));
    FarmMenu = root->AddSubMenu(new Menu("Shen farming"));
    FarmMenu->Add(new MenuSlider("ClearEnergy", "Clear energy percent", 35, 0, 100));
}

inline void OnLoad() {
    PassiveReady = false; PassiveShieldAmount = 0.0f; PassiveShieldUntil = 0;
    BladeTargetId = 0; SpiritBlade = {}; BladeLastSeenTick = 0;
    WActive = false; WCenter = {}; WExpireTick = 0;
    ECastTick = 0; EEndpoint = {}; ETargetId = 0;
    RChannelActive = false; RAllyId = 0; RChannelUntil = 0; RInterrupted = false;
    GapcloserTargetId = 0; GapcloserEndpoint = {}; GapcloserUntil = 0;
    InterruptTargetId = 0; InterruptUntil = 0; TargetedAllyId = 0; TargetedAllyUntil = 0;
    LastAutoTargetId = 0; LastAutoTick = 0; LastCastTick.fill(0);
}
inline void OnUnload() {
    TacticsMenu = nullptr; PassiveMenu = nullptr; BladeMenu = nullptr;
    UltimateMenu = nullptr; FarmMenu = nullptr; OnLoad();
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        if (args.IsAutoAttack) {
            LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
            LastAutoTick = Now();
            return;
        }
        const int slot = args.Slot;
        if (slot >= 0 && slot < 4 && Engine::WasControllerCast(slot)) LastCastTick[slot] = Now();
        const int payloadTargetId = static_cast<int>(
            args.TargetNetworkId != 0 ? args.TargetNetworkId : args.Target.NetworkId);
        if (slot == 0 || Engine::TextContains(args.SpellName, "ShenQ")) {
            const auto player = GameObjects::Player();
            const auto target = HeroByNetworkId(payloadTargetId);
            if (player.IsValid() && target.IsValid()) {
                BladeTargetId = static_cast<int>(target.NetworkId());
                SpiritBlade = SpiritBladePosition(player.Position(), target.Position());
                BladeLastSeenTick = Now();
            }
        } else if (slot == 1 || Engine::TextContains(args.SpellName, "ShenW")) {
            WActive = true;
            WCenter = args.CastPosition.IsValid()
                ? args.CastPosition : GameObjects::Player().Position();
            WExpireTick = Now() + 1800;
        } else if (slot == 2 || Engine::TextContains(args.SpellName, "ShenE")) {
            ECastTick = Now();
            EEndpoint = args.EndPosition;
            ETargetId = payloadTargetId;
        } else if (slot == 3 || Engine::TextContains(args.SpellName, "ShenR")) {
            RChannelActive = true;
            RAllyId = payloadTargetId;
            RChannelUntil = Now() + kRChannelMs;
            RInterrupted = false;
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args, 240.0f, 120.0f);
    if (!analysis.Valid) return;
    const int targetId = args.TargetNetworkId != 0
        ? static_cast<int>(args.TargetNetworkId)
        : static_cast<int>(args.Target.NetworkId);
    if (targetId != 0 && targetId == RAllyId && RChannelActive &&
        Bool(UltimateMenu, "InterruptUnsafeChannel", true)) RInterrupted = true;
    if (targetId != 0 && targetId == TargetedAllyId) {
        TargetedAllyUntil = std::max(TargetedAllyUntil, analysis.CommitmentUntilTick);
        TargetedAllyId = static_cast<int>(analysis.Enemy.NetworkId());
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId); LastAutoTick = Now();
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "shenpassiveshield")) {
        PassiveReady = true; PassiveShieldUntil = Now() + 4000;
        const auto player = GameObjects::Player();
        PassiveShieldAmount = PassiveShield(player.Level(), player.BonusHealth()).Amount;
    }
    if (Engine::TextContains(args.BuffName, "shenw")) WActive = true;
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "shenpassiveshield")) {
        PassiveReady = false; PassiveShieldUntil = 0; PassiveShieldAmount = 0.0f;
    }
}
inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "shenpassiveshield")) {
        PassiveReady = true; PassiveShieldUntil = Now() + 4000;
    }
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) { LastAutoTargetId = static_cast<int>(args.Target.NetworkId()); LastAutoTick = Now(); }
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) { LastAutoTargetId = static_cast<int>(args.Target.NetworkId()); LastAutoTick = Now(); }
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, GapcloserTargetId, GapcloserEndpoint, GapcloserUntil, 900.0f, 1200);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, InterruptTargetId, InterruptUntil, 1050, 250, 6000);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (Engine::TextContains(args.Sender.Name, "shen_spiritblade") ||
        Engine::TextContains(args.Sender.CharacterName, "shen_spiritblade")) {
        SpiritBlade = args.Sender.Position; BladeLastSeenTick = Now();
    }
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (Engine::TextContains(args.Sender.Name, "shen_spiritblade")) {
        BladeTargetId = 0; SpiritBlade = {};
    }
}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (MissileEventIsLocal(args) && Engine::TextContains(args.SpellName, "ShenQ")) {
        BladeLastSeenTick = Now();
    }
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDraw() {}

inline constexpr const char* Scenarios[] = {
    "Passive shield amount, buff lifecycle and cooldown reconciliation",
    "Q Spirit Blade target tracking, pull position and empowered AA ownership",
    "W Spirit's Refuge dodge-zone coverage for allies, autos and objectives",
    "E Shadow Dash prediction, collision, projectile-wall and safe endpoint policy",
    "R Stand United ally selection, shield value, channel and interrupt policy",
    "turret, enemy-count, reach, resource, damage and shield gates",
    "combo, harass, lane clear, jungle objective, last-hit and flee modes",
    "selected target precedence with orbwalker fallback and automatic reactions",
    "manual casts, AA windup ownership, event callbacks and polling reconciliation",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Shen";
    controller.ControllerId = "champion.kuroaio.ai.shen.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIShen.md";
    controller.ImplementationSummary =
        "Passive shield and Spirit Blade telemetry, W dodge-zone placement, safe taunt endpoints, "
        "and ally-health/channel-aware Stand United policy with objective and farm modes.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Shen
