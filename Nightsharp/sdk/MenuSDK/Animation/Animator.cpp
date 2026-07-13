#include "Animator.h"

#include <algorithm>
#include <cmath>

namespace NightSharp::Menu {

void Animator::Reserve(std::size_t count) {
    values_.reserve(count);
}

void Animator::Clear() {
    values_.clear();
}

float Animator::Update(
    const std::string& key,
    float target,
    float speed,
    float deltaTime) {
    float& value = values_[key];
    value = Smooth(value, target, speed, deltaTime);
    if (std::abs(target - value) <= 0.0005f) {
        value = target;
    }
    return std::clamp(value, 0.0f, 1.0f);
}

float Animator::Smooth(
    float current,
    float target,
    float speed,
    float deltaTime) {
    deltaTime = std::clamp(deltaTime, 0.0f, 0.05f);
    const float step = 1.0f - std::exp(-std::max(speed, 0.0f) * deltaTime);
    return current + (target - current) * std::clamp(step, 0.0f, 1.0f);
}

}
