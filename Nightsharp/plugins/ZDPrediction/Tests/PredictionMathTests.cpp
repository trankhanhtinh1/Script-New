#include "../Math/Kinematics.h"
#include "../Analysis/AoeOptimizer.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {

using ZDPrediction::AoeOptimizer;
using ZDPrediction::AoePoint;
using ZDPrediction::Math::Distance;
using ZDPrediction::Math::InterceptSolution;
using ZDPrediction::Math::SolveLinearIntercept;
using ZDPrediction::Math::SolvePathIntercept;
using ZDPrediction::Math::Vector2;

void Require(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "FAILED: " << name << '\n';
        std::abort();
    }
}

void TestStationaryDelayedProjectile() {
    const InterceptSolution solution = SolveLinearIntercept(
        {0.0, 0.0}, {100.0, 0.0}, {}, 1000.0, 0.10, 2.0);
    Require(solution.valid, "stationary delayed valid");
    Require(std::abs(solution.time - 0.20) < 1e-6, "stationary delayed time");
    Require(Distance(solution.position, {100.0, 0.0}) < 1e-6, "stationary delayed position");
}

void TestMovingLinearIntercept() {
    const InterceptSolution solution = SolveLinearIntercept(
        {0.0, 0.0}, {100.0, 0.0}, {100.0, 0.0}, 1000.0, 0.0, 2.0);
    Require(solution.valid, "moving linear valid");
    Require(solution.time > 0.10 && solution.time < 0.12, "moving linear time");
    Require(solution.position.x > 110.0 && solution.position.x < 112.0, "moving linear position");
}

void TestInstantProjectile() {
    const InterceptSolution solution = SolveLinearIntercept(
        {0.0, 0.0}, {50.0, 20.0}, {300.0, 0.0},
        std::numeric_limits<double>::infinity(), 0.25, 2.0);
    Require(solution.valid, "instant valid");
    Require(std::abs(solution.time - 0.25) < 1e-9, "instant time");
    Require(Distance(solution.position, {125.0, 20.0}) < 1e-6, "instant position");
}

void TestPiecewisePath() {
    const std::vector<Vector2> path = {{0.0, 0.0}, {100.0, 0.0}, {100.0, 100.0}};
    const InterceptSolution solution = SolvePathIntercept(
        {-100.0, 0.0}, path, 100.0, 1000.0, 0.0, 3.0);
    Require(solution.valid, "piecewise path valid");
    Require(solution.position.IsFinite(), "piecewise path finite");
    Require(solution.time >= 0.0 && solution.time <= 3.0, "piecewise path time");
}

void TestNoSolution() {
    const InterceptSolution solution = SolveLinearIntercept(
        {0.0, 0.0}, {100.0, 0.0}, {2000.0, 0.0}, 500.0, 0.0, 1.0);
    Require(!solution.valid, "no solution rejected");
}

void TestAoePrimaryInvariant() {
    const std::vector<AoePoint> points = {
        {1, {300.0, 0.0}, true},
        {2, {300.0, 40.0}, false},
        {3, {300.0, 80.0}, false}
    };
    const auto solution = AoeOptimizer::Line({0.0, 0.0}, points, 45.0, 1000.0, 1);
    Require(solution.valid, "aoe line valid");
    Require(solution.hitIds.size() >= 2, "aoe line hit count");
    Require(std::find(solution.hitIds.begin(), solution.hitIds.end(), 1) != solution.hitIds.end(),
            "aoe primary preserved");
}

}

int main() {
    TestStationaryDelayedProjectile();
    TestMovingLinearIntercept();
    TestInstantProjectile();
    TestPiecewisePath();
    TestNoSolution();
    TestAoePrimaryInvariant();
    std::cout << "Prediction math tests passed\n";
    return 0;
}
