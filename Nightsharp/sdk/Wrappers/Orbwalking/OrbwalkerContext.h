#pragma once

#include "OrbwalkerTypes.h"

namespace SDK {

struct OrbwalkerRuntimeContext {
    AttackableUnit forceTarget = {};
    AttackableUnit lastTarget = {};
    AttackableUnit laneClearMinion = {};
    Vector3 orbwalkerPosition = {};
    Vector3 lastMoveOrderPosition = {};

    int lastAutoAttackTick = 0;
    int lastMovementTick = 0;
    int lastMoveOrderTick = 0;
    int lastAttackOrderTick = 0;
    int lastAttackOrderNetworkId = 0;
    int pendingAttackTick = 0;
    int pendingAttackTargetNetworkId = 0;
    int lastAttackConfirmTick = 0;
    int allPauseTick = 0;
    int attackPauseTick = 0;
    int movePauseTick = 0;

    float attackDelayMs = 625.0f;
    float attackWindupMs = 300.0f;

    bool attackEnabled = true;
    bool moveEnabled = true;
    bool pendingAttack = false;
    bool hasConfirmedAttack = false;
    bool attackCastComplete = false;
    bool disposed = false;
    OrbwalkingMode activeMode = OrbwalkingMode::None;
};

} // namespace SDK
