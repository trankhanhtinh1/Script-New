#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIYuumiGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Yuumi {
using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptableEvent;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellEnabled;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<int, 4> LastCastTick{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int AttachedAllyId = 0;
inline int QCastTick = 0;
inline int RCastTick = 0;
inline int RWaveHits = 0;
inline int QMissileId = 0;
inline int RMissileId = 0;
inline int EnemyThreatUntil = 0;
inline int HardCcThreatUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint{};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline Vector3 LastQEndpoint{};
inline Vector3 LastREndpoint{};

enum class AttachmentState { Detached, Attached, Channeling };
inline AttachmentState CurrentState = AttachmentState::Detached;

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
        (reactive || LastCastTick[static_cast<std::size_t>(slot)] + 50 <= Now());
}

inline bool PreserveAttack(bool reactive) {
    return !reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true);
}

inline bool ProtectedTarget(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}

inline bool IsAttachedRuntime() {
    const auto player = GameObjects::Player();
    return player.IsValid() && (player.HasBuff("YuumiW") ||
        player.HasBuff("YuumiWAttach") || player.HasBuff("YuumiWAlly") ||
        player.HasBuff("YuumiWBestFriend"));
}

inline AIHeroClient AllyById(int id) {
    if (id == 0) return {};
    for (const auto& ally : GameObjects::AllyHeroes())
        if (static_cast<int>(ally.NetworkId()) == id) return ally;
    return {};
}

inline AIHeroClient SelectEnemy(float range) {
    return Engine::SelectTarget(range);
}

inline AIHeroClient SelectAlly(bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    AIHeroClient best{};
    float scoreBest = -FLT_MAX;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally, kWRange) ||
            ally.NetworkId() == player.NetworkId()) continue;
        const bool bestFriend = ally.HasBuff("YuumiWBestFriend");
        const int enemies = Engine::CountEnemiesAt(ally.Position(), 700.0f);
        const float score = AllyPriority(ally.HealthPercent(), ally.TotalAttackDamage(),
                                         ally.AP(), enemies, bestFriend);
        if (score > scoreBest) { best = ally; scoreBest = score; }
    }
    if (!best.IsValid() && defensive &&
        Engine::ValidAlly(player, kERange)) return player;
    return best;
}

inline bool Threatened(const AIHeroClient& ally) {
    return Engine::ValidAlly(ally) &&
        (EnemyThreatUntil > Now() || Engine::CountEnemiesAt(ally.Position(), 700.0f) > 0);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange) ||
        ProtectedTarget(target) || !Ready(0, mode, reactive) ||
        player.ManaPercent() < (reactive ? 8.0f : 12.0f) ||
        PreserveAttack(reactive)) return false;
    const bool attached = CurrentState != AttachmentState::Detached;
    const Vector3 predicted = PredictPosition(target, attached ? 0.32f : 0.20f);
    if (!predicted.IsValid() || predicted.IsZero()) return false;
    const float elapsed = QMissileId != 0 && QCastTick > 0 ?
        static_cast<float>(Now() - QCastTick) / 1000.0f : 0.0f;
    const Vector3 cursor = Game::CursorPos();
    const Vector3 guided = attached && cursor.IsValid() && !cursor.IsZero() ?
        cursor : predicted;
    if (attached && !QSteerAllowed(true, elapsed, guided, player.Position())) return false;
    Vector3 endpoint = QEndpoint(player.Position(), guided);
    if (!QHits(player.Position(), endpoint, predicted, target.BoundingRadius()))
        endpoint = QEndpoint(player.Position(), predicted);
    if (!QHits(player.Position(), endpoint, predicted, target.BoundingRadius()) ||
        endpoint.IsZero() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(endpoint, kQWidth * 0.5f))
        return false;
    if (!Engine::ControllerCastPosition(0, endpoint)) return false;
    LastQEndpoint = endpoint;
    LastCastTick[0] = QCastTick = Now();
    return true;
}

inline bool CastW(const AIHeroClient& threat, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode, reactive) ||
        PreserveAttack(reactive)) return false;
    const AIHeroClient ally = SelectAlly(mode == Mode::Flee || reactive);
    if (!ally.IsValid() || ally.NetworkId() == player.NetworkId() ||
        !Engine::ValidAlly(ally, kWRange)) return false;
    const int enemies = Engine::CountEnemiesAt(ally.Position(), 650.0f);
    const bool safe = WAttachAllowed(ally.Position(), true,
        Engine::UnderEnemyTurret(ally.Position()), enemies,
        Slider(WMenu, "MaximumEnemies", 4));
    const bool need = Threatened(ally) || ally.HealthPercent() <=
        Slider(WMenu, "AllyHealth", 72) || mode == Mode::Combo || mode == Mode::Flee;
    if (!safe || !need) return false;
    if (!Engine::ControllerCastUnit(1, ally)) return false;
    AttachedAllyId = static_cast<int>(ally.NetworkId());
    CurrentState = AttachmentState::Attached;
    LastCastTick[1] = Now();
    return true;
}

inline bool DetachW(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || CurrentState == AttachmentState::Detached ||
        !Ready(1, mode, reactive) || PreserveAttack(reactive)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    CurrentState = AttachmentState::Detached;
    AttachedAllyId = 0;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& threat, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode, reactive) ||
        player.ManaPercent() < (reactive ? 8.0f : 18.0f) ||
        PreserveAttack(reactive)) return false;
    const AIHeroClient ally = CurrentState == AttachmentState::Detached ?
        SelectAlly(true) : AllyById(AttachedAllyId);
    const AIHeroClient recipient = ally.IsValid() ? ally : player;
    const int nearby = Engine::CountEnemiesAt(recipient.Position(), 700.0f);
    const bool lethal = recipient.HealthPercent() <= static_cast<float>(Slider(EMenu, "AllyHealth", 66)) - 18.0f;
    const bool worthwhile = EShieldWorthwhile(recipient.HealthPercent(), nearby,
        lethal || HardCcThreatUntil > Now(), CurrentState != AttachmentState::Detached,
        static_cast<float>(Slider(EMenu, "ShieldHealth", 88)));
    if (!worthwhile || (Engine::UnderEnemyTurret(recipient.Position()) && nearby == 0))
        return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastR(const AIHeroClient& threat, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode, reactive) ||
        player.ManaPercent() < (reactive ? 10.0f : 25.0f) ||
        CurrentState == AttachmentState::Channeling || PreserveAttack(reactive))
        return false;
    const AIHeroClient ally = AllyById(AttachedAllyId);
    const Vector3 origin = player.Position();
    const Vector3 aim = Engine::ValidEnemy(threat, kRRange) ?
        PredictPosition(threat, 0.38f) :
        (ally.IsValid() ? ally.Position() : origin);
    if (!aim.IsValid() || aim.IsZero()) return false;
    const Vector3 endpoint = QEndpoint(origin, aim, kRRange);
    if (endpoint.IsZero() || ControllerHelpers::ProjectileWallBlocksFromPlayer(
            endpoint, kRWidth * 0.5f) ||
        SDK::NavMesh::IsWallBetween(origin, endpoint, kRWidth * 0.5f))
        return false;
    std::vector<Vec3> enemyPositions;
    for (const auto& enemy : GameObjects::EnemyHeroes())
        if (Engine::ValidEnemy(enemy, kRRange)) enemyPositions.push_back(enemy.Position());
    const int enemies = RWaveEnemyCount(origin, aim, enemyPositions, kRRange, kRWidth);
    const int allies = Engine::CountAlliesAt(ally.IsValid() ? ally.Position() : origin, 450.0f);
    const bool bestFriend = ally.IsValid() && ally.HasBuff("YuumiWBestFriend");
    const bool defensive = reactive || mode == Mode::Automatic || mode == Mode::Flee;
    const bool healNeed = ally.IsValid() && RHealWorthwhile(ally.HealthPercent(), enemies,
        bestFriend, HardCcThreatUntil > Now(), static_cast<float>(Slider(RMenu, "AllyHealth", 78)));
    if (!defensive && !healNeed && enemies < Slider(RMenu, "MinimumTargets", 2)) return false;
    if (!RChannelSafe(origin, aim, enemies, Engine::UnderEnemyTurret(origin),
                      SDK::NavMesh::IsWallBetween(origin, endpoint, kRWidth * 0.5f),
                      Slider(RMenu, "MaximumEnemies", 4))) return false;
    if (allies == 0 && Engine::UnderEnemyTurret(origin)) return false;
    if (!Engine::ControllerCastPosition(3, endpoint)) return false;
    LastREndpoint = endpoint;
    LastCastTick[3] = RCastTick = Now();
    RWaveHits = 0;
    CurrentState = AttachmentState::Channeling;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (CurrentState == AttachmentState::Detached && CastW(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(QMenu, "HarassMana", 48)) return;
    if (CurrentState == AttachmentState::Detached && CastW(target, Mode::Harass)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastE(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& target) {
    if (CurrentState == AttachmentState::Detached && CastW(target, Mode::Flee, true)) return;
    if (CastE(target, Mode::Flee, true)) return;
    (void)CastR(target, Mode::Flee, true);
}

inline void Automatic(const AIHeroClient& target) {
    const AIHeroClient ally = SelectAlly(true);
    if (ally.IsValid() && Threatened(ally) && CastE(target, Mode::Automatic, true)) return;
    if (CurrentState == AttachmentState::Detached && ally.IsValid() &&
        CastW(target, Mode::Automatic, true)) return;
    if (HardCcThreatUntil > Now() || (ally.IsValid() && ally.HealthPercent() <=
        Slider(EMenu, "AllyHealth", 66))) {
        if (CastE(target, Mode::Automatic, true)) return;
    }
    (void)CastR(target, Mode::Automatic, true);
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const bool attached = IsAttachedRuntime();
    if (attached && CurrentState == AttachmentState::Detached)
        CurrentState = AttachmentState::Attached;
    if (!attached && CurrentState == AttachmentState::Attached &&
        now - LastCastTick[1] > 350) {
        CurrentState = AttachmentState::Detached;
        AttachedAllyId = 0;
    }
    if (CurrentState == AttachmentState::Channeling &&
        (!player.HasBuff("YuumiR") && now - RCastTick > 3500))
        CurrentState = attached ? AttachmentState::Attached : AttachmentState::Detached;
    if (player.HasBuff("YuumiR") || player.HasBuff("YuumiRAbility"))
        CurrentState = AttachmentState::Channeling;
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    ReconcileState();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return true;
    const AIHeroClient target = SelectEnemy(kQRange);
    if (CurrentState == AttachmentState::Channeling) {
        if (mode == Mode::Automatic) (void)CastE(target, mode, true);
        return true;
    }
    if (mode == Mode::Automatic) { Automatic(target); return true; }
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer({}, kWRange)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (player.ManaPercent() >= Slider(FarmMenu, "Mana", 38))
            (void)Engine::TryFarm(mode);
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
        if (slot < 0 || slot >= 4) return;
        LastCastTick[static_cast<std::size_t>(slot)] = now;
        if (slot == 0) QCastTick = now;
        if (slot == 1) {
            const int id = args.TargetNetworkId != 0 ? static_cast<int>(args.TargetNetworkId) :
                static_cast<int>(args.Target.NetworkId);
            if (id != 0) { AttachedAllyId = id; CurrentState = AttachmentState::Attached; }
            else { CurrentState = AttachmentState::Detached; AttachedAllyId = 0; }
        }
        if (slot == 3) { RCastTick = now; CurrentState = AttachmentState::Channeling; }
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
    (void)CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick);
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "YuumiW")) {
        const int senderId = args.Sender.IsValid() ?
            static_cast<int>(args.Sender.NetworkId) : 0;
        if (!IsLocalPlayer(args.Sender) &&
            (AttachedAllyId == 0 || senderId != AttachedAllyId)) return;
        CurrentState = AttachmentState::Attached;
        if (senderId != 0 && !IsLocalPlayer(args.Sender))
            AttachedAllyId = senderId;
    }
    if (Engine::TextContains(args.BuffName, "YuumiR") &&
        IsLocalPlayer(args.Sender)) CurrentState = AttachmentState::Channeling;
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "YuumiW") &&
        (IsLocalPlayer(args.Sender) ||
         static_cast<int>(args.Sender.NetworkId) == AttachedAllyId)) {
        CurrentState = AttachmentState::Detached;
        AttachedAllyId = 0;
    }
    if (Engine::TextContains(args.BuffName, "YuumiR") &&
        IsLocalPlayer(args.Sender))
        CurrentState = IsAttachedRuntime() ? AttachmentState::Attached : AttachmentState::Detached;
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) {
        LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
        LastAutoTick = Now();
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, GapcloserTargetId, GapcloserEndpoint,
                           GapcloserExpireTick, 900.0f, 1100);
    EnemyThreatUntil = std::max(EnemyThreatUntil, GapcloserExpireTick);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 1000, 250, 5000>(args);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (ControllerHelpers::AnyTextContains({args.SpellName, args.MissileName}, {"yuumiq", "prowling"})) {
        QMissileId = args.MissileNetworkId ? static_cast<int>(args.MissileNetworkId) :
            static_cast<int>(args.Sender.NetworkId);
        QCastTick = Now();
    }
    if (ControllerHelpers::AnyTextContains({args.SpellName, args.MissileName}, {"yuumir", "finalchapter"})) {
        RMissileId = args.MissileNetworkId ? static_cast<int>(args.MissileNetworkId) :
            static_cast<int>(args.Sender.NetworkId);
        CurrentState = AttachmentState::Channeling;
    }
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = args.MissileNetworkId ? static_cast<int>(args.MissileNetworkId) :
        static_cast<int>(args.Sender.NetworkId);
    if (id == QMissileId) QMissileId = 0;
    if (id == RMissileId) RMissileId = 0;
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFFFB7E8u, 1.2f, 36);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFB67CFFu, 1.4f, 36);
    if (LastQEndpoint.IsValid()) Drawing::DrawCircle(LastQEndpoint, kQWidth * 0.5f, 0xFFFFB7E8u, 1.0f, 18);
    if (LastREndpoint.IsValid()) Drawing::DrawCircle(LastREndpoint, kRWaveRadius, 0xFFB67CFFu, 1.0f, 18);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("YuumiTactics", "Yuumi attachment tactics"));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Prowling Projectile steering"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 48, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "You and Me attachment"));
    WMenu->Add(new MenuSlider("AllyHealth", "Attach for ally below health %", 72, 10, 95));
    WMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies at ally", 4, 0, 6));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Zoomies heal and shield"));
    EMenu->Add(new MenuSlider("AllyHealth", "Shield ally below health %", 66, 10, 95));
    EMenu->Add(new MenuSlider("ShieldHealth", "Shield threshold", 88, 40, 100));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Final Chapter channel"));
    RMenu->Add(new MenuSlider("AllyHealth", "Heal ally below health %", 78, 10, 95));
    RMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies around wave", 4, 0, 6));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum combo wave targets", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Farm", "Mana-safe farming"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 38, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("Coach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and R ranges", false));
}

inline void OnLoad() {
    LastCastTick.fill(0);
    LastAutoTargetId = LastAutoTick = AttachedAllyId = 0;
    QCastTick = RCastTick = RWaveHits = QMissileId = RMissileId = 0;
    EnemyThreatUntil = HardCcThreatUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = InterruptTargetId = InterruptExpireTick = 0;
    GapcloserEndpoint = {};
    LastQEndpoint = LastREndpoint = {};
    CurrentState = AttachmentState::Detached;
}

inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    OnLoad();
}

inline constexpr const char* Scenarios[] = {
    "Pin Yuumi mechanics to Riot 26.15 and CommunityDragon 16.15 metadata",
    "Reconcile detached, attached ally and Final Chapter channel states from events and polling",
    "Use the engine-selected enemy target for every combat mode",
    "Steer attached Prowling Projectile during its cursor window with prediction and collision",
    "Use Q projectile speed, target radius, reach and projectile-wall rejection gates",
    "Attach only to the highest-value valid carry or threatened ally with bounded enemy density",
    "Never spend You and Me mobility into an unsafe turret or enemy cluster",
    "Keep AttachedAllyId and Best Friend policy coherent after ally transfer or detach",
    "Cast Zoomies as a meaningful heal/shield for the attached ally or endangered player",
    "Gate Zoomies by health, lethal threat, nearby enemies, mana and cooldown readiness",
    "Channel five Final Chapter waves only through predicted reach and wall safety",
    "Use Final Chapter ally heal/shield value and enemy count for defensive or offensive commitment",
    "Preserve ally safety while allowing movement, attach and Zoomies during the R channel",
    "Track Yuumi Q and R missile lifecycle without duplicating channel casts",
    "Preserve ordinary attack windup while allowing urgent peel and protection reactions",
    "Track player Q/W/E/R casts from events while reconciling missing state",
    "Combo attaches, shields, steers Q and channels a safe multi-wave chapter",
    "Harass preserves a mana floor and spends only high-value Q or Zoomies",
    "LaneClear Jungle and LastHit delegate only after resource floor is met",
    "Flee attaches to a safe ally, protects them and uses Final Chapter for peel",
    "Automatic mode prioritizes threatened ally attachment, Zoomies and channel safety",
    "Expose complete load unload menu update draw spell buff attack gapcloser interrupt object and missile callbacks",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Yuumi;
    controller.ControllerId = "champion.kuroaio.ai.yuumi.support";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIYuumi.md";
    controller.ImplementationSummary =
        "Attached/detached state reconciliation, steering Prowling Projectile, ally-safe You and Me and Zoomies, and Final Chapter wave/channel policy.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Yuumi
