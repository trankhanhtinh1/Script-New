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

    int MissileNetworkId() const {
        const int missileNetId = m_ref.GetMissileNetId();
        return missileNetId != 0 ? missileNetId : NetworkId();
    }

    int CasterNetworkId() const { return m_ref.GetMissileCasterNetId(); }
    int TargetNetworkId() const { return m_ref.GetMissileTargetNetId(); }
    Vector3 StartPosition() const { return m_ref.GetMissileStartPos(); }
    Vector3 EndPosition() const { return m_ref.GetMissileEndPos(); }
    Vector3 CastEndPosition() const { return m_ref.GetMissileCastEndPos(); }

    std::string SpellName() const {
        char buf[128] = {};
        const auto cast = m_ref.GetMissileCastInfo();
        if (cast.IsValid() && cast.ReadSpellName(buf, static_cast<int>(sizeof(buf))) && buf[0]) {
            return std::string(buf);
        }
        if (m_ref.ReadMissileSpellName(buf, static_cast<int>(sizeof(buf))) && buf[0]) {
            return std::string(buf);
        }
        if (m_ref.ReadMissileName(buf, static_cast<int>(sizeof(buf))) && buf[0]) {
            return std::string(buf);
        }
        return Name();
    }

    std::string MissileName() const {
        char buf[128] = {};
        if (m_ref.ReadMissileName(buf, static_cast<int>(sizeof(buf))) && buf[0]) {
            return std::string(buf);
        }
        return SpellName();
    }
};

} // namespace SDK
