#include <cstdio>

#include "../sdk/Wrappers/TargetSelector/TargetSelectorRegistry.h"

namespace {

int failures = 0;

void Expect(bool condition, const char* name) {
    if (!condition) {
        std::printf("FAIL: %s\n", name);
        ++failures;
    }
}

struct FakeTargetSelector {
    void Suspend() { ++suspendCount; }
    void Resume() { ++resumeCount; }
    int suspendCount = 0;
    int resumeCount = 0;
};

void TestRegistrationSelectionAndFallback() {
    SDK::TargetSelectorRegistry<FakeTargetSelector> registry;
    FakeTargetSelector sdk;
    FakeTargetSelector impulse;

    Expect(!registry.Add("", &sdk), "empty name rejected");
    Expect(!registry.Add("Null", nullptr), "null implementation rejected");
    Expect(registry.Add("SDK", &sdk), "SDK registered");
    Expect(!registry.Add("SDK", &impulse), "duplicate rejected");
    Expect(registry.Add("Impulse", &impulse), "Impulse registered");
    Expect(!registry.Set("Missing"), "unknown selection rejected");
    Expect(registry.Set("Impulse"), "Impulse selected");
    Expect(registry.Implementation() == &impulse, "Impulse pointer current");
    Expect(registry.CurrentName() == "Impulse", "Impulse name current");

    sdk.Suspend();
    Expect(sdk.suspendCount == 1, "SDK suspended once");
    Expect(registry.Remove("Impulse"), "Impulse removed");
    Expect(registry.Implementation() == &sdk, "fallback pointer is SDK");
    Expect(registry.CurrentName() == "SDK", "fallback name is SDK");
    sdk.Resume();
    Expect(sdk.resumeCount == 1, "SDK resumed once");

    Expect(registry.Remove("SDK"), "SDK removed");
    Expect(registry.Implementation() == nullptr, "implementation cleared");
    Expect(registry.CurrentName().empty(), "selection name cleared");
    Expect(!registry.Remove("SDK"), "missing removal rejected");
}

} // namespace

int main() {
    TestRegistrationSelectionAndFallback();
    if (failures == 0) {
        std::printf("TARGET SELECTOR REGISTRY TESTS PASSED\n");
    }
    return failures == 0 ? 0 : 1;
}
