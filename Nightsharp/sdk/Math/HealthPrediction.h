#pragma once

#include "Prediction/Health.h"

#include <string>
#include <unordered_map>

namespace SDK::HealthPrediction {

class IHealthPrediction {
public:
    virtual ~IHealthPrediction() = default;

    virtual AIBaseClient GetAggroTurret(const AIMinionClient& minion) = 0;
    virtual float GetPrediction(const AIBaseClient& unit,
                                int timeMs,
                                int delayMs = 70,
                                HealthPredictionType type = HealthPredictionType::Default) = 0;
    virtual bool HasMinionAggro(const AIMinionClient& minion) = 0;
    virtual bool HasTurretAggro(const AIMinionClient& minion) = 0;
    virtual int TurretAggroStartTick(const AIMinionClient& minion) = 0;
};

namespace detail {

class SDKHealthPrediction final : public IHealthPrediction {
public:
    AIBaseClient GetAggroTurret(const AIMinionClient& minion) override {
        return Prediction::Health::GetAggroTurret(minion);
    }

    float GetPrediction(const AIBaseClient& unit,
                        int timeMs,
                        int delayMs = 70,
                        HealthPredictionType type = HealthPredictionType::Default) override {
        return Prediction::Health::GetPrediction(unit, timeMs, delayMs, type);
    }

    bool HasMinionAggro(const AIMinionClient& minion) override {
        return Prediction::Health::HasMinionAggro(minion);
    }

    bool HasTurretAggro(const AIMinionClient& minion) override {
        return Prediction::Health::HasTurretAggro(minion);
    }

    int TurretAggroStartTick(const AIMinionClient& minion) override {
        return Prediction::Health::TurretAggroStartTick(minion);
    }
};

inline constexpr const char* SDKHealthPredictionName = "SDK";
inline bool FacadeInitialized = false;
inline SDKHealthPrediction DefaultPrediction;
inline std::unordered_map<std::string, IHealthPrediction*> Implementations;
inline IHealthPrediction* Implementation = nullptr;
inline std::string SelectedPredictionName;

} // namespace detail

inline void Initialize() {
    if (detail::FacadeInitialized) {
        Prediction::Health::Initialize();
        return;
    }

    detail::FacadeInitialized = true;
    Prediction::Health::Initialize();
    detail::Implementations.emplace(detail::SDKHealthPredictionName, &detail::DefaultPrediction);
    detail::Implementation = &detail::DefaultPrediction;
    detail::SelectedPredictionName = detail::SDKHealthPredictionName;
}

inline void Reset() {
    Prediction::Health::Reset();
    detail::Implementations.clear();
    detail::Implementation = nullptr;
    detail::SelectedPredictionName.clear();
    detail::FacadeInitialized = false;
}

inline bool AddPrediction(const std::string& name, IHealthPrediction* prediction) {
    Initialize();
    if (name.empty() || prediction == nullptr ||
        detail::Implementations.find(name) != detail::Implementations.end()) {
        return false;
    }

    detail::Implementations.emplace(name, prediction);
    return true;
}

inline bool AddPrediction(const std::string& name, IHealthPrediction& prediction) {
    return AddPrediction(name, &prediction);
}

inline bool SetPrediction(const std::string& name) {
    Initialize();
    const auto it = detail::Implementations.find(name);
    if (it == detail::Implementations.end() || it->second == nullptr) {
        return false;
    }

    detail::Implementation = it->second;
    detail::SelectedPredictionName = name;
    return true;
}

inline IHealthPrediction* GetPrediction(const std::string& name) {
    Initialize();
    const auto it = detail::Implementations.find(name);
    return it != detail::Implementations.end() ? it->second : nullptr;
}

inline IHealthPrediction* GetPrediction(const char* name) {
    return name ? GetPrediction(std::string(name)) : nullptr;
}

inline IHealthPrediction* GetSDKPrediction() {
    Initialize();
    return &detail::DefaultPrediction;
}

inline IHealthPrediction* CurrentPrediction() {
    Initialize();
    return detail::Implementation;
}

inline const std::string& CurrentPredictionName() {
    Initialize();
    return detail::SelectedPredictionName;
}

inline float GetPrediction(const AIBaseClient& unit,
                           int timeMs,
                           int delayMs = 70,
                           HealthPredictionType type = HealthPredictionType::Default) {
    Initialize();
    return detail::Implementation
        ? detail::Implementation->GetPrediction(unit, timeMs, delayMs, type)
        : Prediction::Health::GetPrediction(unit, timeMs, delayMs, type);
}

inline AIBaseClient GetAggroTurret(const AIMinionClient& minion) {
    Initialize();
    return detail::Implementation
        ? detail::Implementation->GetAggroTurret(minion)
        : Prediction::Health::GetAggroTurret(minion);
}

inline bool HasMinionAggro(const AIMinionClient& minion) {
    Initialize();
    return detail::Implementation
        ? detail::Implementation->HasMinionAggro(minion)
        : Prediction::Health::HasMinionAggro(minion);
}

inline bool HasTurretAggro(const AIMinionClient& minion) {
    Initialize();
    return detail::Implementation
        ? detail::Implementation->HasTurretAggro(minion)
        : Prediction::Health::HasTurretAggro(minion);
}

inline int TurretAggroStartTick(const AIMinionClient& minion) {
    Initialize();
    return detail::Implementation
        ? detail::Implementation->TurretAggroStartTick(minion)
        : Prediction::Health::TurretAggroStartTick(minion);
}

} // namespace SDK::HealthPrediction
