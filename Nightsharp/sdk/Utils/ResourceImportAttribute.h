#pragma once

#include "../../libs/nlohmann/json.hpp"

#include <functional>
#include <string>
#include <utility>

namespace SDK::Utils {

struct ResourceImportAttribute {
    std::string File = {};
    std::function<nlohmann::json(const nlohmann::json&)> Filter = {};

    ResourceImportAttribute() = default;

    explicit ResourceImportAttribute(std::string file)
        : File(std::move(file)) {}

    ResourceImportAttribute(std::string file, std::function<nlohmann::json(const nlohmann::json&)> filter)
        : File(std::move(file))
        , Filter(std::move(filter)) {}
};

} // namespace SDK::Utils
