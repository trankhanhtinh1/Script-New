#pragma once

#include "../Engine/Evader.h"
#include "../Engine/SkillshotDetector.h"

#include <algorithm>
#include <chrono>
#include <cfloat>
#include <memory>

namespace Plugins::KuroEvade::Benchmarking {

struct BenchmarkResult {
    int Iterations = 0;
    int PlansFound = 0;
    int LastCandidateCount = 0;
    int SkillshotDatabaseEntries = 0;
    int EvadeSpellDatabaseEntries = 0;
    int InvalidDatabaseEntries = 0;
    double TotalMicroseconds = 0.0;
    double AverageMicroseconds = 0.0;
    double MinimumMicroseconds = 0.0;
    double MaximumMicroseconds = 0.0;
};

// Native port of Benchmarking/Benchmark.cs. Mouse drag selects the start/end
// points, line/circle helpers inject deterministic test skillshots, and Run
// measures the real candidate planner used in game.
class Benchmark final {
public:
    void CaptureStart(const Vec2& point) {
        if (!point.IsZero()) {
            m_startPoint = point;
        }
    }

    void CaptureEnd(const Vec2& point) {
        if (!point.IsZero()) {
            m_endPoint = point;
        }
    }

    const Vec2& StartPoint() const { return m_startPoint; }
    const Vec2& EndPoint() const { return m_endPoint; }

    bool SpawnLine(SourceSkillshotDetector& detector) const {
        return Spawn(detector, false);
    }

    bool SpawnCircle(SourceSkillshotDetector& detector) const {
        return Spawn(detector, true);
    }

    bool StartLine(SourceSkillshotDetector& detector) {
        const bool spawned = SpawnLine(detector);
        if (spawned) {
            m_lineRunning = true;
            m_nextLineTick = SDK::Variables::TickCount() + 5000;
        }
        return spawned;
    }

    bool StartCircle(SourceSkillshotDetector& detector) {
        const bool spawned = SpawnCircle(detector);
        if (spawned) {
            m_circleRunning = true;
            m_nextCircleTick = SDK::Variables::TickCount() + 5000;
        }
        return spawned;
    }

    // Benchmark.cs recursively schedules the selected synthetic skillshot at
    // five-second intervals. Keeping the schedule here avoids callbacks that
    // could outlive a hot-unloaded plugin while preserving that behavior.
    void Update(SourceSkillshotDetector& detector) {
        const int now = SDK::Variables::TickCount();
        if (m_lineRunning && now >= m_nextLineTick) {
            SpawnLine(detector);
            m_nextLineTick = now + 5000;
        }
        if (m_circleRunning && now >= m_nextCircleTick) {
            SpawnCircle(detector);
            m_nextCircleTick = now + 5000;
        }
    }

    void Stop() {
        m_lineRunning = false;
        m_circleRunning = false;
        m_nextLineTick = 0;
        m_nextCircleTick = 0;
    }

    BenchmarkResult Run(const SDK::AIHeroClient& player,
                        const SourceSkillshotList& skillshots,
                        const EvadeSettings& settings,
                        int iterations = 100) const {
        BenchmarkResult result;
        if (!player.IsValid()) {
            return result;
        }

        result.Iterations = std::clamp(iterations, 1, 2000);
        const auto& spellEntries = Database::SpellDatabase::Spells();
        const auto& evadeEntries = Database::EvadeSpellDatabase::Spells();
        result.SkillshotDatabaseEntries =
            static_cast<int>(spellEntries.size());
        result.EvadeSpellDatabaseEntries =
            static_cast<int>(evadeEntries.size());
        for (const Database::SpellData& spell : spellEntries) {
            if (spell.CharacterName.empty() || spell.SpellName.empty() ||
                spell.Runtime.ChampionName != spell.CharacterName ||
                spell.Runtime.SpellName != spell.SpellName ||
                spell.Runtime.Range < 0 || spell.Runtime.Radius < 0) {
                ++result.InvalidDatabaseEntries;
            }
        }
        for (const Database::EvadeSpellData& spell : evadeEntries) {
            if (spell.ChampionName.empty() || spell.Name.empty()) {
                ++result.InvalidDatabaseEntries;
            }
        }
        result.MinimumMicroseconds = DBL_MAX;
        const Vec2 desired = !m_endPoint.IsZero()
            ? m_endPoint
            : SDK::Game::CursorPos().To2D();

        using Clock = std::chrono::steady_clock;
        for (int index = 0; index < result.Iterations; ++index) {
            const auto start = Clock::now();
            const SourceEvadePlan plan = SourceEvader::FindBestPosition(
                player, desired, skillshots, settings, true);
            const auto end = Clock::now();
            const double elapsed =
                std::chrono::duration<double, std::micro>(end - start).count();
            result.TotalMicroseconds += elapsed;
            result.MinimumMicroseconds =
                std::min(result.MinimumMicroseconds, elapsed);
            result.MaximumMicroseconds =
                std::max(result.MaximumMicroseconds, elapsed);
            result.PlansFound += plan.Found ? 1 : 0;
            result.LastCandidateCount =
                static_cast<int>(plan.Candidates.size());
        }
        result.AverageMicroseconds =
            result.TotalMicroseconds / static_cast<double>(result.Iterations);
        if (result.MinimumMicroseconds == DBL_MAX) {
            result.MinimumMicroseconds = 0.0;
        }
        return result;
    }

private:
    Vec2 m_startPoint;
    Vec2 m_endPoint;
    bool m_lineRunning = false;
    bool m_circleRunning = false;
    int m_nextLineTick = 0;
    int m_nextCircleTick = 0;

    bool Spawn(SourceSkillshotDetector& detector, bool circle) const {
        const SDK::AIHeroClient player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            return false;
        }

        Vec2 start = m_startPoint;
        Vec2 end = m_endPoint;
        if (start.IsZero()) {
            start = SDK::Game::CursorPos().To2D();
        }
        if (end.IsZero()) {
            end = player.ServerPosition().To2D();
        }
        if (start.DistanceSqr(end) < 100.0f * 100.0f) {
            start = end + Vec2(800.0f, 0.0f);
        }

        Database::SpellData data;
        data.CharacterName = "Benchmark";
        data.DisplayName = circle ? "Test Circle Skillshot" : "Test Line Skillshot";
        data.SpellName = circle ? "TestCircleSkillShot" : "TestLineSkillShot";
        data.DangerValue = 3;
        data.Delay = 250;
        data.Range = std::max(1.0f, start.Distance(end));
        data.Radius = circle ? 200.0f : 80.0f;
        data.MissileSpeed = circle ? 0.0f : 1000.0f;
        data.Type = circle
            ? Database::SkillShotType::SkillshotCircle
            : Database::SkillShotType::SkillshotLine;
        data.Finalize();

        std::shared_ptr<SDK::Skillshot> native = circle
            ? std::static_pointer_cast<SDK::Skillshot>(
                std::make_shared<SDK::SkillshotCircle>(data.Runtime))
            : std::static_pointer_cast<SDK::Skillshot>(
                std::make_shared<SDK::SkillshotLine>(data.Runtime));
        native->DetectionType = SDK::SkillshotDetectionType::ProcessSpell;
        native->Caster = player;
        native->StartPosition = start;
        native->EndPosition = end;
        native->Direction = (end - start).Normalized();
        native->StartTime = SDK::Variables::TickCount();
        SpecialSpells::RefreshSkillshotGeometry(*native);
        detector.AddSimulatedSkillshot(native, data);
        return true;
    }
};

} // namespace Plugins::KuroEvade::Benchmarking
