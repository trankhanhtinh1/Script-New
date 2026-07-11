#include <type_traits>

#include "../sdk/Math/Collision.h"

static_assert(std::is_same_v<
    decltype(SDK::Collision::HasProjectileWallCollision(
        Vec3{},
        Vec3{},
        0.0f)),
    bool>);

int main() {
    return 0;
}
