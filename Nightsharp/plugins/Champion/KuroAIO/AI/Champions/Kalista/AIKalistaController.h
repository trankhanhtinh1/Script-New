#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIMarksmanControllerHelpers.h"
#include "AIKalistaGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Kalista {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::HasNearbyEpicMonster;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictionAtLeast;
using ControllerHelpers::RawAllyHeroByNetworkId;
using ControllerHelpers::SelectJungleTarget;
using ControllerHelpers::SpellEventNameContainsAny;

inline Menu* TacticsMenu = nullptr;
inline Menu* RendMenu = nullptr;
inline Menu* HopMenu = nullptr;
inline Menu* FateMenu = nullptr;
inline Menu* FarmMenu = nullptr;

inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int OwnedFocusTargetId = 0;
inline int OwnedFocusUntil = 0;
inline int PendingHopTick = 0;
inline int PendingHopTargetId = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline Mode LastMode = Mode::None;
inline AIHeroClient LastSmartTarget = {};

inline int SpearStacks(const AIBaseClient& target) {
    return target.IsValid()
        ? ClampSpearStacks(ControllerHelpers::MaximumBuffCount(
            target, {"kalistaexpungemarker", "KalistaExpungeMarker", "kalistaexpungemarkerbuff"}))
        : 0;
}

inline bool HasSpear(const AIBaseClient& target) {
    return SpearStacks(target) > 0;
}

inline bool IsOathsworn(const AIHeroClient& ally) {
    return ally.IsValid() && !ally.IsMe() && !ally.IsDead() &&
        ControllerHelpers::HasAnyBuff(
            ally, {"kalistacoopstrikeally", "KalistaCoopStrikeAlly"});
}

inline AIHeroClient OathswornAlly() {
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (IsOathsworn(ally)) return ally;
    }
    return {};
}

inline float RendDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid() || !HasSpear(target)) return 0.0f;
    const bool epic = target.IsMinion() && IsEpicMonster(target);
    const float raw = RendRawDamage(
        ControllerHelpers::SpellRank(2), SpearStacks(target),
        player.TotalAttackDamage(), player.AP(), epic);
    return player.CalculatePhysicalDamage(target, raw);
}

inline float ConservativeRendDamage(const AIBaseClient& target) {
    const float finalDamage = RendDamage(target);
    return target.IsMinion() && IsEpicMonster(target)
        ? ConservativeObjectiveDamage(finalDamage, true) : finalDamage;
}

inline bool RendLethal(const AIBaseClient& target, bool objectiveSecure = false) {
    if (!target.IsValid() || !HasSpear(target)) return false;
    const float damage = objectiveSecure ? ConservativeRendDamage(target) : RendDamage(target);
    return damage >= target.Health() + target.AllShield();
}

inline bool SpearExpiryImminent(const AIBaseClient& target) {
    if (!HasSpear(target)) return false;
    float remaining = 0.0f;
    for (const char* alias :
         {"kalistaexpungemarker", "KalistaExpungeMarker",
          "kalistaexpungemarkerbuff"}) {
        const float candidate = BuffRemainingMs(target, alias);
        if (candidate > 0.0f &&
            (remaining <= 0.0f || candidate < remaining)) {
            remaining = candidate;
        }
    }
    return remaining > 0.0f && remaining <= 700.0f;
}

inline bool SafeAdditionalAuto(const AIHeroClient& target) {
    return Engine::ValidEnemy(target, 1000.0f) &&
        LocalAttackReadySoon(target, 300) &&
        !IsEscaping(target, 0.40f) &&
        Engine::CountEnemiesAt(
            GameObjects::Player().Position(), 650.0f) <= 1;
}

inline RendTargetKind RendKind(const AIBaseClient& target) {
    if (target.IsHero()) return RendTargetKind::Hero;
    const AIMinionClient minion(target.Handle());
    if (minion.IsValid() && minion.IsJungle()) {
        return IsEpicMonster(target)
            ? RendTargetKind::EpicMonster
            : RendTargetKind::JungleMonster;
    }
    return RendTargetKind::LaneMinion;
}

inline RendDecisionContext RendContext(const AIBaseClient& target) {
    RendDecisionContext context{};
    context.TargetKind = RendKind(target);
    context.SpearStacks = SpearStacks(target);
    context.MinimumEscapeStacks =
        Slider(RendMenu, "EscapeStacks",
               kDefaultEscapeSpearThreshold);
    context.Lethal =
        context.TargetKind != RendTargetKind::EpicMonster &&
        RendLethal(target);
    const auto player = GameObjects::Player();
    const float distance = player.IsValid()
        ? player.Position().Distance2D(target.Position()) : FLT_MAX;
    context.TargetEscaping =
        context.TargetKind == RendTargetKind::Hero &&
        distance >= kERange - 75.0f &&
        IsEscaping(target, 0.40f);
    context.SafeAdditionalAuto =
        context.TargetKind == RendTargetKind::Hero &&
        SafeAdditionalAuto(AIHeroClient(target.Handle()));
    context.SpearExpiryImminent =
        context.TargetKind == RendTargetKind::Hero &&
        SpearExpiryImminent(target);
    context.ObjectiveSecure =
        context.TargetKind == RendTargetKind::EpicMonster &&
        RendLethal(target, true);
    context.UnderEnemyTurret =
        Engine::UnderEnemyTurret(target.Position());
    return context;
}

inline bool QPrediction(const AIHeroClient& target,
                        SDK::PredictionOutput* output = nullptr) {
    if (!Engine::RuntimeSpells[0] || !Engine::ValidEnemy(target, kQRange + 40.0f)) {
        return false;
    }
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionHits(0, target, SDK::HitChance::High, true, &prediction) &&
        !PredictionProjectileWall(0, prediction, kQWidth);
    if (output) *output = prediction;
    return hit;
}

inline bool WPrediction(const AIHeroClient& target,
                        SDK::PredictionOutput* output = nullptr) {
    if (!Engine::RuntimeSpells[1] || !Engine::ValidEnemy(target, 5000.0f)) return false;
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionHits(1, target, SDK::HitChance::High, false, &prediction) &&
        !PredictionProjectileWall(1, prediction, 85.0f);
    if (output) *output = prediction;
    return hit;
}

inline MarksmanTargeting::TargetContext TargetFacts(const AIHeroClient& target,
                                                     Mode mode) {
    const auto player = GameObjects::Player();
    const float distance = player.IsValid()
        ? player.Position().Distance2D(target.Position()) : FLT_MAX;
    const bool attack = OrbwalkerAttackRoute(target);
    const bool q = CanUse(0, mode) && distance <= kQRange + target.BoundingRadius() &&
        QPrediction(target);
    const bool e = CanUse(2, mode) && distance <= kERange + target.BoundingRadius() &&
        HasSpear(target) && ShouldRend(RendContext(target));
    auto context = BaseTargetContext(
        target, EstimatedDamage(target, {q, false, e, false}, attack ? 1 : 0));
    context.AutoReachable = attack;
    context.DirectSpellReachable = q || e;
    context.ExecuteReachable = e && RendLethal(target);
    context.SetupReachable = q;
    context.ProjectileBlocked = !attack && !q && !e;
    return context;
}

inline AIHeroClient SelectSmartTarget(Mode mode) {
    LastSmartTarget = ControllerHelpers::SelectReachableEnemy(
        AIHeroClient{}, kQRange + 90.0f,
        [mode](const AIHeroClient& enemy) { return TargetFacts(enemy, mode); });
    return LastSmartTarget;
}

inline AIBaseClient BestRendTarget(Mode mode, bool objectiveOnly = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !CanUse(2, mode, true)) return {};
    AIBaseClient best{};
    float bestScore = -FLT_MAX;
    auto consider = [&](const AIBaseClient& unit, bool objective) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            player.Position().Distance2D(unit.Position()) > kERange + unit.BoundingRadius() ||
            !HasSpear(unit)) return;
        if (objectiveOnly && !objective) return;
        const auto context = RendContext(unit);
        if (!ShouldRend(context)) return;
        float score = RendLethal(unit, objective) ? 1200.0f : 0.0f;
        score += objective ? 720.0f : 0.0f;
        score += static_cast<float>(SpearStacks(unit)) * 85.0f;
        score -= unit.HealthPercent();
        if (unit.IsHero() && LastSmartTarget.IsValid() &&
            unit.NetworkId() == LastSmartTarget.NetworkId()) score += 180.0f;
        if (score > bestScore) { best = unit; bestScore = score; }
    };
    const bool combatMode = mode == Mode::Combo || mode == Mode::Harass ||
        mode == Mode::Flee || mode == Mode::Automatic;
    if (combatMode) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) consider(enemy, false);
    }
    if (!objectiveOnly && (mode == Mode::LastHit || mode == Mode::LaneClear)) {
        for (const auto& minion : Engine::ClearUnits(false)) consider(minion, false);
    }
    if (mode == Mode::Jungle || objectiveOnly) {
        for (const auto& monster : Engine::ClearUnits(true)) {
            consider(monster, IsEpicMonster(monster));
        }
    }
    return best;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool defensive = false) {
    if (!CanUse(0, mode, defensive) || !Engine::ValidEnemy(target, kQRange + 40.0f) ||
        !CastThrottlePassed(LastQCastTick, 30)) return false;
    SDK::PredictionOutput prediction{};
    if (!QPrediction(target, &prediction)) return false;
    QLineContext context{};
    context.PredictionHigh = PredictionAtLeast(prediction, SDK::HitChance::High);
    context.CollisionFree = prediction.CollisionObjects.empty();
    context.ProjectileWall = PredictionProjectileWall(0, prediction, kQWidth);
    context.InRange = GameObjects::Player().Position().Distance2D(target.Position()) <=
        kQRange + target.BoundingRadius();
    context.AttackWindup = Orbwalker::IsWindingUp();
    context.Lethal = SpellDamage(0, target) >= target.Health() + target.AllShield();
    if (!ShouldCastPierce(context)) return false;
    if (!Engine::ControllerCastPosition(0, prediction.GetCastPosition())) return false;
    LastQCastTick = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode) {
    if (!Bool(RendMenu, "UseSentinel", false) ||
        target.HasBuff("kalistacoopstrikemarkally") ||
        !CanUse(1, mode) || !CastThrottlePassed(LastWCastTick, 600)) return false;
    SDK::PredictionOutput prediction{};
    if (!WPrediction(target, &prediction)) return false;
    if (!Engine::ControllerCastPosition(1, prediction.GetCastPosition())) return false;
    LastWCastTick = Now();
    return true;
}

inline bool CastE(Mode mode, bool objectiveOnly = false) {
    if (!CanUse(2, mode, true) || !CastThrottlePassed(LastECastTick, 35)) return false;
    const auto target = BestRendTarget(mode, objectiveOnly);
    if (!target.IsValid()) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    LastECastTick = Now();
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    return true;
}

inline AIHeroClient SelectOathsworn() {
    const auto ally = OathswornAlly();
    if (ally.IsValid()) return ally;
    return RawAllyHeroByNetworkId(0);
}

inline bool CastR(Mode mode) {
    if (!CanUse(3, mode, true) || !Engine::RuntimeSpells[3] ||
        !Engine::RuntimeSpells[3]->IsReady() || !CastThrottlePassed(LastRCastTick, 180)) {
        return false;
    }
    const auto ally = OathswornAlly();
    if (!ally.IsValid() || GameObjects::Player().Position().Distance2D(ally.Position()) > kRRange) {
        return false;
    }
    const auto player = GameObjects::Player();
    FateContext context{};
    context.OathswornBound = true;
    context.AllyInRange = true;
    context.AllyLowHealth = ally.HealthPercent() <=
        static_cast<float>(Slider(FateMenu, "SaveHealth", 32));
    context.EnemyNearAlly = Engine::CountEnemiesAt(ally.Position(), 650.0f) > 0;
    context.AllyThreatened = context.EnemyNearAlly &&
        (ally.HealthPercent() <= 55.0f || player.HealthPercent() <= 38.0f);
    context.AlliedFollowup = ControllerHelpers::CountAlliedFollowup(
        ally.Position(), 850.0f, true) > 0;
    context.SavePolicyEnabled = Bool(FateMenu, "SaveAlly", true);
    context.EngagePolicyEnabled = Bool(FateMenu, "Engage", false) && mode == Mode::Combo;
    if (!ShouldCallFate(context)) return false;
    if (!Engine::ControllerCastUnit(3, ally)) return false;
    LastRCastTick = Now();
    return true;
}

inline bool ApplyHopIntent(const AIHeroClient& target, bool emergency = false) {
    if (!Bool(HopMenu, "Enable", true) || PendingHopTick == 0 ||
        Now() < PendingHopTick || !target.IsValid()) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Orbwalker::MoveEnabled()) return false;
    const Vector3 cursor = Game::CursorPos();
    const Vector3 desired = emergency
        ? player.Position().Extend(cursor, 115.0f)
        : player.Position().Extend(target.Position(), 115.0f);
    const bool cursorTowardTarget = emergency &&
        ControllerHelpers::CursorDirectionAgrees(desired, -0.12f);
    HopContext context{};
    context.AttackConfirmed = LastAfterAttackTargetId == static_cast<int>(target.NetworkId());
    context.PlayerAttackWindup = Orbwalker::IsWindingUp();
    context.CursorValid = !emergency || (cursor.IsValid() && !cursor.IsZero());
    context.CursorTowardTarget = !emergency || cursorTowardTarget;
    context.EmergencyPeel = emergency;
    context.DestinationSafe = !Engine::UnderEnemyTurret(desired) &&
        Engine::CountEnemiesAt(desired, 260.0f) <=
            Engine::CountAlliesAt(player.Position(), 700.0f) + 1;
    if (!ShouldHop(context)) return false;
    Orbwalker::Move(desired);
    PendingHopTick = 0;
    PendingHopTargetId = 0;
    return true;
}

inline bool TryAutomaticR() {
    if (!Bool(FateMenu, "Automatic", true)) return false;
    return CastR(Mode::Automatic);
}

inline bool TryCombat(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target, kQRange + 90.0f)) return false;
    if (CastE(mode)) return true;
    if (CastW(target, mode)) return true;
    if (CastQ(target, mode)) return true;
    return false;
}

inline bool TryFlee() {
    const auto threat = ControllerHelpers::NearestEnemyToPlayer({}, kERange + 80.0f);
    if (Engine::ValidEnemy(threat, kERange) && CastE(Mode::Flee)) return true;
    return CastR(Mode::Flee);
}

inline bool TryFarm(Mode mode) {
    if (!Bool(FarmMenu, "Enable", true)) return false;
    const bool jungle = mode == Mode::Jungle ||
        (mode == Mode::LaneClear && Engine::ClearUnits(false).empty() &&
         !Engine::ClearUnits(true).empty());
    if (jungle && HasNearbyEpicMonster(1200.0f) && CastE(Mode::Jungle, true)) return true;
    const auto rend = BestRendTarget(mode, false);
    if (rend.IsValid() && RendLethal(rend, rend.IsMinion() && IsEpicMonster(rend)) &&
        CastE(mode)) return true;
    if (mode == Mode::LastHit || mode == Mode::LaneClear || mode == Mode::Jungle) {
        return Engine::TryFarmSpell(0, jungle, mode == Mode::LastHit);
    }
    return false;
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    LastMode = mode;
    if (ApplyHopIntent(
            ControllerHelpers::HeroByNetworkId(PendingHopTargetId),
            mode == Mode::Flee)) return true;
    if (TryAutomaticR()) return true;
    if (mode == Mode::Flee) return TryFlee();
    if (mode == Mode::Combo || mode == Mode::Harass) {
        const auto target = SelectSmartTarget(mode);
            if (TryCombat(target, mode)) return true;
            (void)SetTemporaryOrbwalkerFocus(
                target, ControllerHelpers::AutoAttackRange(target), 750,
                OwnedFocusTargetId, OwnedFocusUntil);
        return false;
    }
    if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) {
        return TryFarm(mode);
    }
    return false;
}

inline void ObserveSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender) || args.IsAutoAttack) return;
    const int now = Now();
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
        SpellEventNameContainsAny(args, {"kalistapierce", "kalistapassive"})) {
        LastQCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
               SpellEventNameContainsAny(args, {"kalistasentinel"})) {
        LastWCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
               SpellEventNameContainsAny(args, {"kalistaexpunge", "kalistae"})) {
        LastECastTick = now;
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
               SpellEventNameContainsAny(args, {"kalistacoopstrike"})) {
        LastRCastTick = now;
    }
}

inline void ObserveBuffAdd(const SDK::Events::BuffEventArgs& args) {
    (void)args;
    // Stack/bound state is intentionally reconciled by polling each decision tick.
    PendingHopTick = std::max(PendingHopTick, Now());
}

inline void ObserveBuffRemove(const SDK::Events::BuffEventArgs& args) {
    (void)args;
    PendingHopTick = 0;
    PendingHopTargetId = 0;
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    auto focus = OwnedOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil, kERange + 40.0f);
    if (focus.IsValid()) {
        if (!RedirectBeforeAttackToFocus(args, focus)) {
            ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
        }
    }
    // Never cancel an attack here: Kalista's spear application is the state that
    // makes the subsequent Rend threshold meaningful.
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(args, LastAfterAttackTargetId, LastAfterAttackTick)) return;
    const auto target = HeroByNetworkId(LastAfterAttackTargetId);
    if (Engine::ValidEnemy(target, kERange)) {
        (void)SetTemporaryOrbwalkerFocus(
            target, ControllerHelpers::AutoAttackRange(target), 650,
            OwnedFocusTargetId, OwnedFocusUntil);
        PendingHopTargetId = LastAfterAttackTargetId;
        PendingHopTick = Now() + Slider(HopMenu, "DelayMs", 35);
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("KalistaMechanics", "Kalista Mechanics"));
    RendMenu = TacticsMenu->AddSubMenu(new Menu("RendLogic", "Rend / Spear stacks"));
    RendMenu->Add(new MenuSeparator(
        "Threshold",
        "Hero kill/escape; minion kill"));
    RendMenu->Add(new MenuSlider(
        "EscapeStacks", "Escape hero: minimum spears",
        kDefaultEscapeSpearThreshold, 1, 15));
    HopMenu = TacticsMenu->AddSubMenu(new Menu("MartialPoise", "Hop movement"));
    HopMenu->Add(new MenuBool("Enable", "Hop after confirmed attacks", true));
    HopMenu->Add(new MenuSlider("DelayMs", "Hop delay (ms)", 35, 0, 120));
    FateMenu = TacticsMenu->AddSubMenu(new Menu("FatesCall", "Oathsworn / Fate's Call"));
    FateMenu->Add(new MenuBool("SaveAlly", "Automatic save for Oathsworn ally", true));
    FateMenu->Add(new MenuSlider("SaveHealth", "Save below health (%)", 32, 10, 60));
    FateMenu->Add(new MenuBool("Engage", "Use R engage with allied follow-up", false));
    FateMenu->Add(new MenuBool("Automatic", "Allow automatic R save", true));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("FarmSafety", "Farm / objective safety"));
    FarmMenu->Add(new MenuBool("Enable", "Use Q/E for farm", true));
}

inline void OnLoad() {
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    OwnedFocusTargetId = OwnedFocusUntil = 0;
    PendingHopTick = PendingHopTargetId = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    LastMode = Mode::None;
    LastSmartTarget = {};
}

inline void OnUnload() {
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    TacticsMenu = RendMenu = HopMenu = FateMenu = FarmMenu = nullptr;
    PendingHopTick = PendingHopTargetId = 0;
    LastSmartTarget = {};
}

inline constexpr const char* Scenarios[] = {
    "Track KalistaExpungeMarker through mixed-case buff aliases and clamp observed spear stacks",
    "Compute live Rend damage from rank, total AD, AP and the epic-objective modifier",
    "Never spend Rend on a nonlethal lane minion or ordinary jungle monster",
    "Rend heroes immediately only on lethal or imminent marker expiry",
    "Require five spears by default and actual range loss before nonlethal Rend on an escaping hero",
    "Require the extra conservative half-damage margin before Rend on epic objectives",
    "Require a collision-free high-confidence Pierce prediction and projectile-wall check",
    "Preserve an orbwalker attack windup rather than stealing a confirmed spear application",
    "Keep reachable target routing while redirecting BeforeAttack to the spear target",
    "Queue Martial Poise hop after confirmed attacks with target safety and Flee cursor movement",
    "Reconcile hop, spear and bound state by polling when buff telemetry is incomplete",
    "Use W Sentinel only when enabled and a predicted mark path is available",
    "Use Fate's Call automatically only for a threatened Oathsworn ally below policy threshold",
    "Keep Fate's Call engage disabled by default and require allied follow-up when enabled",
    "Apply Fate's Call only through Oathsworn save and allied-follow-up policy",
    "Prefer hero Rend execution before farming, but allow Q last-hit and lane/jungle fallback",
    "Flee with Rend against a committed pursuer and reserve R for an actual ally save",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Kalista;
    controller.ControllerId = "champion.kuroaio.ai.kalista.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIKalista.md";
    controller.ImplementationSummary =
        "Stack-aware Rend threshold and reset, collision-safe Pierce, Oathsworn "
        "save-first Fate's Call policy, target routing and post-attack hop intent.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &ObserveSpell;
    controller.OnDoCast = &ObserveSpell;
    controller.OnBuffAdd = &ObserveBuffAdd;
    controller.OnBuffRemove = &ObserveBuffRemove;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, kERange, 850>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Kalista
