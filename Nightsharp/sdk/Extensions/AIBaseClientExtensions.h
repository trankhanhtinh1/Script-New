#pragma once

#include "../Core/Objects.h"
#include "../Enums/BuffType.h"
#include "../Enums/GameObjectOrder.h"
#include "../../Core/CoreControl.h"
#include "../../Core/CoreBuffs.h"
#include "../../Core/CoreRuntime.h"
#include "../../Core/Globals.h"

namespace SDK {

// ── Attack timing (delegates to CoreControl) ──

inline float AttackDelay(const AIBaseClient& source) {
    return CoreControl::GetAttackDelay(source.Address());
}

inline float AttackWindup(const AIBaseClient& source) {
    return CoreControl::GetAttackWindup(source.Address());
}

inline float AttackCastDelay(const AIBaseClient& source) {
    return AttackWindup(source);
}

inline float AttackSpeed(const AIBaseClient& source) {
    const float delay = AttackDelay(source);
    return delay > 0.0f ? (1.0f / delay) : 0.0f;
}

// ── CharacterState flags ──

inline uint32_t ReadCharacterState(const AIBaseClient& source) {
    const uintptr_t a = source.Address();
    if (!a) return 0;
    return Globals::Read<uint32_t>(a + Offset::AttackableUnit::CharacterState);
}

inline bool CanAttack(const AIBaseClient& source) {
    return (ReadCharacterState(source) & 0x1u) != 0u;
}

inline bool CanCast(const AIBaseClient& source) {
    return (ReadCharacterState(source) & 0x4u) != 0u;
}

inline bool CanMove(const AIBaseClient& source) {
    return (ReadCharacterState(source) & 0x8u) != 0u;
}

inline bool CanWalk(const AIBaseClient& source) {
    return (ReadCharacterState(source) & 0x10u) == 0u;
}

// ── Buff type check ──

inline bool HasBuffOfType(const AIBaseClient& source, BuffType type) {
    return CoreBuffs::HasBuffType(source.Address(), static_cast<int>(type));
}

// ── IssueOrder (delegates to CoreControl::IssueOrder) ──

inline CoreControl::OrderType ToOrderType(GameObjectOrder order) {
    switch (order) {
        case GameObjectOrder::HoldPosition: return CoreControl::OrderType::Hold;
        case GameObjectOrder::MoveTo:       return CoreControl::OrderType::MoveTo;
        case GameObjectOrder::AttackUnit:   return CoreControl::OrderType::AttackUnit;
        case GameObjectOrder::AttackMove:   return CoreControl::OrderType::AttackMove;
        case GameObjectOrder::Stop:         return CoreControl::OrderType::Stop;
        case GameObjectOrder::PetAttack:    return CoreControl::OrderType::AutoAttackPet;
        case GameObjectOrder::PetMove:      return CoreControl::OrderType::MovePet;
        default:                            return CoreControl::OrderType::MoveTo;
    }
}

inline bool IssueOrder(const AIBaseClient& source, GameObjectOrder order,
                       Vector3 position, bool triggerEvent = true) {
    if (!source.IsValid()) return false;
    Vec3 pos{position.x, position.y, position.z};
    return CoreControl::IssueOrder(ToOrderType(order), pos, 0, triggerEvent);
}

inline bool IssueOrder(const AIBaseClient& source, GameObjectOrder order,
                       const AIBaseClient& target, bool triggerEvent = true) {
    if (!source.IsValid() || !target.IsValid()) return false;
    Vec3 pos{target.Position().x, target.Position().y, target.Position().z};
    return CoreControl::IssueOrder(ToOrderType(order), pos, target.Address(), triggerEvent);
}

} // namespace SDK
