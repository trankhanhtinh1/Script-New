#pragma once

#include <cstdint>
#include <type_traits>

namespace SDK {

enum class CenteredFlags : std::int32_t {
    None = 0,
    HorizontalLeft = 1 << 0,
    HorizontalCenter = 1 << 1,
    HorizontalRight = 1 << 2,
    VerticalUp = 1 << 3,
    VerticalCenter = 1 << 4,
    VerticalDown = 1 << 5,
};

constexpr CenteredFlags operator|(CenteredFlags lhs, CenteredFlags rhs) {
    using U = std::underlying_type_t<CenteredFlags>;
    return static_cast<CenteredFlags>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

constexpr CenteredFlags operator&(CenteredFlags lhs, CenteredFlags rhs) {
    using U = std::underlying_type_t<CenteredFlags>;
    return static_cast<CenteredFlags>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

constexpr CenteredFlags operator~(CenteredFlags value) {
    using U = std::underlying_type_t<CenteredFlags>;
    return static_cast<CenteredFlags>(~static_cast<U>(value));
}

inline CenteredFlags& operator|=(CenteredFlags& lhs, CenteredFlags rhs) {
    lhs = lhs | rhs;
    return lhs;
}

inline CenteredFlags& operator&=(CenteredFlags& lhs, CenteredFlags rhs) {
    lhs = lhs & rhs;
    return lhs;
}

constexpr bool HasFlag(CenteredFlags value, CenteredFlags flag) {
    using U = std::underlying_type_t<CenteredFlags>;
    return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
}

} // namespace SDK
