#pragma once

#include <cstddef>
#include <string>

namespace SDK::Data::Patch26_6 {

extern const std::size_t kJsonSize;

const std::string& Json();

inline const char* CStr() {
    return Json().c_str();
}

} // namespace SDK::Data::Patch26_6
