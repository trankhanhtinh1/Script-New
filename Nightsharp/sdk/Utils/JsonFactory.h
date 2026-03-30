#pragma once

#include "ResourceFactory.h"
#include "../../../Backup/Script-New/Nightsharp/libs/nlohmann/json.hpp"

#include <fstream>
#include <string>
#include <type_traits>

namespace SDK::Utils::JsonFactory {

using Json = nlohmann::json;

struct Settings {
    int Indent = 4;
    bool AllowExceptions = true;
};

inline Settings& DefaultSettings() {
    static Settings settings = {};
    return settings;
}

template<typename T>
inline T JsonString(const std::string& text, const Settings& settings = DefaultSettings()) {
    return Json::parse(text, nullptr, settings.AllowExceptions).template get<T>();
}

template<typename T>
inline bool TryJsonString(const std::string& text, T& value, const Settings& settings = DefaultSettings()) {
    try {
        value = JsonString<T>(text, settings);
        return true;
    } catch (...) {
        value = T{};
        return false;
    }
}

inline Json JsonString(const std::string& text, const Settings& settings = DefaultSettings()) {
    return Json::parse(text, nullptr, settings.AllowExceptions);
}

inline bool TryJsonString(const std::string& text, Json& value, const Settings& settings = DefaultSettings()) {
    try {
        value = JsonString(text, settings);
        return true;
    } catch (...) {
        value = Json();
        return false;
    }
}

template<typename T>
inline T JsonFile(const std::string& file, const Settings& settings = DefaultSettings()) {
    std::ifstream stream(file, std::ios::binary);
    return Json::parse(stream, nullptr, settings.AllowExceptions).template get<T>();
}

template<typename T>
inline bool TryJsonFile(const std::string& file, T& value, const Settings& settings = DefaultSettings()) {
    try {
        value = JsonFile<T>(file, settings);
        return true;
    } catch (...) {
        value = T{};
        return false;
    }
}

inline Json JsonFile(const std::string& file, const Settings& settings = DefaultSettings()) {
    std::ifstream stream(file, std::ios::binary);
    return Json::parse(stream, nullptr, settings.AllowExceptions);
}

inline bool TryJsonFile(const std::string& file, Json& value, const Settings& settings = DefaultSettings()) {
    try {
        value = JsonFile(file, settings);
        return true;
    } catch (...) {
        value = Json();
        return false;
    }
}

template<typename T>
inline T JsonResource(const std::string& file, const Settings& settings = DefaultSettings()) {
    return JsonString<T>(ResourceFactory::StringResource(file), settings);
}

template<typename T>
inline bool TryJsonResource(const std::string& file, T& value, const Settings& settings = DefaultSettings()) {
    try {
        value = JsonResource<T>(file, settings);
        return true;
    } catch (...) {
        value = T{};
        return false;
    }
}

inline Json JsonResource(const std::string& file, const Settings& settings = DefaultSettings()) {
    return JsonString(ResourceFactory::StringResource(file), settings);
}

inline bool TryJsonResource(const std::string& file, Json& value, const Settings& settings = DefaultSettings()) {
    try {
        value = JsonResource(file, settings);
        return true;
    } catch (...) {
        value = Json();
        return false;
    }
}

template<typename T, typename Parser>
inline T JsonResource(const std::string& file, Parser&& parser, const Settings& settings = DefaultSettings()) {
    if constexpr (std::is_invocable_r_v<T, Parser, const Json&>) {
        return parser(JsonResource(file, settings));
    } else {
        return parser(ResourceFactory::StringResource(file));
    }
}

template<typename T>
inline void ToFile(const std::string& file, const T& object, const Settings& settings = DefaultSettings()) {
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream << Json(object).dump(settings.Indent);
}

template<typename T>
inline bool TryToFile(const std::string& file, const T& object, const Settings& settings = DefaultSettings()) {
    try {
        ToFile(file, object, settings);
        return true;
    } catch (...) {
        return false;
    }
}

template<typename T>
inline std::string ToString(const T& object, const Settings& settings = DefaultSettings()) {
    return Json(object).dump(settings.Indent);
}

} // namespace SDK::Utils::JsonFactory
