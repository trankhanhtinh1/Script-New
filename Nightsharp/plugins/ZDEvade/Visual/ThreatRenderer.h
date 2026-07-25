#pragma once

#include "ThreatVisualGeometry.h"
#include "ThreatVisualPlan.h"
#include "ThreatVisualStyle.h"
#include "../../../SDK/SDK.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace ZDEvade {

class ThreatRenderer {
public:
    static void Draw(const Threat& threat, int now, float planeY) {
        if (!threat.HasData() || !std::isfinite(planeY)) return;

        const ThreatVisualPlan plan =
            ResolveThreatVisualPlan(threat, now);
        // Arc pathing is deliberately unsupported. Its dispatch has no body or
        // explosion so an admitted Arc cannot be rendered as a false chord.
        if (!plan.drawBody && !plan.drawEndExplosion) return;

        Vec2 labelAnchor = {};
        bool hasLabelAnchor = false;

        if (plan.drawBody) switch (plan.body) {
        case ThreatVisualBody::Capsule: {
            DrawPath(
                ThreatVisualGeometry::Capsule(
                    plan.line.head,
                    plan.line.end,
                    threat.Radius()),
                planeY);
            const Vec2 route =
                (plan.line.end - plan.line.head).Normalized();
            const Vec2 left(-route.y, route.x);
            labelAnchor =
                plan.line.head + left * (threat.Radius() + 24.0f);
            hasLabelAnchor = route.IsValid() && !route.IsZero();
            break;
        }
        case ThreatVisualBody::Circle:
            DrawCircle(threat.endPos, threat.Radius(), planeY);
            labelAnchor = threat.endPos +
                Vec2(0.0f, threat.Radius() + 24.0f);
            hasLabelAnchor = labelAnchor.IsValid();
            break;
        case ThreatVisualBody::Ring:
            DrawCircle(threat.endPos, threat.Radius(), planeY);
            DrawCircle(threat.endPos, threat.InnerRadius(), planeY);
            labelAnchor = threat.endPos +
                Vec2(0.0f, threat.Radius() + 24.0f);
            hasLabelAnchor = labelAnchor.IsValid();
            break;
        case ThreatVisualBody::Sector: {
            const float fullAngleRadians =
                threat.Angle() * kThreatVisualPi / 180.0f;
            DrawPath(
                ThreatVisualGeometry::Sector(
                    threat.startPos,
                    threat.direction,
                    threat.Range(),
                    fullAngleRadians,
                    kThreatVisualSectorArcSegments,
                    threat.ConeEdgePadding()),
                planeY);
            const Vec2 forward = threat.direction.Normalized();
            const Vec2 left(-forward.y, forward.x);
            labelAnchor = threat.startPos +
                forward * (threat.Range() * 0.55f) +
                left * 24.0f;
            hasLabelAnchor = forward.IsValid() && !forward.IsZero() &&
                labelAnchor.IsValid();
            break;
        }
        case ThreatVisualBody::None:
            return;
        }

        if (plan.drawEndExplosion) {
            DrawCircle(
                threat.EndExplosionCenter(),
                threat.EndExplosionRadius(),
                planeY);
        }

        if (hasLabelAnchor && !threat.SpellName().empty()) {
            SDK::Drawing::DrawText(
                Vec3::From2D(labelAnchor, planeY),
                threat.SpellName().c_str(),
                ThreatVisualStyle::kLabelColor,
                true);
        }
    }

private:
    static void DrawCircle(const Vec2& center,
                           float radius,
                           float planeY) {
        DrawPath(
            ThreatVisualGeometry::Circle(
                center,
                radius,
                kThreatVisualCircleSegments),
            planeY);
    }

    static void DrawPath(const ThreatVisualPath& path, float planeY) {
        if (path.count < 2 || path.count > kThreatVisualMaxPoints) return;

        std::array<Vec2, kThreatVisualMaxPoints> projectedPoints = {};
        std::array<bool, kThreatVisualMaxPoints> visible = {};
        for (std::size_t index = 0; index < path.count; ++index) {
            visible[index] = SDK::Drawing::WorldToScreen(
                    Vec3::From2D(path.points[index], planeY),
                    projectedPoints[index]) &&
                projectedPoints[index].IsValid();
        }

        const ThreatVisualVisibleRuns runs =
            ThreatVisualGeometry::SegmentVisibleRuns(
                visible,
                path.count,
                path.closed);
        std::array<Vec2, kThreatVisualMaxPoints> runPoints = {};
        for (std::size_t runIndex = 0;
             runIndex < runs.count;
             ++runIndex) {
            const ThreatVisualVisibleRun& run = runs.runs[runIndex];
            if (run.count < 2 || run.count > runPoints.size()) continue;

            for (std::size_t offset = 0; offset < run.count; ++offset) {
                runPoints[offset] = projectedPoints[
                    ThreatVisualGeometry::VisibleRunPointIndex(
                        run,
                        offset,
                        path.count)];
            }

            // Both passes reuse the same projected run. Partial closed paths
            // are deliberately open; only a fully visible path stays closed.
            SDK::Drawing::DrawPolyline(
                runPoints.data(),
                static_cast<int>(run.count),
                ThreatVisualStyle::kOuterStrokeThickness,
                ThreatVisualStyle::kOuterStrokeColor,
                run.closed);
            SDK::Drawing::DrawPolyline(
                runPoints.data(),
                static_cast<int>(run.count),
                ThreatVisualStyle::kCoreStrokeThickness,
                ThreatVisualStyle::kCoreStrokeColor,
                run.closed);
        }
    }
};

} // namespace ZDEvade
