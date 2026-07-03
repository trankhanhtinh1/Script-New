#pragma once

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
        static_assert(std::is_trivially_copyable_v<T>,
                      "BinarySerializer<T> only supports trivially-copyable values in the native SDK port.");
        std::vector<std::uint8_t> bytes(sizeof(T));
        std::memcpy(bytes.data(), &value, sizeof(T));
        return bytes;
    }

    static std::vector<std::uint8_t> Serialize(const std::string& value) {
        return std::vector<std::uint8_t>(value.begin(), value.end());
    }

    template <typename T>
    static T Deserialize(const std::vector<std::uint8_t>& data) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "BinarySerializer<T> only supports trivially-copyable values in the native SDK port.");
        T value{};
        if (data.size() >= sizeof(T)) {
            std::memcpy(&value, data.data(), sizeof(T));
        }
        return value;
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using BinarySerializer = ::SDK::Core::Utils::BinarySerializer;
} // namespace SDK::Utils
