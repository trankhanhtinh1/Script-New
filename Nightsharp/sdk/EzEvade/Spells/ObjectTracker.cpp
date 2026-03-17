#include "ObjectTracker.h"
#include <algorithm>
#include <cctype>

namespace EzEvade {

    std::map<int, ObjectTracker::ObjectTrackerInfo> ObjectTracker::objTracker;
    int ObjectTracker::objTrackerID = 0;
    bool ObjectTracker::_loaded = false;

    void ObjectTracker::Initialize()
    {
        // Add hooking if EventManager exists, or just leave manual invocation
        _loaded = true;
    }

    void ObjectTracker::HuiTrackerForceLoad()
    {
        if (!_loaded)
        {
            _loaded = true;
        }
    }

    void ObjectTracker::AddObjTrackerPosition(const std::string& name, const Vec3& position, int timeExpiresMs)
    {
        objTracker[objTrackerID] = ObjectTrackerInfo(name, position);
        
        int trackerID = objTrackerID; // store the id for deletion
        SDK::DelayAction::Add(timeExpiresMs, [trackerID]() {
            objTracker.erase(trackerID);
        });

        objTrackerID++;
    }

    void ObjectTracker::HiuCreate_ObjectTracker(SDK::GameObject* obj)
    {
        if (!obj) return;

        int netId = obj->GetNetId();

        if (objTracker.find(netId) == objTracker.end())
        {
            if (obj->IsMinion() && obj->IsEnemy(SDK::GameObjects::Player)) // Simulates minion.CheckTeam()
            {
                std::string skinName = obj->GetName();
                std::transform(skinName.begin(), skinName.end(), skinName.begin(), ::tolower);
                
                if (skinName.find("testcube") != std::string::npos)
                {
                    objTracker[netId] = ObjectTrackerInfo(obj, "hiu");
                    
                    SDK::DelayAction::Add(250, [netId]() {
                        objTracker.erase(netId);
                    });
                }
            }
        }
    }

    void ObjectTracker::HiuDelete_ObjectTracker(SDK::GameObject* obj)
    {
        if (!obj) return;
        
        auto it = objTracker.find(obj->GetNetId());
        if (it != objTracker.end())
        {
            objTracker.erase(it);
        }
    }

    Vec2 ObjectTracker::GetLastHiuOrientation()
    {
        std::vector<ObjectTracker::ObjectTrackerInfo*> sortedObjList;
        for (auto& pair : objTracker)
        {
            if (pair.second.Name == "hiu")
            {
                sortedObjList.push_back(&pair.second);
            }
        }

        if (sortedObjList.size() >= 2)
        {
            std::sort(sortedObjList.begin(), sortedObjList.end(), 
                [](const ObjectTracker::ObjectTrackerInfo* a, const ObjectTracker::ObjectTrackerInfo* b) {
                    return a->timestamp > b->timestamp;
                });

            if (sortedObjList[0]->obj && sortedObjList[1]->obj)
            {
                Vec3 pos1 = sortedObjList[0]->obj->GetPosition();
                Vec3 pos2 = sortedObjList[1]->obj->GetPosition();

                Vec2 diff = Vec2(pos2.x - pos1.x, pos2.z - pos1.z); 
                return diff.Normalized();
            }
        }

        return Vec2(0, 0);
    }

}


