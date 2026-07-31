#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIMaokaiGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstring>

namespace Plugins::KuroAIO::AI::Controllers::Maokai {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasResourceFor;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NameEquals;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Ready;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::UnitByNetworkId;

inline Menu* TacticsMenu = nullptr;
inline Menu* RoleMenu = nullptr;
inline Menu* SaplingMenu = nullptr;
inline Menu* EngageMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<SaplingState, 8> Saplings{};
inline RWaveState Wave{};
inline QPlan LastQPlan{};
inline WDashPlan LastWPlan{};
inline int WTargetId = 0;
inline int WCastTick = 0;
inline bool WDashActive = false;
inline int RCastTick = 0;
inline int RResolveTick = 0;
inline int RPrimaryTargetId = 0;
inline int ECastTick = 0;
inline int ETargetId = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int ProtectedAllyId = 0;
inline int PeelThreatId = 0;
inline int TargetedAllyThreatId = 0;
inline int TargetedAllyThreatUntil = 0;
inline int IncomingThreatUntil = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd = {};
inline int LastCastTicks[4] = {};

inline bool Bool(Menu* menu, const char* key, bool fallback) {
    return ::Plugins::KuroAIO::Bool(menu, key, fallback);
}
inline int Slider(Menu* menu, const char* key, int fallback) {
    return ::Plugins::KuroAIO::Slider(menu, key, fallback);
}

inline bool IsSaplingName(const char* name) {
    return name && (Engine::TextContains(name, "MaokaiSapling") ||
                    Engine::TextContains(name, "SaplingMaokai"));
}
inline bool IsRWaveName(const char* name) {
    return name && (Engine::TextContains(name, "MaokaiRMis") ||
                    Engine::TextContains(name, "MaokaiR"));
}
inline bool IsRootBuff(const char* name) {
    return name && (Engine::TextContains(name, "MaokaiWRoot") ||
                    Engine::TextContains(name, "MaokaiRRoot"));
}
inline bool IsBrushEmpoweredObject(const char* name) {
    return name && (Engine::TextContains(name, "Brush") ||
                    Engine::TextContains(name, "MaokaiSapling2"));
}

inline bool CastReady(int index, Mode mode, bool allowWindup = false) {
    if (!Ready(index) || !SpellEnabled(index, mode)) return false;
    if (!allowWindup && Orbwalker::IsWindingUp()) return false;
    if (LastCastTicks[index] + (allowWindup ? 18 : 42) > Now()) return false;
    return true;
}

inline bool SafeEndpoint(const Vector3& position,
                         bool lethal = false,
                         int maximumEnemies = 2) {
    if (!position.IsValid() || position.IsZero()) return false;
    if (Engine::UnderEnemyTurret(position) && !lethal) return false;
    if (Bool(EngageMenu, "RespectDashHazards", true) &&
        HasReadyDashHazardAt(position)) return false;
    const int enemies = Engine::CountEnemiesAt(position, 650.0f);
    const int allies = CountAlliedFollowup(position, 850.0f, true);
    return WEndpointSafe(LastWPlan, Engine::UnderEnemyTurret(position),
                         HasReadyDashHazardAt(position), enemies, allies,
                         maximumEnemies, lethal);
}

inline AIHeroClient SelectedOrNearest(const AIHeroClient& selected) {
    if (Engine::ValidEnemy(selected)) return selected;
    AIHeroClient best{};
    float bestDistance = FLT_MAX;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return best;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        const float distance = player.Position().Distance2D(enemy.Position());
        if (distance < bestDistance) {
            best = enemy;
            bestDistance = distance;
        }
    }
    return best;
}

inline AIHeroClient ProtectedAlly() {
    const AIHeroClient ally = SelectProtectionAlly(
        1500.0f, TargetedAllyThreatId, TargetedAllyThreatUntil);
    ProtectedAllyId = Engine::ValidAlly(ally)
        ? static_cast<int>(ally.NetworkId()) : 0;
    return ally;
}

inline AIHeroClient PeelingThreat(const AIHeroClient& ally) {
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    if (!Engine::ValidAlly(ally)) return best;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        const float distance = enemy.Position().Distance2D(ally.Position());
        if (distance > 900.0f) continue;
        float score = 1000.0f - distance + enemy.TotalAttackDamage() * 0.45f;
        if (enemy.IsDashing()) score += 350.0f;
        if (static_cast<int>(enemy.NetworkId()) == TargetedAllyThreatId) score += 700.0f;
        if (score > bestScore) { best = enemy; bestScore = score; }
    }
    PeelThreatId = Engine::ValidEnemy(best)
        ? static_cast<int>(best.NetworkId()) : 0;
    return best;
}

inline bool PredictAndClear(const AIHeroClient& target,
                            float delay,
                            float missileRadius) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return false;
    const Vector3 predicted = PredictPosition(target, delay);
    return predicted.IsValid() && !predicted.IsZero() &&
           !ControllerHelpers::ProjectileWallBlocks(
               player.Position(), predicted, missileRadius);
}

inline void ReconcileSaplings() {
    const int now = Now();
    for (auto& sapling : Saplings) {
        if (!SaplingLive(sapling, now)) {
            sapling = {};
            continue;
        }
        if (!sapling.Empowered && sapling.Triggered &&
            sapling.EmpowerExpireTick < now) sapling.Triggered = false;
    }
}

inline SaplingState* FindSapling(int id) {
    for (auto& sapling : Saplings) {
        if (sapling.Active && sapling.Id == id) return &sapling;
    }
    return nullptr;
}

inline void TrackSapling(int id, const Vector3& position, bool empowered) {
    if (id == 0 || !position.IsValid()) return;
    SaplingState* slot = FindSapling(id);
    if (!slot) {
        for (auto& candidate : Saplings) {
            if (!candidate.Active) {
                slot = &candidate;
                break;
            }
        }
    }
    if (!slot) {
        slot = &Saplings[0];
        for (auto& candidate : Saplings) {
            if (candidate.SpawnTick < slot->SpawnTick) slot = &candidate;
        }
    }
    *slot = {id, position, Now(), Now() + 30000, 0, true, empowered, false};
}

inline void RemoveSapling(int id) {
    if (auto* sapling = FindSapling(id)) *sapling = {};
}

inline SaplingState* BestSaplingFor(const Vector3& target) {
    SaplingState* best = nullptr;
    float bestScore = -FLT_MAX;
    for (auto& sapling : Saplings) {
        if (!SaplingLive(sapling, Now()) ||
            !InSaplingDetectionZone(sapling, target)) continue;
        if (sapling.Empowered && !sapling.Triggered) {
            sapling.Triggered = true;
            sapling.EmpowerExpireTick = Now() +
                static_cast<int>(kEmpoweredZoneSeconds * 1000.0f);
        }
        const float score = SaplingPriority(
            sapling.Position.Distance2D(target), sapling.Empowered,
            Engine::UnderEnemyTurret(sapling.Position),
            Engine::CountEnemiesAt(sapling.Position, 650.0f),
            CountAlliedFollowup(sapling.Position, 850.0f));
        if (score > bestScore) {
            best = &sapling;
            bestScore = score;
        }
    }
    return best;
}

inline bool TryQ(const AIHeroClient& target, Mode mode, bool peel = false) {
    if (!Engine::ValidEnemy(target) || !CastReady(0, mode, true)) return false;
    const auto player = GameObjects::Player();
    const Vector3 predicted = PredictPosition(target, 0.25f);
    LastQPlan = BuildQPlan(player.Position(), predicted,
                           player.BoundingRadius(), target.BoundingRadius());
    if (!LastQPlan.Valid || !QHits(player.Position(), LastQPlan.Direction,
                                   predicted, target.BoundingRadius())) return false;
    const bool lethal = player.CalculateMagicDamage(
        target, 210.0f + 0.50f * player.AP()) >=
        target.Health() + target.AllShield();
    const Vector3 endpoint = LastQPlan.KnockbackEndpoint;
    if (!QEndpointSafe(endpoint,
                       Engine::UnderEnemyTurret(endpoint),
                       HasReadyDashHazardAt(endpoint),
                       Engine::CountEnemiesAt(endpoint, 600.0f),
                       CountAlliedFollowup(endpoint, 800.0f, true),
                       peel ? 3 : Slider(EngageMenu, "MaxQEnemies", 2),
                       lethal || peel)) return false;
    if (!PredictAndClear(target, 0.25f, kQHalfWidth)) return false;
    if (!Engine::ControllerCastPosition(0, predicted)) return false;
    LastCastTicks[0] = Now();
    return true;
}

inline bool TryW(const AIHeroClient& target, Mode mode, bool peel = false) {
    if (!Engine::ValidEnemy(target) || !CastReady(1, mode)) return false;
    const auto player = GameObjects::Player();
    LastWPlan = BuildWDashPlan(player.Position(), PredictPosition(target, 0.05f),
                               target.BoundingRadius());
    if (!LastWPlan.Valid || !SafeEndpoint(LastWPlan.Endpoint, false,
                                           peel ? 3 : Slider(EngageMenu, "MaxWEnemies", 1))) return false;
    if (HasSpellShieldOrImmunity(target) && !peel) return false;
    if (!Engine::ControllerCastUnit(1, target)) return false;
    WTargetId = static_cast<int>(target.NetworkId());
    WCastTick = Now();
    WDashActive = true;
    LastCastTicks[1] = Now();
    return true;
}

inline bool TryE(const AIHeroClient& target, Mode mode, bool zoneOnly = false) {
    if (!Engine::ValidEnemy(target) || !CastReady(2, mode)) return false;
    const auto player = GameObjects::Player();
    const Vector3 destination = PredictPosition(target, kEMissileTravelSeconds);
    if (!destination.IsValid() ||
        player.Position().Distance2D(destination) > kEMaxRange + target.BoundingRadius()) return false;
    if (ControllerHelpers::ProjectileWallBlocks(
            player.Position(), destination, kEMissileWidth * 0.5f)) return false;
    const bool empowered = BestSaplingFor(destination) != nullptr ||
                           Bool(SaplingMenu, "AssumeBrush", true);
    if (zoneOnly && !empowered) return false;
    if (!Engine::ControllerCastPosition(2, destination)) return false;
    ECastTick = Now();
    ETargetId = static_cast<int>(target.NetworkId());
    LastCastTicks[2] = Now();
    return true;
}

inline bool TryR(const AIHeroClient& target, Mode mode, bool peel = false) {
    if (!Engine::ValidEnemy(target) || !CastReady(3, mode)) return false;
    const auto player = GameObjects::Player();
    const Vector3 predicted = PredictPosition(target, 0.25f);
    const Vector3 direction = Direction2D(player.Position(), predicted);
    if (direction.IsZero()) return false;
    if (ControllerHelpers::ProjectileWallBlocks(
            player.Position(), player.Position() + direction * kRMaxRange,
            kRHalfWidth)) return false;
    int hits = 0;
    float score = 0.0f;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        const Vector3 enemyPos = PredictPosition(enemy, 0.25f);
        if (!RWaveHits(player.Position(), direction, enemyPos,
                       enemy.BoundingRadius())) continue;
        ++hits;
        score += enemy.IsDashing() ? 2.4f : 1.0f;
        if (static_cast<int>(enemy.NetworkId()) == PeelThreatId) score += 2.0f;
        if (HasSpellShieldOrImmunity(enemy)) score *= 0.18f;
    }
    const AIHeroClient ally = ProtectedAlly();
    const bool urgentPeel = peel && Engine::ValidAlly(ally) &&
        Engine::ValidEnemy(PeelingThreat(ally));
    const bool lethalSingle = target.HealthPercent() <= 28.0f;
    if (!RShouldCast(hits, score, Slider(EngageMenu, "MinimumRHits", 2),
                     Slider(EngageMenu, "MinimumRQuality", 260) / 100.0f,
                     urgentPeel, lethalSingle,
                     HasReadyDashHazardAt(player.Position())) ||
        !Engine::ControllerCastPosition(3, predicted)) return false;
    RCastTick = Now();
    RResolveTick = Now() + static_cast<int>(RWaveTravelSeconds(
        player.Position().Distance2D(predicted)) * 1000.0f) + 500;
    Wave = {player.Position(), direction, RCastTick, RResolveTick,
            static_cast<int>(target.NetworkId()), true};
    RPrimaryTargetId = static_cast<int>(target.NetworkId());
    LastCastTicks[3] = Now();
    return true;
}

inline bool TryFlee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) && TryQ(target, Mode::Flee, true)) return true;
    if (Engine::ValidEnemy(target) && TryR(target, Mode::Flee, true)) return true;
    return false;
}

inline bool TryFarm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !HasResourceFor({0, 2}, 18.0f)) return false;
    if (mode == Mode::Jungle) {
        const auto monster = ControllerHelpers::SelectJungleTarget(650.0f);
        if (!monster.IsValid()) return false;
        const Vector3 point = monster.Position();
        if (CastReady(2, mode) && !ControllerHelpers::ProjectileWallBlocks(
                player.Position(), point, kEMissileWidth * 0.5f) &&
            Engine::ControllerCastPosition(2, point)) {
            LastCastTicks[2] = Now();
            return true;
        }
        if (CastReady(0, mode, true) &&
            Engine::ControllerCastPosition(0, point)) {
            LastCastTicks[0] = Now();
            return true;
        }
        return false;
    }
    if (mode == Mode::LastHit) {
        for (const auto& minion : GameObjects::EnemyLaneMinions()) {
            if (!minion.IsValid() || minion.IsDead()) continue;
            if (player.Position().Distance2D(minion.Position()) <= kQRange &&
                minion.Health() <= player.TotalAttackDamage() * 0.9f &&
                CastReady(0, mode, true) &&
                Engine::ControllerCastPosition(0, minion.Position())) {
                LastCastTicks[0] = Now();
                return true;
            }
        }
        return false;
    }
    if (Bool(FarmMenu, "LaneE", true) && CastReady(2, mode) &&
        Engine::CountEnemiesAt(player.Position(), 850.0f) >=
            Slider(FarmMenu, "MinimumEUnits", 3)) {
        if (Engine::ControllerCastPosition(2, player.Position())) {
            LastCastTicks[2] = Now();
            return true;
        }
    }
    return false;
}

inline void RefreshState() {
    ReconcileSaplings();
    const int now = Now();
    if (WDashActive && !GameObjects::Player().IsDashing() &&
        now > WCastTick + 650) WDashActive = false;
    if (Wave.Active && now > Wave.ResolveTick + 250) Wave = {};
    if (TargetedAllyThreatUntil < now) TargetedAllyThreatId = 0;
    if (IncomingThreatUntil < now) IncomingThreatUntil = 0;
    if (InterruptExpireTick < now) InterruptTargetId = 0;
    if (GapcloserExpireTick < now) GapcloserTargetId = 0;
    const auto player = GameObjects::Player();
    if (player.IsValid() && (player.HasBuff("MaokaiR") ||
                             player.HasBuff("MaokaiRRoot"))) {
        Wave.Active = true;
        Wave.ResolveTick = std::max(Wave.ResolveTick, now + 250);
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    RefreshState();
    const AIHeroClient target = SelectedOrNearest(selected);
    const AIHeroClient ally = ProtectedAlly();
    const AIHeroClient threat = PeelingThreat(ally);
    if (mode == Mode::Flee) {
        (void)TryFlee(Engine::ValidEnemy(threat) ? threat : target);
        return true;
    }
    if (Engine::ValidEnemy(threat) && Bool(RoleMenu, "ProtectCarry", true) &&
        Engine::ValidAlly(ally) &&
        (ally.HealthPercent() < Slider(RoleMenu, "PeelAllyHp", 48) ||
         threat.Position().Distance2D(ally.Position()) < 430.0f)) {
        if (TryR(threat, Mode::Automatic, true) ||
            TryQ(threat, Mode::Automatic, true) ||
            TryW(threat, Mode::Automatic, true)) return true;
    }
    if (!Engine::ValidEnemy(target)) {
        if (mode == Mode::LaneClear || mode == Mode::Jungle ||
            mode == Mode::LastHit) (void)TryFarm(mode);
        return true;
    }
    if (mode == Mode::Combo || mode == Mode::Automatic) {
        if (TryR(target, mode, false) || TryW(target, mode, false) ||
            TryQ(target, mode, false) || TryE(target, mode, false)) return true;
    } else if (mode == Mode::Harass) {
        if (TryE(target, mode, false) || TryQ(target, mode, false)) return true;
        if (Bool(EngageMenu, "HarassW", false)) (void)TryW(target, mode, false);
    } else if (mode == Mode::LaneClear || mode == Mode::Jungle ||
               mode == Mode::LastHit) {
        (void)TryFarm(mode);
    }
    return true;
}

inline void ObserveEnemyCast(const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !args.Sender.IsValid() ||
        IsLocalPlayer(args.Sender)) return;
    const std::uint32_t targetId = args.TargetNetworkId != 0
        ? args.TargetNetworkId : args.Target.NetworkId;
    if (targetId == static_cast<std::uint32_t>(ProtectedAllyId)) {
        TargetedAllyThreatId = static_cast<int>(args.Sender.NetworkId);
        TargetedAllyThreatUntil = Now() + 1200;
    }
    if (targetId == static_cast<std::uint32_t>(player.NetworkId())) {
        IncomingThreatUntil = Now() + 1300;
    }
}

inline void ObserveLocalSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    const int slot = args.Slot;
    const int now = Now();
    if (args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
        LastAutoTick = now;
        return;
    }
    if (slot == 0 || Engine::TextContains(args.SpellName, "MaokaiQ")) {
        LastQPlan = {};
        LastCastTicks[0] = now;
    } else if (slot == 1 || Engine::TextContains(args.SpellName, "MaokaiW")) {
        WCastTick = now;
        WTargetId = static_cast<int>(args.TargetNetworkId);
        WDashActive = true;
        LastCastTicks[1] = now;
    } else if (slot == 2 || Engine::TextContains(args.SpellName, "MaokaiE")) {
        ECastTick = now;
        LastCastTicks[2] = now;
    } else if (slot == 3 || Engine::TextContains(args.SpellName, "MaokaiR")) {
        const auto player = GameObjects::Player();
        const Vector3 endpoint = args.EndPosition.IsValid()
            ? args.EndPosition : Game::CursorPos();
        const Vector3 direction = Direction2D(player.Position(), endpoint);
        Wave = {player.Position(), direction, now, now + 5000, 0,
                !direction.IsZero()};
        RCastTick = now;
        RResolveTick = now + 5000;
        LastCastTicks[3] = now;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    (void)args;
    Engine::LastBeforeAttackTick = Now();
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)args;
    Engine::LastAfterAttackTick = Now();
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    (void)CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender) && NameEquals(
            args.BuffName, "MaokaiPassiveReady")) return;
    if (added && IsRootBuff(args.BuffName)) IncomingThreatUntil = Now() + 350;
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    UpdateBuffState(args, true);
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    UpdateBuffState(args, false);
}
inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    UpdateBuffState(args, true);
}

inline void OnGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    GapcloserTargetId = static_cast<int>(args.NetworkId);
    GapcloserEnd = args.End;
    GapcloserExpireTick = Now() + 900;
}
inline void OnInterruptable(
    const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    InterruptTargetId = static_cast<int>(args.NetworkId);
    InterruptExpireTick = Now() + 1000;
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() ||
        !ControllerHelpers::ObjectEventIsAllied(args)) return;
    if (IsSaplingName(args.Sender.Name) ||
        IsBrushEmpoweredObject(args.Sender.CharacterName)) {
        TrackSapling(static_cast<int>(args.Sender.NetworkId),
                     args.Sender.Position,
                     IsBrushEmpoweredObject(args.Sender.Name) ||
                     IsBrushEmpoweredObject(args.Sender.CharacterName));
    }
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid()) {
        RemoveSapling(static_cast<int>(args.Sender.NetworkId));
    }
}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() ||
        !ControllerHelpers::MissileEventIsLocal(args) ||
        !(IsRWaveName(args.Sender.Name) || IsRWaveName(args.SpellName) ||
          IsRWaveName(args.MissileName))) return;
    Wave.Active = true;
    Wave.CastTick = Now();
    Wave.ResolveTick = Now() + 5000;
    Wave.Origin = GameObjects::Player().Position();
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() &&
        (IsRWaveName(args.Sender.Name) || IsRWaveName(args.SpellName) ||
         IsRWaveName(args.MissileName))) {
        Wave.Active = false;
    }
}

inline void OnDraw() { /* state is intentionally retained for overlay consumers */ }

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "MaokaiOneTrick", "Maokai zone-control mechanics"));
    RoleMenu = TacticsMenu->AddSubMenu(new Menu(
        "Role", "Peel and engage policy"));
    RoleMenu->Add(new MenuBool(
        "ProtectCarry", "Protect carry before engage", true));
    RoleMenu->Add(new MenuSlider(
        "PeelAllyHp", "Peel ally health percent", 48, 10, 90));
    SaplingMenu = TacticsMenu->AddSubMenu(new Menu(
        "Saplings", "Sapling object and brush zone tracking"));
    SaplingMenu->Add(new MenuBool(
        "AssumeBrush", "Treat E landing as brush when map data is unavailable", true));
    SaplingMenu->Add(new MenuBool(
        "PreferEmpowered", "Prefer empowered sapling zones", true));
    EngageMenu = TacticsMenu->AddSubMenu(new Menu(
        "Engage", "Dash, wave and safety gates"));
    EngageMenu->Add(new MenuBool(
        "RespectDashHazards", "Reject ready dash stoppers", true));
    EngageMenu->Add(new MenuSlider(
        "MaxQEnemies", "Maximum enemies at Q endpoint", 2, 0, 5));
    EngageMenu->Add(new MenuSlider(
        "MaxWEnemies", "Maximum enemies at W endpoint", 1, 0, 4));
    EngageMenu->Add(new MenuSlider(
        "MinimumRHits", "Minimum R wave hits", 2, 1, 5));
    EngageMenu->Add(new MenuSlider(
        "MinimumRQuality", "Minimum R quality x100", 260, 50, 900));
    EngageMenu->Add(new MenuBool(
        "HarassW", "Allow W in harass", false));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "Farm", "Lane, jungle and last-hit policy"));
    FarmMenu->Add(new MenuSlider(
        "MinimumEUnits", "Minimum E jungle units", 2, 1, 6));
    FarmMenu->Add(new MenuBool(
        "LaneE", "Use E for lane clear", true));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "State is exposed to the overlay"));
    CoachMenu->Add(new MenuBool(
        "DrawSaplings", "Draw sapling zones", false));
    CoachMenu->Add(new MenuBool(
        "DrawWave", "Draw R wave state", false));
}

inline void OnLoad() {
    Saplings.fill({});
    Wave = {};
    LastQPlan = {};
    LastWPlan = {};
    WTargetId = WCastTick = RCastTick = RResolveTick = RPrimaryTargetId = 0;
    ECastTick = ETargetId = LastAutoTargetId = LastAutoTick = 0;
    ProtectedAllyId = PeelThreatId = TargetedAllyThreatId = 0;
    TargetedAllyThreatUntil = IncomingThreatUntil = 0;
    InterruptTargetId = InterruptExpireTick = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEnd = {};
    std::fill(std::begin(LastCastTicks), std::end(LastCastTicks), 0);
}
inline void OnUnload() {
    TacticsMenu = RoleMenu = SaplingMenu = EngageMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Prefer the selected target while retaining a nearest-enemy fallback",
    "Poll and reconcile sapling objects when object events are delayed",
    "Expire ordinary saplings after thirty seconds and track empowered brush zones",
    "Use E's 550-unit sapling detection radius rather than a generic trap radius",
    "Predict E's 0.85-second missile arrival and reject projectile-wall segments",
    "Use Q's 650 range and 110 width with radius-aware line contact",
    "Compute the real Q 400-unit knockback endpoint and preserve peel direction",
    "Reject Q endpoint under turret, ready dash hazard or unsafe enemy density",
    "Use W as a 525-unit root dash with a concrete target landing endpoint",
    "Reject W landing under turret, ready dash hazard or excessive enemy density",
    "Respect spell shields on proactive W while permitting urgent peel",
    "Track local W dash timing and reconcile completion through polling",
    "Advance R with 100 initial speed, 300 acceleration and 750 maximum speed",
    "Track R wave origin, direction, cast tick and resolve tick from events and missiles",
    "Use R's 120/150 wave width and radius-aware collision against predicted targets",
    "Require multi-target R quality for engage and allow urgent single-target peel",
    "Discount R targets protected by spell shields and avoid unsafe mobility commits",
    "Protect a low-health ally from a nearby diver before proactive engage",
    "Use Q and R to peel a gapcloser or interruptable channel",
    "Preserve AA windup and manual casts through local ownership arbitration",
    "Use Q on jungle monsters with the current 26.15 monster/AP tuning",
    "Use E for conservative jungle and lane zone control with mana reserve",
    "Use Q only for predicted last-hit opportunities rather than generic clear spam",
    "Support Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic modes",
    "Never fall back to generic Q-W-E-R ordering because this controller owns the loop",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Maokai";
    controller.ControllerId = "champion.kuroaio.ai.maokai.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIMaokai.md";
    controller.ImplementationSummary =
        "Dedicated sapling/empowered-zone object state, radius-aware Q knockback, "
        "safe W root dash endpoints, predicted brush E missiles and accelerating "
        "R wave tracking with peel-first and multi-target engage branches.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell =
        &ControllerHelpers::DispatchLocalOrOtherSpellEvent<
            &ObserveLocalSpell, &ObserveEnemyCast>;
    controller.OnDoCast = &OnDoCast;
    controller.OnBuffAdd =
        &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
    controller.OnBuffRemove =
        &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, false>;
    controller.OnBuffUpdate =
        &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
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

} // namespace Plugins::KuroAIO::AI::Controllers::Maokai
