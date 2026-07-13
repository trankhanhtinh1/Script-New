#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

namespace NightSharp::Menu {

class Animator {
public:
    void Reserve(std::size_t count);
    void Clear();
    float Update(
        const std::string& key,
        float target,
        float speed,
        float deltaTime);
    static float Smooth(
        float current,
        float target,
        float speed,
        float deltaTime);

private:
    std::unordered_map<std::string, float> values_;
};

}
