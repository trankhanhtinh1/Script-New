#pragma once

#include "../Core/Constants.h"

#include <functional>
#include <fstream>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

namespace SDK::Utils::ResourceFactory {

namespace detail {
    using Provider = std::function<std::vector<uint8_t>()>;

    inline std::unordered_map<std::string, Provider>*& Providers() {
        static auto* providers = new(std::nothrow) std::unordered_map<std::string, Provider>();
        return providers;
    }
}

inline void Register(const std::string& file, detail::Provider provider) {
    auto* providers = detail::Providers();
    if (!providers || file.empty() || !provider) {
        return;
    }

    (*providers)[file] = std::move(provider);
}

inline void RegisterBinary(const std::string& file, std::vector<uint8_t> bytes) {
    Register(file, [bytes = std::move(bytes)]() {
        return bytes;
    });
}

inline void RegisterString(const std::string& file, std::string text) {
    Register(file, [text = std::move(text)]() {
        return std::vector<uint8_t>(text.begin(), text.end());
    });
}

inline bool Contains(const std::string& file) {
    if (auto* providers = detail::Providers(); providers) {
        if (providers->find(file) != providers->end()) {
            return true;
        }
    }

    if (file.empty()) {
        return false;
    }

    const std::string resolved =
        (file.size() > 2 && file[1] == ':') ? file : (Constants::BaseDirectory() + "\\" + file);
    std::ifstream stream(resolved, std::ios::binary);
    return stream.good();
}

inline std::string ResolvePath(const std::string& file) {
    if (file.empty()) {
        return {};
    }
    return (file.size() > 2 && file[1] == ':') ? file : (Constants::BaseDirectory() + "\\" + file);
}

inline std::vector<uint8_t> BinaryResource(const std::string& file) {
    if (auto* providers = detail::Providers(); providers) {
        auto it = providers->find(file);
        if (it != providers->end() && it->second) {
            return it->second();
        }
    }

    if (file.empty()) {
        return {};
    }

    const std::string resolved = ResolvePath(file);

    std::ifstream stream(resolved, std::ios::binary);
    if (!stream.is_open()) {
        return {};
    }

    stream.seekg(0, std::ios::end);
    const auto size = static_cast<size_t>(stream.tellg());
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> bytes(size);
    if (size > 0) {
        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    }
    return bytes;
}

inline std::string StringResource(const std::string& file) {
    const auto bytes = BinaryResource(file);
    return std::string(bytes.begin(), bytes.end());
}

inline void Reset() {
    if (auto* providers = detail::Providers()) {
        providers->clear();
    }
}

} // namespace SDK::Utils::ResourceFactory
