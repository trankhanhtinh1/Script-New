#pragma once

#include "../Database/SpellData.h"

namespace ZDEvade {

enum class ThreatVisualBody {
    None,
    Capsule,
    Circle,
    Ring,
    Sector
};

struct ThreatVisualDispatch {
    ThreatVisualBody body = ThreatVisualBody::None;
    bool endExplosion = false;
};

constexpr ThreatVisualDispatch GetThreatVisualDispatch(
        ZDSpellType type,
        bool hasEndExplosion) {
    switch (type) {
    case ZDSpellType::Line:
        return {ThreatVisualBody::Capsule, hasEndExplosion};
    case ZDSpellType::Circular:
        return {ThreatVisualBody::Circle, hasEndExplosion};
    case ZDSpellType::Ring:
        return {ThreatVisualBody::Ring, hasEndExplosion};
    case ZDSpellType::Cone:
        return {ThreatVisualBody::Sector, hasEndExplosion};
    case ZDSpellType::Arc:
        return {ThreatVisualBody::None, false};
    }
    return {};
}

} // namespace ZDEvade
