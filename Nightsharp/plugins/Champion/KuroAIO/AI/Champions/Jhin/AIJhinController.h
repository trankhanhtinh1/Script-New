#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIMarksmanControllerHelpers.h"
#include "AIJhinGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Jhin {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::Now;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::SpellEventNameContainsAny;

inline Menu* TacticsMenu = nullptr;
inline Menu* FlourishMenu = nullptr;
inline Menu* TrapMenu = nullptr;
inline Menu* CurtainMenu = nullptr;

inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int LastTrapTargetId = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int CurtainUntil = 0;
inline int CurtainShots = 0;
inline Vector3 CurtainOrigin = {};
inline Vector3 CurtainDirection = {};
inline bool CurtainOwnsOrbwalkerPause = false;
inline bool PreviousAttackEnabled = true;
inline bool PreviousMoveEnabled = true;
inline int OwnedFocusTargetId = 0;
inline int OwnedFocusUntil = 0;
inline AIHeroClient LastSmartTarget = {};

// Full skillshot prediction (movement + collision scan) is the dominant cost
// of this controller because TargetFacts/ShootCurtain evaluate every enemy
// repeatedly within one decision tick. Cache the PredictionOutput per
// (spell slot, target, tick) so the three passes share one computed result.
struct JhinPredictionCacheEntry {
    int Tick = 0;
    int NetworkId = 0;
    bool ProjectileWallComputed = false;
    bool ProjectileWall = false;
    SDK::PredictionOutput Output = {};
};

inline std::array<JhinPredictionCacheEntry, 4> JhinPredictionCache = {};

inline JhinPredictionCacheEntry* CachedGetPrediction(
    int index,
    const AIBaseClient& target) {
    if (index < 0 || index >= 4 || !target.IsValid() ||
        !Engine::RuntimeSpells[index]) {
        return nullptr;
    }
    const int now = Now();
    const int networkId = static_cast<int>(target.NetworkId());
    auto& entry = JhinPredictionCache[static_cast<std::size_t>(index)];
    if (entry.Tick != now || entry.NetworkId != networkId) {
        entry = {};
        entry.Tick = now;
        entry.NetworkId = networkId;
        entry.Output = Engine::RuntimeSpells[index]->GetPrediction(target);
    }
    return &entry;
}

inline JhinPredictionCacheEntry* CachedGetPredictionWall(
    int index,
    const AIBaseClient& target,
    float wallRadius) {
    auto* entry = CachedGetPrediction(index, target);
    if (!entry) return nullptr;
    if (!entry->ProjectileWallComputed) {
        entry->ProjectileWall = PredictionProjectileWall(
            index, entry->Output, wallRadius);
        entry->ProjectileWallComputed = true;
    }
    return entry;
}

inline bool Marked(const AIBaseClient& target) {
    return target.IsValid() && target.HasBuff("jhinespotteddebuff");
}

inline bool Reloading() {
    const auto player = GameObjects::Player();
    return player.IsValid() && player.HasBuff("JhinPassiveReload");
}

inline bool FourthShotReady() {
    const auto player = GameObjects::Player();
    return player.IsValid() &&
        player.HasBuff("jhinpassiveattackbuff");
}

inline bool RecentlyAttacked(const AIBaseClient& target,
                             int windowMs = 500) {
    return RecentlyAttackedTarget(
        target, LastAfterAttackTargetId, LastAfterAttackTick, windowMs);
}

inline bool CurtainActive() {
    const auto player = GameObjects::Player();
    const bool live =
        ControllerHelpers::RuntimeNameContains(3, "JhinRShot") ||
        (player.IsValid() && player.HasBuff("JhinRShot"));
    const bool recentTransition = CurtainUntil >= Now() &&
        LastRCastTick > 0 && Now() - LastRCastTick <= 650;
    return CurtainShots < 4 && (live || recentTransition);
}

inline void RefreshProjectileCollisionMasks() {
    const auto flags = SDK::CollisionableObjects::Heroes |
                       SDK::CollisionableObjects::YasuoWall;
    if (Engine::RuntimeSpells[1]) {
        Engine::RuntimeSpells[1]->SetCollisionObjects(flags);
    }
    if (Engine::RuntimeSpells[3]) {
        Engine::RuntimeSpells[3]->SetCollisionObjects(flags);
    }
}

inline void RefreshOrbwalkerFocus(Mode mode) {
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    auto owned = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, 800.0f);
    if (!combat || !FourthShotReady() || !owned.IsValid() ||
        !InAutoAttackRange(owned)) {
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
        owned = {};
    }
    if (owned.IsValid()) return;

    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 800.0f) ||
            !InAutoAttackRange(enemy)) continue;
        float score = 100.0f - enemy.HealthPercent();
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    if (best.IsValid()) {
        (void)SetTemporaryOrbwalkerFocus(
            best, 800.0f, 700,
            OwnedFocusTargetId, OwnedFocusUntil);
    }
}

inline bool ClearWPrediction(const AIHeroClient& target,
                             SDK::PredictionOutput* output = nullptr,
                             SDK::HitChance chance = SDK::HitChance::High) {
    const auto* cached = CachedGetPredictionWall(1, target, 45.0f);
    if (!cached) return false;
    const bool valid = ControllerHelpers::PredictionAtLeast(
                           cached->Output, chance) &&
        cached->Output.CollisionObjects.empty() && !cached->ProjectileWall;
    if (output) *output = cached->Output;
    return valid;
}

inline bool ClearRPrediction(const AIHeroClient& target,
                             SDK::PredictionOutput* output = nullptr) {
    const auto* cached = CachedGetPredictionWall(3, target, 80.0f);
    if (!cached) return false;
    const bool valid = ControllerHelpers::PredictionAtLeast(
                           cached->Output, SDK::HitChance::VeryHigh) &&
        cached->Output.CollisionObjects.empty() && !cached->ProjectileWall;
    if (output) *output = cached->Output;
    return valid;
}

inline bool TrapPlan(const AIHeroClient& target,
                     bool gapcloser,
                     SDK::PredictionOutput* output = nullptr) {
    if (!Engine::ValidEnemy(target, 780.0f) || !Engine::RuntimeSpells[2]) {
        return false;
    }
    const auto* cached = CachedGetPrediction(2, target);
    if (!cached) return false;
    const bool hit = ControllerHelpers::PredictionAtLeast(
        cached->Output, SDK::HitChance::High);
    const bool recent = LastTrapTargetId ==
        static_cast<int>(target.NetworkId()) &&
        Now() - LastECastTick < Slider(
            TrapMenu, "RepeatMs", 2500);
    TrapContext context{};
    context.InRange = hit;
    context.AmmoReady = Engine::RuntimeSpells[2]->IsReady();
    context.ExistingTrapNear = recent;
    context.Immobilized = IsImmobile(target);
    context.Dashing = target.IsDashing();
    context.Gapcloser = gapcloser;
    context.Committed = IsEscaping(target, 0.25f) == false &&
        target.Position().Distance2D(GameObjects::Player().Position()) < 650.0f;
    if (output) *output = cached->Output;
    return ShouldPlaceTrap(context);
}

inline bool WPlan(const AIHeroClient& target,
                  SDK::PredictionOutput* output = nullptr) {
    const auto* cached = CachedGetPredictionWall(1, target, 45.0f);
    if (!cached) return false;
    SDK::PredictionOutput prediction = cached->Output;
    const bool hit = ControllerHelpers::PredictionAtLeast(
        cached->Output, SDK::HitChance::High);
    FlourishContext context{};
    context.InRange = Engine::ValidEnemy(target, 2560.0f);
    context.PredictionHits = hit;
    context.FirstChampionIsTarget = prediction.CollisionObjects.empty();
    context.ProjectileWall = cached->ProjectileWall;
    context.Marked = Marked(target);
    context.Immobilized = IsImmobile(target);
    context.Lethal = SpellDamage(1, target) >=
        target.Health() + target.AllShield();
    context.AttackAvailable = LocalAttackAvailable(target);
    context.Reloading = Reloading();
    if (output) *output = prediction;
    return ShouldCastFlourish(context);
}

inline MarksmanTargeting::TargetContext TargetFacts(
    const AIHeroClient& target,
    Mode mode,
    bool allowR) {
    const float distance = GameObjects::Player().Position().Distance2D(
        target.Position());
    const bool attack = OrbwalkerAttackRoute(target);
    const bool q = CanUse(0, mode) && distance <= 580.0f &&
        (Reloading() || RecentlyAttacked(target) || !attack ||
         SpellDamage(0, target) >= target.Health() + target.AllShield());
    const bool w = CanUse(1, mode) && WPlan(target);
    const bool e = CanUse(2, mode) && TrapPlan(target, false);

    SDK::PredictionOutput rPrediction{};
    const bool rLine = allowR && Engine::RuntimeSpells[3] &&
        Engine::RuntimeSpells[3]->IsReady() &&
        ClearRPrediction(target, &rPrediction);
    const bool r = rLine && SpellDamage(3, target) >=
        target.Health() + target.AllShield();
    const std::array<bool, 4> reachable = {q, w, e, r};
    auto context = BaseTargetContext(
        target, EstimatedDamage(target, reachable, attack ? 1 : 0));
    context.AutoReachable = attack;
    context.DirectSpellReachable = q || w;
    context.SetupReachable = e;
    context.ExecuteReachable = r;
    context.ProjectileBlocked = !attack && !q && !w && !e && !r;
    return context;
}

inline AIHeroClient SelectSmartTarget(Mode mode,
                                      bool allowR = false) {
    const float range = allowR ? 3400.0f : 2560.0f;
    const auto target = ControllerHelpers::SelectReachableEnemy(
        AIHeroClient{}, range,
        [mode, allowR](const AIHeroClient& enemy) {
            return TargetFacts(enemy, mode, allowR);
        });
    LastSmartTarget = target;
    return target;
}

inline bool CastQ(const AIHeroClient& target, Mode mode) {
    if (!CanUse(0, mode) || !Engine::ValidEnemy(target, 580.0f) ||
        !CastThrottlePassed(LastQCastTick, 24)) return false;
    GrenadeContext context{};
    context.InRange = true;
    context.AttackWindingUp = Orbwalker::IsWindingUp();
    context.AttackAvailable = LocalAttackAvailable(target);
    context.AfterAttack = RecentlyAttacked(target);
    context.Reloading = Reloading();
    context.Lethal = SpellDamage(0, target) >=
        target.Health() + target.AllShield();
    if (!ShouldCastGrenade(context)) return false;
    if (Engine::ControllerCastUnit(0, target)) {
        LastQCastTick = Now();
        return true;
    }
    return false;
}

inline bool CastW(const AIHeroClient& target, Mode mode) {
    if (!CanUse(1, mode) || !CastThrottlePassed(LastWCastTick, 28)) {
        return false;
    }
    SDK::PredictionOutput prediction{};
    if (!WPlan(target, &prediction)) return false;
    if (Engine::ControllerCastPosition(1, prediction.GetCastPosition())) {
        LastWCastTick = Now();
        return true;
    }
    return false;
}

inline bool CastE(const AIHeroClient& target,
                  Mode mode,
                  bool gapcloser) {
    if (!CanUse(2, mode, gapcloser) ||
        !CastThrottlePassed(LastECastTick, 40)) return false;
    SDK::PredictionOutput prediction{};
    if (!TrapPlan(target, gapcloser, &prediction)) return false;
    const Vector3 position = gapcloser && GapcloserEndpoint.IsValid() &&
            !GapcloserEndpoint.IsZero()
        ? GapcloserEndpoint : prediction.GetCastPosition();
    if (Engine::ControllerCastPosition(2, position)) {
        LastECastTick = Now();
        LastTrapTargetId = static_cast<int>(target.NetworkId());
        return true;
    }
    return false;
}

inline bool StartCurtain(const AIHeroClient& target) {
    if (CurtainActive() || !Engine::RuntimeSpells[3] ||
        !Engine::RuntimeSpells[3]->IsReady() ||
        !Engine::ValidEnemy(target, 3400.0f) ||
        !CastThrottlePassed(LastRCastTick, 180) ||
        ControllerHelpers::HasEnemyChampionNear(900.0f)) {
        return false;
    }
    SDK::PredictionOutput prediction{};
    const bool hit = ClearRPrediction(target, &prediction);
    const bool lethal = SpellDamage(3, target) * 4.0f >=
        target.Health() + target.AllShield();
    if (!hit || !lethal || InAutoAttackRange(target, 150.0f)) return false;
    const auto player = GameObjects::Player();
    const Vector3 cast = prediction.GetCastPosition();
    if (!Engine::ControllerCastPosition(3, cast)) return false;
    LastRCastTick = Now();
    CurtainOrigin = player.Position();
    CurtainDirection = SharedGeometry::Direction2D(CurtainOrigin, cast);
    CurtainUntil = Now() + 11000;
    CurtainShots = 0;
    return true;
}

inline bool ShootCurtain() {
    if (!CurtainActive() || !Engine::RuntimeSpells[3] ||
        !Engine::RuntimeSpells[3]->IsReady() ||
        !CastThrottlePassed(LastRCastTick,
            Slider(CurtainMenu, "ShotDelay", 45))) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (CurtainOrigin.IsZero()) CurtainOrigin = player.Position();
    if (CurtainDirection.IsZero()) {
        const auto target = Engine::SelectTarget(3400.0f);
        CurtainDirection = target.IsValid()
            ? SharedGeometry::Direction2D(CurtainOrigin, target.Position())
            : Vector3{};
    }

    AIHeroClient best{};
    SDK::PredictionOutput bestPrediction{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 3400.0f) ||
            IsCommonUntargetableOrImmune(enemy)) continue;
        const auto* cached = CachedGetPredictionWall(3, enemy, 80.0f);
        if (!cached) continue;
        const SDK::PredictionOutput& prediction = cached->Output;
        const Vector3 cast = prediction.GetCastPosition();
        CurtainShotContext context{};
        context.InCone = InsideCurtainCone(
            CurtainOrigin, CurtainDirection, cast);
        context.PredictionVeryHigh = ControllerHelpers::PredictionAtLeast(
            prediction, SDK::HitChance::VeryHigh);
        context.FirstChampionIsTarget = prediction.CollisionObjects.empty();
        context.ProjectileWall = cached->ProjectileWall;
        context.TargetDamageable = !IsCommonUntargetableOrImmune(enemy);
        context.Lethal = SpellDamage(3, enemy) >=
            enemy.Health() + enemy.AllShield();
        context.Marked = Marked(enemy);
        context.HealthPercent = enemy.HealthPercent();
        float score = CurtainShotScore(context);
        if (score > bestScore) {
            best = enemy;
            bestPrediction = prediction;
            bestScore = score;
        }
    }
    if (!best.IsValid() || bestScore <= -10000.0f) return false;
    if (Engine::ControllerCastPosition(
            3, bestPrediction.GetCastPosition())) {
        LastRCastTick = Now();
        return true;
    }
    return false;
}


inline bool TryAntiGapcloser() {
    if (GapcloserExpireTick < Now()) return false;
    const auto target = ControllerHelpers::HeroByNetworkId(GapcloserTargetId);
    return Engine::ValidEnemy(target, 800.0f) &&
           CastE(target, Mode::Automatic, true);
}

inline bool TryKillSecure() {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    auto target = SelectSmartTarget(Mode::Automatic, false);
    if (Engine::ValidEnemy(target)) {
        if (SpellDamage(0, target) >= target.Health() + target.AllShield() &&
            CastQ(target, Mode::Automatic)) return true;
        if (SpellDamage(1, target) >= target.Health() + target.AllShield() &&
            CastW(target, Mode::Automatic)) return true;
    }
    target = SelectSmartTarget(Mode::Automatic, true);
    return Engine::ValidEnemy(target) && StartCurtain(target);
}

inline bool TryCombat(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CastQ(target, mode)) return true;
    if (CastW(target, mode)) return true;
    return CastE(target, mode, false);
}

inline bool TryFlee() {
    const auto target = ControllerHelpers::NearestEnemyToPlayer({}, 800.0f);
    if (!Engine::ValidEnemy(target)) return false;
    if (CastE(target, Mode::Flee, true)) return true;
    return CastW(target, Mode::Flee);
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    RefreshProjectileCollisionMasks();
    const bool curtain = CurtainActive();
    if (curtain) {
        ClearTemporaryOrbwalkerFocus(
            OwnedFocusTargetId, OwnedFocusUntil);
    } else {
        RefreshOrbwalkerFocus(mode);
    }
    if (curtain && !CurtainOwnsOrbwalkerPause) {
        PreviousAttackEnabled = Orbwalker::AttackEnabled();
        PreviousMoveEnabled = Orbwalker::MoveEnabled();
        Orbwalker::AttackEnabled(false);
        Orbwalker::MoveEnabled(false);
        CurtainOwnsOrbwalkerPause = true;
    } else if (!curtain && CurtainOwnsOrbwalkerPause) {
        Orbwalker::AttackEnabled(PreviousAttackEnabled);
        Orbwalker::MoveEnabled(PreviousMoveEnabled);
        CurtainOwnsOrbwalkerPause = false;
    }
    if (curtain) return ShootCurtain();
    if (TryAntiGapcloser()) return true;
    if (TryKillSecure()) return true;
    if (mode == Mode::Flee) return TryFlee();
    if (mode == Mode::Combo || mode == Mode::Harass) {
        const auto target = SelectSmartTarget(mode, false);
        return TryCombat(target, mode);
    }
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) {
        return Engine::TryFarm(mode);
    }
    return false;
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!ControllerHelpers::IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (args.IsAutoAttack) return;
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
        SpellEventNameContainsAny(args, {"jhinq", "dancinggrenade"})) {
        LastQCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
               SpellEventNameContainsAny(args, {"jhinw", "deadlyflourish"})) {
        LastWCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
               SpellEventNameContainsAny(args, {"jhine", "captivemine"})) {
        LastECastTick = now;
    }
    if (SpellEventNameContainsAny(args, {"jhinrshot"})) {
        LastRCastTick = now;
        CurtainUntil = now + 10000;
        ++CurtainShots;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
               SpellEventNameContainsAny(args, {"jhinr"})) {
        LastRCastTick = now;
        CurtainOrigin = args.StartPosition.IsValid() &&
                !args.StartPosition.IsZero()
            ? args.StartPosition : GameObjects::Player().Position();
        const Vector3 end = args.EndPosition.IsValid() &&
                !args.EndPosition.IsZero()
            ? args.EndPosition : args.CastPosition;
        CurtainDirection = SharedGeometry::Direction2D(CurtainOrigin, end);
        CurtainUntil = now + 11000;
        CurtainShots = 0;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (CurtainActive()) {
        args.Process = false;
        return;
    }
    const auto focus = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, 800.0f);
    if (!focus.IsValid() || !FourthShotReady() ||
        !RedirectBeforeAttackToFocus(args, focus)) {
        ClearTemporaryOrbwalkerFocus(
            OwnedFocusTargetId, OwnedFocusUntil);
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "JhinMechanics", "Jhin Mechanics"));
    FlourishMenu = TacticsMenu->AddSubMenu(new Menu(
        "FlourishLogic", "Deadly Flourish"));
    FlourishMenu->Add(new MenuSeparator(
        "MarkFirst", "W requires mark, CC or lethal"));
    TrapMenu = TacticsMenu->AddSubMenu(new Menu(
        "AudienceLogic", "Captive Audience"));
    TrapMenu->Add(new MenuSlider(
        "RepeatMs", "Do not repeat E on same target (ms)",
        2500, 1200, 4000));
    CurtainMenu = TacticsMenu->AddSubMenu(new Menu(
        "CurtainLogic", "Curtain Call"));
    CurtainMenu->Add(new MenuSlider(
        "ShotDelay", "Minimum delay between R shots (ms)",
        45, 20, 250));
}

inline void OnLoad() {
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = LastTrapTargetId = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    CurtainUntil = CurtainShots = 0;
    CurtainOrigin = CurtainDirection = {};
    CurtainOwnsOrbwalkerPause = false;
    PreviousAttackEnabled = PreviousMoveEnabled = true;
    OwnedFocusTargetId = OwnedFocusUntil = 0;
    LastSmartTarget = {};
    if (Engine::RuntimeSpells[1]) {
        Engine::RuntimeSpells[1]->SetCollisionObjects(
            SDK::CollisionableObjects::Heroes |
            SDK::CollisionableObjects::YasuoWall);
    }
    if (Engine::RuntimeSpells[3]) {
        Engine::RuntimeSpells[3]->SetCollisionObjects(
            SDK::CollisionableObjects::Heroes |
            SDK::CollisionableObjects::YasuoWall);
    }
}

inline void OnUnload() {
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    if (CurtainOwnsOrbwalkerPause) {
        Orbwalker::AttackEnabled(PreviousAttackEnabled);
        Orbwalker::MoveEnabled(PreviousMoveEnabled);
        CurtainOwnsOrbwalkerPause = false;
    }
    TacticsMenu = FlourishMenu = TrapMenu = CurtainMenu = nullptr;
    LastSmartTarget = {};
}

inline constexpr const char* Scenarios[] = {
    "Reject targets outside AA/Q and without a legal marked W route",
    "Use Q immediately after AA or while reloading, never over a ready attack",
    "Force the best reachable fourth-shot target through the orbwalker",
    "Redirect BeforeAttack only while the fourth-shot state remains active",
    "Release owned fourth-shot focus immediately after the attack",
    "Treat the reload buff as downtime without duplicate case-only aliases",
    "Require W mark, hard CC or lethal damage before spending the cast",
    "Reject W when another champion is the first body on its line",
    "Prefer an AA over W when the target is already in attack range",
    "Place E only on immobilized, dashing or committed movement",
    "Suppress repeated E placement on the same target during arm time",
    "Place E at a captured gapcloser endpoint before ordinary combo casts",
    "Start R only outside a punishable local fight",
    "Require clear first-champion geometry before starting an execute R",
    "Disable orbwalker attack and movement only while Curtain Call is active",
    "Keep every R shot inside the originally captured cone",
    "Recompute prediction and first champion for every R shot",
    "Prioritize lethal R shots, then lower-health marked targets",
    "Restore orbwalker control immediately when the channel ends",
    "Keep policy-gated R subject to real reach, cone and blocker validation",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Jhin;
    controller.ControllerId = "champion.kuroaio.ai.jhin.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIJhin.md";
    controller.ImplementationSummary =
        "Reload/after-AA Q pacing; mark/CC/lethal W with first-champion "
        "collision; commitment and anti-gap E; captured Curtain Call cone with "
        "per-shot prediction, collision and lethal scoring; reachable target policy.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &ObserveLocalSpell;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack =
        &CaptureAfterAttackAndReleaseOwnedFocusEvent<
            &LastAfterAttackTargetId, &LastAfterAttackTick,
            &OwnedFocusTargetId, &OwnedFocusUntil>;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, 700, 900>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Jhin
