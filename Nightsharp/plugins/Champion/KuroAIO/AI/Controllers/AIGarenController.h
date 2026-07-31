#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "../Profiles/AIGaren.h"
#include "AIGarenGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Garen {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline SpinState CurrentSpin = SpinState::Ready;
inline CourageState CurrentCourage = CourageState::Ready;
inline DecisiveStrikeState CurrentQ = DecisiveStrikeState::Ready;
inline int SpinStartTick = 0;
inline int SpinTargetId = 0;
inline int SpinBodyId = 0;
inline Vector3 SpinBodyPosition{};
inline Vector3 LastSpinTargetPosition{};
inline int QArmTick = 0;
inline int QSilenceUntil = 0;
inline int QTargetId = 0;
inline int WStartTick = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline Vector3 IncomingThreatEndpoint{};
inline bool PassiveReady = false;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline std::array<int, 4> LastCastTick{};

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        CooldownAvailable(Engine::RuntimeSpells[slot]->IsReady(),
                          Now() - LastCastTick[static_cast<std::size_t>(slot)], 45) &&
        SpellEnabled(slot, mode) && (reactive || LastCastTick[static_cast<std::size_t>(slot)] + 45 <= Now());
}

inline bool PreserveAttack(bool reactive) {
    return !reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true);
}

inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}

inline bool SafeBodyCommit(const Vector3& body, const AIHeroClient&, bool lethal, bool defensive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !body.IsValid() || body.IsZero()) return false;
    if (SDK::NavMesh::IsWall(body)) return false;
    if (Engine::UnderEnemyTurret(body) && !Engine::UnderEnemyTurret(player.Position()) && !lethal)
        return false;
    const int nearby = Engine::CountEnemiesAt(body, 425.0f);
    return SafeJudgmentCommit(false, false, nearby,
                              Slider(TacticsMenu, "MaximumEnemies", 3), lethal, defensive);
}

inline float JudgmentDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const bool isolated = Engine::CountEnemiesAt(target.Position(), 350.0f) <= 1;
    return player.CalculatePhysicalDamage(target,
        JudgmentTickDamage(SpellRank(2), player.TotalAttackDamage(),
                           player.BonusAttackDamage(), isolated));
}

inline float JusticeDamage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    const float missing = std::max(0.0f, target.MaxHealth() - target.Health());
    return DemacianJusticeDamage(SpellRank(3), missing);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(0, mode, reactive) ||
        !InAttackReach(player.Position(), PredictPosition(target, 0.10f), target.BoundingRadius()) ||
        !Orbwalker::CanAttack() || PreserveAttack(reactive)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    CurrentQ = DecisiveStrikeState::Armed;
    QArmTick = Now();
    QTargetId = static_cast<int>(target.NetworkId());
    LastCastTick[0] = QArmTick;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode, reactive) || CurrentCourage == CourageState::Active ||
        PreserveAttack(reactive)) return false;
    const bool threat = IncomingThreatUntil >= Now();
    const int enemies = Engine::CountEnemiesAt(player.Position(), 425.0f);
    const bool urgent = reactive || threat || player.HealthPercent() <= Slider(TacticsMenu, "CourageHealth", 62) || enemies >= 2;
    if (!urgent || (!Engine::ValidEnemy(target) && !threat && !reactive)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    CurrentCourage = CourageState::Active;
    WStartTick = Now();
    LastCastTick[1] = WStartTick;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kJudgmentRadius + 80.0f) ||
        !Ready(2, mode, reactive) || CurrentSpin == SpinState::Spinning ||
        PreserveAttack(reactive)) return false;
    const Vector3 body = player.Position();
    const Vector3 predicted = PredictPosition(target, 0.18f);
    if (!InJudgmentReach(body, predicted, target.BoundingRadius())) return false;
    const bool lethal = target.Health() + target.AllShield() <= JudgmentDamage(target) * 7.0f;
    if (!SafeBodyCommit(body, target, lethal, player.HealthPercent() <= 34.0f && reactive)) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    CurrentSpin = SpinState::Spinning;
    SpinStartTick = Now();
    SpinTargetId = static_cast<int>(target.NetworkId());
    LastSpinTargetPosition = predicted;
    LastCastTick[2] = SpinStartTick;
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Engine::ValidEnemy(target, kDemacianJusticeRange) ||
        !Ready(3, mode, reactive) || PreserveAttack(reactive)) return false;
    if (!ExecuteLethal(target.Health(), target.AllShield(), JusticeDamage(target))) return false;
    if (Engine::UnderEnemyTurret(target.Position()) && !Engine::UnderEnemyTurret(player.Position()) && !reactive)
        return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    LastCastTick[3] = Now();
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, 700.0f)) return;
    if (CastR(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    (void)CastE(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kJudgmentRadius + 80.0f)) return;
    if (CastQ(target, Mode::Harass)) return;
    if (CastE(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}

inline void Farm(Mode mode, const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const bool jungle = mode == Mode::Jungle;
    const bool threat = Engine::CountEnemiesAt(player.Position(), 500.0f) > 0;
    if (!ResourceFreeFarmPolicy(player.HealthPercent(), PassiveReady, threat, jungle)) return;
    if (Engine::ValidEnemy(target, kJudgmentRadius + 80.0f) &&
        Engine::CountEnemiesAt(target.Position(), kJudgmentRadius) >= Slider(FarmMenu, "MinimumSpinTargets", 1)) {
        if (CastE(target, mode)) return;
    }
    if (jungle && Engine::ValidEnemy(target, kDemacianJusticeRange) && CastQ(target, mode)) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target, kDecisiveStrikeRange + 100.0f) && CastW(target, Mode::Flee, true)) return;
    (void)CastQ(target, Mode::Flee, true);
}

inline void Automatic(const AIHeroClient& target) {
    if (ManualOwnershipUntil > Now()) return;
    if (IncomingThreatUntil >= Now() && Engine::ValidEnemy(target) && CastW(target, Mode::Automatic, true)) return;
    if (Engine::ValidEnemy(target) && CastR(target, Mode::Automatic, true)) return;
    if (Engine::ValidEnemy(target) && GameObjects::Player().HealthPercent() <= 28.0f)
        (void)CastW(target, Mode::Automatic, true);
}

inline void ReconcileState() {
    const int now = Now();
    if (CurrentQ == DecisiveStrikeState::Armed && !DecisiveStrikeWindow(CurrentQ, now - QArmTick)) {
        CurrentQ = DecisiveStrikeState::Expired;
        QTargetId = 0;
    }
    if (CurrentCourage == CourageState::Active && now - WStartTick >= kCourageDurationMs)
        CurrentCourage = CourageState::Expired;
    if (CurrentSpin == SpinState::Spinning && now - SpinStartTick >= kJudgmentDurationMs) {
        CurrentSpin = SpinState::Expired;
        SpinTargetId = 0;
        SpinBodyId = 0;
        SpinBodyPosition = {};
    }
    if (CurrentQ == DecisiveStrikeState::Expired ||
        CurrentQ == DecisiveStrikeState::Consumed) CurrentQ = DecisiveStrikeState::Ready;
    if (!SilenceActive(QSilenceUntil, now)) QSilenceUntil = 0;
    if (CurrentCourage == CourageState::Expired) CurrentCourage = CourageState::Ready;
    if (CurrentSpin == SpinState::Expired) CurrentSpin = SpinState::Ready;
    if (IncomingThreatUntil < now) {
        IncomingThreatUntil = 0;
        IncomingThreatTargetId = 0;
        IncomingThreatEndpoint = {};
    }
    if (CurrentSpin == SpinState::Spinning && SpinTargetId != 0) {
        const auto tracked = Engine::EnemyByNetworkId(SpinTargetId);
        if (Engine::ValidEnemy(tracked, kJudgmentRadius + 100.0f)) {
            const Vector3 predicted = PredictPosition(tracked, 0.10f);
            const Vector3 body = SpinBodyPosition.IsValid() ? SpinBodyPosition : GameObjects::Player().Position();
            if (SpinTargetTrackable(CurrentSpin, now - SpinStartTick, SpinTargetId,
                                    static_cast<int>(tracked.NetworkId()), body,
                                    predicted, tracked.BoundingRadius()) ||
                InJudgmentReach(GameObjects::Player().Position(), predicted, tracked.BoundingRadius()))
                LastSpinTargetPosition = predicted;
        }
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const AIHeroClient target = PreferredEnemyTarget(selected, mode == Mode::Flee ? 850.0f : 700.0f);
    if (CurrentSpin == SpinState::Spinning) {
        if (mode == Mode::Automatic && Engine::ValidEnemy(target)) (void)CastR(target, mode, true);
        return true;
    }
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode, target); break;
    case Mode::Flee: Flee(target); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("Garen judgment tactics"));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Garen resource-free farm"));
    TacticsMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies at Judgment body", 3, 0, 5));
    TacticsMenu->Add(new MenuSlider("CourageHealth", "Use Courage below health percent", 62, 1, 100));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Manual cast protection (ms)", 650, 0, 2000));
    TacticsMenu->Add(new MenuBool("PreserveAttacks", "Preserve attack windup", true));
    FarmMenu->Add(new MenuSlider("MinimumSpinTargets", "Minimum tracked bodies for farm spin", 1, 1, 5));
}

inline void OnLoad() {
    CurrentSpin = SpinState::Ready;
    CurrentCourage = CourageState::Ready;
    CurrentQ = DecisiveStrikeState::Ready;
    SpinStartTick = SpinTargetId = SpinBodyId = QArmTick = QTargetId = QSilenceUntil = WStartTick = 0;
    ManualOwnershipUntil = IncomingThreatUntil = IncomingThreatTargetId = 0;
    SpinBodyPosition = LastSpinTargetPosition = IncomingThreatEndpoint = {};
    PassiveReady = false;
    LastAutoTargetId = LastAutoTick = 0;
    LastCastTick.fill(0);
}

inline void OnUnload() {
    TacticsMenu = nullptr;
    FarmMenu = nullptr;
    OnLoad();
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            if (!Engine::WasControllerCast(slot))
                ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 650);
            LastCastTick[static_cast<std::size_t>(slot)] = now;
            if (slot == 0) { CurrentQ = DecisiveStrikeState::Armed; QArmTick = now; }
            if (slot == 1) { CurrentCourage = CourageState::Active; WStartTick = now; }
            if (slot == 2) { CurrentSpin = SpinState::Spinning; SpinStartTick = now; }
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
        IncomingThreatTargetId = static_cast<int>(args.Sender.NetworkId);
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
        IncomingThreatEndpoint = args.EndPosition;
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack) return;
    LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
    LastAutoTick = Now();
    if (CurrentQ == DecisiveStrikeState::Armed && (QTargetId == 0 || QTargetId == LastAutoTargetId)) {
        CurrentQ = DecisiveStrikeState::Consumed;
        QTargetId = 0;
        QSilenceUntil = Now() + kDecisiveStrikeSilenceMs;
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "garenpassive")) PassiveReady = true;
    if (Engine::TextContains(args.BuffName, "garenq")) CurrentQ = DecisiveStrikeState::Armed;
    if (Engine::TextContains(args.BuffName, "garenw")) { CurrentCourage = CourageState::Active; WStartTick = Now(); }
    if (Engine::TextContains(args.BuffName, "garene")) { CurrentSpin = SpinState::Spinning; SpinStartTick = Now(); }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "garenpassive")) PassiveReady = false;
    if (Engine::TextContains(args.BuffName, "garenq")) { CurrentQ = DecisiveStrikeState::Ready; QTargetId = 0; }
    if (Engine::TextContains(args.BuffName, "garenw")) CurrentCourage = CourageState::Ready;
    if (Engine::TextContains(args.BuffName, "garene")) CurrentSpin = SpinState::Ready;
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (args.EndTime <= Game::Time()) OnBuffRemove(args);
    else OnBuffAdd(args);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) {
        LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
        LastAutoTick = Now();
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid() && CurrentQ == DecisiveStrikeState::Armed &&
        (QTargetId == 0 || QTargetId == static_cast<int>(args.Target.NetworkId()))) {
        CurrentQ = DecisiveStrikeState::Consumed;
        QTargetId = 0;
        QSilenceUntil = Now() + kDecisiveStrikeSilenceMs;
    }
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, IncomingThreatTargetId, IncomingThreatEndpoint,
                           IncomingThreatUntil, kJudgmentRadius, 1100);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, IncomingThreatTargetId, IncomingThreatUntil, 900, 250, 5000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!ControllerHelpers::AnyTextContains({args.SpellName, args.MissileName},
                                             {"garen", "judgment", "garenec"})) return;
    if (args.Sender.IsValid()) {
        SpinBodyId = static_cast<int>(args.Sender.NetworkId);
        SpinBodyPosition = args.Sender.Position;
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() && SpinBodyId == static_cast<int>(args.Sender.NetworkId)) {
        SpinBodyId = 0;
        SpinBodyPosition = {};
    }
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDraw() {}

inline constexpr const char* Scenarios[] = {
    "Judgment three-second spin body and target tracking with isolated-target damage",
    "Decisive Strike movement, empowered auto ownership and silence window",
    "Courage shield, 30 percent damage reduction and incoming-threat timing",
    "Demacian Justice missing-health true-damage execute with shield boundary",
    "Perseverance resource-free sustain and health-gated lane/jungle farming",
    "selected-target precedence with orbwalker fallback and attack-windup preservation",
    "polling reconciliation for Q, W, E, passive and Judgment body lifetimes",
    "turret, wall, nearby-enemy and unsafe Judgment commit rejection",
    "manual cast protection plus gapcloser and interrupt threat reactions",
    "combo, harass, lane clear, jungle, last-hit, flee and automatic mode policies",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Garen";
    controller.ControllerId = "champion.kuroaio.ai.garen.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIGaren.md";
    controller.ImplementationSummary =
        "Owns Judgment body tracking, Decisive Strike attack/silence state, Courage defensive timing, "
        "Demacian Justice lethal checks and resource-free farm/sustain decisions.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Garen
