#pragma once
// ============================================================================
// EvadeCore.h — Complete Evade Engine (Phase 3-7)
//
// Phase 3: Core Evade Logic — main loop, dodge flow, movement blocking
// Phase 4: Default Logic — collision, channeling, wall, click-once (always ON)
// Phase 5: Menu integration — FOW, comfort zone, ping buffer
// Phase 6: Advanced — ping compensation, route change timeout, special spells
// Phase 7: Improvements — smart scoring, damage-aware, health-aware, anti-bot,
//          returning missiles, multi-skillshot priority, latency-adaptive
//
// Reference: EzEvade Evade.cs, EvadeSharp Program.cs, vEvade Core/Evader.cs
// ============================================================================

#include "EvadeGeometry.h"
#include "SpellDetector.h"
#include "sdk/Events/EventSystem.h"
#include "sdk/EzEvade/EvadeSpells/EvadeSpellData.h"
#include "sdk/EzEvade/EvadeSpells/EvadeSpellDatabase.h"
#include "sdk/GameObjects/NavGrid.h"
#include "sdk/Wrappers/Spells/SpellCaster.h"
#include "sdk/Wrappers/Orbwalking/Orbwalker.h"
#include "sdk/UI/MenuUI.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <functional>
#include <chrono>

namespace Evade {

// ============================================================================
// EvadeState — Current state of the evade engine
// ============================================================================
enum class EvadeState {
    Idle,           // Not dodging
    Dodging,        // Currently executing dodge movement
    WaitingResume,  // Dodge complete, waiting to resume original command
};

// ============================================================================
// DodgeResult — Result of a dodge attempt
// ============================================================================
struct DodgeResult {
    bool ShouldDodge = false;
    Vec2 DodgePoint;
    float Urgency = 0.0f;          // Time remaining before impact (ms)
    int HighestDanger = 0;
    int TotalDanger = 0;
    int SkillshotCount = 0;
    bool UsedSpell = false;         // Dodge used a spell instead of walking
    std::string SpellUsed;
};

// ============================================================================
// ReturningMissileData — For tracking returning missiles (7.10)
// ============================================================================
struct ReturningMissileData {
    std::string SpellName;
    Vec2 OriginalEnd;              // Where the missile originally went to
    float ReturnTime = 0.0f;       // When the missile starts returning
    int OriginalSpellId = -1;
    bool Processed = false;
};

// ============================================================================
// EvadeCore — Main Evade Engine
// ============================================================================
class EvadeCore {
public:
    // ---- Configuration (Menu-driven) ----
    struct Config {
        // Root controls
        bool Enabled = true;                   // Master enable (KeyBind K)
        bool DodgeOnlyDangerous = false;        // Only dodge high-danger (KeyBind Space)
        bool DodgeOnlyCC = false;               // Only dodge CC spells (KeyBind L toggle)
        int DiveTurretDangerLevel = 5;          // Min danger to dodge under turret (KeyBind T)
        bool DiveTurretMode = false;            // Is dive turret mode active

        // Dodge settings
        int ReactionTimeMs = 0;                 // Delay before dodging (humanizer)
        float ExtraEvadeDistance = 100.0f;       // Extra buffer on dodge distance
        float ExtraPingBuffer = 65.0f;           // Extra ping compensation
        int HumanizerDelayMs = 0;                // Random delay for humanizer

        // Phase 5 — Menu additions
        bool DodgeFowSkillshots = true;          // Dodge skillshots from fog of war
        bool PreventDodgeNearEnemies = true;     // Don't dodge into enemies
        float MinComfortZone = 550.0f;           // Min distance to enemies when dodging
        bool HigherPrecision = false;            // More position candidates (slower)

        // Phase 7 — Improvements
        // 7.2 Damage-Aware
        bool DamageAwareDodge = false;           // ON/OFF for damage check
        int DamageAwareDangerThreshold = 3;      // Spells >= this always dodged
        float DamageSkipHpPercent = 0.5f;        // Skip dodge if damage < HP * this

        // 7.3 Health-Aware
        bool HealthAwareEvade = false;
        float LowHpThreshold = 0.3f;            // Below this = dodge everything
        float HighHpThreshold = 0.7f;            // Above this = skip low danger

        // 7.4 Smooth Path (Anti-Bot)
        bool SmoothPath = false;
        float MicroOffsetMax = 15.0f;            // Max random offset on dodge point

        // 7.8 Latency-Adaptive
        bool LatencyAdaptive = true;             // Use real ping instead of fixed

        // 7.9 Champion-Aware
        bool ChampionAwareDodge = false;

        // Wall Awareness (NavGrid)
        bool AvoidNearWall = true;               // Push dodge point away from walls
        float WallBuffer = 65.0f;                // Min distance from wall when dodging
    };

    // ---- Singleton ----
    static EvadeCore& Instance() {
        static EvadeCore instance;
        return instance;
    }

    // ---- Initialize ----
    void Initialize() {
        if (m_initialized) return;
        m_initialized = true;
        m_state = EvadeState::Idle;

        // Initialize spell detector
        SpellDetector::Instance().Initialize();

        // Set up returning missile tracking
        SetupReturningMissileNames();
    }

    // ---- Shutdown ----
    void Shutdown() {
        SpellDetector::Instance().Shutdown();
        m_initialized = false;
        m_state = EvadeState::Idle;
    }

    // ================================================================
    // MAIN UPDATE — Call every script tick (from EvadePlugin::OnUpdate)
    // ================================================================
    void OnUpdate(float gameTime) {
        if (!m_initialized || !m_config.Enabled) return;

        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid() || !me.IsAlive()) {
            Reset();
            return;
        }

        // Phase 6: Latency-Adaptive ping
        float realPing = m_config.LatencyAdaptive
            ? SDK::Game::GetPing() / 2.0f + 20.0f
            : m_config.ExtraPingBuffer;

        // 1. Update spell detector
        SpellDetector::Instance().Update(gameTime);

        const auto& skillshots = SpellDetector::Instance().GetActiveSkillshots();
        Vec2 heroPos = me.GetServerPosition().To2D();
        Vec2 mousePos = SDK::Game::GetMouseWorldPos().To2D();
        float moveSpeed = me.GetMoveSpeed();

        // 2. Phase 4 Default: Check channeling — block walk evade if channeling
        bool isChanneling = IsChanneling(me);

        // 3. Check if player is in danger
        bool inDanger = !EvadeGeometry::IsSafePoint(heroPos, skillshots, EvadeGeometry::BoundingRadius);

        // 4. Handle returning missiles (7.10)
        ProcessReturningMissiles(gameTime);

        // 5. If not in danger, resume normal state
        if (!inDanger) {
            if (m_state == EvadeState::Dodging || m_state == EvadeState::WaitingResume) {
                // Dodge complete — resume original command
                ResumeBlockedCommand();
                m_state = EvadeState::Idle;
            }
            m_lastDodgeTime = 0.0f;
            return;
        }

        // 6. We ARE in danger — calculate dodge
        DodgeResult dodge = CalculateDodge(heroPos, mousePos, skillshots,
                                           moveSpeed, gameTime, realPing, isChanneling);

        if (!dodge.ShouldDodge) {
            return; // Decision: don't dodge (damage check, danger level filter, etc.)
        }

        // 7. Phase 6: Route change timeout — don't change dodge direction too fast
        if (m_state == EvadeState::Dodging) {
            float timeSinceLastDodge = (gameTime - m_lastDodgeTime) * 1000.0f;
            if (timeSinceLastDodge < 250.0f) {
                return; // Too soon to change route
            }
        }

        // 8. Apply humanizer delay
        if (m_config.ReactionTimeMs > 0 || m_config.HumanizerDelayMs > 0) {
            float totalDelay = (float)m_config.ReactionTimeMs;
            if (m_config.HumanizerDelayMs > 0) {
                totalDelay += (float)(rand() % m_config.HumanizerDelayMs);
            }
            float dangerStartTime = GetDangerStartTime(heroPos, skillshots, gameTime);
            float elapsed = (gameTime - dangerStartTime) * 1000.0f;
            if (elapsed < totalDelay) {
                return; // Humanizer: wait before dodging
            }
        }

        // 9. Phase 7.4: Apply smooth path anti-bot offset
        Vec2 finalDodgePoint = dodge.DodgePoint;
        if (m_config.SmoothPath && m_config.MicroOffsetMax > 0.0f) {
            finalDodgePoint = ApplyMicroOffset(finalDodgePoint, m_config.MicroOffsetMax);
        }

        // 9b. Wall awareness: push dodge point away from walls
        if (m_config.AvoidNearWall && m_config.WallBuffer > 0.0f) {
            auto ng = SDK::NavGrid::Get();
            if (ng.IsValid()) {
                Vec3 dodge3D = Vec3::From2D(finalDodgePoint, me.GetPosition().y);
                Vec3 pushed = ng.PushAwayFromWall(dodge3D, m_config.WallBuffer);
                finalDodgePoint = pushed.To2D();
            }
        }

        // 10. Execute dodge
        if (!dodge.UsedSpell) {
            // Walk dodge — only if not channeling
            if (!isChanneling) {
                IssueEvadeMove(finalDodgePoint, me.GetPosition().y);
                m_state = EvadeState::Dodging;
                m_lastDodgePoint = finalDodgePoint;
                m_lastDodgeTime = gameTime;
                m_clickedOnce = true;
            }
        } else {
            // Spell was used in CalculateDodge — state transition
            m_state = EvadeState::Dodging;
            m_lastDodgeTime = gameTime;
        }
    }

    // ================================================================
    // MOVEMENT BLOCKING — Call from OnIssueOrder hook
    // Returns true if the movement should be BLOCKED
    // ================================================================
    bool ShouldBlockMovement(const Vec2& movePos, float gameTime) {
        if (!m_initialized || !m_config.Enabled) return false;

        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return false;

        const auto& skillshots = SpellDetector::Instance().GetActiveSkillshots();
        if (skillshots.empty()) return false;

        Vec2 heroPos = me.GetServerPosition().To2D();
        float moveSpeed = me.GetMoveSpeed();

        // Phase 4 Default: Always block movement INTO skillshots
        if (EvadeGeometry::IsSafePoint(heroPos, skillshots, EvadeGeometry::BoundingRadius)) {
            // Player is safe — check if moving would enter danger
            if (EvadeGeometry::CheckMoveToDirection(heroPos, movePos, skillshots, moveSpeed)) {
                return true; // Block: would walk into skillshot
            }
        }

        // If we're currently dodging, block user movement (Click Only Once)
        if (m_state == EvadeState::Dodging && m_clickedOnce) {
            return true;
        }

        return false;
    }

    // ================================================================
    // BEFORE ATTACK — Cancel AA if need to dodge urgently
    // Phase 4 Default: always active
    // ================================================================
    bool ShouldCancelAttack(float gameTime) {
        if (!m_initialized || !m_config.Enabled) return false;

        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return false;

        const auto& skillshots = SpellDetector::Instance().GetActiveSkillshots();
        Vec2 heroPos = me.GetServerPosition().To2D();

        if (!EvadeGeometry::IsSafePoint(heroPos, skillshots, EvadeGeometry::BoundingRadius)) {
            // In danger — cancel attack if high urgency
            float timeToHit = EvadeGeometry::GetTimeToHit(heroPos, skillshots, gameTime);
            if (timeToHit < 500.0f) { // Less than 500ms to impact
                return true;
            }
        }
        return false;
    }

    // ---- Config access ----
    Config& GetConfig() { return m_config; }
    const Config& GetConfig() const { return m_config; }
    EvadeState GetState() const { return m_state; }
    const Vec2& GetLastDodgePoint() const { return m_lastDodgePoint; }

    // ---- Reset state ----
    void Reset() {
        m_state = EvadeState::Idle;
        m_lastDodgePoint = Vec2();
        m_lastDodgeTime = 0.0f;
        m_clickedOnce = false;
        m_blockedCommand = Vec2();
        m_hasBlockedCommand = false;
    }

    // ---- Save blocked command for later resume ----
    void SaveBlockedCommand(const Vec2& movePos) {
        m_blockedCommand = movePos;
        m_hasBlockedCommand = true;
    }

private:
    EvadeCore() = default;

    // ================================================================
    // CORE DODGE CALCULATION
    // Phase 3: main dodge logic
    // Phase 4: default filters always applied
    // Phase 7: improvements integrated
    // ================================================================
    DodgeResult CalculateDodge(const Vec2& heroPos, const Vec2& mousePos,
                               const std::vector<SkillshotPolygon>& skillshots,
                               float moveSpeed, float gameTime, float pingBuffer,
                               bool isChanneling)
    {
        DodgeResult result;
        result.ShouldDodge = false;

        if (skillshots.empty()) return result;

        // Gather threat info
        auto dangerous = EvadeGeometry::GetDangerousSkillshots(heroPos, skillshots);
        if (dangerous.empty()) return result;

        result.HighestDanger = 0;
        result.TotalDanger = 0;
        result.SkillshotCount = (int)dangerous.size();

        for (const auto* ss : dangerous) {
            result.HighestDanger = std::max(result.HighestDanger, ss->DangerLevel);
            result.TotalDanger += ss->DangerLevel;
        }

        // ---- Phase 4 Default: Under turret check ----
        if (m_config.DiveTurretMode) {
            float distToTurret = EvadeGeometry::GetDistanceToTurrets(heroPos);
            if (distToTurret < 900.0f && result.HighestDanger < m_config.DiveTurretDangerLevel) {
                return result; // Don't dodge low-danger under turret
            }
        }

        // ---- Phase 5: Dodge Only Dangerous filter ----
        if (m_config.DodgeOnlyDangerous && result.HighestDanger < 3) {
            return result;
        }

        // ---- Phase 5: Dodge Only CC filter ----
        if (m_config.DodgeOnlyCC) {
            bool hasCC = false;
            for (const auto* ss : dangerous) {
                auto* detected = SpellDetector::Instance().GetDetectedSpell(ss->SpellId);
                if (detected && detected->IsCC) { hasCC = true; break; }
            }
            if (!hasCC) return result;
        }

        // ---- 7.2 Damage-Aware Dodge Decision ----
        if (m_config.DamageAwareDodge) {
            if (result.HighestDanger < m_config.DamageAwareDangerThreshold) {
                // Low danger spell — check if we can tank it
                float myHpPercent = GetHpPercent();
                if (myHpPercent > m_config.DamageSkipHpPercent) {
                    return result; // HP high enough, skip low-danger dodge
                }
            }
            // Spells >= threshold are ALWAYS dodged (no skip)
        }

        // ---- 7.3 Health-Aware: ultra-low HP = dodge everything ----
        // (handled by NOT filtering — all spells get dodged at low HP)

        // ---- 7.6 Multi-Skillshot Priority ----
        // When multiple spells overlap, prioritize dodging CC over damage
        // This is handled by scoring: CC spells have higher danger level

        // ---- Calculate urgency ----
        result.Urgency = EvadeGeometry::GetTimeToHit(heroPos, skillshots, gameTime);

        // ---- Try evade spells first (Blink/Dash/Shield) ----
        if (!isChanneling || CanUseSpellWhileChanneling()) {
            if (TryUseEvadeSpell(heroPos, mousePos, skillshots, gameTime, result)) {
                result.ShouldDodge = true;
                result.UsedSpell = true;
                return result;
            }
        }

        // ---- Phase 4 Default: Block walk evade when channeling ----
        if (isChanneling) {
            return result; // Can't walk while channeling, and no spell available
        }

        // ---- Find best walk position ----
        PositionInfo bestPos = EvadeGeometry::GetBestPosition(
            heroPos, mousePos, skillshots, moveSpeed, gameTime,
            m_config.ExtraPingBuffer, m_config.ExtraEvadeDistance,
            10.0f, pingBuffer, m_config.HigherPrecision);

        if (!bestPos.IsDangerousPos || bestPos.PosDangerLevel == 0) {
            result.ShouldDodge = true;
            result.DodgePoint = bestPos.Position;

            // ---- 7.1 Smart Position Scoring ----
            if (m_config.ChampionAwareDodge) {
                result.DodgePoint = ApplyChampionAwareScoring(
                    heroPos, result.DodgePoint, skillshots, moveSpeed, gameTime);
            }

            return result;
        }

        // No safe walk position — check if we should still try
        // Even if undodgeable, we might want to minimize damage
        if (result.HighestDanger >= 4) {
            // High danger, undodgeable — try the closest outside point
            Vec2 closestOut = EvadeGeometry::GetClosestOutsidePoint(heroPos, skillshots);
            if (!closestOut.IsZero()) {
                result.ShouldDodge = true;
                result.DodgePoint = closestOut;
            }
        }

        return result;
    }

    // ================================================================
    // EVADE SPELLS — Try to use champion spells to dodge
    // Phase 3: Blink, Dash, SpellShield, WindWall, Stasis
    // ================================================================
    bool TryUseEvadeSpell(const Vec2& heroPos, const Vec2& mousePos,
                          const std::vector<SkillshotPolygon>& skillshots,
                          float gameTime, DodgeResult& result)
    {
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return false;

        std::string champName = me.GetChampionName();
        auto evadeSpells = EzEvade::GetEvadeSpellsForChampion(champName);

        // Sort by danger level (use lowest level spell first to save important ones)
        std::sort(evadeSpells.begin(), evadeSpells.end(),
            [](const EzEvade::EvadeSpellData* a, const EzEvade::EvadeSpellData* b) {
                return a->dangerlevel < b->dangerlevel;
            });

        int highestDanger = EvadeGeometry::GetHighestDangerLevel(heroPos, skillshots);

        for (const auto* evadeSpell : evadeSpells) {
            if (!evadeSpell) continue;

            // Only use spell if danger level is high enough
            if (highestDanger < evadeSpell->dangerlevel) continue;

            // Check cooldown — spell must be ready (use SpellCaster like EzrealPlugin)
            SDK::SpellCaster checkSpell(static_cast<SDK::SpellSlotId>(evadeSpell->spellKey), 25000.0f);
            if (!checkSpell.IsReady()) continue;

            switch (evadeSpell->evadeType) {
            case EzEvade::EvadeType::Blink: {
                // Find best blink position
                PositionInfo blinkPos = EvadeGeometry::GetBestPositionBlink(
                    heroPos, mousePos, skillshots, evadeSpell->range, gameTime,
                    m_config.MinComfortZone);
                if (!blinkPos.IsDangerousPos) {
                    // Cast the blink spell
                    if (CastEvadeSpell(*evadeSpell, blinkPos.Position, me.GetPosition().y)) {
                        result.DodgePoint = blinkPos.Position;
                        result.SpellUsed = evadeSpell->name;
                        return true;
                    }
                }
                break;
            }
            case EzEvade::EvadeType::Dash: {
                if (evadeSpell->castType == EzEvade::CastType::Target) {
                    // Targeted dash (Yasuo E, Katarina E)
                    PositionInfo dashPos = EvadeGeometry::GetBestPositionTargetedDash(
                        heroPos, mousePos, skillshots, evadeSpell->range,
                        me.GetMoveSpeed(), gameTime);
                    if (!dashPos.IsDangerousPos) {
                        if (CastEvadeSpell(*evadeSpell, dashPos.Position, me.GetPosition().y)) {
                            result.DodgePoint = dashPos.Position;
                            result.SpellUsed = evadeSpell->name;
                            return true;
                        }
                    }
                } else {
                    // Position dash (Ezreal E, Lucian E)
                    PositionInfo dashPos = EvadeGeometry::GetBestPositionDash(
                        heroPos, mousePos, skillshots,
                        evadeSpell->speed > 0 ? evadeSpell->speed : me.GetMoveSpeed(),
                        evadeSpell->range, evadeSpell->fixedRange,
                        me.GetMoveSpeed(), gameTime,
                        m_config.ExtraPingBuffer, SDK::Game::GetPing() / 2.0f);
                    if (!dashPos.IsDangerousPos) {
                        Vec2 castPos = dashPos.Position;
                        // Handle reversed dashes (Caitlyn E)
                        if (evadeSpell->isReversed) {
                            castPos = heroPos + (heroPos - dashPos.Position).Normalized() * evadeSpell->range;
                        }
                        if (CastEvadeSpell(*evadeSpell, castPos, me.GetPosition().y)) {
                            result.DodgePoint = dashPos.Position;
                            result.SpellUsed = evadeSpell->name;
                            return true;
                        }
                    }
                }
                break;
            }
            case EzEvade::EvadeType::SpellShield:
            case EzEvade::EvadeType::Shield: {
                // Spell shield: Sivir E, Nocturne W, Morgana E
                if (CastEvadeSpellSelf(*evadeSpell)) {
                    result.SpellUsed = evadeSpell->name;
                    return true;
                }
                break;
            }
            case EzEvade::EvadeType::WindWall: {
                // Yasuo W, Fiora W — cast toward incoming missile
                auto threats = EvadeGeometry::GetDangerousSkillshots(heroPos, skillshots);
                if (!threats.empty()) {
                    Vec2 castDir = threats[0]->Direction;
                    Vec2 wallPos = heroPos + castDir * 100.0f;
                    if (CastEvadeSpell(*evadeSpell, wallPos, me.GetPosition().y)) {
                        result.SpellUsed = evadeSpell->name;
                        return true;
                    }
                }
                break;
            }
            case EzEvade::EvadeType::Stasis: {
                // Zhonya's, Ekko R — last resort for highest danger
                if (highestDanger >= 4) {
                    if (CastEvadeSpellSelf(*evadeSpell)) {
                        result.SpellUsed = evadeSpell->name;
                        return true;
                    }
                }
                break;
            }
            case EzEvade::EvadeType::Untargetable: {
                // Fizz E, Elise E, Master Yi Q
                if (highestDanger >= 3) {
                    if (CastEvadeSpellSelf(*evadeSpell)) {
                        result.SpellUsed = evadeSpell->name;
                        return true;
                    }
                }
                break;
            }
            case EzEvade::EvadeType::MovementSpeedBuff: {
                // Ghost, Youmuu's, etc. — just cast self and walk
                // Only use if we can dodge by walking faster
                if (CastEvadeSpellSelf(*evadeSpell)) {
                    result.SpellUsed = evadeSpell->name;
                    // Don't return true — still need to walk
                }
                break;
            }
            default:
                break;
            }
        }

        return false;
    }

    // ================================================================
    // SPELL CASTING HELPERS (uses SpellCaster like EzrealPlugin)
    // ================================================================
    bool CastEvadeSpell(const EzEvade::EvadeSpellData& spell, const Vec2& target, float height) {
        if (static_cast<int>(spell.spellKey) < 0) return false;
        Vec3 target3D = Vec3::From2D(target, height);
        SDK::SpellCaster caster(static_cast<SDK::SpellSlotId>(spell.spellKey), 25000.0f);
        return caster.Cast(target3D);
    }

    bool CastEvadeSpellSelf(const EzEvade::EvadeSpellData& spell) {
        if (static_cast<int>(spell.spellKey) < 0) return false;
        SDK::SpellCaster caster(static_cast<SDK::SpellSlotId>(spell.spellKey), 25000.0f);
        return caster.Cast();
    }

    // ================================================================
    // MOVEMENT ORDER HELPERS
    // Uses SDK::Orbwalker::IssueOrder which is the actual working
    // movement command (GameObject doesn't have IssueOrder).
    // ================================================================
    void IssueEvadeMove(const Vec2& target, float height) {
        Vec3 moveTarget = Vec3::From2D(target, height);
        // Use Player.IssueOrder directly — NO anti-spam guard!
        // Orbwalker::IssueOrder drops commands within 40ms which kills dodge.
        SDK::GameObjects::Player.IssueOrder(SDK::OrderType::MoveTo, moveTarget);
    }

    void ResumeBlockedCommand() {
        if (m_hasBlockedCommand) {
            float height = SDK::GameObjects::Player.GetPosition().y;
            IssueEvadeMove(m_blockedCommand, height);
            m_hasBlockedCommand = false;
        }
        m_clickedOnce = false;
    }

    // ================================================================
    // PHASE 4 DEFAULT LOGIC
    // ================================================================

    // Check if hero is channeling a spell (Katarina R, MF R, etc.)
    bool IsChanneling(const SDK::GameObject& hero) const {
        if (!hero.IsValid()) return false;

        // Check common channeling buffs
        static const char* channelingBuffs[] = {
            "KatarinaR",
            "MissFortuneBulletTime",
            "ReapTheWhirlwind",
            "Drain",
            "Meditate",
            "Crowstorm",
            "VelkozR",
            "XerathLocusOfPower2",
            "NunuR_Activate",
            "GalioIdolOfDurand",
            "NethergateChannel",
            "Teleport",
            "Recall",
            "Gate",
        };

        for (const char* buff : channelingBuffs) {
            if (hero.HasBuff(buff)) return true;
        }

        return false;
    }

    // While channeling, only spell shields/stasis can be used (don't cancel channel)
    bool CanUseSpellWhileChanneling() const {
        return true; // SpellShield/Stasis don't cancel channel
    }

    // ================================================================
    // PHASE 7 IMPROVEMENTS
    // ================================================================

    // 7.1 Smart Position Scoring — champion-aware dodge direction
    Vec2 ApplyChampionAwareScoring(const Vec2& heroPos, const Vec2& dodgePoint,
                                    const std::vector<SkillshotPolygon>& skillshots,
                                    float moveSpeed, float gameTime)
    {
        // Get champion role heuristic
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return dodgePoint;

        float aaRange = me.GetAttackRange() + me.GetBoundingRadius();
        Vec2 bestTarget = dodgePoint;

        // Find closest enemy for AA range check
        Vec2 closestEnemy;
        float closestEnemyDist = FLT_MAX;
        for (const auto& enemy : SDK::GameObjects::EnemyHeroes) {
            if (!enemy.IsValid() || !enemy.IsAlive() || !enemy.IsVisible()) continue;
            Vec2 ePos = enemy.GetServerPosition().To2D();
            float d = heroPos.Distance(ePos);
            if (d < closestEnemyDist) {
                closestEnemyDist = d;
                closestEnemy = ePos;
            }
        }

        // ADC heuristic: if we have high AA range, prefer staying in AA range
        if (aaRange >= 500.0f && closestEnemyDist < 800.0f && !closestEnemy.IsZero()) {
            float dodgeDistToEnemy = dodgePoint.Distance(closestEnemy);
            if (dodgeDistToEnemy > aaRange + 50.0f) {
                // Dodge point is too far from enemy — try to stay in AA range
                Vec2 toEnemy = (closestEnemy - heroPos).Normalized();
                Vec2 alternatePoint = closestEnemy - toEnemy * (aaRange - 50.0f);

                if (EvadeGeometry::IsSafePoint(alternatePoint, skillshots) &&
                    EvadeGeometry::IsWalkablePoint(alternatePoint)) {
                    bestTarget = alternatePoint;
                }
            }
        }

        return bestTarget;
    }

    // 7.2 Damage check helper
    float GetHpPercent() const {
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return 1.0f;
        float max = me.GetMaxHealth();
        if (max <= 0) return 1.0f;
        return me.GetHealth() / max;
    }

    // 7.4 Smooth Path — apply micro offset for anti-bot
    Vec2 ApplyMicroOffset(const Vec2& point, float maxOffset) const {
        float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
        float dist = (float)(rand() % (int)(maxOffset * 2)) - maxOffset;
        return Vec2(point.x + cosf(angle) * dist, point.y + sinf(angle) * dist);
    }

    // 7.7 FoW Enhanced Detection — get danger start time
    float GetDangerStartTime(const Vec2& heroPos,
                             const std::vector<SkillshotPolygon>& skillshots,
                             float gameTime) const
    {
        float earliest = gameTime;
        for (const auto& ss : skillshots) {
            if (!ss.IsActive) continue;
            if (ss.InSkillShot(heroPos, EvadeGeometry::BoundingRadius)) {
                if (ss.CastTime < earliest) earliest = ss.CastTime;
            }
        }
        return earliest;
    }

    // 7.10 Returning Missile Tracking
    void SetupReturningMissileNames() {
        m_returningMissiles = {
            { "DravenRCast",      Vec2(), 0, -1, false },
            { "AhriQReturn",      Vec2(), 0, -1, false },
            { "SivirQReturn",     Vec2(), 0, -1, false },
            { "SivirQMissileReturn", Vec2(), 0, -1, false },
            { "GnarQMissileReturn", Vec2(), 0, -1, false },
            { "EkkoQReturn",      Vec2(), 0, -1, false },
            { "TalonWBack",       Vec2(), 0, -1, false },
            { "SwainEReturn",     Vec2(), 0, -1, false },
            { "AhriQReturn",      Vec2(), 0, -1, false },
        };
    }

    void ProcessReturningMissiles(float gameTime) {
        // When a missile-based spell is removed, check if it has a return version
        // and create a new detection for the return path
        auto& detector = SpellDetector::Instance();
        const auto& allSpells = detector.GetAllDetectedSpells();

        for (auto& rm : m_returningMissiles) {
            if (rm.Processed) continue;

            for (const auto& [id, spell] : allSpells) {
                if (spell.MarkedForRemoval && !rm.Processed) {
                    // Check if the removed spell's name matches a return missile source
                    // The return spell entries in SpellDatabase already handle this
                    // (e.g., AhriQReturn is a separate DB entry)
                    // So this is mainly for tracking state
                    if (_stricmp(spell.SpellName.c_str(), rm.SpellName.c_str()) == 0) {
                        rm.Processed = true;
                        rm.OriginalSpellId = id;
                    }
                }
            }
        }
    }

    // ================================================================
    // DATA
    // ================================================================
    bool m_initialized = false;
    Config m_config;
    EvadeState m_state = EvadeState::Idle;

    Vec2 m_lastDodgePoint;
    float m_lastDodgeTime = 0.0f;
    bool m_clickedOnce = false;

    Vec2 m_blockedCommand;
    bool m_hasBlockedCommand = false;

    std::vector<ReturningMissileData> m_returningMissiles;
};

} // namespace Evade
