#pragma once

#include "../../libs/nlohmann/json.hpp"

#include <cstring>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace SDK::Core::Utils {

class BinarySerializer {
public:
    template <typename T>
    static std::vector<std::uint8_t> Serialize(const T& value) {
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::vector<std::uint8_t> bytes(sizeof(T));
            std::memcpy(bytes.data(), &value, sizeof(T));
            return bytes;
        } else if constexpr (requires(const T& v) { nlohmann::json(v); }) {
            const std::string text = nlohmann::json(value).dump();
            return std::vector<std::uint8_t>(text.begin(), text.end());
        } else {
            static_assert(std::is_trivially_copyable_v<T>,
                          "BinarySerializer<T> needs either trivially-copyable storage or nlohmann::json support.");
        }
    }

    static std::vector<std::uint8_t> Serialize(const std::string& value) {
        return std::vector<std::uint8_t>(value.begin(), value.end());
    }

    template <typename T>
    static T Deserialize(const std::vector<std::uint8_t>& data) {
        if constexpr (std::is_same_v<T, std::string>) {
            return std::string(data.begin(), data.end());
        } else {
            if constexpr (std::is_trivially_copyable_v<T>) {
                T value{};
                if (data.size() >= sizeof(T)) {
                    std::memcpy(&value, data.data(), sizeof(T));
                }
                return value;
            } else if constexpr (requires(const nlohmann::json& j) { j.template get<T>(); }) {
                return nlohmann::json::parse(data.begin(), data.end()).template get<T>();
            } else {
                static_assert(std::is_trivially_copyable_v<T>,
                              "BinarySerializer<T> needs either trivially-copyable storage or nlohmann::json support.");
            }
        }
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using BinarySerializer = ::SDK::Core::Utils::BinarySerializer;
} // namespace SDK::Utils
