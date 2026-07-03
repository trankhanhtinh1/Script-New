#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreRuntime.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../imgui/imgui.h"

#include <cstdio>
#include <string>

namespace Plugins {

class PredictionDebugPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Prediction Debug"; }
    const char* GetInternalId() const override { return "utility.prediction_debug"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Utility; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override {
        s_instance = this;
        m_loadTime = GetTickCount64();
        m_lastCalcMs = GetTickCount64();
    }

    void OnUnload() override {
        if (s_instance == this) {
            s_instance = nullptr;
        }
        NightSharpDebug::Logf("[PredictionDebug] unloaded");
    }

    void OnRender() override {
        if (!m_enabled || !ImGui::GetCurrentContext()) return;

        if (GetTickCount64() - m_loadTime < 5000) {
            ImGui::GetForegroundDrawList()->AddText({10, 10}, 0xFFFFFFFF,
                "PredictionDebug: waiting for game to stabilize...");
            return;
        }

        if (!m_gameReady && !CoreRuntime::IsReady()) return;

        const ULONGLONG now = GetTickCount64();

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) return;

        m_currentFrom = player.Position();

        const auto target = FindTarget(m_currentFrom);
        if (!target.IsValid()) return;

        if (now - m_lastCalcMs >= 50 || target.NetworkId() != m_lastTargetNetId) {
            if (target.NetworkId() != m_lastTargetNetId) {
                m_lastTargetNetId = target.NetworkId();
                m_gameReady = false;
                LoadSpellData(target);
                m_gameReady = true;
            }
            m_lastCalcMs = now;
            m_output = SDK::Prediction::GetPrediction(BuildInput(target));
        }

        DrawAll(target);
    }

    void OnMenu() override {
        ImGui::Checkbox("Enable prediction debug", &m_enabled);

        ImGui::Separator();
        ImGui::Text("Spell Source");

        const char* slots[] = { "Manual", "Q", "W", "E", "R" };
        ImGui::Combo("Slot##spellSlot", &m_spellSlotOverride, slots, 5);

        if (m_spellSlotOverride == 0) {
            float speedVal = m_manualSpeed;
            if (speedVal >= FLT_MAX / 2.0f) speedVal = 0.0f;

            ImGui::SliderFloat("Delay (ms)", &m_manualDelay, 0.0f, 3000.0f, "%.0f");
            ImGui::SliderFloat("Speed", &speedVal, 0.0f, 5000.0f, "%.0f");
            ImGui::SliderFloat("Width", &m_manualWidth, 0.0f, 500.0f, "%.0f");
            ImGui::SliderFloat("Range", &m_manualRange, 0.0f, 5000.0f, "%.0f");

            m_manualSpeed = speedVal <= 0.0f ? FLT_MAX : speedVal;

            const char* types[] = { "Line", "Circle", "Cone" };
            ImGui::Combo("Type##skillType", &m_manualType, types, 3);
        } else if (!m_lastEntryName.empty()) {
            ImGui::Text("Spell: %s", m_lastEntryName.c_str());
        }

        ImGui::Separator();
        ImGui::Text("Display");
        ImGui::SliderFloat("Max target range", &m_maxRange, 500.0f, 5000.0f, "%.0f");
        ImGui::Checkbox("Show unit position", &m_drawUnitPos);
        ImGui::Checkbox("Show predicted position", &m_drawPredictedPos);
        ImGui::Checkbox("Show cast position", &m_drawCastPos);
        ImGui::Checkbox("Show skillshot shape", &m_drawSkillshot);
        ImGui::Checkbox("Show path", &m_drawPath);
        ImGui::Checkbox("Show collision", &m_drawCollision);
        ImGui::Checkbox("Show hitchance text", &m_drawHitChance);

        ImGui::Separator();
        ImGui::Text("Current values: Range=%.0f  Delay=%.0fms  Speed=%s  Width=%.0f",
            m_currentRange,
            m_currentDelay * 1000.0f,
            (m_currentSpeed >= FLT_MAX / 2.0f) ? "Instant" : std::to_string(m_currentSpeed).c_str(),
            m_currentRadius);

        if (m_output.Hitchance >= SDK::HitChance::Low) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "HC=%s  Pos=%s  Aoe=%d",
                HitChanceName(m_output.Hitchance),
                m_output.IsValid() ? "valid" : "invalid",
                m_output.AoeTargetsHitCount());
            ImGui::Text("Prediction: %s", buf);
        }
    }

private:
    static inline PredictionDebugPlugin* s_instance = nullptr;

    // Config
    bool m_enabled = true;
    float m_maxRange = 2000.0f;
    int m_spellSlotOverride = 0;

    // Manual parameters
    float m_manualDelay = 250.0f;
    float m_manualSpeed = FLT_MAX;
    float m_manualWidth = 100.0f;
    float m_manualRange = 1000.0f;
    int m_manualType = 0;

    // Current effective parameters
    float m_currentDelay = 0.25f;
    float m_currentSpeed = FLT_MAX;
    float m_currentRadius = 100.0f;
    float m_currentRange = 1000.0f;
    float m_currentAngleDeg = 40.0f;
    SDK::SkillshotType m_currentType = SDK::SkillshotType::SkillshotLine;
    SDK::CollisionableObjects m_currentCollisionMask =
        SDK::CollisionableObjects::Minions | SDK::CollisionableObjects::Heroes;
    std::string m_lastEntryName;

    // Display toggles
    bool m_drawUnitPos = true;
    bool m_drawPredictedPos = true;
    bool m_drawCastPos = true;
    bool m_drawSkillshot = true;
    bool m_drawPath = true;
    bool m_drawCollision = true;
    bool m_drawHitChance = true;

    // Cached state
    ULONGLONG m_loadTime = 0;
    ULONGLONG m_lastCalcMs = 0;
    int m_lastTargetNetId = 0;
    Vec3 m_currentFrom = {};
    SDK::PredictionOutput m_output = {};
    bool m_gameReady = false;

    // Colors
    static constexpr std::uint32_t kColorUnit = 0xAA66FF66u;
    static constexpr std::uint32_t kColorPredicted = 0xAA42FFE8u;
    static constexpr std::uint32_t kColorCast = 0xAAFF5A2Au;
    static constexpr std::uint32_t kColorSkillshot = 0xAAFFFFFFu;
    static constexpr std::uint32_t kColorPath = 0xFFFFFF00u;
    static constexpr std::uint32_t kColorCollision = 0xAAFF4444u;

    SDK::AIHeroClient FindTarget(const Vec3& from) {
        uintptr_t buf[256] = {};
        const int count = Core::ObjectManager::EnumerateHeroes(buf, 256);
        if (count <= 0) return {};

        SDK::AIHeroClient best;
        float bestDistSq = m_maxRange * m_maxRange;

        for (int i = 0; i < count; ++i) {
            const uintptr_t addr = buf[i];
            if (!Globals::IsValidPtr(addr)) continue;

            SDK::AIHeroClient unit(addr);
            if (!unit.IsValid() || unit.IsDead() || !unit.IsVisible()) continue;
            if (!unit.IsTargetable()) continue;
            if (!unit.IsEnemy()) continue;

            const float distSq = unit.Position().DistanceSqr2D(from);
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                best = unit;
            }
        }
        return best;
    }

    void LoadSpellData(const SDK::AIHeroClient& unit) {
        if (m_spellSlotOverride <= 0 || m_spellSlotOverride > 4) {
            ApplyManualParams();
            return;
        }

        const SDK::SpellSlot slot = static_cast<SDK::SpellSlot>(m_spellSlotOverride - 1);
        const std::string champName = unit.CharacterName();

        for (const auto& entry : SDK::SpellDatabase::Spells()) {
            if (entry.Slot != slot) continue;
            if (!CIEquals(champName, entry.ChampionName)) continue;

            m_currentDelay = entry.Delay / 1000.0f;
            m_currentSpeed = entry.MissileSpeed > 0 ? static_cast<float>(entry.MissileSpeed) : FLT_MAX;
            m_currentRadius = entry.Width > 0 ? static_cast<float>(entry.Width)
                : (entry.Radius > 0 ? static_cast<float>(entry.Radius) : 100.0f);
            m_currentRange = entry.Range < std::numeric_limits<int>::max() / 2
                ? static_cast<float>(entry.Range) : FLT_MAX;
            m_currentAngleDeg = static_cast<float>(entry.Angle);

            switch (entry.SpellType) {
            case SDK::SpellType::SkillshotLine:
            case SDK::SpellType::SkillshotMissileLine:
                m_currentType = SDK::SkillshotType::SkillshotLine;
                break;
            case SDK::SpellType::SkillshotCircle:
            case SDK::SpellType::SkillshotMissileCircle:
                m_currentType = SDK::SkillshotType::SkillshotCircle;
                break;
            case SDK::SpellType::SkillshotCone:
            case SDK::SpellType::SkillshotMissileCone:
                m_currentType = SDK::SkillshotType::SkillshotCone;
                break;
            default:
                m_currentType = SDK::SkillshotType::SkillshotLine;
                break;
            }

            SDK::CollisionableObjects mask = {};
            for (const auto& obj : entry.CollisionObjects) {
                mask |= obj;
            }
            m_currentCollisionMask = mask;

            m_lastEntryName = entry.SpellName;
            return;
        }

        m_lastEntryName.clear();
        ApplyManualParams();
    }

    void ApplyManualParams() {
        m_currentDelay = m_manualDelay / 1000.0f;
        m_currentSpeed = m_manualSpeed;
        m_currentRadius = m_manualWidth;
        m_currentRange = m_manualRange;
        m_currentType = static_cast<SDK::SkillshotType>(m_manualType);
        m_currentAngleDeg = 40.0f;
        m_currentCollisionMask = SDK::CollisionableObjects::Minions | SDK::CollisionableObjects::Heroes;
    }

    static bool CIEquals(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
            }
        }
        return true;
    }

    SDK::PredictionInput BuildInput(const SDK::AIBaseClient& unit) {
        SDK::PredictionInput input;
        input.Unit = unit;
        input.Delay = m_currentDelay;
        input.Speed = m_currentSpeed;
        input.Radius = m_currentRadius;
        input.Range = m_currentRange;
        input.Type = m_currentType;
        input.Collision = m_drawCollision;
        input.CollisionObjects = m_currentCollisionMask;
        input.AoE = false;
        input.From = m_currentFrom;
        input.RangeCheckFrom = m_currentFrom;
        return input;
    }

    void DrawAll(const SDK::AIHeroClient& target) {
        if (m_drawUnitPos) {
            DrawCircleWorld(target.Position(), target.BoundingRadius(), kColorUnit, 2.0f, true);
        }

        const bool valid = m_output.Hitchance > SDK::HitChance::Collision;

        if (valid && m_drawPredictedPos) {
            DrawCircleWorld(m_output.UnitPosition, target.BoundingRadius(), kColorPredicted, 2.0f, true);
        }

        if (valid && m_drawCastPos) {
            DrawCircleWorld(m_output.CastPosition, 30.0f, kColorCast, 2.0f, true);
            DrawLineWorld(m_currentFrom, m_output.CastPosition, kColorCast, 1.0f);
        }

        if (valid && m_drawSkillshot && !m_output.CastPosition.IsZero()) {
            DrawSkillshotShape(m_currentFrom, m_output.CastPosition);
        }

        if (m_drawPath) {
            DrawPathWorld(target);
        }

        if (m_drawCollision && !m_output.CollisionObjects.empty()) {
            DrawCollisions(m_output.CollisionObjects);
        }

        if (m_drawHitChance) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "HC: %s", HitChanceName(m_output.Hitchance));
            DrawTextWorld(target.Position(), HitChanceColor(m_output.Hitchance), buf);
        }
    }

    static const char* HitChanceName(SDK::HitChance hc) {
        switch (hc) {
        case SDK::HitChance::Immobile: return "Immobile";
        case SDK::HitChance::Dashing: return "Dashing";
        case SDK::HitChance::VeryHigh: return "VeryHigh";
        case SDK::HitChance::High: return "High";
        case SDK::HitChance::Medium: return "Medium";
        case SDK::HitChance::Low: return "Low";
        case SDK::HitChance::Impossible: return "Impossible";
        case SDK::HitChance::OutOfRange: return "OutOfRange";
        case SDK::HitChance::Collision: return "Collision";
        default: return "None";
        }
    }

    static std::uint32_t HitChanceColor(SDK::HitChance hc) {
        switch (hc) {
        case SDK::HitChance::Immobile:
        case SDK::HitChance::Dashing:
        case SDK::HitChance::VeryHigh:
        case SDK::HitChance::High:
            return 0xFF66FF66u;
        case SDK::HitChance::Medium:
            return 0xFFFFFF00u;
        default:
            return 0xFFFF4444u;
        }
    }

    void DrawCircleWorld(const Vec3& center, float radius, std::uint32_t color, float thickness, bool filled) {
        Vec2 screen;
        if (!SDK::Drawing::WorldToScreen(center, screen)) return;

        Vec2 edge;
        if (!SDK::Drawing::WorldToScreen(Vec3(center.x + radius, center.y, center.z), edge)) return;

        const float screenRadius = std::max(1.0f, screen.Distance(edge));
        if (!std::isfinite(screenRadius) || screenRadius <= 0.0f || screenRadius >= 10000.0f) return;

        if (filled) {
            const ImU32 fillColor = (static_cast<ImU32>(color) & 0x00FFFFFF) | 0x40000000u;
            ImGui::GetForegroundDrawList()->AddCircleFilled(
                ImVec2(screen.x, screen.y), screenRadius, fillColor, 64);
        }
        SDK::Drawing::DrawCircle(screen, screenRadius, thickness, color, 64);
    }

    void DrawLineWorld(const Vec3& from, const Vec3& to, std::uint32_t color, float thickness) {
        Vec2 fromScreen, toScreen;
        if (!SDK::Drawing::WorldToScreen(from, fromScreen)) return;
        if (!SDK::Drawing::WorldToScreen(to, toScreen)) return;
        SDK::Drawing::DrawLine(fromScreen, toScreen, thickness, color);
    }

    void DrawSkillshotShape(const Vec3& from, const Vec3& to) {
        if (from.IsZero() || to.IsZero()) return;

        switch (m_currentType) {
        case SDK::SkillshotType::SkillshotLine:
            DrawLineSkillshot(from, to);
            break;
        case SDK::SkillshotType::SkillshotCircle:
            DrawCircleWorld(to, m_currentRadius, kColorSkillshot, 1.5f, false);
            break;
        case SDK::SkillshotType::SkillshotCone:
            DrawConeSkillshot(from, to);
            break;
        }
    }

    void DrawLineSkillshot(const Vec3& from, const Vec3& to) {
        const Vec2 from2D = from.To2D();
        const Vec2 to2D = to.To2D();
        const Vec2 dir = (to2D - from2D).Normalized();
        const Vec2 perp = dir.Perpendicular();

        const float halfWidth = m_currentRadius * 0.5f;
        const Vec2 left = to2D + perp * halfWidth;
        const Vec2 right = to2D - perp * halfWidth;
        const Vec2 leftFrom = from2D + perp * halfWidth;
        const Vec2 rightFrom = from2D - perp * halfWidth;

        DrawLineWorld(Vec3::From2D(leftFrom), Vec3::From2D(left), kColorSkillshot, 1.0f);
        DrawLineWorld(Vec3::From2D(rightFrom), Vec3::From2D(right), kColorSkillshot, 1.0f);
    }

    void DrawConeSkillshot(const Vec3& from, const Vec3& to) {
        const Vec2 from2D = from.To2D();
        const Vec2 to2D = to.To2D();
        const Vec2 dir = (to2D - from2D).Normalized();

        const float halfAngleRad = m_currentAngleDeg * 0.5f * (3.14159265f / 180.0f);
        const Vec2 leftDir = dir.Rotated(-halfAngleRad);
        const Vec2 rightDir = dir.Rotated(halfAngleRad);

        const float range = m_currentRange;
        const Vec2 leftEnd = from2D + leftDir * range;
        const Vec2 rightEnd = from2D + rightDir * range;

        DrawLineWorld(from, Vec3::From2D(leftEnd), kColorSkillshot, 1.0f);
        DrawLineWorld(from, Vec3::From2D(rightEnd), kColorSkillshot, 1.0f);

        const int segments = 24;
        Vec3 arcPoints[segments + 1] = {};
        arcPoints[0] = Vec3::From2D(leftEnd);
        for (int i = 1; i <= segments; ++i) {
            const float t = static_cast<float>(i) / segments;
            const Vec2 cur(
                leftEnd.x + (rightEnd.x - leftEnd.x) * t,
                leftEnd.y + (rightEnd.y - leftEnd.y) * t);
            arcPoints[i] = Vec3::From2D(cur);
        }

        SDK::Drawing::DrawPolylineWorld(
            arcPoints,
            segments + 1,
            1.0f,
            kColorSkillshot);
    }

    void DrawPathWorld(const SDK::AIBaseClient& unit) {
        const auto waypoints = unit.GetWaypoints();
        if (waypoints.size() < 2) return;

        SDK::Drawing::DrawPolylineWorld(
            waypoints.data(),
            static_cast<int>(waypoints.size()),
            1.0f,
            kColorPath);
        for (const auto& wp : waypoints) {
            DrawCircleWorld(wp, 8.0f, kColorPath, 1.0f, true);
        }
    }

    void DrawCollisions(const std::vector<SDK::GameObject>& objects) {
        for (const auto& obj : objects) {
            if (!obj.IsValid()) continue;
            DrawCircleWorld(obj.Position(), obj.BoundingRadius(), kColorCollision, 1.5f, true);
        }
    }

    void DrawTextWorld(const Vec3& worldPos, std::uint32_t color, const char* text) {
        Vec2 screen;
        if (!SDK::Drawing::WorldToScreen(worldPos, screen)) return;
        SDK::Drawing::DrawText(screen.x, screen.y - 16.0f, color, text);
    }
};


} // namespace Plugins
