#pragma once

#include "JsonFactory.h"
#include "Logging.h"

#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace SDK::Core::Utils {

class ResourceLoader {
public:
    using ImportTask = std::function<void()>;

    static void Register(ImportTask task) {
        if (task) {
            Tasks().push_back(std::move(task));
        }
    }

    template <typename T, typename Filter = std::nullptr_t>
    static void RegisterJsonImport(T* target, std::string file, Filter filter = nullptr) {
        Register([target, file = std::move(file), filter = std::move(filter)]() mutable {
            if (!target) {
                return;
            }
            T value = JsonFactory::JsonResource<T>(file);
            *target = ApplyFilter<T>(std::move(value), filter);
        });
    }

    template <typename T, typename Filter = std::nullptr_t>
    static void RegisterJsonImport(T& target, std::string file, Filter filter = nullptr) {
        RegisterJsonImport(&target, std::move(file), std::move(filter));
    }

    static void Initialize() {
        for (auto& task : Tasks()) {
            if (!task) {
                continue;
            }
            try {
                task();
            } catch (...) {
                Logging::Write()(LogLevel::Error, "ResourceLoader import task crashed");
            }
        }
    }

private:
    static std::vector<ImportTask>& Tasks() {
        static std::vector<ImportTask> tasks;
        return tasks;
    }

    template <typename T, typename Filter>
    static T ApplyFilter(T value, Filter& filter) {
        if constexpr (std::is_same_v<std::decay_t<Filter>, std::nullptr_t>) {
            return value;
        } else if constexpr (requires(Filter f, T v) { f.Apply(v); }) {
            return static_cast<T>(filter.Apply(value));
        } else if constexpr (std::is_invocable_v<Filter, T>) {
            return static_cast<T>(std::invoke(filter, value));
        } else {
            return value;
        }
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using ResourceLoader = ::SDK::Core::Utils::ResourceLoader;
} // namespace SDK::Utils
