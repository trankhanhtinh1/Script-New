#pragma once

#include "../../Enumerations/OrbwalkerMode.h"
#include "../../Enumerations/OrbwalkingType.h"
#include "../../Core/Game.h"
#include "../../Core/Objects.h"
#include "../../Utils/AutoAttack.h"

#include <algorithm>
#include <cstdint>

namespace SDK {

// --- Event args matching EnsoulSharp OrbwalkingActionArgs ---
struct OrbwalkingActionArgs {
    AIBaseClient Sender = {};
    AIBaseClient Target = {};
    Vector3 Position = {};
    bool Process = true;
    OrbwalkingType Type = OrbwalkingType::None;
};

// BeforeAttack / AfterAttack aliases kept for backward compat
using BeforeAttackEventArgs = OrbwalkingActionArgs;
using AfterAttackEventArgs  = OrbwalkingActionArgs;

// --- OrbwalkerBase – matches EnsoulSharp OrbwalkerBase<TK, T> ---
class OrbwalkerBase {
public:
    using ActionHandler = void(*)(OrbwalkingActionArgs&);

    // ── Properties (matching C#) ──

    OrbwalkerMode ActiveMode   = OrbwalkerMode::None;
    bool          AttackState  = true;   // SetAttackState
    bool          MovementState = true;  // SetMovementState

    int  LastAutoAttackCommandTick = 0;
    int  LastAutoAttackTick        = 0;
    int  LastMovementOrderTick     = 0;
    int  TotalAutoAttacks          = 0;
    bool MissileLaunched           = false;

    AIBaseClient  LastTarget       = {};

    // ── CanAttack – matching C# logic:
    //    TickCount + Ping/2 + 25 >= LastAutoAttackTick + AttackDelay*1000 + extra
    bool CanAttack(float extraWindup = 0.0f) const {
        const int now = Game::TickCount();
        const int ping = CoreAPI::Control::GetPing();
        const float attackDelayMs = CoreAPI::Control::GetAttackDelay() * 1000.0f;
        return (now + (ping / 2) + 25) >= (LastAutoAttackTick + static_cast<int>(attackDelayMs + extraWindup));
    }

    // ── CanMove – matching C# OrbwalkerBase.CanMove ──
    //    1. If MissileLaunched && missile check enabled → safe to move (missile confirmed release)
    //    2. If CanCancelAutoAttack() is false (Kalista) → always can move
    //    3. Otherwise → timing gate: TickCount + Ping/2 >= LastAutoAttackTick + windupMs + extra
    bool CanMove(float extraWindup = 0.0f, bool disableMissileCheck = false) const {
        // C# line 248-251: if (this.MissileLaunched && !disableMissileCheck) return true;
        if (MissileLaunched && !disableMissileCheck) {
            return true;
        }

        const int now = Game::TickCount();
        const int ping = CoreAPI::Control::GetPing();
        const float windupMs = CoreAPI::Control::GetAttackWindup() * 1000.0f;
        // C# line 253-255: !CanCancelAutoAttack() || (timing check)
        return (now + (ping / 2)) >= (LastAutoAttackTick + static_cast<int>(windupMs + extraWindup));
    }

    bool CanOrbwalk(const AIBaseClient& target, float range = 0.0f, float extraWindup = 0.0f) const {
        const auto player = ObjectManager::Player();
        const float r = range > 0.0f ? range : (player.AttackRange() + player.BoundingRadius());
        return target.IsValidTarget(r, player.Position()) && CanAttack(extraWindup);
    }

    void ResetSwingTimer() { LastAutoAttackTick = 0; }

    void SetAttackState(bool state)   { AttackState = state; }
    void SetMovementState(bool state) { MovementState = state; }

    // ── Event registration ──

    bool AddOnAction(ActionHandler handler) {
        if (!handler) return false;
        for (int i = 0; i < kMaxHandlers; ++i) {
            if (s_actionHandlers[i] == nullptr) {
                s_actionHandlers[i] = handler;
                return true;
            }
        }
        return false;
    }

    // Backward-compat aliases
    bool AddOnBeforeAttack(ActionHandler h) { return AddOnAction(h); }
    bool AddOnAfterAttack(ActionHandler h)  { return AddOnAction(h); }
    bool OnBeforeAttack(ActionHandler h)    { return AddOnAction(h); }
    bool OnAfterAttack(ActionHandler h)     { return AddOnAction(h); }

protected:
    void InvokeAction(OrbwalkingActionArgs& e) const {
        for (int i = 0; i < kMaxHandlers && s_actionHandlers[i]; ++i) {
            s_actionHandlers[i](e);
        }
    }

    // ── Spell cast callbacks (matching OrbwalkerBase.cs logic) ──

    // Gọi khi phát hiện OnDoCast auto-attack (poll-based)
    void OnDetectAutoAttackStarted(const AIBaseClient& sender, const std::string& spellName, const AIBaseClient& target) {
        if (!sender.IsValid() || !sender.IsMe()) return;

        if (target.IsValid() && AutoAttack::IsAutoAttack(spellName)) {
            // Keep the actual local detection tick. Backdating by ping/2 makes
            // the movement gate open too early and can cancel the auto.
            LastAutoAttackTick = Game::TickCount();
            MissileLaunched = false;
            LastMovementOrderTick = 0;
            TotalAutoAttacks++;

            if (LastTarget.IsValid() && target.NetworkId() != LastTarget.NetworkId()) {
                OrbwalkingActionArgs e{};
                e.Target = target;
                e.Sender = sender;
                e.Type = OrbwalkingType::TargetSwitch;
                InvokeAction(e);
            }
            LastTarget = target;

            OrbwalkingActionArgs e{};
            e.Target = target;
            e.Sender = sender;
            e.Type = OrbwalkingType::OnAttack;
            InvokeAction(e);
        }

        if (AutoAttack::IsAutoAttackReset(spellName)) {
            ResetSwingTimer();
        }
    }

    // Gọi khi phát hiện missile launched (OnProcessSpellCast auto-attack)
    void OnDetectAutoAttackMissile(const AIBaseClient& sender, const std::string& spellName, const AIBaseClient& target) {
        if (!sender.IsValid() || !sender.IsMe()) return;

        if (AutoAttack::IsAutoAttackReset(spellName)) {
            ResetSwingTimer();
        } else if (AutoAttack::IsAutoAttack(spellName)) {
            MissileLaunched = true;
            OrbwalkingActionArgs e{};
            e.Target = target;
            e.Sender = sender;
            e.Type = OrbwalkingType::AfterAttack;
            InvokeAction(e);
        }
    }

    // Gọi khi stop cast (cancel AA)
    void OnDetectStopCast(bool destroyMissile, bool keepAnimation) {
        if (destroyMissile && keepAnimation) {
            ResetSwingTimer();
        }
    }

private:
    static constexpr int kMaxHandlers = 32;
    static inline ActionHandler s_actionHandlers[kMaxHandlers] = {};
};

} // namespace SDK
