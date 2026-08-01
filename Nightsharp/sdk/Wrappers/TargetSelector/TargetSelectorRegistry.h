#pragma once

#include "ITargetSelector.h"

#include <Windows.h>
#include <shlobj.h>

#include <array>
#include <string>
#include <unordered_map>

namespace SDK {

template <typename T>
class TargetSelectorRegistry {
public:
    TargetSelectorRegistry() {
        LoadPersistedSelection();
    }

    void SetDefaultImplementation(const std::string& name) {
        if (!name.empty()) defaultName_ = name;
    }

    bool Add(const std::string& name, T* implementation) {
        if (name.empty() || !implementation || implementations_.find(name) != implementations_.end()) {
            return false;
        }
        implementations_.emplace(name, implementation);
        if (!implementation_) {
            implementation_ = implementation;
            selectedName_ = name;
            implementation_->Resume();
        }

        // Loading is intentionally deferred until the named implementation is
        // registered.  This lets SDK bootstrap first while still restoring a
        // Kuro/Impulse choice when that plugin arrives later in the lifecycle.
        if (name == persistedName_ ||
            (persistedName_.empty() && name == defaultName_)) {
            SwitchTo(name, false);
        }
        return true;
    }

    bool Set(const std::string& name) {
        return SwitchTo(name, true);
    }

    T* Get(const std::string& name) const {
        const auto it = implementations_.find(name);
        return it != implementations_.end() ? it->second : nullptr;
    }

    bool Remove(const std::string& name) {
        const auto it = implementations_.find(name);
        if (it == implementations_.end()) return false;
        T* removed = it->second;
        if (implementation_ != removed) {
            implementations_.erase(it);
            return true;
        }

        // The active implementation is still owned by its plugin at this
        // point.  Suspend it before changing the pointer, then resume the
        // fallback while preserving the user's current target identity.
        const auto preserved = removed ? removed->GetSelectedTarget() : AIHeroClient();
        if (removed) removed->Suspend();

        T* fallback = nullptr;
        std::string fallbackName;
        const auto sdk = implementations_.find("SDK");
        if (sdk != implementations_.end() && sdk->second != removed) {
            fallback = sdk->second;
            fallbackName = "SDK";
        }

        if (!fallback) {
            std::string bestName;
            for (const auto& [candidateName, candidate] : implementations_) {
                if (candidateName == name || !candidate) continue;
                if (!fallback || candidateName < bestName) {
                    fallback = candidate;
                    fallbackName = candidateName;
                    bestName = candidateName;
                }
            }
        }

        implementation_ = fallback;
        selectedName_ = fallbackName;
        if (implementation_) {
            implementation_->SetTarget(preserved);
            implementation_->Resume();
        } else {
            selectedName_.clear();
        }
        implementations_.erase(it);
        return true;
    }

    T* Implementation() const { return implementation_; }
    const std::string& CurrentName() const { return selectedName_; }
    const std::string& PersistedName() const { return persistedName_; }

private:
    static std::string PersistencePath() {
        char appData[MAX_PATH] = {};
        if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appData) != S_OK || !appData[0]) {
            return "target_selector.ini";
        }

        char nightSharp[MAX_PATH] = {};
        wsprintfA(nightSharp, "%s\\NightSharp", appData);
        CreateDirectoryA(nightSharp, nullptr);

        char config[MAX_PATH] = {};
        wsprintfA(config, "%s\\config", nightSharp);
        CreateDirectoryA(config, nullptr);

        char path[MAX_PATH] = {};
        wsprintfA(path, "%s\\target_selector.ini", config);
        return path;
    }

    void LoadPersistedSelection() {
        std::array<char, 128> value = {};
        const std::string path = PersistencePath();
        GetPrivateProfileStringA(
            "TargetSelector", "implementation", "", value.data(),
            static_cast<DWORD>(value.size()), path.c_str());
        persistedName_ = value.data();
    }

    void PersistSelection() const {
        if (selectedName_.empty()) return;
        const std::string path = PersistencePath();
        WritePrivateProfileStringA(
            "TargetSelector", "implementation", selectedName_.c_str(), path.c_str());
    }

    bool SwitchTo(const std::string& name, bool persist) {
        const auto it = implementations_.find(name);
        if (it == implementations_.end() || !it->second) return false;

        T* next = it->second;
        if (implementation_ == next) {
            selectedName_ = name;
            if (persist) PersistSelection();
            return true;
        }

        const auto preserved = implementation_ ? implementation_->GetSelectedTarget() : AIHeroClient();
        if (implementation_) implementation_->Suspend();

        implementation_ = next;
        selectedName_ = name;
        implementation_->SetTarget(preserved);
        implementation_->Resume();

        if (persist) PersistSelection();
        return true;
    }

    std::unordered_map<std::string, T*> implementations_;
    T* implementation_ = nullptr;
    std::string selectedName_;
    std::string persistedName_;
    std::string defaultName_ = "Kuro";
};

} // namespace SDK
