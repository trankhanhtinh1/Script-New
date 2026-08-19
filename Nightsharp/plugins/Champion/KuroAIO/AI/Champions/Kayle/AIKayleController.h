#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AIKayle.h"
#include "AIKayleGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Kayle {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerManaPercent;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellEnabled;

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline int PassiveStacks = 0;
inline int ObservedLevel = 1;
inline Form CurrentForm = Form::Melee;
inline bool PassiveConfirmed = false;
inline bool EArmed = false;
inline int EArmedTargetId = 0;
inline int LastBeforeAttackTargetId = 0;
inline int LastAfterAttackTargetId = 0;
inline int LastAfterAttackTick = 0;
inline int QShredTargetId = 0;
inline int QShredUntilTick = 0;
inline bool RInvulnerabilityActive = false;
inline int RProtectedTargetId = 0;
inline int RExpireTick = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline Vector3 IncomingThreatEndpoint{};
inline std::array<int, 4> LastCastTick{};

inline int Rank(int slot) { return ControllerHelpers::SpellRank(slot); }
inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
           Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
           (reactive || LastCastTick[static_cast<std::size_t>(slot)] + 45 <= Now());
}
inline bool InRange(const AIBaseClient& target, float range) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid() &&
           player.Position().Distance2D(target.Position()) <=
               range + target.BoundingRadius();
}
inline bool EnemyBlocked(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           HasSpellShieldOrImmunity(target);
}
inline bool PreserveAttack(bool reactive, bool lethal = false) {
    return !reactive && !lethal && Orbwalker::IsWindingUp() &&
           Orbwalker::AttackCastDelayRemaining() > 25 &&
           Bool(Engine::HumanMenu, "PreserveAttacks", true);
}
inline bool Throttle(int slot, int ms = 55) {
    return slot >= 0 && slot < 4 &&
           LastCastTick[static_cast<std::size_t>(slot)] + ms <= Now();
}

inline float QRawDamage() {
    const int rank = Rank(0);
    const float base = RankValue(std::array<float, 6>{0.0f, 60.0f, 85.0f, 110.0f, 135.0f, 160.0f}, rank);
    return base + 0.60f * std::max(0.0f, ControllerHelpers::AP()) +
           0.40f * std::max(0.0f, ControllerHelpers::BonusAttackDamage());
}
inline float EMagicDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float missing = std::max(0.0f, target.MaxHealth() - target.Health());
    const float execute = EExecuteDamage(Rank(2), player.AP(), target.MaxHealth(), missing);
    const float base = 20.0f + 0.10f * std::max(0.0f, player.AP()) + execute;
    return player.CalculateMagicDamage(target, base);
}
inline bool LethalWithoutE(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target) &&
           player.CalculatePhysicalDamage(target, player.TotalAttackDamage()) >=
               target.Health() + target.AllShield();
}
inline bool LethalWithE(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return false;
    return player.CalculatePhysicalDamage(target, player.TotalAttackDamage()) +
               EMagicDamage(target) + 2.0f >=
           target.Health() + target.AllShield();
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    ObservedLevel = std::clamp(player.Level(), 1, 18);
    CurrentForm = FormForLevel(ObservedLevel);
    if (player.HasBuff("KaylePassiveStack") || player.HasBuff("KaylePassiveStacks")) {
        PassiveConfirmed = true;
    }
    if (RInvulnerabilityActive && RExpireTick <= Now()) {
        RInvulnerabilityActive = false;
        RProtectedTargetId = 0;
        RExpireTick = 0;
    }
    if (QShredUntilTick <= Now()) {
        QShredTargetId = 0;
        QShredUntilTick = 0;
    }
    if (EArmed && (!Engine::RuntimeSpells[2] || !Engine::RuntimeSpells[2]->IsReady()) &&
        Now() - LastAfterAttackTick > kEExecuteWindowMs) {
        EArmed = false;
        EArmedTargetId = 0;
    }
    if (IncomingThreatUntil <= Now()) {
        IncomingThreatUntil = 0;
        IncomingThreatTargetId = 0;
        IncomingThreatEndpoint = {};
    }
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || EnemyBlocked(target) || !Ready(0, mode, reactive) ||
        !Throttle(0) || PreserveAttack(reactive)) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vector3 aim = prediction.GetCastPosition();
    QCollisionContext context{};
    context.Ready = true;
    context.EndpointValid = QEndpointValid(player.Position(), aim);
    context.TargetInReach = context.EndpointValid &&
        QLineHits(player.Position(), aim, target.Position(), target.BoundingRadius());
    context.CollisionFree = prediction.CollisionObjects.empty();
    context.ProjectileWallClear = !ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f);
    if (!QCastAllowed(context) || SDK::NavMesh::IsWall(aim)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    QShredTargetId = static_cast<int>(target.NetworkId());
    QShredUntilTick = Now() + kQShredDurationMs;
    return true;
}

inline AIHeroClient SelectHealTarget(float range, bool urgent) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    AIHeroClient best = player;
    float bestScore = player.HealthPercent() < 100.0f
        ? (100.0f - player.HealthPercent()) * 1.4f +
          static_cast<float>(Engine::CountEnemiesAt(player.Position(), 500.0f)) * 20.0f
        : -FLT_MAX;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally, range)) continue;
        const float threat = static_cast<float>(Engine::CountEnemiesAt(ally.Position(), 500.0f));
        const float score = (100.0f - ally.HealthPercent()) * 1.55f + threat * 24.0f +
                            (urgent && ally.HealthPercent() < 35.0f ? 140.0f : 0.0f);
        if (score > bestScore) {
            best = ally;
            bestScore = score;
        }
    }
    if (!urgent && bestScore < static_cast<float>(Slider(WMenu, "MinimumMissing", 18))) return {};
    return best;
}

inline bool CastW(const AIHeroClient& requested, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode, reactive) || !Throttle(1, 80)) return false;
    AIHeroClient target = requested.IsValid() ? requested : SelectHealTarget(kWRange, reactive);
    if (!Engine::ValidAlly(target, kWRange)) return false;
    const bool urgent = target.HealthPercent() <= Slider(WMenu, "EmergencyHP", 38) ||
                        player.HealthPercent() <= Slider(WMenu, "SelfHP", 52);
    if (!urgent && PlayerManaPercent() < Slider(WMenu, "ManaFloor", 45)) return false;
    if (PreserveAttack(reactive, urgent)) return false;
    if (!Engine::ControllerCastUnit(1, target)) return false;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || EnemyBlocked(target) || !InRange(target, kERange) ||
        !Ready(2, mode, reactive) || !Throttle(2, 65)) return false;
    const bool lethal = LethalWithE(target);
    const bool low = target.HealthPercent() <= Slider(TacticsMenu, "EExecuteHP", 42);
    EExecuteContext context{};
    context.Ready = true;
    context.TargetValid = true;
    context.InRange = true;
    context.NextAttackAvailable = Orbwalker::CanAttack() ||
        (LastAfterAttackTargetId == static_cast<int>(target.NetworkId()) &&
         Now() - LastAfterAttackTick <= 480);
    context.PreserveAttack = PreserveAttack(reactive, lethal);
    context.TargetLow = low;
    context.Lethal = lethal;
    if (!ShouldCastExecute(context) &&
        !(mode == Mode::Combo && context.NextAttackAvailable && !context.PreserveAttack)) return false;
    if (!Engine::ControllerCastUnit(2, target)) return false;
    LastCastTick[2] = Now();
    EArmed = true;
    EArmedTargetId = static_cast<int>(target.NetworkId());
    return true;
}

inline AIHeroClient SelectUltimateTarget(bool urgent) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    auto scoreAlly = [&](const AIHeroClient& ally) {
        if (!Engine::ValidAlly(ally, kRRange)) return;
        const int enemies = Engine::CountEnemiesAt(ally.Position(), kRRadius);
        const bool low = ally.HealthPercent() <= Slider(UltimateMenu, "AllyHP", 38);
        const bool threat = enemies > 0 || (IncomingThreatUntil > Now() &&
            static_cast<int>(ally.NetworkId()) == RProtectedTargetId);
        if (!urgent && !low && !threat) return;
        UltimateContext context{};
        context.Ready = true;
        context.AllyValid = true;
        context.AllyLow = low;
        context.AllyThreatened = threat;
        context.PlayerIsAlly = ally.NetworkId() == player.NetworkId();
        context.IncomingHardCC = IncomingThreatUntil > Now();
        context.EnemiesAtAlly = enemies;
        context.MaximumEnemies = Slider(UltimateMenu, "MaxEnemies", 3);
        if (!ShouldCastUltimate(context)) return;
        const float score = (100.0f - ally.HealthPercent()) * 2.2f +
            static_cast<float>(enemies) * 78.0f +
            (context.PlayerIsAlly ? 95.0f : 0.0f) +
            (urgent && low ? 160.0f : 0.0f);
        if (score > bestScore) {
            best = ally;
            bestScore = score;
        }
    };
    scoreAlly(player);
    for (const auto& ally : GameObjects::AllyHeroes()) scoreAlly(ally);
    return best;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidAlly(target, kRRange) ||
        !Ready(3, mode, reactive) || !Throttle(3, 120) || RInvulnerabilityActive) return false;
    const bool urgent = reactive || target.HealthPercent() <= Slider(UltimateMenu, "AllyHP", 38) ||
        IncomingThreatUntil > Now();
    const int enemies = Engine::CountEnemiesAt(target.Position(), kRRadius);
    if (!urgent && enemies < Slider(UltimateMenu, "MinimumEnemies", 2)) return false;
    if (!urgent && Orbwalker::IsWindingUp()) return false;
    if (Engine::UnderEnemyTurret(target.Position()) && !urgent) return false;
    UltimateContext context{};
    context.Ready = true;
    context.AllyValid = true;
    context.AllyLow = target.HealthPercent() <= Slider(UltimateMenu, "AllyHP", 38);
    context.AllyThreatened = enemies > 0;
    context.PlayerIsAlly = target.NetworkId() == player.NetworkId();
    context.IncomingHardCC = IncomingThreatUntil > Now();
    context.Manual = reactive;
    context.EnemiesAtAlly = enemies;
    context.MaximumEnemies = Slider(UltimateMenu, "MaxEnemies", 3);
    if (!ShouldCastUltimate(context)) return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    LastCastTick[3] = Now();
    RInvulnerabilityActive = true;
    RProtectedTargetId = static_cast<int>(target.NetworkId());
    RExpireTick = Now() + kRInvulnerabilityMs;
    return true;
}

inline bool Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kQRange + 80.0f)) return false;
    if (CastE(target, Mode::Combo)) return true;
    if (CastQ(target, Mode::Combo)) return true;
    if (CastW({}, Mode::Combo)) return true;
    return CastR(SelectUltimateTarget(false), Mode::Combo);
}
inline bool Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kQRange + 80.0f) ||
        PlayerManaPercent() < Slider(TacticsMenu, "HarassMana", 52)) return false;
    if (CastQ(target, Mode::Harass)) return true;
    return CastE(target, Mode::Harass);
}
inline bool Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool late = player.Level() >= kWaveLevel;
    if (mode == Mode::Jungle && player.HealthPercent() <= 62.0f && CastW({}, mode)) return true;
    if (late && mode == Mode::LaneClear && PlayerManaPercent() < Slider(FarmMenu, "LateMana", 48)) {
        return Engine::TryFarm(Mode::LastHit);
    }
    // Before level 11, spend Q/E only on secured farm; after waves unlock,
    // Q line clear is preferred and E remains an auto-reset last-hit tool.
    return Engine::TryFarm(mode);
}
inline bool Flee(const AIHeroClient& threat) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (CastW({}, Mode::Flee, true)) return true;
    if (Engine::ValidAlly(player, kRRange) && player.HealthPercent() < 40.0f &&
        CastR(player, Mode::Flee, true)) return true;
    return Engine::ValidEnemy(threat, 500.0f) && CastR(player, Mode::Flee, true);
}
inline bool Automatic(const AIHeroClient& target) {
    if (IncomingThreatUntil > Now()) {
        const AIHeroClient save = SelectUltimateTarget(true);
        if (CastR(save, Mode::Automatic, true)) return true;
    }
    const AIHeroClient save = SelectUltimateTarget(false);
    if (CastR(save, Mode::Automatic, true)) return true;
    if (Engine::ValidEnemy(target, kERange) && LethalWithE(target)) return CastE(target, Mode::Automatic, true);
    if (GameObjects::Player().HealthPercent() < Slider(WMenu, "SelfHP", 52)) return CastW({}, Mode::Automatic, true);
    return false;
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const AIHeroClient target = PreferredEnemyTarget(selected, kQRange + 80.0f);
    const AIHeroClient threat = ControllerHelpers::NearestEnemyToPlayer(target, 950.0f);
    switch (mode) {
    case Mode::Combo: (void)Combo(target); break;
    case Mode::Harass: (void)Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)Farm(mode); break;
    case Mode::Flee: (void)Flee(threat); break;
    case Mode::Automatic: (void)Automatic(target); break;
    default: break;
    }
    return true;
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("Kayle tactics"));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 52, 0, 100));
    TacticsMenu->Add(new MenuSlider("EExecuteHP", "E execute health percent", 42, 1, 100));
    WMenu = root->AddSubMenu(new Menu("Kayle blessing"));
    WMenu->Add(new MenuSlider("EmergencyHP", "Emergency ally health percent", 38, 1, 100));
    WMenu->Add(new MenuSlider("SelfHP", "Self heal health percent", 52, 1, 100));
    WMenu->Add(new MenuSlider("MinimumMissing", "Minimum missing health", 18, 0, 100));
    WMenu->Add(new MenuSlider("ManaFloor", "W mana floor", 45, 0, 100));
    UltimateMenu = root->AddSubMenu(new Menu("Kayle judgment"));
    UltimateMenu->Add(new MenuSlider("AllyHP", "Protect ally below health percent", 38, 1, 100));
    UltimateMenu->Add(new MenuSlider("MinimumEnemies", "Minimum enemies for proactive R", 2, 0, 5));
    UltimateMenu->Add(new MenuSlider("MaxEnemies", "Maximum enemies at R center", 3, 0, 5));
    FarmMenu = root->AddSubMenu(new Menu("Kayle ascent farm"));
    FarmMenu->Add(new MenuSlider("LateMana", "Late wave mana floor", 48, 0, 100));
    CoachMenu = root->AddSubMenu(new Menu("Kayle coach"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw level-gated ranges", true));
}
inline void OnLoad() {
    PassiveStacks = 0; ObservedLevel = 1; CurrentForm = Form::Melee;
    PassiveConfirmed = false; EArmed = false; EArmedTargetId = 0;
    LastBeforeAttackTargetId = LastAfterAttackTargetId = 0; LastAfterAttackTick = 0;
    QShredTargetId = 0; QShredUntilTick = 0; RInvulnerabilityActive = false;
    RProtectedTargetId = 0; RExpireTick = 0; IncomingThreatUntil = 0;
    IncomingThreatTargetId = 0; IncomingThreatEndpoint = {}; LastCastTick.fill(0);
}
inline void OnUnload() {
    TacticsMenu = nullptr; FarmMenu = nullptr; UltimateMenu = nullptr;
    WMenu = nullptr; CoachMenu = nullptr; OnLoad();
}
inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawRanges", true)) return;
    Drawing::DrawCircle(player.Position(), AttackRangeForLevel(ObservedLevel), 0x448EA7FFu, 1.0f, 64);
    Drawing::DrawCircle(player.Position(), kQRange, 0x2240D8FFu, 1.0f, 72);
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        if (args.IsAutoAttack && Engine::WasControllerCast(static_cast<int>(args.Slot))) {
            LastBeforeAttackTargetId = static_cast<int>(args.TargetNetworkId);
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && analysis.Enemy.IsValid()) {
        IncomingThreatTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
    }
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAfterAttackTargetId = static_cast<int>(args.TargetNetworkId);
        LastAfterAttackTick = Now();
        if (EArmed && LastAfterAttackTargetId == EArmedTargetId) {
            EArmed = false;
            EArmedTargetId = 0;
        }
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "kaylepassive") ||
        Engine::TextContains(args.BuffName, "zealous")) {
        PassiveConfirmed = true;
        PassiveStacks = std::clamp(args.Count, 0, kPassiveStackCap);
    }
    if (Engine::TextContains(args.BuffName, "kayler") ||
        Engine::TextContains(args.BuffName, "judgment")) {
        RInvulnerabilityActive = true;
        RExpireTick = Now() + kRInvulnerabilityMs;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "kaylepassive") ||
        Engine::TextContains(args.BuffName, "zealous")) {
        PassiveStacks = 0;
        PassiveConfirmed = false;
    }
    if (Engine::TextContains(args.BuffName, "kayler") ||
        Engine::TextContains(args.BuffName, "judgment")) {
        RInvulnerabilityActive = false;
        RExpireTick = 0;
        RProtectedTargetId = 0;
    }
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastBeforeAttackTargetId, LastAfterAttackTick);
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAfterAttackTargetId, LastAfterAttackTick);
    if (PassiveStacks < kPassiveStackCap) ++PassiveStacks;
    if (EArmed && args.Target.IsValid() &&
        static_cast<int>(args.Target.NetworkId()) == EArmedTargetId) {
        EArmed = false;
        EArmedTargetId = 0;
    }
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, IncomingThreatTargetId, IncomingThreatEndpoint,
                           IncomingThreatUntil, kRRadius, 1200);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, IncomingThreatTargetId, IncomingThreatUntil, 900, 250, 5000);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) { (void)args; }
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) { (void)args; }
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) { (void)args; }
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) { (void)args; }

inline constexpr const char* Scenarios[] = {
    "level 6/11/16 form and ranged-wave transition reconciliation",
    "Zealous passive stack capture, five-stack attack posture and reset",
    "Radiant Blast prediction, collision, wall and Q shred duration",
    "Celestial Blessing heal/speed ally selection and mana gate",
    "Starfire Spellblade attack reset with missing-health execute boundary",
    "Divine Judgment self or ally invulnerability save and sword-area safety",
    "early melee farm versus late ranged-wave lane and jungle posture",
    "turret/enemy-count rejection, manual windup protection and threat polling",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Kayle;
    controller.ControllerId = "champion.kuroaio.ai.kayle.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIKayle.md";
    controller.ImplementationSummary =
        "Level-gated forms, Zealous stacks, Q shred, W ally sustain, E execute reset and R invulnerability save loop.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Kayle
