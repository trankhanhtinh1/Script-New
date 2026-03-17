#pragma once
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include "../../GameObjects/GameObjects.h"
#include "../../Math/MathUtils.h"
#include "ObjectCache.h"
#include "../Utils/EvadeUtils.h"
#include "../Draw/RenderCircle.h"
#include "../Draw/RenderLine.h"
#include "../Draw/RenderObject.h"

// ============================================================================
// AutoSetPing
//   C# original: ezEvade.AutoSetPing (AutoSetPing.cs, 282 lines)
//   Line-by-line port preserving original logic
//
//   Automatically calibrates extra ping buffer by observing the delay
//   between issuing a move order and the server's new-path response.
// ============================================================================

namespace EzEvade {

    class AutoSetPing {
    public:
        // C# line 19: public static AIHeroClient myHero
        // → SDK::GameObjects::Player

        // ====================================================================
        // State variables (C# lines 21-41)
        // ====================================================================
        static inline float sumExtraDelayTime  = 0;                     // C# line 21
        static inline float avgExtraDelayTime  = 0;                     // C# line 22
        static inline float numExtraDelayTime  = 0;                     // C# line 23
        static inline float maxExtraDelayTime  = 0;                     // C# line 25

        // C# line 27: PlayerIssueOrderEventArgs lastIssueOrderArgs
        struct IssueOrderArgs {
            bool process = false;
            int  order   = 0;                                            // 0=None, 2=MoveTo
            Vec2 targetPosition = { 0, 0 };
        };
        static inline IssueOrderArgs lastIssueOrderArgs;                // C# line 27

        static inline Vec2 lastMoveToServerPos = { 0, 0 };             // C# line 28
        static inline Vec2 lastPathEndPos      = { 0, 0 };             // C# line 29

        // C# line 31: SpellbookCastSpellEventArgs lastSpellCastArgs
        struct SpellCastArgs {
            bool process = false;
        };
        static inline SpellCastArgs lastSpellCastArgs;                  // C# line 31
        static inline Vec2 lastSpellCastServerPos = { 0, 0 };          // C# line 32
        static inline Vec2 lastSpellCastEndPos    = { 0, 0 };          // C# line 33

        static inline float testSkillshotDelayStart = 0;                // C# line 35
        static inline bool  testSkillshotDelayOn    = false;            // C# line 36
        static inline bool  checkPing               = true;             // C# line 38

        static inline std::vector<float> pingList;                      // C# line 40

        // ====================================================================
        // Constructor
        //   C# lines 44-66
        //   Hooks events — in C++ we hook via explicit Initialize() call
        // ====================================================================
        static void Initialize() {
            // C# line 46: AIHeroClient.OnNewPath += Hero_OnNewPath;
            // C# line 47: Player.OnIssueOrder += Hero_OnIssueOrder;
            // C# line 49: Spellbook.OnCastSpell += Game_OnCastSpell;
            // C# line 50: MissileClient.OnCreate += Game_OnCreateObj;
            // C# line 51: AIHeroClient.OnProcessSpellCast += Game_ProcessSpell;
            // Events wired externally via hook system
        }

        // ====================================================================
        // Game_ProcessSpell
        //   C# lines 68-76
        //   Currently only checks sender.IsMe — essentially a no-op in original
        // ====================================================================
        static void OnProcessSpell(SDK::GameObject* sender) {
            if (!sender || sender != &SDK::GameObjects::Player)         // C# line 70
            {
                return;
            }
            // C# line 75: lastSpellCastServerPos = myHero.Position.To2D(); (commented out in original)
        }

        // ====================================================================
        // Hero_OnIssueOrder
        //   C# lines 145-174
        // ====================================================================
        static void OnIssueOrder(SDK::GameObject* hero, int orderType, Vec2 targetPos) {
            checkPing = false;                                          // C# line 147

            auto& myHero = SDK::GameObjects::Player;

            // C# lines 149-151: distance / moveTime debug (commented out in original)
            float distance = myHero.GetPosition().To2D().Distance(
                myHero.GetPosition().To2D());                           // ServerPosition approx
            float moveTime = 1000.0f * distance / myHero.GetMoveSpeed();
            (void)moveTime;

            // C# line 153: if (menuCache["AutoSetPingOn"].GetValue<bool>() == false) return;
            if (!ObjectCache::GetBool("AutoSetPingOn")) return;             // C# line 153

            // C# line 158: if (!hero.IsMe) return;
            if (hero != &myHero)
            {
                return;
            }

            lastIssueOrderArgs.process = true;
            lastIssueOrderArgs.order = orderType;
            lastIssueOrderArgs.targetPosition = targetPos;

            // C# line 165: if (args.Order == GameObjectOrder.MoveTo)
            if (orderType == 2) // MoveTo
            {
                // C# line 167: if (myHero.IsMoving && myHero.Path.Count() > 0)
                if (myHero.IsMoving())
                {
                    lastMoveToServerPos = myHero.GetPosition().To2D();  // C# line 169
                    // C# line 170: lastPathEndPos = myHero.Path.Last().To2D();
                    lastPathEndPos = myHero.GetPosition().To2D(); // TODO: path end pos
                    checkPing = true;                                   // C# line 171
                }
            }
        }

        // ====================================================================
        // Hero_OnNewPath
        //   C# lines 176-279
        //   Main ping calculation logic
        // ====================================================================
        static void OnNewPath(SDK::GameObject* hero, const std::vector<Vec3>& path, bool isDash) {
            // C# line 178: if (menuCache["AutoSetPingOn"].GetValue<bool>() == false) return;
            if (!ObjectCache::GetBool("AutoSetPingOn")) return;             // C# line 178

            auto& myHero = SDK::GameObjects::Player;

            // C# line 183: if (!hero.IsMe) return;
            if (hero != &myHero)
            {
                return;
            }

            // C# line 190: if (path.Length > 1 && !args.IsDash)
            if (path.size() > 1 && !isDash)
            {
                Vec2 movePos = path.back().To2D();                      // C# line 192

                // C# lines 194-200: complex validation
                if (checkPing
                    && lastIssueOrderArgs.process == true               // C# line 195
                    && lastIssueOrderArgs.order == 2                    // C# line 196: MoveTo
                    && lastIssueOrderArgs.targetPosition.Distance(movePos) < 3 // C# line 197
                    // C# line 198-200: path count and isMoving checks
                    && path.size() == 2
                    && myHero.IsMoving())
                {
                    // C# lines 204-206: RenderLine visualization
                    RenderObjects::Add(new RenderLine(
                        path.front().To2D(), path.back().To2D(), 1000.0f));
                    RenderObjects::Add(new RenderLine(
                        myHero.GetPosition().To2D(), path.back().To2D(), 1000.0f));

                    // C# lines 209-215: distance check
                    float distanceTillEnd = path.back().To2D().Distance(
                        myHero.GetPosition().To2D());
                    float moveTimeTillEnd = 1000.0f * distanceTillEnd / myHero.GetMoveSpeed();

                    if (moveTimeTillEnd < 500)                          // C# line 212
                    {
                        return;                                         // C# line 214
                    }

                    // C# lines 217-274: Ray intersection logic for ping calculation
                    Vec2 dir1 = (path.back().To2D() - myHero.GetPosition().To2D()).Normalized(); // C# line 217

                    Vec2 dir2 = (path.front().To2D() - path.back().To2D()).Normalized(); // C# line 220

                    // Simple line intersection (replacing SharpDX Ray.Intersects)
                    // P1 + t*D1 = P2 + s*D2
                    Vec2 P1 = myHero.GetPosition().To2D();
                    Vec2 P2 = path.front().To2D();

                    float cross = dir1.x * dir2.y - dir1.y * dir2.x;
                    if (std::abs(cross) > 0.001f)
                    {
                        Vec2 diff = P2 - P1;
                        float t = (diff.x * dir2.y - diff.y * dir2.x) / cross;

                        Vec2 intersection = P1 + dir1 * t;             // C# line 227

                        // C# line 229: projection check
                        auto projection = Vec2_ProjectOn(intersection, 
                            path.back().To2D(), myHero.GetPosition().To2D());

                        // C# line 231: angle check
                        float angle = std::abs(std::atan2(
                            dir1.x * dir2.y - dir1.y * dir2.x,
                            dir1.x * dir2.x + dir1.y * dir2.y)) * 180.0f / 3.14159265f;

                        if (projection.isOnSegment && angle > 20 && angle < 160) // C# line 231
                        {
                            // C# line 233: render intersection
                            RenderObjects::Add(new RenderCircle(
                                intersection, 1000.0f, IM_COL32(255, 0, 0, 255), 10));

                            // C# lines 235-237
                            float dist = lastMoveToServerPos.Distance(intersection);
                            float pingMoveTime = 1000.0f * dist / myHero.GetMoveSpeed();

                            if (pingMoveTime < 1000)                    // C# line 241
                            {
                                if (numExtraDelayTime > 0)              // C# line 243
                                {
                                    sumExtraDelayTime += pingMoveTime;   // C# line 245
                                    avgExtraDelayTime = sumExtraDelayTime / numExtraDelayTime; // C# line 246
                                    pingList.push_back(pingMoveTime);   // C# line 248
                                }
                                numExtraDelayTime += 1;                 // C# line 250

                                if (maxExtraDelayTime == 0)             // C# line 252
                                {
                                    maxExtraDelayTime = (float)ObjectCache::GetSlider("ExtraPingBuffer"); // C# line 254
                                }

                                // C# lines 257-269: periodic percentile recalculation
                                if ((int)numExtraDelayTime % 100 == 0)  // C# line 257
                                {
                                    std::sort(pingList.begin(), pingList.end()); // C# line 259

                                    int percentile = ObjectCache::GetSlider("AutoSetPercentile"); // C# line 260
                                    int percentIndex = (int)std::floor(
                                        pingList.size() * (percentile / 100.0f)) - 1; // C# line 262
                                    if (percentIndex < 0) percentIndex = 0;

                                    float gamePing = ObjectCache::gamePing;
                                    maxExtraDelayTime = std::max(
                                        pingList[percentIndex] - gamePing, 0.0f); // C# line 263

                                    // TODO: ObjectCache::menuCache["ExtraPingBuffer"].SetValue(...)
                                    // C# line 264

                                    pingList.clear();                   // C# line 266
                                }
                            }
                        }
                    }
                }

                checkPing = false;                                      // C# line 277
            }
        }

        // ====================================================================
        // Game_OnCastSpell
        //   C# lines 84-107
        // ====================================================================
        static void OnCastSpell(SDK::GameObject* sender) {
            checkPing = false;                                          // C# line 88

            auto& myHero = SDK::GameObjects::Player;

            if (sender != &myHero)                                      // C# line 90
            {
                return;
            }

            lastSpellCastArgs.process = true;                           // C# line 95 (implicit)

            // C# line 98: if (myHero.IsMoving && myHero.Path.Count() > 0)
            if (myHero.IsMoving())
            {
                lastSpellCastServerPos = EvadeUtils::GetGamePosition(
                    &myHero, ObjectCache::gamePing);                    // C# line 100
                // C# line 101: lastSpellCastEndPos = myHero.Path.Last().To2D()
                lastSpellCastEndPos = myHero.GetPosition().To2D();      // approximation
                checkPing = true;                                       // C# line 102

                // C# line 104: RenderObjects.Add(new RenderCircle(lastSpellCastServerPos, 1000, Color.Green, 10))
                RenderObjects::Add(new RenderCircle(
                    lastSpellCastServerPos, 1000.0f, IM_COL32(0, 255, 0, 255), 10));
            }
        }

        // ====================================================================
        // Game_OnCreateObj (missile created)
        //   C# lines 109-125
        // ====================================================================
        static void OnMissileCreated(SDK::GameObject* missile, int casterNetId) {
            auto& myHero = SDK::GameObjects::Player;

            // C# line 112: if (missile != null && missile.SpellCaster.IsMe)
            if (missile && casterNetId == myHero.GetNetId())
            {
                // C# line 114: if (lastSpellCastArgs.Process == true)
                if (lastSpellCastArgs.process)
                {
                    // C# line 118: RenderCircle at missile start
                    Vec2 missileStartPos = missile->GetPosition().To2D();
                    RenderObjects::Add(new RenderCircle(
                        missileStartPos, 1000.0f, IM_COL32(255, 0, 0, 255), 10)); // C# line 118

                    // C# lines 120-122
                    float distance = lastSpellCastServerPos.Distance(missileStartPos);
                    float moveTime = 1000.0f * distance / myHero.GetMoveSpeed();
                    // Console.WriteLine("Extra Delay: " + moveTime);   // C# line 122
                    (void)moveTime;
                }
            }
        }
    };

} // namespace EzEvade
