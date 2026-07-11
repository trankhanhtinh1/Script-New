#include "../Analysis/AoeOptimizer.h"
#include "../Analysis/MovementPatternAnalyzer.h"
#include "../Learning/AdaptiveWeights.h"
#include "../Math/Kinematics.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using ZDPrediction::AdaptiveWeights;
using ZDPrediction::AoeOptimizer;
using ZDPrediction::AoePoint;
using ZDPrediction::MovementHistoryPoint;
using ZDPrediction::MovementPatternAnalyzer;
using ZDPrediction::MovementPatternMetrics;
using ZDPrediction::Math::Distance;
using ZDPrediction::Math::InterceptResidual;
using ZDPrediction::Math::PositionOnPath;
using ZDPrediction::Math::SolveAcceleratedIntercept;
using ZDPrediction::Math::SolveLinearIntercept;
using ZDPrediction::Math::SolvePathIntercept;
using ZDPrediction::Math::SolveTurnIntercept;
using ZDPrediction::Math::Vector2;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void RequireNear(double actual, double expected, double tolerance, const std::string& message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(message + " actual=" + std::to_string(actual) +
                                 " expected=" + std::to_string(expected));
    }
}

void TestStationaryIntercept() {
    const auto result = SolveLinearIntercept({0.0, 0.0}, {1000.0, 0.0}, {}, 1000.0, 0.25);
    Require(result.valid, "stationary intercept invalid");
    RequireNear(result.time, 1.25, 1e-7, "stationary intercept time");
    RequireNear(result.residual, 0.0, 1e-7, "stationary intercept residual");
}

void TestPerpendicularIntercept() {
    const auto result = SolveLinearIntercept(
        {0.0, 0.0}, {1000.0, 0.0}, {0.0, 325.0}, 1400.0, 0.18);
    Require(result.valid, "perpendicular intercept invalid");
    Require(result.time >= 0.18, "perpendicular intercept before launch");
    RequireNear(result.residual, 0.0, 1e-5, "perpendicular intercept residual");
}

void TestRecedingIntercept() {
    const auto result = SolveLinearIntercept(
        {0.0, 0.0}, {1000.0, 0.0}, {300.0, 0.0}, 1000.0, 0.0);
    Require(result.valid, "receding intercept invalid");
    RequireNear(result.time, 1000.0 / 700.0, 1e-6, "receding intercept time");
}

void TestImpossibleIntercept() {
    const auto result = SolveLinearIntercept(
        {0.0, 0.0}, {1000.0, 0.0}, {1200.0, 0.0}, 1000.0, 0.0, 6.0);
    Require(!result.valid, "impossible intercept reported valid");
}

void TestInstantPathPosition() {
    const std::vector<Vector2> path = {{0.0, 0.0}, {300.0, 0.0}, {300.0, 400.0}};
    const Vector2 position = PositionOnPath(path, 300.0, 1.5);
    RequireNear(position.x, 300.0, 1e-7, "instant path x");
    RequireNear(position.y, 150.0, 1e-7, "instant path y");
    const auto intercept = SolvePathIntercept(
        {-500.0, 0.0}, path, 300.0, std::numeric_limits<double>::infinity(), 1.5);
    Require(intercept.valid, "instant path intercept invalid");
    RequireNear(intercept.position.x, 300.0, 1e-7, "instant intercept x");
    RequireNear(intercept.position.y, 150.0, 1e-7, "instant intercept y");
}

void TestPolylineIntercept() {
    const std::vector<Vector2> path = {
        {700.0, 0.0}, {1100.0, 0.0}, {1100.0, 600.0}, {1500.0, 600.0}
    };
    const auto result = SolvePathIntercept({0.0, 0.0}, path, 350.0, 1500.0, 0.2, 8.0);
    Require(result.valid, "polyline intercept invalid");
    Require(result.time >= 0.2, "polyline intercept before launch");
    RequireNear(result.residual, 0.0, 1e-4, "polyline intercept residual");
}

void TestTurnIntercept() {
    const auto result = SolveTurnIntercept(
        {0.0, 0.0}, {1000.0, 0.0}, {0.0, 350.0}, 0.65, 1650.0, 0.15, 5.0);
    Require(result.valid, "turn intercept invalid");
    RequireNear(result.residual, 0.0, 0.001, "turn intercept residual");
    const Vector2 expected = ZDPrediction::Math::PositionWithTurn(
        {1000.0, 0.0}, {0.0, 350.0}, 0.65, result.time);
    RequireNear(Distance(result.position, expected), 0.0, 1e-7, "turn intercept position");
}

void TestAcceleratedIntercept() {
    const auto result = SolveAcceleratedIntercept(
        {0.0, 0.0}, {900.0, 200.0}, {120.0, 210.0}, {-25.0, 15.0}, 1700.0, 0.12, 5.0);
    Require(result.valid, "accelerated intercept invalid");
    RequireNear(result.residual, 0.0, 0.01, "accelerated intercept residual");
}

void TestAoeCircle() {
    const std::vector<AoePoint> points = {
        {1, {600.0, 0.0}}, {2, {650.0, 40.0}}, {3, {620.0, -45.0}}, {4, {900.0, 300.0}}
    };
    const auto result = AoeOptimizer::Circle({0.0, 0.0}, points, 100.0, 1000.0);
    Require(result.valid, "circle aoe invalid");
    Require(result.hitIds.size() == 3, "circle aoe hit count");
}

void TestAoeLine() {
    const std::vector<AoePoint> points = {
        {1, {500.0, 10.0}}, {2, {700.0, -20.0}}, {3, {900.0, 25.0}}, {4, {700.0, 250.0}}
    };
    const auto result = AoeOptimizer::Line({0.0, 0.0}, points, 55.0, 1000.0);
    Require(result.valid, "line aoe invalid");
    Require(result.hitIds.size() == 3, "line aoe hit count");
}

void TestAoeCone() {
    const std::vector<AoePoint> points = {
        {1, {600.0, 30.0}}, {2, {650.0, -40.0}}, {3, {700.0, 80.0}}, {4, {-500.0, 0.0}}
    };
    const auto result = AoeOptimizer::Cone({0.0, 0.0}, points, 25.0 * ZDPrediction::Math::Pi / 180.0, 900.0);
    Require(result.valid, "cone aoe invalid");
    Require(result.hitIds.size() == 3, "cone aoe hit count");
}

void TestAntiJukePolicy() {
    std::vector<MovementHistoryPoint> straight;
    std::vector<MovementHistoryPoint> oscillating;
    for (int index = 0; index < 20; ++index) {
        const double age = 0.95 - static_cast<double>(index) * 0.05;
        straight.push_back({age, {static_cast<double>(index) * 17.5, 0.0}, {350.0, 0.0}});
        const double side = index % 2 == 0 ? -100.0 : 100.0;
        const double velocity = index % 2 == 0 ? -350.0 : 350.0;
        oscillating.push_back({age, {1000.0, side}, {0.0, velocity}});
    }

    const auto straightSummary = MovementPatternAnalyzer::SummarizeHistory(straight);
    const auto jukeSummary = MovementPatternAnalyzer::SummarizeHistory(oscillating);
    Require(straightSummary.displacementEfficiency > 0.95, "straight efficiency");
    Require(straightSummary.directionReversalsPerSecond < 0.1, "straight reversals");
    Require(jukeSummary.displacementEfficiency < 0.15, "juke efficiency");
    Require(jukeSummary.directionReversalsPerSecond > 10.0, "juke reversals");

    const auto stablePolicy = MovementPatternAnalyzer::Evaluate({
        0.98, 0.98, straightSummary.displacementEfficiency, 1.0,
        straightSummary.directionReversalsPerSecond,
        straightSummary.directionReversalCount, 0.05, 0.0});
    const auto singleReversalPolicy = MovementPatternAnalyzer::Evaluate({
        0.75, 0.95, 0.80, 3.0, 4.0, 1, 0.08, 0.0});
    const auto jukePolicy = MovementPatternAnalyzer::Evaluate({
        0.15, 0.90, jukeSummary.displacementEfficiency, 6.0,
        jukeSummary.directionReversalsPerSecond,
        jukeSummary.directionReversalCount, 0.05, 0.0});
    Require(stablePolicy.jukeScore < 0.15, "stable false juke");
    Require(singleReversalPolicy.jukeScore < 0.20, "single reversal treated as repeated juke");
    Require(jukeSummary.directionReversalCount >= 2, "repeated juke count");
    Require(jukePolicy.jukeScore > 0.85, "juke not detected");
    Require(jukePolicy.pathWeight < jukePolicy.velocityWeight, "juke path not suppressed");
    Require(jukePolicy.accelerationWeight < jukePolicy.velocityWeight,
            "juke acceleration not suppressed");
    Require(jukePolicy.velocityScale < 0.35, "juke velocity not damped");

    const Vector2 stabilized = MovementPatternAnalyzer::StabilizedPosition(
        {1000.0, 100.0}, jukeSummary.recentCenter, jukePolicy);
    Require(std::abs(stabilized.y) < 65.0, "juke center stabilization");
    const Vector2 bounded = MovementPatternAnalyzer::ClampDisplacement(
        {1000.0, 100.0}, {1000.0, 1000.0}, 180.0);
    RequireNear(Distance({1000.0, 100.0}, bounded), 180.0, 1e-7,
                "juke displacement bound");

    std::vector<MovementHistoryPoint> recovered;
    for (int index = 0; index < 8; ++index) {
        const double age = 1.00 - static_cast<double>(index) * 0.04;
        const double side = index % 2 == 0 ? -100.0 : 100.0;
        const double velocity = index % 2 == 0 ? -350.0 : 350.0;
        recovered.push_back({age, {0.0, side}, {0.0, velocity}});
    }
    for (int index = 0; index < 11; ++index) {
        const double age = 0.50 - static_cast<double>(index) * 0.05;
        recovered.push_back({age, {static_cast<double>(index) * 17.5, 0.0}, {350.0, 0.0}});
    }
    const auto recoveredSummary = MovementPatternAnalyzer::SummarizeHistory(recovered);
    Require(recoveredSummary.displacementEfficiency > 0.95, "juke recovery efficiency");
    Require(recoveredSummary.directionReversalsPerSecond < 0.1, "juke recovery reversals");

    const double closeCommitment =
        MovementPatternAnalyzer::RequiredDirectionCommitment(0.30, 1);
    const double longCommitment =
        MovementPatternAnalyzer::RequiredDirectionCommitment(1.00, 1);
    const double repeatedCommitment =
        MovementPatternAnalyzer::RequiredDirectionCommitment(1.00, 2);
    Require(closeCommitment < longCommitment, "travel commitment scaling");
    Require(longCommitment < repeatedCommitment, "repeated juke commitment scaling");
    Require(repeatedCommitment <= 0.22, "commitment upper bound");
    Require(MovementPatternAnalyzer::ActiveDirectionReversalCount(3, 0.20) == 3,
            "active reversal evidence lost");
    Require(MovementPatternAnalyzer::ActiveDirectionReversalCount(3, 0.50) == 1,
            "stable reversal evidence did not decay");
}

void TestStateTransitions() {
    Require(!MovementPatternAnalyzer::IsPositionDiscontinuity(
                {1000.0, 1000.0}, {1018.0, 1000.0}, 0.05, 350.0),
            "normal movement classified as teleport");
    Require(MovementPatternAnalyzer::IsPositionDiscontinuity(
                {1000.0, 1000.0}, {1400.0, 1000.0}, 0.05, 350.0),
            "blink transition not detected");
    Require(MovementPatternAnalyzer::IsPositionDiscontinuity(
                {1000.0, 1000.0}, {1400.0, 1000.0}, 0.0, 350.0),
            "same-tick blink not detected");
    Require(!MovementPatternAnalyzer::IsPositionDiscontinuity(
                {1000.0, 1000.0}, {1400.0, 1000.0}, 1.0, 350.0),
            "stale interval classified as instantaneous blink");

    Require(!MovementPatternAnalyzer::IsHistoryReliable(1, 0.0, 0.02, 0.02),
            "newly visible history accepted");
    Require(!MovementPatternAnalyzer::IsHistoryReliable(3, 0.08, 0.08, 0.12),
            "visibility hold ignored");
    Require(!MovementPatternAnalyzer::IsHistoryReliable(3, 0.08, 0.12, 0.08),
            "position stability hold ignored");
    Require(MovementPatternAnalyzer::IsHistoryReliable(4, 0.12, 0.14, 0.14),
            "stable visible history rejected");
}

void TestAdaptiveLearning() {
    AdaptiveWeights learning;
    for (int index = 0; index < 120; ++index) {
        learning.Observe({8.0, 90.0, 130.0}, 120.0);
    }
    const auto pathRegime = learning.Weights();
    Require(pathRegime.path > pathRegime.velocity &&
            pathRegime.path > pathRegime.acceleration,
            "adaptive path regime");

    for (int index = 0; index < 240; ++index) {
        learning.Observe({120.0, 7.0, 75.0}, 120.0);
    }
    const auto velocityRegime = learning.Weights();
    Require(velocityRegime.velocity > velocityRegime.path &&
            velocityRegime.velocity > velocityRegime.acceleration,
            "adaptive velocity regime");
}

void TestMonteCarloResiduals() {
    std::mt19937_64 random(0x5A4450524544ULL);
    std::uniform_real_distribution<double> position(-1800.0, 1800.0);
    std::uniform_real_distribution<double> velocity(-450.0, 450.0);
    std::uniform_real_distribution<double> projectile(650.0, 2800.0);
    std::uniform_real_distribution<double> delay(0.0, 0.8);
    int valid = 0;
    for (int index = 0; index < 20000; ++index) {
        const Vector2 source{position(random), position(random)};
        const Vector2 target{position(random), position(random)};
        const Vector2 targetVelocity{velocity(random), velocity(random)};
        const double speed = projectile(random);
        const double launchDelay = delay(random);
        const auto result = SolveLinearIntercept(
            source, target, targetVelocity, speed, launchDelay, 10.0);
        if (!result.valid) continue;
        ++valid;
        const double residual = InterceptResidual(
            source, result.position, speed, launchDelay, result.time);
        Require(residual <= 1e-4, "monte carlo residual");
        Require(result.time >= launchDelay, "monte carlo launch ordering");
    }
    Require(valid >= 15000, "monte carlo valid coverage");
}

}

int main() {
    try {
        TestStationaryIntercept();
        TestPerpendicularIntercept();
        TestRecedingIntercept();
        TestImpossibleIntercept();
        TestInstantPathPosition();
        TestPolylineIntercept();
        TestTurnIntercept();
        TestAcceleratedIntercept();
        TestAoeCircle();
        TestAoeLine();
        TestAoeCone();
        TestAntiJukePolicy();
        TestStateTransitions();
        TestAdaptiveLearning();
        TestMonteCarloResiduals();
        std::cout << "ZDPrediction math tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ZDPrediction math tests failed: " << error.what() << '\n';
        return 1;
    }
}
