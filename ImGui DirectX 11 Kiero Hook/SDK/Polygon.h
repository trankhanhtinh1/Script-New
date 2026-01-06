#pragma once
// ============================================================================
// POLYGON.H - Polygon classes for skillshot zones
// Port from EnsoulSharp.SDK Polygons to C++
// Used for: Evade, skillshot visualization, zone detection
// ============================================================================

#include <vector>
#include <cmath>
#include <algorithm>
#include "../Vector.h"

namespace SDK
{
    // ============================================================================
    // BASE POLYGON CLASS
    // ============================================================================
    class Polygon
    {
    public:
        std::vector<Vector3> Points;
        
        Polygon() = default;
        
        // Add a point to the polygon
        void Add(Vector3 point) {
            Points.push_back(point);
        }
        
        void Add(float x, float y, float z = 0) {
            Points.push_back(Vector3(x, y, z));
        }
        
        // Check if a point is inside the polygon (Ray casting algorithm)
        bool IsInside(Vector3 point) const {
            return !IsOutside(point);
        }
        
        bool IsOutside(Vector3 point) const {
            // Ray casting algorithm
            int count = 0;
            int n = (int)Points.size();
            
            for (int i = 0; i < n; i++) {
                Vector3 a = Points[i];
                Vector3 b = Points[(i + 1) % n];
                
                if ((a.z <= point.z && b.z > point.z) || (b.z <= point.z && a.z > point.z)) {
                    float vt = (point.z - a.z) / (b.z - a.z);
                    if (point.x < a.x + vt * (b.x - a.x)) {
                        count++;
                    }
                }
            }
            
            return (count % 2) == 0;  // Outside if even crossings
        }
        
        // Get the center of the polygon
        Vector3 GetCenter() const {
            if (Points.empty()) return Vector3(0, 0, 0);
            
            float sumX = 0, sumZ = 0;
            for (const auto& p : Points) {
                sumX += p.x;
                sumZ += p.z;
            }
            
            return Vector3(sumX / Points.size(), 0, sumZ / Points.size());
        }
        
        // Clear all points
        void Clear() {
            Points.clear();
        }
    };

    // ============================================================================
    // LINE POLYGON - For linear skillshots (Lux Q, Morgana Q)
    // ============================================================================
    class LinePoly : public Polygon
    {
    public:
        Vector3 Start;
        Vector3 End;
        float Width;
        
        LinePoly(Vector3 start, Vector3 end, float width)
            : Start(start), End(end), Width(width)
        {
            UpdatePolygon();
        }
        
        void UpdatePolygon() {
            Points.clear();
            
            // Calculate perpendicular direction
            Vector3 direction = (End - Start);
            float len = direction.Length();
            if (len < 0.001f) return;
            
            direction = direction / len;
            Vector3 perpendicular(-direction.z, 0, direction.x);
            
            float halfWidth = Width / 2.0f;
            
            // Add four corners of the rectangle
            Points.push_back(Start + perpendicular * halfWidth);
            Points.push_back(Start - perpendicular * halfWidth);
            Points.push_back(End - perpendicular * halfWidth);
            Points.push_back(End + perpendicular * halfWidth);
        }
        
        // Check if point is inside line hitbox
        bool IsInside(Vector3 point) const {
            // Use distance to line segment
            return DistanceToLineSegment(point, Start, End) <= Width / 2.0f;
        }
        
    private:
        static float DistanceToLineSegment(Vector3 point, Vector3 lineStart, Vector3 lineEnd) {
            Vector3 line = lineEnd - lineStart;
            float lineLenSq = line.x * line.x + line.z * line.z;
            
            if (lineLenSq < 0.0001f) {
                return (point - lineStart).Length();
            }
            
            float t = ((point.x - lineStart.x) * line.x + (point.z - lineStart.z) * line.z) / lineLenSq;
            t = std::max(0.0f, std::min(1.0f, t));
            
            Vector3 proj = lineStart + line * t;
            return (point - proj).Length();
        }
    };

    // ============================================================================
    // CIRCLE POLYGON - For circular skillshots (Lux E, Ziggs Q)
    // ============================================================================
    class CirclePoly : public Polygon
    {
    public:
        Vector3 Center;
        float Radius;
        int Quality;  // Number of segments
        
        CirclePoly(Vector3 center, float radius, int quality = 36)
            : Center(center), Radius(radius), Quality(quality)
        {
            UpdatePolygon();
        }
        
        void UpdatePolygon() {
            Points.clear();
            
            float step = 2.0f * 3.14159265f / Quality;
            for (int i = 0; i < Quality; i++) {
                float angle = step * i;
                float x = Center.x + Radius * cosf(angle);
                float z = Center.z + Radius * sinf(angle);
                Points.push_back(Vector3(x, 0, z));
            }
        }
        
        // Simple circle check
        bool IsInside(Vector3 point) const {
            return (point - Center).Length() <= Radius;
        }
        
        // Get point on circle edge at angle (radians)
        Vector3 GetPointOnCircle(float angle) const {
            float x = Center.x + Radius * cosf(angle);
            float z = Center.z + Radius * sinf(angle);
            return Vector3(x, 0, z);
        }
    };

    // ============================================================================
    // SECTOR (CONE) POLYGON - For cone skillshots (Annie W, Cho'Gath W)
    // ============================================================================
    class SectorPoly : public Polygon
    {
    public:
        Vector3 Center;
        Vector3 Direction;
        float Radius;
        float Angle;  // In radians
        int Quality;
        
        SectorPoly(Vector3 center, Vector3 direction, float angle, float radius, int quality = 20)
            : Center(center), Radius(radius), Angle(angle), Quality(quality)
        {
            // Normalize direction (XZ plane)
            float len = sqrtf(direction.x * direction.x + direction.z * direction.z);
            if (len > 0.001f) {
                Direction = Vector3(direction.x / len, 0, direction.z / len);
            } else {
                Direction = Vector3(1, 0, 0);
            }
            UpdatePolygon();
        }
        
        void UpdatePolygon() {
            Points.clear();
            
            // Add center point first
            Points.push_back(Center);
            
            // Calculate starting angle
            float startAngle = atan2f(Direction.z, Direction.x) - Angle / 2.0f;
            float step = Angle / Quality;
            
            // Add arc points
            for (int i = 0; i <= Quality; i++) {
                float currentAngle = startAngle + step * i;
                float x = Center.x + Radius * cosf(currentAngle);
                float z = Center.z + Radius * sinf(currentAngle);
                Points.push_back(Vector3(x, 0, z));
            }
        }
        
        // Check if point is inside cone
        bool IsInside(Vector3 point) const {
            // Check distance
            float dist = (point - Center).Length();
            if (dist > Radius || dist < 0.001f) return false;
            
            // Check angle
            Vector3 toPoint = (point - Center);
            float len = toPoint.Length();
            if (len < 0.001f) return true;  // At center
            
            toPoint = toPoint / len;
            
            // Dot product gives cosine of angle
            float dot = Direction.x * toPoint.x + Direction.z * toPoint.z;
            float angleBetween = acosf(std::max(-1.0f, std::min(1.0f, dot)));
            
            return angleBetween <= Angle / 2.0f;
        }
    };

    // ============================================================================
    // RECTANGLE POLYGON - For rectangular zones
    // ============================================================================
    class RectanglePoly : public Polygon
    {
    public:
        Vector3 Start;
        Vector3 End;
        float Width;
        
        RectanglePoly(Vector3 start, Vector3 end, float width)
            : Start(start), End(end), Width(width)
        {
            UpdatePolygon();
        }
        
        void UpdatePolygon() {
            Points.clear();
            
            Vector3 direction = (End - Start);
            float len = direction.Length();
            if (len < 0.001f) return;
            
            direction = direction / len;
            Vector3 perpendicular(-direction.z, 0, direction.x);
            
            float halfWidth = Width / 2.0f;
            
            Points.push_back(Start - perpendicular * halfWidth - direction * halfWidth);
            Points.push_back(Start + perpendicular * halfWidth - direction * halfWidth);
            Points.push_back(End + perpendicular * halfWidth + direction * halfWidth);
            Points.push_back(End - perpendicular * halfWidth + direction * halfWidth);
        }
    };

    // ============================================================================
    // RING POLYGON - Donut shape (e.g., Veigar E)
    // ============================================================================
    class RingPoly : public Polygon
    {
    public:
        Vector3 Center;
        float InnerRadius;
        float OuterRadius;
        int Quality;
        
        std::vector<Vector3> OuterPoints;
        std::vector<Vector3> InnerPoints;
        
        RingPoly(Vector3 center, float innerRadius, float outerRadius, int quality = 36)
            : Center(center), InnerRadius(innerRadius), OuterRadius(outerRadius), Quality(quality)
        {
            UpdatePolygon();
        }
        
        void UpdatePolygon() {
            Points.clear();
            OuterPoints.clear();
            InnerPoints.clear();
            
            float step = 2.0f * 3.14159265f / Quality;
            
            // Create outer circle
            for (int i = 0; i < Quality; i++) {
                float angle = step * i;
                float x = Center.x + OuterRadius * cosf(angle);
                float z = Center.z + OuterRadius * sinf(angle);
                OuterPoints.push_back(Vector3(x, 0, z));
                Points.push_back(Vector3(x, 0, z));
            }
            
            // Create inner circle
            for (int i = 0; i < Quality; i++) {
                float angle = step * i;
                float x = Center.x + InnerRadius * cosf(angle);
                float z = Center.z + InnerRadius * sinf(angle);
                InnerPoints.push_back(Vector3(x, 0, z));
            }
        }
        
        // Point is inside ring if between inner and outer radius
        bool IsInside(Vector3 point) const {
            float dist = (point - Center).Length();
            return dist >= InnerRadius && dist <= OuterRadius;
        }
    };

    // ============================================================================
    // ARC POLYGON - For arc-shaped skillshots
    // ============================================================================
    class ArcPoly : public Polygon
    {
    public:
        Vector3 Start;
        Vector3 End;
        float Distance;  // Height of arc from line
        int Quality;
        
        ArcPoly(Vector3 start, Vector3 end, float distance, int quality = 36)
            : Start(start), End(end), Distance(distance), Quality(quality)
        {
            UpdatePolygon();
        }
        
        void UpdatePolygon() {
            Points.clear();
            
            Vector3 midPoint = (Start + End) * 0.5f;
            Vector3 direction = End - Start;
            float lineLen = direction.Length();
            if (lineLen < 0.001f) return;
            
            direction = direction / lineLen;
            Vector3 perpendicular(-direction.z, 0, direction.x);
            
            // Arc center is perpendicular from midpoint
            Vector3 arcCenter = midPoint + perpendicular * Distance;
            
            // Calculate radius
            float radius = (Start - arcCenter).Length();
            
            // Calculate angles
            float startAngle = atan2f(Start.z - arcCenter.z, Start.x - arcCenter.x);
            float endAngle = atan2f(End.z - arcCenter.z, End.x - arcCenter.x);
            
            // Ensure we go the short way around
            if (Distance > 0) {
                if (endAngle < startAngle) endAngle += 2 * 3.14159265f;
            } else {
                if (startAngle < endAngle) startAngle += 2 * 3.14159265f;
            }
            
            float step = (endAngle - startAngle) / Quality;
            
            for (int i = 0; i <= Quality; i++) {
                float angle = startAngle + step * i;
                float x = arcCenter.x + radius * cosf(angle);
                float z = arcCenter.z + radius * sinf(angle);
                Points.push_back(Vector3(x, 0, z));
            }
        }
    };

    // ============================================================================
    // SKILLSHOT DATA - Combines type with polygon
    // ============================================================================
    struct SkillshotData
    {
        enum class Type {
            Line,
            Circle,
            Cone,
            Ring,
            Arc
        };
        
        Type SkillshotType;
        Vector3 Start;
        Vector3 End;
        float Radius;
        float Width;
        float Angle;  // For cones, in radians
        float Speed;
        float Delay;
        float InnerRadius;  // For rings
        bool IsDangerous;
        uint64_t OwnerAddress;
        float EndTime;  // Game time when skillshot expires
        
        // Create polygon based on type
        Polygon* CreatePolygon() const {
            switch (SkillshotType) {
                case Type::Line:
                    return new LinePoly(Start, End, Width);
                case Type::Circle:
                    return new CirclePoly(End, Radius);
                case Type::Cone:
                    return new SectorPoly(Start, End - Start, Angle, Radius);
                case Type::Ring:
                    return new RingPoly(End, InnerRadius, Radius);
                case Type::Arc:
                    return new ArcPoly(Start, End, Radius);
                default:
                    return nullptr;
            }
        }
        
        // Check if point is in danger zone
        bool IsPointInDanger(Vector3 point) const {
            Polygon* poly = CreatePolygon();
            if (!poly) return false;
            
            bool inside = poly->IsInside(point);
            delete poly;
            return inside;
        }
    };
}
