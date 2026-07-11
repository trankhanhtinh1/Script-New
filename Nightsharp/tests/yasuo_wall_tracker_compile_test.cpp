#include <type_traits>
#include <vector>

#include "../sdk/GameObjects/YasuoWallTracker.h"

static_assert(std::is_same_v<
    decltype(SDK::YasuoWallTracker::ActiveWalls()),
    std::vector<SDK::YasuoWallTracker::WallSnapshot>>);

static_assert(std::is_same_v<
    decltype(SDK::YasuoWallTracker::Intersects(Vec3{}, Vec3{}, 0.0f)),
    bool>);

template <typename T>
concept HasOrientation = requires(const T& value) {
    value.Orientation();
};

static_assert(!HasOrientation<SDK::EffectEmitter>);

int main() {
    return 0;
}
