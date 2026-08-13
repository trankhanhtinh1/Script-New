#pragma once

#include "../../Core/Game.h"
#include "../../Core/Hud.h"
#include "../../Core/Objects.h"
#include "../../Core/Variables.h"
#include "../../Enumerations/DamageType.h"
#include "../../Enumerations/HealthPredictionType.h"
#include "../../Enumerations/KeyBindType.h"
#include "../../Enumerations/MinionTypes.h"
#include "../../Enumerations/OrbwalkingMode.h"
#include "../../Enumerations/OrbwalkingType.h"
#include "../../Enumerations/SpellSlot.h"
#include "../../Events/Events.h"
#include "../../Extensions/Unit.h"
#include "../../GameObjects/GameObjects.h"
#include "../../GameObjects/ObjectManager.h"
#include "../../Math/Collision.h"
#include "../../Math/HealthPrediction.h"
#include "../../UI/Drawing.h"
#include "../../UI/Icons.h"
#include "../../UI/UI.h"
#include "../../Utils/AssetInstaller.h"
#include "../../Utils/AutoAttack.h"
#include "../../Utils/DelayAction.h"
#include "../../Variables.h"
#include "../../Wrappers/Damages/Damage.h"
#include "../../../Core/CoreAttackableUnit.h"
#include "../../../Core/CoreBuffs.h"
#include "../../../Core/CoreControl.h"
#include "../../../Core/CoreObjectManager.h"
#include "../../../Core/Globals.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SDK {

enum class OrbwalkerModeFlag : std::int32_t {
    None = -1,
    Combo = 1,
    Harass = 2,
    LaneClear = 3,
    LastHit = 4,
    Flee = 5,
    Custom = 6,
};

class OrbwalkerBase;
class OrbwalkerSelector;
class Orbwalker;

class OrbwalkingActionArgs {
public:
    AttackableUnit Target = {};
    Vector3 Position = {};
    bool Process = true;
    OrbwalkingType Type = OrbwalkingType::None;
    const char* OrbwalkerName = "SDK";

    OrbwalkingActionArgs() = default;
    OrbwalkingActionArgs(OrbwalkingType type,
                         const AttackableUnit& target,
                         const Vector3& position = {},
                         const char* orbwalkerName = "SDK")
        : Target(target),
          Position(position),
          Process(true),
          Type(type),
          OrbwalkerName(orbwalkerName ? orbwalkerName : "SDK") {}
};

class IOrbwalker {
public:
    virtual ~IOrbwalker() = default;

    virtual AttackableUnit ForceTarget() const = 0;
    virtual void ForceTarget(const AttackableUnit& target) = 0;
    virtual AttackableUnit LastTarget() const = 0;
    virtual OrbwalkingMode ActiveMode() const = 0;
    virtual int LastAutoAttackTick() const = 0;
    virtual void LastAutoAttackTick(int value) = 0;
    virtual int LastMovementTick() const = 0;
    virtual void LastMovementTick(int value) = 0;
    virtual bool AttackEnabled() const = 0;
    virtual void AttackEnabled(bool value) = 0;
    virtual bool MoveEnabled() const = 0;
    virtual void MoveEnabled(bool value) = 0;
    virtual void SetOrbwalkerPosition(const Vector3& position) = 0;
    virtual void SetPauseTime(int time) = 0;
    virtual void SetServerPauseTime(int time) = 0;
    virtual void SetAttackPauseTime(int time) = 0;
    virtual void SetAttackServerPauseTime(int time) = 0;
    virtual void SetMovePauseTime(int time) = 0;
    virtual void SetMoveServerPauseTime(int time) = 0;
    virtual AttackableUnit GetTarget() = 0;
    virtual bool CanAttack() = 0;
    virtual bool CanAttack(float extraWindup) = 0;
    virtual bool CanMove() = 0;
    virtual bool CanMove(float extraWindup, bool disableMissileCheck) = 0;
    virtual bool Attack(const AttackableUnit& target) = 0;
    virtual void Move(const Vector3& position) = 0;
    virtual void Orbwalk(const AttackableUnit& target, const Vector3& position = {}) = 0;
    virtual void ResetAutoAttackTimer() = 0;
    virtual void Dispose() = 0;

    // ── Extended timing/debug API ──
    // Virtual with safe defaults so implementations only override what they
    // can answer; the facade (SDK::Orbwalker) forwards to these.
    virtual bool IsAutoAttacking() { return false; }
    virtual bool IsWindingUp() { return false; }
    virtual bool IsAttackCastComplete() { return true; }
    virtual int AttackCastDelayRemaining() { return 0; }
    virtual int NextAttackReadyTick() { return 0; }
    virtual int AttackCooldownRemaining() { return 0; }
    virtual bool ShouldWait() { return false; }
    virtual void DebugPrint(const char* /*text*/) {}
    virtual void ClearDebugConsole() {}

    // Suspend/Resume let an overriding orbwalker (registered via
    // Orbwalker::AddOrbwalker) fully stop this implementation: unhook its
    // game events and hide its menu, without destroying it.
    virtual void Suspend() {}
    virtual void Resume() {}
};

namespace OrbwalkingDetail {

template <typename T, int MaxHandlers = 32>
class EventList {
public:
    using Handler = void(*)(T&);

    bool Add(Handler handler) {
        if (!handler) {
            return false;
        }
        for (int i = 0; i < count_; ++i) {
            if (handlers_[i] == handler) {
                return true;
            }
        }
        if (count_ >= MaxHandlers) {
            return false;
        }
        handlers_[count_++] = handler;
        return true;
    }

    bool Remove(Handler handler) {
        if (!handler) {
            return false;
        }
        for (int i = 0; i < count_; ++i) {
            if (handlers_[i] != handler) {
                continue;
            }
            for (int j = i; j + 1 < count_; ++j) {
                handlers_[j] = handlers_[j + 1];
            }
            handlers_[--count_] = nullptr;
            return true;
        }
        return false;
    }

    void Fire(T& args) const {
        for (int i = 0; i < count_; ++i) {
            Handler handler = handlers_[i];
            if (!handler) {
                continue;
            }
            __try {
                handler(args);
            } __except (1) {}
        }
    }

private:
    Handler handlers_[MaxHandlers] = {};
    int count_ = 0;
};

inline EventList<OrbwalkingActionArgs> BeforeAttackHandlers;
inline EventList<OrbwalkingActionArgs> AttackHandlers;
inline EventList<OrbwalkingActionArgs> AfterAttackHandlers;
inline EventList<OrbwalkingActionArgs> BeforeMoveHandlers;
inline EventList<OrbwalkingActionArgs> NonKillableMinionHandlers;
inline OrbwalkerBase* RuntimeInstance = nullptr;
inline IOrbwalker* Implementation = nullptr;
inline std::unordered_map<std::string, IOrbwalker*> Implementations;
inline std::string SelectedImplementationName = "SDK";

inline std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

inline bool EqualsIgnoreCase(const std::string& left, const char* right) {
    return ToLower(left) == ToLower(right ? std::string(right) : std::string());
}

inline bool Contains(const char* const* values, const std::string& value) {
    for (int i = 0; values[i]; ++i) {
        if (value == values[i]) {
            return true;
        }
    }
    return false;
}

inline bool Contains(std::initializer_list<const char*> values, const std::string& value) {
    for (const char* item : values) {
        if (item && value == item) {
            return true;
        }
    }
    return false;
}

inline float PathLength(const std::vector<Vector3>& path) {
    float length = 0.0f;
    for (std::size_t i = 1; i < path.size(); ++i) {
        length += path[i - 1].Distance(path[i]);
    }
    return length;
}

inline bool SameObject(const GameObject& left, const GameObject& right) {
    return left.IsValid() && right.IsValid() && left.Compare(right);
}

inline bool IsLocalPlayer(const ::Core::Events::ObjectInfo& sender) {
    return Events::IsLocalPlayer(sender);
}

inline AttackableUnit ToAttackableUnit(const ::Core::Events::ObjectInfo& info) {
    if (!info.IsValid()) {
        return {};
    }
    auto type = info.Type;
    if (type == ::Core::Objects::ObjectType::Unknown) {
        type = ::Core::ObjectManager::InferType(info.Ptr);
    }
    return AttackableUnit(info.Ptr, type);
}

inline AIBaseClient ToAIBaseClient(const ::Core::Events::ObjectInfo& info) {
    if (!info.IsValid()) {
        return {};
    }
    auto type = info.Type;
    if (type == ::Core::Objects::ObjectType::Unknown) {
        type = ::Core::ObjectManager::InferType(info.Ptr);
    }
    return AIBaseClient(info.Ptr, type);
}

inline bool IsEnemyOrNeutral(const GameObject& unit) {
    return unit.IsEnemy() || unit.Team() == GameObjectTeam::Neutral;
}

inline bool IsValidAttackTarget(const AttackableUnit& unit,
                                float range = FLT_MAX,
                                const Vector3& from = {}) {
    if (!unit.IsValid() || (unit.IsDead() && !unit.IsZombie()) ||
        !unit.IsTargetable() || unit.IsInvulnerable() ||
        !IsEnemyOrNeutral(unit)) {
        return false;
    }

    const auto player = GameObjects::Player();
    // EnsoulSharp measures range from source.ServerPosition to the target's
    // ServerPosition (predicted server-side positions), not the visual Position.
    // Using the player's ServerPosition as the origin is what lets attacks land
    // at the true max range while kiting. Target side stays on Position() so we
    // never read AiManager pathing on static structures.
    const Vector3 origin = from.IsZero()
        ? (player.IsValid() ? player.ServerPosition() : Vector3())
        : from;
    if (range < FLT_MAX && origin.IsValid() && !origin.IsZero()) {
        const float rangeSqr = range * range;
        if (origin.DistanceSqr2D(unit.Position()) > rangeSqr) {
            return false;
        }
    }
    return true;
}

inline bool IsLaneMinion(const AIMinionClient& minion) {
    const MinionTypes type = minion.GetMinionType();
    return HasFlag(type, MinionTypes::Melee) ||
           HasFlag(type, MinionTypes::Ranged) ||
           HasFlag(type, MinionTypes::Siege) ||
           HasFlag(type, MinionTypes::Super);
}

inline bool IsWard(const AIMinionClient& minion) {
    return HasFlag(minion.GetMinionType(), MinionTypes::Ward);
}

inline bool IsEnsoulSpecialMinion(const AIMinionClient& minion) {
    if (!minion.IsValid() || minion.IsDead() || !minion.IsMinion() ||
        !minion.IsEnemy() || minion.Team() == GameObjectTeam::Neutral ||
        minion.IsPlant() || IsWard(minion)) {
        return false;
    }

    const std::string name = ToLower(minion.CharacterName());
    return Contains({
        "tibbers", "annietibbers", "elisespiderling",
        "heimertyellow", "heimertblue", "ivernminion",
        "malzaharvoidling", "zacrebirthbloblet", "shacobox",
        "yorickghoulmelee", "yorickbigghoul", "zyrathornplant",
        "zyragraspingplant", "teemomushroom", "apheliosturret",
        "kalistaspawn", "jhintrap", "nidaleespear",
        "illaoiminion", "sru_riftherald_mercenary",
        "belvethvoidling", "leblanc", "monkeyking",
        "neeko", "shaco",
    }, name);
}

inline void FireBeforeAttack(OrbwalkingActionArgs& args) {
    BeforeAttackHandlers.Fire(args);
}

inline void FireOnAttack(OrbwalkingActionArgs& args) {
    AttackHandlers.Fire(args);
}

inline void FireAfterAttack(OrbwalkingActionArgs& args) {
    AfterAttackHandlers.Fire(args);
}

inline void FireBeforeMove(OrbwalkingActionArgs& args) {
    BeforeMoveHandlers.Fire(args);
}

inline void FireNonKillableMinion(OrbwalkingActionArgs& args) {
    NonKillableMinionHandlers.Fire(args);
}

} // namespace OrbwalkingDetail

class OrbwalkerBase : public IOrbwalker {
public:
    explicit OrbwalkerBase(Menu* parentMenu) {
        InitializeMenu(parentMenu);
        InitializeChampionFlags();
        RegisterEvents();
    }

    ~OrbwalkerBase() override {
        Dispose();
    }

    AttackableUnit ForceTarget() const override { return forceTarget_; }
    void ForceTarget(const AttackableUnit& target) override { forceTarget_ = target; }
    AttackableUnit LastTarget() const override { return lastTarget_; }
    OrbwalkingMode ActiveMode() const override {
        if (activeMode_ != OrbwalkingMode::None) {
            return activeMode_;
        }
        if (KeyActive("Combo") || KeyActive("ComboWithMove")) {
            return OrbwalkingMode::Combo;
        }
        if (KeyActive("Harass")) {
            return OrbwalkingMode::Harass;
        }
        if (KeyActive("LaneClear")) {
            return OrbwalkingMode::LaneClear;
        }
        if (KeyActive("LastHit")) {
            return OrbwalkingMode::LastHit;
        }
        if (KeyActive("Flee")) {
            return OrbwalkingMode::Flee;
        }
        return OrbwalkingMode::None;
    }

    int LastAutoAttackTick() const override { return lastAutoAttackTick_; }
    void LastAutoAttackTick(int value) override { lastAutoAttackTick_ = value; }
    int LastMovementTick() const override { return lastMovementTick_; }
    void LastMovementTick(int value) override { lastMovementTick_ = value; }
    bool AttackEnabled() const override { return attackEnabled_; }
    void AttackEnabled(bool value) override { attackEnabled_ = value; }
    bool MoveEnabled() const override { return moveEnabled_; }
    void MoveEnabled(bool value) override { moveEnabled_ = value; }
    void SetOrbwalkerPosition(const Vector3& position) override { orbwalkerPosition_ = position; }

    void SetPauseTime(int time) override { allPauseTick_ = Tick() + std::max(0, time); }
    void SetServerPauseTime(int time) override { allPauseTick_ = Tick() + std::max(0, time - Game::Ping() / 2); }
    void SetAttackPauseTime(int time) override { attackPauseTick_ = Tick() + std::max(0, time); }
    void SetAttackServerPauseTime(int time) override { attackPauseTick_ = Tick() + std::max(0, time - Game::Ping() / 2); }
    void SetMovePauseTime(int time) override { movePauseTick_ = Tick() + std::max(0, time); }
    void SetMoveServerPauseTime(int time) override { movePauseTick_ = Tick() + std::max(0, time - Game::Ping() / 2); }

    // ── Extended timing API (derived from the legacy attack-tick state) ──
    bool IsWindingUp() override {
        const auto player = GameObjects::Player();
        if (!initialized_ || !player.IsValid() || player.IsDead()) {
            return false;
        }
        const int last = std::max(lastAutoAttackTick_, lastLocalAttackTick_);
        if (last <= 0) {
            return false;
        }
        const float windup = GetAttackCastDelay() * 1000.0f;
        const float safetyBuffer = std::clamp(windup * 0.08f, 20.0f, 45.0f);
        const float animGap = lastAttackOrderToAnimGapMs_ > 0
            ? static_cast<float>(lastAttackOrderToAnimGapMs_)
            : 0.0f;
        const float serverNow = static_cast<float>(Tick()) + static_cast<float>(Game::Ping()) / 2.0f;
        return serverNow < static_cast<float>(last) + animGap + windup + safetyBuffer;
    }

    bool IsAutoAttacking() override { return IsWindingUp(); }

    bool IsAttackCastComplete() override {
        return lastAutoAttackTick_ > 0 && !IsWindingUp();
    }

    int AttackCastDelayRemaining() override {
        const int last = std::max(lastAutoAttackTick_, lastLocalAttackTick_);
        if (last <= 0 || !IsWindingUp()) {
            return 0;
        }
        const int readyAt = last +
            std::max(0, lastAttackOrderToAnimGapMs_) +
            static_cast<int>(GetAttackCastDelay() * 1000.0f);
        return std::max(0, readyAt - Tick());
    }

    int NextAttackReadyTick() override {
        const int last = std::max(lastAutoAttackTick_, lastLocalAttackTick_);
        if (last <= 0) {
            return 0;
        }
        return std::max(
            std::max(allPauseTick_, attackPauseTick_),
            last + static_cast<int>(GetAttackDelay() * 1000.0f));
    }

    int AttackCooldownRemaining() override {
        const int readyTick = NextAttackReadyTick();
        return readyTick > 0 ? std::max(0, readyTick - Tick()) : 0;
    }

    // ── Suspend/Resume: dùng khi một orbwalker khác override bản SDK ──
    void Suspend() override {
        Dispose();
        if (rootMenu_) {
            rootMenu_->Visible = false;
        }
    }

    void Resume() override {
        RegisterEvents();
        if (rootMenu_) {
            rootMenu_->Visible = true;
        }
    }

    bool CanAttack() override { return CanAttack(0.0f); }

    bool CanAttack(float extraWindup) override {
        if (!initialized_) {
            return false;
        }

        const auto player = GameObjects::Player();
        if (!player.IsValid() || player.IsDead()) {
            return false;
        }

        const int now = Tick();
        if ((allPauseTick_ > 0 && now < allPauseTick_) ||
            (attackPauseTick_ > 0 && now < attackPauseTick_)) {
            return false;
        }

        if (!PlayerCanAttack(player) ||
            player.HasBuff("tahmkenchwdevoured") ||
            CoreBuffs::HasBuffType(player.Address(), kBuffFear) ||
            CoreBuffs::HasBuffType(player.Address(), kBuffPolymorph) ||
            player.HasBuff("Polymorph")) {
            return false;
        }

        if (!isKalista_ && CoreBuffs::HasBuffType(player.Address(), kBuffBlind)) {
            return false;
        }

        float attackDelay = GetAttackDelay() * 1000.0f;
        if (isGraves_) {
            if (!player.HasBuff("gravesbasicattackammo1")) {
                return false;
            }
            attackDelay = attackDelay * 1.0740297f - 716.2381f;
        }
        if (isSett_ && nextAttackIsPassive_) {
            attackDelay /= 8.0f;
        }

        if (attackOrderPending_ && lastLocalAttackTick_ > 0) {
            const float pendingWindow = std::clamp(attackDelay, 180.0f, 650.0f);
            if (static_cast<float>(now - lastLocalAttackTick_) < pendingWindow) {
                return false;
            }
            attackOrderPending_ = false;
        }

        if (player.HasBuff("rengarq") || player.HasBuff("rengarqemp")) {
            return true;
        }

        if (isAphelios_ && player.HasBuff("apheliospreload")) {
            return false;
        }
        if (isJhin_ && player.HasBuff("JhinPassiveReload")) {
            return false;
        }

        return static_cast<float>(now) + (static_cast<float>(Game::Ping()) / 2.0f) + 25.0f >=
               static_cast<float>(lastAutoAttackTick_) + attackDelay + extraWindup;
    }

    bool CanMove() override { return CanMove(0.0f, false); }

    bool CanMove(float extraWindup, bool disableMissileCheck) override {
        if (!initialized_) {
            return false;
        }

        const auto player = GameObjects::Player();
        if (!player.IsValid() || player.IsDead()) {
            return false;
        }

        if (ActiveMode() == OrbwalkingMode::Flee) {
            return true;
        }

        const int now = Tick();
        if ((allPauseTick_ > 0 && now < allPauseTick_) ||
            (movePauseTick_ > 0 && now < movePauseTick_)) {
            AADebugLogCanMove(false, "pause", 0.0f, 0.0f, extraWindup, disableMissileCheck);
            return false;
        }

        if (player.HasBuff("tahmkenchwdevoured")) {
            AADebugLogCanMove(false, "tahm-devoured", 0.0f, 0.0f, extraWindup, disableMissileCheck);
            return false;
        }

        if (isKalista_) {
            AADebugLogCanMove(true, "kalista", 0.0f, 0.0f, extraWindup, disableMissileCheck);
            return true;
        }

        const int rengarExtra = (player.HasBuff("rengarq") || player.HasBuff("rengarqemp")) ? 200 : 0;
        if (lastAutoAttackTick_ <= 0) {
            AADebugLogCanMove(true, "no-last-aa", 0.0f, 0.0f, extraWindup, disableMissileCheck);
            return true;
        }

        // NOTE: we intentionally do NOT early-return true just because
        // missileLaunched_ is set. OnDoCast (which flips missileLaunched_)
        // fires when the server registers the cast starting, not when the
        // client-side windup animation has actually finished playing.
        // At low attack speed (e.g. level 1, before any AS growth/items)
        // the windup is long in absolute ms, so that gap becomes visible:
        // Move() would fire mid-animation and cancel/restart the raise.
        // Gate strictly on the computed windup instead, with a proportional
        // buffer since the desync seems to scale with windup length.
        const float windup = GetAttackCastDelay() * 1000.0f;
        if (attackOrderPending_ && lastLocalAttackTick_ > 0) {
            const float attackDelay = std::max(350.0f, GetAttackDelay() * 1000.0f);
            const float pendingMax = std::max(250.0f, std::min(850.0f, attackDelay - 80.0f));
            const float pendingWanted = std::max(420.0f, windup + extraWindup + 220.0f);
            const float pendingWindow = std::min(std::max(pendingWanted, 250.0f), pendingMax);
            const float pendingElapsed = static_cast<float>(now - lastLocalAttackTick_);
            if (pendingElapsed < pendingWindow) {
                AADebugLogCanMove(
                    false,
                    "pending-before-process",
                    pendingWindow - pendingElapsed,
                    windup,
                    extraWindup,
                    disableMissileCheck);
                return false;
            }
            attackOrderPending_ = false;
            AADebugAppend(
                "[AADebug] PENDING_TIMEOUT id=%d elapsed=%.1f window=%.1f windup=%.1f extra=%.1f",
                aaDebugAttackId_,
                pendingElapsed,
                pendingWindow,
                windup,
                extraWindup);
        }

        // IMPORTANT: OnDoCast (attackDamageIssued_) fires at the cast START, not
        // when the windup animation finishes and the projectile launches. Moving
        // between those two moments STILL cancels the auto-attack. So we must NOT
        // early-return on attackDamageIssued_ — we gate on the computed windup
        // (AttackCastDelay) elapsing, which is the real point where the shot is
        // guaranteed out. We keep the wait as tight as possible (windup + a small
        // desync safety) so movement is responsive without cancelling the shot.
        const float safetyBuffer = std::clamp(windup * 0.08f, 20.0f, 45.0f);
        const float animGap = lastAttackOrderToAnimGapMs_ > 0
            ? static_cast<float>(lastAttackOrderToAnimGapMs_)
            : 0.0f;
        const float readyAt = static_cast<float>(lastAutoAttackTick_) +
            animGap + windup + extraWindup + safetyBuffer + static_cast<float>(rengarExtra);
        const float serverNow = static_cast<float>(now) + (static_cast<float>(Game::Ping()) / 2.0f);

        const bool ready = serverNow >= readyAt;
        AADebugLogCanMove(
            ready,
            ready ? "windup-ready" : "windup",
            std::max(0.0f, readyAt - serverNow),
            windup,
            extraWindup,
            disableMissileCheck);
        return ready;
    }

    bool Attack(const AttackableUnit& target) override {
        OrbwalkerDropFpsScope perf(this, "Attack");
        if (!initialized_) {
            FarmDebugLogState(FarmDebugSlot::Attack, "attack-blocked", target, "not-initialized", true);
            return false;
        }

        const auto player = GameObjects::Player();
        if (!player.IsValid()) {
            FarmDebugLogState(FarmDebugSlot::Attack, "attack-blocked", target, "player-invalid", true);
            return false;
        }
        if (player.IsDead()) {
            FarmDebugLogState(FarmDebugSlot::Attack, "attack-blocked", target, "player-dead", true);
            return false;
        }
        if (player.Spellbook().IsWindingUp()) {
            FarmDebugLogState(FarmDebugSlot::Attack, "attack-blocked", target, "spellbook-winding-up", true);
            return false;
        }

        if (!target.IsValid()) {
            FarmDebugLogState(FarmDebugSlot::Attack, "attack-blocked", target, "target-invalid", true);
            return false;
        }

        if (!OrbwalkingDetail::IsValidAttackTarget(target, GetAutoAttackRange(target))) {
            FarmDebugLogState(FarmDebugSlot::Attack, "attack-blocked", target, "invalid-attack-target/range-filter", true);
            return false;
        }

        if (!CanAttackWithWindWall(target)) {
            FarmDebugLogState(FarmDebugSlot::Attack, "attack-blocked", target, "windwall-or-special-filter", true);
            return false;
        }

        OrbwalkingActionArgs args(OrbwalkingType::BeforeAttack, target, {}, "SDK");
        OrbwalkingDetail::FireBeforeAttack(args);
        if (!args.Process) {
            FarmDebugLogState(FarmDebugSlot::Attack, "attack-blocked", target, "before-attack-cancelled", true);
            return false;
        }

        const int now = Tick();
        if (lastAttackOrderTick_ > 0 &&
            now - lastAttackOrderTick_ < 80 &&
            target.NetworkId() == lastAttackOrderTargetNetworkId_) {
            FarmDebugLogState(FarmDebugSlot::Attack, "attack-blocked", target, "duplicate-attack-order-throttle", true);
            return false;
        }

        if (isKalista_) {
            missileLaunched_ = false;
        }

        if (CoreControl::IssueAttack(target.Address(), target.Position(), true)) {
            ++aaDebugAttackId_;
            lastLocalAttackTick_ = now;
            lastAutoAttackTick_ = now - Game::Ping() / 2;
            lastAttackOrderTick_ = now;
            lastAttackOrderTargetNetworkId_ = target.NetworkId();
            lastAttackOrderToAnimGapMs_ = 0;
            lastAfterAttackStartTick_ = 0;
            attackOrderPending_ = true;
            lastTarget_ = target;
            missileLaunched_ = false;
            attackDamageIssued_ = false;
            lastAutoAttackEventTick_ = 0;
            aaDebugCanMoveInitialized_ = false;
            AADebugAppend(
                "[AADebug] ISSUE_ATTACK id=%d target=%s name='%s' net=%d dist=%.1f range=%.1f delay=%.1f windup=%.1f ping=%d localAtk=%d lastAA=%d",
                aaDebugAttackId_,
                TargetKind(target),
                target.CharacterName().c_str(),
                target.NetworkId(),
                player.Distance(target),
                GetAutoAttackRange(target),
                GetAttackDelay() * 1000.0f,
                GetAttackCastDelay() * 1000.0f,
                Game::Ping(),
                lastLocalAttackTick_,
                lastAutoAttackTick_);
            FarmDebugLogState(FarmDebugSlot::Attack, "attack-ok", target, "issue-attack-ok", true);
            perf.Finish();
            TryShowFakeClick(Hud::ClickType::Attack, target.Position(), now, lastFakeAttackClickTick_);
            return true;
        }
        AADebugAppend(
            "[AADebug] ISSUE_ATTACK_FAIL id=%d targetValid=%d targetNet=%d dist=%.1f canAttack=%d",
            aaDebugAttackId_,
            target.IsValid() ? 1 : 0,
            target.IsValid() ? target.NetworkId() : 0,
            target.IsValid() ? player.Distance(target) : -1.0f,
            CanAttack() ? 1 : 0);
        FarmDebugLogState(FarmDebugSlot::Attack, "attack-failed", target, "issue-attack-failed", true);
        return false;
    }

    void Move(const Vector3& position) override {
        OrbwalkerDropFpsScope perf(this, "Move");
        if (!initialized_ || !position.IsValid() || position.IsZero()) {
            AADebugLogMoveBlocked("not-initialized-or-invalid-position", position, position, 0, -1.0f, -1.0f, 0.0f);
            FarmDebugLogState(FarmDebugSlot::Move, "move-blocked", {}, "not-initialized-or-invalid-position", true);
            return;
        }

        const auto player = GameObjects::Player();
        if (!player.IsValid() || player.IsDead()) {
            AADebugLogMoveBlocked("player-invalid-or-dead", position, position, 0, -1.0f, -1.0f, 0.0f);
            FarmDebugLogState(FarmDebugSlot::Move, "move-blocked", {}, "player-invalid-or-dead", true);
            return;
        }

        Vector3 movePosition = position;
        const int now = Tick();
        const int extraHold = std::max(30, Slider(orbwalkerMenu_, "ExtraHold", 50));

        if (player.ServerPosition().DistanceSqr2D(movePosition) <=
            static_cast<float>(extraHold * extraHold)) {
            if (!player.Path().empty()) {
                lastMovementTick_ = now - 70;
            }
            AADebugLogMoveBlocked(
                "inside-extra-hold-position",
                position,
                movePosition,
                now - lastMovementTick_,
                lastMoveOrderPosition_.IsValid() && !lastMoveOrderPosition_.IsZero()
                    ? lastMoveOrderPosition_.Distance2D(movePosition)
                    : FLT_MAX,
                player.ServerPosition().Distance2D(movePosition),
                0.0f);
            FarmDebugLogState(FarmDebugSlot::Move, "move-blocked", {}, "inside-extra-hold-position", true);
            return;
        }

        if (Bool(orbwalkerMenu_, "MoveRandom", false) &&
            player.ServerPosition().Distance2D(movePosition) < 150.0f) {
            const float randomScale = RandomFloat(0.6f, 1.0f) + 0.2f;
            movePosition = player.ServerPosition().Extend(movePosition, randomScale * 400.0f);
        }

        const auto path = player.GetWaypoints();
        float angle = 0.0f;
        if (path.size() > 1) {
            angle = (path[1] - player.ServerPosition()).AngleBetween(movePosition - player.ServerPosition());
        }

        Vector3 currentDestination = {};
        const auto ai = player.AiManagerSnapshot();
        if (ai.targetPosition.IsValid() && !ai.targetPosition.IsZero()) {
            currentDestination = ai.targetPosition;
        } else if (!path.empty()) {
            currentDestination = path.back();
        }

        const float cursorDelta = lastMoveOrderPosition_.IsValid() && !lastMoveOrderPosition_.IsZero()
            ? lastMoveOrderPosition_.Distance2D(movePosition)
            : FLT_MAX;
        const float destinationDelta = currentDestination.IsValid() && !currentDestination.IsZero()
            ? currentDestination.Distance2D(movePosition)
            : FLT_MAX;
        const int elapsedMove = now - lastMovementTick_;
        const int hardMoveDelay = 75;
        const int softMoveDelay = std::clamp(100 + Game::Ping() / 2, 110, 150);

        if (lastMovementTick_ > 0 && elapsedMove < hardMoveDelay) {
            AADebugLogMoveBlocked("hard-move-delay", position, movePosition, elapsedMove, cursorDelta, destinationDelta, angle);
            FarmDebugLogState(FarmDebugSlot::Move, "move-blocked", {}, "hard-move-delay", true);
            return;
        }
        if (lastMovementTick_ > 0 &&
            elapsedMove < softMoveDelay &&
            cursorDelta < 140.0f &&
            angle < 65.0f) {
            AADebugLogMoveBlocked("soft-move-delay", position, movePosition, elapsedMove, cursorDelta, destinationDelta, angle);
            FarmDebugLogState(FarmDebugSlot::Move, "move-blocked", {}, "soft-move-delay", true);
            return;
        }
        if (lastMovementTick_ > 0 &&
            destinationDelta < 35.0f &&
            cursorDelta < 70.0f &&
            elapsedMove < 220) {
            AADebugLogMoveBlocked("destination-already-close", position, movePosition, elapsedMove, cursorDelta, destinationDelta, angle);
            FarmDebugLogState(FarmDebugSlot::Move, "move-blocked", {}, "destination-already-close", true);
            return;
        }

        OrbwalkingActionArgs args(OrbwalkingType::Movement, {}, movePosition, "SDK");
        OrbwalkingDetail::FireBeforeMove(args);
        if (!args.Process) {
            AADebugLogMoveBlocked("before-move-cancelled", position, movePosition, elapsedMove, cursorDelta, destinationDelta, angle);
            FarmDebugLogState(FarmDebugSlot::Move, "move-blocked", {}, "before-move-cancelled", true);
            return;
        }

        if (CoreControl::IssueMove(args.Position, true)) {
            lastMovementTick_ = now;
            lastMoveOrderPosition_ = args.Position;
            AADebugAppend(
                "[AADebug] ISSUE_MOVE id=%d dtAA=%d dtLocal=%d pending=%d missile=%d damageIssued=%d pos=(%.1f,%.1f,%.1f)",
                aaDebugAttackId_,
                now - lastAutoAttackTick_,
                now - lastLocalAttackTick_,
                attackOrderPending_ ? 1 : 0,
                missileLaunched_ ? 1 : 0,
                attackDamageIssued_ ? 1 : 0,
                args.Position.x,
                args.Position.y,
                args.Position.z);
            FarmDebugLogState(FarmDebugSlot::Move, "move-ok", {}, "issue-move-ok", true);
            perf.Finish();
            TryShowFakeClick(Hud::ClickType::Move, args.Position, now, lastFakeMoveClickTick_);
        } else {
            AADebugAppend(
                "[AADebug] ISSUE_MOVE_FAIL id=%d dtAA=%d dtLocal=%d pending=%d missile=%d damageIssued=%d",
                aaDebugAttackId_,
                now - lastAutoAttackTick_,
                now - lastLocalAttackTick_,
                attackOrderPending_ ? 1 : 0,
                missileLaunched_ ? 1 : 0,
                attackDamageIssued_ ? 1 : 0);
            FarmDebugLogState(FarmDebugSlot::Move, "move-failed", {}, "issue-move-failed", true);
        }
    }

    void Orbwalk(const AttackableUnit& target, const Vector3& position = {}) override {
        if (!initialized_) {
            FarmDebugLogState(FarmDebugSlot::Orbwalk, "orbwalk-blocked", target, "not-initialized", true);
            return;
        }

        const int now = Tick();
        const auto player = GameObjects::Player();
        const bool isFleeMode = (ActiveMode() == OrbwalkingMode::Flee);
        // Gate the move for the duration of the attack windup (AttackCastDelay).
        // OnDoCast/attackDamageIssued_ fires at cast START, before the windup
        // finishes, so moving during this window still cancels the shot — we must
        // wait out the windup itself, not just until the cast begins.
        const bool attackWindupGate =
            !isFleeMode &&
            player.IsValid() &&
            lastLocalAttackTick_ > 0 &&
            static_cast<float>(now - lastLocalAttackTick_) <
                GetAttackCastDelay() * 1000.0f +
                    static_cast<float>(std::max(0, lastAttackOrderToAnimGapMs_)) +
                    static_cast<float>(Game::Ping()) / 4.0f;

        bool attackBlocked = true;
        const bool canAttackNow = attackEnabled_ && CanAttack();
        if (!attackEnabled_) {
            FarmDebugLogState(FarmDebugSlot::Orbwalk, "orbwalk-attack-blocked", target, "attack-disabled", true);
        } else if (!canAttackNow) {
            FarmDebugLogState(FarmDebugSlot::Orbwalk, "orbwalk-attack-blocked", target, "can-attack-false", true);
        } else if (!target.IsValid()) {
            FarmDebugLogState(FarmDebugSlot::Orbwalk, "orbwalk-attack-blocked", target, "no-target", true);
        } else {
            attackBlocked = !Attack(target);
            FarmDebugLogState(
                FarmDebugSlot::Orbwalk,
                attackBlocked ? "orbwalk-attack-blocked" : "orbwalk-attack-ok",
                target,
                attackBlocked ? "attack-returned-false" : "attack-issued",
                attackBlocked);
        }

        const bool canMoveNow =
            moveEnabled_ &&
            CanMove(static_cast<float>(Slider(orbwalkerMenu_, "WindupDelay", 100)), false);
        const bool comboWithMove = KeyActive("ComboWithMove");
        const bool limitAttackOk =
            !Bool(orbwalkerMenu_, "LimitAttack", false) ||
            GetAttackDelay() >= 0.3846154f ||
            (autoAttackCounter_ % 3) == 0 ||
            CanMove(500.0f, true);

        if (!attackBlocked) {
            return;
        }

        if (attackWindupGate) {
            FarmDebugLogState(FarmDebugSlot::Orbwalk, "orbwalk-move-blocked", target, "attack-windup-gate", true);
            return;
        }
        if (!moveEnabled_) {
            FarmDebugLogState(FarmDebugSlot::Orbwalk, "orbwalk-move-blocked", target, "move-disabled", true);
            return;
        }
        if (!canMoveNow) {
            FarmDebugLogState(FarmDebugSlot::Orbwalk, "orbwalk-move-blocked", target, "can-move-false", true);
            return;
        }
        if (comboWithMove) {
            FarmDebugLogState(FarmDebugSlot::Orbwalk, "orbwalk-move-blocked", target, "combo-without-move-key", true);
            return;
        }
        if (!limitAttackOk) {
            FarmDebugLogState(FarmDebugSlot::Orbwalk, "orbwalk-move-blocked", target, "limit-attack-high-as", true);
            return;
        }

        if (!attackWindupGate && attackBlocked && canMoveNow && !comboWithMove && limitAttackOk) {
            const Vector3 movePosition = position.IsValid() && !position.IsZero()
                ? position
                : Game::CursorPos();
            FarmDebugLogState(FarmDebugSlot::Orbwalk, "orbwalk-move", target, "calling-move", true);
            Move(movePosition);
        }
    }

    void ResetAutoAttackTimer() override {
        lastAutoAttackTick_ = 0;
        lastLocalAttackTick_ = 0;
        lastAutoAttackEventTick_ = 0;
        lastAttackOrderToAnimGapMs_ = 0;
        lastAfterAttackStartTick_ = 0;
        attackOrderPending_ = false;
        missileLaunched_ = true;
        attackDamageIssued_ = true;
    }

    void Dispose() override {
        if (!eventsRegistered_) {
            return;
        }
        eventsRegistered_ = false;
        Events::hook.OnGameUpdate -= &OrbwalkerBase::OnGameUpdateStatic;
        Events::hook.OnProcessSpell -= &OrbwalkerBase::OnProcessSpellStatic;
        Events::hook.OnDoCast -= &OrbwalkerBase::OnDoCastStatic;
        Events::hook.OnStopCast -= &OrbwalkerBase::OnStopCastStatic;
        Events::hook.OnPlayAnimation -= &OrbwalkerBase::OnPlayAnimationStatic;
        Events::hook.OnBuffAdd -= &OrbwalkerBase::OnBuffAddStatic;
        Events::hook.OnDeleteObject -= &OrbwalkerBase::OnDeleteStatic;
        Events::hook.OnMissileCreate -= &OrbwalkerBase::OnMissileCreateStatic;
        Events::hook.OnMissileDelete -= &OrbwalkerBase::OnMissileDeleteStatic;
        Drawing::RemoveOnDraw(&OrbwalkerBase::OnDrawStatic);
        Drawing::RemoveOnAlwaysDraw(&OrbwalkerBase::OnAlwaysDrawStatic);
        if (fakeCursorTexture_.Texture) {
            UI::Icons::ReleaseTexture(fakeCursorTexture_);
        }
        fakeCursorTextureLoadTried_ = false;
        fakeCursorTexturePath_.clear();
        if (OrbwalkingDetail::RuntimeInstance == this) {
            OrbwalkingDetail::RuntimeInstance = nullptr;
        }
    }

    static bool IsAutoAttack(std::string name) {
        name = OrbwalkingDetail::ToLower(std::move(name));
        return ((name.find("attack") != std::string::npos &&
                 !OrbwalkingDetail::Contains(NoAttacks(), name)) ||
                OrbwalkingDetail::Contains(Attacks(), name));
    }

    static bool IsAutoAttackReset(std::string name) {
        name = OrbwalkingDetail::ToLower(std::move(name));
        return OrbwalkingDetail::Contains(AttackResets(), name);
    }

    struct AutoAttackResetSlotEntry {
        const char* ChampionName;
        SpellSlot Slot;
    };

    static bool IsKnownAutoAttackResetSlot(const std::string& championName, int slot) {
        if (championName.empty()) {
            return false;
        }
        static constexpr AutoAttackResetSlotEntry entries[] = {
            { "Aatrox", SpellSlot::E },
            { "Ashe", SpellSlot::Q },
            { "Belveth", SpellSlot::Q },
            { "Blitzcrank", SpellSlot::E },
            { "Briar", SpellSlot::Q },
            { "Briar", SpellSlot::W },
            { "Camille", SpellSlot::Q },
            { "Chogath", SpellSlot::E },
            { "Darius", SpellSlot::W },
            { "DrMundo", SpellSlot::E },
            { "Ekko", SpellSlot::E },
            { "Fiora", SpellSlot::E },
            { "Fizz", SpellSlot::W },
            { "Garen", SpellSlot::Q },
            { "Graves", SpellSlot::E },
            { "Gwen", SpellSlot::E },
            { "Hecarim", SpellSlot::E },
            { "Illaoi", SpellSlot::W },
            { "Jax", SpellSlot::W },
            { "Kassadin", SpellSlot::W },
            { "Katarina", SpellSlot::E },
            { "Kayle", SpellSlot::E },
            { "Kindred", SpellSlot::Q },
            { "Leona", SpellSlot::Q },
            { "Lucian", SpellSlot::Q },
            { "Lucian", SpellSlot::W },
            { "Lucian", SpellSlot::E },
            { "Lucian", SpellSlot::R },
            { "Malphite", SpellSlot::W },
            { "MasterYi", SpellSlot::W },
            { "MonkeyKing", SpellSlot::Q },
            { "Nasus", SpellSlot::Q },
            { "Nautilus", SpellSlot::W },
            { "Nilah", SpellSlot::E },
            { "Olaf", SpellSlot::W },
            { "Pantheon", SpellSlot::W },
            { "Kaisa", SpellSlot::R },
            { "Quinn", SpellSlot::E },
            { "RekSai", SpellSlot::Q },
            { "Rell", SpellSlot::W },
            { "Renekton", SpellSlot::W },
            { "Rengar", SpellSlot::Q },
            { "Riven", SpellSlot::Q },
            { "Sejuani", SpellSlot::E },
            { "Sett", SpellSlot::Q },
            { "Shyvana", SpellSlot::Q },
            { "Sivir", SpellSlot::W },
            { "Talon", SpellSlot::Q },
            { "Trundle", SpellSlot::Q },
            { "Vayne", SpellSlot::Q },
            { "Vi", SpellSlot::E },
            { "Viego", SpellSlot::W },
            { "Volibear", SpellSlot::Q },
            { "XinZhao", SpellSlot::Q },
            { "Yorick", SpellSlot::Q },
            { "Zac", SpellSlot::Q },
            { "Zeri", SpellSlot::E },
            { "Zoe", SpellSlot::R },
        };
        for (const auto& entry : entries) {
            if (slot == static_cast<int>(entry.Slot) &&
                _stricmp(championName.c_str(), entry.ChampionName) == 0) {
                return true;
            }
        }
        return false;
    }

    bool IsLocalAutoAttackResetSlot(const ::Core::Events::ObjectInfo& sender, int slot) const {
        if (!OrbwalkingDetail::IsLocalPlayer(sender)) {
            return false;
        }
        std::string championName;
        const auto player = GameObjects::Player();
        if (player.IsValid()) {
            championName = player.CharacterName();
        }
        if (championName.empty()) {
            championName = sender.CharacterName;
        }
        return IsKnownAutoAttackResetSlot(championName, slot);
    }

protected:
    static constexpr bool kFarmDebugEnabled = false;
    static constexpr bool kAADebugEnabled = false;
    static constexpr const char* kAADebugPath =
        "C:\\Users\\Public\\ns_orbwalker_aa_debug.txt";
    static constexpr bool kFakeClickDropFpsDebugEnabled = false;
    static constexpr bool kFakeCursorDropFpsDebugEnabled = false;
    static constexpr const char* kFakeClickDropFpsDebugPath =
        "C:\\Users\\Public\\ns_orbwalker_fake_click_dropfps.txt";
    static constexpr const char* kFakeCursorDropFpsDebugPath =
        "C:\\Users\\Public\\ns_orbwalker_fake_cursor_dropfps.txt";
    static constexpr bool kOrbwalkerDropFpsDebugEnabled = false;
    static constexpr const char* kOrbwalkerDropFpsDebugPath =
        "C:\\Users\\Public\\ns_orbwalker_dropfps.txt";

    enum class FarmDebugSlot {
        Frame,
        Scan,
        Select,
        Orbwalk,
        Attack,
        Move,
    };

    struct DropFpsDebugStats {
        int WindowStartTick = 0;
        int LastSlowLogTick = 0;
        int Hits = 0;
        int SlowHits = 0;
        double TotalMs = 0.0;
        double MaxMs = 0.0;
        char MaxPhase[48] = {};
    };

    struct KillableDrawCircle {
        AttackableUnit Target = {};
        float RadiusPadding = 25.0f;
    };

    static LARGE_INTEGER DropFpsNow() {
        static LARGE_INTEGER frequency = {};
        if (frequency.QuadPart == 0) {
            QueryPerformanceFrequency(&frequency);
        }

        LARGE_INTEGER now = {};
        QueryPerformanceCounter(&now);
        return now;
    }

    static double DropFpsMsSince(const LARGE_INTEGER& start) {
        static LARGE_INTEGER frequency = {};
        if (frequency.QuadPart == 0) {
            QueryPerformanceFrequency(&frequency);
        }

        LARGE_INTEGER now = {};
        QueryPerformanceCounter(&now);
        return static_cast<double>(now.QuadPart - start.QuadPart) * 1000.0 /
            static_cast<double>(frequency.QuadPart);
    }

    static void DropFpsAppend(const char* path, const char* text) {
        if (!path || !path[0] || !text || !text[0]) {
            return;
        }

        HANDLE file = CreateFileA(
            path,
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return;
        }

        DWORD written = 0;
        WriteFile(file, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
        CloseHandle(file);
    }

    static bool FileExists(const std::string& path) {
        if (path.empty()) {
            return false;
        }
        const DWORD attr = GetFileAttributesA(path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    static std::string DirectoryOf(std::string path) {
        const auto slash = path.find_last_of("\\/");
        return slash == std::string::npos ? std::string() : path.substr(0, slash);
    }

    static std::string FullPath(const std::string& path) {
        char full[MAX_PATH] = {};
        const DWORD len = GetFullPathNameA(path.c_str(), MAX_PATH, full, nullptr);
        if (len == 0 || len >= MAX_PATH) {
            return path;
        }
        return std::string(full, full + len);
    }

    static std::string ModuleDirectory() {
        HMODULE module = Variables::Detail::CurrentModule();
        char path[MAX_PATH] = {};
        if (!module || GetModuleFileNameA(module, path, MAX_PATH) == 0) {
            return {};
        }
        return DirectoryOf(path);
    }

    static std::string HostDirectory() {
        char path[MAX_PATH] = {};
        if (GetModuleFileNameA(nullptr, path, MAX_PATH) == 0) {
            return {};
        }
        return DirectoryOf(path);
    }

    static std::string ResolveHandCursorPath() {
        const std::string moduleDir = ModuleDirectory();
        const std::string hostDir = HostDirectory();

        std::vector<std::string> candidates;
        candidates.push_back("NightSharp\\SDK\\Data\\hand1.png");
        candidates.push_back("C:\\Users\\MR THINH\\Downloads\\New\\NightSharp\\SDK\\Data\\hand1.png");
        if (!moduleDir.empty()) {
            candidates.push_back(moduleDir + "\\..\\..\\SDK\\Data\\hand1.png");
            candidates.push_back(moduleDir + "\\..\\SDK\\Data\\hand1.png");
            candidates.push_back(moduleDir + "\\SDK\\Data\\hand1.png");
            candidates.push_back(moduleDir + "\\data\\hand1.png");
        }
        if (!hostDir.empty()) {
            candidates.push_back(hostDir + "\\NightSharp\\SDK\\Data\\hand1.png");
            candidates.push_back(hostDir + "\\data\\hand1.png");
        }
        candidates.push_back("C:\\NightSharp\\data\\hand1.png");

        for (const auto& candidate : candidates) {
            const std::string full = FullPath(candidate);
            if (FileExists(full)) {
                return full;
            }
        }
        return candidates.empty() ? std::string() : FullPath(candidates.front());
    }

    void RecordDropFpsDebug(DropFpsDebugStats& stats,
                            const char* path,
                            const char* tag,
                            const char* phase,
                            double ms,
                            const char* detail,
                            double slowThresholdMs) {
        if ((!kFakeClickDropFpsDebugEnabled &&
             path && std::strcmp(path, kFakeClickDropFpsDebugPath) == 0) ||
            (!kFakeCursorDropFpsDebugEnabled &&
             path && std::strcmp(path, kFakeCursorDropFpsDebugPath) == 0) ||
            (!kOrbwalkerDropFpsDebugEnabled &&
             path && std::strcmp(path, kOrbwalkerDropFpsDebugPath) == 0)) {
            return;
        }

        const int now = Tick();
        if (stats.WindowStartTick == 0) {
            stats.WindowStartTick = now;
        }

        ++stats.Hits;
        stats.TotalMs += ms;
        if (ms > stats.MaxMs) {
            stats.MaxMs = ms;
            _snprintf_s(stats.MaxPhase, sizeof(stats.MaxPhase), _TRUNCATE, "%s", phase ? phase : "?");
        }

        if (ms >= slowThresholdMs && now - stats.LastSlowLogTick >= 250) {
            ++stats.SlowHits;
            stats.LastSlowLogTick = now;
            SYSTEMTIME st = {};
            GetLocalTime(&st);
            char line[768] = {};
            _snprintf_s(
                line,
                sizeof(line),
                _TRUNCATE,
                "[%02d:%02d:%02d.%03d tick=%d] [%s] slow phase=%s ms=%.3f detail=%s\r\n",
                st.wHour,
                st.wMinute,
                st.wSecond,
                st.wMilliseconds,
                now,
                tag ? tag : "?",
                phase ? phase : "?",
                ms,
                detail ? detail : "");
            DropFpsAppend(path, line);
        }

        if (now - stats.WindowStartTick >= 1000 && stats.Hits > 0) {
            SYSTEMTIME st = {};
            GetLocalTime(&st);
            char line[768] = {};
            _snprintf_s(
                line,
                sizeof(line),
                _TRUNCATE,
                "[%02d:%02d:%02d.%03d tick=%d] [%s] summary hits=%d slow=%d total=%.3f avg=%.4f max=%.3f maxPhase=%s detail=%s\r\n",
                st.wHour,
                st.wMinute,
                st.wSecond,
                st.wMilliseconds,
                now,
                tag ? tag : "?",
                stats.Hits,
                stats.SlowHits,
                stats.TotalMs,
                stats.TotalMs / static_cast<double>(std::max(1, stats.Hits)),
                stats.MaxMs,
                stats.MaxPhase[0] ? stats.MaxPhase : "?",
                detail ? detail : "");
            DropFpsAppend(path, line);
            stats.WindowStartTick = now;
            stats.Hits = 0;
            stats.SlowHits = 0;
            stats.TotalMs = 0.0;
            stats.MaxMs = 0.0;
            stats.MaxPhase[0] = '\0';
        }
    }

    bool ShouldRecordOrbwalkerDropFps() const {
        if (!kOrbwalkerDropFpsDebugEnabled || !initialized_) {
            return false;
        }
        const OrbwalkingMode mode = ActiveMode();
        return mode != OrbwalkingMode::None ||
            Bool(drawingMenu_, "DrawAttackRange", true) ||
            Bool(drawingMenu_, "DrawHoldPosition", false) ||
            Bool(drawingMenu_, "DrawKillableMinion", false);
    }

    void RecordOrbwalkerDropFps(const char* phase, double ms, const char* detail) {
        if (!ShouldRecordOrbwalkerDropFps()) {
            return;
        }
        RecordDropFpsDebug(
            orbwalkerPerfStats_,
            kOrbwalkerDropFpsDebugPath,
            "Orbwalker",
            phase,
            ms,
            detail,
            0.35);
    }

    class OrbwalkerDropFpsScope {
    public:
        OrbwalkerDropFpsScope(OrbwalkerBase* owner, const char* phase, const char* detail = "")
            : owner_(owner),
              phase_(phase),
              detail_(detail),
              enabled_(owner && owner->ShouldRecordOrbwalkerDropFps()) {
            if (enabled_) {
                start_ = DropFpsNow();
            }
        }

        ~OrbwalkerDropFpsScope() {
            Finish();
        }

        void Finish() {
            if (finished_ || !owner_ || !enabled_) {
                return;
            }
            finished_ = true;
            owner_->RecordOrbwalkerDropFps(phase_, DropFpsMsSince(start_), detail_);
        }

    private:
        OrbwalkerBase* owner_ = nullptr;
        const char* phase_ = "";
        const char* detail_ = "";
        LARGE_INTEGER start_ = {};
        bool enabled_ = false;
        bool finished_ = false;
    };

    static const char* ModeName(OrbwalkingMode mode) {
        switch (mode) {
        case OrbwalkingMode::Combo: return "Combo";
        case OrbwalkingMode::Harass: return "Harass";
        case OrbwalkingMode::LaneClear: return "LaneClear";
        case OrbwalkingMode::LastHit: return "LastHit";
        case OrbwalkingMode::Flee: return "Flee";
        default: return "None";
        }
    }

    static const char* TargetKind(const AttackableUnit& target) {
        if (!target.IsValid()) {
            return "none";
        }
        if (target.IsHero()) {
            return "hero";
        }
        if (target.IsMinion()) {
            return target.Team() == GameObjectTeam::Neutral ? "jungle/minion" : "minion";
        }
        if (target.IsTurret()) {
            return "turret";
        }
        return "object";
    }

    void TryShowFakeClick(Hud::ClickType type, const Vector3& position, int now, int& lastTick) {
        const bool fakeClickEnabled = Bool(drawingMenu_, "ShowFakeClick", false);
        const bool fakeCursorEnabled = fakeClickEnabled && Bool(drawingMenu_, "ShowFakeCursor", false);
        if (!fakeClickEnabled) {
            fakeCursorScreenValid_ = false;
            return;
        }

        LARGE_INTEGER perfStart = {};
        if (kFakeClickDropFpsDebugEnabled) {
            perfStart = DropFpsNow();
        }
        const char* typeName = type == Hud::ClickType::Attack ? "attack" : "move";

        if (!position.IsValid() || position.IsZero()) {
            if (kFakeClickDropFpsDebugEnabled) {
                RecordDropFpsDebug(
                    fakeClickPerfStats_,
                    kFakeClickDropFpsDebugPath,
                    "FakeClick",
                    "invalid-position",
                    DropFpsMsSince(perfStart),
                    typeName,
                    0.25);
            }
            return;
        }

        const int minDelay = std::max(0, 250 - Game::Ping() * 10);
        if (now - lastTick <= minDelay) {
            if (kFakeClickDropFpsDebugEnabled) {
                RecordDropFpsDebug(
                    fakeClickPerfStats_,
                    kFakeClickDropFpsDebugPath,
                    "FakeClick",
                    "throttled",
                    DropFpsMsSince(perfStart),
                    typeName,
                    0.25);
            }
            return;
        }

        bool nativeOk = false;
        if (fakeClickEnabled) {
            nativeOk = Hud::ShowClick(type, position);
        }
        if (fakeClickEnabled && !nativeOk) {
            fakeClickPosition_ = position;
            fakeClickExpireTick_ = now + 350;
        }
        if (fakeCursorEnabled) {
            TrackFakeCursorClick(position, now);
        }
        lastTick = now;

        if (kFakeClickDropFpsDebugEnabled) {
            char detail[192] = {};
            _snprintf_s(
                detail,
                sizeof(detail),
                _TRUNCATE,
                "type=%s native=%d pos=(%.1f,%.1f,%.1f)",
                typeName,
                fakeClickEnabled && nativeOk ? 1 : 0,
                position.x,
                position.y,
                position.z);
            RecordDropFpsDebug(
                fakeClickPerfStats_,
                kFakeClickDropFpsDebugPath,
                "FakeClick",
                nativeOk ? "native-show-click" : "fallback-draw",
                DropFpsMsSince(perfStart),
                detail,
                0.25);
        }
    }

    void TrackFakeCursorClick(const Vector3& position, int now) {
        const bool enabled = Bool(drawingMenu_, "ShowFakeClick", false) &&
            Bool(drawingMenu_, "ShowFakeCursor", false);
        if (!enabled) {
            fakeCursorScreenValid_ = false;
            return;
        }

        LARGE_INTEGER perfStart = {};
        if (kFakeCursorDropFpsDebugEnabled) {
            perfStart = DropFpsNow();
        }
        if (!position.IsValid() || position.IsZero()) {
            if (kFakeCursorDropFpsDebugEnabled) {
                RecordDropFpsDebug(
                    fakeCursorPerfStats_,
                    kFakeCursorDropFpsDebugPath,
                    "FakeCursor",
                    "track-invalid-click",
                    DropFpsMsSince(perfStart),
                    "click-invalid",
                    0.20);
            }
            return;
        }

        fakeCursorTargetPosition_ = position;
        fakeCursorVisibleUntilTick_ = now + 350;
        fakeCursorScreenValid_ = false;
        if (kFakeCursorDropFpsDebugEnabled) {
            RecordDropFpsDebug(
                fakeCursorPerfStats_,
                kFakeCursorDropFpsDebugPath,
                "FakeCursor",
                "track-click",
                DropFpsMsSince(perfStart),
                "fake-click-position",
                0.20);
        }
    }

    bool EnsureFakeCursorTexture() {
        if (fakeCursorTexture_.Texture) {
            return true;
        }
        const int now = Tick();
        if (fakeCursorTextureLoadTried_ && now - fakeCursorTextureLastTryTick_ < 1000) {
            return false;
        }

        fakeCursorTextureLoadTried_ = true;
        fakeCursorTextureLastTryTick_ = now;
        LARGE_INTEGER perfStart = {};
        if (kFakeCursorDropFpsDebugEnabled) {
            perfStart = DropFpsNow();
        }
        fakeCursorTexturePath_ = Utils::AssetInstaller::CursorHandPath();
        if (fakeCursorTexturePath_.empty() || !FileExists(fakeCursorTexturePath_)) {
            fakeCursorTexturePath_ = ResolveHandCursorPath();
        }

        const bool ok =
            !fakeCursorTexturePath_.empty() &&
            UI::Icons::LoadTextureFromFile(fakeCursorTexturePath_.c_str(), fakeCursorTexture_);

        if (kFakeCursorDropFpsDebugEnabled) {
            char detail[384] = {};
            _snprintf_s(
                detail,
                sizeof(detail),
                _TRUNCATE,
                "ok=%d size=%dx%d path=%s",
                ok ? 1 : 0,
                fakeCursorTexture_.Width,
                fakeCursorTexture_.Height,
                fakeCursorTexturePath_.c_str());
            RecordDropFpsDebug(
                fakeCursorPerfStats_,
                kFakeCursorDropFpsDebugPath,
                "FakeCursor",
                "texture-load",
                DropFpsMsSince(perfStart),
                detail,
                0.20);
        }
        return ok;
    }

    void DrawFakeCursorFallback(ImDrawList* draw, const Vec2& position, float size) const {
        if (!draw || !position.IsValid()) {
            return;
        }

        const float s = std::clamp(size, 12.0f, 42.0f);
        const ImVec2 tip(position.x, position.y);
        const ImVec2 left(position.x + s * 0.10f, position.y + s * 1.20f);
        const ImVec2 right(position.x + s * 0.62f, position.y + s * 0.82f);
        const ImVec2 innerTip(position.x + s * 0.08f, position.y + s * 0.10f);
        const ImVec2 innerLeft(position.x + s * 0.16f, position.y + s * 0.98f);
        const ImVec2 innerRight(position.x + s * 0.50f, position.y + s * 0.74f);
        const ImVec2 stemStart(position.x + s * 0.30f, position.y + s * 0.82f);
        const ImVec2 stemEnd(position.x + s * 0.56f, position.y + s * 1.36f);

        draw->AddTriangleFilled(tip, left, right, IM_COL32(0, 0, 0, 220));
        draw->AddTriangleFilled(innerTip, innerLeft, innerRight, IM_COL32(255, 255, 255, 245));
        draw->AddTriangle(tip, left, right, IM_COL32(0, 0, 0, 240), 1.4f);
        draw->AddLine(stemStart, stemEnd, IM_COL32(0, 0, 0, 230), 4.0f);
        draw->AddLine(stemStart, stemEnd, IM_COL32(255, 255, 255, 245), 2.0f);
    }

    void DrawFakeCursor() {
        const bool enabled = Bool(drawingMenu_, "ShowFakeClick", false) &&
            Bool(drawingMenu_, "ShowFakeCursor", false);
        if (!enabled ||
            fakeCursorVisibleUntilTick_ <= Tick() ||
            !fakeCursorTargetPosition_.IsValid() ||
            fakeCursorTargetPosition_.IsZero()) {
            fakeCursorScreenValid_ = false;
            return;
        }

        LARGE_INTEGER perfStart = {};
        if (kFakeCursorDropFpsDebugEnabled) {
            perfStart = DropFpsNow();
        }
        Vec2 targetScreen = {};
        if (!Drawing::WorldToScreenAlways(fakeCursorTargetPosition_, targetScreen) ||
            !Drawing::OnScreenAlways(targetScreen)) {
            fakeCursorScreenValid_ = false;
            if (kFakeCursorDropFpsDebugEnabled) {
                RecordDropFpsDebug(
                    fakeCursorPerfStats_,
                    kFakeCursorDropFpsDebugPath,
                    "FakeCursor",
                    "project-offscreen",
                    DropFpsMsSince(perfStart),
                    "world-to-screen-failed",
                    0.20);
            }
            return;
        }

        fakeCursorScreenPosition_ = targetScreen;
        fakeCursorScreenValid_ = true;
        Drawing::MarkCaptureVisibleContent(180);

        auto* draw = Drawing::GetDrawList(true);
        if (!draw) {
            if (kFakeCursorDropFpsDebugEnabled) {
                RecordDropFpsDebug(
                    fakeCursorPerfStats_,
                    kFakeCursorDropFpsDebugPath,
                    "FakeCursor",
                    "missing-draw-list",
                    DropFpsMsSince(perfStart),
                    "draw-list-null",
                    0.20);
            }
            return;
        }

        const float size = std::clamp(static_cast<float>(Slider(drawingMenu_, "FakeCursorSize", 22)), 12.0f, 42.0f);
        const bool textureReady = EnsureFakeCursorTexture();
        if (!textureReady) {
            DrawFakeCursorFallback(draw, fakeCursorScreenPosition_, size);
            if (kFakeCursorDropFpsDebugEnabled) {
                RecordDropFpsDebug(
                    fakeCursorPerfStats_,
                    kFakeCursorDropFpsDebugPath,
                    "FakeCursor",
                    "draw-fallback-texture-missing",
                    DropFpsMsSince(perfStart),
                    fakeCursorTexturePath_.c_str(),
                    0.20);
            }
            return;
        }

        const float scale = size / 22.0f;
        const Vec2 p = fakeCursorScreenPosition_;
        const float width = static_cast<float>(fakeCursorTexture_.Width) * scale;
        const float height = static_cast<float>(fakeCursorTexture_.Height) * scale;
        draw->AddImage(
            fakeCursorTexture_.Texture,
            ImVec2(p.x, p.y),
            ImVec2(p.x + width, p.y + height),
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            IM_COL32(255, 255, 255, 245));

        if (kFakeCursorDropFpsDebugEnabled) {
            char detail[256] = {};
            _snprintf_s(
                detail,
                sizeof(detail),
                _TRUNCATE,
                "texture=%d screen=(%.1f,%.1f) target=(%.1f,%.1f)",
                textureReady ? 1 : 0,
                p.x,
                p.y,
                targetScreen.x,
                targetScreen.y);
            RecordDropFpsDebug(
                fakeCursorPerfStats_,
                kFakeCursorDropFpsDebugPath,
                "FakeCursor",
                "draw",
                DropFpsMsSince(perfStart),
                detail,
                0.20);
        }
    }

    void DrawFakeVisuals() {
        if (Bool(drawingMenu_, "ShowFakeClick", false) &&
            fakeClickExpireTick_ > Tick() &&
            fakeClickPosition_.IsValid() &&
            !fakeClickPosition_.IsZero()) {
            Drawing::DrawCircleAlways(fakeClickPosition_, 65.0f, 0xAA66CCFFu, 1.5f, 48);
            Drawing::DrawCircleAlways(fakeClickPosition_, 14.0f, 0xCCFFFFFFu, 1.25f, 32);
        }

        DrawFakeCursor();
    }

    static unsigned AADebugHash(const char* text) {
        unsigned hash = 2166136261u;
        if (!text) {
            return hash;
        }
        while (*text) {
            hash ^= static_cast<unsigned char>(*text++);
            hash *= 16777619u;
        }
        return hash;
    }

    void AADebugAppend(const char* fmt, ...) const {
        if (!kAADebugEnabled || !fmt) {
            return;
        }

        char body[1800] = {};
        va_list args;
        va_start(args, fmt);
        _vsnprintf_s(body, sizeof(body), _TRUNCATE, fmt, args);
        va_end(args);

        SYSTEMTIME st = {};
        GetLocalTime(&st);
        char line[2048] = {};
        _snprintf_s(
            line,
            sizeof(line),
            _TRUNCATE,
            "[%02u:%02u:%02u.%03u tick=%d] %s\r\n",
            static_cast<unsigned>(st.wHour),
            static_cast<unsigned>(st.wMinute),
            static_cast<unsigned>(st.wSecond),
            static_cast<unsigned>(st.wMilliseconds),
            Tick(),
            body);

        HANDLE file = CreateFileA(
            kAADebugPath,
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return;
        }

        DWORD written = 0;
        WriteFile(file, line, static_cast<DWORD>(lstrlenA(line)), &written, nullptr);
        CloseHandle(file);
    }

    void AADebugResetLog() const {
        if (!kAADebugEnabled) {
            return;
        }
        DeleteFileA(kAADebugPath);
        AADebugAppend("[AADebug] begin local-player AA timeline");
    }

    void AADebugLogCanMove(bool value,
                           const char* reason,
                           float readyIn,
                           float windup,
                           float extraWindup,
                           bool disableMissileCheck) const {
        if (!kAADebugEnabled) {
            return;
        }

        const int now = Tick();
        const unsigned reasonHash = AADebugHash(reason);
        if (aaDebugCanMoveInitialized_ &&
            value == aaDebugLastCanMoveValue_ &&
            reasonHash == aaDebugLastCanMoveReasonHash_ &&
            now - aaDebugLastCanMoveTick_ < 120) {
            return;
        }

        aaDebugCanMoveInitialized_ = true;
        aaDebugLastCanMoveValue_ = value;
        aaDebugLastCanMoveReasonHash_ = reasonHash;
        aaDebugLastCanMoveTick_ = now;

        AADebugAppend(
            "[AADebug] CAN_MOVE id=%d value=%d reason=%s readyIn=%.1f windup=%.1f extra=%.1f disableMissile=%d "
            "lastAA=%d localAtk=%d dtAA=%d dtLocal=%d pending=%d missile=%d damageIssued=%d moveTick=%d",
            aaDebugAttackId_,
            value ? 1 : 0,
            reason ? reason : "?",
            readyIn,
            windup,
            extraWindup,
            disableMissileCheck ? 1 : 0,
            lastAutoAttackTick_,
            lastLocalAttackTick_,
            now - lastAutoAttackTick_,
            now - lastLocalAttackTick_,
            attackOrderPending_ ? 1 : 0,
            missileLaunched_ ? 1 : 0,
            attackDamageIssued_ ? 1 : 0,
            lastMovementTick_);
    }

    void AADebugLogMoveBlocked(const char* reason,
                               const Vector3& requested,
                               const Vector3& adjusted,
                               int elapsedMove,
                               float cursorDelta,
                               float destinationDelta,
                               float angle) const {
        if (!kAADebugEnabled) {
            return;
        }

        const int now = Tick();
        const unsigned reasonHash = AADebugHash(reason);
        if (aaDebugLastMoveBlockReasonHash_ == reasonHash &&
            now - aaDebugLastMoveBlockTick_ < 120) {
            return;
        }

        aaDebugLastMoveBlockReasonHash_ = reasonHash;
        aaDebugLastMoveBlockTick_ = now;

        const auto player = GameObjects::Player();
        const Vector3 serverPos = player.IsValid() ? player.ServerPosition() : Vector3();
        AADebugAppend(
            "[AADebug] MOVE_BLOCK id=%d reason=%s elapsedMove=%d cursorDelta=%.1f destDelta=%.1f angle=%.1f "
            "lastMove=%d req=(%.1f,%.1f,%.1f) adj=(%.1f,%.1f,%.1f) server=(%.1f,%.1f,%.1f)",
            aaDebugAttackId_,
            reason ? reason : "?",
            elapsedMove,
            cursorDelta,
            destinationDelta,
            angle,
            lastMovementTick_,
            requested.x,
            requested.y,
            requested.z,
            adjusted.x,
            adjusted.y,
            adjusted.z,
            serverPos.x,
            serverPos.y,
            serverPos.z);
    }

    int ComputeFarmDebugKeyMask() const {
        if (!kFarmDebugEnabled) {
            return 0;
        }

        int mask = 0;
        if (KeyActive("Harass") || (::GetAsyncKeyState('C') & 0x8000) != 0) {
            mask |= 1;
        }
        if (KeyActive("LaneClear") || (::GetAsyncKeyState('V') & 0x8000) != 0) {
            mask |= 2;
        }
        if (KeyActive("LastHit") || (::GetAsyncKeyState('X') & 0x8000) != 0) {
            mask |= 4;
        }
        return mask;
    }

    int FarmDebugKeyMask() const {
        if (!kFarmDebugEnabled) {
            return 0;
        }
        return farmDebugFrameMask_ >= 0 ? farmDebugFrameMask_ : ComputeFarmDebugKeyMask();
    }

    bool FarmDebugActive() const {
        return kFarmDebugEnabled && FarmDebugKeyMask() != 0;
    }

    void FarmDebugKeyText(char* out, std::size_t outSize) const {
        if (!out || outSize == 0) {
            return;
        }
        const int mask = FarmDebugKeyMask();
        _snprintf_s(
            out,
            outSize,
            _TRUNCATE,
            "%s%s%s%s",
            (mask & 1) ? "C/Harass" : "",
            ((mask & 1) && (mask & (2 | 4))) ? "+" : "",
            (mask & 2) ? "V/LaneClear" : "",
            (mask & 4) ? ((mask & (1 | 2)) ? "+X/LastHit" : "X/LastHit") : "");
        if (!out[0]) {
            lstrcpynA(out, "none", static_cast<int>(outSize));
        }
    }

    void FarmDebugAppend(const char* fmt, ...) const {
        if (!kFarmDebugEnabled) {
            return;
        }

        char body[2048] = {};
        va_list args;
        va_start(args, fmt);
        _vsnprintf_s(body, sizeof(body), _TRUNCATE, fmt, args);
        va_end(args);

        char line[2300] = {};
        SYSTEMTIME st = {};
        GetLocalTime(&st);
        _snprintf_s(
            line,
            sizeof(line),
            _TRUNCATE,
            "[%02u:%02u:%02u.%03u tick=%d] %s\r\n",
            static_cast<unsigned>(st.wHour),
            static_cast<unsigned>(st.wMinute),
            static_cast<unsigned>(st.wSecond),
            static_cast<unsigned>(st.wMilliseconds),
            Tick(),
            body);

        HANDLE file = CreateFileA(
            "C:\\Users\\Public\\ns_orbwalker_farm_debug.txt",
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return;
        }
        DWORD written = 0;
        WriteFile(file, line, static_cast<DWORD>(lstrlenA(line)), &written, nullptr);
        CloseHandle(file);
    }

    int& FarmDebugLastTick(FarmDebugSlot slot) const {
        switch (slot) {
        case FarmDebugSlot::Scan: return lastFarmDebugScanTick_;
        case FarmDebugSlot::Select: return lastFarmDebugSelectTick_;
        case FarmDebugSlot::Orbwalk: return lastFarmDebugOrbwalkTick_;
        case FarmDebugSlot::Attack: return lastFarmDebugAttackTick_;
        case FarmDebugSlot::Move: return lastFarmDebugMoveTick_;
        default: return lastFarmDebugFrameTick_;
        }
    }

    bool ShouldWriteFarmDebug(FarmDebugSlot slot, int delay, bool force = false) const {
        if (!kFarmDebugEnabled) {
            return false;
        }

        const int mask = FarmDebugKeyMask();
        if (mask == 0) {
            if (farmDebugWasActive_) {
                FarmDebugAppend("[FarmDebug] END keys released");
                farmDebugWasActive_ = false;
                lastFarmDebugKeyMask_ = 0;
            }
            return false;
        }

        if (!farmDebugWasActive_ || mask != lastFarmDebugKeyMask_) {
            char keys[64] = {};
            FarmDebugKeyText(keys, sizeof(keys));
            FarmDebugAppend(
                "[FarmDebug] BEGIN keys=%s mode=%s menuC=%d menuV=%d menuX=%d asyncC=%d asyncV=%d asyncX=%d",
                keys,
                ModeName(ActiveMode()),
                keyMenu_ ? keyMenu_->GetKeyBindValue("Harass", false) ? 1 : 0 : 0,
                keyMenu_ ? keyMenu_->GetKeyBindValue("LaneClear", false) ? 1 : 0 : 0,
                keyMenu_ ? keyMenu_->GetKeyBindValue("LastHit", false) ? 1 : 0 : 0,
                (::GetAsyncKeyState('C') & 0x8000) != 0 ? 1 : 0,
                (::GetAsyncKeyState('V') & 0x8000) != 0 ? 1 : 0,
                (::GetAsyncKeyState('X') & 0x8000) != 0 ? 1 : 0);
            farmDebugWasActive_ = true;
            lastFarmDebugKeyMask_ = mask;
            force = true;
        }

        const int now = Tick();
        int& lastTick = FarmDebugLastTick(slot);
        if (force || lastTick <= 0 || now - lastTick >= delay) {
            lastTick = now;
            return true;
        }
        return false;
    }

    std::string DebugCanAttackReason(const AIHeroClient& player) const {
        if (!initialized_) {
            return "false:not-initialized";
        }
        if (!player.IsValid()) {
            return "false:player-invalid";
        }
        if (player.IsDead()) {
            return "false:player-dead";
        }

        const int now = Tick();
        if (allPauseTick_ > 0 && now < allPauseTick_) {
            return "false:all-pause";
        }
        if (attackPauseTick_ > 0 && now < attackPauseTick_) {
            return "false:attack-pause";
        }
        if (!PlayerCanAttack(player)) {
            return "false:actionstate-cannot-attack";
        }
        if (player.HasBuff("tahmkenchwdevoured")) {
            return "false:tahm-devoured";
        }
        if (CoreBuffs::HasBuffType(player.Address(), kBuffFear)) {
            return "false:fear";
        }
        if (CoreBuffs::HasBuffType(player.Address(), kBuffPolymorph) || player.HasBuff("Polymorph")) {
            return "false:polymorph";
        }
        if (!isKalista_ && CoreBuffs::HasBuffType(player.Address(), kBuffBlind)) {
            return "false:blind";
        }

        float attackDelay = GetAttackDelay() * 1000.0f;
        if (isGraves_) {
            if (!player.HasBuff("gravesbasicattackammo1")) {
                return "false:graves-no-ammo";
            }
            attackDelay = attackDelay * 1.0740297f - 716.2381f;
        }
        if (isSett_ && nextAttackIsPassive_) {
            attackDelay /= 8.0f;
        }
        if (attackOrderPending_ && lastLocalAttackTick_ > 0) {
            const float pendingWindow = std::clamp(attackDelay, 180.0f, 650.0f);
            if (static_cast<float>(now - lastLocalAttackTick_) < pendingWindow) {
                char text[96] = {};
                _snprintf_s(text, sizeof(text), _TRUNCATE, "false:pending-order %.0fms-left",
                    pendingWindow - static_cast<float>(now - lastLocalAttackTick_));
                return text;
            }
        }
        if (isAphelios_ && player.HasBuff("apheliospreload")) {
            return "false:aphelios-preload";
        }
        if (isJhin_ && player.HasBuff("JhinPassiveReload")) {
            return "false:jhin-reload";
        }

        const float readyAt = static_cast<float>(lastAutoAttackTick_) + attackDelay;
        const float serverNow = static_cast<float>(now) + static_cast<float>(Game::Ping()) / 2.0f + 25.0f;
        char text[128] = {};
        _snprintf_s(
            text,
            sizeof(text),
            _TRUNCATE,
            "%s delay=%.1f readyIn=%.1f lastAA=%d",
            serverNow >= readyAt ? "true" : "false:attack-delay",
            attackDelay,
            std::max(0.0f, readyAt - serverNow),
            lastAutoAttackTick_);
        return text;
    }

    std::string DebugCanMoveReason(const AIHeroClient& player, float extraWindup, bool disableMissileCheck) const {
        if (!initialized_) {
            return "false:not-initialized";
        }
        if (!player.IsValid()) {
            return "false:player-invalid";
        }
        if (player.IsDead()) {
            return "false:player-dead";
        }

        const int now = Tick();
        if (allPauseTick_ > 0 && now < allPauseTick_) {
            return "false:all-pause";
        }
        if (movePauseTick_ > 0 && now < movePauseTick_) {
            return "false:move-pause";
        }
        if (player.HasBuff("tahmkenchwdevoured")) {
            return "false:tahm-devoured";
        }
        if (isKalista_) {
            return "true:kalista";
        }
        if (missileLaunched_ && !disableMissileCheck && Bool(advancedMenu_, "MissileCheck", true)) {
            return "true:missile-launched";
        }

        const int rengarExtra = (player.HasBuff("rengarq") || player.HasBuff("rengarqemp")) ? 200 : 0;
        const float readyAt = static_cast<float>(lastAutoAttackTick_) +
            GetAttackCastDelay() * 1000.0f + extraWindup + static_cast<float>(rengarExtra);
        const float serverNow = static_cast<float>(now) + static_cast<float>(Game::Ping()) / 2.0f;
        char text[128] = {};
        _snprintf_s(
            text,
            sizeof(text),
            _TRUNCATE,
            "%s windup=%.1f readyIn=%.1f missile=%d lastAA=%d",
            serverNow >= readyAt ? "true" : "false:windup",
            GetAttackCastDelay() * 1000.0f + extraWindup,
            std::max(0.0f, readyAt - serverNow),
            missileLaunched_ ? 1 : 0,
            lastAutoAttackTick_);
        return text;
    }

    void FarmDebugLogState(FarmDebugSlot slot,
                           const char* stage,
                           const AttackableUnit& target,
                           const char* reason,
                           bool force = false) const {
        if (!ShouldWriteFarmDebug(slot, force ? 80 : 220, force)) {
            return;
        }

        const auto player = GameObjects::Player();
        char keys[64] = {};
        FarmDebugKeyText(keys, sizeof(keys));

        const std::string targetName = target.IsValid() ? target.CharacterName() : std::string();
        const float dist = player.IsValid() && target.IsValid()
            ? player.Position().Distance2D(target.Position())
            : -1.0f;
        const float aaRange = target.IsValid() ? GetAutoAttackRange(target) : GetAutoAttackRange(AttackableUnit());
        const bool validAttackTarget = target.IsValid()
            ? OrbwalkingDetail::IsValidAttackTarget(target, aaRange)
            : false;
        const float damage = target.IsValid() ? GetAutoAttackDamage(target) : 0.0f;

        const std::string canAttack = DebugCanAttackReason(player);
        const std::string canMove = DebugCanMoveReason(
            player,
            static_cast<float>(Slider(orbwalkerMenu_, "WindupDelay", 60)),
            false);

        FarmDebugAppend(
            "[FarmDebug] stage=%s mode=%s keys=%s reason=%s target=%s name=%s net=%d hp=%.1f/%.1f team=%d enemy=%d visible=%d targetable=%d invuln=%d dead=%d dist=%.1f range=%.1f validAA=%d dmg=%.1f canAttack={%s} canMove={%s} lastMove=%d lastLocalAttack=%d pending=%d missile=%d cursor=(%.1f,%.1f,%.1f)",
            stage ? stage : "?",
            ModeName(ActiveMode()),
            keys,
            reason ? reason : "?",
            TargetKind(target),
            targetName.empty() ? "?" : targetName.c_str(),
            target.IsValid() ? target.NetworkId() : 0,
            target.IsValid() ? target.Health() : 0.0f,
            target.IsValid() ? target.MaxHealth() : 0.0f,
            target.IsValid() ? static_cast<int>(target.Team()) : 0,
            target.IsValid() && target.IsEnemy() ? 1 : 0,
            target.IsValid() && target.IsVisible() ? 1 : 0,
            target.IsValid() && target.IsTargetable() ? 1 : 0,
            target.IsValid() && target.IsInvulnerable() ? 1 : 0,
            target.IsValid() && target.IsDead() ? 1 : 0,
            dist,
            aaRange,
            validAttackTarget ? 1 : 0,
            damage,
            canAttack.c_str(),
            canMove.c_str(),
            lastMovementTick_,
            lastLocalAttackTick_,
            attackOrderPending_ ? 1 : 0,
            missileLaunched_ ? 1 : 0,
            Game::CursorPos().x,
            Game::CursorPos().y,
            Game::CursorPos().z);
    }

    void FarmDebugLogMinionScan(int total,
                                int accepted,
                                int invalid,
                                int ignored,
                                int rejectedTarget,
                                float extraRange,
                                float range) const {
        if (!ShouldWriteFarmDebug(FarmDebugSlot::Scan, 350, false)) {
            return;
        }
        FarmDebugAppend(
            "[FarmDebug] stage=minion-scan mode=%s total=%d accepted=%d invalid=%d ignored=%d rejectedTarget=%d extra=%.1f scanRange=%.1f baseRange=%.1f",
            ModeName(ActiveMode()),
            total,
            accepted,
            invalid,
            ignored,
            rejectedTarget,
            extraRange,
            range,
            GetAutoAttackRange(AttackableUnit()));
    }

    void FarmDebugLogTargetDecision(const char* decision,
                                    const AttackableUnit& target,
                                    std::size_t minionCount,
                                    bool waitForFarm,
                                    bool force = false) const {
        if (FarmDebugKeyMask() == 0) {
            return;
        }

        if (!force && !ShouldWriteFarmDebug(FarmDebugSlot::Select, 120, false)) {
            return;
        }

        const auto player = GameObjects::Player();
        char keys[64] = {};
        FarmDebugKeyText(keys, sizeof(keys));
        const std::string targetName = target.IsValid() ? target.CharacterName() : std::string();
        const float dist = player.IsValid() && target.IsValid()
            ? player.Position().Distance2D(target.Position())
            : -1.0f;
        const float range = target.IsValid() ? GetAutoAttackRange(target) : GetAutoAttackRange(AttackableUnit());
        const bool validAttackTarget = target.IsValid()
            ? OrbwalkingDetail::IsValidAttackTarget(target, range)
            : false;

        FarmDebugAppend(
            "[FarmDebug] stage=target-decision mode=%s keys=%s decision=%s minions=%llu wait=%d target=%s name=%s net=%d hp=%.1f/%.1f dist=%.1f range=%.1f validAA=%d canAttack={%s} canMove={%s}",
            ModeName(ActiveMode()),
            keys,
            decision ? decision : "?",
            static_cast<unsigned long long>(minionCount),
            waitForFarm ? 1 : 0,
            TargetKind(target),
            targetName.empty() ? "?" : targetName.c_str(),
            target.IsValid() ? target.NetworkId() : 0,
            target.IsValid() ? target.Health() : 0.0f,
            target.IsValid() ? target.MaxHealth() : 0.0f,
            dist,
            range,
            validAttackTarget ? 1 : 0,
            DebugCanAttackReason(player).c_str(),
            DebugCanMoveReason(player, static_cast<float>(Slider(orbwalkerMenu_, "WindupDelay", 60)), false).c_str());
    }

    void FarmDebugBreadcrumb(const char* step,
                             std::size_t minionCount = 0,
                             const AttackableUnit& target = {}) const {
        if (FarmDebugKeyMask() == 0) {
            return;
        }
        char keys[64] = {};
        FarmDebugKeyText(keys, sizeof(keys));
        FarmDebugAppend(
            "[FarmDebug] stage=breadcrumb mode=%s keys=%s step=%s minions=%llu target=%s name=%s net=%d",
            ModeName(ActiveMode()),
            keys,
            step ? step : "?",
            static_cast<unsigned long long>(minionCount),
            TargetKind(target),
            target.IsValid() ? target.CharacterName().c_str() : "?",
            target.IsValid() ? target.NetworkId() : 0);
    }

    bool ShouldBlockForChat() const {
        return ::CoreGame::IsChatOpenByKeyboard();
    }

    virtual void OnGameUpdate(const Events::GameUpdateEventArgs&) {
        OrbwalkerDropFpsScope perf(this, "OnGameUpdate.pre");
        farmDebugFrameMask_ = ComputeFarmDebugKeyMask();
        if (!initialized_) {
            FarmDebugLogState(FarmDebugSlot::Frame, "update-blocked", {}, "not-initialized", true);
            return;
        }

        const auto player = GameObjects::Player();
        FarmDebugLogState(FarmDebugSlot::Frame, "update-enter", {}, "tick", false);

        if (isSett_ && nextAttackIsPassive_ && settInfo_.AttackTime > 0 &&
            Tick() - settInfo_.AttackTime > 2000) {
            nextAttackIsPassive_ = false;
        }

        calcItemDamage_ = Bool(advancedMenu_, "CalcItemDamage", false);
        if (!player.IsValid()) {
            FarmDebugLogState(FarmDebugSlot::Frame, "update-blocked", {}, "player-invalid", true);
            return;
        }
        if (player.IsDead()) {
            FarmDebugLogState(FarmDebugSlot::Frame, "update-blocked", {}, "player-dead", true);
            return;
        }
        const OrbwalkingMode mode = ActiveMode();
        const bool chatMemory = Game::IsChatOpen();
        const bool chatKeyboard = ::CoreGame::IsChatOpenByKeyboard();
        if (mode != OrbwalkingMode::Flee && ShouldBlockForChat()) {
            char reason[96] = {};
            _snprintf_s(
                reason,
                sizeof(reason),
                _TRUNCATE,
                "chat-open keyboard=%d memory=%d",
                chatKeyboard ? 1 : 0,
                chatMemory ? 1 : 0);
            FarmDebugLogState(FarmDebugSlot::Frame, "update-blocked", {}, reason, true);
            return;
        }
        if (Game::IsShopOpen()) {
            FarmDebugLogState(FarmDebugSlot::Frame, "update-blocked", {}, "shop-open", true);
            return;
        }
        if (mode == OrbwalkingMode::None) {
            FarmDebugLogState(FarmDebugSlot::Frame, "update-blocked", {}, "active-mode-none", false);
            return;
        }

        perf.Finish();
        AttackableUnit target = GetThrottledTarget();
        FarmDebugLogState(
            FarmDebugSlot::Frame,
            "before-orbwalk",
            target,
            target.IsValid() ? "target-selected" : "no-target-selected",
            true);
        Orbwalk(target, orbwalkerPosition_);
    }

    // GetTarget() (in OrbwalkerSelector) re-scans every enemy minion/hero/ward list and
    // runs multiple HealthPrediction simulations per candidate. OnGameUpdate() fires once
    // per render frame, so at 120-130 FPS that whole pipeline was being rebuilt from
    // scratch 120-130 times a second while a mode key (V/C/X) was held - this was the
    // direct cause of the FPS drop. The game logic only needs a fresh target roughly
    // every kTargetSelectionIntervalMs; Attack()/Move() are still re-issued every frame
    // below, only the expensive *selection* step is throttled here.
    static constexpr int kTargetSelectionIntervalMs = 50;

    AttackableUnit GetThrottledTarget() {
        OrbwalkerDropFpsScope perf(this, "GetThrottledTarget");
        const int now = Tick();
        const OrbwalkingMode mode = ActiveMode();
        const bool modeChanged = cachedOrbwalkTargetMode_ != mode;
        if (modeChanged) {
            cachedOrbwalkTarget_ = {};
            lastTargetSelectTick_ = 0;
            cachedOrbwalkTargetMode_ = mode;
        }

        const float attackDelayMs = std::max(350.0f, GetAttackDelay() * 1000.0f);
        const bool nearAttackReady =
            lastAutoAttackTick_ <= 0 ||
            static_cast<float>(now + 120) >= static_cast<float>(lastAutoAttackTick_) + attackDelayMs;
        if (!nearAttackReady && !CanAttack()) {
            return cachedOrbwalkTarget_;
        }

        const bool firstSelection = lastTargetSelectTick_ <= 0;
        const bool expired = firstSelection || (now - lastTargetSelectTick_) >= kTargetSelectionIntervalMs;
        const bool staleTarget = cachedOrbwalkTarget_.IsValid() &&
            !OrbwalkingDetail::IsValidAttackTarget(
                cachedOrbwalkTarget_, GetAutoAttackRange(cachedOrbwalkTarget_));

        if (staleTarget) {
            cachedOrbwalkTarget_ = {};
        }

        if (expired) {
            cachedOrbwalkTarget_ = GetTarget();
            lastTargetSelectTick_ = now;
            cachedOrbwalkTargetMode_ = mode;
        }
        return cachedOrbwalkTarget_;
    }

    virtual void OnDraw() {
        if (!initialized_) {
            return;
        }

        const auto player = GameObjects::Player();
        if (!player.IsValid() || player.IsDead()) {
            return;
        }

        OrbwalkerDropFpsScope drawCorePerf(this, "OnDraw.core");
        const Vector3 playerPosition = player.Position();
        if (Bool(drawingMenu_, "DrawAttackRange", true)) {
            float drawRange = Utils::AutoAttack::GetRealAutoAttackRange(player);
            if (!IsSaneDrawRange(drawRange)) {
                drawRange = player.AttackRange();
            }
            if (playerPosition.IsValid() && !playerPosition.IsZero() && IsSaneDrawRange(drawRange)) {
                Drawing::DrawCircle(playerPosition, drawRange, 0xAA66FF66u, 2.0f, 64);
            }
        }

        if (Bool(drawingMenu_, "DrawHoldPosition", false) &&
            playerPosition.IsValid() && !playerPosition.IsZero()) {
            const float holdRadius =
                player.BoundingRadius() + static_cast<float>(Slider(orbwalkerMenu_, "ExtraHold", 50));
            if (IsSaneDrawRange(holdRadius)) {
                Drawing::DrawCircle(playerPosition, holdRadius, 0xB0A050FFu, 1.5f, 64);
            }
        }
        drawCorePerf.Finish();

        if (Bool(drawingMenu_, "DrawKillableMinion", false)) {
            OrbwalkerDropFpsScope killablePerf(this, "OnDraw.killable");
            const int now = Tick();
            if (now - killableDrawCacheTick_ >= 220) {
                killableDrawCacheTick_ = now;
                killableDrawCache_.clear();
                const float range = GetAutoAttackRange(AttackableUnit());
                const float rangeSqr = range * range;
                const float quickDamageGate =
                    std::max(120.0f, player.TotalAttackDamage() * 2.0f + 40.0f);

                auto scanMinion = [&](const AIMinionClient& minion) {
                    if (!minion.IsValid() || minion.IsDead() ||
                        !minion.IsTargetable() || minion.IsInvulnerable()) {
                        return;
                    }
                    const Vector3 minionPosition = minion.Position();
                    if (!minionPosition.IsValid() || minionPosition.IsZero() ||
                        playerPosition.DistanceSqr2D(minionPosition) > rangeSqr ||
                        minion.Health() > quickDamageGate) {
                        return;
                    }
                    if (IsDrawKillableMinionThreshold(minion)) {
                        killableDrawCache_.push_back({ AttackableUnit(minion.Handle()), 25.0f });
                    }
                };

                const auto& laneMinions = GameObjects::EnemyLaneMinions();
                if (!laneMinions.empty()) {
                    for (const auto& minion : laneMinions) {
                        scanMinion(minion);
                    }
                } else {
                    for (const auto& minion : GameObjects::EnemyMinions()) {
                        if (OrbwalkingDetail::IsLaneMinion(minion)) {
                            scanMinion(minion);
                        }
                    }
                }
            }
            for (const auto& circle : killableDrawCache_) {
                if (!circle.Target.IsValid() || circle.Target.IsDead()) {
                    continue;
                }
                const Vector3 position = circle.Target.Position();
                const float radius = circle.Target.BoundingRadius() + circle.RadiusPadding;
                if (position.IsValid() && !position.IsZero() && radius > 0.0f) {
                    Drawing::DrawCircle(position, radius, 0xAA33FF66u, 1.25f, 32);
                }
            }
        }
    }

    virtual void OnAlwaysDraw() {
        if (!initialized_ || !drawingMenu_) {
            return;
        }

        const auto player = GameObjects::Player();
        if (!player.IsValid() || player.IsDead()) {
            return;
        }

        DrawFakeVisuals();
    }

    virtual void OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
        if (!initialized_ || !OrbwalkingDetail::IsLocalPlayer(args.Sender)) {
            return;
        }

        if (isSett_ && args.Slot == static_cast<int>(SpellSlot::Q)) {
            nextAttackIsPassive_ = false;
        }

        const std::string spellName = BestSpellName(args);
        const bool nativeAutoAttack = IsAutoAttack(spellName) || args.IsAutoAttack;
        const char* attackEventReason = nullptr;
        const bool localAttackEvent =
            IsIssuedAutoAttackEvent(args, spellName, nativeAutoAttack, &attackEventReason);
        if (IsAutoAttackReset(spellName) || IsLocalAutoAttackResetSlot(args.Sender, args.Slot)) {
            AADebugAppend(
                "[AADebug] PROCESS_RESET id=%d spell='%s' slot=%d dtLocal=%d",
                aaDebugAttackId_,
                spellName.c_str(),
                args.Slot,
                Tick() - lastLocalAttackTick_);
            ResetAutoAttackTimer();
        }

        if (!localAttackEvent) {
            AADebugAppend(
                "[AADebug] PROCESS_NON_AA id=%d spell='%s' missile='%s' script='%s' slot=%d isAA=%d dtLocal=%d pending=%d damageIssued=%d raw=(0x%llX,0x%llX,0x%llX,0x%llX)",
                aaDebugAttackId_,
                spellName.c_str(),
                args.MissileName,
                args.ScriptName,
                args.Slot,
                args.IsAutoAttack ? 1 : 0,
                Tick() - lastLocalAttackTick_,
                attackOrderPending_ ? 1 : 0,
                attackDamageIssued_ ? 1 : 0,
                static_cast<unsigned long long>(args.Raw.Rcx),
                static_cast<unsigned long long>(args.Raw.Rdx),
                static_cast<unsigned long long>(args.Raw.R8),
                static_cast<unsigned long long>(args.Raw.R9));
            return;
        }

        AttackableUnit target = OrbwalkingDetail::ToAttackableUnit(args.Target);
        if (!target.IsValid()) {
            target = lastTarget_;
        }

        const int now = Tick();
        const bool hadPendingAttack =
            attackOrderPending_ &&
            lastLocalAttackTick_ > 0 &&
            now - lastLocalAttackTick_ >= -50 &&
            now - lastLocalAttackTick_ <= PendingAttackHookWindow();
        if (lastLocalAttackTick_ <= 0 || now - lastLocalAttackTick_ > PendingAttackHookWindow()) {
            lastLocalAttackTick_ = now;
            lastAttackOrderToAnimGapMs_ = 0;
        } else if (hadPendingAttack) {
            lastAttackOrderToAnimGapMs_ = std::max(0, now - lastLocalAttackTick_);
        }
        const bool hadDamageIssued = attackDamageIssued_;
        if (!hadDamageIssued) {
            lastAutoAttackTick_ = hadPendingAttack ? lastLocalAttackTick_ : now;
        } else if (lastAutoAttackTick_ <= 0) {
            lastAutoAttackTick_ = now;
        }
        lastMovementTick_ = 0;
        attackOrderPending_ = false;
        missileLaunched_ = hadDamageIssued;
        attackDamageIssued_ = hadDamageIssued;
        lastAutoAttackEventTick_ = now;
        if (target.IsValid()) {
            lastTarget_ = target;
        }
        aaDebugCanMoveInitialized_ = false;
        AADebugAppend(
            "[AADebug] PROCESS_AA id=%d reason=%s native=%d spell='%s' slot=%d targetValid=%d targetNet=%d dtIssue=%d lastAA=%d localAtk=%d ping=%d damageIssued=%d",
            aaDebugAttackId_,
            attackEventReason ? attackEventReason : "?",
            nativeAutoAttack ? 1 : 0,
            spellName.c_str(),
            args.Slot,
            target.IsValid() ? 1 : 0,
            target.IsValid() ? target.NetworkId() : 0,
            now - lastLocalAttackTick_,
            lastAutoAttackTick_,
            lastLocalAttackTick_,
            Game::Ping(),
            attackDamageIssued_ ? 1 : 0);
    }

    virtual void OnDoCast(const Events::ProcessSpellEventArgs& args) {
        if (!initialized_ || !OrbwalkingDetail::IsLocalPlayer(args.Sender)) {
            return;
        }

        const std::string spellName = BestSpellName(args);
        const bool nativeAutoAttack = IsAutoAttack(spellName) || args.IsAutoAttack;
        const char* attackEventReason = nullptr;
        const bool localAttackEvent =
            IsIssuedAutoAttackEvent(args, spellName, nativeAutoAttack, &attackEventReason);
        if ((IsAutoAttackReset(spellName) || IsLocalAutoAttackResetSlot(args.Sender, args.Slot)) && args.CastDelay <= 0.0f) {
            AADebugAppend(
                "[AADebug] DOCAST_RESET id=%d spell='%s' slot=%d castDelay=%.1f dtAA=%d dtLocal=%d",
                aaDebugAttackId_,
                spellName.c_str(),
                args.Slot,
                args.CastDelay,
                Tick() - lastAutoAttackTick_,
                Tick() - lastLocalAttackTick_);
            ResetAutoAttackTimer();
        }

        if (!localAttackEvent) {
            AADebugAppend(
                "[AADebug] DOCAST_NON_AA id=%d spell='%s' missile='%s' script='%s' slot=%d isAA=%d dtLocal=%d pending=%d damageIssued=%d raw=(0x%llX,0x%llX,0x%llX,0x%llX)",
                aaDebugAttackId_,
                spellName.c_str(),
                args.MissileName,
                args.ScriptName,
                args.Slot,
                args.IsAutoAttack ? 1 : 0,
                Tick() - lastLocalAttackTick_,
                attackOrderPending_ ? 1 : 0,
                attackDamageIssued_ ? 1 : 0,
                static_cast<unsigned long long>(args.Raw.Rcx),
                static_cast<unsigned long long>(args.Raw.Rdx),
                static_cast<unsigned long long>(args.Raw.R8),
                static_cast<unsigned long long>(args.Raw.R9));
            return;
        }

        AttackableUnit target = OrbwalkingDetail::ToAttackableUnit(args.Target);
        if (!target.IsValid()) {
            target = lastTarget_;
        }

        const int now = Tick();
        const bool firstDamageMilestone = !attackDamageIssued_;
        const bool hadPendingAttack =
            attackOrderPending_ &&
            lastLocalAttackTick_ > 0 &&
            now - lastLocalAttackTick_ >= -50 &&
            now - lastLocalAttackTick_ <= PendingAttackHookWindow();
        if (lastLocalAttackTick_ <= 0 || now - lastLocalAttackTick_ > PendingAttackHookWindow()) {
            lastLocalAttackTick_ = now;
            lastAttackOrderToAnimGapMs_ = 0;
        } else if (hadPendingAttack) {
            lastAttackOrderToAnimGapMs_ = std::max(0, now - lastLocalAttackTick_);
        } else {
            lastAttackOrderToAnimGapMs_ = 0;
        }
        const int attackStartTick = hadPendingAttack ? lastLocalAttackTick_ : now;
        lastAutoAttackTick_ = attackStartTick;
        attackOrderPending_ = false;
        missileLaunched_ = true;
        attackDamageIssued_ = true;
        lastAutoAttackEventTick_ = now;
        lastMovementTick_ = 0;
        if (firstDamageMilestone) {
            ++autoAttackCounter_;
        }
        if (target.IsValid() && !target.Compare(lastTarget_)) {
            lastTarget_ = target;
        }
        aaDebugCanMoveInitialized_ = false;
        AADebugAppend(
            "[AADebug] DOCAST_AA id=%d reason=%s native=%d first=%d spell='%s' slot=%d targetValid=%d targetNet=%d dtAA=%d dtLocal=%d delay=%.1f windup=%.1f castDelay=%.1f missileSpeed=%.1f",
            aaDebugAttackId_,
            attackEventReason ? attackEventReason : "?",
            nativeAutoAttack ? 1 : 0,
            firstDamageMilestone ? 1 : 0,
            spellName.c_str(),
            args.Slot,
            target.IsValid() ? 1 : 0,
            target.IsValid() ? target.NetworkId() : 0,
            now - lastAutoAttackTick_,
            now - lastLocalAttackTick_,
            GetAttackDelay() * 1000.0f,
            GetAttackCastDelay() * 1000.0f,
            args.CastDelay,
            args.MissileSpeed);

        if (firstDamageMilestone && target.IsValid()) {
            const int afterAttackStartTick = lastAutoAttackTick_;
            OrbwalkingActionArgs attackArgs(OrbwalkingType::OnAttack, target, {}, "SDK");
            OrbwalkingDetail::FireOnAttack(attackArgs);
            if (lastAfterAttackStartTick_ != afterAttackStartTick) {
                lastAfterAttackStartTick_ = afterAttackStartTick;
                const float windup = GetAttackCastDelay() * 1000.0f;
                const int delayMs = std::max(0, static_cast<int>(std::ceil(windup)));
                Utils::DelayAction::Add(delayMs, [this, target, afterAttackStartTick]() {
                    if (!initialized_ ||
                        lastAfterAttackStartTick_ != afterAttackStartTick ||
                        !target.IsValid()) {
                        return;
                    }
                    OrbwalkingActionArgs afterArgs(
                        OrbwalkingType::AfterAttack,
                        target,
                        target.Position(),
                        "SDK");
                    OrbwalkingDetail::FireAfterAttack(afterArgs);
                });
            }
        }
    }

    virtual void OnStopCast(const Events::StopCastEventArgs& args) {
        if (!initialized_ || !OrbwalkingDetail::IsLocalPlayer(args.Sender)) {
            return;
        }

        // Reset AA only on a real forced stop before the attack was cast.
        // KeepAnimationPlaying + DestroyMissile is also used by normal attack/missile
        // cleanup paths; resetting there makes CanAttack() true inside windup, causing
        // the level-1 "raise weapon -> cancel -> raise again" loop.
        const int elapsedLocalAttack = Tick() - lastLocalAttackTick_;
        const bool shouldReset = !args.HasBeenCast &&
            args.ForceStop &&
            attackOrderPending_ &&
            !attackDamageIssued_ &&
            !missileLaunched_ &&
            lastLocalAttackTick_ > 0 &&
            elapsedLocalAttack >= 0 &&
            elapsedLocalAttack <= 1000;
        AADebugAppend(
            "[AADebug] STOPCAST id=%d hasBeenCast=%d force=%d keepAnim=%d destroyMissile=%d missileNet=%d spellCastId=%d dtLocal=%d pending=%d missile=%d damageIssued=%d reset=%d",
            aaDebugAttackId_,
            args.HasBeenCast ? 1 : 0,
            args.ForceStop ? 1 : 0,
            args.KeepAnimationPlaying ? 1 : 0,
            args.DestroyMissile ? 1 : 0,
            args.MissileNetworkId,
            args.SpellCastId,
            elapsedLocalAttack,
            attackOrderPending_ ? 1 : 0,
            missileLaunched_ ? 1 : 0,
            attackDamageIssued_ ? 1 : 0,
            shouldReset ? 1 : 0);
        if (shouldReset) {
            ResetAutoAttackTimer();
        }
    }

    virtual void OnPlayAnimation(const Events::PlayAnimationEventArgs& args) {
        if (!initialized_ || !OrbwalkingDetail::IsLocalPlayer(args.Sender)) {
            return;
        }

        const std::string animation = args.Animation;
        const auto player = GameObjects::Player();
        if (isRengar_ && animation == "Spell5") {
            int extra = 0;
            if (lastTarget_.IsValid() && player.IsValid() && player.Position().IsValid()) {
                extra += static_cast<int>(std::min(player.Distance(lastTarget_) / 1.5f, 0.6f));
            }
            lastAutoAttackTick_ = Tick() - Game::Ping() / 2 + extra;
        }

        if (!isSett_) {
            return;
        }

        if (animation.find("Attack") != std::string::npos) {
            if (animation.find("Passive") != std::string::npos) {
                settInfo_ = { true, Tick() };
                nextAttackIsPassive_ = false;
                return;
            }
            settInfo_ = { false, Tick() };
            nextAttackIsPassive_ = true;
            return;
        }

        if (animation == "Spell1_A") {
            settInfo_ = { false, Tick() };
            nextAttackIsPassive_ = true;
        } else if (animation == "Spell1_B") {
            settInfo_ = { true, Tick() };
            nextAttackIsPassive_ = false;
        }
    }

    virtual void OnBuffAdd(const Events::BuffEventArgs& args) {
        if (!initialized_ || !OrbwalkingDetail::IsLocalPlayer(args.Sender)) {
            return;
        }

        const std::string buffName = OrbwalkingDetail::ToLower(args.BuffName);
        if (buffName == "sonapassiveattack") {
            ResetAutoAttackTimer();
        }
    }

    virtual void OnDelete(const Events::ObjectEventArgs& args) {
        if (!initialized_ || !args.Sender.IsValid()) {
            return;
        }

        const AttackableUnit sender = OrbwalkingDetail::ToAttackableUnit(args.Sender);
        if (sender.IsValid()) {
            if (sender.Compare(forceTarget_)) {
                forceTarget_ = {};
            }
            if (sender.Compare(laneClearMinion_)) {
                laneClearMinion_ = {};
            }
            if (sender.Compare(lastTarget_)) {
                lastTarget_ = {};
            }
        }

        if (!isAphelios_) {
            return;
        }

        const bool localSource = args.Source.IsValid() && OrbwalkingDetail::IsLocalPlayer(args.Source);
        const std::string missileName =
            !std::string(args.MissileName).empty()
                ? args.MissileName
                : (!std::string(args.SpellName).empty()
                       ? args.SpellName
                       : args.Sender.CharacterName);
        if ((localSource || !args.Source.IsValid()) &&
            OrbwalkingDetail::ToLower(missileName) == "aphelioscrescendumattackmisin") {
            ResetAutoAttackTimer();
        }
    }

    virtual void OnMissileCreate(const Events::ObjectEventArgs& args) {
        if (!initialized_) {
            return;
        }

        AIBaseClient source;
        if (args.Source.IsValid()) {
            source = AIBaseClient(args.Source.Ptr, args.Source.Type);
        }
        if (!source.IsValid() && args.SourceNetworkId != 0 && args.SourceNetworkId != 0xFFFFFFFFu) {
            source = ObjectManager::GetUnitByNetworkId<AIBaseClient>(static_cast<int>(args.SourceNetworkId));
        }
        if (!source.IsValid() || !source.IsAlly() || source.IsDead()) {
            return;
        }

        const bool sourceIsTurret = source.IsTurret();
        const bool sourceIsMinion = source.IsMinion() && !AIMinionClient(source.Handle()).IsJungle();
        if (!sourceIsTurret && !sourceIsMinion) {
            return;
        }

        AIMinionClient target = ResolveMissileTarget(args, source);
        if (!target.IsValid() || target.IsDead() || !target.IsEnemy() ||
            target.IsJungle() || target.IsPlant() || target.IsPet() || target.IsClone() ||
            !OrbwalkingDetail::IsLaneMinion(target)) {
            return;
        }

        const int now = Tick();
        PruneFarmMissiles(now);

        FarmMissileAttack record = {};
        record.SourceNetworkId = source.NetworkId();
        record.TargetNetworkId = target.NetworkId();
        record.MissileNetworkId = static_cast<int>(args.MissileNetworkId);
        record.SourceIsTurret = sourceIsTurret;
        record.SourceIsMinion = sourceIsMinion;
        // REMOVED: Turret/Inhibitor/Nexus disabled by user request
        // record.Damage = sourceIsTurret
        //     ? Prediction::Health::GetAutoAttackDamage(AITurretClient(source.Handle()), target)
        //     : source.GetAutoAttackDamage(target, true);
        // REMOVED: Turret/Inhibitor/Nexus disabled
        record.Damage = sourceIsTurret
            ? 0.0f
            : source.GetAutoAttackDamage(target, true);
        record.ExpireTick = now + (sourceIsTurret ? 1800 : 1400);

        auto sameSource = [&](const FarmMissileAttack& item) {
            return item.SourceNetworkId == record.SourceNetworkId ||
                   (record.MissileNetworkId != 0 && item.MissileNetworkId == record.MissileNetworkId);
        };
        farmMissileAttacks_.erase(
            std::remove_if(farmMissileAttacks_.begin(), farmMissileAttacks_.end(), sameSource),
            farmMissileAttacks_.end());
        farmMissileAttacks_.push_back(record);
        ++farmMissileVersion_;
    }

    virtual void OnMissileDelete(const Events::ObjectEventArgs& args) {
        if (!initialized_) {
            return;
        }

        const int missileNet = static_cast<int>(args.MissileNetworkId);
        const int sourceNet = static_cast<int>(args.SourceNetworkId);
        if (missileNet == 0 && sourceNet == 0) {
            return;
        }

        farmMissileAttacks_.erase(
            std::remove_if(
                farmMissileAttacks_.begin(),
                farmMissileAttacks_.end(),
                [&](const FarmMissileAttack& item) {
                    return (missileNet != 0 && item.MissileNetworkId == missileNet) ||
                           (sourceNet != 0 && item.SourceNetworkId == sourceNet);
            }),
            farmMissileAttacks_.end());
        ++farmMissileVersion_;
    }

    static void OnGameUpdateStatic(const Events::GameUpdateEventArgs& args) {
        if (OrbwalkingDetail::RuntimeInstance) {
            OrbwalkingDetail::RuntimeInstance->OnGameUpdate(args);
        }
    }
    static void OnDrawStatic() {
        if (OrbwalkingDetail::RuntimeInstance) {
            OrbwalkingDetail::RuntimeInstance->OnDraw();
        }
    }
    static void OnAlwaysDrawStatic() {
        if (OrbwalkingDetail::RuntimeInstance) {
            OrbwalkingDetail::RuntimeInstance->OnAlwaysDraw();
        }
    }
    static void OnProcessSpellStatic(const Events::ProcessSpellEventArgs& args) {
        if (OrbwalkingDetail::RuntimeInstance) {
            OrbwalkingDetail::RuntimeInstance->OnProcessSpell(args);
        }
    }
    static void OnDoCastStatic(const Events::ProcessSpellEventArgs& args) {
        if (OrbwalkingDetail::RuntimeInstance) {
            OrbwalkingDetail::RuntimeInstance->OnDoCast(args);
        }
    }
    static void OnStopCastStatic(const Events::StopCastEventArgs& args) {
        if (OrbwalkingDetail::RuntimeInstance) {
            OrbwalkingDetail::RuntimeInstance->OnStopCast(args);
        }
    }
    static void OnPlayAnimationStatic(const Events::PlayAnimationEventArgs& args) {
        if (OrbwalkingDetail::RuntimeInstance) {
            OrbwalkingDetail::RuntimeInstance->OnPlayAnimation(args);
        }
    }
    static void OnBuffAddStatic(const Events::BuffEventArgs& args) {
        if (OrbwalkingDetail::RuntimeInstance) {
            OrbwalkingDetail::RuntimeInstance->OnBuffAdd(args);
        }
    }
    static void OnDeleteStatic(const Events::ObjectEventArgs& args) {
        if (OrbwalkingDetail::RuntimeInstance) {
            OrbwalkingDetail::RuntimeInstance->OnDelete(args);
        }
    }
    static void OnMissileCreateStatic(const Events::ObjectEventArgs& args) {
        if (OrbwalkingDetail::RuntimeInstance) {
            OrbwalkingDetail::RuntimeInstance->OnMissileCreate(args);
        }
    }
    static void OnMissileDeleteStatic(const Events::ObjectEventArgs& args) {
        if (OrbwalkingDetail::RuntimeInstance) {
            OrbwalkingDetail::RuntimeInstance->OnMissileDelete(args);
        }
    }

    void RegisterEvents() {
        if (eventsRegistered_) {
            return;
        }
        eventsRegistered_ = true;
        OrbwalkingDetail::RuntimeInstance = this;
        AADebugResetLog();
        Events::hook.OnGameUpdate += &OrbwalkerBase::OnGameUpdateStatic;
        Events::hook.OnProcessSpell += &OrbwalkerBase::OnProcessSpellStatic;
        Events::hook.OnDoCast += &OrbwalkerBase::OnDoCastStatic;
        Events::hook.OnStopCast += &OrbwalkerBase::OnStopCastStatic;
        Events::hook.OnPlayAnimation += &OrbwalkerBase::OnPlayAnimationStatic;
        Events::hook.OnBuffAdd += &OrbwalkerBase::OnBuffAddStatic;
        Events::hook.OnDeleteObject += &OrbwalkerBase::OnDeleteStatic;
        Events::hook.OnMissileCreate += &OrbwalkerBase::OnMissileCreateStatic;
        Events::hook.OnMissileDelete += &OrbwalkerBase::OnMissileDeleteStatic;
        Drawing::AddOnDraw(&OrbwalkerBase::OnDrawStatic);
        Drawing::AddOnAlwaysDraw(&OrbwalkerBase::OnAlwaysDrawStatic);
    }

    void InitializeMenu(Menu* parentMenu) {
        rootMenu_ = new Menu("orbwalker", "Orbwalker");
        if (parentMenu) {
            parentMenu->Add(rootMenu_);
        } else {
            rootMenu_->Root = true;
            rootMenu_->Attach();
        }

        attackableMenu_ = rootMenu_->AddSubMenu(new Menu("Attackable", "Attackable Unit"));
        attackableMenu_->Add(new MenuBool("Barrels", "Barrels", true));
        attackableMenu_->Add(new MenuBool("JunglePlant", "Jungle Plant", false));
        attackableMenu_->Add(new MenuBool("SpecialMinions", "Pets", true));
        attackableMenu_->Add(new MenuBool("Wards", "Wards", true));

        prioritizeMenu_ = rootMenu_->AddSubMenu(new Menu("Prioritize", "Prioritize"));
        prioritizeMenu_->Add(new MenuBool("FarmOverHarass", "Farm Over Harass", true));
        prioritizeMenu_->Add(new MenuBool("SpecialMinion", "Special Minion", false));
        prioritizeMenu_->Add(new MenuBool("SmallJungle", "Small Jungle", false));
        prioritizeMenu_->Add(new MenuBool("Turret", "Turret", true));

        orbwalkerMenu_ = rootMenu_->AddSubMenu(new Menu("Orbwalker", "Orbwalker"));
        orbwalkerMenu_->Add(new MenuSlider("ExtraHold", "Extra Hold", 50, 0, 250));
        orbwalkerMenu_->Add(new MenuBool("MoveRandom", "Move Random", false));
        orbwalkerMenu_->Add(new MenuSlider("WindupDelay", "Windup Delay", 60, 0, 250));
        orbwalkerMenu_->Add(new MenuBool("LimitAttack", "Limit Attack", false));

        farmMenu_ = rootMenu_->AddSubMenu(new Menu("Farm", "Farm"));
        farmMenu_->Add(new MenuSlider("FarmDelay", "Farm Delay", 30, 0, 200));
        farmMenu_->Add(new MenuSlider("FastFarmDelay", "Fast Farm Delay", 220, 0, 1000));
        farmMenu_->Add(new MenuBool("ShouldWait", "Should Wait", true));
        farmMenu_->Add(new MenuBool("TurretFarm", "Turret Farm", true));
        farmMenu_->Add(new MenuSlider("TurretFramMaxLevel", "Turret Farm Max Level", 13, 1, 18));

        advancedMenu_ = rootMenu_->AddSubMenu(new Menu("Advanced", "Advanced"));
        advancedMenu_->Add(new MenuBool("CalcItemDamage", "Calc Item Damage", false));
        advancedMenu_->Add(new MenuBool("YasuoWallCheck", "Yasuo Wall Check", true));
        advancedMenu_->Add(new MenuBool("MissileCheck", "Missile Check", true));

        const auto player = GameObjects::Player();
        const std::string championName = player.IsValid() ? player.CharacterName() : "Player";
        supportModeName_ = "SupportMode_" + championName;
        advancedMenu_->Add(new MenuBool(supportModeName_.c_str(), "Support Mode", false));

        drawingMenu_ = rootMenu_->AddSubMenu(new Menu("Drawing", "Drawing"));
        drawingMenu_->Add(new MenuBool("DrawAttackRange", "Draw Attack Range", true));
        drawingMenu_->Add(new MenuBool("DrawHoldPosition", "Draw Hold Position", false));
        drawingMenu_->Add(new MenuBool("DrawKillableMinion", "Draw Killable Minion", false));
        drawingMenu_->Add(new MenuBool("ShowFakeClick", "Show Fake Click", false));
        drawingMenu_->Add(new MenuBool("ShowFakeCursor", "Show Fake Cursor", false));
        drawingMenu_->Add(new MenuSlider("FakeCursorSize", "Fake Cursor Size", 22, 12, 42));

        keyMenu_ = rootMenu_->AddSubMenu(new Menu("Keys", "Keys"));
        keyMenu_->Add(new MenuKeyBind("Combo", "Combo", Keys::Space, KeyBindType::Press));
        keyMenu_->Add(new MenuKeyBind("ComboWithMove", "Combo Without Move", Keys::N, KeyBindType::Toggle));
        keyMenu_->Add(new MenuKeyBind("Harass", "Harass", Keys::C, KeyBindType::Press));
        keyMenu_->Add(new MenuKeyBind("LaneClear", "LaneClear", Keys::V, KeyBindType::Press));
        keyMenu_->Add(new MenuKeyBind("FastLaneClear", "Fast LaneClear", Keys::LMB, KeyBindType::Press));
        keyMenu_->Add(new MenuKeyBind("FastLaneClearToggle", "Fast LaneClear Toggle", Keys::CapsLock, KeyBindType::Toggle));
        keyMenu_->Add(new MenuKeyBind("LastHit", "LastHit", Keys::X, KeyBindType::Press));
        keyMenu_->Add(new MenuKeyBind("Flee", "Flee", Keys::Z, KeyBindType::Press));
    }

    void InitializeChampionFlags() {
        const auto player = GameObjects::Player();
        if (!player.IsValid()) {
            initialized_ = true;
            return;
        }
        const std::string name = OrbwalkingDetail::ToLower(player.CharacterName());
        isAphelios_ = name == "aphelios";
        isGraves_ = name == "graves";
        isJhin_ = name == "jhin";
        isKalista_ = name == "kalista";
        isRengar_ = name == "rengar";
        isSett_ = name == "sett";

        for (const auto& hero : GameObjects::Heroes()) {
            const std::string heroName = OrbwalkingDetail::ToLower(hero.CharacterName());
            if (heroName == "jax") {
                jaxInGame_ = true;
            } else if (heroName == "gangplank") {
                gangplankInGame_ = true;
            } else if (heroName == "tahmkench") {
                tahmKenchInGame_ = true;
            }
        }
        initialized_ = true;
    }

    bool KeyActive(const char* name) const {
        if (!keyMenu_) {
            return false;
        }
        const auto* key = keyMenu_->Get<MenuKeyBind>(name);
        if (!key) {
            return false;
        }
        if (key->Active) {
            return true;
        }
        if (key->Type == KeyBindType::Press) {
            return (::GetAsyncKeyState(key->Key) & 0x8000) != 0;
        }
        return false;
    }

    bool IsFastLaneClear() const {
        return ActiveMode() == OrbwalkingMode::LaneClear &&
               (KeyActive("FastLaneClear") ||
                KeyActive("FastLaneClearToggle") ||
                ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0));
    }

    static bool IsSaneDrawRange(float value) {
        return std::isfinite(value) && value > 0.0f && value < 5000.0f;
    }

    static bool Bool(const Menu* menu, const char* name, bool fallback) {
        return menu ? menu->GetBoolValue(name, fallback) : fallback;
    }

    static int Slider(const Menu* menu, const char* name, int fallback) {
        return menu ? menu->GetSliderValue(name, fallback) : fallback;
    }

    static int ListIndex(const Menu* menu, const char* name, int fallback) {
        return menu ? menu->GetListIndex(name, fallback) : fallback;
    }

    int Tick() const {
        return Variables::TickCount();
    }

    float GetAttackDelay() const {
        const auto player = GameObjects::Player();
        return CoreControl::GetAttackDelay(player.Address());
    }

    float GetAttackCastDelay() const {
        const auto player = GameObjects::Player();
        float windup = CoreControl::GetAttackWindup(player.Address());
        if (isSett_ && nextAttackIsPassive_) {
            windup -= windup / 8.0f;
        }
        return std::max(0.0f, windup);
    }

    // Auto-attack range used by the orbwalker for its attack/move decision.
    //
    // Port of EnsoulSharp AIBaseClientExtensions.GetRealAutoAttackRange:
    //   range = sender.AttackRange + sender.BoundingRadius + target.BoundingRadius
    // plus ping compensation matching Extensions::GetCurrentAutoAttackRange:
    //   - min(ping/4, 10) - 5
    //
    // The ping compensation was previously missing, which made the orbwalker's
    // attack check ~15 units larger than the practical in-game reach and caused
    // it to try attacking targets that were just barely out of real AA range.
    float GetAutoAttackRange(const AttackableUnit& target) const {
        const auto player = GameObjects::Player();
        if (!player.IsValid()) {
            return 0.0f;
        }

        float range = player.AttackRange() + player.BoundingRadius();
        if (target.IsValid() && !target.IsDead()) {
            if (player.CharacterName() == "Caitlyn") {
                const AIBaseClient targetBase(target.Handle());
                if (targetBase.HasBuff("CaitlynWSnare") || targetBase.HasBuff("CaitlynEMissile")) {
                    range = 1300.0f;
                }
            } else if (player.CharacterName() == "Aphelios") {
                const AIBaseClient targetBase(target.Handle());
                if (targetBase.HasBuff("aphelioscalibrumbonusrangedebuff") &&
                    player.HasBuff("aphelioscalibrumbonusrangebuff")) {
                    range = 1800.0f;
                }
            }
            range += target.BoundingRadius();
            // Ping compensation — matches Extensions::GetCurrentAutoAttackRange.
            // Without this the range was ~15 units too large at high ping.
            range -= std::min(static_cast<float>(Game::Ping()) / 4.0f, 10.0f) + 5.0f;
        }
        return range;
    }

    float GetAutoAttackDamage(const AttackableUnit& target) const {
        const auto player = GameObjects::Player();
        if (!player.IsValid() || !target.IsValid()) {
            return 0.0f;
        }
        return Damage::GetAutoAttackDamage(player, AIBaseClient(target.Handle()), true);
    }

    bool IsDrawKillableMinionThreshold(const AIMinionClient& minion, float damage) const {
        return minion.IsValid() &&
               !minion.IsDead() &&
               minion.IsTargetable() &&
               !minion.IsInvulnerable() &&
               damage > 0.0f &&
               minion.Health() <= damage;
    }

    bool IsDrawKillableMinionThreshold(const AIMinionClient& minion) const {
        return IsDrawKillableMinionThreshold(minion, GetAutoAttackDamage(minion));
    }

    float GetProjectileSpeed() const {
        const auto player = GameObjects::Player();
        if (!player.IsValid()) {
            return 2000.0f;
        }

        const std::string name = OrbwalkingDetail::ToLower(player.CharacterName());
        const float maxSpeed = std::numeric_limits<float>::max();
        if (player.IsMelee() || name == "azir" || name == "thresh" ||
            name == "velkoz" || name == "zeri") {
            return maxSpeed;
        }
        if (name == "aphelios") {
            return 1500.0f;
        }
        if (name == "ivern") {
            return 1600.0f;
        }
        if (name == "jayce") {
            return 2000.0f;
        }
        if (name == "jinx") {
            return 2000.0f;
        }
        if (name == "kayle" && player.AttackRange() >= 530.0f) {
            return 2250.0f;
        }
        if (name == "poppy") {
            return 1600.0f;
        }
        return 2000.0f;
    }

    float GetTimeToHit(const AttackableUnit& target) const {
        const auto player = GameObjects::Player();
        if (!player.IsValid()) {
            return 0.0f;
        }

        float time = (GetAttackCastDelay() * 1000.0f) - 100.0f + static_cast<float>(Game::Ping()) / 2.0f;
        const float missileSpeed = GetProjectileSpeed();
        if (missileSpeed < std::numeric_limits<float>::max() && target.IsValid()) {
            time += 1000.0f *
                    std::max(0.0f, player.Distance(target.Position()) - player.BoundingRadius()) /
                    std::max(1.0f, missileSpeed);
        }
        return std::max(0.0f, time);
    }

    bool PlayerCanAttack(const AIHeroClient& player) const {
        constexpr std::uint32_t canAttack = 0x1;
        return (CoreAttackableUnit::ActionState1(player.Address()) & canAttack) != 0;
    }

    bool PlayerCanMove(const AIHeroClient& player) const {
        constexpr std::uint32_t canMove = 0x4;
        return (CoreAttackableUnit::ActionState1(player.Address()) & canMove) != 0;
    }

    bool CanAttackWithWindWall(const AttackableUnit& target) const {
        if (!initialized_ || !target.IsValid()) {
            return false;
        }
        if (jaxInGame_ && AIBaseClient(target.Handle()).HasBuff("JaxCounterStrike")) {
            return false;
        }
        if (!Bool(advancedMenu_, "YasuoWallCheck", true)) {
            return true;
        }

        const auto player = GameObjects::Player();
        if (!player.IsValid()) {
            return false;
        }

        const std::string name = OrbwalkingDetail::ToLower(player.CharacterName());
        bool shouldCheck = OrbwalkingDetail::Contains(WindWallBrokenChampions(), name);
        if (!shouldCheck && OrbwalkingDetail::Contains(SpecialWindWallChampions(), name)) {
            shouldCheck =
                (name == "kayle" && player.AttackRange() >= 530.0f) ||
                (name == "elise" && !player.IsMelee()) ||
                (name == "nidalee" && !player.IsMelee()) ||
                (name == "jayce" && !player.IsMelee()) ||
                (name == "gnar" && !player.IsMelee()) ||
                (name == "neeko" && !player.HasBuff("neekowpassiveready"));
        }

        return !shouldCheck ||
               !Collisions::HasProjectileWallCollision(
                   player.ServerPosition(),
                   target.Position(),
                   0.0f);
    }

    bool SupportMode() const {
        return Bool(advancedMenu_, supportModeName_.c_str(), false);
    }

    bool ShouldSkipFarmForSupportMode() const {
        if (!SupportMode()) {
            return false;
        }
        const auto player = GameObjects::Player();
        if (!player.IsValid()) {
            return false;
        }
        for (const auto& hero : GameObjects::AllyHeroes()) {
            if (!hero.IsValid() || hero.IsDead() || hero.Compare(player)) {
                continue;
            }
            if (hero.Distance(player) <= 1500.0f) {
                return true;
            }
        }
        return false;
    }

    struct FarmMissileAttack {
        int SourceNetworkId = 0;
        int TargetNetworkId = 0;
        int MissileNetworkId = 0;
        int ExpireTick = 0;
        float Damage = 0.0f;
        bool SourceIsMinion = false;
        bool SourceIsTurret = false;
    };

    static bool IsValidMissilePosition(const Vector3& position) {
        return position.IsValid() &&
               !position.IsZero() &&
               position.x >= -30000.0f && position.x <= 30000.0f &&
               position.y >= -5000.0f && position.y <= 5000.0f &&
               position.z >= -30000.0f && position.z <= 30000.0f;
    }

    AIMinionClient ResolveMissileTarget(const Events::ObjectEventArgs& args,
                                        const AIBaseClient& source) const {
        if (args.Target.IsValid()) {
            AIMinionClient target(args.Target.Ptr);
            if (target.IsValid()) {
                return target;
            }
        }
        if (args.TargetNetworkId != 0 && args.TargetNetworkId != 0xFFFFFFFFu) {
            AIMinionClient target =
                ObjectManager::GetUnitByNetworkId<AIMinionClient>(static_cast<int>(args.TargetNetworkId));
            if (target.IsValid()) {
                return target;
            }
        }

        Vector3 end = args.EndPosition;
        if (!IsValidMissilePosition(end)) {
            end = args.CastEndPosition;
        }
        if (!IsValidMissilePosition(end)) {
            return {};
        }

        AIMinionClient best;
        float bestDistance = FLT_MAX;
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!minion.IsValid() || minion.IsDead() || minion.IsJungle() || minion.IsPlant() ||
                minion.IsPet() || minion.IsClone() || !OrbwalkingDetail::IsLaneMinion(minion)) {
                continue;
            }
            if (source.IsValid() && source.Distance(minion) > 2500.0f) {
                continue;
            }

            const float radius = std::max(120.0f, minion.BoundingRadius() + 80.0f);
            const float distance = minion.Position().DistanceSqr2D(end);
            if (distance <= radius * radius && distance < bestDistance) {
                bestDistance = distance;
                best = minion;
            }
        }
        return best;
    }

    void PruneFarmMissiles(int now) const {
        farmMissileAttacks_.erase(
            std::remove_if(
                farmMissileAttacks_.begin(),
                farmMissileAttacks_.end(),
                [now](const FarmMissileAttack& item) {
                    return item.ExpireTick <= now ||
                           item.TargetNetworkId == 0 ||
                           item.SourceNetworkId == 0;
                }),
            farmMissileAttacks_.end());
    }

    bool HasActiveAllyMinionMissiles() const {
        PruneFarmMissiles(Tick());
        for (const auto& item : farmMissileAttacks_) {
            if (item.SourceIsMinion) {
                return true;
            }
        }
        return false;
    }

    int CountAllyMinionMissilesTo(const AIMinionClient& target) const {
        if (!target.IsValid()) {
            return 0;
        }
        PruneFarmMissiles(Tick());
        int count = 0;
        const int targetNet = target.NetworkId();
        for (const auto& item : farmMissileAttacks_) {
            if (item.SourceIsMinion && item.TargetNetworkId == targetNet) {
                ++count;
            }
        }
        return count;
    }

    float AllyMinionMissileDamageTo(const AIMinionClient& target) const {
        if (!target.IsValid()) {
            return 0.0f;
        }
        PruneFarmMissiles(Tick());
        float damage = 0.0f;
        const int targetNet = target.NetworkId();
        for (const auto& item : farmMissileAttacks_) {
            if (item.SourceIsMinion && item.TargetNetworkId == targetNet) {
                damage += std::max(0.0f, item.Damage);
            }
        }
        return damage;
    }

    float AllyTurretMissileDamageTo(const AIMinionClient& target, int turretNetworkId = 0) const {
        if (!target.IsValid()) {
            return 0.0f;
        }
        PruneFarmMissiles(Tick());
        float damage = 0.0f;
        const int targetNet = target.NetworkId();
        for (const auto& item : farmMissileAttacks_) {
            if (!item.SourceIsTurret || item.TargetNetworkId != targetNet) {
                continue;
            }
            if (turretNetworkId != 0 && item.SourceNetworkId != turretNetworkId) {
                continue;
            }
            damage += std::max(0.0f, item.Damage);
        }
        return damage;
    }

    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    /*AIMinionClient TrackedTurretTarget(const AITurretClient& turret,
                                       const std::vector<AIMinionClient>& candidates) const {
        if (!turret.IsValid()) {
            return {};
        }
        PruneFarmMissiles(Tick());
        const int turretNet = turret.NetworkId();
        for (const auto& item : farmMissileAttacks_) {
            if (!item.SourceIsTurret || item.SourceNetworkId != turretNet) {
                continue;
            }
            for (const auto& minion : candidates) {
                if (minion.IsValid() && minion.NetworkId() == item.TargetNetworkId) {
                    return minion;
                }
            }
        }
        return {};
    }*/

    int FarmMissileVersion() const { return farmMissileVersion_; }

    float RandomFloat(float minValue, float maxValue) {
        std::uniform_real_distribution<float> distribution(minValue, maxValue);
        return distribution(random_);
    }

    static bool IsPlausibleSpellText(const char* text) {
        if (!text || !text[0]) {
            return false;
        }

        bool hasAlphaNumeric = false;
        for (int i = 0; text[i] && i < 96; ++i) {
            const unsigned char ch = static_cast<unsigned char>(text[i]);
            if (ch < 32 || ch > 126) {
                return false;
            }
            if (std::isalnum(ch)) {
                hasAlphaNumeric = true;
                continue;
            }
            if (ch == '_' || ch == '-' || ch == '.') {
                continue;
            }
            return false;
        }
        return hasAlphaNumeric;
    }

    static bool IsLikelyAutoAttackSlot(int slot) {
        return slot == 0 || slot == 64 || slot == -1;
    }

    int PendingAttackHookWindow() const {
        const float windup = GetAttackCastDelay() * 1000.0f;
        const float attackDelay = std::max(350.0f, GetAttackDelay() * 1000.0f);
        const float window = std::max(500.0f, windup + 350.0f);
        return static_cast<int>(std::min(window, std::min(900.0f, attackDelay + 120.0f)));
    }

    bool IsIssuedAutoAttackEvent(const Events::ProcessSpellEventArgs& args,
                                 const std::string& spellName,
                                 bool nativeAutoAttack,
                                 const char** reason) const {
        if (nativeAutoAttack) {
            if (reason) {
                *reason = "native-aa";
            }
            return true;
        }

        if (lastLocalAttackTick_ <= 0 || !lastTarget_.IsValid()) {
            return false;
        }
        if (!IsLikelyAutoAttackSlot(args.Slot)) {
            return false;
        }

        const int now = Tick();
        const int dtLocal = now - lastLocalAttackTick_;
        if (dtLocal < -50 || dtLocal > PendingAttackHookWindow()) {
            return false;
        }

        const bool sameAttackEvent =
            lastAutoAttackEventTick_ > 0 &&
            now - lastAutoAttackEventTick_ >= 0 &&
            now - lastAutoAttackEventTick_ <= 80;
        if (!attackOrderPending_ && attackDamageIssued_ && !sameAttackEvent) {
            return false;
        }

        const bool spellNameMissingOrBad = spellName.empty() || !IsPlausibleSpellText(spellName.c_str());
        const bool targetMatches =
            args.TargetNetworkId == 0 ||
            args.TargetNetworkId == static_cast<uint32_t>(lastTarget_.NetworkId()) ||
            !args.Target.IsValid();

        if (!spellNameMissingOrBad && !targetMatches) {
            return false;
        }

        if (reason) {
            *reason = spellNameMissingOrBad ? "issued-aa-bad-name" : "issued-aa-target";
        }
        return true;
    }

    std::string BestSpellName(const Events::ProcessSpellEventArgs& args) const {
        if (IsPlausibleSpellText(args.SpellName)) {
            return args.SpellName;
        }
        if (IsPlausibleSpellText(args.SpellSlotName)) {
            return args.SpellSlotName;
        }
        if (IsPlausibleSpellText(args.ScriptName)) {
            return args.ScriptName;
        }
        if (IsPlausibleSpellText(args.MissileName)) {
            return args.MissileName;
        }
        if (IsPlausibleSpellText(args.PayloadSpellName)) {
            return args.PayloadSpellName;
        }
        if (IsPlausibleSpellText(args.PayloadMissileName)) {
            return args.PayloadMissileName;
        }
        return {};
    }

    static const char* const* AttackResets() {
        static const char* values[] = {
            "asheq","camilleq2","camilleq","dariusnoxiantacticsonh","elisespiderw",
            "fiorae","gravesmove","garenq","gangplankqwrapper","illaoiw",
            "jaycehypercharge","jaxempowertwo","kaylee","luciane","leonashieldofdaybreakattack",
            "leonashieldofdaybreak","mordekaisermaceofspades","monkeykingdoubleattack",
            "meditate","masochism","netherblade","nautiluspiercinggaze","nasusq",
            "powerfist","rengarqemp","rengarq","renektonpreexecute","reksaiq","settq",
            "sivirw","shyvanadoubleattack","sejuaninorthernwinds","trundletrollsmash",
            "talonnoxiandiplomacy","takedown","vorpalspikes","volibearq","vie",
            "vaynetumble","xinzhaoq","xinzhaocombotarget","yorickspectral",
            "apheliosinfernumq","gravesautoattackrecoilcastedummy", nullptr
        };
        return values;
    }

    static const char* const* Attacks() {
        static const char* values[] = {
            "caitlynpassivemissile","itemtitanichydracleave","itemtiamatcleave",
            "kennenmegaproc","masteryidoublestrike","quinnwenhanced","renektonsuperexecute",
            "renektonexecute","trundleq","viktorqbuff","xinzhaoqthrust1",
            "xinzhaoqthrust2","xinzhaoqthrust3", nullptr
        };
        return values;
    }

    static const char* const* NoAttacks() {
        static const char* values[] = {
            "asheqattacknoonhit","annietibbersbasicattack","annietibbersbasicattack2",
            "bluecardattack","dravenattackp_r","dravenattackp_rc","dravenattackp_rq",
            "dravenattackp_l","dravenattackp_lc","dravenattackp_lq",
            "elisespiderlingbasicattack","gravesbasicattackspread","gravesautoattackrecoil",
            "goldcardattack","heimertyellowbasicattack","heimertyellowbasicattack2",
            "heimertbluebasicattack","heimerdingerwattack2","heimerdingerwattack2ult",
            "ivernminionbasicattack2","ivernminionbasicattack","kindredwolfbasicattack",
            "monkeykingdoubleattack","malzaharvoidlingbasicattack","malzaharvoidlingbasicattack2",
            "malzaharvoidlingbasicattack3","redcardattack","shyvanadoubleattackdragon",
            "shyvanadoubleattack","talonqdashattack","talonqattack","volleyattackwithsound",
            "volleyattack","yorickghoulmeleebasicattack","yorickghoulmeleebasicattack2",
            "yorickghoulmeleebasicattack3","yorickbigghoulbasicattack","zyraeplantattack",
            "zoebasicattackspecial1","zoebasicattackspecial2","zoebasicattackspecial3",
            "zoebasicattackspecial4","apheliosseverumattackmis","aphelioscrescendumattackmisin",
            "aphelioscrescendumattackmisout","gravesautoattackrecoilcastedummy",
            "gravesautoattackrecoil","gravesbasicattackspread", nullptr
        };
        return values;
    }

    static const char* const* WindWallBrokenChampions() {
        static const char* values[] = {
            "annie","twistedfate","leblanc","urgot","vladimir","fiddlesticks","ryze",
            "sivir","soraka","teemo","tristana","missfortune","ashe","morgana",
            "zilean","twitch","karthus","anivia","sona","janna","corki","karma",
            "veigar","swain","caitlyn","orianna","brand","vayne","cassiopeia",
            "heimerdinger","ezreal","kennen","kogmaw","lux","xerath","ahri","graves",
            "varus","viktor","lulu","ziggs","draven","quinn","syndra","aurelionsol",
            "zoe","zyra","kaisa","taliyah","jhin","kindred","jinx","lucian","yuumi",
            "thresh","kalista","xayah","aphelios","bard","ivern","nami","velkoz",
            "lissandra","malzahar", nullptr
        };
        return values;
    }

    static const char* const* SpecialWindWallChampions() {
        static const char* values[] = {
            "kayle","elise","nidalee","jayce","gnar","azir","neeko", nullptr
        };
        return values;
    }

    Menu* rootMenu_ = nullptr;
    Menu* attackableMenu_ = nullptr;
    Menu* prioritizeMenu_ = nullptr;
    Menu* orbwalkerMenu_ = nullptr;
    Menu* farmMenu_ = nullptr;
    Menu* advancedMenu_ = nullptr;
    Menu* drawingMenu_ = nullptr;
    Menu* keyMenu_ = nullptr;

    AttackableUnit forceTarget_ = {};
    AttackableUnit lastTarget_ = {};
    AIMinionClient laneClearMinion_ = {};
    Vector3 orbwalkerPosition_ = {};
    OrbwalkingMode activeMode_ = OrbwalkingMode::None;

    // Per-frame cache for GetLaneMinions() (see OrbwalkerSelector::GetLaneMinions).
    // Avoids recomputing the lane-minion filter multiple times within the same tick.
    mutable int laneMinionsCacheTick_ = -1;
    mutable std::size_t laneMinionsCacheSourceSize_ = 0;
    mutable std::vector<AIMinionClient> laneMinionsCache_ = {};

    int lastAutoAttackTick_ = 0;
    int lastMovementTick_ = 0;
    int lastLocalAttackTick_ = 0;
    int lastAutoAttackEventTick_ = 0;
    int lastAttackOrderTick_ = 0;
    int lastAttackOrderTargetNetworkId_ = 0;
    int lastAttackOrderToAnimGapMs_ = 0;
    int lastAfterAttackStartTick_ = 0;
    int autoAttackCounter_ = 0;
    int attackPauseTick_ = 0;
    int movePauseTick_ = 0;
    int allPauseTick_ = 0;
    int lastFakeMoveClickTick_ = 0;
    int lastFakeAttackClickTick_ = 0;
    int fakeClickExpireTick_ = 0;
    int fakeCursorVisibleUntilTick_ = 0;
    int lastTargetSelectTick_ = 0;
    AttackableUnit cachedOrbwalkTarget_ = {};
    OrbwalkingMode cachedOrbwalkTargetMode_ = OrbwalkingMode::None;

    bool attackEnabled_ = true;
    bool moveEnabled_ = true;
    bool initialized_ = false;
    bool eventsRegistered_ = false;
    bool attackOrderPending_ = false;
    bool missileLaunched_ = true;
    bool attackDamageIssued_ = true;
    bool nextAttackIsPassive_ = false;
    bool calcItemDamage_ = false;

    bool isAphelios_ = false;
    bool isGraves_ = false;
    bool isJhin_ = false;
    bool isKalista_ = false;
    bool isRengar_ = false;
    bool isSett_ = false;
    bool jaxInGame_ = false;
    bool gangplankInGame_ = false;
    bool tahmKenchInGame_ = false;

    DropFpsDebugStats fakeClickPerfStats_ = {};
    DropFpsDebugStats fakeCursorPerfStats_ = {};
    DropFpsDebugStats orbwalkerPerfStats_ = {};

    struct SettAttackInfo {
        bool IsPassive = true;
        int AttackTime = 0;
    } settInfo_ = {};

    Vector3 lastMoveOrderPosition_ = {};
    Vector3 fakeClickPosition_ = {};
    Vector3 fakeCursorTargetPosition_ = {};
    Vec2 fakeCursorScreenPosition_ = {};
    bool fakeCursorScreenValid_ = false;
    UI::Icons::LoadedTexture fakeCursorTexture_ = {};
    bool fakeCursorTextureLoadTried_ = false;
    int fakeCursorTextureLastTryTick_ = 0;
    std::string fakeCursorTexturePath_;
    std::string supportModeName_;
    std::mt19937 random_{ static_cast<std::uint32_t>(Variables::TickCount()) };

    mutable int lastFarmDebugFrameTick_ = 0;
    mutable int lastFarmDebugScanTick_ = 0;
    mutable int lastFarmDebugSelectTick_ = 0;
    mutable int lastFarmDebugOrbwalkTick_ = 0;
    mutable int lastFarmDebugAttackTick_ = 0;
    mutable int lastFarmDebugMoveTick_ = 0;
    mutable int lastFarmDebugKeyMask_ = 0;
    mutable int farmDebugFrameMask_ = -1;
    mutable bool farmDebugWasActive_ = false;
    mutable int aaDebugAttackId_ = 0;
    mutable int aaDebugLastCanMoveTick_ = 0;
    mutable unsigned aaDebugLastCanMoveReasonHash_ = 0;
    mutable bool aaDebugLastCanMoveValue_ = false;
    mutable bool aaDebugCanMoveInitialized_ = false;
    mutable int aaDebugLastMoveBlockTick_ = 0;
    mutable unsigned aaDebugLastMoveBlockReasonHash_ = 0;
    mutable std::vector<FarmMissileAttack> farmMissileAttacks_ = {};
    mutable int farmMissileVersion_ = 0;
    mutable int killableDrawCacheTick_ = 0;
    mutable std::vector<KillableDrawCircle> killableDrawCache_ = {};

    static constexpr int kBuffPolymorph = 10;
    static constexpr int kBuffFear = 22;
    static constexpr int kBuffBlind = 26;
};

} // namespace SDK
