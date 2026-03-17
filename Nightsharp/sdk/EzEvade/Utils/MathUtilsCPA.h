#pragma once
#include <cmath>
#include <cfloat>
#include <algorithm>
#include "../../Math/MathUtils.h"
#include "MathUtilsEz.h"

// ============================================================================
// MathUtilsCPA — Closest Point of Approach algorithms
//   C# original: ezEvade.MathUtilsCPA (MathUtilsCPA.cs, 331 lines)
//   Line-by-line port preserving original logic
//
//   Copyright 2001 softSurfer, 2012 Dan Sunday
//   Freely usable/modifiable with this notice included.
//
//   Contains: line-to-line distance, segment-to-segment distance,
//   CPA time/distance for two tracks, and various CPA helper overloads.
// ============================================================================

namespace EzEvade {

    namespace MathUtilsCPA {

        // ====================================================================
        // Helper types
        //   C# lines 38-72
        // ====================================================================

        // C# lines 38-48: class Line
        struct Line {
            Vec2 P0;
            Vec2 P1;
            Line() = default;
            Line(Vec2 p0, Vec2 p1) : P0(p0), P1(p1) {}
        };

        // C# lines 50-60: class Segment
        struct Segment {
            Vec2 P0;
            Vec2 P1;
            Segment() = default;
            Segment(Vec2 p0, Vec2 p1) : P0(p0), P1(p1) {}
        };

        // C# lines 62-72: class Track
        struct Track {
            Vec2 P0;
            Vec2 v;
            Track() = default;
            Track(Vec2 p0, Vec2 vel) : P0(p0), v(vel) {}
        };

        // ====================================================================
        // Constants & helpers
        //   C# lines 75-94
        // ====================================================================
        constexpr float SMALL_NUM = 0.00000001f;                        // C# line 75

        // C# line 78-81: dot product
        inline float dot(const Vec2& u, const Vec2& v) {
            return u.x * v.x + u.y * v.y;                              // C# line 80
        }

        // C# line 82-85: norm (length)
        inline float norm(const Vec2& v) {
            return std::sqrt(dot(v, v));                                // C# line 84
        }

        // C# line 86-89: distance
        inline float d(const Vec2& u, const Vec2& v) {
            return norm(u - v);                                         // C# line 88
        }

        // ====================================================================
        // dist3D_Line_to_Line
        //   C# lines 99-129
        //   Minimum distance between two infinite lines (2D, despite the name)
        // ====================================================================
        inline float dist_Line_to_Line(const Line& L1, const Line& L2) {
            Vec2 u = L1.P1 - L1.P0;                                    // C# line 102
            Vec2 v = L2.P1 - L2.P0;                                    // C# line 103
            Vec2 w = L1.P0 - L2.P0;                                    // C# line 104

            float a = dot(u, u);                                        // C# line 105
            float b = dot(u, v);                                        // C# line 106
            float c = dot(v, v);                                        // C# line 107
            float dd = dot(u, w);                                       // C# line 108 (renamed to avoid shadowing)
            float e = dot(v, w);                                        // C# line 109
            float D = a * c - b * b;                                    // C# line 110

            float sc, tc;                                               // C# line 111

            if (D < SMALL_NUM)                                          // C# line 114
            {
                sc = 0.0f;                                              // C# line 116
                tc = (b > c ? dd / b : e / c);                          // C# line 117
            }
            else                                                        // C# line 119
            {
                sc = (b * e - c * dd) / D;                              // C# line 121
                tc = (a * e - b * dd) / D;                              // C# line 122
            }

            // C# line 126
            Vec2 dP = w + u * sc - v * tc;
            return norm(dP);                                            // C# line 128
        }

        // ====================================================================
        // dist3D_Segment_to_Segment
        //   C# lines 136-212
        //   Minimum distance between two line segments (2D, despite the name)
        // ====================================================================
        inline float dist_Segment_to_Segment(const Segment& S1, const Segment& S2) {
            Vec2 u = S1.P1 - S1.P0;                                    // C# line 138
            Vec2 v = S2.P1 - S2.P0;                                    // C# line 139
            Vec2 w = S1.P0 - S2.P0;                                    // C# line 140

            float a = dot(u, u);                                        // C# line 141
            float b = dot(u, v);                                        // C# line 142
            float c = dot(v, v);                                        // C# line 143
            float dd = dot(u, w);                                       // C# line 144
            float e = dot(v, w);                                        // C# line 145
            float D = a * c - b * b;                                    // C# line 146

            float sc, sN, sD = D;                                       // C# line 147
            float tc, tN, tD = D;                                       // C# line 148

            if (D < SMALL_NUM)                                          // C# line 151
            {
                sN = 0.0f;                                              // C# line 153
                sD = 1.0f;                                              // C# line 154
                tN = e;                                                 // C# line 155
                tD = c;                                                 // C# line 156
            }
            else                                                        // C# line 158
            {
                sN = (b * e - c * dd);                                  // C# line 160
                tN = (a * e - b * dd);                                  // C# line 161

                if (sN < 0.0f)                                          // C# line 162
                {
                    sN = 0.0f;                                          // C# line 164
                    tN = e;                                             // C# line 165
                    tD = c;                                             // C# line 166
                }
                else if (sN > sD)                                       // C# line 168
                {
                    sN = sD;                                            // C# line 170
                    tN = e + b;                                         // C# line 171
                    tD = c;                                             // C# line 172
                }
            }

            if (tN < 0.0f)                                              // C# line 176
            {
                tN = 0.0f;                                              // C# line 178
                if (-dd < 0.0f)                                         // C# line 180
                    sN = 0.0f;                                          // C# line 181
                else if (-dd > a)                                       // C# line 182
                    sN = sD;                                            // C# line 183
                else                                                    // C# line 185
                {
                    sN = -dd;                                           // C# line 186
                    sD = a;                                             // C# line 187
                }
            }
            else if (tN > tD)                                           // C# line 190
            {
                tN = tD;                                                // C# line 192
                if ((-dd + b) < 0.0f)                                   // C# line 194
                    sN = 0;                                             // C# line 195
                else if ((-dd + b) > a)                                 // C# line 196
                    sN = sD;                                            // C# line 197
                else                                                    // C# line 198
                {
                    sN = (-dd + b);                                     // C# line 200
                    sD = a;                                             // C# line 201
                }
            }

            // C# lines 204-206
            sc = (std::abs(sN) < SMALL_NUM ? 0.0f : sN / sD);
            tc = (std::abs(tN) < SMALL_NUM ? 0.0f : tN / tD);

            // C# line 209
            Vec2 dP = w + u * sc - v * tc;
            return norm(dP);                                            // C# line 211
        }

        // ====================================================================
        // cpa_time
        //   C# lines 219-231
        //   Compute the time of Closest Point of Approach for two tracks
        // ====================================================================
        inline float cpa_time(const Track& Tr1, const Track& Tr2) {
            Vec2 dv = Tr1.v - Tr2.v;                                   // C# line 221

            float dv2 = dot(dv, dv);                                    // C# line 223
            if (dv2 < SMALL_NUM)                                        // C# line 224
                return 0.0f;                                            // C# line 225

            Vec2 w0 = Tr1.P0 - Tr2.P0;                                 // C# line 227
            float cpatime = -dot(w0, dv) / dv2;                        // C# line 228

            return cpatime;                                             // C# line 230
        }

        // ====================================================================
        // cpa_distance
        //   C# lines 238-245
        //   Compute the distance at CPA for two tracks
        // ====================================================================
        inline float cpa_distance(const Track& Tr1, const Track& Tr2) {
            float ctime = cpa_time(Tr1, Tr2);                          // C# line 240
            Vec2 P1 = Tr1.P0 + Tr1.v * ctime;                         // C# line 241
            Vec2 P2 = Tr2.P0 + Tr2.v * ctime;                         // C# line 242

            return d(P1, P2);                                           // C# line 244
        }

        // ====================================================================
        // cpa_distance (4-arg overload)
        //   C# lines 248-251
        // ====================================================================
        inline float cpa_distance(const Vec2& p1, const Vec2& v1,
                                   const Vec2& p2, const Vec2& v2) {
            return cpa_distance(Track(p1, v1), Track(p2, v2));          // C# line 250
        }

        // ====================================================================
        // CPAPoints
        //   C# lines 253-273
        //   Returns CPA distance and the two closest positions
        // ====================================================================
        inline float CPAPoints(const Vec2& p1, const Vec2& v1,
                                const Vec2& p2, const Vec2& v2,
                                Vec2& ret1, Vec2& ret2)
        {
            Track Tr1(p1, v1);                                          // C# line 255
            Track Tr2(p2, v2);                                          // C# line 256

            float ctime = cpa_time(Tr1, Tr2);                          // C# line 258

            Vec2 P1 = Tr1.P0 + Tr1.v * ctime;                         // C# line 260
            Vec2 P2 = Tr2.P0 + Tr2.v * ctime;                         // C# line 261

            if (ctime <= 0)                                             // C# line 263
            {
                P1 = Tr1.P0;                                           // C# line 265
                P2 = Tr2.P0;                                           // C# line 266
            }

            ret1 = P1;                                                  // C# line 269
            ret2 = P2;                                                  // C# line 270

            return d(P1, P2);                                           // C# line 272
        }

        // ====================================================================
        // CPAPointsEx (simple — clamps to endpoints)
        //   C# lines 275-289
        // ====================================================================
        inline float CPAPointsEx(const Vec2& p1, const Vec2& v1,
                                  const Vec2& p2, const Vec2& v2,
                                  const Vec2& p1end, const Vec2& p2end)
        {
            Track Tr1(p1, v1);                                          // C# line 277
            Track Tr2(p2, v2);                                          // C# line 278

            float ctime = std::max(0.0f, cpa_time(Tr1, Tr2));          // C# line 280

            Vec2 P1 = Tr1.P0 + Tr1.v * ctime;                         // C# line 282
            Vec2 P2 = Tr2.P0 + Tr2.v * ctime;                         // C# line 283

            // C# lines 285-286: clamp to segment endpoints
            P1 = d(p1, P1) > d(p1, p1end) ? p1end : P1;
            P2 = d(p2, P2) > d(p2, p2end) ? p2end : P2;

            return d(P1, P2);                                           // C# line 288
        }

        // ====================================================================
        // CPAPointsEx (extended — with collision fallback & output)
        //   C# lines 291-320
        // ====================================================================
        inline float CPAPointsEx(const Vec2& p1, const Vec2& v1,
                                  const Vec2& p2, const Vec2& v2,
                                  const Vec2& p1end, const Vec2& p2end,
                                  Vec2& p1out, Vec2& p2out)
        {
            Track Tr1(p1, v1);                                          // C# line 294
            Track Tr2(p2, v2);                                          // C# line 295

            float ctime = cpa_time(Tr1, Tr2);                          // C# line 297

            // C# lines 299-308: if ctime == 0, try collision fallback
            if (ctime == 0)
            {
                bool collision;
                float collisionTime = EzMathUtils::GetCollisionTime(
                    p1, p2, v1, v2, 10, 10, collision);                 // C# line 302

                if (collision)                                          // C# line 304
                {
                    ctime = collisionTime;                              // C# line 306
                }
            }

            Vec2 P1 = Tr1.P0 + Tr1.v * ctime;                         // C# line 310
            Vec2 P2 = Tr2.P0 + Tr2.v * ctime;                         // C# line 311

            // C# lines 313-314: projection commented out in original
            // P1 = d(p1, P1) > d(p1, p1end) ? p1end : P1;
            // P2 = d(p2, P2) > d(p2, p2end) ? p2end : P2;

            p1out = P1;                                                 // C# line 316
            p2out = P2;                                                 // C# line 317

            return d(P1, P2);                                           // C# line 319
        }

        // ====================================================================
        // CPATime
        //   C# lines 322-328
        // ====================================================================
        inline float CPATime(const Vec2& p1, const Vec2& v1,
                              const Vec2& p2, const Vec2& v2)
        {
            Track Tr1(p1, v1);                                          // C# line 324
            Track Tr2(p2, v2);                                          // C# line 325

            return cpa_time(Tr1, Tr2);                                  // C# line 327
        }

    } // namespace MathUtilsCPA

} // namespace EzEvade
