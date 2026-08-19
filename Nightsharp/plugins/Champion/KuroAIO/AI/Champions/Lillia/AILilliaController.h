#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AILilliaGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Lillia {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::Now;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Protected;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline DreamState Dream{};
inline MarkState Marks{};
inline std::array<int, 4> LastCastTick{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCcUntil = 0;
inline Mode LastMode = Mode::None;
inline Vector3 LastEAim{};
inline Vector3 LastWAim{};

inline bool Throttle(int slot, int delay = 80) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
inline bool Ready(int slot, Mode mode) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() &&
        (mode == Mode::None || SpellEnabled(slot, mode));
}
inline bool Lethal(const AIHeroClient& target, int slot) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) || slot < 0 || slot > 3 ||
        !Engine::RuntimeSpells[slot]) return false;
    return player.CalculateMagicDamage(target, Engine::RuntimeSpells[slot]->GetDamage(target)) >=
        target.Health() + target.AllShield();
}
inline AIHeroClient Target(float range, const AIHeroClient& selected) {
    return ControllerHelpers::PreferredEnemyTarget(selected, range);
}
inline bool TargetMarked(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) &&
        (HasMark(Marks, static_cast<int>(target.NetworkId()), Now()) ||
         target.HasBuff("LilliaRMark") || target.HasBuff("LilliaR"));
}
inline bool SafePosition(const AIHeroClient& target, bool lethal = false,
                         bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), 575.0f);
    const SafetyContext context{
        Engine::ValidEnemy(target), lethal, defensive,
        Engine::UnderEnemyTurret(player.Position()), Orbwalker::IsWindingUp(),
        ManualOwnershipUntil > Now(), enemies, Slider(RMenu, "MaxCommitEnemies", 3)};
    return SafeCommit(context);
}
inline bool ManaOkay(Mode mode, int slot) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (mode == Mode::Harass) return player.ManaPercent() >= Slider(FarmMenu, "HarassMana", 52);
    if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit)
        return player.ManaPercent() >= Slider(FarmMenu, "ClearMana", 42);
    (void)slot;
    return true;
}
inline bool EBlockedByMinion(const Vec3& origin, const Vec3& aim,
                             const AIHeroClient& target) {
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable() ||
            minion.NetworkId() == target.NetworkId()) continue;
        if (origin.Distance2D(minion.Position()) >= origin.Distance2D(target.Position())) continue;
        if (LineHits(origin, aim, minion.Position(), kERange, kEWidth,
                     minion.BoundingRadius())) return true;
    }
    return false;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + 35.0f) ||
        !Ready(0, mode) || !Throttle(0) || !ManaOkay(mode, 0) ||
        Protected(target) || PreserveAttack(reactive, lethal)) return false;
    const Vec3 predicted = PredictPosition(target, kQDelay);
    if (!QReachable(player.Position(), predicted, target.BoundingRadius()) ||
        !QOuterRingHits(player.Position(), predicted, target.BoundingRadius()) &&
            !QInnerAreaHits(player.Position(), predicted, target.BoundingRadius())) return false;
    const bool underTurret = Engine::UnderEnemyTurret(player.Position());
    const int nearby = Engine::CountEnemiesAt(player.Position(), 575.0f);
    const bool preferredKite = SafeKite(player.Position().Distance2D(predicted),
        320.0f, nearby, Slider(RMenu, "MaxCommitEnemies", 3));
    if (underTurret && !reactive && !lethal) return false;
    if (!reactive && !lethal && !preferredKite &&
        !QInnerAreaHits(player.Position(), predicted, target.BoundingRadius())) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    Dream = ApplyDream(Dream, Now());
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kWRange + 50.0f) ||
        !Ready(1, mode) || !Throttle(1) || !ManaOkay(mode, 1) ||
        Protected(target) || PreserveAttack(reactive, lethal)) return false;
    const Vec3 aim = PredictPosition(target, kWDelay);
    const Vec3 impact = PredictPosition(target, kWDelay);
    if (!ValidPoint(aim) || player.Position().Distance2D(aim) > kWRange + target.BoundingRadius() ||
        !CircleHits(aim, impact, kWRadius, target.BoundingRadius())) return false;
    const bool center = CircleHits(aim, impact, kWCenterRadius, target.BoundingRadius());
    if (Bool(WMenu, "RequireCenter", true) && !center && !reactive && !lethal) return false;
    const bool lethalStrike = lethal || (center && Lethal(target, 1));
    if (!SafePosition(target, lethalStrike, reactive)) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    Dream = ApplyDream(Dream, Now());
    LastWAim = aim;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kERange + 55.0f) ||
        !Ready(2, mode) || !Throttle(2) || !ManaOkay(mode, 2) ||
        Protected(target) || PreserveAttack(reactive, lethal)) return false;
    const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    const Vec3 aim = prediction.GetCastPosition().IsValid()
        ? prediction.GetCastPosition() : PredictPosition(target, kEDelay +
            player.Position().Distance2D(target.Position()) / kESpeed);
    if (!ValidPoint(aim) || player.Position().Distance2D(aim) > kERange + target.BoundingRadius() ||
        (!reactive && !lethal && prediction.Hitchance < SDK::HitChance::High) ||
        !LineHits(player.Position(), aim, PredictPosition(target, kEDelay), kERange,
                   kEWidth, target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kEWidth * 0.5f) ||
        EBlockedByMinion(player.Position(), aim, target)) return false;
    if (!SafePosition(target, lethal, reactive)) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    Dream = ApplyDream(Dream, Now());
    Marks = MarkTarget(Marks, static_cast<int>(target.NetworkId()), Now());
    LastEAim = aim;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kRRange) ||
        !Ready(3, mode) || !Throttle(3, 160) || !TargetMarked(target) ||
        Protected(target)) return false;
    const bool lethal = Lethal(target, 3);
    const bool defensive = reactive || IncomingHardCcUntil > Now() ||
        player.HealthPercent() <= Slider(RMenu, "DefensiveHealth", 34);
    const int nearby = Engine::CountEnemiesAt(player.Position(), 650.0f);
    if (!ShouldSleep(true, true, lethal, defensive,
                    Engine::UnderEnemyTurret(player.Position()), nearby,
                    Slider(RMenu, "MinimumSleepTargets", 1))) return false;
    if (!SafePosition(target, lethal, defensive)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    return true;
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "ClearMana", 42) ||
        !Ready(0, mode) || !Throttle(0) || !Engine::TryFarm(mode)) return;
    Dream = ApplyDream(Dream, Now());
    LastCastTick[0] = Now();
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (TargetMarked(target) && CastR(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastQ(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastE(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (TargetMarked(target) && CastR(target, Mode::Flee, true)) return;
    if (CastE(target, Mode::Flee, true)) return;
    (void)CastQ(target, Mode::Flee, true);
}
inline void ReconcileState() {
    const int now = Now();
    Dream = ExpireDream(Dream, now);
    Marks = ExpireMarks(Marks, now);
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.HasBuff("LilliaP") || player.HasBuff("LilliaPassive")) {
        const int observed = std::clamp(Dream.Stacks, 1, kMaxDreamStacks);
        Dream = ReconcileDream(Dream, observed, now);
    }
    if (ManualOwnershipUntil <= now) ManualOwnershipUntil = 0;
    if (IncomingThreatUntil <= now) IncomingThreatUntil = 0;
    if (IncomingHardCcUntil <= now) IncomingHardCcUntil = 0;
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const auto target = Target(mode == Mode::Flee ? kRRange : kERange, selected);
    const auto player = GameObjects::Player();
    if (player.IsValid() && IncomingHardCcUntil > Now() && Engine::ValidEnemy(target))
        if (TargetMarked(target) && CastR(target, mode, true)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, kRRange)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target)) {
            if (TargetMarked(target) && CastR(target, mode)) return true;
            (void)CastQ(target, mode, true);
        }
        break;
    default: break;
    }
    return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = args.Slot;
        if (slot >= 0 && slot < 4) {
            LastCastTick[slot] = now;
            if (!Engine::WasControllerCast(slot))
                ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 600);
            if (slot == 0 || slot == 1 || slot == 2) Dream = ApplyDream(Dream, now);
        }
        return;
    }
    if (args.TargetNetworkId == GameObjects::Player().NetworkId()) {
        IncomingThreatUntil = std::max(IncomingThreatUntil, now + 450);
        if (args.IsSpell) IncomingHardCcUntil = std::max(IncomingHardCcUntil, now + 350);
    }
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    (void)ControllerHelpers::CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "lilliap") ||
            Engine::TextContains(args.BuffName, "dream"))
            Dream = ReconcileDream(Dream, args.Count > 0 ? args.Count : Dream.Stacks + 1, now);
        return;
    }
    if (Engine::TextContains(args.BuffName, "lilliar") &&
        (Engine::TextContains(args.BuffName, "mark") || Engine::TextContains(args.BuffName, "sleep")))
        Marks = MarkTarget(Marks, static_cast<int>(args.Sender.NetworkId), now);
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender) &&
        (Engine::TextContains(args.BuffName, "lilliap") || Engine::TextContains(args.BuffName, "dream")))
        Dream = {};
    if (!IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "lilliar")) {
        for (auto& mark : Marks.Marks)
            if (mark.TargetId == static_cast<int>(args.Sender.NetworkId)) mark = {};
    }
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (ManualOwnershipUntil > Now() && Bool(TacticsMenu, "ProtectManual", true)) args.Process = false;
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFAA66DDu, 1.2f, 40);
    Drawing::DrawCircle(player.Position(), kWRange, 0xFFDD9966u, 1.0f, 40);
    Drawing::DrawCircle(player.Position(), kERange, 0xFF66CC99u, 1.0f, 40);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("LilliaTactics", "Dream-Laden Bough tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 600, 180, 1400));
    TacticsMenu->Add(new MenuBool("ProtectManual", "Protect manual spell ownership", true));
    QMenu = TacticsMenu->AddSubMenu(new Menu("LilliaQ", "Blooming Blows rings"));
    QMenu->Add(new MenuBool("PreferOuterRing", "Prefer outer true-damage ring", true));
    WMenu = TacticsMenu->AddSubMenu(new Menu("LilliaW", "Watch Out! Eep! center"));
    WMenu->Add(new MenuBool("RequireCenter", "Require center strike for combo", true));
    EMenu = TacticsMenu->AddSubMenu(new Menu("LilliaE", "Swirlseed bowling"));
    EMenu->Add(new MenuBool("RequireHighHitChance", "Require high prediction", true));
    RMenu = TacticsMenu->AddSubMenu(new Menu("LilliaR", "Lilting Lullaby sleep"));
    RMenu->Add(new MenuSlider("MinimumSleepTargets", "Minimum marked targets", 1, 1, 4));
    RMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum nearby commit enemies", 3, 1, 5));
    RMenu->Add(new MenuSlider("DefensiveHealth", "Defensive sleep health percent", 34, 10, 70));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("LilliaFarm", "Conservative Q farm"));
    FarmMenu->Add(new MenuSlider("HarassMana", "Harass mana reserve", 52, 0, 100));
    FarmMenu->Add(new MenuSlider("ClearMana", "Clear mana reserve", 42, 0, 100));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("LilliaCoach", "Route visualization"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/W/E ranges", false));
}
inline void OnLoad() {
    Dream = {};
    Marks = {};
    LastCastTick.fill(0);
    LastAutoTargetId = LastAutoTick = ManualOwnershipUntil = 0;
    IncomingThreatUntil = IncomingHardCcUntil = 0;
    LastMode = Mode::None;
    LastEAim = LastWAim = {};
}
inline void OnUnload() { OnLoad(); }

inline constexpr const char* Scenarios[] = {
    "Track Dream-Laden Bough movement stacks and six-second expiry from buffs and polling",
    "Reconcile local passive state when a buff event is delayed or omitted",
    "Apply Dream stacks on Q W and E casts without inventing stacks from unrelated spells",
    "Prefer Q outer-ring true damage while accepting the inner magic ring at valid reach",
    "Reject Q targets outside the 450 radius or across an unsafe turret commitment",
    "Use W predicted center strike only when its delayed impact can hit the target",
    "Respect W center-strike multiplier and preserve ordinary attack windup",
    "Roll E Swirlseed through predicted clear corridors with high hitchance and wall rejection",
    "Reject E paths blocked by projectile walls or outside 1600 range",
    "Track R Dream marks by target id, expiration and buff add/remove reconciliation",
    "Cast Lilting Lullaby only on marked targets with lethal defensive or multi-target value",
    "Reject ordinary sleep casts under turret or excessive nearby-enemy pressure",
    "Use movement stacks and preferred distance for kiting and zone safety decisions",
    "Prefer the selected target, then orbwalker target, for every champion route",
    "Protect manual spell ownership and auto-attack windup from synthetic casts",
    "Support Combo Harass LaneClear Jungle LastHit Flee and Automatic modes distinctly",
    "Apply real damage, shield, resource, cooldown, prediction and target-validity gates",
    "Keep pure stacks marks rings projectile and safety mechanics in standalone geometry",
    "Keep ChampionController ABI and shared catalog registration untouched",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Lillia;
    controller.ControllerId = "champion.kuroaio.ai.lillia.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AILillia.md";
    controller.ImplementationSummary =
        "Dream stack movement, Q outer/inner rings, W center strike, E bowling projectile, and marked R sleep with kiting safety.";
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
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Lillia
