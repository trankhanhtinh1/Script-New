#pragma once
#include <cmath>
#include <cfloat>
#include <algorithm>
#include <tuple>
#include "../../Math/MathUtils.h"

// ============================================================================
// EzEvade::MathUtils (geometry helpers)
//   C# original: ezEvade.MathUtils (MathUtils.cs, 325 lines)
//   Line-by-line port preserving original logic
//
//   Contains: line intersection, rotation, projectile movement collision,
//   point-on-segment, circle-circle collision time, line-circle intersection.
// ============================================================================

namespace EzEvade {

    namespace EzMathUtils {

        // ====================================================================
        // LineToLineIntersection
        //   C# lines 65-78
        //   Returns (t1, t2) parameters for intersection of line (x1,y1)-(x2,y2)
        //   with line (x3,y3)-(x4,y4).
        //   If lines are parallel, returns (FLT_MAX, FLT_MAX).
        // ====================================================================
        inline std::pair<float, float> LineToLineIntersection(
            float x1, float y1, float x2, float y2,
            float x3, float y3, float x4, float y4)
        {
            // C# line 67: var d = (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);
            float d = (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);

            if (d == 0)                                                 // C# line 69
            {
                return { FLT_MAX, FLT_MAX };                            // C# line 71: parallel
            }
            else                                                        // C# line 73
            {
                float t1 = ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / d; // C# line 75
                float t2 = ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / d; // C# line 76
                return { t1, t2 };
            }
        }

        // ====================================================================
        // CheckLineIntersection
        //   C# lines 15-18
        //   Uses LeagueSharp's Intersection() helper. Port uses our own.
        // ====================================================================
        inline bool CheckLineIntersection(const Vec2& a, const Vec2& b,
                                           const Vec2& c, const Vec2& d)
        {
            // C# line 17: return a.Intersection(b, c, d).Intersects;
            // Reimplement: check if segments a-b and c-d intersect
            auto [t1, t2] = LineToLineIntersection(
                a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y);
            return (t1 >= 0 && t1 <= 1 && t2 >= 0 && t2 <= 1);
        }

        // ====================================================================
        // CheckLineIntersectionEx
        //   C# lines 20-35
        //   Explicit segment intersection check using our LineToLineIntersection
        // ====================================================================
        inline bool CheckLineIntersectionEx(const Vec2& a, const Vec2& b,
                                             const Vec2& c, const Vec2& d)
        {
            auto [t1, t2] = LineToLineIntersection(                     // C# line 22
                a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y);

            // C# lines 27-34
            if (t1 >= 0 && t1 <= 1 && t2 >= 0 && t2 <= 1)
            {
                return true;                                            // C# line 29
            }
            else
            {
                return false;                                           // C# line 33
            }
        }

        // ====================================================================
        // CheckLineIntersectionEx2
        //   C# lines 37-52
        //   Returns (t1, t2) if intersection, or (0, 0) if no intersection
        // ====================================================================
        inline Vec2 CheckLineIntersectionEx2(const Vec2& a, const Vec2& b,
                                              const Vec2& c, const Vec2& d)
        {
            auto [t1, t2] = LineToLineIntersection(                     // C# line 39
                a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y);

            // C# lines 44-51
            if (t1 >= 0 && t1 <= 1 && t2 >= 0 && t2 <= 1)
            {
                return Vec2(t1, t2);                                    // C# line 46
            }
            else
            {
                return Vec2(0, 0);                                      // C# line 50: Vector2.Zero
            }
        }

        // ====================================================================
        // RotateVector
        //   C# lines 54-63
        //   public static Vector2 RotateVector(Vector2 start, Vector2 end, float angle)
        //   angle is in degrees
        // ====================================================================
        inline Vec2 RotateVector(const Vec2& start, const Vec2& end, float angle) {
            // C# line 56: angle = angle * (Math.PI / 180)
            angle = angle * (3.14159265358979323846f / 180.0f);

            Vec2 ret = end;                                             // C# line 57
            // C# lines 58-61
            ret.x = std::cos(angle) * (end.x - start.x) -
                    std::sin(angle) * (end.y - start.y) + start.x;
            ret.y = std::sin(angle) * (end.x - start.x) +
                    std::cos(angle) * (end.y - start.y) + start.y;
            return ret;                                                 // C# line 62
        }

        // ====================================================================
        // VectorMovementCollisionEx
        //   C# lines 100-140
        //   Calculates the time for a projectile from sourcePos at projSpeed
        //   to intercept a target at targetPos moving in targetDir at targetSpeed.
        // ====================================================================
        inline float VectorMovementCollisionEx(
            const Vec2& targetPos, const Vec2& targetDir, float targetSpeed,
            const Vec2& sourcePos, float projSpeed,
            bool& collision,
            float extraDelay = 0, float extraDist = 0)
        {
            Vec2 velocity = targetDir * targetSpeed;                    // C# line 102
            Vec2 adjTargetPos = targetPos - velocity * (extraDelay / 1000.0f); // C# line 103

            float velocityX = velocity.x;                               // C# line 105
            float velocityY = velocity.y;                               // C# line 106

            Vec2 relStart = adjTargetPos - sourcePos;                   // C# line 108

            float relStartX = relStart.x;                               // C# line 110
            float relStartY = relStart.y;                               // C# line 111

            // C# lines 113-115
            float a = velocityX * velocityX + velocityY * velocityY - projSpeed * projSpeed;
            float b = 2.0f * velocityX * relStartX + 2.0f * velocityY * relStartY;
            float c = std::max(0.0f,
                relStartX * relStartX + relStartY * relStartY + extraDist * extraDist);

            float disc = b * b - 4.0f * a * c;                         // C# line 117

            if (disc >= 0)                                              // C# line 119
            {
                float t1 = -(b + std::sqrt(disc)) / (2.0f * a);        // C# line 121
                float t2 = -(b - std::sqrt(disc)) / (2.0f * a);        // C# line 122

                collision = true;                                       // C# line 124

                if (t1 > 0 && t2 > 0)                                  // C# line 126
                {
                    return (t1 > t2) ? t2 : t1;                        // C# line 128
                }
                else if (t1 > 0)                                       // C# line 131
                    return t1;                                          // C# line 132
                else if (t2 > 0)                                       // C# line 133
                    return t2;                                          // C# line 134
            }

            collision = false;                                          // C# line 137
            return 0;                                                   // C# line 139
        }

        // ====================================================================
        // PointOnLineSegment (dot-product method)
        //   C# lines 142-153
        // ====================================================================
        inline bool PointOnLineSegment(const Vec2& point, const Vec2& start, const Vec2& end) {
            Vec2 toEnd   = end - start;                                 // C# line 144
            Vec2 toPoint = point - start;

            float dotProduct = toEnd.x * toPoint.x + toEnd.y * toPoint.y; // Vec2.Dot
            if (dotProduct < 0)                                         // C# line 145
                return false;

            float lengthSquared = (end.x - start.x) * (end.x - start.x) +
                                  (end.y - start.y) * (end.y - start.y); // C# line 148
            if (dotProduct > lengthSquared)                             // C# line 149
                return false;

            return true;                                                // C# line 152
        }

        // ====================================================================
        // isPointOnLineSegment (bounding-box method)
        //   C# lines 155-164
        // ====================================================================
        inline bool IsPointOnLineSegment(const Vec2& point, const Vec2& start, const Vec2& end) {
            // C# lines 157-158
            if (std::max(start.x, end.x) > point.x && point.x > std::min(start.x, end.x)
                && std::max(start.y, end.y) > point.y && point.y > std::min(start.y, end.y))
            {
                return true;                                            // C# line 160
            }
            return false;                                               // C# line 163
        }

        // ====================================================================
        // GetCollisionTime
        //   C# lines 168-204
        //   Circle-vs-circle collision time using relative velocity
        // ====================================================================
        inline float GetCollisionTime(
            const Vec2& Pa, const Vec2& Pb,
            const Vec2& Va, const Vec2& Vb,
            float Ra, float Rb,
            bool& collision)
        {
            Vec2 Pab = Pa - Pb;                                        // C# line 170
            Vec2 Vab = Va - Vb;                                        // C# line 171

            // Dot products
            float a = Vab.x * Vab.x + Vab.y * Vab.y;                  // C# line 172
            float b = 2.0f * (Pab.x * Vab.x + Pab.y * Vab.y);        // C# line 173
            float c = (Pab.x * Pab.x + Pab.y * Pab.y) -
                      (Ra + Rb) * (Ra + Rb);                           // C# line 174

            float discriminant = b * b - 4.0f * a * c;                 // C# line 176

            float t;                                                    // C# line 178
            if (discriminant < 0)                                       // C# line 179
            {
                t = -b / (2.0f * a);                                   // C# line 181
                collision = false;                                      // C# line 182
            }
            else                                                        // C# line 184
            {
                float t0 = (-b + std::sqrt(discriminant)) / (2.0f * a); // C# line 186
                float t1 = (-b - std::sqrt(discriminant)) / (2.0f * a); // C# line 187

                if (t0 >= 0 && t1 >= 0)                                 // C# line 189
                    t = std::min(t0, t1);                               // C# line 190
                else
                    t = std::max(t0, t1);                               // C# line 192

                if (t < 0)                                              // C# line 194
                    collision = false;                                   // C# line 195
                else
                    collision = true;                                   // C# line 197
            }

            if (t < 0)                                                  // C# line 200
                t = 0;                                                  // C# line 201

            return t;                                                   // C# line 203
        }

        // ====================================================================
        // GetCollisionDistanceEx
        //   C# lines 206-225
        //   Returns distance between two objects at their collision time
        // ====================================================================
        inline float GetCollisionDistanceEx(
            const Vec2& Pa, const Vec2& Va, float Ra,
            const Vec2& Pb, const Vec2& Vb, float Rb,
            Vec2& PA_out, Vec2& PB_out)
        {
            bool collision;                                             // C# line 210
            float collisionTime = GetCollisionTime(Pa, Pb, Va, Vb, Ra, Rb, collision); // C# line 211

            if (collision)                                              // C# line 213
            {
                PA_out = Pa + Va * collisionTime;                       // C# line 215
                PB_out = Pb + Vb * collisionTime;                       // C# line 216
                return PA_out.Distance(PB_out);                         // C# line 218
            }

            PA_out = Vec2(0, 0);                                        // C# line 221
            PB_out = Vec2(0, 0);                                        // C# line 222
            return FLT_MAX;                                             // C# line 224
        }

        // ====================================================================
        // GetCollisionDistance
        //   C# lines 227-245
        //   Same as above but projects result onto their respective segments
        // ====================================================================
        inline float GetCollisionDistance(
            const Vec2& Pa, const Vec2& PaEnd, const Vec2& Va, float Ra,
            const Vec2& Pb, const Vec2& PbEnd, const Vec2& Vb, float Rb)
        {
            bool collision;                                             // C# line 230
            float collisionTime = GetCollisionTime(Pa, Pb, Va, Vb, Ra, Rb, collision); // C# line 231

            if (collision)                                              // C# line 233
            {
                Vec2 PA_raw = Pa + Va * collisionTime;                  // C# line 235
                Vec2 PB_raw = Pb + Vb * collisionTime;                  // C# line 236

                // C# line 238: PA = Vec2_ProjectOn(PA, Pa, PaEnd).segmentPoint;
                auto projA = Vec2_ProjectOn(PA_raw, Pa, PaEnd);
                Vec2 PA = projA.segmentPoint;

                // C# line 239: PB = Vec2_ProjectOn(PB, Pb, PbEnd).segmentPoint;
                auto projB = Vec2_ProjectOn(PB_raw, Pb, PbEnd);
                Vec2 PB = projB.segmentPoint;

                return PA.Distance(PB);                                 // C# line 241
            }

            return FLT_MAX;                                             // C# line 244
        }

        // ====================================================================
        // FindLineCircleIntersections
        //   C# lines 249-322
        //   Find intersection(s) of segment from-to with circle (center, radius)
        //   Returns count of intersections on segment (0, 1, or 2)
        // ====================================================================
        inline int FindLineCircleIntersections(
            const Vec2& center, float radius,
            const Vec2& from, const Vec2& to,
            Vec2& intersection1, Vec2& intersection2)
        {
            float cx = center.x;                                        // C# line 254
            float cy = center.y;                                        // C# line 255
            float dx, dy, A, B, C, det, t;                             // C# line 256

            dx = to.x - from.x;                                        // C# line 258
            dy = to.y - from.y;                                        // C# line 259

            A = dx * dx + dy * dy;                                      // C# line 261
            B = 2.0f * (dx * (from.x - cx) + dy * (from.y - cy));     // C# line 262
            C = (from.x - cx) * (from.x - cx) +
                (from.y - cy) * (from.y - cy) -
                radius * radius;                                        // C# lines 263-265

            det = B * B - 4.0f * A * C;                                 // C# line 267

            if ((A <= 0.0000001f) || (det < 0))                         // C# line 268
            {
                // No real solutions
                intersection1 = Vec2(NAN, NAN);                         // C# line 271
                intersection2 = Vec2(NAN, NAN);                         // C# line 272
                return 0;                                               // C# line 273
            }
            else if (det == 0)                                          // C# line 275
            {
                // One solution
                t = -B / (2.0f * A);                                    // C# line 278
                intersection1 = Vec2(from.x + t * dx, from.y + t * dy); // C# lines 279-280
                intersection2 = Vec2(NAN, NAN);                         // C# line 281

                // C# lines 283-291: check if on segment
                auto projection1 = Vec2_ProjectOn(intersection1, from, to);
                if (projection1.isOnSegment)                            // C# line 284
                {
                    return 1;                                           // C# line 286
                }
                else
                {
                    return 0;                                           // C# line 290
                }
            }
            else                                                        // C# line 293
            {
                // Two solutions
                t = (-B + std::sqrt(det)) / (2.0f * A);                // C# line 296
                intersection1 = Vec2(from.x + t * dx, from.y + t * dy); // C# lines 297-298

                t = (-B - std::sqrt(det)) / (2.0f * A);                // C# line 299
                intersection2 = Vec2(from.x + t * dx, from.y + t * dy); // C# lines 300-301

                // C# lines 303-318: check which are on segment
                auto projection1 = Vec2_ProjectOn(intersection1, from, to);
                auto projection2 = Vec2_ProjectOn(intersection2, from, to);

                if (projection1.isOnSegment && projection2.isOnSegment) // C# line 306
                {
                    return 2;                                           // C# line 308
                }
                else if (projection1.isOnSegment && !projection2.isOnSegment) // C# line 310
                {
                    return 1;                                           // C# line 312
                }
                else if (!projection1.isOnSegment && projection2.isOnSegment) // C# line 314
                {
                    intersection1 = intersection2;                      // C# line 316
                    return 1;                                           // C# line 317
                }

                return 0;                                               // C# line 320
            }
        }

    } // namespace EzMathUtils

} // namespace EzEvade
