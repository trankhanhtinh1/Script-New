#pragma once

#include "../../libs/nlohmann/json.hpp"
#include "../Constants.h"
#include "Logging.h"

#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace SDK::Core::Utils {

class Storage {
public:
    explicit Storage(std::string storageName = "Generic")
        : storageName_(std::move(storageName)) {
        ValidateName(storageName_);
        RegisterInstance();
    }

    Storage(std::string storageName, bool isAttribute)
        : storageName_(std::move(storageName)) {
        ValidateName(storageName_);
        if (!isAttribute) {
            RegisterInstance();
        }
    }

    Storage(const Storage& other)
        : contents_(other.contents_),
          storageName_(other.storageName_),
          bindings_(other.bindings_) {
        RegisterInstance();
    }

    Storage(Storage&& other) noexcept
        : contents_(std::move(other.contents_)),
          storageName_(std::move(other.storageName_)),
          bindings_(std::move(other.bindings_)) {
        RegisterInstance();
    }

    Storage& operator=(const Storage& other) {
        if (this != &other) {
            contents_ = other.contents_;
            storageName_ = other.storageName_;
            bindings_ = other.bindings_;
            ValidateName(storageName_);
            RegisterInstance();
        }
        return *this;
    }

    Storage& operator=(Storage&& other) noexcept {
        if (this != &other) {
            contents_ = std::move(other.contents_);
            storageName_ = std::move(other.storageName_);
            bindings_ = std::move(other.bindings_);
            RegisterInstance();
        }
        return *this;
    }

    ~Storage() {
        auto& list = StorageList();
        list.erase(std::remove(list.begin(), list.end(), this), list.end());
    }

    static bool Exists(const std::string& storageName = "Generic") {
        ValidateName(storageName);
        return std::filesystem::exists(PathFor(storageName));
    }

    static Storage Load(const std::string& storageName = "Generic") {
        ValidateName(storageName);
        Storage storage(storageName);
        const auto path = PathFor(storageName);
        if (!std::filesystem::exists(path)) {
            return storage;
        }

        std::ifstream stream(path);
        if (stream) {
            stream >> storage.contents_;
        }
        return storage;
    }

    template <typename T>
    bool Add(const std::string& key, const T& value) {
        if (contents_.contains(key)) {
            return false;
        }
        contents_[key] = value;
        return true;
    }

    template <typename T>
    T Get(const std::string& key) const {
        const auto it = contents_.find(key);
        return it != contents_.end() ? it->template get<T>() : T{};
    }

    bool Remove(const std::string& key) {
        return contents_.erase(key) != 0;
    }

    void Save() {
        SyncBindingsToContents();
        std::filesystem::create_directories(StoragePath());
        std::ofstream stream(PathFor(storageName_), std::ios::trunc);
        stream << contents_.dump(4);
    }

    template <typename T>
    bool Update(const std::string& key, const T& value) {
        if (!contents_.contains(key)) {
            return false;
        }
        contents_[key] = value;
        return true;
    }

    template <typename T>
    Storage& Bind(const std::string& key, T& value) {
        ValidateKey(key);
        Binding binding;
        binding.Key = key;
        binding.Getter = [&value]() { return nlohmann::json(value); };
        binding.Setter = [&value](const nlohmann::json& data) {
            if (!data.is_null()) {
                value = data.template get<T>();
            }
        };
        bindings_.push_back(std::move(binding));
        ApplyBindingFromContents(bindings_.back());
        return *this;
    }

    template <typename T, typename Getter, typename Setter>
    Storage& Bind(const std::string& key, Getter getter, Setter setter) {
        ValidateKey(key);
        Binding binding;
        binding.Key = key;
        binding.Getter = [getter = std::move(getter)]() mutable {
            return nlohmann::json(static_cast<T>(std::invoke(getter)));
        };
        binding.Setter = [setter = std::move(setter)](const nlohmann::json& data) mutable {
            if (!data.is_null()) {
                std::invoke(setter, data.template get<T>());
            }
        };
        bindings_.push_back(std::move(binding));
        ApplyBindingFromContents(bindings_.back());
        return *this;
    }

    void LoadBoundValues() {
        for (const auto& binding : bindings_) {
            ApplyBindingFromContents(binding);
        }
    }

    const nlohmann::json& Contents() const {
        return contents_;
    }

    const std::string& StorageName() const {
        return storageName_;
    }

    static void SaveAll() {
        for (auto* storage : StorageList()) {
            if (storage) {
                try {
                    storage->Save();
                } catch (...) {
                    Logging::Write()(LogLevel::Error, "Storage::SaveAll failed");
                }
            }
        }
    }

private:
    struct Binding {
        std::string Key;
        std::function<nlohmann::json()> Getter;
        std::function<void(const nlohmann::json&)> Setter;
    };

    static std::vector<Storage*>& StorageList() {
        static std::vector<Storage*> list;
        return list;
    }

    void RegisterInstance() {
        auto& list = StorageList();
        if (std::find(list.begin(), list.end(), this) == list.end()) {
            list.push_back(this);
        }
    }

    static std::filesystem::path StoragePath() {
        return SDK::Constants::EnsoulSharpAppData() / "Storage";
    }

    static std::filesystem::path PathFor(const std::string& storageName) {
        return StoragePath() / (storageName + ".storage");
    }

    static void ValidateName(const std::string& storageName) {
        if (storageName.empty() ||
            storageName.find_first_of("\\/:*?\"<>|") != std::string::npos) {
            throw std::invalid_argument("Storage name can't have invalid file name characters.");
        }
    }

    static void ValidateKey(const std::string& key) {
        if (key.empty()) {
            throw std::invalid_argument("Storage key can't be empty.");
        }
    }

    void ApplyBindingFromContents(const Binding& binding) {
        if (!binding.Setter) {
            return;
        }
        const auto it = contents_.find(binding.Key);
        if (it == contents_.end()) {
            return;
        }
        try {
            binding.Setter(*it);
        } catch (...) {
            Logging::Write()(LogLevel::Error, "Storage binding load failed for key %s", binding.Key.c_str());
        }
    }

    void SyncBindingsToContents() {
        for (const auto& binding : bindings_) {
            if (!binding.Getter) {
                continue;
            }
            try {
                contents_[binding.Key] = binding.Getter();
            } catch (...) {
                Logging::Write()(LogLevel::Error, "Storage binding save failed for key %s", binding.Key.c_str());
            }
        }
    }

    nlohmann::json contents_ = nlohmann::json::object();
    std::string storageName_ = "Generic";
    std::vector<Binding> bindings_;
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using Storage = ::SDK::Core::Utils::Storage;
} // namespace SDK::Utils
