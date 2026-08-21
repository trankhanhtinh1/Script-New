#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AIZed.h"
#include "AIZedGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Zed {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MissileEventIsLocal;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Ready;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellRank;
using ControllerHelpers::SpellEventNameContains;

inline Menu* TacticsMenu = nullptr;
inline Menu* ShadowMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline ShadowPairState Shadows{};
inline ShadowMark LastMark{};
inline DeathMark Death{};
inline std::array<int, 4> LastCastTick{};
inline int QCastTick = 0;
inline int QTargetId = 0;
inline int QMissileId = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserUntil = 0;
inline Vector3 GapcloserEndpoint{};
inline int InterruptTargetId = 0;
inline int InterruptUntil = 0;

inline constexpr int kWShadowMs = 5000;
inline constexpr int kRShadowMs = 4000;
inline constexpr int kMarkMs = 3000;
inline constexpr int kRWindowMs = 3000;

inline bool ReadyFor(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Ready(slot, mode) &&
           CastThrottleReady(slot, 55, reactive ? 0 : -1);
}

inline bool HasEnergyFor(std::initializer_list<int> slots,
                         float reserve = kEnergyReserve) {
    float cost = std::max(0.0f, reserve);
    for (const int slot : slots) {
        if (slot >= 0 && slot < 4 && Ready(slot)) cost += SpellCost(slot);
    }
    return CurrentResource() + 0.5f >= cost;
}

inline bool IsTargetProtected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsDead() ||
           IsCommonUntargetableOrImmune(target);
}

inline bool IsUnsafe(const Vector3& position, bool lethal, bool emergency) {
    if (!position.IsValid() || position.IsZero() || SDK::NavMesh::IsWall(position)) return true;
    if (lethal || emergency) return false;
    if (Engine::UnderEnemyTurret(position)) return true;
    return Engine::CountEnemiesAt(position, 625.0f) >
        Slider(TacticsMenu, "MaxCommitEnemies", 2);
}

inline bool CanAct(bool reactive, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead() ||
        ControllerHelpers::PlayerMobilityLocked()) return false;
    return !PreserveAttack(reactive, lethal);
}

inline float DamageFor(const AIHeroClient& target, int slot) {
    if (!target.IsValid() || !Engine::ValidEnemy(target) || slot < 0 || slot > 3) return 0.0f;
    if (!Engine::RuntimeSpells[slot]) return 0.0f;
    return std::max(0.0f, Engine::RuntimeSpells[slot]->GetDamage(target));
}

inline float ExpectedComboDamage(const AIHeroClient& target, bool includeR) {
    float damage = DamageFor(target, 0) * ShurikenDamageMultiplier(true);
    if (ShadowAlive(Shadows.W, Now()) &&
        Shadows.W.Position.Distance2D(target.Position()) <= kERadius)
        damage += DamageFor(target, 2);
    if (includeR) damage += DamageFor(target, 3);
    if (Death.Active) damage += DeathMarkDamage(Death.StoredDamage, std::max(1, SpellRank(3)));
    return damage;
}

inline std::vector<CollisionBody> QBodies(const AIHeroClient& target) {
    std::vector<CollisionBody> bodies;
    bodies.reserve(GameObjects::EnemyHeroes().size() + GameObjects::EnemyMinions().size() + 1);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kQRange + 100.0f)) continue;
        bodies.push_back({enemy.Position(), enemy.BoundingRadius(),
            static_cast<int>(enemy.NetworkId()),
            static_cast<int>(enemy.NetworkId()) == static_cast<int>(target.NetworkId()), true});
    }
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable()) continue;
        bodies.push_back({minion.Position(), minion.BoundingRadius(),
            static_cast<int>(minion.NetworkId()), false, true});
    }
    if (std::none_of(bodies.begin(), bodies.end(), [&](const CollisionBody& body) {
            return body.NetworkId == static_cast<int>(target.NetworkId()); })) {
        bodies.push_back({target.Position(), target.BoundingRadius(),
            static_cast<int>(target.NetworkId()), true, true});
    }
    return bodies;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || IsTargetProtected(target) ||
        !Engine::ValidEnemy(target, kQRange + target.BoundingRadius()) ||
        !ReadyFor(0, mode, reactive) || !CanAct(reactive, lethal) ||
        !HasEnergyFor({0}, static_cast<float>(Slider(TacticsMenu, "EnergyReserve", 20)))) return false;
    const Vector3 aim = Engine::RuntimeSpells[0]
        ? Engine::RuntimeSpells[0]->GetPrediction(target).GetCastPosition()
        : PredictPosition(target, 0.25f);
    if (!aim.IsValid() || aim.IsZero() ||
        ProjectileWallBlocksFromPlayer(aim, kQHalfWidth) ||
        !ShurikenHits(player.Position(), aim, PredictPosition(target, 0.25f),
                      target.BoundingRadius())) return false;
    const auto collision = EvaluateQFirstCollision(player.Position(), aim,
                                                    QBodies(target),
                                                    static_cast<int>(target.NetworkId()));
    if (!collision.TargetHit) return false;
    const float damage = DamageFor(target, 0) * collision.TargetDamageMultiplier;
    if (lethal && damage + 1.0f < target.Health() + target.AllShield()) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = QCastTick = Now();
    QTargetId = static_cast<int>(target.NetworkId());
    return true;
}

inline Vector3 WPointFor(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return {};
    const Vector3 aim = PredictPosition(target, 0.25f);
    const Vector3 direction = Direction2D(player.Position(), aim);
    if (direction.IsZero()) return {};
    const float distance = std::min(kWRange, player.Position().Distance2D(aim));
    return player.Position() + direction * std::max(120.0f, distance);
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kWRange + 80.0f) ||
        !ReadyFor(1, mode, reactive) || !CanAct(reactive) ||
        !HasEnergyFor({1, 2, 0}, static_cast<float>(Slider(TacticsMenu, "EnergyReserve", 20)))) return false;
    const Vector3 point = WPointFor(target);
    if (IsUnsafe(point, false, reactive) || SDK::NavMesh::IsWall(point)) return false;
    if (!Engine::ControllerCastPosition(1, point)) return false;
    Shadows.W = ReconcileShadow(Shadows.W, ShadowKind::W, 0, point, Now(), kWShadowMs, false);
    Shadows.W.Observed = false;
    LastCastTick[1] = Now();
    LastMark = ApplyShadowMark(LastMark, static_cast<int>(target.NetworkId()),
                               Shadows.W.NetworkId, Now(), kMarkMs, true, false);
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || IsTargetProtected(target) ||
        !ReadyFor(2, mode, reactive) || !CanAct(reactive) ||
        !HasEnergyFor({2}, static_cast<float>(Slider(TacticsMenu, "EnergyReserve", 20)))) return false;
    const bool localHit = player.Position().Distance2D(target.Position()) <=
        kERadius + target.BoundingRadius();
    const bool shadowHit = ShadowAlive(Shadows.W, Now()) &&
        Shadows.W.Position.Distance2D(target.Position()) <= kERadius + target.BoundingRadius();
    if (!localHit && !shadowHit) return false;
    const float damage = DamageFor(target, 2) * (shadowHit ? 1.0f : 0.75f);
    if (!reactive && mode == Mode::Harass && damage < target.Health() * 0.04f &&
        !MarkActive(LastMark, static_cast<int>(target.NetworkId()), Now())) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    LastCastTick[2] = Now();
    LastMark = ApplyShadowMark(LastMark, static_cast<int>(target.NetworkId()),
                               shadowHit ? Shadows.W.NetworkId : 0, Now(), kMarkMs,
                               shadowHit, true);
    if (Death.Active && Death.TargetId == static_cast<int>(target.NetworkId()))
        Death.StoredDamage += damage;
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || IsTargetProtected(target) || Death.Active ||
        !Engine::ValidEnemy(target, kRRange + target.BoundingRadius()) ||
        !ReadyFor(3, mode, reactive) || !CanAct(reactive) ||
        !HasEnergyFor({3, 2, 0}, static_cast<float>(Slider(RMenu, "EnergyReserve", 30)))) return false;
    const bool lethal = ExpectedComboDamage(target, true) >= target.Health() + target.AllShield();
    if (!lethal && IsUnsafe(target.Position(), false, reactive)) return false;
    if (!lethal && Engine::CountEnemiesAt(target.Position(), 625.0f) >
            Slider(RMenu, "MaxCommitEnemies", 2)) return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    const int now = Now();
    Death = {true, static_cast<int>(target.NetworkId()), player.Position(), now,
             now + kRWindowMs, DamageFor(target, 3)};
    Shadows.R = ReconcileShadow(Shadows.R, ShadowKind::R, 0,
                                target.Position(), now, kRShadowMs, false);
    LastCastTick[3] = now;
    return true;
}

inline bool SwapW(bool emergency = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1) || !ShadowAlive(Shadows.W, Now())) return false;
    const bool lethal = Death.Active && Death.TargetId != 0;
    if (!CanSwapToShadow(Shadows.W, Now(), !IsUnsafe(Shadows.W.Position, lethal, emergency),
                         Engine::CountEnemiesAt(Shadows.W.Position, 625.0f),
                         Slider(TacticsMenu, "MaxCommitEnemies", 2), lethal, emergency)) return false;
    if (!Engine::ControllerCastPosition(1, Shadows.W.Position)) return false;
    Shadows.Swapped = !Shadows.Swapped;
    LastCastTick[1] = Now();
    return true;
}

inline bool RecastR(bool emergency = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Death.Active || Death.ExpireTick <= Now() ||
        !Ready(3)) return false;
    const auto target = HeroByNetworkId(Death.TargetId);
    const bool lethal = target.IsValid() && DeathMarkReady(
        Death, Death.TargetId, Now(), target.Health(), target.AllShield(),
        std::max(1, SpellRank(3)));
    const Vector3 point = Death.ReturnPosition;
    const bool safe = SafeReturn(point, !SDK::NavMesh::IsWall(point),
        !Engine::UnderEnemyTurret(point), Engine::CountEnemiesAt(point, 625.0f),
        Slider(RMenu, "MaxReturnEnemies", 2), lethal, emergency);
    if (!safe) return false;
    if (!Engine::ControllerCastPosition(3, point)) return false;
    Death = {};
    Shadows.R = {};
    LastCastTick[3] = Now();
    return true;
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || CurrentResource() < Slider(FarmMenu, "FarmEnergy", 35)) return;
    if (mode == Mode::LastHit) {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!minion.IsValid() || minion.IsDead() ||
                player.Position().Distance2D(minion.Position()) > kQRange ||
                ProjectileWallBlocksFromPlayer(minion.Position(), kQHalfWidth)) continue;
            if (Engine::ControllerCastPosition(0, minion.Position())) return;
        }
        return;
    }
    if (ReadyFor(2, mode) && CurrentResource() >= Slider(FarmMenu, "FarmEnergy", 35)) {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (minion.IsValid() && !minion.IsDead() &&
                player.Position().Distance2D(minion.Position()) <= kERadius) {
                if (Engine::ControllerCastSelf(2)) return;
            }
        }
    }
    if (!ReadyFor(0, mode) || CurrentResource() < Slider(FarmMenu, "FarmEnergy", 35)) return;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (minion.IsValid() && !minion.IsDead() &&
            player.Position().Distance2D(minion.Position()) <= kQRange &&
            !ProjectileWallBlocksFromPlayer(minion.Position(), kQHalfWidth) &&
            Engine::ControllerCastPosition(0, minion.Position())) return;
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (monster.IsValid() && !monster.IsDead() &&
            player.Position().Distance2D(monster.Position()) <= kQRange &&
            !ProjectileWallBlocksFromPlayer(monster.Position(), kQHalfWidth) &&
            Engine::ControllerCastPosition(0, monster.Position())) return;
    }
}

inline void Combo(const AIHeroClient& target) {
    if (!target.IsValid()) return;
    if (Death.Active) {
        if (CastE(target, Mode::Combo, false)) return;
        if (CastQ(target, Mode::Combo, false, false)) return;
        if (RecastR(false)) return;
        (void)SwapW(false);
        return;
    }
    if (CastR(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    (void)CastQ(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    if (!target.IsValid() || CurrentResource() < Slider(TacticsMenu, "HarassEnergy", 55)) return;
    if (CastW(target, Mode::Harass)) return;
    if (CastE(target, Mode::Harass)) return;
    (void)CastQ(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& target) {
    if (ShadowAlive(Shadows.W, Now()) && SwapW(true)) return;
    if (!target.IsValid() || !ReadyFor(1, Mode::Flee, true)) return;
    const auto player = GameObjects::Player();
    const Vector3 cursor = Game::CursorPos();
    const Vector3 direction = Direction2D(player.Position(), cursor);
    if (!direction.IsZero()) {
        const Vector3 point = player.Position() + direction * kWRange;
        if (!IsUnsafe(point, false, true)) (void)Engine::ControllerCastPosition(1, point);
    }
}

inline void Automatic(const AIHeroClient& target) {
    if (InterruptTargetId != 0 && InterruptUntil >= Now()) {
        const auto threat = HeroByNetworkId(InterruptTargetId);
        if (Engine::ValidEnemy(threat, kQRange) && CastQ(threat, Mode::Automatic, true, true)) return;
    }
    if (GapcloserTargetId != 0 && GapcloserUntil >= Now()) {
        const auto threat = HeroByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(threat, kERadius) && CastE(threat, Mode::Automatic, true)) return;
    }
    if (target.IsValid() && !Death.Active && CastR(target, Mode::Automatic, true)) return;
    if (Death.Active) (void)RecastR(true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    const int now = Now();
    if (Shadows.W.ExpireTick <= now) Shadows.W = {};
    if (Shadows.R.ExpireTick <= now) Shadows.R = {};
    if (Death.TargetId != 0) {
        const auto marked = HeroByNetworkId(Death.TargetId);
        const bool deathBuff = marked.IsValid() &&
            (marked.HasBuff("ZedRDeathMark") ||
             marked.HasBuff("ZedRDeathMarkActive") ||
             marked.HasBuff("zedrdeathmark"));
        if (deathBuff) {
            Death.Active = true;
            Death.ExpireTick = std::max(Death.ExpireTick, now + kRWindowMs);
        } else if (Death.ExpireTick <= now) {
            Death = {};
        }
    } else if (Death.ExpireTick <= now) {
        Death = {};
    }
    LastMark = ReconcileShadowMark(LastMark,
        LastMark.TargetId != 0 && HeroByNetworkId(LastMark.TargetId).HasBuff("ZedE"),
        LastMark.TargetId, now, kMarkMs);
    if (InterruptUntil < now) InterruptTargetId = 0;
    if (GapcloserUntil < now) GapcloserTargetId = 0;
    const Mode decisionMode = mode == Mode::None ? Mode::Automatic : mode;
    const AIHeroClient target = Engine::SelectTarget(
        decisionMode == Mode::Flee ? kWRange : kQRange + 100.0f);
    switch (decisionMode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(decisionMode); break;
    case Mode::Flee: Flee(target); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            LastCastTick[slot] = Now();
            if (slot == 1 && !ControllerHelpers::RuntimeNameContains(1, "ZedW2")) {
                const Vector3 end = args.EndPosition.IsValid() && !args.EndPosition.IsZero()
                    ? args.EndPosition : Game::CursorPos();
                Shadows.W = ReconcileShadow(Shadows.W, ShadowKind::W, 0,
                                            end, Now(), kWShadowMs, false);
            }
            if (slot == 3 && !ControllerHelpers::RuntimeNameContains(3, "ZedR2")) {
                Death = {true, args.TargetNetworkId != 0
                    ? static_cast<int>(args.TargetNetworkId) : 0,
                    GameObjects::Player().Position(), Now(), Now() + kRWindowMs, 0.0f};
            }
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args, 220.0f, 100.0f, 300, 250, 220, 1500, 450);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
        InterruptUntil = std::max(InterruptUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack)
        (void)CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick);
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!args.Sender.IsValid() || !args.BuffName[0]) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "zedrdeathmark") ||
        Engine::TextContains(args.BuffName, "zedrdeathmarkactive")) {
        if (added) {
            Death.Active = true;
            Death.TargetId = id;
            Death.ExpireTick = ControllerHelpers::BuffExpireTick(args, kRWindowMs);
        } else if (Death.TargetId == id) Death = {};
    }
    if (Engine::TextContains(args.BuffName, "zede") ||
        Engine::TextContains(args.BuffName, "zedshadowmark")) {
        LastMark = ReconcileShadowMark(LastMark, added, id, Now(), kMarkMs);
        if (added) LastMark.FromE = true;
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) { UpdateBuffState(args, true); }
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) { UpdateBuffState(args, false); }

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
    (void)CaptureGapcloser(args, GapcloserTargetId, GapcloserEndpoint, GapcloserUntil, 700, 1100);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, InterruptTargetId, InterruptUntil, 900, 250, 5000);
}

inline bool IsShadowObject(const SDK::Events::ObjectEventArgs& args, bool r) {
    return ControllerHelpers::AnyTextContains(
        {args.Sender.Name, args.Sender.CharacterName, args.SpellName, args.MissileName},
        r ? std::initializer_list<const char*>{"zedrshadow", "zedrshadowclone"}
          : std::initializer_list<const char*>{"zedshadow", "zedwshadow", "zedshadowclone"});
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() || !ControllerHelpers::ObjectEventIsAllied(args)) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (IsShadowObject(args, true)) {
        Shadows.R = ReconcileShadow(Shadows.R, ShadowKind::R, id,
                                     args.Sender.Position, Now(), kRShadowMs, true);
    } else if (IsShadowObject(args, false)) {
        Shadows.W = ReconcileShadow(Shadows.W, ShadowKind::W, id,
                                    args.Sender.Position, Now(), kWShadowMs, true);
    }
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (id != 0 && id == Shadows.W.NetworkId) Shadows.W = {};
    if (id != 0 && id == Shadows.R.NetworkId) Shadows.R = {};
}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args)) return;
    if (ControllerHelpers::AnyTextContains(
            {args.SpellName, args.MissileName}, {"zedq", "razorshuriken"})) {
        QMissileId = args.MissileNetworkId != 0
            ? static_cast<int>(args.MissileNetworkId) : static_cast<int>(args.Sender.NetworkId);
    }
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId) : static_cast<int>(args.Sender.NetworkId);
    if (id == QMissileId) QMissileId = 0;
}
inline void OnDraw() {
    if (!CoachMenu || !Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFB050E8u, 1.0f, 40);
    Drawing::DrawCircle(player.Position(), kERadius, 0xFFB050E8u, 1.0f, 40);
    if (ShadowAlive(Shadows.W, Now())) Drawing::DrawCircle(Shadows.W.Position, 90.0f, 0xFF50D0FFu, 1.5f, 32);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("Zed tactics", "Zed shadow tactics"));
    TacticsMenu->Add(new MenuSlider("EnergyReserve", "Energy reserve", 20, 0, 100));
    TacticsMenu->Add(new MenuSlider("HarassEnergy", "Harass energy percent", 55, 0, 100));
    TacticsMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum commit enemies", 2, 0, 5));
    ShadowMenu = TacticsMenu->AddSubMenu(new Menu("Shadow pairs", "W shadow and swaps"));
    ShadowMenu->Add(new MenuBool("AllowEmergencySwap", "Allow emergency W swap", true));
    RMenu = TacticsMenu->AddSubMenu(new Menu("Death Mark", "R return safety"));
    RMenu->Add(new MenuSlider("EnergyReserve", "R energy reserve", 30, 0, 100));
    RMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies at R target", 2, 0, 5));
    RMenu->Add(new MenuSlider("MaxReturnEnemies", "Maximum enemies at R return", 2, 0, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Zed farming", "Q/E farm gates"));
    FarmMenu->Add(new MenuSlider("FarmEnergy", "Minimum farming energy", 35, 0, 100));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("Zed coach", "State visualization"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/E and W shadow", false));
}

inline void OnLoad() {
    Shadows = {};
    LastMark = {};
    Death = {};
    LastCastTick.fill(0);
    QCastTick = QTargetId = QMissileId = LastAutoTargetId = LastAutoTick = 0;
    GapcloserTargetId = GapcloserUntil = InterruptTargetId = InterruptUntil = 0;
    GapcloserEndpoint = {};
}
inline void OnUnload() {
    TacticsMenu = ShadowMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    OnLoad();
}

inline constexpr const char* Scenarios[] = {
    "Pin Zed to Riot 26.15 and CommunityDragon PC 16.15 runtime spell/object data",
    "Use the engine-selected target for autonomous combat",
    "Predict Razor Shuriken and evaluate the first collision body",
    "Apply full Q damage only to the first collision and reduced damage afterward",
    "Preserve a real attack windup while allowing explicit reactive exceptions",
    "Track Living Shadow W spawn, expiry and recast swap state",
    "Reconcile W shadow state from object events, casts and polling",
    "Track W/E shadow marks with source provenance and expiry",
    "Cast E only when Zed or an active shadow reaches the target",
    "Track Death Mark target, return position, expiry and stored damage",
    "Recast R only through a walkable safe return or lethal/emergency override",
    "Reject ordinary R commits under turrets and excessive enemy counts",
    "Gate Q/W/E/R by live energy costs plus reserve",
    "Use runtime mitigated damage and shield-inclusive lethal checks",
    "Reconcile player casts while retaining shadow and mark state",
    "Combo uses death mark, shadow slash, shuriken and controlled return",
    "Harass uses W/E/Q poke with an independent energy threshold",
    "LaneClear, Jungle and LastHit use Q/E only with real range and resource gates",
    "Flee prioritizes an existing W swap and otherwise places W toward cursor",
    "Automatic mode reacts to gapclosers/interrupts before lethal R",
    "Expose complete event callbacks without taking ownership of shared registration",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Zed;
    controller.ControllerId = "champion.kuroaio.ai.zed.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIZed.md";
    controller.ImplementationSummary =
        "First-collision shuriken damage, W/R shadow-pair reconciliation, "
        "W/E mark tracking and safe death-mark return state machine.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Zed
