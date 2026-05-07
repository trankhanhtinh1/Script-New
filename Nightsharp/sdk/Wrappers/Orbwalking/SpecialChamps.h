#pragma once

namespace SDK::OrbwalkingSpecial {

struct SpecialState {};

inline void DetectSelf(SpecialState&) {}
inline void DetectOpponents(SpecialState&) {}
inline void OnUpdate(SpecialState&) {}
inline int CanAttackOverride(const SpecialState&) { return 0; }

} // namespace SDK::OrbwalkingSpecial
