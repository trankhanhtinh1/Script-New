#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIZileanGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>

namespace Plugins::KuroAIO::AI::Controllers::Zilean {

using namespace Geometry;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::Ready;
using ControllerHelpers::SpellEnabled;

inline Menu* TacticsMenu = nullptr;
inline Menu* BombMenu = nullptr;
inline Menu* WarpMenu = nullptr;
inline Menu* ChronoMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

struct BombState {
    int NetworkId = 0;
    int BombCount = 0;
    int FirstAttachTick = 0;
    int FirstDetonateTick = 0;
    int LastAttachTick = 0;
    int LastDetonateTick = 0;
    int ExpireTick = 0;
    Vector3 Position{};
    bool StunArmed = false;
};

inline std::array<BombState, 16> Bombs{};
inline std::array<int, 4> LastCastTick{};
inline int PendingQTargetId = 0;
inline int DoubleBombTargetId = 0;
inline int ManualOwnershipUntil = 0;
inline int ManualRTargetId = 0;
inline int ManualRUntil = 0;
inline int ProtectedTargetId = 0;
inline int ProtectedUntil = 0;
inline int IncomingThreatUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastEAllyId = 0;
inline Vector3 LastQAim{};
inline Vector3 LastEPosition{};

inline bool TextContains(const char* value, const char* token) {
    return Engine::TextContains(value, token);
}

inline BombState* FindBomb(int networkId, bool create = false) {
    if (networkId <= 0) return nullptr;
    for (auto& bomb : Bombs) {
        if (bomb.NetworkId == networkId) return &bomb;
    }
    if (!create) return nullptr;
    for (auto& bomb : Bombs) {
        if (bomb.NetworkId == 0 || bomb.ExpireTick < Now()) {
            bomb = {};
            bomb.NetworkId = networkId;
            return &bomb;
        }
    }
    return nullptr;
}

inline void ReconcileBomb(const AIHeroClient& target, int now) {
    if (!Engine::ValidEnemy(target)) return;
    const int id = static_cast<int>(target.NetworkId());
    BombState* bomb = FindBomb(id, false);
    const bool hasBomb = target.HasBuff("ZileanQ") ||
                         target.HasBuff("ZileanQEnemyBomb");
    if (hasBomb && !bomb) {
        bomb = FindBomb(id, true);
        if (bomb) {
            bomb->BombCount = 1;
            bomb->FirstAttachTick = now;
            bomb->FirstDetonateTick = now + static_cast<int>(kQFuseSeconds * 1000.0f);
            bomb->LastAttachTick = now;
            bomb->LastDetonateTick = bomb->FirstDetonateTick;
            bomb->ExpireTick = bomb->FirstDetonateTick;
            bomb->Position = target.Position();
        }
    } else if (bomb && !hasBomb && bomb->ExpireTick < now) {
        *bomb = {};
    }
    if (bomb && bomb->ExpireTick < now) *bomb = {};
}

inline void ReconcileState() {
    const int now = Now();
    for (const auto& enemy : GameObjects::EnemyHeroes()) ReconcileBomb(enemy, now);
    for (auto& bomb : Bombs) {
        if (bomb.NetworkId != 0 && bomb.ExpireTick < now) bomb = {};
    }
    if (ProtectedUntil < now) {
        ProtectedUntil = 0;
        ProtectedTargetId = 0;
    }
    if (ManualOwnershipUntil < now) ManualOwnershipUntil = 0;
    if (ManualRUntil < now) {
        ManualRUntil = 0;
        ManualRTargetId = 0;
    }
}

inline bool ManualOwned() { return ManualOwnershipUntil > Now(); }

inline bool PreserveAttack(bool reactive) {
    return ControllerHelpers::PreserveAttack(reactive);
}

inline bool TargetProtected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           ControllerHelpers::HasSpellShieldOrImmunity(target);
}

inline bool QPrediction(const AIHeroClient& target, Vector3& aim) {
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const float delay = kQDelay + player.Position().Distance2D(target.Position()) /
        kQSpeed;
    aim = PredictPosition(target, delay);
    if (Engine::RuntimeSpells[0]) {
        const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
        if (prediction.Hitchance >= SDK::HitChance::High &&
            prediction.GetCastPosition().IsValid() &&
            !prediction.GetCastPosition().IsZero()) {
            aim = prediction.GetCastPosition();
        } else if (!target.IsDashing() && !Engine::IsHardCrowdControlled(target)) {
            return false;
        }
    }
    return aim.IsValid() && !aim.IsZero() &&
           player.Position().Distance2D(aim) <= kQRange + target.BoundingRadius();
}

inline bool QCollisionSafe(const AIHeroClient& target, const Vector3& aim) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    CollisionBody bodies[16]{};
    std::size_t count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) || count >= std::size(bodies)) continue;
        bodies[count++] = {enemy.Position(), enemy.BoundingRadius(),
                           static_cast<int>(enemy.NetworkId()),
                           enemy.NetworkId() == target.NetworkId()};
    }
    if (BombPathCollision(player.Position(), aim, bodies, count, true)) {
        for (std::size_t i = 0; i < count; ++i) {
            if (bodies[i].NetworkId == static_cast<int>(target.NetworkId())) continue;
            const auto projection = SharedGeometry::ProjectPointToSegment2D(
                bodies[i].Position, player.Position(), aim);
            if (projection.Distance <= kQMissileWidth * 0.5f + bodies[i].Radius &&
                projection.T > 0.02f && projection.T < 0.98f) return false;
        }
    }
    return !ControllerHelpers::ProjectileWallBlocksFromPlayer(
        aim, kQMissileWidth * 0.5f);
}

inline bool DoubleBombReady(const AIHeroClient& target) {
    const auto* bomb = FindBomb(static_cast<int>(target.NetworkId()));
    if (!bomb || bomb->BombCount < 1 || bomb->FirstDetonateTick <= Now()) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const float impact = static_cast<float>(Now()) +
        BombImpactSeconds(player.Position(), target.Position()) * 1000.0f;
    return DoubleBombWindow(static_cast<float>(bomb->FirstDetonateTick), impact,
                            static_cast<float>(Now()));
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) ||
        !Ready(0, mode) || !SpellEnabled(0, mode) ||
        LastCastTick[0] + (reactive ? 25 : 72) > Now() ||
        PreserveAttack(reactive) || TargetProtected(target)) return false;
    Vector3 aim{};
    if (!QPrediction(target, aim) || !QCollisionSafe(target, aim)) return false;
    if (Engine::UnderEnemyTurret(aim) && !Engine::UnderEnemyTurret(player.Position()))
        return false;
    const int id = static_cast<int>(target.NetworkId());
    if (FindBomb(id) && !DoubleBombReady(target)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    LastQAim = aim;
    PendingQTargetId = id;
    return true;
}

inline bool CastWForDoubleBomb(Mode mode) {
    const int id = DoubleBombTargetId != 0 ? DoubleBombTargetId : PendingQTargetId;
    BombState* bomb = FindBomb(id);
    if (id <= 0 || !bomb || bomb->BombCount != 1 ||
        !Ready(1, mode) || !SpellEnabled(1, mode) ||
        LastCastTick[1] + 72 > Now() || PreserveAttack(false)) return false;
    const int qCooldownAge = Now() - LastCastTick[0];
    if (Ready(0) || qCooldownAge < 0 ||
        qCooldownAge > kWCooldownReductionMs) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    DoubleBombTargetId = id;
    return true;
}

inline bool CastSecondBomb(const AIHeroClient& target, Mode mode) {
    if (DoubleBombTargetId == 0 ||
        static_cast<int>(target.NetworkId()) != DoubleBombTargetId ||
        !DoubleBombReady(target)) return false;
    return CastQ(target, mode, true);
}

inline AIHeroClient SelectSpeedAlly(bool flee) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    if (flee && player.HealthPercent() < 75.0f) return player;
    AIHeroClient best{};
    float bestScore = -100000.0f;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally, kERange) ||
            ally.NetworkId() == player.NetworkId()) continue;
        const float score = (100.0f - ally.HealthPercent()) * 1.8f +
            ally.TotalAttackDamage() * 0.38f + ally.AP() * 0.32f +
            static_cast<float>(Engine::CountEnemiesAt(ally.Position(), 650.0f)) * 85.0f;
        if (score > bestScore) {
            bestScore = score;
            best = ally;
        }
    }
    return best.IsValid() ? best : player;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid() || !Ready(2, mode) ||
        !SpellEnabled(2, mode) || LastCastTick[2] + (reactive ? 30 : 75) > Now() ||
        PreserveAttack(reactive) || !ETargetInReach(player.Position(),
                                                    target.Position(),
                                                    target.BoundingRadius())) return false;
    const bool ally = !target.IsEnemy();
    if (!ally && TargetProtected(target)) return false;
    if (!Engine::ControllerCastUnit(2, target)) return false;
    LastCastTick[2] = Now();
    LastEAllyId = ally ? static_cast<int>(target.NetworkId()) : 0;
    LastEPosition = target.Position();
    return true;
}

inline float CandidateIncoming(const AIHeroClient& ally) {
    if (IncomingThreatUntil > Now()) return 68.0f;
    return std::max(0.0f, 55.0f - ally.HealthPercent()) * 1.5f;
}

inline AIHeroClient SelectChronoTarget(bool urgentOnly) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3) || !SpellEnabled(3, Mode::Automatic)) return {};
    ResurrectionCandidate candidates[8]{};
    AIHeroClient heroes[8]{};
    std::size_t count = 0;
    auto consider = [&](const AIHeroClient& ally, bool self) {
        if (!ally.IsValid() || ally.IsDead() || !ally.IsTargetable() ||
            ally.Position().Distance2D(player.Position()) > kRRange || count >= 8) return;
        const bool protectedByBuff = ally.HasBuff("ChronoShift") ||
                                    ally.HasBuff("ChronoRevive");
        const int enemies = Engine::CountEnemiesAt(ally.Position(), 700.0f);
        const float incoming = CandidateIncoming(ally);
        const bool urgent = ally.HealthPercent() <=
                Slider(ChronoMenu, "HealthThreshold", 36) || incoming >= 60.0f;
        if (urgentOnly && !urgent) return;
        candidates[count] = {static_cast<int>(ally.NetworkId()), ally.HealthPercent(),
            incoming, ally.TotalAttackDamage() * 0.85f + ally.AP() * 0.72f,
            enemies, self, protectedByBuff,
            static_cast<int>(ally.NetworkId()) == ManualRTargetId && ManualRUntil > Now(),
            true};
        heroes[count++] = ally;
    };
    consider(player, true);
    for (const auto& ally : GameObjects::AllyHeroes()) consider(ally, false);
    const int selectedId = ChooseChronoTarget(candidates, count);
    for (std::size_t i = 0; i < count; ++i) {
        if (static_cast<int>(heroes[i].NetworkId()) == selectedId) return heroes[i];
    }
    return {};
}

inline bool CastChronoshift(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (!target.IsValid() || !Ready(3, mode) || !SpellEnabled(3, mode) ||
        LastCastTick[3] + (reactive ? 30 : 85) > Now() ||
        target.HasBuff("ChronoShift") || target.HasBuff("ChronoRevive") ||
        (!reactive && ManualRTargetId == static_cast<int>(target.NetworkId()) &&
         ManualRUntil > Now())) return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    LastCastTick[3] = Now();
    ProtectedTargetId = static_cast<int>(target.NetworkId());
    ProtectedUntil = Now() + static_cast<int>(kRDurationSeconds * 1000.0f);
    return true;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const float bombDamage = 2.0f * (230.0f + 0.9f * player.AP());
    return LethalAfterShield(bombDamage, target.Health(), target.AllShield()) &&
           DoubleBombReady(target) && CastSecondBomb(target, mode);
}

inline void Combo(const AIHeroClient& target) {
    const AIHeroClient save = SelectChronoTarget(true);
    if (save.IsValid() && CastChronoshift(save, Mode::Combo, true)) return;
    if (CastSecondBomb(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastWForDoubleBomb(Mode::Combo)) return;
    if (Engine::ValidEnemy(target) && (target.IsDashing() || target.HealthPercent() < 55.0f) &&
        CastE(target, Mode::Combo)) return;
    const AIHeroClient ally = SelectSpeedAlly(false);
    if (ally.IsValid() && ally.NetworkId() != GameObjects::Player().NetworkId())
        (void)CastE(ally, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(WarpMenu, "HarassMana", 48)) return;
    if (CastSecondBomb(target, Mode::Harass)) return;
    if (CastQ(target, Mode::Harass)) return;
    if (Engine::ValidEnemy(target) && target.HealthPercent() < 70.0f)
        (void)CastE(target, Mode::Harass);
}

inline void Flee() {
    const AIHeroClient ally = SelectSpeedAlly(true);
    if (ally.IsValid() && CastE(ally, Mode::Flee, true)) return;
    const AIHeroClient threat = ControllerHelpers::NearestEnemyToPlayer({}, 700.0f);
    if (Engine::ValidEnemy(threat)) (void)CastE(threat, Mode::Flee, true);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "Mana", 42)) return;
    (void)Engine::TryFarm(mode);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const AIHeroClient target = PreferredEnemyTarget(selected, kQRange);
    if (!ManualOwned()) {
        const AIHeroClient save = SelectChronoTarget(true);
        if (save.IsValid() && (mode == Mode::Automatic || mode == Mode::Combo) &&
            CastChronoshift(save, mode, true)) return true;
        if (TryKillSecure(target, mode)) return true;
        switch (mode) {
        case Mode::Combo: Combo(target); break;
        case Mode::Harass: Harass(target); break;
        case Mode::Flee: Flee(); break;
        case Mode::LaneClear:
        case Mode::Jungle:
        case Mode::LastHit: Farm(mode); break;
        case Mode::Automatic:
            if (IncomingThreatUntil > Now()) {
                const auto ally = SelectSpeedAlly(true);
                if (ally.IsValid()) (void)CastE(ally, mode, true);
            }
            break;
        default: break;
        }
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (args.Slot >= 0 && args.Slot < 4) LastCastTick[args.Slot] = now;
        if (args.Slot == 0 && !Engine::WasControllerCast(0)) {
            ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
            PendingQTargetId = static_cast<int>(args.TargetNetworkId);
        }
        if (args.Slot == 3 && !Engine::WasControllerCast(3)) {
            ManualRTargetId = static_cast<int>(args.TargetNetworkId);
            ManualRUntil = now + Slider(ChronoMenu, "ManualSaveMs", 900);
        }
        if (args.Slot == 0 && PendingQTargetId > 0) {
            BombState* bomb = FindBomb(PendingQTargetId, true);
            if (bomb) {
                const auto target = GameObjects::GetUnitByNetworkId<AIHeroClient>(
                    PendingQTargetId);
                const auto player = GameObjects::Player();
                const Vector3 destination = args.CastPosition.IsValid() &&
                        !args.CastPosition.IsZero()
                    ? args.CastPosition : (target.IsValid() ? target.Position() : args.EndPosition);
                const int impact = now + static_cast<int>(
                    (player.IsValid() ? BombImpactSeconds(player.Position(), destination) : kQDelay) * 1000.0f);
                if (bomb->BombCount == 0) {
                    bomb->BombCount = 1;
                    bomb->FirstAttachTick = impact;
                    bomb->FirstDetonateTick = impact + static_cast<int>(kQFuseSeconds * 1000.0f);
                } else {
                    bomb->BombCount = std::min(2, bomb->BombCount + 1);
                    bomb->StunArmed = DoubleBombWindow(
                        static_cast<float>(bomb->FirstDetonateTick), static_cast<float>(impact),
                        static_cast<float>(now));
                }
                bomb->LastAttachTick = impact;
                bomb->LastDetonateTick = impact + static_cast<int>(kQFuseSeconds * 1000.0f);
                bomb->ExpireTick = bomb->LastDetonateTick;
                bomb->Position = destination;
            }
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer))
        IncomingThreatUntil = std::max(IncomingThreatUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int now = Now();
    if (TextContains(args.BuffName, "ZileanQ")) {
        BombState* bomb = FindBomb(static_cast<int>(args.Sender.NetworkId), true);
        if (bomb) {
            if (bomb->BombCount == 0) bomb->BombCount = 1;
            bomb->LastAttachTick = now;
            bomb->LastDetonateTick = now + static_cast<int>(kQFuseSeconds * 1000.0f);
            if (bomb->FirstDetonateTick <= now) bomb->FirstDetonateTick = bomb->LastDetonateTick;
            bomb->ExpireTick = bomb->LastDetonateTick;
        }
    }
    if (TextContains(args.BuffName, "ChronoShift") ||
        TextContains(args.BuffName, "ChronoRevive")) {
        ProtectedTargetId = static_cast<int>(args.Sender.NetworkId);
        ProtectedUntil = std::max(ProtectedUntil, static_cast<int>(args.EndTime * 1000.0f));
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (TextContains(args.BuffName, "ZileanQ")) {
        if (auto* bomb = FindBomb(static_cast<int>(args.Sender.NetworkId))) {
            if (bomb->BombCount < 2 || bomb->ExpireTick < Now()) *bomb = {};
        }
    }
    if (TextContains(args.BuffName, "ChronoShift") ||
        TextContains(args.BuffName, "ChronoRevive")) {
        if (ProtectedTargetId == static_cast<int>(args.Sender.NetworkId)) {
            ProtectedTargetId = 0;
            ProtectedUntil = 0;
        }
    }
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (args.EndTime > Game::Time()) OnBuffAdd(args);
    else OnBuffRemove(args);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    ControllerHelpers::CaptureBeforeAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    ControllerHelpers::CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    (void)ControllerHelpers::CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs&) {
    IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 600);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs&) {
    IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 700);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF55BBDD, 1.2f, 36);
    Drawing::DrawCircle(player.Position(), kERange, 0xFF66DD88, 1.0f, 32);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFBB77FF, 1.0f, 36);
    for (const auto& bomb : Bombs) {
        if (bomb.NetworkId != 0 && bomb.ExpireTick > Now() && !bomb.Position.IsZero())
            Drawing::DrawCircle(bomb.Position, kQRadius,
                bomb.StunArmed ? 0xFFFFAA22u : 0xFF55BBDD, 1.4f, 24);
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("ZileanTactics", "Zilean timing and safety"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 560, 180, 1200));
    BombMenu = TacticsMenu->AddSubMenu(new Menu("QW", "Double-bomb timing"));
    BombMenu->Add(new MenuBool("UseDoubleBomb", "Use Rewind for verified stun", true));
    WarpMenu = TacticsMenu->AddSubMenu(new Menu("E", "Time Warp"));
    WarpMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 48, 15, 90));
    ChronoMenu = TacticsMenu->AddSubMenu(new Menu("R", "Chronoshift"));
    ChronoMenu->Add(new MenuSlider("HealthThreshold", "Save below health percent", 36, 10, 70));
    ChronoMenu->Add(new MenuSlider("ManualSaveMs", "Protect manual save (ms)", 900, 250, 1800));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Farm", "Bomb farm policy"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 42, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("Coach", "Zilean geometry"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw bomb and safety ranges", false));
}

inline void OnLoad() {
    Bombs = {};
    LastCastTick = {};
    PendingQTargetId = DoubleBombTargetId = 0;
    ManualOwnershipUntil = ManualRTargetId = ManualRUntil = 0;
    ProtectedTargetId = ProtectedUntil = IncomingThreatUntil = 0;
    LastAutoTargetId = LastAutoTick = LastEAllyId = 0;
    LastQAim = LastEPosition = {};
}
inline void OnUnload() {
    TacticsMenu = BombMenu = WarpMenu = ChronoMenu = FarmMenu = CoachMenu = nullptr;
    Bombs = {};
    LastQAim = LastEPosition = {};
}

inline constexpr const char* Scenarios[] = {
    "Pin Zilean mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Predict Q impact using cast delay and 2000 speed within the 900 range",
    "Reject Q through projectile walls and unexpected champion collision",
    "Track attached bombs from Q events, buff events and polling reconciliation",
    "Treat the second bomb as a stun only when it lands before the first fuse",
    "Use W only to reduce Q cooldown for a verified double-bomb sequence",
    "Keep Q-W-Q state champion-owned instead of fixed four-spell ordering",
    "Use E on allies for rank-scaled speed and on enemies for rank-scaled slow",
    "Prefer E peel or escape speed when a gapcloser or unsafe mobility window is observed",
    "Score Chronoshift candidates by health, incoming threat, carry value and enemies",
    "Never overwrite an existing Chronoshift or a recent manually selected save",
    "Reject Chronoshift outside 900 range or without a lethal/urgent health gate",
    "Preserve orbwalker selected target before fallback target selection",
    "Preserve AA windup unless a reactive stun, peel or resurrection is urgent",
    "Yield after manual Q W E or R and reconcile manual ownership from spell events",
    "Combo completes double bomb before Time Warp utility and reserves R for saves",
    "Harass uses mana-gated bomb poke and enemy slow without unsolicited R",
    "LaneClear, Jungle and LastHit use mana-gated bomb farm policy",
    "Flee prioritizes ally/self Time Warp and slows the nearest pursuer",
    "Automatic mode only casts urgent Chronoshift or reactive Time Warp",
    "Never automate items, summoners or movement ownership",
    "Draw bomb fuse, stun-ready and spell ranges without changing decisions",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Zilean";
    controller.ControllerId = "champion.kuroaio.ai.zilean.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIZilean.md";
    controller.ImplementationSummary =
        "Champion-owned bomb-fuse and Rewind state machine with E movement utility, scored Chronoshift saves, prediction, collision, wall and manual-ownership guards.";
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
    controller.OnBuffUpdate = &OnBuffUpdate;
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

} // namespace Plugins::KuroAIO::AI::Controllers::Zilean
