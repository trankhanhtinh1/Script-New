#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AINeekoGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Neeko {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline DisguiseState Disguise{};
inline UltimateStage RStage = UltimateStage::Idle;
inline int RChannelStartedTick = 0;
inline int RChannelExpireTick = 0;
inline int LastCastTick[4]{};
inline int PlayerOverrideUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastObjectId = 0;
inline Mode LastMode = Mode::None;
inline Vector3 LastEAim{};
inline Vector3 LastQAim{};
inline Vector3 LastRPosition{};

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
inline bool Throttle(int slot, int delay = 90) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
using ControllerHelpers::Protected;
using ControllerHelpers::PreserveAttack;
inline int Rank(int slot) { return SpellRank(slot); }
using ControllerHelpers::AP;
inline float QDamage(const AIHeroClient& target, bool bloom = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = bloom ? QBloomRawDamage(Rank(0), AP()) : QRawDamage(Rank(0), AP());
    return player.CalculateMagicDamage(target, raw);
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, ERawDamage(Rank(2), AP())) : 0.0f;
}
inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, RRawDamage(Rank(3), AP())) : 0.0f;
}
using ControllerHelpers::Lethal;
inline bool SafePosition(const Vector3& position, bool defensive = false) {
    if (!position.IsValid() || position.IsZero() || SDK::NavMesh::IsWall(position)) return false;
    if (!defensive && Engine::UnderEnemyTurret(position) &&
        !Engine::UnderEnemyTurret(GameObjects::Player().Position())) return false;
    return defensive || Engine::CountEnemiesAt(position, 250.0f) <=
        Slider(EMenu, "MaxEndpointEnemies", 2);
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    const bool passive = player.HasBuff("NeekoPassive") || player.HasBuff("NeekoPassiveDisguise") ||
                         player.HasBuff("NeekoPassiveClone");
    Disguise.Disguised = passive;
    Disguise.PassiveReady = !passive && !player.HasBuff("NeekoPassiveCooldown");
    if (Disguise.ExpireTick > 0 && now >= Disguise.ExpireTick) {
        Disguise.CloneActive = false;
        Disguise.ExpireTick = 0;
    }
    if (player.HasBuff("NeekoW") || player.HasBuff("NeekoWStealth") ||
        player.HasBuff("NeekoWClone")) {
        Disguise.CloneActive = true;
        Disguise.ExpireTick = std::max(Disguise.ExpireTick,
            now + static_cast<int>(kWCloneSeconds * 1000.0f));
    }
    const bool ultimateBuff = player.HasBuff("NeekoR") || player.HasBuff("NeekoRChannel") ||
        player.HasBuff("NeekoRRoot");
    if (ultimateBuff) {
        if (RStage == UltimateStage::Idle) RChannelStartedTick = now;
        RStage = UltimateStage::Channeling;
        RChannelExpireTick = std::max(RChannelExpireTick,
            RChannelStartedTick + static_cast<int>(kRChannelSeconds * 1000.0f));
    } else if (RStage == UltimateStage::Channeling && now >= RChannelExpireTick) {
        RStage = UltimateStage::Landing;
        RChannelExpireTick = now + static_cast<int>(kRLandingDelay * 1000.0f);
    } else if (RStage == UltimateStage::Landing && now >= RChannelExpireTick) {
        RStage = UltimateStage::Idle;
        RChannelStartedTick = RChannelExpireTick = 0;
        LastRPosition = {};
    } else if (RStage != UltimateStage::Idle && now > RChannelExpireTick + 500) {
        RStage = UltimateStage::Idle;
        RChannelStartedTick = RChannelExpireTick = 0;
        LastRPosition = {};
    }
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(0, mode) || !Throttle(0) ||
        PreserveAttack(reactive)) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vector3 aim = prediction.GetCastPosition().IsValid() &&
        !prediction.GetCastPosition().IsZero() ? prediction.GetCastPosition() :
        PredictPosition(target, kQDelay);
    if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) > kQRange + target.BoundingRadius() ||
        !BloomHits(aim, PredictPosition(target, kQDelay), target.BoundingRadius())) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    LastQAim = aim;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1) || PreserveAttack(reactive)) return false;
    const bool threat = IncomingThreatUntil > Now() ||
        player.HealthPercent() <= Slider(WMenu, "EmergencyHP", 34);
    const bool useful = fleeing || reactive || threat || Engine::ValidEnemy(target, kWRange);
    if (!useful) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    Disguise.CloneActive = true;
    Disguise.StartedTick = Now();
    Disguise.ExpireTick = Now() + static_cast<int>(kWCloneSeconds * 1000.0f);
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || (!fleeing && Protected(target)) || !Ready(2, mode) || !Throttle(2) ||
        PreserveAttack(reactive)) return false;
    const Vector3 requested = fleeing ? Game::CursorPos() : PredictPosition(target, 0.25f);
    const Vector3 endpoint = ClampLineEndpoint(player.Position(), requested);
    if (!endpoint.IsValid() || endpoint.IsZero() ||
        player.Position().Distance2D(endpoint) < 50.0f || !SafePosition(endpoint, fleeing) ||
        ProjectileWallBlocksFromPlayer(endpoint, kEWidth * 0.5f)) return false;
    if (fleeing) {
        if (!Engine::ControllerCastPosition(2, endpoint)) return false;
        LastCastTick[2] = Now();
        LastEAim = endpoint;
        return true;
    }
    const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    const Vector3 predicted = prediction.GetCastPosition().IsValid() &&
        !prediction.GetCastPosition().IsZero() ? prediction.GetCastPosition() : requested;
    const bool hit = RootLineHits(player.Position(), endpoint, predicted,
        target.BoundingRadius());
    if (!hit || prediction.Hitchance < (reactive ? SDK::HitChance::Medium : SDK::HitChance::High)) return false;
    CollisionResult collisions{true, true,
        prediction.CollisionObjects.size() > static_cast<std::size_t>(Slider(EMenu, "MaxCollisions", 4)),
        static_cast<int>(prediction.CollisionObjects.size())};
    if (!ECollisionAcceptable(collisions)) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    LastCastTick[2] = Now();
    LastEAim = endpoint;
    return true;
}

inline int EnemiesAtR(const Vector3& center) {
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && CircleHits(center, enemy.Position(),
            kRRadius, enemy.BoundingRadius())) ++count;
    }
    return count;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool manual = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 150) || PreserveAttack(reactive) ||
        RStage != UltimateStage::Idle) return false;
    const int hits = EnemiesAtR(player.Position());
    const bool validTarget = Engine::ValidEnemy(target, kRRadius);
    const bool lethal = validTarget && Lethal(target, RDamage(target));
    const bool defensive = player.HealthPercent() <= Slider(RMenu, "DefensiveHP", 30) ||
        IncomingHardCCUntil > Now();
    const UltimateContext context{true, validTarget, hits > 0, lethal, defensive, manual,
        Orbwalker::IsWindingUp(), Engine::UnderEnemyTurret(player.Position()), hits,
        Slider(RMenu, "MinimumTargets", 2)};
    if (!ShouldStartUltimate(context)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    RStage = UltimateStage::Channeling;
    RChannelStartedTick = Now();
    RChannelExpireTick = RChannelStartedTick + static_cast<int>(kRChannelSeconds * 1000.0f);
    LastRPosition = player.Position();
    return true;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, QDamage(target, true)) && CastQ(target, mode, true)) return true;
    if (Lethal(target, EDamage(target)) && CastE(target, mode, true)) return true;
    return Lethal(target, RDamage(target)) && CastR(target, mode, true, true);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(WMenu, "HarassMana", 48)) return;
    if (CastE(target, Mode::Harass)) return;
    (void)CastQ(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (CastW(target, Mode::Flee, true, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastE(target, Mode::Flee, true, true);
}
inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "Mana", 40)) return;
    (void)Engine::TryFarm(mode);
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, mode == Mode::Flee ? 1000.0f : kERange);
    if (PlayerOverrideUntil > Now()) return true;
    if (RStage == UltimateStage::Channeling) {
        const bool abort = ShouldAbortUltimate(true, GameObjects::Player().IsDead(),
            IncomingHardCCUntil > Now(), PlayerOverrideUntil > Now());
        if (abort) RStage = UltimateStage::Idle;
        return true;
    }
    if (IncomingThreatUntil > Now() && Engine::ValidEnemy(target) &&
        CastW(target, mode, true)) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1000.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
    case Mode::Automatic:
        if (InterruptExpireTick > Now()) {
            const auto interrupt = Engine::EnemyByNetworkId(InterruptTargetId);
            if (Engine::ValidEnemy(interrupt) && CastE(interrupt, mode, true)) return true;
        }
        if (Engine::ValidEnemy(target) && IncomingHardCCUntil > Now())
            (void)CastW(target, mode, true);
        break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot < 0 || slot > 3) return;
        if (!Engine::WasControllerCast(slot))
            PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
        LastCastTick[slot] = now;
        if (slot == 1) {
            Disguise.CloneActive = true;
            Disguise.ExpireTick = now + static_cast<int>(kWCloneSeconds * 1000.0f);
        } else if (slot == 3) {
            RStage = UltimateStage::Channeling;
            RChannelStartedTick = now;
            RChannelExpireTick = now + static_cast<int>(kRChannelSeconds * 1000.0f);
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    if (analysis.TargetsPlayer && Disguise.Disguised)
        Disguise.Disguised = false;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl)
        IncomingHardCCUntil = std::max(IncomingHardCCUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "NeekoPassive")) Disguise.Disguised = true;
    if (Engine::TextContains(args.BuffName, "NeekoW")) {
        Disguise.CloneActive = true;
        Disguise.ExpireTick = Now() + static_cast<int>(kWCloneSeconds * 1000.0f);
    }
    if (Engine::TextContains(args.BuffName, "NeekoR")) {
        RStage = UltimateStage::Channeling;
        RChannelStartedTick = Now();
        RChannelExpireTick = RChannelStartedTick + static_cast<int>(kRChannelSeconds * 1000.0f);
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "NeekoPassive")) Disguise.Disguised = false;
    if (Engine::TextContains(args.BuffName, "NeekoW")) Disguise.CloneActive = false;
    if (Engine::TextContains(args.BuffName, "NeekoR") && Now() > RChannelExpireTick + 300) {
        RStage = UltimateStage::Idle;
    }
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (args.Sender.IsValid() && IsLocalPlayer(args.Sender)) LastAutoTick = Now();
}
inline bool CloneObject(const SDK::Events::ObjectEventArgs& args) {
    return args.Sender.IsValid() && ControllerHelpers::ObjectEventIsAllied(args) &&
        (Engine::TextContains(args.Sender.Name, "NeekoW") ||
         Engine::TextContains(args.Sender.CharacterName, "NeekoW"));
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!CloneObject(args)) return;
    LastObjectId = static_cast<int>(args.Sender.NetworkId);
    Disguise.CloneActive = true;
    Disguise.ExpireTick = Now() + static_cast<int>(kWCloneSeconds * 1000.0f);
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() && static_cast<int>(args.Sender.NetworkId) == LastObjectId) {
        Disguise.CloneActive = false;
        LastObjectId = 0;
    }
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFCC66DDu, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kRRadius, 0xFFFFAA55u, 1.5f, 40);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("NeekoOneTrick", "Neeko disguise and bloom tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Blooming Burst"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Shapesplitter"));
    WMenu->Add(new MenuSlider("EmergencyHP", "Defensive W health (%)", 34, 10, 75));
    WMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 48, 10, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Tangle-Barbs"));
    EMenu->Add(new MenuSlider("MaxCollisions", "Maximum observed line collisions", 4, 1, 8));
    EMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum endpoint enemies", 2, 1, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Pop Blossom"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum R targets", 2, 1, 5));
    RMenu->Add(new MenuSlider("DefensiveHP", "Defensive R health (%)", 30, 5, 65));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("NeekoFarm", "Mana-aware farming"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 40, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("NeekoCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and R ranges", false));
}
inline void OnLoad() {
    Disguise = {};
    Disguise.PassiveReady = true;
    RStage = UltimateStage::Idle;
    RChannelStartedTick = RChannelExpireTick = PlayerOverrideUntil = 0;
    IncomingThreatUntil = IncomingHardCCUntil = InterruptTargetId = InterruptExpireTick = 0;
    LastAutoTargetId = LastAutoTick = LastObjectId = 0;
    LastMode = Mode::None;
    LastEAim = LastQAim = LastRPosition = {};
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    Disguise = {};
    RStage = UltimateStage::Idle;
}

inline constexpr const char* Scenarios[] = {
    "Pin all values and behavior to Riot 26.15 / CommunityDragon 16.15",
    "Poll Neeko passive disguise, W stealth/clone and R channel buffs",
    "Reconcile passive and clone state from buff, spell and object events",
    "Preserve selected target before orbwalker and selector fallback",
    "Preserve AA windup unless a reactive or lethal cast justifies interruption",
    "Aim Q Blooming Burst at predicted landing position and honor bloom radius",
    "Use E Tangle-Barbs with 1000 range, 1300 speed and observed collision budget",
    "Reject E casts through unsafe endpoints, walls, or excessive collision bodies",
    "Use W clone and stealth for threat response, engage setup and fleeing",
    "Start Pop Blossom only for lethal, defensive, manual or configured multi-target value",
    "Track R channel duration and never silently recast while channeling",
    "Abort an observed R channel only for death, hard crowd control or manual ownership",
    "Keep R landing policy conservative when target telemetry is stale",
    "Automatic mode is restricted to interrupt pressure, defense and kill secure",
    "Combo uses W stealth setup, E root, Q bloom and R landing in that order",
    "Harass uses E and Q while preserving a mana reserve and ultimate",
    "LaneClear, Jungle and LastHit delegate to the shared farm policy",
    "Flee uses W clone/stealth and cursor-directed E without unsolicited R",
    "Reject protected, invulnerable and spell-shielded targets",
    "Never automate items, summoner spells or movement ownership",
    "Draw ranges and disguise state without changing gameplay decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Neeko;
    controller.ControllerId = "champion.kuroaio.ai.neeko.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AINeeko.md";
    controller.ImplementationSummary =
        "Disguise and clone reconciliation, bloom/root collision safety, W stealth setup and Pop Blossom channel/landing policy.";
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
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffEvent<OnBuffAdd>;
    controller.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 1300, 250, 6000>;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Neeko
