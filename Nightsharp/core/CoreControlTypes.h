#pragma once

#include <cstdint>

namespace CoreControl {

enum class OrderIssueResult : std::uint8_t {
    Issued,
    Throttled,
    Blocked,
    Failed,
};

} // namespace CoreControl
