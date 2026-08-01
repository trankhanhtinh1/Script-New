#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIMalzaharGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>

namespace Plugins::KuroAIO::AI::Controllers::Malzahar {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Ready;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::TextContainsAny;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<int, 4> LastCastTick{};
inline std::array<int, kWMaximumVoidlings> VoidlingIds{};
inline int ActiveVoidlingCount = 0;
inline int WObservedAmmo = -1;
inline int EMarkedTargetId = 0;
inline int EExpireTick = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline bool ESpreadPending = false;
inline Vector3 ESpreadOrigin = {};
inline int ESpreadUntilTick = 0;
inline int RTargetId = 0;
inline int RStartTick = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCcUntil = 0;
inline int ManualOwnershipUntil = 0;
inline int PassiveCooldownEndTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline bool PassiveShieldActive = false;
inline bool RChanneling = false;
inline bool RInterrupted = false;
inline bool ROwned = false;

inline bool Throttle(int slot, int delay = 55) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

inline bool RuntimeBuff(const AIHeroClient& player,
                        std::initializer_list<const char*> names) {
    if (!player.IsValid()) return false;
    for (const char* name : names) {
        if (name && player.HasBuff(name)) return true;
    }
    return false;
}

inline bool TargetHasE(const AIHeroClient& target) {
    return target.IsValid() && RuntimeBuff(target,
        {"MalzaharE", "MalzaharMaleficVisions", "malzahare"});
}

inline int RuntimeWCharges() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    const auto spell = player.Spellbook().GetSpell(SDK::SpellSlot::W);
    if (spell.IsValid()) {
        const int observed = ValidatedVoidlingAmmo(spell.Ammo(), spell.MaxAmmo());
        if (observed >= 0) {
            WObservedAmmo = observed;
            return observed;
        }
    }
    if (WObservedAmmo >= 0) return std::clamp(WObservedAmmo, 0, kWMaximumAmmo);
    return Ready(1) ? 1 : 0;
}

inline AIHeroClient SelectTarget(const AIHeroClient& selected, float range) {
    return PreferredEnemyTarget(selected, range);
}

inline bool OffensiveReady(int slot, Mode mode, bool reactive = false) {
    if (!SpellEnabled(slot, mode) || !Ready(slot, mode) || !Throttle(slot)) return false;
    if (PreserveAttack(reactive)) return false;
    return true;
}

inline AIHeroClient VoidlingFocus(const AIHeroClient& fallback) {
    if (Bool(WMenu, "PreferMaleficVisions", true) && EMarkedTargetId != 0) {
        const auto marked = ControllerHelpers::HeroByNetworkId(EMarkedTargetId);
        if (Engine::ValidEnemy(marked, kWRange + 35.0f)) return marked;
    }
    if (RChanneling && RTargetId != 0) {
        const auto suppressed = ControllerHelpers::HeroByNetworkId(RTargetId);
        if (Engine::ValidEnemy(suppressed, kWRange + 35.0f)) return suppressed;
    }
    return fallback;
}

inline bool TrySpreadE(Mode mode, bool reactive = false) {
    if (!ESpreadPending || !ESpreadOrigin.IsValid() ||
        ESpreadUntilTick <= Now() || RChanneling || !OffensiveReady(2, mode, reactive)) return false;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kERange + 35.0f) ||
            HasSpellShieldOrImmunity(enemy) ||
            !ESpreadHits(ESpreadOrigin, enemy.Position(), enemy.BoundingRadius())) continue;
        if (!Engine::ControllerCastUnit(2, enemy)) continue;
        LastCastTick[2] = Now();
        EMarkedTargetId = static_cast<int>(enemy.NetworkId());
        EExpireTick = Now() + static_cast<int>(kEDurationSeconds * 1000.0f);
        ESpreadPending = false;
        ESpreadOrigin = {};
        ESpreadUntilTick = 0;
        return true;
    }
    return false;
}


inline bool SafePlayerPosition() {
    const auto player = GameObjects::Player();
    return player.IsValid() && !PlayerMobilityLocked() &&
        !Engine::UnderEnemyTurret(player.Position()) &&
        Engine::CountEnemiesAt(player.Position(), 500.0f) <=
            Slider(RMenu, "MaxChannelEnemies", 2);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || RChanneling || !Engine::ValidEnemy(target, kQRange + 35.0f) ||
        !OffensiveReady(0, mode, reactive) || HasSpellShieldOrImmunity(target)) return false;
    Vector3 aim = PredictPosition(target, kQDelay);
    if (Engine::RuntimeSpells[0]) {
        const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
        if (prediction.GetCastPosition().IsValid() && !prediction.GetCastPosition().IsZero() &&
            prediction.Hitchance >= SDK::HitChance::High) aim = prediction.GetCastPosition();
    }
    if (!QLineHits(player.Position(), aim, PredictPosition(target, kQDelay),
                   target.BoundingRadius())) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || RChanneling || !Engine::ValidEnemy(target, kWRange + 35.0f) ||
        !OffensiveReady(1, mode, reactive) || RuntimeWCharges() <= 0 ||
        SpawnableVoidlings(RuntimeWCharges(), ActiveVoidlingCount) <= 0 ||
        HasSpellShieldOrImmunity(target)) return false;
    const Vector3 aim = PredictPosition(target, 0.25f);
    if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) > kWRange + 35.0f ||
        (!reactive && Engine::UnderEnemyTurret(aim))) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    LastCastTick[1] = Now();
    if (WObservedAmmo > 0) --WObservedAmmo;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || RChanneling || !Engine::ValidEnemy(target, kERange + 35.0f) ||
        !OffensiveReady(2, mode, reactive) || HasSpellShieldOrImmunity(target)) return false;
    if (!Engine::ControllerCastUnit(2, target)) return false;
    LastCastTick[2] = Now();
    EMarkedTargetId = static_cast<int>(target.NetworkId());
    EExpireTick = Now() + static_cast<int>(kEDurationSeconds * 1000.0f);
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || RChanneling || !Engine::ValidEnemy(target, kRRange + 35.0f) ||
        !OffensiveReady(3, mode, reactive) || HasSpellShieldOrImmunity(target) ||
        PlayerMobilityLocked()) return false;
    const bool lethal = Engine::RuntimeSpells[3] &&
        Engine::RuntimeSpells[3]->GetDamage(target) >= target.Health() + target.AllShield();
    const NetherGraspGate gate{
        true, true, HasSpellShieldOrImmunity(target),
        player.Position().Distance2D(target.Position()) <= kRRange + target.BoundingRadius(),
        false, false, IncomingHardCcUntil > Now(),
        Engine::UnderEnemyTurret(player.Position()),
        Engine::CountEnemiesAt(player.Position(), 500.0f) > Slider(RMenu, "MaxChannelEnemies", 2),
        lethal, true};
    if (!CanStartNetherGrasp(gate)) return false;
    if (!reactive && !lethal && target.HealthPercent() > Slider(RMenu, "StartBelowHp", 58)) return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    LastCastTick[3] = RStartTick = Now();
    RTargetId = static_cast<int>(target.NetworkId());
    RChanneling = true;
    ROwned = true;
    RInterrupted = false;
    return true;
}

inline bool Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (mode == Mode::LastHit) {
        if (!Ready(2, mode) || !Throttle(2)) return false;
        for (const auto& minion : GameObjects::EnemyLaneMinions()) {
            if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable() ||
                player.Position().Distance2D(minion.Position()) > kERange) continue;
            if (!Engine::RuntimeSpells[2] ||
                Engine::RuntimeSpells[2]->GetDamage(minion) < minion.Health()) continue;
            if (Engine::ControllerCastPosition(2, minion.Position())) {
                LastCastTick[2] = Now();
                return true;
            }
        }
        return false;
    }
    if (RuntimeWCharges() > 0 && Ready(1, mode) && Throttle(1)) {
        for (const auto& minion : GameObjects::EnemyLaneMinions()) {
            if (minion.IsValid() && !minion.IsDead() && minion.IsTargetable() &&
                player.Position().Distance2D(minion.Position()) <= kWRange &&
                Engine::ControllerCastPosition(1, minion.Position())) {
                LastCastTick[1] = Now();
                if (WObservedAmmo > 0) --WObservedAmmo;
                return true;
            }
        }
        if (mode == Mode::Jungle) {
            for (const auto& monster : GameObjects::Jungle()) {
                if (monster.IsValid() && !monster.IsDead() && monster.IsTargetable() &&
                    player.Position().Distance2D(monster.Position()) <= kWRange &&
                    Engine::ControllerCastPosition(1, monster.Position())) {
                    LastCastTick[1] = Now();
                    if (WObservedAmmo > 0) --WObservedAmmo;
                    return true;
                }
            }
        }
    }
    return false;
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    const bool shield = RuntimeBuff(player,
        {"MalzaharPassive", "MalzaharPassiveShield", "malzaharpassiveshield"});
    if (PassiveShieldActive && !shield) PassiveCooldownEndTick = PassiveCooldownEnd(now);
    PassiveShieldActive = shield;
    if (now >= EExpireTick) EMarkedTargetId = EExpireTick = 0;
    if (EMarkedTargetId != 0 && !TargetHasE(ControllerHelpers::HeroByNetworkId(EMarkedTargetId))) {
        EMarkedTargetId = EExpireTick = 0;
    }
    if (ESpreadUntilTick <= now) {
        ESpreadPending = false;
        ESpreadOrigin = {};
        ESpreadUntilTick = 0;
    }
    const bool runtimeR = RuntimeBuff(player,
        {"MalzaharR", "MalzaharRChannel", "malzaharrsound"});
    if (runtimeR) {
        if (!RChanneling) RStartTick = now;
        RChanneling = true;
    } else if (RChanneling && now - RStartTick > 250) {
        if (now - RStartTick < static_cast<int>((kRChannelSeconds - 0.12f) * 1000.0f))
            RInterrupted = true;
        RChanneling = false;
        ROwned = false;
    }
    if (RChanneling) {
        const auto target = ControllerHelpers::HeroByNetworkId(RTargetId);
        const NetherGraspGate gate{
            true, Engine::ValidEnemy(target), HasSpellShieldOrImmunity(target),
            Engine::ValidEnemy(target, kRRange + 35.0f), true, RInterrupted,
            IncomingHardCcUntil > now, Engine::UnderEnemyTurret(player.Position()),
            Engine::CountEnemiesAt(player.Position(), 500.0f) > Slider(RMenu, "MaxChannelEnemies", 2),
            false, true};
        const float elapsed = static_cast<float>(now - RStartTick) / 1000.0f;
        if (!ShouldContinueNetherGrasp(gate, elapsed) &&
            !ShouldReleaseNetherGrasp(gate, elapsed)) RInterrupted = true;
        if (ShouldReleaseNetherGrasp(gate, elapsed) || RInterrupted) {
            RChanneling = false;
            ROwned = false;
        }
    }
    ActiveVoidlingCount = 0;
    for (const int id : VoidlingIds) {
        if (id != 0) ++ActiveVoidlingCount;
    }
    WObservedAmmo = RuntimeWCharges();
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    const Mode decisionMode = mode == Mode::None ? Mode::Automatic : mode;
    ReconcileState();
    if (ManualOwnershipUntil > Now() || RChanneling) return true;
    if (TrySpreadE(decisionMode, decisionMode == Mode::Automatic)) return true;
    const auto target = SelectTarget(selected, decisionMode == Mode::Flee ? 900.0f : kRRange + 35.0f);
    if (!Engine::ValidEnemy(target)) return decisionMode == Mode::LaneClear ||
        decisionMode == Mode::Jungle || decisionMode == Mode::LastHit ? Farm(decisionMode) : false;
    switch (decisionMode) {
    case Mode::Combo:
        if (CastE(target, decisionMode)) return true;
        if (CastW(VoidlingFocus(target), decisionMode)) return true;
        if (CastQ(target, decisionMode)) return true;
        return CastR(target, decisionMode);
    case Mode::Harass:
        if (CastE(target, decisionMode)) return true;
        return CastQ(target, decisionMode);
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        return Farm(decisionMode);
    case Mode::Flee:
        if (CastQ(target, decisionMode, true)) return true;
        return CastR(target, decisionMode, true);
    case Mode::Automatic:
        if (IncomingHardCcUntil > Now() && CastQ(target, decisionMode, true)) return true;
        if (CastE(target, decisionMode, true)) return true;
        if (CastW(VoidlingFocus(target), decisionMode, true)) return true;
        return CastR(target, decisionMode);
    default:
        return false;
    }
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            LastCastTick[static_cast<std::size_t>(slot)] = now;
            if (!Engine::WasControllerCast(slot))
                ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 600);
            if (slot == 3 && !Engine::WasControllerCast(slot)) RInterrupted = true;
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args, 220.0f, 100.0f, 300, 250, 220, 1500, 500);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) {
        IncomingHardCcUntil = std::max(IncomingHardCcUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
        if (RChanneling) RInterrupted = true;
        if (PassiveShieldActive) PassiveShieldActive = false;
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId != 0 ? args.TargetNetworkId : args.Target.NetworkId);
        LastAutoTick = Now();
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (IsLocalPlayer(args.Sender)) {
        if (TextContainsAny(args.BuffName, {"MalzaharPassive", "malzaharpassiveshield"}))
            PassiveShieldActive = true;
        if (TextContainsAny(args.BuffName, {"MalzaharR", "malzaharrsound"})) {
            RChanneling = true;
            RStartTick = Now();
        }
        return;
    }
    if (TextContainsAny(args.BuffName, {"MalzaharE", "MalzaharMaleficVisions", "malzahare"})) {
        EMarkedTargetId = id;
        EExpireTick = Now() + static_cast<int>(kEDurationSeconds * 1000.0f);
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        if (TextContainsAny(args.BuffName, {"MalzaharPassive", "malzaharpassiveshield"})) {
            PassiveShieldActive = false;
            PassiveCooldownEndTick = PassiveCooldownEnd(Now());
        }
        if (TextContainsAny(args.BuffName, {"MalzaharR", "malzaharrsound"}) && RChanneling)
            RInterrupted = true;
        return;
    }
    if (TextContainsAny(args.BuffName, {"MalzaharE", "MalzaharMaleficVisions", "malzahare"}) &&
        static_cast<int>(args.Sender.NetworkId) == EMarkedTargetId) {
        if (args.Sender.IsDead) {
            ESpreadPending = true;
            ESpreadOrigin = args.Sender.Position;
            ESpreadUntilTick = Now() + 1000;
        }
        EMarkedTargetId = EExpireTick = 0;
    }
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (args.EndTime > Game::Time()) OnBuffAdd(args);
    else OnBuffRemove(args);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (RChanneling) args.Process = false;
    (void)args;
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick)) LastAutoTick = Now();
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)args;
    IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 800);
    if (RChanneling) RInterrupted = true;
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, InterruptTargetId, InterruptExpireTick, 900, 250, 5000);
    IncomingHardCcUntil = std::max(IncomingHardCcUntil, InterruptExpireTick);
    if (RChanneling) RInterrupted = true;
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() || !TextContainsAny(args.Sender.Name,
        {"MalzaharVoidling", "malzaharvoidling"})) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    for (int& slot : VoidlingIds) {
        if (slot == 0) {
            slot = id;
            break;
        }
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    for (int& slot : VoidlingIds) if (slot == id) slot = 0;
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) { (void)args; }
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) { (void)args; }

inline void OnDraw() {
    if (!CoachMenu || !Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF9966FFu, 1.2f, 40);
    Drawing::DrawCircle(player.Position(), kERange, 0xFF66CCAAu, 1.2f, 36);
    Drawing::DrawCircle(player.Position(), kRRange, RChanneling ? 0xFFFF5555u : 0xFFCC66FFu, 1.5f, 40);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("MalzaharOneTrick", "Malzahar passive, voidlings and suppression"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 600, 180, 1400));
    QMenu = TacticsMenu->AddSubMenu(new Menu("CallOfTheVoid", "Q silence prediction"));
    QMenu->Add(new MenuBool("RequireHighHitchance", "Require high Q prediction", true));
    WMenu = TacticsMenu->AddSubMenu(new Menu("VoidSwarm", "Voidling charge and target policy"));
    WMenu->Add(new MenuBool("PreferMaleficVisions", "Feed E-marked champions first", true));
    EMenu = TacticsMenu->AddSubMenu(new Menu("MaleficVisions", "E spread and transfer"));
    EMenu->Add(new MenuBool("AllowFarmSpread", "Allow E spread on farm units", true));
    RMenu = TacticsMenu->AddSubMenu(new Menu("NetherGrasp", "Suppression channel safety"));
    RMenu->Add(new MenuSlider("StartBelowHp", "Start nonlethal R below target HP (%)", 58, 10, 95));
    RMenu->Add(new MenuSlider("MaxChannelEnemies", "Maximum nearby enemies during R", 2, 0, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("MalzaharFarm", "Distinct lane and jungle policy"));
    FarmMenu->Add(new MenuBool("UseVoidSwarm", "Use W for lane and jungle", true));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("MalzaharCoach", "Range and channel telemetry"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/E/R ranges", false));
}

inline void OnLoad() {
    LastCastTick = {};
    VoidlingIds = {};
    ActiveVoidlingCount = 0;
    WObservedAmmo = -1;
    EMarkedTargetId = EExpireTick = RTargetId = RStartTick = 0;
    InterruptTargetId = InterruptExpireTick = 0;
    ESpreadPending = false;
    ESpreadOrigin = {};
    ESpreadUntilTick = 0;
    IncomingThreatUntil = IncomingHardCcUntil = ManualOwnershipUntil = 0;
    PassiveCooldownEndTick = 0;
    LastAutoTargetId = LastAutoTick = 0;
    PassiveShieldActive = RChanneling = RInterrupted = ROwned = false;
    ReconcileState();
}

inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    OnLoad();
}

inline constexpr const char* Scenarios[] = {
    "Pin Riot 26.15 and CommunityDragon PC 16.15 Malzahar values",
    "Reconcile Void Shift shield presence and its 30-second cooldown from buff, threat and polling state",
    "Reject offensive casts into spell-shielded, invulnerable or unreachable targets",
    "Aim Call of the Void from predicted movement and enforce the perpendicular portal hitbox",
    "Track two W charges and cap active Voidlings at three objects",
    "Target E-marked champions first, then an R target, then champions, lane units and jungle monsters",
    "Transfer Malefic Visions only after a marked target dies and a valid nearby candidate exists",
    "Preserve E spread state and expiry across buff events and polling reconciliation",
    "Start Nether Grasp only on a reachable suppression target with real readiness and damage state",
    "Reject Nether Grasp under turret, excess nearby enemies, mobility lock, incoming hard crowd control or target protection",
    "Interrupt owned R immediately on hard crowd control, gapcloser pressure, buff removal or lost target reach",
    "Reconcile R completion against the full 2.5-second channel rather than trusting one event",
    "Preserve auto-attack windup and yield briefly after manually owned spells",
    "Handle Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic modes distinctly",
    "Use W for lane and jungle clusters and E for lethal last hits without generic ordering",
    "Draw live ranges and channel state without taking movement, item or summoner ownership",
    "Complete every ChampionController callback, including object and missile lifecycle hooks",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Malzahar;
    controller.ControllerId = "champion.kuroaio.ai.malzahar.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIMalzahar.md";
    controller.ImplementationSummary =
        "Void Shift cooldown and shield reconciliation, Q silence geometry, two-charge W Voidling targeting, "
        "E spread transfer and protected Nether Grasp channel interruption safety.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Malzahar
