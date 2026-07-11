#include "../sdk/Enumerations/CollisionableObjects.h"

int main() {
    SDK::CollisionObjectsBridge defaults;
    return defaults.contains(SDK::CollisionableObjects::YasuoWall) &&
           defaults.contains(SDK::CollisionableObjects::SamiraWall) &&
           defaults.contains(SDK::CollisionableObjects::MelWall)
        ? 0
        : 1;
}
