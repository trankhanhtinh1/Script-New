#pragma once

// ============================================================================
// MissileClient — projectile / in-flight skillshot entity
// ============================================================================
// Inherits GameObject to reuse the identity / position / validity surface, and
// adds missile-specific reads: caster / target netId (so trackers can stitch
// a missile back to the ability that spawned it) and start / end position for
// linear or line-skillshot prediction. Missile width / speed / delay come
// from the static `SpellData` lookup in `sdk/Data/Database.h`, not from this
// wrapper, because the client-side missile object only carries the flight
// endpoints. Do NOT add stat accessors here — missiles are non-combat
// entities and most `AttackableUnit` fields read garbage on them.
// ============================================================================

#include "GameObject.h"

namespace SDK {

class MissileClient : public GameObject {
public:
    using GameObject::GameObject;

    int CasterNetworkId() const { return m_ref.GetMissileCasterNetId(); }
    int TargetNetworkId() const { return m_ref.GetMissileTargetNetId(); }
    Vector3 StartPosition() const { return m_ref.GetMissileStartPos(); }
    Vector3 EndPosition() const { return m_ref.GetMissileEndPos(); }
};

} // namespace SDK
