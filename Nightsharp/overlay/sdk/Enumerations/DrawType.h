#pragma once

#include <cstdint>
#include <type_traits>

namespace SDK {

enum class DrawType : std::int32_t {
    OnBeginScene,
    OnDraw,
    OnEndScene,
};

constexpr DrawType operator|(DrawType lhs, DrawType rhs) {
    using U = std::underlying_type_t<DrawType>;
    return static_cast<DrawType>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

constexpr DrawType operator&(DrawType lhs, DrawType rhs) {
    using U = std::underlying_type_t<DrawType>;
    return static_cast<DrawType>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

constexpr DrawType operator~(DrawType value) {
    using U = std::underlying_type_t<DrawType>;
    return static_cast<DrawType>(~static_cast<U>(value));
}

inline DrawType& operator|=(DrawType& lhs, DrawType rhs) {
    lhs = lhs | rhs;
    return lhs;
}

inline DrawType& operator&=(DrawType& lhs, DrawType rhs) {
    lhs = lhs & rhs;
    return lhs;
}

constexpr bool HasFlag(DrawType value, DrawType flag) {
    using U = std::underlying_type_t<DrawType>;
    return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
}

} // namespace SDK
