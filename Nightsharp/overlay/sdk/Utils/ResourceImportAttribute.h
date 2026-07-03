#pragma once

#include <string>
#include <utility>

namespace SDK::Core::Utils {

struct ResourceImportAttribute {
    ResourceImportAttribute() = default;
    explicit ResourceImportAttribute(std::string file)
        : File(std::move(file)) {}

    std::string File = {};
    const char* Filter = nullptr;
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using ResourceImportAttribute = ::SDK::Core::Utils::ResourceImportAttribute;
} // namespace SDK::Utils
