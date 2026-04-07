#pragma once

namespace SDK {

enum class HitChance : int {
    Impossible = 0,
    OutOfRange = 1,
    Collision = 2,
    Low = 3,
    Medium = 4,
    High = 5,
    VeryHigh = 6,
    Dashing = 7,
    Immobile = 8
};

// Comparison operators so you can write: pred.Hitchance >= HitChance::High
inline bool operator>=(HitChance lhs, HitChance rhs) { return static_cast<int>(lhs) >= static_cast<int>(rhs); }
inline bool operator<=(HitChance lhs, HitChance rhs) { return static_cast<int>(lhs) <= static_cast<int>(rhs); }
inline bool operator>(HitChance lhs, HitChance rhs)  { return static_cast<int>(lhs) >  static_cast<int>(rhs); }
inline bool operator<(HitChance lhs, HitChance rhs)  { return static_cast<int>(lhs) <  static_cast<int>(rhs); }

} // namespace SDK
