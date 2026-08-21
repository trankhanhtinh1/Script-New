#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AITryndamere.h"
#include "AITryndamereGeometry.h"
#include "../../../../../../SDK/Extensions/Unit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Tryndamere {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HasCurrentResource;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;

inline Menu* TacticsMenu = nullptr;
inline int Fury = 0;
inline int LastAttackTargetId = 0;
inline int LastAttackTick = 0;
inline int LastCastTick = 0;
inline std::array<int, 4> LastSpellCastTick{};
inline bool RActive = false;
inline int RExpireTick = 0;
inline int LastRCastTick = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline Vector3 IncomingThreatEndpoint{};

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
        HasCurrentResource(SpellCost(slot)) &&
        (reactive || LastSpellCastTick[static_cast<std::size_t>(slot)] + 55 <= Now());
}

inline bool PreserveAttackWindow(bool reactive) {
    return !reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true);
}

inline bool TargetInShoutRange(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target, kShoutRadius + 60.0f) &&
        MockingShoutInRange(player.Position().Distance2D(target.Position()),
                            target.BoundingRadius());
}

inline bool CastQ(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0, mode, reactive) ||
        PreserveAttackWindow(reactive)) return false;
    const float health = player.HealthPercent();
    const float threshold = static_cast<float>(Slider(TacticsMenu, "QHealth", 54));
    const bool missingHealth = health <= threshold && player.Health() < player.MaxHealth();
    const bool postR = RActive && health <= 72.0f;
    if (!missingHealth && !postR && mode != Mode::Flee) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    Fury = std::clamp(static_cast<int>(CurrentResource(100.0f)), 0, 100);
    LastSpellCastTick[0] = LastCastTick = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !TargetInShoutRange(target) ||
        !Ready(1, mode, reactive) || PreserveAttackWindow(reactive)) return false;
    const float distance = player.Position().Distance2D(target.Position());
    const bool targetFacing = SDK::Extensions::IsFacing(target, player);
    const bool slow = MockingShoutSlowApplies(distance, targetFacing,
                                               target.BoundingRadius());
    const bool defensive = player.HealthPercent() <=
        static_cast<float>(Slider(TacticsMenu, "WHealth", 58));
    if (!slow && !defensive && mode == Mode::Harass) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastSpellCastTick[1] = LastCastTick = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool escape = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode, reactive) ||
        PreserveAttackWindow(reactive)) return false;
    const Vector3 desired = escape ? Game::CursorPos() : PredictPosition(target, 0.16f);
    const Vector3 endpoint = SpinEndpoint(player.Position(), desired);
    if (!endpoint.IsValid() || endpoint.IsZero()) return false;
    const bool lethal = !escape && Engine::ValidEnemy(target) &&
        ShouldPostRKill(target.Health(), target.AllShield(),
                        player.GetAutoAttackDamage(target, true),
                        Engine::RuntimeSpells[2]->GetDamage(target),
                        player.Position().Distance2D(target.Position()) <=
                            kSpinRange + target.BoundingRadius());
    const bool unsafeLanding = Engine::UnderEnemyTurret(endpoint) && !lethal && !escape;
    const int nearby = Engine::CountEnemiesAt(endpoint, 500.0f);
    if (!SpinDestinationSafe(player.Position(), endpoint,
                             SDK::NavMesh::IsWall(endpoint), unsafeLanding,
                             nearby, Slider(TacticsMenu, "MaxSpinEnemies", 2), lethal || escape)) {
        return false;
    }
    if (!escape && Engine::ValidEnemy(target)) {
        auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
        if (!prediction.GetCastPosition().IsValid() ||
            prediction.GetCastPosition().IsZero() ||
            !SpinLineHits(player.Position(), endpoint, prediction.GetCastPosition(),
                          target.BoundingRadius())) return false;
    }
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    LastSpellCastTick[2] = LastCastTick = Now();
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || RActive || !Ready(3, mode, reactive)) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), 650.0f);
    const bool hardThreat = IncomingThreatUntil >= Now();
    const float projectedDamage = hardThreat
        ? 46.0f : (enemies >= 2 ? 31.0f : 18.0f);
    const bool urgent = UndyingRageCastWindow(player.HealthPercent(),
                                              projectedDamage,
                                              Orbwalker::CanAttack() ||
                                                  Orbwalker::IsWindingUp(),
                                              enemies,
                                              Slider(TacticsMenu, "MaxREnemies", 2));
    if (!reactive && !urgent && player.HealthPercent() >
            static_cast<float>(Slider(TacticsMenu, "RHealth", 34))) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    RActive = true;
    LastSpellCastTick[3] = LastRCastTick = LastCastTick = Now();
    RExpireTick = LastRCastTick + kUndyingRageDurationMs;
    return true;
}

inline bool TryPostR(const AIHeroClient& target, Mode mode) {
    if (!RActive) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int remaining = RExpireTick - Now();
    const bool kill = Engine::ValidEnemy(target) &&
        ShouldPostRKill(target.Health(), target.AllShield(),
                        player.GetAutoAttackDamage(target, true),
                        Engine::RuntimeSpells[2] ? Engine::RuntimeSpells[2]->GetDamage(target) : 0.0f,
                        player.Position().Distance2D(target.Position()) <=
                            kAutoAttackReach + target.BoundingRadius());
    if (ShouldPostREscape(remaining, Slider(TacticsMenu, "RExitBuffer", 720),
                          player.HealthPercent(),
                          Engine::CountEnemiesAt(player.Position(), 650.0f), kill)) {
        return CastE(target, Mode::Flee, true, true);
    }
    if (kill && Engine::ValidEnemy(target) && Ready(2, mode, true)) {
        return CastE(target, mode, true, false);
    }
    if (!kill && player.HealthPercent() <= 48.0f) return CastQ(mode, true);
    return false;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, 920.0f)) return;
    if (TryPostR(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastR(target, Mode::Combo)) return;
    (void)CastQ(Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, 920.0f)) return;
    if (CastW(target, Mode::Harass)) return;
    if (CastE(target, Mode::Harass)) return;
    (void)CastQ(Mode::Harass);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.HealthPercent() <= Slider(TacticsMenu, "FarmQHealth", 62) &&
        CastQ(mode)) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (Engine::ValidEnemy(target, kShoutRadius) && CastW(target, Mode::Flee, true)) return;
    if (CastR(target, Mode::Flee, true)) return;
    (void)CastE(target, Mode::Flee, true, true);
}

inline void Automatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (IncomingThreatUntil >= Now()) {
        const auto threat = HeroByNetworkId(IncomingThreatTargetId);
        if (Engine::ValidEnemy(threat, kShoutRadius) && CastW(threat, Mode::Automatic, true)) return;
        if (CastR(threat, Mode::Automatic, true)) return;
    }
    if (TryPostR(target, Mode::Automatic)) return;
    if (Engine::ValidEnemy(target, 920.0f) && player.HealthPercent() <= 38.0f) {
        if (CastR(target, Mode::Automatic, true)) return;
        (void)CastQ(Mode::Automatic, true);
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (player.IsValid()) Fury = std::clamp(static_cast<int>(CurrentResource(100.0f)), 0, 100);
    if (RActive && (RExpireTick <= now ||
        (player.IsValid() && !player.HasBuff("UndyingRage") &&
         now - LastRCastTick > 260))) RActive = false;
    if (IncomingThreatUntil < now) {
        IncomingThreatUntil = 0;
        IncomingThreatTargetId = 0;
        IncomingThreatEndpoint = {};
    }
    const AIHeroClient target = Engine::SelectTarget(920.0f);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
    case Mode::Flee: Flee(target); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("Tryndamere tactics"));
    TacticsMenu->Add(new MenuSlider("QHealth", "Q heal below health percent", 54, 5, 95));
    TacticsMenu->Add(new MenuSlider("WHealth", "W defensive health percent", 58, 5, 95));
    TacticsMenu->Add(new MenuSlider("RHealth", "R emergency health percent", 34, 5, 80));
    TacticsMenu->Add(new MenuSlider("FarmQHealth", "Q farm sustain health percent", 62, 5, 95));
    TacticsMenu->Add(new MenuSlider("MaxSpinEnemies", "Maximum enemies at E landing", 2, 1, 5));
    TacticsMenu->Add(new MenuSlider("MaxREnemies", "Maximum enemies for R commit", 2, 1, 5));
    TacticsMenu->Add(new MenuSlider("RExitBuffer", "Escape before R expiry (ms)", 720, 200, 1800));
}

inline void OnLoad() {
    Fury = 0;
    LastAttackTargetId = LastAttackTick = LastCastTick = 0;
    LastSpellCastTick.fill(0);
    RActive = false;
    RExpireTick = LastRCastTick = 0;
    IncomingThreatUntil = IncomingThreatTargetId = 0;
    IncomingThreatEndpoint = {};
}

inline void OnUnload() {
    TacticsMenu = nullptr;
    OnLoad();
}

inline void OnDraw() {}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        if (args.IsAutoAttack) {
            LastAttackTargetId = static_cast<int>(args.TargetNetworkId);
            LastAttackTick = Now();
        }
        if (args.Slot >= 0 && args.Slot < 4 && Engine::WasControllerCast(args.Slot)) {
            LastSpellCastTick[static_cast<std::size_t>(args.Slot)] = Now();
            if (args.Slot == 3) {
                RActive = true;
                LastRCastTick = Now();
                RExpireTick = LastRCastTick + kUndyingRageDurationMs;
            }
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && analysis.Enemy.IsValid()) {
        IncomingThreatTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick,
                                       analysis.LineThreatUntilTick);
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAttackTargetId = static_cast<int>(args.TargetNetworkId);
        LastAttackTick = Now();
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "undyingrage")) {
        RActive = true;
        RExpireTick = Now() + kUndyingRageDurationMs;
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) &&
        Engine::TextContains(args.BuffName, "undyingrage")) RActive = false;
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) {
        LastAttackTargetId = static_cast<int>(args.Target.NetworkId());
        LastAttackTick = Now();
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) {
        LastAttackTargetId = static_cast<int>(args.Target.NetworkId());
        LastAttackTick = Now();
    }
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, IncomingThreatTargetId, IncomingThreatEndpoint,
                           IncomingThreatUntil, 700.0f, 1200);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, IncomingThreatTargetId, IncomingThreatUntil, 900, 250, 5000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}

inline constexpr const char* Scenarios[] = {
    "Fury polling and Bloodlust missing-health heal threshold",
    "Mocking Shout radius, facing-away slow and attack-damage debuff",
    "Spinning Slash prediction, line collision and projectile-wall rejection",
    "Spinning Slash real 650-unit reach with terrain and enemy-density safety",
    "Undying Rage five-second lethal-threat timing and cast-event reconciliation",
    "Post-Rage kill check before a safe escape before expiry",
    "AA windup preservation and E reset after a confirmed attack",
    "Turret dive only when the Engine target is killable",
    "Lane clear, jungle clear and last-hit sustain without spending R",
    "Flee W peel, R survival and safe cursor spin",
    "Gapcloser, interruptable spell and polling threat reconciliation",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Tryndamere;
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.ControllerId = "champion.kuroaio.ai.tryndamere.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AITryndamere.md";
    controller.ImplementationSummary =
        "Fury-aware Q sustain, W facing/debuff timing, collision-safe E spin, and five-second R kill-or-escape management.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
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

} // namespace Plugins::KuroAIO::AI::Controllers::Tryndamere
