#include <cmath>
#include <cstdio>

#include "../sdk/Math/YasuoWallModel.h"

namespace M = SDK::YasuoWallModel;

namespace {
int failures = 0;

void Expect(bool condition, const char* name) {
    if (!condition) {
        std::printf("FAIL: %s\n", name);
        ++failures;
    }
}

void ExpectNear(float actual, float expected, const char* name) {
    if (std::fabs(actual - expected) > 0.01f) {
        std::printf("FAIL: %s expected %.2f got %.2f\n", name, expected, actual);
        ++failures;
    }
}

M::Identity Id(std::uint32_t networkId, std::uint32_t index = 1) {
    return {static_cast<std::uintptr_t>(0x100000 + networkId), networkId, index};
}

void AddSampleWall(M::Registry& registry, int tick = 1000) {
    registry.OnCreate(Id(100, 10), tick, "Yasuo_Base_W_windwall1", {922.76f, 644.08f});
    registry.OnCreate(Id(101, 11), tick + 5, "YasuoWChildMis", {907.37f, 803.34f});
    registry.OnCreate(Id(102, 12), tick + 7, "YasuoWChildMis", {939.40f, 484.95f});
}

void TestNames() {
    for (int level = 1; level <= 5; ++level) {
        const std::string name = "Yasuo_Base_W_windwall" + std::to_string(level);
        Expect(M::ClassifyName(name) == M::ObjectRole::Main, "main name accepted");
        Expect(M::ParseMainLevel(name) == level, "main level parsed");
    }
    Expect(M::ClassifyName("yAsUoWcHiLdMiS") == M::ObjectRole::Endpoint,
           "endpoint case-insensitive");
    Expect(M::ClassifyName("YasuoW_VisualMis") == M::ObjectRole::Visual,
           "visual classified");
    Expect(M::ClassifyName("Yasuo_Base_W_windwall_activate") == M::ObjectRole::Other,
           "activate rejected");
    Expect(M::ClassifyName("Yasuo_Base_W_windwall_big_impact") == M::ObjectRole::Other,
           "impact rejected");
    Expect(M::ClassifyName("Yasuo_Base_W_windwall_groud_crack.tex") == M::ObjectRole::Other,
           "ground crack rejected");
}

void TestMeasuredEndpoints() {
    M::Registry registry;
    AddSampleWall(registry);
    registry.Refresh(1010);
    const auto& walls = registry.ActiveWalls();
    Expect(walls.size() == 1, "one wall assembled");
    if (!walls.empty()) {
        ExpectNear(walls[0].Span(), 320.0f, "CE sample span");
        ExpectNear(walls[0].center.x, 922.76f, "main center x");
        ExpectNear(walls[0].center.y, 644.08f, "main center z");
    }
}

void TestDelayedNameAndLifecycle() {
    M::Registry registry;
    registry.OnCreate(Id(200, 20), 1000, "", {0.0f, 0.0f});
    registry.OnCreate(Id(201, 21), 1001, "YasuoWChildMis", {-160.0f, 0.0f});
    registry.OnCreate(Id(202, 22), 1002, "YasuoWChildMis", {160.0f, 0.0f});
    registry.Refresh(1010);
    Expect(registry.ActiveWalls().empty(), "pending main not published");

    registry.Update(Id(200, 20), 1100, "Yasuo_Base_W_windwall2", {0.0f, 0.0f});
    registry.Refresh(1100);
    Expect(registry.ActiveWalls().size() == 1, "late name publishes wall");

    registry.OnDelete(Id(201, 21));
    registry.Refresh(1101);
    Expect(registry.ActiveWalls().empty(), "delete invalidates wall");

    M::Registry pending;
    pending.OnCreate(Id(300, 30), 1000, "", {0.0f, 0.0f});
    pending.Refresh(1501);
    Expect(pending.Entries().empty(), "pending expires after 500 ms");

    M::Registry expired;
    AddSampleWall(expired, 1000);
    expired.Refresh(6001);
    Expect(expired.ActiveWalls().empty(), "wall hard-expires after 5000 ms");
}

void TestPairingAndTieBreak() {
    M::Registry registry;
    registry.OnCreate(Id(400, 40), 1000, "Yasuo_Base_W_windwall3", {0.0f, 0.0f});
    registry.OnCreate(Id(410, 41), 1000, "YasuoWChildMis", {-160.0f, 0.0f});
    registry.OnCreate(Id(411, 42), 1000, "YasuoWChildMis", {160.0f, 0.0f});
    registry.OnCreate(Id(420, 43), 1000, "YasuoWChildMis", {0.0f, -160.0f});
    registry.OnCreate(Id(421, 44), 1000, "YasuoWChildMis", {0.0f, 160.0f});
    registry.OnCreate(Id(430, 45), 1000, "YasuoWChildMis", {1000.0f, 1000.0f});
    registry.Refresh(1010);
    const auto& walls = registry.ActiveWalls();
    Expect(walls.size() == 1, "one deterministic pair");
    if (!walls.empty()) {
        Expect(walls[0].endpointA.networkId == 410, "lower-id endpoint A selected");
        Expect(walls[0].endpointB.networkId == 411, "lower-id endpoint B selected");
    }

    M::Registry missing;
    missing.OnCreate(Id(500, 50), 1000, "Yasuo_Base_W_windwall1", {0.0f, 0.0f});
    missing.OnCreate(Id(501, 51), 1000, "YasuoWChildMis", {160.0f, 0.0f});
    missing.Refresh(1001);
    Expect(missing.ActiveWalls().empty(), "one endpoint is insufficient");
}

void TestCollisionGeometry() {
    const M::WallSegment wall = {
        Id(600), Id(601), Id(602), 1, 1000,
        {0.0f, 0.0f}, {0.0f, -100.0f}, {0.0f, 100.0f}
    };
    Expect(M::IntersectsProjectilePath({-50.0f, 0.0f}, {50.0f, 0.0f}, wall, 0.0f),
           "crossing path collides");
    Expect(!M::IntersectsProjectilePath({-50.0f, 120.0f}, {50.0f, 120.0f}, wall, 10.0f),
           "outside radius misses");
    Expect(M::IntersectsProjectilePath({-50.0f, 110.0f}, {50.0f, 110.0f}, wall, 10.0f),
           "capsule radius collides");
    Expect(M::IntersectsProjectilePath({0.0f, 105.0f}, {0.0f, 105.0f}, wall, 5.0f),
           "endpoint radius collides");
}
} // namespace

int main() {
    TestNames();
    TestMeasuredEndpoints();
    TestDelayedNameAndLifecycle();
    TestPairingAndTieBreak();
    TestCollisionGeometry();
    if (failures == 0) {
        std::printf("ALL YASUO WALL MODEL TESTS PASSED\n");
    }
    return failures == 0 ? 0 : 1;
}
