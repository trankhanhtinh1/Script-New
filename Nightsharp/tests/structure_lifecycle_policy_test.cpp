#include <cstdio>

#include "../sdk/Events/StructureLifecyclePolicy.h"

namespace {

int failures = 0;

void Expect(bool condition, const char* name) {
    if (!condition) {
        std::printf("FAIL: %s\n", name);
        ++failures;
    }
}

void TestStructuresUseSnapshotPolling() {
    using Core::Objects::ObjectType;
    using SDK::Events::detail::ShouldQueueObjectLifecycle;

    Expect(!ShouldQueueObjectLifecycle(ObjectType::AITurretClient),
           "turrets skip lifecycle queue");
    Expect(!ShouldQueueObjectLifecycle(ObjectType::BarracksDampenerClient),
           "inhibitors skip lifecycle queue");
    Expect(!ShouldQueueObjectLifecycle(ObjectType::HQClient),
           "nexuses skip lifecycle queue");
}

void TestOtherObjectsKeepNativeLifecycle() {
    using Core::Objects::ObjectType;
    using SDK::Events::detail::ShouldQueueObjectLifecycle;

    Expect(ShouldQueueObjectLifecycle(ObjectType::AIHeroClient),
           "heroes keep lifecycle queue");
    Expect(ShouldQueueObjectLifecycle(ObjectType::AIMinionClient),
           "minions keep lifecycle queue");
    Expect(ShouldQueueObjectLifecycle(ObjectType::MissileClient),
           "missiles keep lifecycle queue");
    Expect(ShouldQueueObjectLifecycle(ObjectType::EffectEmitter),
           "effects keep lifecycle queue");
}

} // namespace

int main() {
    TestStructuresUseSnapshotPolling();
    TestOtherObjectsKeepNativeLifecycle();

    if (failures == 0) {
        std::printf("STRUCTURE LIFECYCLE POLICY TESTS PASSED\n");
    }
    return failures == 0 ? 0 : 1;
}
