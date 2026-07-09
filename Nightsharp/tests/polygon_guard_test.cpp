#include <Windows.h>

#include <cstdio>
#include <vector>

#include "../sdk/Math/Polygons/Polygon.h"

namespace {

using SDK::Clipper::IntPoint;
using Path = std::vector<IntPoint>;

bool ExpectEqual(const char* name, int actual, int expected) {
    if (actual == expected) {
        return true;
    }

    std::printf("%s failed: expected %d, got %d\n", name, expected, actual);
    return false;
}

bool ValidPolygonStillWorks() {
    const Path square = {
        IntPoint(0LL, 0LL),
        IntPoint(10LL, 0LL),
        IntPoint(10LL, 10LL),
        IntPoint(0LL, 10LL),
    };

    bool ok = true;
    ok &= ExpectEqual("inside square",
                      SDK::Clipper::PointInPolygon(IntPoint(5LL, 5LL), square),
                      1);
    ok &= ExpectEqual("short path",
                      SDK::Clipper::PointInPolygon(
                          IntPoint(5LL, 5LL),
                          Path{IntPoint(0LL, 0LL), IntPoint(10LL, 0LL)}),
                      0);
    return ok;
}

bool UnreadableVectorStorageIsRejected() {
    struct FakeVectorLayout {
        IntPoint* first;
        IntPoint* last;
        IntPoint* end;
    };

    FakeVectorLayout fake = {};
    fake.first = reinterpret_cast<IntPoint*>(0x1);
    fake.last = fake.first + 3;
    fake.end = fake.last;

    const auto& path = *reinterpret_cast<const Path*>(&fake);
    bool raised = false;
    int result = -99;

    __try {
        result = SDK::Clipper::PointInPolygon(IntPoint(0LL, 0LL), path);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        raised = true;
    }

    if (raised) {
        std::printf("unreadable vector storage raised an exception\n");
        return false;
    }

    return ExpectEqual("unreadable vector storage", result, 0);
}

} // namespace

int main() {
    bool ok = true;
    ok &= ValidPolygonStillWorks();
    ok &= UnreadableVectorStorageIsRejected();
    return ok ? 0 : 1;
}
