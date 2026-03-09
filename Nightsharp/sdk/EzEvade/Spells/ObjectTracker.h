#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Utils/DelayAction.h"
#include "sdk/EzEvade/Utils/EvadeUtils.h"
#include <unordered_map>
#include <vector>

namespace EzEvade {

struct ObjectTrackerInfo {
    SDK::GameObject Obj = SDK::GameObject();
    Vec3 Position = Vec3();
    Vec3 Direction = Vec3();
    std::string Name = "";
    int OwnerNetworkID = 0;
    bool UsePosition = false;
    float Timestamp = 0.0f;
    std::unordered_map<int, SDK::GameObject> ObjList = {};

    ObjectTrackerInfo() = default;
    explicit ObjectTrackerInfo(const SDK::GameObject& obj)
        : Obj(obj), Position(obj.IsValid() ? obj.GetPosition() : Vec3()),
          Name(obj.IsValid() ? obj.GetName() : ""), Timestamp(EvadeUtils::TickCount()) {}
    ObjectTrackerInfo(const SDK::GameObject& obj, const std::string& name)
        : Obj(obj), Position(obj.IsValid() ? obj.GetPosition() : Vec3()),
          Name(name), Timestamp(EvadeUtils::TickCount()) {}
    ObjectTrackerInfo(const std::string& name, const Vec3& position)
        : Position(position), Name(name), UsePosition(true), Timestamp(EvadeUtils::TickCount()) {}
};

class ObjectTracker {
public:
    static inline std::unordered_map<int, ObjectTrackerInfo> ObjTracker = {};
    static inline int ObjTrackerID = 0;

    static void AddObjTrackerPosition(const std::string& name, const Vec3& position, float timeExpiresMs) {
        const int trackerId = ObjTrackerID++;
        ObjTracker.emplace(trackerId, ObjectTrackerInfo(name, position));
        DelayAction::Add((int)timeExpiresMs, [trackerId]() {
            ObjTracker.erase(trackerId);
        });
    }

    static void AddOrUpdate(const SDK::GameObject& obj, const std::string& alias = "") {
        if (!obj.IsValid()) return;
        const int key = obj.GetNetId();
        auto it = ObjTracker.find(key);
        if (it == ObjTracker.end()) {
            ObjTracker.emplace(key, alias.empty() ? ObjectTrackerInfo(obj) : ObjectTrackerInfo(obj, alias));
        } else {
            it->second.Obj = obj;
            it->second.Position = obj.GetPosition();
            if (!alias.empty()) {
                it->second.Name = alias;
            } else if (it->second.Name.empty()) {
                it->second.Name = obj.GetName();
            }
            it->second.UsePosition = false;
            it->second.Timestamp = EvadeUtils::TickCount();
        }
    }

    static void Remove(const SDK::GameObject& obj) {
        if (!obj.IsValid()) return;
        ObjTracker.erase(obj.GetNetId());
    }

    static void RemoveByNetId(int netId) {
        ObjTracker.erase(netId);
    }

    static void Update() {
        std::vector<int> toDelete = {};
        toDelete.reserve(ObjTracker.size());

        for (auto& [id, info] : ObjTracker) {
            if (info.UsePosition) {
                continue;
            }

            if (!info.Obj.IsValid() || info.Obj.IsDead()) {
                toDelete.push_back(id);
            }
        }

        for (int id : toDelete) {
            ObjTracker.erase(id);
        }
    }

    static Vec2 GetLastHiuOrientation() {
        std::vector<const ObjectTrackerInfo*> list = {};
        for (auto& [id, info] : ObjTracker) {
            (void)id;
            if (_stricmp(info.Name.c_str(), "hiu") == 0) {
                list.push_back(&info);
            }
        }

        if (list.size() < 2) {
            return Vec2();
        }

        std::sort(list.begin(), list.end(), [](const ObjectTrackerInfo* a, const ObjectTrackerInfo* b) {
            return a->Timestamp > b->Timestamp;
        });

        const Vec2 p1 = list[0]->Obj.IsValid() ? list[0]->Obj.GetPosition().To2D() : list[0]->Position.To2D();
        const Vec2 p2 = list[1]->Obj.IsValid() ? list[1]->Obj.GetPosition().To2D() : list[1]->Position.To2D();
        return (p2 - p1).Normalized();
    }
};

} // namespace EzEvade

