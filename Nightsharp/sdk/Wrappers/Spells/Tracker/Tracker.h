#pragma once

#include "Detector.h"

#include "../../../Events/Events.h"
#include "../../../Utils/Logging.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace SDK {

class Tracker {
public:
    using OnDetectSkillshotH = Detector::OnDetectSkillshotH;

    static void Initialize() {
        if (Initialized()) {
            return;
        }

        Initialized() = true;
        Events::AddOnGameUpdate(&Game_OnUpdate);
        Events::AddOnMissileDelete(&MissileClient_OnDelete);
        Detector::AddOnDetectSkillshot(&Detector_OnDetectSkillshot);
    }

    static void Shutdown() {
        if (!Initialized()) {
            return;
        }

        Detector::RemoveOnDetectSkillshot(&Detector_OnDetectSkillshot);
        Events::RemoveOnMissileDelete(&MissileClient_OnDelete);
        Events::RemoveOnGameUpdate(&Game_OnUpdate);
        DetectedSkillshots().clear();
        Initialized() = false;
    }

    static std::vector<std::shared_ptr<Skillshot>>& DetectedSkillshots() {
        static std::vector<std::shared_ptr<Skillshot>> detectedSkillshots;
        return detectedSkillshots;
    }

    static bool AddOnDetectSkillshot(OnDetectSkillshotH handler) {
        Initialize();
        auto& handlers = Handlers();
        if (!handler) {
            return false;
        }
        if (std::find(handlers.begin(), handlers.end(), handler) != handlers.end()) {
            return true;
        }
        handlers.push_back(handler);
        return true;
    }

    static bool RemoveOnDetectSkillshot(OnDetectSkillshotH handler) {
        auto& handlers = Handlers();
        const auto oldSize = handlers.size();
        handlers.erase(std::remove(handlers.begin(), handlers.end(), handler), handlers.end());
        const bool removed = oldSize != handlers.size();
        if (removed && handlers.empty()) {
            Shutdown();
        }
        return removed;
    }

private:
    static bool& Initialized() {
        static bool initialized = false;
        return initialized;
    }

    static std::vector<OnDetectSkillshotH>& Handlers() {
        static std::vector<OnDetectSkillshotH> handlers;
        return handlers;
    }

    static void MissileClient_OnDelete(const Events::ObjectEventArgs& args) {
        auto& skillshots = DetectedSkillshots();
        for (auto it = skillshots.begin(); it != skillshots.end();) {
            auto missileSkillshot = std::dynamic_pointer_cast<SkillshotMissile>(*it);
            if (!missileSkillshot ||
                !missileSkillshot->Missile.IsValid() ||
                !missileSkillshot->SData.CanBeRemoved) {
                ++it;
                continue;
            }

            const int missileNetworkId = missileSkillshot->Missile.NetworkId();
            const bool sameMissile =
                missileNetworkId == static_cast<int>(args.MissileNetworkId) ||
                missileNetworkId == static_cast<int>(args.Sender.NetworkId);

            if (!sameMissile) {
                ++it;
                continue;
            }

            missileSkillshot->MissileDestroyed = true;
            it = skillshots.erase(it);
        }
    }

    static void Game_OnUpdate(const Events::GameUpdateEventArgs&) {
        auto& skillshots = DetectedSkillshots();
        skillshots.erase(
            std::remove_if(skillshots.begin(), skillshots.end(), [](const std::shared_ptr<Skillshot>& skillshot) {
                return !skillshot || skillshot->HasExpired();
            }),
            skillshots.end());

        for (const auto& skillshot : skillshots) {
            if (skillshot) {
                skillshot->Game_OnUpdate();
            }
        }
    }

    static void Detector_OnDetectSkillshot(const std::shared_ptr<Skillshot>& skillshot) {
        bool isAlreadyDetected = false;
        for (const auto& detectedSkillshot : DetectedSkillshots()) {
            if (!detectedSkillshot ||
                detectedSkillshot->SData.SpellName != skillshot->SData.SpellName ||
                detectedSkillshot->Caster.NetworkId() != skillshot->Caster.NetworkId()) {
                continue;
            }

            if (Skillshot::AngleBetween(skillshot->Direction, detectedSkillshot->Direction) < 5.0f) {
                isAlreadyDetected = true;

                if (skillshot->DetectionType == SkillshotDetectionType::MissileCreate) {
                    auto detectedMissile = std::dynamic_pointer_cast<SkillshotMissile>(detectedSkillshot);
                    auto newMissile = std::dynamic_pointer_cast<SkillshotMissile>(skillshot);
                    if (detectedMissile && newMissile) {
                        detectedMissile->Missile = newMissile->Missile;
                    } else {
                        Utils::Logging::Write()(LogLevel::Warn,
                            "Wrong SpellType for Skillshot %s, a Missile Type was expected",
                            skillshot->SData.SpellName.c_str());
                    }
                }
            }
        }

        if (isAlreadyDetected) {
            return;
        }

        skillshot->PrintSpellData();
        DetectedSkillshots().push_back(skillshot);
        TriggerOnDetectSkillshot(skillshot);
    }

    static void TriggerOnDetectSkillshot(const std::shared_ptr<Skillshot>& skillshot) {
        for (auto handler : Handlers()) {
            if (handler) {
                handler(skillshot);
            }
        }
    }
};

} // namespace SDK
