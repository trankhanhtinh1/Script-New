#pragma once

#include "Logging.h"

#include <memory>
#include <type_traits>

namespace SDK::Core::Utils {

class DynamicInitializer {
public:
    template <typename T>
    static std::unique_ptr<T> NewInstance() {
        static_assert(std::is_default_constructible_v<T>, "T must have a default constructor");
        try {
            return std::make_unique<T>();
        } catch (...) {
            Logging::Write()(LogLevel::Error, "DynamicInitializer::NewInstance crashed");
            return nullptr;
        }
    }

    template <typename T>
    static T* NewRawInstance() {
        auto instance = NewInstance<T>();
        return instance.release();
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using DynamicInitializer = ::SDK::Core::Utils::DynamicInitializer;
} // namespace SDK::Utils
