#include <cstdint>
#include <cstdio>

#include "../core/BuffEventDecodePolicy.h"

namespace {

int failures = 0;

void Expect(bool condition, const char* name) {
    if (!condition) {
        std::printf("FAIL: %s\n", name);
        ++failures;
    }
}

void TestBridgeHooksDoNotInventBuffPointers() {
    using Core::Events::detail::BuffBridgeHook;
    using Core::Events::detail::DecodeBridgeBuffAddress;

    constexpr std::uintptr_t object = 0x11110000;
    constexpr std::uintptr_t bridge = 0x22220000;
    constexpr std::uintptr_t nameView = 0x33330000;
    constexpr std::uintptr_t ownerComponent = 0x444402B0;

    Expect(
        DecodeBridgeBuffAddress(
            BuffBridgeHook::Add,
            object,
            bridge,
            nameView,
            ownerComponent) == 0,
        "add bridge exposes no BuffData pointer");
    Expect(
        DecodeBridgeBuffAddress(
            BuffBridgeHook::Remove,
            object,
            bridge,
            nameView,
            ownerComponent) == 0,
        "remove bridge exposes no BuffData pointer");
    Expect(
        DecodeBridgeBuffAddress(
            BuffBridgeHook::Update,
            object,
            bridge,
            nameView,
            1) == 0,
        "update count is never treated as a pointer");
}

} // namespace

int main() {
    TestBridgeHooksDoNotInventBuffPointers();
    if (failures == 0) {
        std::printf("BUFF EVENT DECODE POLICY TESTS PASSED\n");
    }
    return failures == 0 ? 0 : 1;
}
