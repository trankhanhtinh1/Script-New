#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AISonaGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Sona {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::ProjectileWallBlocks;
using ControllerHelpers::Ready;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::Now;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int SelectedAllyId = 0;
inline int ManualOwnershipUntil = 0;
inline int EnemyThreatUntil = 0;
inline int HardCcThreatUntil = 0;
inline int InterruptTargetId = 0;
inline int InterruptUntil = 0;
inline int RLastCastTick = 0;
inline int PowerChordStacks = 0;
inline int AccelerandoStacks = 0;
inline Aura CurrentAura = Aura::None;
inline PowerChord ReadyChord = PowerChord::None;
inline Vector3 LastRAim{};

inline bool Throttle(int slot, int delay = 44) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay) &&
        ControllerHelpers::CastThrottleReady(slot, delay, -1);
}

inline bool ProtectedTarget(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}

inline bool SafeAura(bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    return UnsafeAuraCommit(Engine::CountEnemiesAt(player.Position(), 500.0f),
                            Engine::UnderEnemyTurret(player.Position()),
                            Slider(TacticsMenu, "MaxTurretEnemies", 2), defensive);
}

inline AIHeroClient SelectEnemy(const AIHeroClient& selected, float range) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    const auto orb = ControllerHelpers::OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orb, range)) return orb;
    return Engine::SelectTarget(range);
}

inline AIHeroClient SelectAlly(bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    auto ally = SelectProtectionAlly(1000.0f, SelectedAllyId,
                                     SelectedAllyId == 0 ? 0 : Now() + 300,
                                     300.0f, 600.0f);
    if (Engine::ValidAlly(ally, 1000.0f) &&
        ally.Position().Distance2D(player.Position()) <= kAuraRange) {
        SelectedAllyId = static_cast<int>(ally.NetworkId());
        return ally;
    }
    if (defensive && player.HealthPercent() <=
            Slider(TacticsMenu, "PlayerHealthThreshold", 42)) return player;
    return {};
}

inline bool HasLowAlly(bool includePlayer = true) {
    const auto player = GameObjects::Player();
    if (includePlayer && player.IsValid() && player.HealthPercent() <=
            Slider(WMenu, "PlayerHealthThreshold", 52)) return true;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (Engine::ValidAlly(ally, 1000.0f) &&
            ally.Position().Distance2D(player.Position()) <= kAuraRange &&
            ally.HealthPercent() <= Slider(WMenu, "AllyHealthThreshold", 62)) return true;
    }
    return false;
}

inline float QRawDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0.0f;
    const int rank = std::clamp(SpellRank(0), 1, 5);
    const float base = 50.0f + 35.0f * static_cast<float>(rank - 1);
    const float chord = ReadyChord == PowerChord::Staccato ?
        30.0f + 15.0f * static_cast<float>(rank - 1) : 0.0f;
    return player.CalculateMagicDamage(target, base + player.AP() * 0.4f + chord);
}
inline float RRawDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0.0f;
    const int rank = std::clamp(SpellRank(3), 1, 3);
    const float base = 150.0f + 100.0f * static_cast<float>(rank - 1);
    return player.CalculateMagicDamage(target, base + player.AP() * 0.5f);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + 50.0f) ||
        ProtectedTarget(target) || !Ready(0, mode) || !Throttle(0) ||
        ControllerHelpers::PreserveAttack(reactive) ||
        player.ManaPercent() < Slider(QMenu, "MinimumMana", 38)) return false;
    const Vector3 predicted = PredictPosition(target, TravelSeconds(
        player.Position().Distance2D(target.Position())));
    if (!QTargetReachable(player.Position(), predicted, target.BoundingRadius()) ||
        !QProjectileContacts(player.Position(), predicted, target.Position(),
                             target.BoundingRadius(), kQMissileWidth) ||
        ProjectileWallBlocks(player.Position(), predicted, kQMissileWidth * 0.5f))
        return false;
    if (!reactive && Engine::CountEnemiesAt(player.Position(), 450.0f) >
            Slider(QMenu, "MaxPokeEnemies", 2)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1) ||
        ControllerHelpers::PreserveAttack(reactive) || !HasLowAlly(true) ||
        !SafeAura(reactive)) return false;
    const auto ally = SelectAlly(reactive);
    const bool allyNeeds = Engine::ValidAlly(ally) &&
        ally.HealthPercent() <= Slider(WMenu, "AllyHealthThreshold", 62);
    const bool playerNeeds = player.HealthPercent() <=
        Slider(WMenu, "PlayerHealthThreshold", 52);
    if (!allyNeeds && !playerNeeds) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || !Throttle(2) ||
        ControllerHelpers::PreserveAttack(reactive)) return false;
    const auto ally = SelectAlly(false);
    const bool allyThreat = Engine::ValidAlly(ally) &&
        (Engine::CountEnemiesAt(ally.Position(), 650.0f) > 0 ||
         ally.IsMoving());
    const bool escape = mode == Mode::Flee ||
        EnemyThreatUntil > Now() || HardCcThreatUntil > Now();
    if (!allyThreat && !escape && mode != Mode::Combo) return false;
    if (!SafeAura(reactive || escape)) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastR(const AIHeroClient& selected, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 120) ||
        ControllerHelpers::PreserveAttack(reactive)) return false;
    AIHeroClient target = SelectEnemy(selected, kRRange);
    if (!Engine::ValidEnemy(target, kRRange) || ProtectedTarget(target)) return false;
    const Vector3 predicted = PredictPosition(target, TravelSeconds(
        player.Position().Distance2D(target.Position()), kCastDelay, kRSpeed));
    const Vector3 endpoint = ClampUltimateEndpoint(player.Position(), predicted);
    const int nearby = Engine::CountEnemiesAt(player.Position(), 600.0f);
    const bool defensive = reactive || mode == Mode::Flee ||
        HardCcThreatUntil > Now() || InterruptUntil > Now();
    if (!ConeContacts(player.Position(), endpoint, predicted,
                      target.BoundingRadius(), kRWidth) ||
        !UltimateWallSafe(player.Position(), endpoint,
                          ProjectileWallBlocks(player.Position(), endpoint,
                                               kRWidth * 0.5f))) return false;
    const float rDamage = RRawDamage(target);
    const bool lethal = rDamage >= target.Health() + target.AllShield();
    if (!defensive && nearby < Slider(RMenu, "MinimumTargets", 2) &&
        target.HealthPercent() > Slider(RMenu, "ExecuteHealth", 32) && !lethal) return false;
    if (!defensive && Engine::UnderEnemyTurret(player.Position()) &&
        nearby > Slider(RMenu, "MaxTurretEnemies", 2)) return false;
    if (!Engine::ControllerCastPosition(3, endpoint)) return false;
    LastCastTick[3] = RLastCastTick = Now();
    LastRAim = endpoint;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (CastR(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(Mode::Combo)) return;
    (void)CastE(Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(QMenu, "HarassMana", 48)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(Mode::Harass);
}

inline void Flee(const AIHeroClient& target) {
    if (CastE(Mode::Flee, true)) return;
    if (CastW(Mode::Flee, true)) return;
    (void)CastR(target, Mode::Flee, true);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "Mana", 35)) return;
    if (mode == Mode::Jungle && ControllerHelpers::HasNearbyJungleTarget(kQRange)) {
        const auto monster = ControllerHelpers::SelectJungleTarget(kQRange);
        if (monster.IsValid() && Ready(0, mode) && Throttle(0) && SafeAura())
            (void)Engine::ControllerCastSelf(0);
    } else if (mode == Mode::LaneClear || mode == Mode::LastHit) {
        (void)Engine::TryFarm(mode);
    }
}

inline void Automatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (HasLowAlly(true) && CastW(Mode::Automatic, true)) return;
    if ((HardCcThreatUntil > Now() || EnemyThreatUntil > Now()) &&
        CastE(Mode::Automatic, true)) return;
    (void)CastR(target, Mode::Automatic, true);
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    if (player.HasBuff("SonaQ")) CurrentAura = Aura::HymnOfValor;
    else if (player.HasBuff("SonaW")) CurrentAura = Aura::AriaOfPerseverance;
    else if (player.HasBuff("SonaE")) CurrentAura = Aura::SongOfCelerity;
    AccelerandoStacks = std::clamp(player.GetBuffCount("SonaPassiveAccelerandoCount"), 0, 120);
    if (PowerChordStacks >= 3) ReadyChord = ChordForAura(CurrentAura);
    else ReadyChord = PowerChord::None;
    if (ManualOwnershipUntil <= now) ManualOwnershipUntil = 0;
    if (EnemyThreatUntil <= now) EnemyThreatUntil = 0;
    if (HardCcThreatUntil <= now) HardCcThreatUntil = 0;
    if (InterruptUntil <= now) InterruptTargetId = InterruptUntil = 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const AIHeroClient target = SelectEnemy(selected, mode == Mode::Flee ? 1000.0f : kRRange);
    if (mode == Mode::Automatic) Automatic(target);
    else if (mode == Mode::Combo) Combo(target);
    else if (mode == Mode::Harass) Harass(target);
    else if (mode == Mode::Flee) Flee(NearestEnemyToPlayer(target, 1000.0f));
    else if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) Farm(mode);
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (args.IsAutoAttack) return;
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            LastCastTick[slot] = now;
            if (slot < 3) {
                CurrentAura = AuraFromSpell(slot);
                PowerChordStacks = AdvanceChordStacks(PowerChordStacks);
                ReadyChord = ChordToConsume(PowerChordStacks, CurrentAura);
            }
            if (!Engine::WasControllerCast(slot))
                ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 600);
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    EnemyThreatUntil = std::max(EnemyThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl)
        HardCcThreatUntil = std::max(HardCcThreatUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick)) return;
    if (!ChordReady(PowerChordStacks)) return;
    ReadyChord = ChordForAura(CurrentAura);
    PowerChordStacks = 0;
    ReadyChord = PowerChord::None;
}

inline void OnBuffState(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "SonaQ")) {
        if (added) CurrentAura = Aura::HymnOfValor;
    } else if (Engine::TextContains(args.BuffName, "SonaW")) {
        if (added) CurrentAura = Aura::AriaOfPerseverance;
    } else if (Engine::TextContains(args.BuffName, "SonaE")) {
        if (added) CurrentAura = Aura::SongOfCelerity;
    } else if (Engine::TextContains(args.BuffName, "SonaPassiveAccelerando")) {
        AccelerandoStacks = std::clamp(args.Count, 0, 120);
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) { OnBuffState(args, true); }
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) { OnBuffState(args, false); }
inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) { OnBuffState(args, true); }

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    const auto player = GameObjects::Player();
    if (player.IsValid() && (args.IsDirectedToPlayer ||
            (args.End.IsValid() && args.End.Distance2D(player.Position()) < 450.0f))) {
        EnemyThreatUntil = std::max(EnemyThreatUntil, Now() + 700);
        HardCcThreatUntil = std::max(HardCcThreatUntil, Now() + 450);
    }
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    InterruptTargetId = static_cast<int>(args.NetworkId);
    const int remaining = args.EndTime > Game::Time()
        ? static_cast<int>((args.EndTime - Game::Time()) * 1000.0f)
        : 900;
    InterruptUntil = Now() + std::clamp(remaining, 250, 5000);
    HardCcThreatUntil = std::max(HardCcThreatUntil, InterruptUntil);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF62D5E8u, 1.4f, 36);
    Drawing::DrawCircle(player.Position(), kAuraRange, 0xFF8C6BFFu, 1.2f, 32);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFE76BFFu, 1.0f, 36);
    if (LastRAim.IsValid() && !LastRAim.IsZero())
        Drawing::DrawLine(player.Position(), LastRAim, 0xFFE76BFFu, 1.0f);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("SonaTactics", "Sona aura and support tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 600, 180, 1400));
    TacticsMenu->Add(new MenuSlider("MaxTurretEnemies", "Max enemies under turret for aura", 2, 0, 5));
    TacticsMenu->Add(new MenuSlider("PlayerHealthThreshold", "Emergency self-health %", 42, 10, 90));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Hymn of Valor poke"));
    QMenu->Add(new MenuSlider("MinimumMana", "Minimum Q mana %", 38, 0, 90));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana %", 48, 10, 90));
    QMenu->Add(new MenuSlider("MaxPokeEnemies", "Max nearby enemies while poking", 2, 0, 5));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Aria of Perseverance support"));
    WMenu->Add(new MenuSlider("AllyHealthThreshold", "Heal ally below health %", 62, 10, 95));
    WMenu->Add(new MenuSlider("PlayerHealthThreshold", "Heal self below health %", 52, 10, 95));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Song of Celerity movement"));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Crescendo"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum non-defensive hits", 2, 1, 5));
    RMenu->Add(new MenuSlider("ExecuteHealth", "Allow single low-health target %", 32, 5, 75));
    RMenu->Add(new MenuSlider("MaxTurretEnemies", "Max enemies under turret for engage", 2, 0, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("SonaFarm", "Resource-safe farm"));
    FarmMenu->Add(new MenuSlider("Mana", "Farm mana floor %", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("SonaCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q aura and R ranges", false));
}

inline void OnLoad() {
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = SelectedAllyId = 0;
    ManualOwnershipUntil = EnemyThreatUntil = HardCcThreatUntil = 0;
    InterruptTargetId = InterruptUntil = RLastCastTick = 0;
    PowerChordStacks = AccelerandoStacks = 0;
    CurrentAura = Aura::None;
    ReadyChord = PowerChord::None;
    LastRAim = {};
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    LastRAim = {};
}

inline constexpr const char* Scenarios[] = {
    "Pin aura and damage constants to Riot 26.15 and CommunityDragon 16.15",
    "Reconcile Q W E aura buffs and Accelerando stacks from both events and polling",
    "Advance exactly one Power Chord stack per basic spell and consume it on the next attack",
    "Map Hymn Staccato Aria Diminuendo and Celerity Tempo to the active aura",
    "Respect selected target first, then orbwalker target, then engine fallback",
    "Predict Q missile contact and reject projectile-wall paths or protected targets",
    "Cast Aria only for a real low-health ally or player inside the 400 aura",
    "Cast Celerity for grouped ally movement, chase or escape without turret overcommit",
    "Predict piercing Crescendo endpoint and enforce 140 width and wall checks",
    "Use Crescendo for multi-hit engage, low-health secure or incoming hard crowd control",
    "Preserve auto attack windup unless a defensive reaction is urgent",
    "Yield to manual Q W E R ownership before polling decisions resume",
    "Automatic mode is defensive and cannot start a fresh engage without threat",
    "Combo prioritizes Crescendo then Q poke, W sustain and E follow-up speed",
    "Harass uses Q with a mana floor and W only inside an actual heal window",
    "LaneClear Jungle and LastHit retain resource-safe farm policy without random ultimates",
    "Flee prioritizes Celerity, then Aria, then defensive Crescendo peel",
    "Capture gapcloser and interruptable threats for reactive aura or Crescendo policy",
    "Draw Q aura and R reach without changing decisions",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Sona";
    controller.ControllerId = "champion.kuroaio.ai.sona.support";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AISona.md";
    controller.ImplementationSummary =
        "Distinct aura/Power Chord state machine with ally-proximity healing, movement support, Q poke and Crescendo reach gating.";
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
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Sona
