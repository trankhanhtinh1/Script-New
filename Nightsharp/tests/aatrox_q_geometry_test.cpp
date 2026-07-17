#include <cmath>
#include <cstdio>

#include "../plugins/Champion/KuroAIO/AI/Controllers/AIAatroxGeometry.h"

using namespace Plugins::KuroAIO::AI::Controllers::Aatrox::Geometry;

static int g_failures = 0;

static void ExpectTrue(const char* name, bool value) {
    if (!value) {
        std::printf("FAIL: %s\n", name);
        ++g_failures;
    }
}

static void ExpectNear(const char* name, float actual, float expected, float epsilon = 0.001f) {
    if (std::fabs(actual - expected) > epsilon) {
        std::printf("FAIL: %s expected %.4f got %.4f\n", name, expected, actual);
        ++g_failures;
    }
}

int main() {
    const Vec3 origin{ 0.0f, 0.0f, 0.0f };
    const Vec3 forward{ 1.0f, 0.0f, 0.0f };

    ExpectNear("Q1 range", QRange(QStage::First), 650.0f);
    ExpectNear("Q2 range", QRange(QStage::Second), 525.0f);
    ExpectNear("Q3 directional range", QRange(QStage::Third), 400.0f);
    ExpectNear("Q1 ideal", QIdealForward(QStage::First), 565.0f);
    ExpectNear("Q2 ideal", QIdealForward(QStage::Second), 410.0f);
    ExpectNear("Q3 center", QIdealForward(QStage::Third), 200.0f);

    ExpectNear("Q1 centered sweetspot",
               SweetspotScore(QStage::First, origin, forward,
                              Vec3{ 565.0f, 0.0f, 0.0f }, 35.0f),
               1.0f);
    ExpectNear("Q2 centered sweetspot",
               SweetspotScore(QStage::Second, origin, forward,
                              Vec3{ 410.0f, 0.0f, 0.0f }, 35.0f),
               1.0f);
    ExpectNear("Q3 centered sweetspot",
               SweetspotScore(QStage::Third, origin, forward,
                              Vec3{ 200.0f, 0.0f, 0.0f }, 35.0f),
               1.0f);

    ExpectTrue("Q1 inner body is not sweet",
               SweetspotScore(QStage::First, origin, forward,
                              Vec3{ 250.0f, 0.0f, 0.0f }, 35.0f) == 0.0f &&
               BodyCanHit(QStage::First, origin, forward,
                          Vec3{ 250.0f, 0.0f, 0.0f }, 35.0f));
    ExpectTrue("Q2 can use a wide outer corner",
               SweetspotScore(QStage::Second, origin, forward,
                              Vec3{ 410.0f, 0.0f, 200.0f }, 35.0f) > 0.70f);
    ExpectTrue("Q3 body edge is not sweet",
               SweetspotScore(QStage::Third, origin, forward,
                              Vec3{ 450.0f, 0.0f, 0.0f }, 35.0f) == 0.0f &&
               BodyCanHit(QStage::Third, origin, forward,
                          Vec3{ 450.0f, 0.0f, 0.0f }, 35.0f));

    // Q1 target is initially beyond the blade. A 235-unit E forward solves it.
    const Vec3 farTarget{ 800.0f, 0.0f, 0.0f };
    const Vec3 q1CorrectedSource{ 235.0f, 0.0f, 0.0f };
    ExpectNear("Q1 before forward E misses",
               SweetspotScore(QStage::First, origin, forward, farTarget, 35.0f),
               0.0f);
    ExpectNear("Q1 forward E centers sweetspot",
               SweetspotScore(QStage::First, q1CorrectedSource, forward,
                              farTarget, 35.0f),
               1.0f);

    // A target behind/inside Q3 is recovered by E backward while Q direction
    // remains fixed, reproducing the melee disengage branch.
    const Vec3 insideTarget{ -50.0f, 0.0f, 0.0f };
    const Vec3 q3CorrectedSource{ -250.0f, 0.0f, 0.0f };
    ExpectNear("Q3 before backward E misses",
               SweetspotScore(QStage::Third, origin, forward, insideTarget, 35.0f),
               0.0f);
    ExpectNear("Q3 backward E centers sweetspot",
               SweetspotScore(QStage::Third, q3CorrectedSource, forward,
                              insideTarget, 35.0f),
               1.0f);

    const Vec3 diagonal = Direction2D(origin, Vec3{ 10.0f, 0.0f, 10.0f });
    ExpectNear("direction normalized", diagonal.Length2D(), 1.0f);
    ExpectNear("direction ignores height",
               Direction2D(origin, Vec3{ 0.0f, 100.0f, 10.0f }).y,
               0.0f);

    // Broad invariant sweep: confidence is normalized and a scored sweetspot
    // must also be within that cast's body hitbox.
    for (int stageValue = 1; stageValue <= 3; ++stageValue) {
        const auto stage = static_cast<QStage>(stageValue);
        for (int x = -150; x <= 900; x += 25) {
            for (int z = -400; z <= 400; z += 25) {
                const Vec3 point{ static_cast<float>(x), 0.0f, static_cast<float>(z) };
                const float score = SweetspotScore(stage, origin, forward, point, 35.0f);
                ExpectTrue("sweetspot score normalized", score >= 0.0f && score <= 1.0f);
                if (score > 0.0f) {
                    ExpectTrue("sweetspot belongs to body",
                               BodyCanHit(stage, origin, forward, point, 35.0f));
                }
            }
        }
    }

    if (g_failures == 0) {
        std::printf("ALL AATROX Q GEOMETRY TESTS PASSED\n");
        return 0;
    }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}

