#pragma once

// ============================================================================
// EzEvadePlugin.h — EzEvade ("ezEvade") ported to NightSharp C++
//
// Source: plugins/EzEvade/EzEvade_CSharp/ (LeagueSharp/EloBuddy C#).
//
// Architecture note (important):
//   NightSharp already ships a full skillshot DETECTION layer:
//     SDK::Tracker::DetectedSkillshots() -> vector<shared_ptr<Skillshot>>
//   Each Skillshot carries StartPosition/EndPosition/Direction, an SData
//   entry (Range/Radius/Delay/MissileSpeed/Angle), a polygon Path, live
//   missile position (SkillshotMissile::GetMissilePosition), HasExpired(),
//   and Draw(). The 133KB C# SpellDatabase + SpellDetector are therefore
//   handled natively and are NOT re-ported.
//
//   What IS ported here is EzEvade's *decision engine* — the part the SDK
//   does not provide (BaseSpell::IsAboutToHit is a documented placeholder):
//     - CPA (closest point of approach) math   [MathUtilsCPA.cs]
//     - closest-distance-of-approach per spell  [EvadeHelper.cs]
//     - the ring/candidate position search       [EvadeHelper.GetBestPosition]
//     - the move-order dodge loop                 [Evade.cs]
//
// Detection -> Skillshot (SDK).  Decision -> this plugin.  Action -> IssueMove.
//
// Follows the verified plugin pattern from
//   plugins/Champion/EzrealMissileLifecyclePlugin.h
// ============================================================================

#include "../IPlugin.h"
#include "../../Core/CoreRuntime.h"
#include "../../Core/Globals.h"
#include "../../Core/CoreControl.h"
#include "../../Core/CoreNavGrid.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../SDK/UI/IMenu/Menu.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace Plugins {

// ============================================================================
// CPA math — 1:1 port of MathUtilsCPA.cs (the 2D subset EzEvade actually uses).
// All positions are world X/Z packed into Vec2 (x=world.x, y=world.z), exactly
// like the C# Vector2 usage.
// ============================================================================
namespace EvadeMath {

inline constexpr float kSmallNum = 0.00000001f;

inline float Dot(const Vec2& u, const Vec2& v) { return u.x * v.x + u.y * v.y; }
inline float Norm(const Vec2& v) { return std::sqrt(Dot(v, v)); }
inline float Dist(const Vec2& u, const Vec2& v) { return Norm(u - v); }

// cpa_time: time at which two linear tracks are closest.
inline float CpaTime(const Vec2& p1, const Vec2& v1, const Vec2& p2, const Vec2& v2) {
    const Vec2 dv = v1 - v2;
    const float dv2 = Dot(dv, dv);
    if (dv2 < kSmallNum) {
        return 0.0f; // near-parallel; any time works, use 0
    }
    const Vec2 w0 = p1 - p2;
    return -Dot(w0, dv) / dv2;
}

// VectorMovementCollisionEx / GetCollisionTime hybrid used by CPAPointsEx.
// Circle/circle collision time between two moving points with combined radius.
inline float GetCollisionTime(const Vec2& pa, const Vec2& pb,
                              const Vec2& va, const Vec2& vb,
                              float ra, float rb, bool& collision) {
    const Vec2 pab = pa - pb;
    const Vec2 vab = va - vb;
    const float a = Dot(vab, vab);
    const float b = 2.0f * Dot(pab, vab);
    const float c = Dot(pab, pab) - (ra + rb) * (ra + rb);

    const float disc = b * b - 4.0f * a * c;
    float t;
    if (disc < 0.0f || a == 0.0f) {
        t = (a != 0.0f) ? (-b / (2.0f * a)) : 0.0f;
        collision = false;
    } else {
        const float sq = std::sqrt(disc);
        const float t0 = (-b + sq) / (2.0f * a);
        const float t1 = (-b - sq) / (2.0f * a);
        if (t0 >= 0.0f && t1 >= 0.0f) {
            t = std::min(t0, t1);
        } else {
            t = std::max(t0, t1);
        }
        collision = t >= 0.0f;
    }
    if (t < 0.0f) {
        t = 0.0f;
    }
    return t;
}

// Project point onto segment [a,b], returning the clamped segment point and
// whether the projection landed within the segment.
inline Vec2 ProjectOn(const Vec2& point, const Vec2& a, const Vec2& b, bool& isOnSegment) {
    const Vec2 ab = b - a;
    const float lenSqr = Dot(ab, ab);
    if (lenSqr < kSmallNum) {
        isOnSegment = false;
        return a;
    }
    const float t = Dot(point - a, ab) / lenSqr;
    isOnSegment = (t >= 0.0f && t <= 1.0f);
    const float ct = std::clamp(t, 0.0f, 1.0f);
    return a + ab * ct;
}

// CPAPointsEx (the out-param variant EzEvade's GetClosestDistanceApproach uses):
// closest distance between a moving hero and a moving spell, with the CPA time
// clamped to >= 0 and each closest point returned.
inline float CpaPointsEx(const Vec2& p1, const Vec2& v1,
                         const Vec2& p2, const Vec2& v2,
                         Vec2& out1, Vec2& out2) {
    const float ctime = std::max(0.0f, CpaTime(p1, v1, p2, v2));
    out1 = p1 + v1 * ctime;
    out2 = p2 + v2 * ctime;
    return Dist(out1, out2);
}

} // namespace EvadeMath

// ============================================================================
// EzEvadePlugin
// ============================================================================
class EzEvadePlugin final : public IPlugin {
public:
    const char* GetName() const override { return "EzEvade"; }
    const char* GetInternalId() const override { return "core.ezevade"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return false; }

    void OnLoad() override {
        s_instance = this;
        CreateMenu();
        // Ensure the SDK detection layer is running; Tracker lazily initializes
        // on first AddOnDetectSkillshot, but we want it live even with no extra
        // handler, so poke it explicitly.
        SDK::Tracker::Initialize();
        SDK::Game::OnUpdate += &EzEvadePlugin::OnUpdateStatic;
        NightSharpDebug::Logf("[EzEvade] loaded");
    }

    void OnUnload() override {
        SDK::Game::OnUpdate -= &EzEvadePlugin::OnUpdateStatic;
        DestroyMenu();
        if (s_instance == this) {
            s_instance = nullptr;
        }
        NightSharpDebug::Logf("[EzEvade] unloaded");
    }

    void OnRender() override {
        if (!Enabled() || !DrawSpells() || !ImGui::GetCurrentContext()) {
            return;
        }
        for (const auto& skillshot : SDK::Tracker::DetectedSkillshots()) {
            if (skillshot) {
                skillshot->Draw(SpellColor(), SpellColor(), 2);
            }
        }
    }

    void OnMenu() override {
        if (!m_menu) {
            return;
        }
        m_menu->DrawImGui();
        ImGui::Separator();
        ImGui::Text("Tracked skillshots: %d",
                    static_cast<int>(SDK::Tracker::DetectedSkillshots().size()));
        ImGui::Text("Last dodge: %s", m_lastEvent);
    }

private:
    static inline EzEvadePlugin* s_instance = nullptr;

    Menu* m_menu = nullptr;
    MenuBool* m_enabledMenu = nullptr;
    MenuKeyBind* m_dodgeKeyMenu = nullptr;
    MenuBool* m_drawSpellsMenu = nullptr;
    MenuSlider* m_extraDelayMenu = nullptr;   // ExtraPingBuffer
    MenuSlider* m_extraDistMenu = nullptr;    // ExtraCPADistance
    MenuColor* m_spellColorMenu = nullptr;
    // Humanizer / gates
    MenuSlider* m_reactionTimeMenu = nullptr; // ms to wait after detection before dodging
    MenuSlider* m_minHitTimeMenu = nullptr;   // ignore spells landing later than this (ms)
    MenuSlider* m_dodgeIntervalMenu = nullptr;// min ms between move re-issues
    MenuSlider* m_dodgeHpMenu = nullptr;      // only dodge at/below this HP%
    char m_lastEvent[96] = "none";
    int m_lastDodgeTick = 0;

    // ── settings accessors ──────────────────────────────────────────────
    bool Enabled() const { return !m_enabledMenu || m_enabledMenu->Value; }
    bool DrawSpells() const { return !m_drawSpellsMenu || m_drawSpellsMenu->Value; }
    bool DodgeKeyActive() const { return !m_dodgeKeyMenu || m_dodgeKeyMenu->Active; }
    float ExtraDelay() const { return m_extraDelayMenu ? static_cast<float>(m_extraDelayMenu->Value) : 60.0f; }
    float ExtraDist() const { return m_extraDistMenu ? static_cast<float>(m_extraDistMenu->Value) : 15.0f; }
    int ReactionTime() const { return m_reactionTimeMenu ? m_reactionTimeMenu->Value : 0; }
    int MinHitTime() const { return m_minHitTimeMenu ? m_minHitTimeMenu->Value : 900; }
    int DodgeInterval() const { return m_dodgeIntervalMenu ? m_dodgeIntervalMenu->Value : 0; }
    float DodgeHp() const { return m_dodgeHpMenu ? static_cast<float>(m_dodgeHpMenu->Value) : 100.0f; }
    std::uint32_t SpellColor() const {
        return m_spellColorMenu ? m_spellColorMenu->GetImU32() : IM_COL32(255, 80, 80, 220);
    }

    // ── per-spell hit timing (EvadeHelper: spellHitTime) ──────────────────
    // Milliseconds from now until the spell reaches `pos`. Missiles use the
    // remaining travel distance at current speed; non-missile circles use the
    // fixed cast delay. Returns a large value when it will not reach us.
    static float SpellHitTime(const SDK::Skillshot& spell, const Vec2& pos) {
        const int now = SDK::Variables::TickCount();
        if (const auto* missile = dynamic_cast<const SDK::SkillshotMissile*>(&spell)) {
            const float speed = std::max(1.0f, static_cast<float>(spell.SData.MissileSpeed));
            const Vec2 head = missile->GetMissilePosition(0);
            const float dist = EvadeMath::Dist(head, pos);
            return (dist / speed) * 1000.0f;
        }
        // Non-missile: lands at StartTime + Delay.
        return static_cast<float>(spell.StartTime + spell.SData.Delay - now);
    }

    void SetLastEvent(const char* text) {
        strncpy_s(m_lastEvent, text ? text : "", _TRUNCATE);
    }

    // ── main tick ────────────────────────────────────────────────────────
    static void OnUpdateStatic() {
        if (s_instance) {
            s_instance->Tick();
        }
    }

    void Tick() {
        if (!Enabled() || !DodgeKeyActive()) {
            return;
        }
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) {
            return;
        }

        // Gate: can the hero even act on a dodge right now?
        //  - dashing: position is controlled by the dash, IssueMove is ignored
        //  - invulnerable: no need to dodge (Kayle R / Kindred R / Zhonya etc.)
        if (player.IsDashing() || player.IsInvulnerable()) {
            return;
        }

        // Gate: HP threshold — only dodge at/below the configured HP%.
        if (player.HealthPercent() > DodgeHp()) {
            return;
        }

        auto& skillshots = SDK::Tracker::DetectedSkillshots();
        if (skillshots.empty()) {
            return;
        }

        const Vec2 heroPos = player.ServerPosition().To2D();
        const float boundingRadius = player.BoundingRadius();

        // Only dodge if endangered by a spell that (a) will actually hit us in
        // the near future (MinHitTime) and (b) has been visible long enough to
        // react to (ReactionTime). Both are the EzEvade humanizer gates.
        if (!IsEndangered(player, heroPos, boundingRadius, skillshots)) {
            return;
        }

        // Humanizer: throttle move re-issues so we don't spam IssueMove every
        // frame (jitter / unnatural). DodgeInterval == 0 disables the throttle.
        const int now = SDK::Variables::TickCount();
        const int interval = DodgeInterval();
        if (interval > 0 && now - m_lastDodgeTick < interval) {
            return;
        }

        Vec2 best;
        if (FindBestPosition(player, heroPos, boundingRadius, skillshots, best)) {
            const float planeY = player.ServerPosition().y;
            CoreControl::IssueMove(Vec3::From2D(best, planeY), true);
            m_lastDodgeTick = now;
            SetLastEvent("dodging");
        } else {
            SetLastEvent("no safe position");
        }
    }

    // ── danger test ───────────────────────────────────────────────────────
    // Endangered only if the hero is inside a skillshot AND that spell passes
    // the humanizer gates: reaction-time elapsed since detection, and it lands
    // within MinHitTime (ignore far-future spells we can react to later).
    bool IsEndangered(const SDK::AIHeroClient& /*player*/, const Vec2& heroPos,
                      float boundingRadius,
                      const std::vector<std::shared_ptr<SDK::Skillshot>>& skillshots) const {
        const int now = SDK::Variables::TickCount();
        const int reaction = ReactionTime();
        const float minHit = static_cast<float>(MinHitTime());

        for (const auto& s : skillshots) {
            if (!s || !InSkillShot(*s, heroPos, boundingRadius)) {
                continue;
            }
            // Reaction-time gate: spell must have existed at least `reaction` ms.
            if (reaction > 0 && now - s->StartTime < reaction) {
                continue;
            }
            // Min-hit-time gate: skip spells that will not reach us soon.
            if (SpellHitTime(*s, heroPos) > minHit) {
                continue;
            }
            return true;
        }
        return false;
    }

    // Point-in-polygon test against the skillshot's own built polygon (Path),
    // sampled at pos plus an 8-point body ring to account for boundingRadius.
    // Works for every geometry the SDK builds (Line/Circle/Cone/Arc/Ring). For
    // Ring the polygon is a donut, so the safe center reads as OUTSIDE for free
    // (fixes Veigar cage / Darius outer-ring cases).
    static bool InPolygon(const SDK::Skillshot& spell, const Vec2& pos, float radius) {
        if (spell.Path.empty()) {
            return false;
        }
        if (SDK::Clipper::PointInPolygon(
                SDK::Clipper::IntPoint(pos.x, pos.y), spell.Path) == 1) {
            return true;
        }
        if (radius <= 0.0f) {
            return false;
        }
        constexpr float twoPi = 6.28318530717958647692f;
        for (int i = 0; i < 8; ++i) {
            const float a = static_cast<float>(i) * (twoPi / 8.0f);
            const Vec2 p(pos.x + radius * std::cos(a), pos.y + radius * std::sin(a));
            if (SDK::Clipper::PointInPolygon(
                    SDK::Clipper::IntPoint(p.x, p.y), spell.Path) == 1) {
                return true;
            }
        }
        return false;
    }

    // Port of Position.InSkillShot. Line/Circle use the analytic test (cleanly
    // incorporates the hero bounding radius); every other geometry falls back to
    // the SDK-built polygon so Cone/Arc/Ring are actually detected.
    static bool InSkillShot(const SDK::Skillshot& spell, const Vec2& pos, float radius) {
        const SDK::SpellType type = spell.SData.SpellType;
        const float spellRadius = static_cast<float>(spell.SData.Radius);

        if (SDK::IsLineSpellType(type)) {
            bool onSeg = false;
            const Vec2 proj = EvadeMath::ProjectOn(pos, spell.StartPosition, spell.EndPosition, onSeg);
            return onSeg && EvadeMath::Dist(proj, pos) <= spellRadius + radius;
        }
        if (SDK::IsCircleSpellType(type)) {
            return EvadeMath::Dist(pos, spell.EndPosition) <= spellRadius + radius;
        }
        return InPolygon(spell, pos, radius);
    }

    // ── closest distance of approach (EvadeHelper.GetClosestDistanceApproach) ──
    // Returns how close the hero's walk to `pos` comes to being hit. 0 == hit.
    float GetClosestDistanceApproach(const SDK::Skillshot& spell,
                                     const Vec2& pos, float speed, float delayMs,
                                     const Vec2& heroPos, float boundingRadius,
                                     float extraDist) const {
        const Vec2 walkDir = (pos - heroPos).Normalized();
        const SDK::SpellType type = spell.SData.SpellType;
        const float spellRadius = static_cast<float>(spell.SData.Radius);

        if (SDK::IsLineSpellType(type)) {
            const float missileSpeed = std::max(1.0f, static_cast<float>(spell.SData.MissileSpeed));

            // Current spell head position (account for the delay we are about to burn).
            Vec2 spellPos = spell.StartPosition;
            if (const auto* missile = dynamic_cast<const SDK::SkillshotMissile*>(&spell)) {
                spellPos = missile->GetMissilePosition(static_cast<int>(delayMs));
            }
            const Vec2 spellEnd = spell.EndPosition;
            const Vec2 spellVel = spell.Direction * missileSpeed;

            Vec2 cHero, cSpell;
            const float cpa = EvadeMath::CpaPointsEx(
                heroPos, walkDir * speed, spellPos, spellVel, cHero, cSpell);

            bool heroOn = false, spellOn = false;
            const Vec2 extendedPos = pos + walkDir * boundingRadius;
            EvadeMath::ProjectOn(cHero, heroPos, extendedPos, heroOn);
            EvadeMath::ProjectOn(cSpell, spellPos, spellEnd, spellOn);

            const float checkDist = boundingRadius + spellRadius + extraDist;
            if (spellOn && heroOn) {
                return std::max(0.0f, cpa - checkDist);
            }
            return checkDist;
        }

        if (SDK::IsCircleSpellType(type)) {
            // Circular: predict hero position at spell landing, measure ring gap.
            const int now = SDK::Variables::TickCount();
            const float hitTime = std::max(0.0f,
                static_cast<float>(spell.StartTime + spell.SData.Delay - now) - delayMs);
            const float walkRange = EvadeMath::Dist(heroPos, pos);
            const float predictedRange = speed * (hitTime / 1000.0f);
            const Vec2 tHeroPos = heroPos + walkDir * std::min(predictedRange, walkRange);
            return std::max(0.0f, EvadeMath::Dist(tHeroPos, spell.EndPosition) - (spellRadius + extraDist));
        }

        return 1.0f; // unsupported geometry: treat as non-threatening here
    }

    bool PredictSpellCollision(const SDK::Skillshot& spell, const Vec2& pos, float speed,
                               float delayMs, const Vec2& heroPos, float boundingRadius,
                               float extraDist) const {
        return GetClosestDistanceApproach(spell, pos, speed, delayMs, heroPos,
                                          boundingRadius, extraDist + 10.0f) == 0.0f;
    }

    // ── candidate scoring (EvadeHelper.CanHeroWalkToPos) ──────────────────
    struct PosInfo {
        Vec2 position;
        int dangerCount = 0;
        bool dangerous = false;
        bool wall = false;          // candidate is unwalkable or a wall blocks the path
        float distToMouse = 0.0f;
    };

    // World nav-grid wall test. The candidate (walk destination) must be
    // walkable AND the straight line from the hero to it must not cross a
    // wall — otherwise IssueMove would path around it and the CPA/geometry
    // math (which assumes a straight walk) becomes invalid. Ported from the
    // C# EvadeHelper wall checks using NightSharp's CoreNavGrid.
    bool WallBlocks(const Vec2& heroPos, const Vec2& pos, float planeY) const {
        const Vec3 dest = Vec3::From2D(pos, planeY);
        const Vec3 from = Vec3::From2D(heroPos, planeY);
        if (!CoreNavGrid::IsWalkable(dest)) {
            return true;
        }
        return CoreNavGrid::IsWallBetween(from, dest);
    }

    PosInfo ScorePosition(const Vec2& pos, float speed, float delayMs, float extraDist,
                          const Vec2& heroPos, float boundingRadius, float planeY,
                          const Vec2& mousePos,
                          const std::vector<std::shared_ptr<SDK::Skillshot>>& skillshots) const {
        PosInfo info;
        info.position = pos;
        info.distToMouse = EvadeMath::Dist(pos, mousePos);
        info.wall = WallBlocks(heroPos, pos, planeY);

        for (const auto& s : skillshots) {
            if (!s) {
                continue;
            }
            const int danger = std::max(1, s->SData.DangerValue);
            if (InSkillShot(*s, pos, boundingRadius - 8.0f) ||
                PredictSpellCollision(*s, pos, speed, delayMs, heroPos, boundingRadius, extraDist)) {
                info.dangerCount += danger;
            }
        }
        // A wall position is never a valid dodge target, even if spell-safe.
        info.dangerous = info.dangerCount > 0 || info.wall;
        return info;
    }

    // ── ring search (EvadeHelper.GetBestPosition) ─────────────────────────
    bool FindBestPosition(const SDK::AIHeroClient& player, const Vec2& heroPos,
                          float boundingRadius,
                          const std::vector<std::shared_ptr<SDK::Skillshot>>& skillshots,
                          Vec2& out) const {
        const float speed = std::max(50.0f, player.MoveSpeed());
        const float delayMs = ExtraDelay() + static_cast<float>(SDK::Game::Ping());
        const float extraDist = ExtraDist();
        const Vec2 mousePos = SDK::Game::CursorPos().To2D();
        const float planeY = player.ServerPosition().y;

        constexpr int kMaxPosToCheck = 100;
        constexpr int kPosRadius = 50;

        PosInfo best;
        bool haveBest = false;
        int posChecked = 0;
        int radiusIndex = 0;

        while (posChecked < kMaxPosToCheck) {
            radiusIndex++;
            const int curRadius = radiusIndex * (2 * kPosRadius);
            const int circleChecks = std::max(1,
                static_cast<int>(std::ceil((2.0 * 3.14159265358979323846 * curRadius) / (2.0 * kPosRadius))));

            for (int i = 1; i < circleChecks && posChecked < kMaxPosToCheck; ++i) {
                posChecked++;
                const double rad = (2.0 * 3.14159265358979323846 / (circleChecks - 1)) * i;
                const Vec2 candidate(
                    std::floor(heroPos.x + curRadius * static_cast<float>(std::cos(rad))),
                    std::floor(heroPos.y + curRadius * static_cast<float>(std::sin(rad))));

                const PosInfo info = ScorePosition(candidate, speed, delayMs, extraDist,
                                                   heroPos, boundingRadius, planeY, mousePos, skillshots);

                // Sort key: prefer non-dangerous, then lower danger count, then
                // closer to the mouse (matches the C# OrderBy chain).
                if (!haveBest || Better(info, best)) {
                    best = info;
                    haveBest = true;
                }
            }
        }

        if (haveBest && !best.dangerous) {
            out = best.position;
            return true;
        }
        return false;
    }

    static bool Better(const PosInfo& a, const PosInfo& b) {
        if (a.dangerous != b.dangerous) {
            return !a.dangerous;              // non-dangerous first
        }
        if (a.dangerCount != b.dangerCount) {
            return a.dangerCount < b.dangerCount;
        }
        return a.distToMouse < b.distToMouse; // closer to intended move
    }

    // ── menu ──────────────────────────────────────────────────────────────
    void CreateMenu() {
        DestroyMenu();
        m_menu = new Menu(GetInternalId(), GetName(), true);
        auto* settings = m_menu->AddSubMenu(new Menu("settings", "Settings"));
        m_enabledMenu = settings->Add(new MenuBool("enabled", "Enable Evade", true));
        m_dodgeKeyMenu = settings->Add(new MenuKeyBind(
            "dodgeKey", "Dodge Skillshots", VK_SPACE, KeyBindType::Toggle, true));
        m_drawSpellsMenu = settings->Add(new MenuBool("drawSpells", "Draw Skillshots", true));
        m_extraDelayMenu = settings->Add(new MenuSlider(
            "extraDelay", "Extra Ping Buffer (ms)", 60, 0, 200));
        m_extraDistMenu = settings->Add(new MenuSlider(
            "extraDist", "Extra CPA Distance", 15, 0, 150));

        auto* humanizer = m_menu->AddSubMenu(new Menu("humanizer", "Humanizer / Gates"));
        m_reactionTimeMenu = humanizer->Add(new MenuSlider(
            "reactionTime", "Reaction Time (ms)", 0, 0, 500));
        m_minHitTimeMenu = humanizer->Add(new MenuSlider(
            "minHitTime", "Max Hit Time to Dodge (ms)", 900, 100, 2000));
        m_dodgeIntervalMenu = humanizer->Add(new MenuSlider(
            "dodgeInterval", "Dodge Re-issue Interval (ms)", 0, 0, 500));
        m_dodgeHpMenu = humanizer->Add(new MenuSlider(
            "dodgeHp", "Only Dodge Below HP %", 100, 1, 100));

        m_spellColorMenu = settings->Add(new MenuColor(
            "spellColor", "Skillshot color", 1.0f, 0.31f, 0.31f, 0.86f));
        m_menu->Attach();
    }

    void DestroyMenu() {
        if (!m_menu) {
            return;
        }
        MenuManager::Instance().Remove(m_menu);
        delete m_menu;
        m_menu = nullptr;
        m_enabledMenu = nullptr;
        m_dodgeKeyMenu = nullptr;
        m_drawSpellsMenu = nullptr;
        m_extraDelayMenu = nullptr;
        m_extraDistMenu = nullptr;
        m_reactionTimeMenu = nullptr;
        m_minHitTimeMenu = nullptr;
        m_dodgeIntervalMenu = nullptr;
        m_dodgeHpMenu = nullptr;
        m_spellColorMenu = nullptr;
    }
};

} // namespace Plugins
