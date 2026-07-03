#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace SDK::Core::Utils {

class ResourceFactory {
public:
    static void RegisterResource(const std::string& name, const std::vector<std::uint8_t>& bytes) {
        Resources()[name] = bytes;
    }

    static std::string StringResource(const std::string& file) {
        const auto bytes = ByteResource(file);
        return std::string(bytes.begin(), bytes.end());
    }

    static std::vector<std::uint8_t> ByteResource(const std::string& file) {
        if (file.empty()) {
            throw std::invalid_argument("file");
        }

        for (const auto& pair : Resources()) {
            if (EndsWith(pair.first, file)) {
                return pair.second;
            }
        }

        if (std::filesystem::exists(file)) {
            std::ifstream stream(file, std::ios::binary);
            return std::vector<std::uint8_t>(
                std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>());
        }

        // TODO(Resource parity): .NET Assembly.GetManifestResourceNames has no
        // native equivalent here. Register embedded bytes with RegisterResource
        // or pass a real file path.
        throw std::runtime_error("resourceFile Embedded Resource not found");
    }

private:
    static std::unordered_map<std::string, std::vector<std::uint8_t>>& Resources() {
        static std::unordered_map<std::string, std::vector<std::uint8_t>> resources;
        return resources;
    }

    static bool EndsWith(const std::string& value, const std::string& suffix) {
        return value.size() >= suffix.size() &&
               value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using ResourceFactory = ::SDK::Core::Utils::ResourceFactory;
} // namespace SDK::Utils
