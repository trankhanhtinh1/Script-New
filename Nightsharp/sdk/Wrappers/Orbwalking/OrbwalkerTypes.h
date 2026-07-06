#pragma once

#include "../../Core/Objects.h"
#include "../../Enumerations/OrbwalkingMode.h"
#include "../../Enumerations/OrbwalkingType.h"
#include "../../UI/UI.h"

#include <cstdint>

namespace SDK {

enum class OrbwalkerModeFlag : std::int32_t {
    None = -1,
    Combo = 1,
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
    virtual bool ShouldWait() = 0;
    virtual void ResetAutoAttackTimer() = 0;
    virtual void Dispose() = 0;
};

} // namespace SDK
