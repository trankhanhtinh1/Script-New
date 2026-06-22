#pragma once

#include "Logging.h"

#include <functional>
#include <vector>

namespace SDK::Core::Utils {

class ResourceLoader {
public:
    using ImportTask = void(*)();

    static void Register(ImportTask task) {
        if (task) {
            Tasks().push_back(task);
        }
    }

    static void Initialize() {
        for (auto task : Tasks()) {
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
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using ResourceLoader = ::SDK::Core::Utils::ResourceLoader;
} // namespace SDK::Utils
