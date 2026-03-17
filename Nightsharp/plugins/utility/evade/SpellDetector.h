#pragma once
// ============================================================================
// SpellDetector.h — Complete Spell Detection Engine (Phase 2)
// Reference: EzEvade SpellDetector.cs, EvadeSharp SkillshotDetector.cs,
//            vEvade SpellDetector.cs, Chimera SpellDetector
// ============================================================================

#include "EvadeGeometry.h"
#include "sdk/Events/EventSystem.h"
#include "sdk/EzEvade/Spells/SpellDatabase.h"
#include "sdk/EzEvade/Spells/SpellData.h"
#include "sdk/EzEvade/EvadeSpells/EvadeSpellData.h"
#include "sdk/EzEvade/EvadeSpells/EvadeSpellDatabase.h"
#include "sdk/Wrappers/Spells/SpellDatabase.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <mutex>
#include <functional>

namespace Evade {

// ============================================================================
// DetectedSpellData — Internal tracking for a detected skillshot
// ============================================================================
struct DetectedSpellData {
    int SpellId = -1;                      // Unique ID for this detection
    const EzEvade::SpellData* DatabaseEntry = nullptr; // Pointer to SpellDatabase entry

    // Caster info
    int CasterNetId = 0;
    std::string CasterName;
    std::string SpellName;

    // Geometry
    Vec2 StartPos;
    Vec2 EndPos;
    Vec2 Direction;
    Vec2 CurrentMissilePos;
    float Speed = 0.0f;
    float Width = 0.0f;                    // Radius
    float Range = 0.0f;
    float ConeAngle = 0.0f;               // For cone spells
    float RingInnerRadius = 0.0f;          // For ring spells
    float ArcAngle = 0.0f;                 // For arc spells

    // Timing
    float CastTime = 0.0f;                // Game time when cast started
    float CastDelay = 0.0f;               // Delay before spell activates (seconds)
    float EndTime = 0.0f;                  // Game time when spell expires

    // State
    SpellType Type = SpellType::Line;
    int DangerLevel = 1;
    bool IsProcessed = false;              // Has polygon been created
    bool IsMissile = false;                // Has a moving missile
    bool MissileConfirmed = false;         // Missile object has been seen
    bool IsCC = false;
    bool HasCollision = false;             // Can be blocked by minions/heroes
    bool IsWindWallable = false;           // Blocked by Yasuo W
    bool IsTrap = false;                   // Is a ground trap (Teemo R, Caitlyn W)
    bool IsToggle = false;                 // Is a toggle/persistent zone
    bool MarkedForRemoval = false;

    // Missile tracking
    int MissileNetId = 0;                  // NetId of the missile object
    Vec3 MissileObj3DPos;                  // 3D position of missile

    // Convert EzEvade::SkillshotType to our SpellType
    static SpellType ConvertType(EzEvade::SkillshotType t) {
        switch (t) {
        case EzEvade::SkillshotType::Line:        return SpellType::Line;
        case EzEvade::SkillshotType::MissileLine:  return SpellType::Line;
        case EzEvade::SkillshotType::Circle:       return SpellType::Circle;
        case EzEvade::SkillshotType::Cone:         return SpellType::Cone;
        case EzEvade::SkillshotType::Ring:         return SpellType::Ring;
        case EzEvade::SkillshotType::Arc:          return SpellType::Arc;
        case EzEvade::SkillshotType::MissileArc:   return SpellType::Arc;
        default:                                    return SpellType::Line;
        }
    }

    // Build a SkillshotPolygon from this detection data
    SkillshotPolygon ToSkillshotPolygon() const {
        SkillshotPolygon poly;
        poly.Start = StartPos;
        poly.End = EndPos;
        poly.Direction = Direction;
        poly.MissilePosition = IsMissile ? CurrentMissilePos : StartPos;
        poly.Speed = Speed;
        poly.Width = Width;
        poly.Range = Range;
        poly.CastTime = CastTime;
        poly.EndTime = EndTime;
        poly.ExtraDelay = CastDelay;
        poly.DangerLevel = DangerLevel;
        poly.Type = Type;
        poly.IsActive = !MarkedForRemoval;
        poly.IsMissileSpell = IsMissile;
        poly.HasCollision = HasCollision;
        poly.SpellId = SpellId;
        poly.SpellName = SpellName;
        poly.CasterName = CasterName;
        poly.ConeAngle = ConeAngle;
        poly.RingInnerRadius = RingInnerRadius;
        poly.ArcAngle = ArcAngle;
        poly.UpdatePolygon(0);
        return poly;
    }
};

// ============================================================================
// CollisionChecker — Checks if a skillshot is blocked by minions/heroes/walls
// ============================================================================
namespace CollisionChecker {

    // Check if a line skillshot is blocked by any allied minion between start and end
    inline bool IsBlockedByMinions(const Vec2& start, const Vec2& end, float width) {
        for (const auto& minion : SDK::GameObjects::AllyMinions) {
            if (!minion.IsValid() || !minion.IsAlive()) continue;
            Vec2 minionPos = minion.GetServerPosition().To2D();
            auto proj = SDK::GeometryAdv::ProjectOn(minionPos, start, end);
            if (proj.IsOnSegment && proj.SegmentPoint.Distance(minionPos) <= width + minion.GetBoundingRadius()) {
                return true;
            }
        }
        return false;
    }

    // Check if a line skillshot is blocked by any allied hero between start and end
    inline bool IsBlockedByHeroes(const Vec2& start, const Vec2& end, float width) {
        for (const auto& hero : SDK::GameObjects::AllyHeroes) {
            if (!hero.IsValid() || !hero.IsAlive() || hero.IsMe()) continue;
            Vec2 heroPos = hero.GetServerPosition().To2D();
            auto proj = SDK::GeometryAdv::ProjectOn(heroPos, start, end);
            if (proj.IsOnSegment && proj.SegmentPoint.Distance(heroPos) <= width + hero.GetBoundingRadius()) {
                return true;
            }
        }
        return false;
    }

    // Check if Yasuo Wind Wall is blocking the path
    // Wind Wall creates a buff "YasuoWMovingWall" on Yasuo — check if wall is between start and missile
    inline bool IsBlockedByWindWall(const Vec2& start, const Vec2& end) {
        // Look for Yasuo's wind wall object in ally heroes
        for (const auto& hero : SDK::GameObjects::AllyHeroes) {
            if (!hero.IsValid() || !hero.IsAlive()) continue;
            if (_stricmp(hero.GetChampionName().c_str(), "Yasuo") != 0) continue;

            // Check if Yasuo has the wind wall buff active
            if (hero.HasBuff("YasuoWMovingWall")) {
                // Wind wall is active — simplified check:
                // The wall is perpendicular to Yasuo's cast direction
                // For now, check if the missile line passes near Yasuo's position
                Vec2 yasuoPos = hero.GetServerPosition().To2D();
                auto proj = SDK::GeometryAdv::ProjectOn(yasuoPos, start, end);
                if (proj.IsOnSegment && proj.SegmentPoint.Distance(yasuoPos) <= 500.0f) {
                    return true; // Simplified — wind wall is likely blocking
                }
            }
        }
        return false;
    }

    // Combined collision check for a skillshot
    inline bool IsBlocked(const DetectedSpellData& spell) {
        if (!spell.HasCollision && !spell.IsWindWallable) return false;

        Vec2 missilePos = spell.IsMissile ? spell.CurrentMissilePos : spell.StartPos;

        // Check minion collision
        if (spell.HasCollision) {
            if (IsBlockedByMinions(missilePos, spell.EndPos, spell.Width)) return true;
            if (IsBlockedByHeroes(missilePos, spell.EndPos, spell.Width)) return true;
        }

        // Check wind wall
        if (spell.IsWindWallable) {
            if (IsBlockedByWindWall(missilePos, spell.EndPos)) return true;
        }

        return false;
    }

} // namespace CollisionChecker

// ============================================================================
// TrapDetector — Handles ground trap detection (Teemo R, Caitlyn W, etc.)
// ============================================================================
namespace TrapDetector {

    struct TrapEntry {
        std::string SpellName;
        std::string ChampionName;
        float Radius = 100.0f;
        int DangerLevel = 3;
        float Duration = 120.0f;  // Lifetime in seconds
    };

    inline std::vector<TrapEntry> GetKnownTraps() {
        return {
            { "TeemoR",       "Teemo",    75.0f,  3, 300.0f  },  // Noxious Trap
            { "CaitlynW",     "Caitlyn",  67.5f,  4, 30.0f   },  // Yordle Snap Trap
            { "JhinE",        "Jhin",     130.0f, 3, 180.0f  },  // Captive Audience
            { "ShacoW",       "Shaco",    300.0f, 4, 60.0f   },  // Jack In The Box
            { "NidaleeW",     "Nidalee",  75.0f,  2, 120.0f  },  // Bushwhack
            { "MaokaiE",      "Maokai",   175.0f, 2, 30.0f   },  // Sapling Toss
            { "ZiggsE",       "Ziggs",    120.0f, 2, 10.0f   },  // Hexplosive Minefield
        };
    }

    inline bool IsKnownTrap(const std::string& spellName) {
        for (const auto& trap : GetKnownTraps()) {
            if (_stricmp(trap.SpellName.c_str(), spellName.c_str()) == 0) return true;
        }
        return false;
    }

    inline const TrapEntry* GetTrapData(const std::string& spellName) {
        static auto traps = GetKnownTraps();
        for (const auto& trap : traps) {
            if (_stricmp(trap.SpellName.c_str(), spellName.c_str()) == 0) return &trap;
        }
        return nullptr;
    }

} // namespace TrapDetector

// ============================================================================
// SpellDetector — Main detection engine
// ============================================================================
class SpellDetector {
public:
    // ---- Callbacks for external systems ----
    using OnSpellDetectedFn = std::function<void(const DetectedSpellData&)>;
    using OnSpellRemovedFn  = std::function<void(int /*spellId*/)>;
    using OnSpellUpdatedFn  = std::function<void(const DetectedSpellData&)>;

    // ---- Configuration ----
    struct Config {
        float ExtraDetectionRange = 1000.0f;   // Detect skillshots beyond screen range
        float TrapDetectionRange = 800.0f;     // Detect traps within this range
        bool EnableCollisionCheck = true;       // Default ON: don't dodge blocked skillshots
        bool EnableWindWallCheck = true;        // Default ON: respect Yasuo wind wall
        bool EnableTrapDetection = true;
        bool EnableMissileConfirmation = true;
        bool EnableFowDetection = true;        // Detect spells from fog of war
        float PingBuffer = 30.0f;              // Extra ms buffer for ping compensation
    };

    // ---- Singleton ----
    static SpellDetector& Instance() {
        static SpellDetector instance;
        return instance;
    }

    // ---- Initialize: register all event hooks ----
    void Initialize() {
        if (m_initialized) return;
        m_initialized = true;
        m_nextSpellId = 1;

        // Hook into EventSystem
        SDK::EventSystem::OnProcessSpellCast([this](const SDK::SpellCastArgs& args) {
            OnProcessSpellCast(args);
        });

        SDK::EventSystem::OnMissileCreated([this](const SDK::MissileArgs& args) {
            OnMissileCreated(args);
        });

        SDK::EventSystem::OnMissileDeleted([this](const SDK::MissileArgs& args) {
            OnMissileDeleted(args);
        });

        SDK::EventSystem::OnStopCast([this](const SDK::StopCastArgs& args) {
            OnStopCast(args);
        });
    }

    // ---- Shutdown: clear everything ----
    void Shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_detectedSpells.clear();
        m_activePolygons.clear();
        m_initialized = false;
    }

    // ---- Main update (call every script tick) ----
    void Update(float gameTime) {
        std::lock_guard<std::mutex> lock(m_mutex);

        // 1. Remove expired skillshots
        RemoveExpired(gameTime);

        // 2. Update missile positions for moving spells
        UpdateMissilePositions(gameTime);

        // 3. Check collision — remove blocked skillshots
        if (m_config.EnableCollisionCheck) {
            CheckCollisions();
        }

        // 4. Rebuild active polygon list
        RebuildActivePolygons();
    }

    // ---- Get all active skillshot polygons (for EvadeGeometry functions) ----
    const std::vector<SkillshotPolygon>& GetActiveSkillshots() const {
        return m_activePolygons;
    }

    // ---- Get mutable reference for direct manipulation ----
    std::vector<SkillshotPolygon>& GetActiveSkillshotsMut() {
        return m_activePolygons;
    }

    // ---- Get detected spell data by ID ----
    const DetectedSpellData* GetDetectedSpell(int spellId) const {
        auto it = m_detectedSpells.find(spellId);
        if (it != m_detectedSpells.end()) return &it->second;
        return nullptr;
    }

    // ---- Get all detected spells ----
    const std::unordered_map<int, DetectedSpellData>& GetAllDetectedSpells() const {
        return m_detectedSpells;
    }

    // ---- Count active skillshots ----
    int GetActiveCount() const {
        int count = 0;
        for (const auto& [id, spell] : m_detectedSpells) {
            if (!spell.MarkedForRemoval) count++;
        }
        return count;
    }

    // ---- Check if any skillshot is active ----
    bool HasActiveSkillshots() const {
        return GetActiveCount() > 0;
    }

    // ---- Manually remove a skillshot ----
    void RemoveSpell(int spellId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_detectedSpells.find(spellId);
        if (it != m_detectedSpells.end()) {
            it->second.MarkedForRemoval = true;
            if (m_onSpellRemoved) m_onSpellRemoved(spellId);
        }
    }

    // ---- Register callbacks ----
    void OnSpellDetected(OnSpellDetectedFn fn) { m_onSpellDetected = fn; }
    void OnSpellRemoved(OnSpellRemovedFn fn)   { m_onSpellRemoved = fn; }
    void OnSpellUpdated(OnSpellUpdatedFn fn)   { m_onSpellUpdated = fn; }

    // ---- Config access ----
    Config& GetConfig() { return m_config; }
    const Config& GetConfig() const { return m_config; }

    // ---- Check if a specific spell should be dodged (menu integration) ----
    // menuCheckFn returns true if the spell is enabled in the evade menu
    using MenuCheckFn = std::function<bool(const std::string& spellName, int dangerLevel)>;
    void SetMenuCheckFunction(MenuCheckFn fn) { m_menuCheckFn = fn; }

private:
    SpellDetector() = default;

    // ================================================================
    // EVENT HANDLERS
    // ================================================================

    // OnProcessSpellCast — fires when an enemy champion starts casting
    void OnProcessSpellCast(const SDK::SpellCastArgs& args) {
        // Ignore ally spells and auto attacks
        if (args.IsAutoAttack) return;
        if (!args.Sender.IsValid()) return;
        if (args.Sender.GetTeam() == SDK::GameObjects::Player.GetTeam()) return;

        // Only process enemy hero spells
        bool isEnemyHero = false;
        for (const auto& enemy : SDK::GameObjects::EnemyHeroes) {
            if (enemy.IsValid() && enemy.GetNetId() == args.Sender.GetNetId()) {
                isEnemyHero = true;
                break;
            }
        }
        if (!isEnemyHero) return;

        // Look up spell in database
        const EzEvade::SpellData* spellData = EzEvade::GetSpellByName(args.SpellName);
        if (!spellData) return;
        if (!spellData->IsSkillshot()) return;

        // Check menu: is this spell enabled for dodging?
        if (m_menuCheckFn && !m_menuCheckFn(args.SpellName, spellData->dangerLevel)) {
            return;
        }

        // Create detection entry
        DetectedSpellData detected;
        detected.SpellId = m_nextSpellId++;
        detected.DatabaseEntry = spellData;
        detected.CasterNetId = args.Sender.GetNetId();
        detected.CasterName = args.Sender.GetChampionName();
        detected.SpellName = args.SpellName;
        detected.Type = DetectedSpellData::ConvertType(spellData->type);

        // Geometry from database
        detected.Speed = spellData->speed;
        detected.Width = spellData->radius;
        detected.Range = spellData->range + spellData->extraRange;
        detected.ConeAngle = spellData->angle * 3.14159f / 180.0f;  // degrees to radians
        detected.CastDelay = spellData->castDelay;
        detected.IsMissile = spellData->isMissile;
        detected.IsCC = spellData->isCC;
        detected.HasCollision = spellData->CollidesWithMinions();
        detected.IsWindWallable = spellData->CanBeWindWalled();
        detected.DangerLevel = spellData->dangerLevel;

        // Try to augment geometry with the full SDK database entries 
        if (const auto* sdkEntry = SDK::SpellDatabase::GetByName(args.SpellName)) {
            if (sdkEntry->MissileSpeed > 0) detected.Speed = (float)sdkEntry->MissileSpeed;
            if (sdkEntry->Radius > 0) detected.Width = (float)sdkEntry->Radius;
            else if (sdkEntry->Width > 0) detected.Width = (float)sdkEntry->Width;
            if (sdkEntry->Range > 0) detected.Range = (float)(sdkEntry->Range + sdkEntry->ExtraRange);
            if (sdkEntry->Angle > 0) detected.ConeAngle = sdkEntry->Angle * 3.14159f / 180.0f;
            if (sdkEntry->Delay > 0) detected.CastDelay = sdkEntry->Delay / 1000.0f;
            detected.IsMissile = (sdkEntry->Type == SDK::SpellType::SkillshotMissileLine) || 
                                 (sdkEntry->Type == SDK::SpellType::SkillshotMissileCircle) || 
                                 (sdkEntry->Type == SDK::SpellType::SkillshotMissileCone) || 
                                 (sdkEntry->Type == SDK::SpellType::SkillshotMissileArc);
            detected.HasCollision = (sdkEntry->CollisionObjects & SDK::CollisionMinions) != 0;
            detected.IsWindWallable = (sdkEntry->CollisionObjects & SDK::CollisionYasuoWall) != 0;

            // Fix type from SDK DB (critical: EzEvade DB hardcodes Line for everything)
            if (sdkEntry->IsSkillshot()) {
                switch (sdkEntry->Type) {
                case SDK::SpellType::SkillshotCircle:
                case SDK::SpellType::SkillshotMissileCircle:
                    detected.Type = SpellType::Circle;
                    break;
                case SDK::SpellType::SkillshotCone:
                case SDK::SpellType::SkillshotMissileCone:
                    detected.Type = SpellType::Cone;
                    break;
                case SDK::SpellType::SkillshotRing:
                    detected.Type = SpellType::Ring;
                    break;
                case SDK::SpellType::SkillshotArc:
                case SDK::SpellType::SkillshotMissileArc:
                    detected.Type = SpellType::Arc;
                    break;
                default:
                    detected.Type = SpellType::Line;
                    break;
                }
            }
        }

        // Failsafe bounds
        if (detected.Speed <= 0.0f) detected.Speed = 99999.0f;
        if (detected.Width <= 0.0f) detected.Width = 50.0f;

        // Positions from cast args
        detected.StartPos = args.StartPos.To2D();

        // Calculate end position based on range and cast direction
        Vec2 direction = (args.EndPos.To2D() - args.StartPos.To2D()).Normalized();
        float castDist = args.StartPos.To2D().Distance(args.EndPos.To2D());

        if (spellData->fixedRange || castDist > detected.Range) {
            detected.EndPos = detected.StartPos + direction * detected.Range;
        } else {
            detected.EndPos = args.EndPos.To2D();
        }

        detected.Direction = direction;
        detected.CurrentMissilePos = detected.StartPos;

        // Timing
        detected.CastTime = args.CastTime > 0 ? args.CastTime : SDK::Game::GetTime();

        // Calculate end time
        if (detected.Speed > 0.0f) {
            detected.EndTime = detected.CastTime + detected.CastDelay +
                              (detected.Range / detected.Speed) + 0.5f; // +0.5s buffer
        } else {
            // Instant/ground spell — use a reasonable duration
            detected.EndTime = detected.CastTime + detected.CastDelay + 3.0f;
        }

        // Check if this is a trap
        if (TrapDetector::IsKnownTrap(args.SpellName)) {
            const auto* trapData = TrapDetector::GetTrapData(args.SpellName);
            if (trapData) {
                detected.IsTrap = true;
                detected.Type = SpellType::Circle;
                detected.Width = trapData->Radius;
                detected.EndTime = detected.CastTime + trapData->Duration;
                detected.DangerLevel = trapData->DangerLevel;
                detected.Speed = 0.0f;
                detected.EndPos = args.EndPos.To2D();
            }
        }

        // Add to tracked list
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            detected.IsProcessed = true;
            m_detectedSpells[detected.SpellId] = detected;
        }

        // Fire callback
        if (m_onSpellDetected) {
            m_onSpellDetected(detected);
        }
    }

    // OnMissileCreated — fires when a missile object is created in the game
    void OnMissileCreated(const SDK::MissileArgs& args) {
        if (!m_config.EnableMissileConfirmation) return;

        // Check if this missile is from an enemy
        bool isEnemyMissile = false;
        for (const auto& enemy : SDK::GameObjects::EnemyHeroes) {
            if (enemy.IsValid() && enemy.GetNetId() == args.CasterNetId) {
                isEnemyMissile = true;
                break;
            }
        }
        if (!isEnemyMissile) return;

        std::lock_guard<std::mutex> lock(m_mutex);

        // Try to match missile to existing detected spell
        bool matched = false;
        for (auto& [id, spell] : m_detectedSpells) {
            if (spell.MarkedForRemoval) continue;
            if (spell.CasterNetId != args.CasterNetId) continue;
            if (!spell.IsMissile) continue;
            if (spell.MissileConfirmed) continue;

            // Match by spell name or missile name
            if (_stricmp(spell.SpellName.c_str(), args.SpellName.c_str()) == 0 ||
                (spell.DatabaseEntry && !spell.DatabaseEntry->missileSpellName.empty() &&
                 _stricmp(spell.DatabaseEntry->missileSpellName.c_str(), args.SpellName.c_str()) == 0))
            {
                // Confirm missile — update with real missile data
                spell.MissileConfirmed = true;
                spell.MissileNetId = args.MissileObj.GetNetworkId();
                spell.MissileObj3DPos = args.StartPos;

                // Update start position to actual missile start
                Vec2 actualStart = args.StartPos.To2D();
                Vec2 actualEnd = args.EndPos.To2D();
                Vec2 dir = (actualEnd - actualStart).Normalized();
                float dist = actualStart.Distance(actualEnd);

                // Use actual positions if they're valid
                if (!actualStart.IsZero()) {
                    spell.StartPos = actualStart;
                    spell.CurrentMissilePos = actualStart;
                }
                if (!actualEnd.IsZero() && dist > 10.0f) {
                    spell.Direction = dir;
                    if (spell.DatabaseEntry && spell.DatabaseEntry->fixedRange) {
                        spell.EndPos = spell.StartPos + dir * spell.Range;
                    } else {
                        spell.EndPos = actualEnd;
                    }
                }

                // Rebuild polygon with confirmed data
                spell.IsProcessed = true;

                matched = true;
                if (m_onSpellUpdated) m_onSpellUpdated(spell);
                break;
            }
        }

        // If missile wasn't matched to a process spell cast (FoW detection)
        if (!matched && m_config.EnableFowDetection) {
            // Try looking up the missile name in spell database
            const EzEvade::SpellData* spellData = EzEvade::GetSpellByMissile(args.SpellName);
            if (!spellData) spellData = EzEvade::GetSpellByName(args.SpellName);
            if (spellData && spellData->IsSkillshot()) {
                DetectedSpellData detected;
                detected.SpellId = m_nextSpellId++;
                detected.DatabaseEntry = spellData;
                detected.CasterNetId = args.CasterNetId;
                detected.SpellName = args.SpellName;
                detected.Type = DetectedSpellData::ConvertType(spellData->type);
                detected.Speed = spellData->speed;
                detected.Width = spellData->radius;
                detected.Range = spellData->range + spellData->extraRange;
                detected.ConeAngle = spellData->angle * 3.14159f / 180.0f;
                detected.CastDelay = 0.0f; // Already cast, no delay
                detected.IsMissile = true;
                detected.MissileConfirmed = true;
                detected.MissileNetId = args.MissileObj.GetNetworkId();
                detected.IsCC = spellData->isCC;
                detected.HasCollision = spellData->CollidesWithMinions();
                detected.IsWindWallable = spellData->CanBeWindWalled();
                
                // Try to augment geometry from SDK database
                const auto* sdkEntry = SDK::SpellDatabase::GetByMissileName(args.SpellName);
                if (!sdkEntry) sdkEntry = SDK::SpellDatabase::GetByName(args.SpellName);
                if (sdkEntry) {
                    if (sdkEntry->MissileSpeed > 0) detected.Speed = (float)sdkEntry->MissileSpeed;
                    if (sdkEntry->Radius > 0) detected.Width = (float)sdkEntry->Radius;
                    else if (sdkEntry->Width > 0) detected.Width = (float)sdkEntry->Width;
                    if (sdkEntry->Range > 0) detected.Range = (float)(sdkEntry->Range + sdkEntry->ExtraRange);
                    if (sdkEntry->Angle > 0) detected.ConeAngle = sdkEntry->Angle * 3.14159f / 180.0f;
                    detected.HasCollision = (sdkEntry->CollisionObjects & SDK::CollisionMinions) != 0;
                    detected.IsWindWallable = (sdkEntry->CollisionObjects & SDK::CollisionYasuoWall) != 0;
                }

                if (detected.Speed <= 0.0f) detected.Speed = 99999.0f;
                if (detected.Width <= 0.0f) detected.Width = 50.0f;

                // Fix type from SDK DB for FoW missiles too
                if (sdkEntry && sdkEntry->IsSkillshot()) {
                    switch (sdkEntry->Type) {
                    case SDK::SpellType::SkillshotCircle:
                    case SDK::SpellType::SkillshotMissileCircle:
                        detected.Type = SpellType::Circle;
                        break;
                    case SDK::SpellType::SkillshotCone:
                    case SDK::SpellType::SkillshotMissileCone:
                        detected.Type = SpellType::Cone;
                        break;
                    case SDK::SpellType::SkillshotRing:
                        detected.Type = SpellType::Ring;
                        break;
                    case SDK::SpellType::SkillshotArc:
                    case SDK::SpellType::SkillshotMissileArc:
                        detected.Type = SpellType::Arc;
                        break;
                    default:
                        detected.Type = SpellType::Line;
                        break;
                    }
                }

                detected.DangerLevel = spellData->dangerLevel;

                detected.StartPos = args.StartPos.To2D();
                Vec2 direction = (args.EndPos.To2D() - args.StartPos.To2D()).Normalized();
                detected.Direction = direction;
                detected.EndPos = detected.StartPos + direction * detected.Range;
                detected.CurrentMissilePos = detected.StartPos;
                detected.CastTime = SDK::Game::GetTime();
                detected.EndTime = detected.CastTime + (detected.Range / std::max(1.0f, detected.Speed)) + 0.5f;
                detected.IsProcessed = true;

                // Find caster name
                for (const auto& enemy : SDK::GameObjects::EnemyHeroes) {
                    if (enemy.IsValid() && enemy.GetNetId() == args.CasterNetId) {
                        detected.CasterName = enemy.GetChampionName();
                        break;
                    }
                }

                m_detectedSpells[detected.SpellId] = detected;
                if (m_onSpellDetected) m_onSpellDetected(detected);
            }
        }
    }

    // OnMissileDeleted — fires when a missile is destroyed (hit something or reached end)
    void OnMissileDeleted(const SDK::MissileArgs& args) {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (auto& [id, spell] : m_detectedSpells) {
            if (spell.MarkedForRemoval) continue;

            // Match by missile NetId
            if (spell.MissileConfirmed && spell.MissileNetId == args.MissileObj.GetNetworkId()) {
                spell.MarkedForRemoval = true;
                if (m_onSpellRemoved) m_onSpellRemoved(id);
                break;
            }

            // Fallback: match by caster + spell name
            if (spell.IsMissile && spell.CasterNetId == args.CasterNetId &&
                _stricmp(spell.SpellName.c_str(), args.SpellName.c_str()) == 0) {
                spell.MarkedForRemoval = true;
                if (m_onSpellRemoved) m_onSpellRemoved(id);
                break;
            }
        }
    }

    // OnStopCast — fires when a spell cast is interrupted
    void OnStopCast(const SDK::StopCastArgs& args) {
        if (!args.ForceStop) return; // Only handle interrupts, not natural completions

        std::lock_guard<std::mutex> lock(m_mutex);

        // Remove spells from this caster that haven't launched their missile yet
        for (auto& [id, spell] : m_detectedSpells) {
            if (spell.MarkedForRemoval) continue;
            if (spell.CasterNetId != args.Sender.GetNetId()) continue;
            if (spell.MissileConfirmed) continue; // Missile already in flight, can't be canceled

            // Check if it's the same spell
            if (_stricmp(spell.SpellName.c_str(), args.SpellName.c_str()) == 0) {
                spell.MarkedForRemoval = true;
                if (m_onSpellRemoved) m_onSpellRemoved(id);
            }
        }
    }

    // ================================================================
    // UPDATE HELPERS
    // ================================================================

    // Remove expired skillshots
    void RemoveExpired(float gameTime) {
        for (auto it = m_detectedSpells.begin(); it != m_detectedSpells.end(); ) {
            auto& spell = it->second;
            bool shouldRemove = spell.MarkedForRemoval;

            // Check expiry
            if (!shouldRemove && spell.EndTime > 0.0f && gameTime > spell.EndTime) {
                shouldRemove = true;
            }

            // Check if missile-based spell has traveled full range
            if (!shouldRemove && spell.IsMissile && spell.Speed > 0.0f) {
                float elapsed = gameTime - spell.CastTime - spell.CastDelay;
                if (elapsed > 0) {
                    float traveled = elapsed * spell.Speed;
                    if (traveled > spell.Range + 100.0f) {
                        shouldRemove = true;
                    }
                }
            }

            if (shouldRemove) {
                if (m_onSpellRemoved) m_onSpellRemoved(it->first);
                it = m_detectedSpells.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Update missile positions for moving spells
    void UpdateMissilePositions(float gameTime) {
        for (auto& [id, spell] : m_detectedSpells) {
            if (spell.MarkedForRemoval) continue;
            if (!spell.IsMissile || spell.Speed <= 0.0f) continue;

            float elapsed = gameTime - spell.CastTime - spell.CastDelay;
            if (elapsed < 0.0f) elapsed = 0.0f;

            float distTraveled = elapsed * spell.Speed;
            if (distTraveled > spell.Range) distTraveled = spell.Range;

            spell.CurrentMissilePos = spell.StartPos + spell.Direction * distTraveled;
        }
    }

    // Check collisions and mark blocked skillshots for removal
    void CheckCollisions() {
        for (auto& [id, spell] : m_detectedSpells) {
            if (spell.MarkedForRemoval) continue;
            if (CollisionChecker::IsBlocked(spell)) {
                spell.MarkedForRemoval = true;
            }
        }
    }

    // Rebuild the active polygon list from detected spells
    void RebuildActivePolygons() {
        m_activePolygons.clear();
        m_activePolygons.reserve(m_detectedSpells.size());

        for (const auto& [id, spell] : m_detectedSpells) {
            if (spell.MarkedForRemoval) continue;
            if (!spell.IsProcessed) continue;
            m_activePolygons.push_back(spell.ToSkillshotPolygon());
        }
    }

    // ================================================================
    // DATA
    // ================================================================
    bool m_initialized = false;
    int m_nextSpellId = 1;
    std::mutex m_mutex;

    Config m_config;
    std::unordered_map<int, DetectedSpellData> m_detectedSpells;
    std::vector<SkillshotPolygon> m_activePolygons;

    // Callbacks
    OnSpellDetectedFn m_onSpellDetected;
    OnSpellRemovedFn  m_onSpellRemoved;
    OnSpellUpdatedFn  m_onSpellUpdated;
    MenuCheckFn       m_menuCheckFn;
};

} // namespace Evade
