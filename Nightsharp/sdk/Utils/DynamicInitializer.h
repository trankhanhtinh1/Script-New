#pragma once

#include <functional>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace SDK::Utils::DynamicInitializer {

namespace detail {
    struct State {
        std::unordered_map<std::type_index, std::function<std::shared_ptr<void>()>> TypeFactories = {};
        std::unordered_map<std::string, std::function<std::shared_ptr<void>()>> NamedFactories = {};
        std::unordered_map<std::string, std::function<void()>> DeferredInitializers = {};
    };

    inline State*& GetState() {
        static auto* state = new(std::nothrow) State();
        return state;
    }
}

template<typename T>
inline std::shared_ptr<T> NewInstance() {
    static_assert(!std::is_array_v<T>, "DynamicInitializer::NewInstance does not support arrays.");

    if constexpr (std::is_default_constructible_v<T>) {
        return std::make_shared<T>();
    } else {
        const auto* state = detail::GetState();
        if (!state) {
            return {};
        }

        auto it = state->TypeFactories.find(std::type_index(typeid(T)));
        if (it == state->TypeFactories.end() || !it->second) {
            return {};
        }

        return std::static_pointer_cast<T>(it->second());
    }
}

inline std::shared_ptr<void> NewInstance(const std::string& name) {
    const auto* state = detail::GetState();
    if (!state || name.empty()) {
        return {};
    }

    auto it = state->NamedFactories.find(name);
    if (it == state->NamedFactories.end() || !it->second) {
        return {};
    }

    return it->second();
}

template<typename T>
inline void RegisterType(const std::string& name = {}) {
    auto* state = detail::GetState();
    if (!state) {
        return;
    }

    state->TypeFactories[std::type_index(typeid(T))] = []() -> std::shared_ptr<void> {
        if constexpr (std::is_default_constructible_v<T>) {
            return std::make_shared<T>();
        } else {
            return {};
        }
    };

    if (!name.empty()) {
        state->NamedFactories[name] = []() -> std::shared_ptr<void> {
            if constexpr (std::is_default_constructible_v<T>) {
                return std::make_shared<T>();
            } else {
                return {};
            }
        };
    }
}

template<typename T>
inline void RegisterFactory(std::function<std::shared_ptr<T>()> factory, const std::string& name = {}) {
    auto* state = detail::GetState();
    if (!state || !factory) {
        return;
    }

    auto sharedFactory = std::make_shared<std::function<std::shared_ptr<T>()>>(std::move(factory));
    state->TypeFactories[std::type_index(typeid(T))] = [sharedFactory]() -> std::shared_ptr<void> {
        return (*sharedFactory)();
    };

    if (!name.empty()) {
        state->NamedFactories[name] = [sharedFactory]() -> std::shared_ptr<void> {
            return (*sharedFactory)();
        };
    }
}

inline void Register(const std::string& name, std::function<void()> initializer) {
    auto* state = detail::GetState();
    if (!state || name.empty() || !initializer) {
        return;
    }

    state->DeferredInitializers[name] = std::move(initializer);
}

inline void RunPending() {
    auto* state = detail::GetState();
    if (!state) {
        return;
    }

    const auto snapshot = state->DeferredInitializers;
    state->DeferredInitializers.clear();
    for (const auto& [name, initializer] : snapshot) {
        (void)name;
        if (initializer) {
            initializer();
        }
    }
}

inline void Reset() {
    auto* state = detail::GetState();
    if (!state) {
        return;
    }

    state->TypeFactories.clear();
    state->NamedFactories.clear();
    state->DeferredInitializers.clear();
}

} // namespace SDK::Utils::DynamicInitializer
