#pragma once

#include "../../libs/nlohmann/json.hpp"
#include "Logging.h"

#include <Windows.h>
#include <algorithm>
#include <filesystem>
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
        StorageList().push_back(this);
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

    void Save() const {
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
    static std::vector<Storage*>& StorageList() {
        static std::vector<Storage*> list;
        return list;
    }

    static std::filesystem::path StoragePath() {
        char appData[MAX_PATH] = {};
        DWORD len = GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
        std::filesystem::path root = (len > 0 && len < MAX_PATH)
            ? std::filesystem::path(appData)
            : std::filesystem::path("C:\\Users\\Public");
        return root / "NightSharp" / "Storage";
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

    nlohmann::json contents_ = nlohmann::json::object();
    std::string storageName_ = "Generic";
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using Storage = ::SDK::Core::Utils::Storage;
} // namespace SDK::Utils
