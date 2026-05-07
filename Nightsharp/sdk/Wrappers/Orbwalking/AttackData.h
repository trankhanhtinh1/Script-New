#pragma once

#include "../../Utils/AutoAttack.h"

namespace SDK::OrbwalkingData {

inline bool IsAutoAttack(const std::string& spellName) {
    return Utils::AutoAttack::IsAutoAttack(spellName);
}

inline bool IsAutoAttackReset(const std::string& spellName) {
    return Utils::AutoAttack::IsAutoAttackReset(spellName);
}

} // namespace SDK::OrbwalkingData
