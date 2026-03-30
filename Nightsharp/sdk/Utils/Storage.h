#pragma once

#include "BinarySerializer.h"
#include "JsonFactory.h"
#include "../Core/Constants.h"

#include <Windows.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SDK::Utils {

class Storage {
public:
    struct Binding {
        std::string Key = {};
        std::function<JsonFactory::Json()> Getter = {};
        std::function<void(const JsonFactory::Json&)> Setter = {};
    };

    struct FileEnvelope {
        std::string StorageName = {};
        std::vector<std::string> StorageTypes = {};
        std::string JsonText = {};

        void Serialize(BinarySerializer::BinaryWriter& writer) const {
            BinarySerializer::SerializeValue(writer, StorageName);
            BinarySerializer::SerializeValue(writer, StorageTypes);
            BinarySerializer::SerializeValue(writer, JsonText);
        }

        void Deserialize(BinarySerializer::BinaryReader& reader) {
            BinarySerializer::DeserializeValue(reader, StorageName);
            BinarySerializer::DeserializeValue(reader, StorageTypes);
            BinarySerializer::DeserializeValue(reader, JsonText);
        }
    };

    explicit Storage(std::string storageName = "Generic", std::vector<std::string> storageTypes = {})
        : m_storageName(IsValidStorageName(storageName) ? std::move(storageName) : std::string("Generic"))
        , m_storageTypes(std::move(storageTypes)) {}

    static bool IsValidStorageName(const std::string& storageName) {
        if (storageName.empty()) {
            return false;
        }

        const std::string chars = "\\/:*?\"<>|";
        return std::none_of(storageName.begin(), storageName.end(), [&](char ch) {
            return chars.find(ch) != std::string::npos;
        });
    }

    static std::string StoragePath() {
        const std::string path = Constants::PublicLogDirectory() + "\\NightSharpStorage";
        ::CreateDirectoryA(path.c_str(), nullptr);
        return path;
    }

    static std::string ResolveFile(const std::string& storageName) {
        return StoragePath() + "\\" + storageName + ".storage";
    }

    static bool Exists(const std::string& storageName = "Generic") {
        if (!IsValidStorageName(storageName)) {
            return false;
        }
        std::ifstream stream(ResolveFile(storageName), std::ios::binary);
        return stream.good();
    }

    static Storage Load(const std::string& storageName = "Generic", std::vector<std::string> storageTypes = {}) {
        Storage storage(storageName, std::move(storageTypes));
        if (!Exists(storage.m_storageName)) {
            TrackStorageName(storage.m_storageName);
            return storage;
        }

        FileEnvelope envelope;
        if (BinarySerializer::DeserializeFromFile(ResolveFile(storage.m_storageName), envelope)) {
            if (!envelope.StorageName.empty()) {
                storage.m_storageName = envelope.StorageName;
            }
            storage.m_storageTypes = std::move(envelope.StorageTypes);
            if (!envelope.JsonText.empty()) {
                storage.m_json = JsonFactory::JsonString(envelope.JsonText);
            }
        }

        TrackStorageName(storage.m_storageName);
        return storage;
    }

    static Storage LoadOrCreate(const std::string& storageName = "Generic", std::vector<std::string> storageTypes = {}) {
        return Load(storageName, std::move(storageTypes));
    }

    static void RegisterBinding(const std::string& storageName, Binding binding) {
        auto* registry = GetBindings();
        if (!registry || !IsValidStorageName(storageName) || binding.Key.empty()) {
            return;
        }

        TrackStorageName(storageName);
        (*registry)[storageName].push_back(std::move(binding));
    }

    static void SaveRegistered(const std::string& storageName = "Generic") {
        Storage storage = Load(storageName);
        if (auto* registry = GetBindings(); registry) {
            auto it = registry->find(storage.storageName());
            if (it != registry->end()) {
                for (const auto& binding : it->second) {
                    if (binding.Getter) {
                        storage.m_json[binding.Key] = binding.Getter();
                    }
                }
            }
        }
        storage.Save();
    }

    static void LoadRegistered(const std::string& storageName = "Generic") {
        const Storage storage = Load(storageName);
        if (auto* registry = GetBindings(); registry) {
            auto it = registry->find(storage.storageName());
            if (it != registry->end()) {
                for (const auto& binding : it->second) {
                    if (binding.Setter && storage.m_json.contains(binding.Key)) {
                        binding.Setter(storage.m_json.at(binding.Key));
                    }
                }
            }
        }
    }

    static void SaveAllRegistered() {
        if (auto* names = GetTrackedStorageNames(); names) {
            for (const auto& name : *names) {
                SaveRegistered(name);
            }
        }
    }

    static void LoadAllRegistered() {
        if (auto* names = GetTrackedStorageNames(); names) {
            for (const auto& name : *names) {
                LoadRegistered(name);
            }
        }
    }

    template<typename T>
    T Get(const std::string& key, const T& fallback = T{}) const {
        if (!m_json.contains(key)) {
            return fallback;
        }
        try {
            return m_json.at(key).template get<T>();
        } catch (...) {
            return fallback;
        }
    }

    template<typename T>
    bool Add(const std::string& key, const T& value) {
        if (m_json.contains(key)) {
            return false;
        }
        m_json[key] = value;
        return true;
    }

    template<typename T>
    void Set(const std::string& key, const T& value) {
        m_json[key] = value;
    }

    template<typename T>
    bool Update(const std::string& key, const T& value) {
        if (!m_json.contains(key)) {
            return false;
        }
        m_json[key] = value;
        return true;
    }

    bool Contains(const std::string& key) const {
        return m_json.contains(key);
    }

    bool Remove(const std::string& key) {
        return m_json.erase(key) > 0;
    }

    void RegisterType(std::string typeName) {
        if (!typeName.empty() &&
            std::find(m_storageTypes.begin(), m_storageTypes.end(), typeName) == m_storageTypes.end()) {
            m_storageTypes.push_back(std::move(typeName));
        }
    }

    const std::vector<std::string>& storageTypes() const {
        return m_storageTypes;
    }

    const std::string& storageName() const {
        return m_storageName;
    }

    void Save() const {
        FileEnvelope envelope;
        envelope.StorageName = m_storageName;
        envelope.StorageTypes = m_storageTypes;
        envelope.JsonText = JsonFactory::ToString(m_json);
        BinarySerializer::SerializeToFile(ResolveFile(m_storageName), envelope);
    }

    const JsonFactory::Json& Contents() const {
        return m_json;
    }

private:
    static void TrackStorageName(const std::string& storageName) {
        auto* names = GetTrackedStorageNames();
        if (!names || storageName.empty()) {
            return;
        }

        if (std::find(names->begin(), names->end(), storageName) == names->end()) {
            names->push_back(storageName);
        }
    }

    static std::unordered_map<std::string, std::vector<Binding>>*& GetBindings() {
        static auto* bindings = new(std::nothrow) std::unordered_map<std::string, std::vector<Binding>>();
        return bindings;
    }

    static std::vector<std::string>*& GetTrackedStorageNames() {
        static auto* names = new(std::nothrow) std::vector<std::string>();
        return names;
    }

    std::string m_storageName = {};
    std::vector<std::string> m_storageTypes = {};
    JsonFactory::Json m_json = JsonFactory::Json::object();
};

} // namespace SDK::Utils
