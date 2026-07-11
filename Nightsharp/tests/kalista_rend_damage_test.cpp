#include <cmath>
#include <cstdio>

#include "../plugins/Champion/7UPAIO/KalistaRendDamage.h"

namespace {

bool ExpectNear(const char* name, double actual, double expected) {
    if (std::fabs(actual - expected) <= 0.001) {
        return true;
    }
    std::printf("%s failed: expected %.3f, got %.3f\n", name, expected, actual);
    return false;
}

} // namespace

int main() {
    using Plugins::AIO7UP::Kalista::RendDamage::RawDamage;
    using Plugins::AIO7UP::Kalista::RendDamage::SecureJungleDamage;

    bool ok = true;
    ok &= ExpectNear("rank1 one spear", RawDamage(1, 1, 100.0f, 0.0f, false), 75.0);
    ok &= ExpectNear("rank1 three spears", RawDamage(1, 3, 100.0f, 0.0f, false), 129.0);
    ok &= ExpectNear("epic monster modifier", RawDamage(1, 3, 100.0f, 0.0f, true), 64.5);
    ok &= ExpectNear("rank5 epic", RawDamage(5, 5, 200.0f, 0.0f, true), 362.5);
    ok &= ExpectNear("large or epic secure factor", SecureJungleDamage(200.0, true), 100.0);
    ok &= ExpectNear("small jungle unchanged", SecureJungleDamage(200.0, false), 200.0);
    return ok ? 0 : 1;
}
