#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIIvernGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Ivern {

using namespace Geometry;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Lethal;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SelectJungleTarget;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* DaisyMenu = nullptr;
inline std::array<int, 4> LastCastTick{};
inline int LastAutoTargetId = 0;
inline int IncomingThreatUntil = 0;
inline Vector3 IncomingThreatEndpoint{};
inline int IncomingThreatTargetId = 0;
inline int MarkedCampId = 0;
inline int MarkExpireTick = 0;
inline MarkState GroveMark{};
inline BrushState ActiveBrush{};
inline ShieldState ActiveShield{};
inline DaisyState Daisy{};

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
        (reactive || LastCastTick[static_cast<std::size_t>(slot)] + 45 <= Now());
}

inline bool HasResourceFor(int slot, float reserve = 0.0f) {
    return CurrentResource() + 0.5f >= SpellCost(slot) + std::max(0.0f, reserve);
}

inline bool PreserveAttack(bool reactive, bool lethal = false) {
    return !reactive && !lethal && Orbwalker::IsWindingUp() &&
        ::Plugins::KuroAIO::Bool(Engine::HumanMenu, "PreserveAttacks", true);
}

inline bool TargetBlocked(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || IsCommonUntargetableOrImmune(target) ||
        target.HasBuff("BansheesVeil") || target.HasBuff("EdgeOfNight") ||
        target.HasBuff("MorganaE") || target.HasBuff("BlackShield");
}

inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculateMagicDamage(target,
        QRawDamage(ControllerHelpers::SpellRank(0), player.AP()));
}

inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculateMagicDamage(target,
        WBonusDamage(ControllerHelpers::SpellRank(1), player.AP()));
}

inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculateMagicDamage(target,
        ERawDamage(ControllerHelpers::SpellRank(2), player.AP()));
}

inline bool SafeDash(const AIHeroClient& target, bool lethal, bool reactive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return false;
    const Vector3 endpoint = ClampQDashEndpoint(player.Position(), target.Position());
    if (endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint)) return false;
    if (Engine::UnderEnemyTurret(endpoint) && !lethal && !reactive) return false;
    if (Engine::CountEnemiesAt(endpoint, 525.0f) > Slider(TacticsMenu, "MaxDashEnemies", 2) &&
        !lethal && !reactive) return false;
    return true;
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    const int now = Now();
    GroveMark = ReconcileGroveMark(GroveMark, false, MarkedCampId, now);
    if (MarkExpireTick > 0 && now >= MarkExpireTick) {
        MarkedCampId = 0;
        MarkExpireTick = 0;
        GroveMark = {};
    }
    if (ActiveBrush.Active && now >= ActiveBrush.ExpireTick) ActiveBrush = {};
    if (ActiveShield.Active && now >= ActiveShield.DetonateTick) ActiveShield = {};
    Daisy = ReconcileDaisy(Daisy, false, Daisy.NetworkId, now);
    if (!player.IsValid()) return;
    if (player.HasBuff("IvernW") || player.HasBuff("IvernWActive")) {
        ActiveBrush = ReconcileBrush(ActiveBrush, true, player.Position(), now);
    }
    if (player.HasBuff("IvernE") || player.HasBuff("IvernEManager")) {
        ActiveShield = ReconcileShield(ActiveShield, true,
            static_cast<int>(player.NetworkId()), now);
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (!monster.IsValid() || monster.IsDead() || !monster.IsTargetable()) continue;
        const int id = static_cast<int>(monster.NetworkId());
        if (monster.HasBuff("IvernPassive") || monster.HasBuff("IvernMark")) {
            MarkedCampId = id;
            MarkExpireTick = now + 6000;
            GroveMark = ReconcileGroveMark(GroveMark, true, id, now);
            break;
        }
    }
}

inline AIHeroClient ProtectedAlly(bool urgent = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    if (urgent || player.HealthPercent() <= Slider(TacticsMenu, "SelfShieldHP", 35))
        return player;
    const auto ally = SelectProtectionAlly(kERange, 0, 0, 300.0f, 700.0f);
    return Engine::ValidAlly(ally, kERange) ? ally : player;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) || !Ready(0, mode, reactive) ||
        !HasResourceFor(0, reactive ? 0.0f : Slider(TacticsMenu, "ManaReserve", 20)) ||
        PreserveAttack(reactive, Lethal(target, QDamage(target))) ||
        player.Position().Distance2D(target.Position()) > kQRange + target.BoundingRadius()) return false;
    const Vector3 predicted = PredictPosition(target, kQDelay);
    if (predicted.IsZero() || ControllerHelpers::ProjectileWallBlocksFromPlayer(predicted, kQWidth * 0.5f)) return false;
    std::vector<QBody> bodies;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && !IsCommonUntargetableOrImmune(enemy))
            bodies.push_back({PredictPosition(enemy, kQDelay), enemy.BoundingRadius(),
                static_cast<int>(enemy.NetworkId()), true});
    }
    const Vec3 direction = Direction2D(player.Position(), predicted);
    const int first = FirstQCollision(player.Position(), direction, bodies);
    if (first < 0 || bodies[static_cast<std::size_t>(first)].Id != static_cast<int>(target.NetworkId())) return false;
    const bool lethal = Lethal(target, QDamage(target));
    if (!SafeDash(target, lethal, reactive)) return false;
    if (!Engine::ControllerCastPosition(0, predicted)) return false;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode, reactive) ||
        !HasResourceFor(1, reactive ? 0.0f : 10.0f) || PreserveAttack(reactive) ||
        (!Engine::ValidEnemy(target, kWRange) && mode != Mode::Automatic)) return false;
    Vector3 center = Engine::ValidEnemy(target, kWRange) ? PredictPosition(target, 0.2f) : player.Position();
    if (center.IsZero() || player.Position().Distance2D(center) > kWRange) return false;
    if (Engine::UnderEnemyTurret(center) && !Engine::UnderEnemyTurret(player.Position())) return false;
    if (!Engine::ControllerCastPosition(1, center)) return false;
    ActiveBrush = BeginBrush(center, Now(), Slider(TacticsMenu, "BrushDurationMs", kBrushDurationMs));
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& requested, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode, reactive) ||
        !HasResourceFor(2, reactive ? 0.0f : Slider(TacticsMenu, "ManaReserve", 20))) return false;
    AIHeroClient ally = requested;
    if (!Engine::ValidAlly(ally, kERange)) ally = ProtectedAlly(reactive || mode == Mode::Flee);
    if (!Engine::ValidAlly(ally, kERange)) return false;
    const bool urgent = reactive || mode == Mode::Flee || ally.HealthPercent() <= Slider(TacticsMenu, "ShieldAllyHP", 65) ||
        Engine::CountEnemiesAt(ally.Position(), kEDetonationRadius) > 0;
    if (!urgent && mode == Mode::Harass && ally.NetworkId() == player.NetworkId()) return false;
    if (!Engine::ControllerCastUnit(2, ally)) return false;
    ActiveShield = ApplyShield(static_cast<int>(ally.NetworkId()), Now(), kEShieldDurationMs);
    LastCastTick[2] = Now();
    return true;
}

inline bool CastR(Mode mode, bool reactive = false, bool objective = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Daisy.Active || !Ready(3, mode, reactive) ||
        !HasResourceFor(3, reactive ? 0.0f : Slider(DaisyMenu, "ManaReserve", 15))) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), 850.0f);
    if (!reactive && !objective && enemies < Slider(DaisyMenu, "MinimumEnemies", 1)) return false;
    if (!objective && mode != Mode::Combo && mode != Mode::Automatic && !reactive) return false;
    if (enemies > Slider(DaisyMenu, "MaxSummonEnemies", 3) && !reactive && !objective) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    Daisy = SummonDaisy(0xD415A, Now(), Slider(DaisyMenu, "DaisyDurationMs", kDaisyDurationMs));
    LastCastTick[3] = Now();
    return true;
}

inline bool TryJungle(Mode mode) {
    const auto monster = SelectJungleTarget(850.0f, 0.15f);
    if (!monster.IsValid()) return false;
    const AIBaseClient unit(monster.Handle());
    const int id = static_cast<int>(monster.NetworkId());
    const bool objective = IsEpicMonster(unit);
    const auto player = GameObjects::Player();
    if (objective && !Daisy.Active && ::Plugins::KuroAIO::Bool(FarmMenu, "ObjectiveDaisy", true) &&
        Engine::CountEnemiesAt(monster.Position(), 900.0f) <= Slider(FarmMenu, "ObjectiveMaxEnemies", 3) &&
        CastR(Mode::Jungle, false, true)) return true;
    if (objective && Daisy.Active && ::Plugins::KuroAIO::Bool(FarmMenu, "ObjectiveShield", true) &&
        CastE(player, Mode::Jungle, false)) return true;
    if (!GroveMarkActive(GroveMark, id, Now()) && ::Plugins::KuroAIO::Bool(FarmMenu, "TrackGroveMarks", true)) {
        MarkedCampId = id;
        MarkExpireTick = Now() + 6000;
        GroveMark = ApplyGroveMark(id, Now());
    }
    if (Ready(0, mode) && SpellEnabled(0, mode) && HasResourceFor(0, 15.0f) &&
        player.Position().Distance2D(monster.Position()) <= kQRange + monster.BoundingRadius())
        return Engine::ControllerCastPosition(0, monster.Position());
    return false;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    const auto ally = ProtectedAlly();
    if (Engine::CountEnemiesAt(target.Position(), 650.0f) >= 2 && CastR(Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastE(ally, Mode::Combo)) return;
    (void)CastR(Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || GameObjects::Player().ManaPercent() < Slider(TacticsMenu, "HarassMana", 55)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (Engine::CountEnemiesAt(player.Position(), 850.0f) > 0 && CastE(player, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastQ(target, Mode::Flee, true);
}

inline void Automatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (IncomingThreatUntil > Now() && CastE(player, Mode::Automatic, true)) return;
    if (ControllerHelpers::HasNearbyEpicMonster(850.0f) && !Daisy.Active && CastR(Mode::Automatic, false, true)) return;
    if (Engine::ValidEnemy(target) && Engine::CountEnemiesAt(target.Position(), 650.0f) >= 2)
        (void)CastR(Mode::Automatic);
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    ReconcileState();
    const AIHeroClient target = Engine::SelectTarget(kQRange);
    if (mode == Mode::Flee) Flee(target);
    else if (mode == Mode::Combo) Combo(target);
    else if (mode == Mode::Harass) Harass(target);
    else if (mode == Mode::Jungle) TryJungle(mode);
    else if (mode == Mode::LaneClear || mode == Mode::LastHit) {
        if (GameObjects::Player().ManaPercent() >= Slider(FarmMenu, "LaneMana", 45))
            (void)Engine::TryFarm(mode);
    } else Automatic(target);
    return true;
}

inline void OnLoad() {
    TacticsMenu = nullptr; FarmMenu = nullptr; DaisyMenu = nullptr;
    LastCastTick = {}; LastAutoTargetId = 0;
    IncomingThreatUntil = IncomingThreatTargetId = 0; IncomingThreatEndpoint = {};
    MarkedCampId = MarkExpireTick = 0;
    GroveMark = {}; ActiveBrush = {}; ActiveShield = {}; Daisy = {};
}
inline void OnUnload() { OnLoad(); }

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            LastCastTick[static_cast<std::size_t>(slot)] = Now();
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args, 220.0f, 110.0f, 250, 260, 260, 1500, 450);
    if (analysis.Valid && analysis.Enemy.IsValid()) {
        IncomingThreatTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
    }
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack && args.Target.IsValid())
        LastAutoTargetId = static_cast<int>(args.Target.NetworkId);
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "IvernW"))
        ActiveBrush = BeginBrush(args.Sender.Position, Now());
    if (Engine::TextContains(args.BuffName, "IvernE"))
        ActiveShield = ApplyShield(static_cast<int>(args.Sender.NetworkId), Now());
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "IvernE") &&
        ActiveShield.AllyId == static_cast<int>(args.Sender.NetworkId)) ActiveShield = {};
    if (Engine::TextContains(args.BuffName, "IvernW") && IsLocalPlayer(args.Sender)) ActiveBrush = {};
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    CaptureGapcloser(args, IncomingThreatTargetId, IncomingThreatEndpoint,
        IncomingThreatUntil, 900.0f, 1100);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    int target = 0, until = 0;
    CaptureInterruptable(args, target, until, 900, 250, 5000);
    if (until > Now()) { IncomingThreatTargetId = target; IncomingThreatUntil = until; }
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() && Engine::TextContains(args.Sender.Name, "Daisy"))
        Daisy = SummonDaisy(static_cast<int>(args.Sender.NetworkId), Now());
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (Daisy.Active && args.Sender.IsValid() &&
        static_cast<int>(args.Sender.NetworkId) == Daisy.NetworkId) Daisy = {};
}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDraw() {}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("IvernTactics", "Grove, rootcaller and protection"));
    TacticsMenu->Add(new MenuSlider("ManaReserve", "Mana reserve", 20, 0, 80));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 55, 0, 100));
    TacticsMenu->Add(new MenuSlider("MaxDashEnemies", "Maximum enemies at Q dash endpoint", 2, 1, 5));
    TacticsMenu->Add(new MenuSlider("ShieldAllyHP", "Shield ally below HP percent", 65, 1, 100));
    TacticsMenu->Add(new MenuSlider("SelfShieldHP", "Prefer self shield below HP percent", 35, 1, 100));
    TacticsMenu->Add(new MenuSlider("BrushDurationMs", "Brush state duration (ms)", kBrushDurationMs, 5000, 30000));
    FarmMenu = root->AddSubMenu(new Menu("IvernFarm", "Grove, lane and objectives"));
    FarmMenu->Add(new MenuBool("TrackGroveMarks", "Track marked jungle camps", true));
    FarmMenu->Add(new MenuBool("ObjectiveDaisy", "Summon Daisy for epic objectives", true));
    FarmMenu->Add(new MenuBool("ObjectiveShield", "Shield Daisy at objectives", true));
    FarmMenu->Add(new MenuSlider("ObjectiveMaxEnemies", "Maximum nearby objective enemies", 3, 0, 5));
    FarmMenu->Add(new MenuSlider("LaneMana", "Lane clear mana percent", 45, 0, 100));
    DaisyMenu = root->AddSubMenu(new Menu("IvernDaisy", "Daisy lifecycle and teamfight gate"));
    DaisyMenu->Add(new MenuSlider("ManaReserve", "Daisy mana reserve", 15, 0, 80));
    DaisyMenu->Add(new MenuSlider("MinimumEnemies", "Minimum enemies for Daisy", 1, 1, 5));
    DaisyMenu->Add(new MenuSlider("MaxSummonEnemies", "Maximum nearby enemies for summon", 3, 1, 5));
    DaisyMenu->Add(new MenuSlider("DaisyDurationMs", "Daisy lifecycle duration (ms)", kDaisyDurationMs, 10000, 60000));
}

inline constexpr const char* Scenarios[] = {
    "Passive Friend of the Forest grove marks with camp identity, expiry and polling reconciliation",
    "Q Rootcaller prediction, projectile-wall and first-collision checks with safe root dash gating",
    "W Brushmaker placement, brush empowerment state and attack-windup protection",
    "E Triggerseed ally selection, shield duration, detonation expiry and threat-aware peel",
    "R Daisy summon object lifecycle, teamfight safety gate and epic-objective policy",
    "Autonomous target selection, resource/cooldown checks and turret/enemy-count gates",
    "Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic routes",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Ivern;
    controller.ControllerId = "champion.kuroaio.ai.ivern.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIIvern.md";
    controller.ImplementationSummary =
        "Ivern grove-mark, brush empowerment, root dash, ally Triggerseed shield/detonation and Daisy lifecycle controller with safe objective policy.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Ivern
