#pragma once
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <windows.h>
#include "../../GameObjects/GameObjects.h"
#include "../../Utils/DelayAction.h"

namespace EzEvade {

    class ObjectTracker {
    public:
        struct ObjectTrackerInfo {
            SDK::GameObject* obj = nullptr;
            Vec3 position = { 0, 0, 0 };
            Vec3 direction = { 0, 0, 0 };
            std::string Name;
            int OwnerNetworkID = 0;
            bool usePosition = false;
            long timestamp = 0;
            std::map<int, SDK::GameObject*> objList;

            ObjectTrackerInfo() = default;

            ObjectTrackerInfo(SDK::GameObject* targetObj) {
                this->obj = targetObj;
                if (targetObj) {
                    const std::string name = targetObj->GetName();
                    if (!name.empty()) {
                        this->Name = name;
                    }
                }
                this->timestamp = ::GetTickCount();
            }

            ObjectTrackerInfo(SDK::GameObject* targetObj, const std::string& nameStr) {
                this->obj = targetObj;
                this->Name = nameStr;
                this->timestamp = ::GetTickCount();
            }

            ObjectTrackerInfo(const std::string& nameStr, const Vec3& pos) {
                this->Name = nameStr;
                this->usePosition = true;
                this->position = pos;
                this->timestamp = ::GetTickCount();
            }
        };

        static std::map<int, ObjectTrackerInfo> objTracker;
        static int objTrackerID;

        static void Initialize();
        static void HuiTrackerForceLoad();
        static void AddObjTrackerPosition(const std::string& name, const Vec3& position, int timeExpiresMs);
        static void HiuCreate_ObjectTracker(SDK::GameObject* obj);
        static void HiuDelete_ObjectTracker(SDK::GameObject* obj);
        static Vec2 GetLastHiuOrientation();

    private:
        static bool _loaded;
    };

}


