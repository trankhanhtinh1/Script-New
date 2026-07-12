#pragma once

#include <string>
#include <unordered_map>

namespace SDK {

template <typename T>
class TargetSelectorRegistry {
public:
    bool Add(const std::string& name, T* implementation) {
        if (name.empty() || !implementation || implementations_.find(name) != implementations_.end()) {
            return false;
        }
        implementations_.emplace(name, implementation);
        if (!implementation_) {
            implementation_ = implementation;
            selectedName_ = name;
        }
        return true;
    }

    bool Set(const std::string& name) {
        const auto it = implementations_.find(name);
        if (it == implementations_.end() || !it->second) return false;
        implementation_ = it->second;
        selectedName_ = name;
        return true;
    }

    T* Get(const std::string& name) const {
        const auto it = implementations_.find(name);
        return it != implementations_.end() ? it->second : nullptr;
    }

    bool Remove(const std::string& name) {
        const auto it = implementations_.find(name);
        if (it == implementations_.end()) return false;
        T* removed = it->second;
        implementations_.erase(it);
        if (implementation_ == removed) {
            const auto sdk = implementations_.find("SDK");
            if (sdk != implementations_.end()) {
                implementation_ = sdk->second;
                selectedName_ = "SDK";
            } else {
                implementation_ = nullptr;
                selectedName_.clear();
            }
        }
        return true;
    }

    T* Implementation() const { return implementation_; }
    const std::string& CurrentName() const { return selectedName_; }

private:
    std::unordered_map<std::string, T*> implementations_;
    T* implementation_ = nullptr;
    std::string selectedName_;
};

} // namespace SDK
