#pragma once

#include "../../libs/nlohmann/json.hpp"
#include "ResourceFactory.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace SDK::Core::Utils {

class JsonFactory {
public:
    template <typename T>
    static T JsonResource(const std::string& file) {
        return nlohmann::json::parse(ResourceFactory::StringResource(file)).get<T>();
    }

    static nlohmann::json JsonResource(const std::string& file) {
        return nlohmann::json::parse(ResourceFactory::StringResource(file));
    }

    template <typename T>
    static T JsonFile(const std::string& file) {
        return ReadJsonFile(file).get<T>();
    }

    static nlohmann::json JsonFile(const std::string& file) {
        return ReadJsonFile(file);
    }

    template <typename T>
    static T JsonString(const std::string& text) {
        return nlohmann::json::parse(text).get<T>();
    }

    static nlohmann::json JsonString(const std::string& text) {
        return nlohmann::json::parse(text);
    }

    template <typename T>
    static void ToFile(const std::string& file, const T& value) {
        std::ofstream stream(file, std::ios::trunc);
        if (!stream) {
            throw std::runtime_error("JsonFactory::ToFile cannot open file");
        }
        stream << nlohmann::json(value).dump(4);
    }

    template <typename T>
    static std::string ToString(const T& value) {
        return nlohmann::json(value).dump(4);
    }

private:
    static nlohmann::json ReadJsonFile(const std::string& file) {
        if (file.empty()) {
            throw std::invalid_argument("file");
        }

        std::ifstream stream(file);
        if (!stream) {
            throw std::runtime_error("JsonFactory::JsonFile cannot open file");
        }
        return nlohmann::json::parse(stream);
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using JsonFactory = ::SDK::Core::Utils::JsonFactory;
} // namespace SDK::Utils
