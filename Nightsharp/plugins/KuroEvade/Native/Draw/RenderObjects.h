#pragma once

#include "RenderObject.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace Plugins::KuroEvade {

struct RenderObjects final {
    static std::vector<std::unique_ptr<RenderObject>>& Objects() {
        static std::vector<std::unique_ptr<RenderObject>> objects;
        return objects;
    }

    static void Render() {
        auto& objects = Objects();
        objects.erase(
            std::remove_if(objects.begin(), objects.end(), [](const auto& object) {
                return !object || object->Expired();
            }),
            objects.end());

        for (const auto& object : objects) {
            if (object) {
                object->Draw();
            }
        }
    }

    static void Add(std::unique_ptr<RenderObject> object) {
        if (object) {
            Objects().push_back(std::move(object));
        }
    }

    template <typename T, typename... Args>
    static T* Emplace(Args&&... args) {
        auto object = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = object.get();
        Add(std::move(object));
        return raw;
    }
};

} // namespace Plugins::KuroEvade
