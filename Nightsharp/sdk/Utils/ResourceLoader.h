#pragma once

#include "Logging.h"
#include "ResourceFactory.h"
#include "ResourceImportAttribute.h"
#include "../../libs/nlohmann/json.hpp"

#include <functional>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

namespace SDK::Utils::ResourceLoader {

namespace detail {
    struct ImportEntry {
        std::string Name = {};
        std::string File = {};
        std::function<void()> Loader = {};
    };

    inline std::unordered_map<std::string, ImportEntry>*& Imports() {
        static auto* imports = new(std::nothrow) std::unordered_map<std::string, ImportEntry>();
        return imports;
    }

    inline bool& Initialized() {
        static bool initialized = false;
        return initialized;
    }
}

inline std::vector<uint8_t> Binary(const std::string& file) {
    return ResourceFactory::BinaryResource(file);
}

inline std::string String(const std::string& file) {
    return ResourceFactory::StringResource(file);
}

inline void RegisterImport(const std::string& name, std::function<void()> loader) {
    auto* imports = detail::Imports();
    if (!imports || name.empty() || !loader) {
        return;
    }

    auto& entry = (*imports)[name];
    entry.Name = name;
    entry.Loader = std::move(loader);
}

template<typename T>
inline void RegisterJsonImport(const std::string& name,
                               const ResourceImportAttribute& import,
                               std::function<void(const T&)> setter) {
    RegisterImport(name, [import, setter = std::move(setter)]() {
        if (!setter || import.File.empty()) {
            return;
        }

        try {
            auto value = nlohmann::json::parse(String(import.File));
            if (import.Filter) {
                value = import.Filter(value);
            }
            setter(value.get<T>());
        } catch (...) {
            Logging::Write(true, true, "ResourceLoader::RegisterJsonImport")(
                LogLevel::Error,
                "Unable to import resource '%s'.",
                import.File.c_str());
        }
    });
    if (auto* imports = detail::Imports(); imports) {
        (*imports)[name].File = import.File;
    }
}

inline void RegisterStringImport(const std::string& name,
                                 const ResourceImportAttribute& import,
                                 std::function<void(const std::string&)> setter) {
    RegisterImport(name, [import, setter = std::move(setter)]() {
        if (!setter || import.File.empty()) {
            return;
        }

        std::string value = String(import.File);
        if (import.Filter) {
            try {
                value = import.Filter(nlohmann::json::parse(value)).dump();
            } catch (...) {
            }
        }
        setter(value);
    });
    if (auto* imports = detail::Imports(); imports) {
        (*imports)[name].File = import.File;
    }
}

inline void RegisterBinaryImport(const std::string& name,
                                 const ResourceImportAttribute& import,
                                 std::function<void(const std::vector<uint8_t>&)> setter) {
    RegisterImport(name, [import, setter = std::move(setter)]() {
        if (!setter || import.File.empty()) {
            return;
        }
        setter(Binary(import.File));
    });
    if (auto* imports = detail::Imports(); imports) {
        (*imports)[name].File = import.File;
    }
}

inline bool ContainsImport(const std::string& name) {
    if (auto* imports = detail::Imports(); imports) {
        return imports->find(name) != imports->end();
    }
    return false;
}

inline bool InvokeImport(const std::string& name) {
    if (auto* imports = detail::Imports(); imports) {
        auto it = imports->find(name);
        if (it != imports->end() && it->second.Loader) {
            it->second.Loader();
            return true;
        }
    }
    return false;
}

inline void RemoveImport(const std::string& name) {
    if (auto* imports = detail::Imports(); imports) {
        imports->erase(name);
    }
}

inline void Initialize() {
    if (detail::Initialized()) {
        return;
    }
    auto* imports = detail::Imports();
    if (!imports) {
        return;
    }

    for (const auto& [name, entry] : *imports) {
        (void)name;
        if (entry.Loader) {
            entry.Loader();
        }
    }
    detail::Initialized() = true;
}

inline void Reset() {
    if (auto* imports = detail::Imports()) {
        imports->clear();
    }
    detail::Initialized() = false;
}

} // namespace SDK::Utils::ResourceLoader
