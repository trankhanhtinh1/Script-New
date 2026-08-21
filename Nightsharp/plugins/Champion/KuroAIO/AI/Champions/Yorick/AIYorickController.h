#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIYorickGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Yorick {

using namespace Geometry;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::ObjectEventIsAllied;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::Ready;
using ControllerHelpers::SpellCost;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<GraveState, 24> Graves = {};
inline std::array<MistWalkerState, 8> Walkers = {};
inline int WalkerCount = 0;
inline int MaidenNetworkId = 0;
inline MaidenState Maiden = MaidenState::Absent;
inline int MaidenSpawnTick = 0;
inline int MaidenExpireTick = 0;
inline int QCastTick = 0;
inline int WCastTick = 0;
inline int ECastTick = 0;
inline int RCastTick = 0;
inline int ETargetId = 0;
inline int EMarkExpireTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline std::array<int, 4> LastCastTick = {};

inline bool Throttle(int slot, int delay = 55) {
    return slot >= 0 && slot < 4 && LastCastTick[static_cast<std::size_t>(slot)] + delay <= Now();
}

inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           ControllerHelpers::HasSpellShieldOrImmunity(target);
}

inline GraveState* FindGrave(const Vec3& position, bool create = false) {
    for (auto& grave : Graves) {
        if (grave.Confirmed && !grave.Consumed && grave.Position.Distance2D(position) < 80.0f)
            return &grave;
    }
    if (!create) return nullptr;
    for (auto& grave : Graves) {
        if (!grave.Confirmed || grave.Consumed || grave.SpawnTick + 30000 < Now()) {
            grave = {};
            grave.Position = position;
            grave.SpawnTick = Now();
            grave.Confirmed = true;
            return &grave;
        }
    }
    return nullptr;
}
inline bool HasResource(int slot) {
    return CurrentResource() + 0.01f >= SpellCost(slot);
}

inline int LiveGraveCount() {
    int count = 0;
    for (const auto& grave : Graves) {
        if (grave.Confirmed && !grave.Consumed && grave.SpawnTick + 30000 >= Now()) ++count;
    }
    return count;
}

inline void ScanWalkers() {
    for (auto& walker : Walkers) {
        if (walker.NetworkId != 0 && walker.SpawnTick + 12000 < Now()) walker = {};
    }
    for (const auto& minion : GameObjects::AllyMinions()) {
        if (!minion.IsValid() || minion.IsDead() ||
            !ControllerHelpers::AnyTextContains({minion.Name().c_str(), minion.CharacterName().c_str()},
                                                 {"yorickghoul", "mistwalker", "yorickwalker"})) continue;
        const int id = static_cast<int>(minion.NetworkId());
        MistWalkerState* slot = nullptr;
        for (auto& walker : Walkers) if (walker.NetworkId == id) slot = &walker;
        if (!slot) for (auto& walker : Walkers) if (walker.NetworkId == 0) { slot = &walker; walker.NetworkId = id; break; }
        if (!slot) continue;
        slot->Position = minion.Position();
        slot->SpawnTick = slot->SpawnTick == 0 ? Now() : slot->SpawnTick;
        slot->Confirmed = true;
        slot->Alive = true;
    }
    WalkerCount = 0;
    for (const auto& walker : Walkers) if (walker.NetworkId != 0 && walker.Confirmed && walker.Alive) ++WalkerCount;
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    ScanWalkers();
    for (auto& grave : Graves) {
        if (grave.Confirmed && (grave.Consumed || grave.SpawnTick + 30000 < now)) grave = {};
    }
    const bool rBuff = player.HasBuff("YorickR") || player.HasBuff("YorickRMaiden");
    if (Maiden == MaidenState::Alive && (!rBuff || now > MaidenExpireTick)) Maiden = MaidenState::Dead;
    if (Maiden == MaidenState::Alive && MaidenSpawnTick > 0 && now > MaidenExpireTick) Maiden = MaidenState::Dead;
    if (EMarkExpireTick > 0 && now > EMarkExpireTick) ETargetId = EMarkExpireTick = 0;
}

inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculatePhysicalDamage(target, QRawDamage(SpellRank(0), player.TotalAttackDamage()));
}

inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculateMagicDamage(target, ERawDamage(SpellRank(2), target.Health(), player.BonusAttackDamage()));
}

inline bool SafePlacement(const Vector3& center, bool defensive, bool lethal) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !center.IsValid() || center.IsZero()) return false;
    WPlacementContext context{};
    context.Ready = Ready(1);
    context.CenterValid = InWRange(player.Position(), center);
    context.Walkable = !SDK::NavMesh::IsWall(center);
    context.ProjectileWall = false;
    context.UnderEnemyTurret = Engine::UnderEnemyTurret(center);
    context.Defensive = defensive;
    context.Lethal = lethal;
    context.NearbyEnemies = Engine::CountEnemiesAt(center, 430.0f);
    context.MaximumEnemies = Slider(WMenu, "MaxEnemies", 2);
    return WPlacementSafe(context);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!HasResource(0)) return false;
    if (!player.IsValid() || Protected(target) || !Ready(0, mode) || !Throttle(0) ||
        !Engine::ValidEnemy(target, kQRange + target.BoundingRadius())) return false;
    const bool lethal = ControllerHelpers::Lethal(target, QDamage(target));
    if (!reactive && !lethal && ControllerHelpers::PreserveAttack(false, false)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    QCastTick = LastCastTick[0] = Now();
    const auto grave = FindGrave(player.Position());
    const bool raise = grave && CanRaiseWalkers(
        LiveGraveCount(), WalkerCount, true,
        Engine::UnderEnemyTurret(player.Position()), lethal);
    if (raise) grave->Consumed = true;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!HasResource(1)) return false;
    if (!player.IsValid() || Protected(target) || !Ready(1, mode) || !Throttle(1) ||
        !Engine::ValidEnemy(target, kWRange)) return false;
    const Vector3 aim = PredictPosition(target, 0.25f);
    const bool lethal = ControllerHelpers::Lethal(target, QDamage(target));
    if (!SafePlacement(aim, reactive, lethal)) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    WCastTick = LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!HasResource(2)) return false;
    if (!player.IsValid() || Protected(target) || !Ready(2, mode) || !Throttle(2) ||
        !Engine::ValidEnemy(target, kERange)) return false;
    const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    const Vector3 aim = prediction.GetCastPosition().IsValid() ? prediction.GetCastPosition() : PredictPosition(target, kECastDelay);
    if (!aim.IsValid() || aim.IsZero() || prediction.CollisionObjects.size() > 0 ||
        ProjectileWallBlocksFromPlayer(aim, kEWidth * 0.5f)) return false;
    const bool marked = ETargetId == static_cast<int>(target.NetworkId()) && EMarkExpireTick > Now();
    const bool lethal = ControllerHelpers::Lethal(target, EDamage(target));
    if (!EProjectileSafe(true, false, false, marked, lethal) && !reactive) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    ECastTick = LastCastTick[2] = Now();
    ETargetId = static_cast<int>(target.NetworkId());
    EMarkExpireTick = ECastTick + static_cast<int>(EMarkDurationSeconds(SpellRank(2)) * 1000.0f);
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!HasResource(3)) return false;
    if (!player.IsValid() || !Engine::ValidEnemy(target, kRRange) || !Ready(3, mode) || !Throttle(3) ||
        Maiden == MaidenState::Alive) return false;
    const Vector3 aim = PredictPosition(target, 0.25f);
    MaidenCastContext context{};
    context.Ready = true;
    context.TargetValid = aim.IsValid() && !aim.IsZero();
    context.UnderEnemyTurret = Engine::UnderEnemyTurret(aim);
    context.Defensive = reactive || player.HealthPercent() <= 34.0f;
    bool wave = false;
    for (const auto& minion : GameObjects::AllyMinions()) {
        if (minion.IsValid() && !minion.IsDead() &&
            minion.Position().Distance2D(aim) <= 900.0f &&
            !ControllerHelpers::AnyTextContains(
                {minion.Name().c_str(), minion.CharacterName().c_str()},
                {"yorickghoul", "mistwalker", "yorickwalker"})) {
            wave = true;
            break;
        }
    }
    context.SplitPush = Bool(TacticsMenu, "SplitPush", true) &&
        Engine::CountEnemiesAt(player.Position(), 800.0f) == 0;
    context.SafeSplit = !context.SplitPush || SplitPushSafe(
        {true, player.HealthPercent() > 45.0f, false, player.IsVisible(),
         false, false, wave, 0, 1});
    context.NearbyEnemies = Engine::CountEnemiesAt(aim, 430.0f);
    context.MaximumEnemies = Slider(RMenu, "MinimumEnemies", 1);
    if (!ShouldCastMaiden(context)) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    RCastTick = LastCastTick[3] = Now();
    Maiden = MaidenState::Alive;
    MaidenSpawnTick = RCastTick;
    MaidenExpireTick = RCastTick + static_cast<int>(MaidenDurationSeconds(SpellRank(3)) * 1000.0f);
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kRRange)) return;
    if (CastR(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastQ(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kERange)) return;
    if (CastE(target, Mode::Harass)) return;
    if (CastW(target, Mode::Harass)) return;
    (void)CastQ(target, Mode::Harass);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Engine::CountEnemiesAt(player.Position(), 850.0f) > 0) return;
    if (mode == Mode::LastHit && LiveGraveCount() > 0) {
        const AIHeroClient target = Engine::SelectTarget(kQRange);
        if (target.IsValid() && CastQ(target, mode)) return;
    }
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kWRange)) return;
    if (CastW(target, Mode::Flee, true)) return;
    (void)CastE(target, Mode::Flee, true);
}

inline void Automatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (Engine::ValidEnemy(target, kERange) &&
        ControllerHelpers::Lethal(target, EDamage(target))) {
        if (CastE(target, Mode::Automatic, true)) return;
    }
    if (Engine::ValidEnemy(target, kWRange) && CastW(target, Mode::Automatic, true)) return;
    if (Engine::ValidEnemy(target, kQRange) &&
        ControllerHelpers::Lethal(target, QDamage(target))) (void)CastQ(target, Mode::Automatic, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    ReconcileState();
    const float range = mode == Mode::Flee ? kWRange : kRRange;
    const AIHeroClient target = Engine::SelectTarget(range);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer({}, kWRange)); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int slot = static_cast<int>(args.Slot);
    if (IsLocalPlayer(args.Sender)) {
        if (slot < 0 || slot > 3) return;
        if (slot == 0) QCastTick = Now();
        else if (slot == 2) { ECastTick = Now(); ETargetId = static_cast<int>(args.TargetNetworkId); EMarkExpireTick = Now() + 6000; }
        else if (slot == 3) { RCastTick = Now(); Maiden = MaidenState::Alive; MaidenSpawnTick = Now(); MaidenExpireTick = Now() + 60000; }
        return;
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "YorickE") &&
        args.Sender.IsValid()) {
        ETargetId = static_cast<int>(args.Sender.NetworkId);
        EMarkExpireTick = Now() + 6000;
    }
    if (IsLocalPlayer(args.Sender) &&
        Engine::TextContains(args.BuffName, "YorickR")) {
        Maiden = MaidenState::Alive;
        MaidenExpireTick = Now() + 60000;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "YorickE") &&
        static_cast<int>(args.Sender.NetworkId) == ETargetId) {
        ETargetId = EMarkExpireTick = 0;
    }
    if (IsLocalPlayer(args.Sender) &&
        Engine::TextContains(args.BuffName, "YorickR")) {
        Maiden = MaidenState::Dead;
    }
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) { if (args.Target.IsValid()) { LastAutoTargetId = static_cast<int>(args.Target.NetworkId()); LastAutoTick = Now(); } }
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) { (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick); }
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) { if (IsLocalPlayer(args.Sender) && args.IsAutoAttack && QCastTick > 0 && QCastTick + 600 > Now()) QCastTick = 0; }

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const bool graveObject = ControllerHelpers::AnyTextContains(
        {args.Sender.Name, args.Sender.CharacterName, args.SpellName},
        {"yorickgrave"});
    if (graveObject) FindGrave(args.Sender.Position, true);
    if (ObjectEventIsAllied(args) &&
        ControllerHelpers::AnyTextContains(
            {args.Sender.Name, args.Sender.CharacterName, args.SpellName},
            {"yorickmaiden", "yorickr"})) {
        Maiden = MaidenState::Alive;
        MaidenNetworkId = static_cast<int>(args.Sender.NetworkId);
        MaidenSpawnTick = Now();
        MaidenExpireTick = Now() + 60000;
    }
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (id != 0 && id == MaidenNetworkId) {
        Maiden = MaidenState::Dead;
        MaidenNetworkId = 0;
    }
    for (auto& walker : Walkers) if (walker.NetworkId == id) walker = {};
}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) { (void)args; }
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) { (void)args; }
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFB38CFFu, 1.2f, 32);
    Drawing::DrawCircle(player.Position(), kERange, 0xFF70C8FFu, 1.2f, 36);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("YorickTactics", "Yorick grave and Maiden tactics"));
    TacticsMenu->Add(new MenuBool("SplitPush", "Allow safe split-push Maiden", true));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Last Rites"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Dark Procession"));
    WMenu->Add(new MenuSlider("MaxEnemies", "Maximum cage endpoint enemies", 2, 0, 5));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Mourning Mist"));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Eulogy of the Isles"));
    RMenu->Add(new MenuSlider("MinimumEnemies", "Minimum fight enemies", 1, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("YorickFarm", "Grave and lane policy"));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("YorickCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/E ranges", false));
}

inline void ResetState() {
    Graves = {};
    Walkers = {};
    WalkerCount = 0;
    MaidenNetworkId = 0;
    Maiden = MaidenState::Absent;
    MaidenSpawnTick = MaidenExpireTick = 0;
    QCastTick = WCastTick = ECastTick = RCastTick = 0;
    ETargetId = EMarkExpireTick = 0;
    LastAutoTargetId = LastAutoTick = 0;
    LastCastTick.fill(0);
}
inline void OnLoad() { ResetState(); }
inline void OnUnload() { ResetState(); TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr; }

inline constexpr const char* Scenarios[] = {
    "Track confirmed graves and four Mist Walkers from object and minion observations",
    "Consume one nearby grave only after Q Last Rites cast and preserve the auto reset",
    "Use the engine-selected target for every combat mode",
    "Place Dark Procession only on a valid walkable endpoint with turret and enemy-count gates",
    "Reject cage projectile-wall paths and allow defensive or lethal exceptions",
    "Predict Mourning Mist, reject collision and projectile walls, then reconcile E mark lifetime",
    "Apply current-health and bonus-AD E damage for marked lethal checks",
    "Track Maiden summon object, buff lifecycle, death and 60-second expiry",
    "Release Maiden only when the target is valid and fight or safe split-push criteria hold",
    "Require wave, health, visibility and retreat/teleport safety before split-push automation",
    "Reconcile every state from both events and polling rather than trusting local casts",
    "Preserve ordinary attack windup and let Q own its explicit auto reset",
    "Gate automatic reactions on lethal E/Q or an active wall peel",
    "Use distinct Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic routes",
    "Never automate item, summoner, wall-blink or unsafe turret-diving decisions",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Yorick;
    controller.ControllerId = "champion.kuroaio.ai.yorick.graves";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIYorick.md";
    controller.ImplementationSummary = "Grave and Mist Walker tracking, Q reset, cage and marked mist safety, and bounded Maiden split-push lifecycle.";
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
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Yorick
