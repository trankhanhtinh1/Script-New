#pragma once
#include "EvadeMenuData.h"
#include "sdk/Events/EventSystem.h"
#include "sdk/Game.h"
#include "sdk/GameObjects/GameObjects.h"
#include "sdk/GameObjects/Missile.h"
#include "sdk/Math/Polygon.h"
#include "sdk/UI/Drawing.h"
#include "sdk/UI/MenuUI.h"
#include "sdk/Utils/DebugConsole.h"
#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace Plugins::Evade {

    class EvadeDrawer {
    public:
        static bool IsEnemyMissile(const SDK::Missile& missile) {
            if (!missile.IsValid()) {
                return false;
            }

            // Filter out auto attacks (minions, heroes) and turret shots by name
            if (missile.IsAutoAttack() || missile.IsTurretShot()) {
                return false;
            }

            // Use missile team vs player team (simple and reliable)
            SDK::GameObject obj(missile.address);
            auto missileTeam = obj.GetTeam();
            auto playerTeam  = SDK::GameObjects::Player.GetTeam();

            if (missileTeam == SDK::GameObjectTeam::Neutral) {
                return false;
            }

            // Enemy = different team. Minions only do BasicAttack (already filtered above).
            return missileTeam != playerTeam;
        }

        static void Initialize() {
            s_enabled = true;
            RegisterCallbacks();
            s_tracked.clear();
            s_debug = {};
        }

        static void Shutdown() {
            s_enabled = false;
            s_tracked.clear();
            s_debug = {};
        }

        static void Update(SDK::MenuUI::Menu* rootMenu) {
            if (!s_enabled || !SDK::GameObjects::Player.IsValid()) {
                return;
            }

            RefreshDebugOptions(rootMenu);

            const float now = SDK::Game::GetTime();

            // ============================================================
            // PRIMARY PATH: Poll MissileManager directly every frame
            // (more reliable than event-only approach since events can miss)
            // ============================================================
            auto missiles = SDK::MissileManager::GetMissiles();
            s_debug.TotalMissilesInFlight = static_cast<int>(missiles.size());

            std::unordered_map<int, SDK::Missile> missilesById;
            missilesById.reserve(missiles.size());

            int totalProcessed = 0;
            int filteredInvalid = 0;
            int filteredTurretAA = 0;
            int filteredNotEnemy = 0;
            int filteredNoMatch = 0;
            int filteredNotSkillshot = 0;
            int addedOrUpdated = 0;

            for (const auto& missile : missiles) {
                if (!missile.IsValid()) {
                    ++filteredInvalid;
                    continue;
                }

                ++totalProcessed;

                // Read all names FIRST — needed for AA/turret/no-name filtering
                std::string missileName = missile.GetMissileName();
                std::string spellName = missile.GetSpellName();
                std::string objectName = SDK::GameObject(missile.address).GetName();
                const int casterNetId = missile.GetCasterNetId();
                const int missileNetId = missile.GetNetworkId();

                // Filter: if ALL names are empty, this is garbage (not a real spell missile)
                if (missileName.empty() && spellName.empty() && objectName.empty()) {
                    ++filteredInvalid;
                    continue;
                }

                // Filter: auto attacks (minions + heroes both use "BasicAttack")
                if (missile.IsAutoAttack()) {
                    ++filteredTurretAA;
                    continue;
                }

                // Filter: turret shots
                if (missile.IsTurretShot()) {
                    ++filteredTurretAA;
                    continue;
                }

                if (!IsEnemyMissile(missile)) {
                    ++filteredNotEnemy;
                    continue;
                }

                // Track as "seen" for debug panel
                s_debug.LastMissileSeen = missileName.empty() ? spellName : missileName;
                ++s_debug.MissileSeen;

                // Lookup in EzEvade SpellDatabase
                const auto* spell = LookupSpellData(missileName, spellName, objectName);

                if (spell == nullptr) {
                    ++filteredNoMatch;
                    DebugLog("NoMatch", "ENEMY missile not in DB: caster=%d missile=[%s] spell=[%s] obj=[%s]",
                        casterNetId, missileName.c_str(), spellName.c_str(), objectName.c_str());

                    // FALLBACK: Draw unmatched enemy missiles with generic rectangle
                    // so user can see SOMETHING and report what's missing from the DB
                    if (s_drawUnmatchedMissiles) {
                        DrawFallbackMissile(missile, missileName.empty() ? spellName : missileName, now);
                    }
                    continue;
                }

                if (!spell->IsSkillshot()) {
                    ++filteredNotSkillshot;
                    DebugLog("Filter", "Matched but NOT skillshot: %s.%s type=%d",
                        spell->charName.c_str(), spell->spellName.c_str(), (int)spell->type);
                    continue;
                }

                // SUCCESS: Build tracked skillshot
                ++addedOrUpdated;
                ++s_debug.MissileMatched;
                s_debug.LastMissileMatch = ResolveChampionName(casterNetId, spell->charName) + " " + spell->spellName;

                TrackedSkillshot tracked = BuildMissileSkillshot(*spell, missile, casterNetId, now);
                AddOrUpdateTracked(tracked);
                missilesById[tracked.MissileNetId] = missile;
            }

            // Update debug counters
            s_debug.FilteredInvalid = filteredInvalid;
            s_debug.FilteredTurretAA = filteredTurretAA;
            s_debug.FilteredNotEnemy = filteredNotEnemy;
            s_debug.FilteredNoMatch = filteredNoMatch;
            s_debug.FilteredNotSkillshot = filteredNotSkillshot;
            s_debug.AddedOrUpdated = addedOrUpdated;

            // Update current positions for tracked missiles still in flight
            for (auto& tracked : s_tracked) {
                if (tracked.HasMissile && tracked.MissileNetId != 0) {
                    auto it = missilesById.find(tracked.MissileNetId);
                    if (it != missilesById.end()) {
                        tracked.CurrentPos = ResolveMissileCurrentPos(it->second, tracked);
                        tracked.EndPos = ResolveMissileEndPosition(*tracked.Data, tracked.StartPos, it->second);
                        tracked.LastSeenTime = now;

                        // Re-resolve StartPos if it was zero initially
                        if (tracked.StartPos.IsZero()) {
                            tracked.StartPos = ResolveStartPosition(it->second);
                        }
                    }
                }
            }

            // Expire old tracked skillshots
            // Use generous grace periods — missiles can disappear from MissileManager
            // faster than our poll rate, so we need to keep drawing for a while
            s_tracked.erase(
                std::remove_if(s_tracked.begin(), s_tracked.end(), [now](const TrackedSkillshot& tracked) {
                    if (!tracked.Active || tracked.Data == nullptr) {
                        return true;
                    }

                    if (tracked.HasMissile) {
                        // Grace period: keep drawing even after missile disappears
                        // canBeRemoved spells (e.g. Yasuo wall blocks) expire faster
                        const float grace = tracked.Data->canBeRemoved ? 0.50f : 1.50f;
                        return (now - tracked.LastSeenTime) > grace;
                    }

                    return now > tracked.ExpireTime;
                }),
                s_tracked.end());

            // Expire fallback missiles
            s_fallbackMissiles.erase(
                std::remove_if(s_fallbackMissiles.begin(), s_fallbackMissiles.end(),
                    [now](const FallbackMissile& fm) { return (now - fm.LastSeenTime) > 1.50f; }),
                s_fallbackMissiles.end());

            s_debug.ActiveTracked = static_cast<int>(s_tracked.size());
            s_debug.ActiveFallback = static_cast<int>(s_fallbackMissiles.size());
        }

        static void Draw(SDK::MenuUI::Menu* rootMenu) {
            if (!s_enabled || rootMenu == nullptr || !SDK::GameObjects::Player.IsValid()) {
                return;
            }

            RefreshDebugOptions(rootMenu);

            const bool drawPolygon = GetDebugBool(rootMenu, "DrawSkillshotPolygon");
            const bool drawDirection = GetDebugBool(rootMenu, "DrawSkillshotDirection");
            const bool drawDanger = GetDebugBool(rootMenu, "DrawDangerLevel");

            s_debug.FrameDrawn = 0;
            s_debug.FrameSkippedByMenu = 0;
            s_debug.FrameSkippedByShape = 0;
            s_debug.LastDraw.clear();

            // Draw tracked (matched) skillshots
            for (const auto& tracked : s_tracked) {
                if (!tracked.Active || tracked.Data == nullptr) {
                    continue;
                }

                if (!ShouldDrawSkillshot(rootMenu, tracked)) {
                    ++s_debug.FrameSkippedByMenu;
                    continue;
                }

                const float extraRadius = static_cast<float>(GetExtraRadius(rootMenu, tracked));
                const ImU32 color = ResolveColor(*tracked.Data);
                const ImU32 fillColor = drawPolygon ? ApplyAlpha(color, 48) : ApplyAlpha(color, 28);

                if (!DrawSkillshotShape(tracked, extraRadius, color, fillColor)) {
                    ++s_debug.FrameSkippedByShape;
                    continue;
                }

                if (drawDirection) {
                    SDK::Drawing::DrawLine3D(tracked.StartPos, tracked.EndPos, ApplyAlpha(color, 180), 1.5f);
                }

                if (drawDanger) {
                    const std::string label = EvadeMenu::GetSkillshotDisplayName(*tracked.Data) + " [" + std::to_string(tracked.Data->dangerLevel) + "]";
                    SDK::Drawing::DrawTextCentered(GetLabelPosition(tracked), label.c_str(), color);
                }

                s_debug.LastDraw = tracked.ChampionName + " " + tracked.SpellName;
                ++s_debug.FrameDrawn;
            }

            // Draw fallback (unmatched) enemy missiles
            for (const auto& fm : s_fallbackMissiles) {
                SDK::Missile missile(fm.Address);
                if (!missile.IsValid()) continue;

                Vec3 startPos = missile.GetStartPos();
                if (startPos.IsZero()) {
                    startPos = missile.GetPosition();
                }
                if (startPos.IsZero()) {
                    auto caster = SDK::ObjectManager::GetObjectByNetId(missile.GetCasterNetId());
                    if (caster.IsValid()) {
                        startPos = caster.GetPosition();
                    }
                }

                Vec3 endPos = missile.GetEndPos();
                Vec3 curPos = missile.GetPosition();

                if (endPos.IsZero()) endPos = missile.GetCastEndPos();
                if (endPos.IsZero()) endPos = missile.GetCastEndPos();
                if (endPos.IsZero()) continue;

                ImU32 color = IM_COL32(255, 255, 100, 180); // Yellow = unmatched
                float radius = 60.0f;

                // Draw rectangle
                if (!startPos.IsZero() && startPos.Distance2D(endPos) > 2.0f) {
                    Vec3 drawFrom = !curPos.IsZero() ? curPos : startPos;
                    SDK::RectanglePoly rect(drawFrom, endPos, radius);
                    SDK::Drawing::DrawPolygon(rect.Points, drawFrom.y, color, 1.7f, ApplyAlpha(color, 30));
                }

                // Draw current position dot
                if (!curPos.IsZero()) {
                    SDK::Drawing::DrawCircle(curPos, 25.0f, IM_COL32(255, 200, 50, 230), 2.5f);
                }

                // Label
                Vec3 labelPos = !curPos.IsZero() ? curPos : endPos;
                if (!labelPos.IsZero()) {
                    std::string label = "[?] " + fm.DisplayName;
                    SDK::Drawing::DrawTextCentered(labelPos, label.c_str(), IM_COL32(255, 255, 100, 230));
                }
            }

            if (s_drawDebugPanel) {
                DrawDebugPanel();
            }
        }

    private:
        struct TrackedSkillshot {
            const EzEvade::SpellData* Data = nullptr;
            std::string ChampionName;
            std::string MenuSpellId;
            std::string SpellName;
            Vec3 StartPos;
            Vec3 EndPos;
            Vec3 CurrentPos;
            float StartTime = 0.0f;
            float ExpireTime = 0.0f;
            float LastSeenTime = 0.0f;
            int CasterNetId = 0;
            int MissileNetId = 0;
            bool HasMissile = false;
            bool Active = true;
        };

        struct FallbackMissile {
            uintptr_t Address = 0;
            int NetId = 0;
            std::string DisplayName;
            float LastSeenTime = 0.0f;
        };

        struct DebugState {
            int TotalMissilesInFlight = 0;
            int MissileSeen = 0;
            int MissileMatched = 0;
            int ActiveTracked = 0;
            int ActiveFallback = 0;
            int FrameDrawn = 0;
            int FrameSkippedByMenu = 0;
            int FrameSkippedByShape = 0;
            int FilteredInvalid = 0;
            int FilteredTurretAA = 0;
            int FilteredNotEnemy = 0;
            int FilteredNoMatch = 0;
            int FilteredNotSkillshot = 0;
            int AddedOrUpdated = 0;
            std::string LastMissileSeen;
            std::string LastMissileMatch;
            std::string LastDraw;
        };

        static inline bool s_enabled = false;
        static inline bool s_callbacksRegistered = false;
        static inline bool s_drawDebugPanel = true;
        static inline bool s_logConsole = true;
        static inline bool s_drawUnmatchedMissiles = false;
        static inline std::vector<TrackedSkillshot> s_tracked;
        static inline std::vector<FallbackMissile> s_fallbackMissiles;
        static inline DebugState s_debug;
        static inline float s_lastFullLog = 0.0f;

        // ====================================================================
        // Callbacks (supplementary — events may also catch some missiles)
        // ====================================================================
        static void RegisterCallbacks() {
            if (s_callbacksRegistered) {
                return;
            }

            SDK::EventSystem::OnMissileCreated([](const SDK::MissileArgs& args) {
                if (!s_enabled || !args.MissileObj.IsValid()) {
                    return;
                }

                if (!IsEnemyMissile(args.MissileObj)) {
                    return;
                }

                const auto* spell = LookupSpellData(
                    args.MissileObj.GetMissileName(),
                    args.SpellName.empty() ? args.MissileObj.GetSpellName() : args.SpellName,
                    SDK::GameObject(args.MissileObj.address).GetName());
                if (spell == nullptr || !spell->IsSkillshot()) {
                    return;
                }

                const auto tracked = BuildMissileSkillshot(*spell, args.MissileObj, args.MissileObj.GetCasterNetId(), SDK::Game::GetTime());
                AddOrUpdateTracked(tracked);

                DebugLog("Event", "OnMissileCreated: %s %s -> tracked",
                    tracked.ChampionName.c_str(), spell->spellName.c_str());
            });

            SDK::EventSystem::OnMissileDeleted([](const SDK::MissileArgs& args) {
                const int missileNetId = args.MissileObj.GetNetworkId();
                if (missileNetId == 0) {
                    return;
                }

                for (auto& tracked : s_tracked) {
                    if (tracked.MissileNetId == missileNetId) {
                        tracked.LastSeenTime = 0.0f;
                        tracked.Active = false;
                    }
                }

                // Also remove from fallback
                s_fallbackMissiles.erase(
                    std::remove_if(s_fallbackMissiles.begin(), s_fallbackMissiles.end(),
                        [missileNetId](const FallbackMissile& fm) { return fm.NetId == missileNetId; }),
                    s_fallbackMissiles.end());
            });

            s_callbacksRegistered = true;
        }

        // ====================================================================
        // Debug helpers
        // ====================================================================
        static void RefreshDebugOptions(SDK::MenuUI::Menu* rootMenu) {
            if (rootMenu == nullptr) {
                return;
            }

            auto* debugMenu = rootMenu->GetSubMenu("Debug");
            if (debugMenu == nullptr) {
                return;
            }

            s_drawDebugPanel = debugMenu->GetBoolValue("DrawDetectionPanel", true);
            s_logConsole = debugMenu->GetBoolValue("LogDetectionConsole", true);
        }

        static void DebugLog(const char* tag, const char* format, ...) {
            if (!s_logConsole || format == nullptr) {
                return;
            }

            char buffer[1024] = {};
            va_list args;
            va_start(args, format);
            vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);
            DebugConsole::LogTagged(tag, "%s", buffer);
        }

        static void DrawDebugPanel() {
            float px = 10.0f;
            float py = 500.0f;
            float lh = 15.0f;
            int line = 0;
            char buf[300] = {};

            auto drawLine = [&](const char* text, ImU32 color = IM_COL32(230, 230, 230, 255)) {
                SDK::Drawing::DrawScreenText(Vec2(px, py + lh * line++), text, color);
            };

            drawLine("=== Evade Detection Debug ===", IM_COL32(255, 210, 70, 255));

            snprintf(buf, sizeof(buf), "missiles_in_flight=%d  enemy_seen=%d  db_matched=%d",
                s_debug.TotalMissilesInFlight, s_debug.MissileSeen, s_debug.MissileMatched);
            drawLine(buf, IM_COL32(120, 220, 255, 255));

            snprintf(buf, sizeof(buf), "tracked=%d  fallback=%d  drawn=%d  skipMenu=%d  skipShape=%d",
                s_debug.ActiveTracked, s_debug.ActiveFallback, s_debug.FrameDrawn,
                s_debug.FrameSkippedByMenu, s_debug.FrameSkippedByShape);
            drawLine(buf, IM_COL32(100, 255, 160, 240));

            snprintf(buf, sizeof(buf), "FILTER: invalid=%d turret/aa=%d notEnemy=%d noMatch=%d notSkill=%d",
                s_debug.FilteredInvalid, s_debug.FilteredTurretAA, s_debug.FilteredNotEnemy,
                s_debug.FilteredNoMatch, s_debug.FilteredNotSkillshot);
            drawLine(buf, IM_COL32(255, 160, 100, 230));

            if (!s_debug.LastMissileSeen.empty()) {
                snprintf(buf, sizeof(buf), "last seen: %s", s_debug.LastMissileSeen.c_str());
                drawLine(buf, IM_COL32(180, 180, 210, 230));
            }
            if (!s_debug.LastMissileMatch.empty()) {
                snprintf(buf, sizeof(buf), "last match: %s", s_debug.LastMissileMatch.c_str());
                drawLine(buf, IM_COL32(120, 255, 180, 230));
            }
            if (!s_debug.LastDraw.empty()) {
                snprintf(buf, sizeof(buf), "last draw: %s", s_debug.LastDraw.c_str());
                drawLine(buf, IM_COL32(255, 170, 120, 230));
            }

            // Show enemy hero NetIds for reference
            snprintf(buf, sizeof(buf), "EnemyHeroes: %d", (int)SDK::GameObjects::EnemyHeroes.size());
            drawLine(buf, IM_COL32(200, 150, 255, 220));
            int enemyShown = 0;
            for (const auto& enemy : SDK::GameObjects::EnemyHeroes) {
                if (!enemy.IsValid() || enemyShown >= 5) break;
                snprintf(buf, sizeof(buf), "  [%d] %s netId=%d alive=%d",
                    enemyShown + 1,
                    enemy.GetChampionName().c_str(),
                    enemy.GetNetId(),
                    enemy.IsAlive() ? 1 : 0);
                drawLine(buf, IM_COL32(180, 140, 230, 210));
                ++enemyShown;
            }
        }

        // ====================================================================
        // Fallback drawing for unmatched enemy missiles
        // ====================================================================
        static void DrawFallbackMissile(const SDK::Missile& missile, const std::string& displayName, float now) {
            int netId = missile.GetNetworkId();
            if (netId == 0) return;

            // Check if already tracked
            for (auto& fm : s_fallbackMissiles) {
                if (fm.NetId == netId) {
                    fm.LastSeenTime = now;
                    fm.Address = missile.address;
                    return;
                }
            }

            FallbackMissile fm;
            fm.Address = missile.address;
            fm.NetId = netId;
            fm.DisplayName = displayName;
            fm.LastSeenTime = now;
            s_fallbackMissiles.push_back(fm);
        }

        // Try to match missile to enemy by checking if any enemy is casting a spell with that name
        static bool TryMatchEnemyByCastState(const std::string& spellName, const std::string& missileName) {
            if (spellName.empty() && missileName.empty()) return false;

            // If we have EzEvade spell data for this name, and the champion it belongs to is an enemy
            const auto* spell = LookupSpellData(missileName, spellName, "");
            if (spell != nullptr && !spell->charName.empty()) {
                for (const auto& enemy : SDK::GameObjects::EnemyHeroes) {
                    if (!enemy.IsValid()) continue;
                    std::string champName = enemy.GetChampionName();
                    if (EzEvade::detail::EqualsIgnoreCase(champName, spell->charName)) {
                        DebugLog("Fallback", "Matched missile to enemy %s via spell DB: %s",
                            champName.c_str(), spell->spellName.c_str());
                        return true;
                    }
                }
            }

            return false;
        }

        // ====================================================================
        // SpellData lookup
        // ====================================================================
        static const EzEvade::SpellData* LookupSpellData(
            const std::string& missileName,
            const std::string& spellName,
            const std::string& objectName) {
            if (!missileName.empty()) {
                if (const auto* byMissile = EzEvade::GetSpellByMissile(missileName)) {
                    return byMissile;
                }
            }

            if (!spellName.empty()) {
                if (const auto* bySpell = EzEvade::GetSpellByName(spellName)) {
                    return bySpell;
                }
                if (const auto* byMissile = EzEvade::GetSpellByMissile(spellName)) {
                    return byMissile;
                }
            }

            if (!objectName.empty()) {
                if (const auto* byObject = EzEvade::GetSpellByMissile(objectName)) {
                    return byObject;
                }
                if (const auto* byObjectSpell = EzEvade::GetSpellByName(objectName)) {
                    return byObjectSpell;
                }
            }

            // FALLBACK: GetSpellName() sometimes returns just the champion name
            // (e.g. "Lux" instead of "LuxLightBinding"). Try matching by charName.
            // Pick the first missile-type spell for that champion.
            auto tryMatchByChampion = [](const std::string& name) -> const EzEvade::SpellData* {
                if (name.empty()) return nullptr;
                auto spells = EzEvade::GetSpellsForChampion(name);
                // Prefer missile spells (most likely what produced this missile object)
                for (const auto* s : spells) {
                    if (s->isMissile && s->IsSkillshot()) return s;
                }
                // Otherwise any skillshot from that champion
                for (const auto* s : spells) {
                    if (s->IsSkillshot()) return s;
                }
                return nullptr;
            };

            if (const auto* byChamp = tryMatchByChampion(spellName)) {
                return byChamp;
            }
            if (const auto* byChamp = tryMatchByChampion(objectName)) {
                return byChamp;
            }

            return nullptr;
        }

        // ====================================================================
        // Position resolution
        // ====================================================================
        static Vec3 ResolveStartPosition(const SDK::Missile& missile) {
            Vec3 startPos = missile.GetStartPos();
            if (!startPos.IsZero()) {
                return startPos;
            }
            
            startPos = missile.GetPosition();
            if (!startPos.IsZero()) {
                return startPos;
            }
            
            auto caster = SDK::ObjectManager::GetObjectByNetId(missile.GetCasterNetId());
            if (caster.IsValid()) {
                startPos = caster.GetPosition();
                if (!startPos.IsZero()) {
                    return startPos;
                }
            }

            return missile.GetCastEndPos();
        }

        static Vec3 ResolveEndPosition(const EzEvade::SpellData& spell, const Vec3& startPos, const Vec3& rawEnd) {
            if (startPos.IsZero()) {
                return rawEnd;
            }

            Vec3 endPos = rawEnd;
            if (endPos.IsZero()) {
                return startPos;
            }

            const float maxRange = spell.range + spell.extraRange;
            if (maxRange <= 0.0f) {
                return endPos;
            }

            Vec2 start2D = startPos.To2D();
            Vec2 end2D = endPos.To2D();
            Vec2 dir = end2D - start2D;
            const float dist = dir.Length();
            if (dist <= 1.0f) {
                return endPos;
            }

            if (spell.fixedRange || dist > maxRange) {
                end2D = start2D + dir.Normalized() * maxRange;
                endPos.x = end2D.x;
                endPos.z = end2D.y;
            }

            return endPos;
        }

        static Vec3 ResolveMissileEndPosition(const EzEvade::SpellData& spell, const Vec3& startPos, const SDK::Missile& missile) {
            Vec3 endPos = missile.GetEndPos();
            if (endPos.IsZero()) {
                endPos = missile.GetCastEndPos();
            }
            if (endPos.IsZero()) {
                endPos = missile.GetPosition();
            }
            return ResolveEndPosition(spell, startPos, endPos);
        }

        static float ResolveLifetime(const EzEvade::SpellData& spell, const Vec3& startPos, const Vec3& endPos, bool hasMissile) {
            const float dist = startPos.Distance2D(endPos);
            float travelTime = 0.35f;
            if (spell.speed > 1.0f && dist > 1.0f) {
                travelTime = dist / spell.speed;
            }

            const float linger = hasMissile ? 0.35f : 0.65f;
            return std::max(0.50f, spell.castDelay + travelTime + linger);
        }

        static TrackedSkillshot BuildMissileSkillshot(const EzEvade::SpellData& spell, const SDK::Missile& missile, int casterNetId, float now) {
            TrackedSkillshot tracked;
            tracked.Data = &spell;
            tracked.ChampionName = ResolveChampionName(casterNetId, spell.charName);
            tracked.MenuSpellId = EvadeMenu::GetSkillshotMenuId(spell);
            tracked.SpellName = spell.spellName;
            tracked.StartPos = ResolveStartPosition(missile);
            tracked.EndPos = ResolveMissileEndPosition(spell, tracked.StartPos, missile);
            tracked.CurrentPos = ResolveMissileCurrentPos(missile, tracked);
            tracked.StartTime = now;
            tracked.LastSeenTime = now;
            tracked.ExpireTime = now + ResolveLifetime(spell, tracked.StartPos, tracked.EndPos, true);
            tracked.CasterNetId = casterNetId;
            tracked.MissileNetId = missile.GetNetworkId();
            tracked.HasMissile = true;
            tracked.Active = true;
            return tracked;
        }

        static std::string ResolveChampionName(int casterNetId, const std::string& fallback) {
            for (const auto& enemy : SDK::GameObjects::EnemyHeroes) {
                if (enemy.IsValid() && enemy.GetNetId() == casterNetId) {
                    return enemy.GetChampionName();
                }
            }
            return fallback;
        }

        static Vec3 ResolveMissileCurrentPos(const SDK::Missile& missile, const TrackedSkillshot& tracked) {
            Vec3 current = missile.GetPosition();
            if (!current.IsZero()) {
                return current;
            }
            return tracked.StartPos;
        }

        static void AddOrUpdateTracked(const TrackedSkillshot& tracked) {
            if (tracked.Data == nullptr) {
                return;
            }

            for (auto& existing : s_tracked) {
                if (tracked.HasMissile && existing.MissileNetId != 0 && existing.MissileNetId == tracked.MissileNetId) {
                    existing = tracked;
                    return;
                }

                if (!tracked.HasMissile &&
                    !existing.HasMissile &&
                    existing.CasterNetId == tracked.CasterNetId &&
                    existing.SpellName == tracked.SpellName &&
                    std::fabs(existing.StartTime - tracked.StartTime) <= 0.10f) {
                    existing = tracked;
                    return;
                }
            }

            s_tracked.push_back(tracked);
        }

        // ====================================================================
        // Menu-driven visibility checks
        // ====================================================================
        static bool ShouldDrawSkillshot(SDK::MenuUI::Menu* rootMenu, const TrackedSkillshot& tracked) {
            auto* skillshotRoot = rootMenu->GetSubMenu("SkillshotSettings");
            if (skillshotRoot == nullptr) {
                return true;
            }

            auto* championMenu = skillshotRoot->GetSubMenu(EvadeMenu::NormalizeId(tracked.ChampionName));
            if (championMenu == nullptr) {
                return true;
            }

            auto* spellMenu = championMenu->GetSubMenu(tracked.MenuSpellId);
            if (spellMenu == nullptr) {
                return true;
            }

            return spellMenu->GetBoolValue("Draw", true);
        }

        static int GetExtraRadius(SDK::MenuUI::Menu* rootMenu, const TrackedSkillshot& tracked) {
            auto* skillshotRoot = rootMenu->GetSubMenu("SkillshotSettings");
            if (skillshotRoot == nullptr) {
                return 0;
            }

            auto* championMenu = skillshotRoot->GetSubMenu(EvadeMenu::NormalizeId(tracked.ChampionName));
            if (championMenu == nullptr) {
                return 0;
            }

            auto* spellMenu = championMenu->GetSubMenu(tracked.MenuSpellId);
            if (spellMenu == nullptr) {
                return 0;
            }

            return spellMenu->GetSliderValue("ExtraRadius", 0);
        }

        static bool GetDebugBool(SDK::MenuUI::Menu* rootMenu, const std::string& name) {
            auto* debugMenu = rootMenu->GetSubMenu("Debug");
            if (debugMenu == nullptr) {
                return false;
            }
            return debugMenu->GetBoolValue(name, false);
        }

        // ====================================================================
        // Drawing helpers
        // ====================================================================
        static ImU32 ResolveColor(const EzEvade::SpellData& spell) {
            switch (spell.dangerLevel) {
            case 5: return IM_COL32(255, 70, 160, 235);
            case 4: return IM_COL32(255, 90, 90, 235);
            case 3: return IM_COL32(255, 170, 70, 225);
            case 2: return IM_COL32(90, 190, 255, 220);
            default: return IM_COL32(120, 220, 120, 210);
            }
        }

        static ImU32 ApplyAlpha(ImU32 color, int alpha) {
            return (color & 0x00FFFFFF) | (static_cast<ImU32>(std::clamp(alpha, 0, 255)) << 24);
        }

        static float ResolveRadius(const TrackedSkillshot& tracked, float extraRadius) {
            return std::max(1.0f, tracked.Data->radius + extraRadius);
        }

        static float ResolveRange(const TrackedSkillshot& tracked) {
            const float dist = tracked.StartPos.Distance2D(tracked.EndPos);
            if (tracked.Data->range > 1.0f) {
                return tracked.Data->range + tracked.Data->extraRange;
            }
            return std::max(1.0f, dist);
        }

        static float ResolveAngleRadians(const TrackedSkillshot& tracked) {
            if (tracked.Data->angle > 0) {
                return tracked.Data->angle * 0.01745329251f;
            }
            return 0.78539816339f;
        }

        static Vec3 GetLabelPosition(const TrackedSkillshot& tracked) {
            if (tracked.HasMissile && !tracked.CurrentPos.IsZero()) {
                return tracked.CurrentPos;
            }
            if (!tracked.EndPos.IsZero()) {
                return tracked.EndPos;
            }
            return tracked.StartPos;
        }

        static bool DrawSkillshotShape(const TrackedSkillshot& tracked, float extraRadius, ImU32 color, ImU32 fillColor) {
            const float radius = ResolveRadius(tracked, extraRadius);
            const float drawY = !tracked.StartPos.IsZero() ? tracked.StartPos.y : tracked.EndPos.y;

            // EzEvade-style: use a brighter fill for better visibility
            const ImU32 edgeColor = color;
            const ImU32 solidFill = ApplyAlpha(color, 55);       // 55 alpha fill
            const ImU32 trailFill = ApplyAlpha(color, 20);       // dimmer trail
            const float borderWidth = 2.5f;

            switch (tracked.Data->type) {
            case EzEvade::SkillshotType::Line:
            case EzEvade::SkillshotType::MissileLine: {
                // ====================================================
                // EzEvade Style: DrawLineRectangle(currentPos, endPos, radius)
                // Draw main rectangle from current missile pos → end
                // Draw trail rectangle from start → current pos (dimmer)
                // ====================================================
                Vec3 spellPos = tracked.HasMissile && !tracked.CurrentPos.IsZero() ? tracked.CurrentPos : tracked.StartPos;
                Vec3 spellEndPos = tracked.EndPos;

                if (spellPos.IsZero() || spellEndPos.IsZero() || spellPos.Distance2D(spellEndPos) <= 2.0f) {
                    return false;
                }

                // Helper lambda: draw a line rectangle (EzEvade's DrawLineRectangle)
                auto drawLineRect = [&](const Vec3& from, const Vec3& to, float halfWidth, ImU32 border, ImU32 fill, float thick) {
                    Vec2 dir2D = Vec2(to.x - from.x, to.z - from.z);
                    float len = dir2D.Length();
                    if (len < 1.0f) return;
                    dir2D = dir2D * (1.0f / len);
                    Vec2 perp(-dir2D.y, dir2D.x);

                    Vec3 corners[4] = {
                        Vec3(from.x + perp.x * halfWidth, drawY, from.z + perp.y * halfWidth),
                        Vec3(from.x - perp.x * halfWidth, drawY, from.z - perp.y * halfWidth),
                        Vec3(to.x   - perp.x * halfWidth, drawY, to.z   - perp.y * halfWidth),
                        Vec3(to.x   + perp.x * halfWidth, drawY, to.z   + perp.y * halfWidth)
                    };

                    // Draw 4 edges (EzEvade's DrawLineRectangle)
                    SDK::Drawing::DrawLine3D(corners[0], corners[3], border, thick);
                    SDK::Drawing::DrawLine3D(corners[1], corners[2], border, thick);
                    SDK::Drawing::DrawLine3D(corners[0], corners[1], border, thick);
                    SDK::Drawing::DrawLine3D(corners[2], corners[3], border, thick);

                    // Fill polygon
                    if ((fill & 0xFF000000) != 0) {
                        SDK::RectanglePoly rect(from, to, halfWidth);
                        SDK::Drawing::DrawPolygon(rect.Points, drawY, 0, 0, fill);
                    }
                };

                // Main area: current pos → end pos (full color)
                drawLineRect(spellPos, spellEndPos, radius, edgeColor, solidFill, borderWidth);

                // Trail area: start → current pos (dim, shows where missile has been)
                if (tracked.HasMissile && !tracked.CurrentPos.IsZero() && !tracked.StartPos.IsZero()) {
                    float trailDist = tracked.StartPos.Distance2D(tracked.CurrentPos);
                    if (trailDist > 10.0f) {
                        drawLineRect(tracked.StartPos, tracked.CurrentPos, radius, ApplyAlpha(edgeColor, 80), trailFill, 1.5f);
                    }
                }

                // EzEvade: Draw spell position indicator circle (small circle at missile head)
                if (tracked.HasMissile && !tracked.CurrentPos.IsZero()) {
                    SDK::Drawing::DrawCircle(tracked.CurrentPos, std::max(25.0f, radius * 0.6f), edgeColor, borderWidth);
                }
                return true;
            }
            case EzEvade::SkillshotType::Circle: {
                // ====================================================
                // EzEvade: Render.Circle.DrawCircle(endPos, radius)
                // ====================================================
                if (tracked.EndPos.IsZero()) {
                    return false;
                }

                // Draw border circle with fill
                SDK::Drawing::DrawCircle(tracked.EndPos, radius, edgeColor, borderWidth);

                // Fill circle
                SDK::CirclePoly circle(tracked.EndPos, radius, 32);
                SDK::Drawing::DrawPolygon(circle.Points, tracked.EndPos.y, 0, 0, solidFill);

                // Special case: Ring-type circles
                if (tracked.SpellName == "VeigarEventHorizon" && radius > 125.0f) {
                    SDK::Drawing::DrawCircle(tracked.EndPos, radius - 125.0f, edgeColor, borderWidth);
                } else if (tracked.SpellName == "DariusCleave" && radius > 220.0f) {
                    SDK::Drawing::DrawCircle(tracked.EndPos, radius - 220.0f, edgeColor, borderWidth);
                }
                return true;
            }
            case EzEvade::SkillshotType::Cone: {
                // ====================================================
                // EzEvade: DrawLineTriangle(startPos, endPos, radius)
                // ====================================================
                if (tracked.StartPos.IsZero() || tracked.EndPos.IsZero()) {
                    return false;
                }

                Vec2 dir2D = Vec2(tracked.EndPos.x - tracked.StartPos.x, tracked.EndPos.z - tracked.StartPos.z);
                float len = dir2D.Length();
                if (len < 1.0f) return false;
                dir2D = dir2D * (1.0f / len);
                Vec2 perp(-dir2D.y, dir2D.x);

                Vec3 apex(tracked.StartPos.x, drawY, tracked.StartPos.z);
                Vec3 rightEnd(tracked.EndPos.x + perp.x * radius, drawY, tracked.EndPos.z + perp.y * radius);
                Vec3 leftEnd(tracked.EndPos.x - perp.x * radius, drawY, tracked.EndPos.z - perp.y * radius);

                // Draw 3 edges of triangle
                SDK::Drawing::DrawLine3D(apex, rightEnd, edgeColor, borderWidth);
                SDK::Drawing::DrawLine3D(apex, leftEnd, edgeColor, borderWidth);
                SDK::Drawing::DrawLine3D(rightEnd, leftEnd, edgeColor, borderWidth);

                // Fill sector
                SDK::SectorPoly sector(tracked.StartPos, tracked.EndPos, ResolveAngleRadians(tracked), ResolveRange(tracked), 28);
                SDK::Drawing::DrawPolygon(sector.Points, drawY, 0, 0, solidFill);
                return true;
            }
            case EzEvade::SkillshotType::Ring: {
                // ====================================================
                // Ring: outer + inner circle
                // ====================================================
                if (tracked.EndPos.IsZero()) {
                    return false;
                }

                SDK::Drawing::DrawCircle(tracked.EndPos, radius, edgeColor, borderWidth);

                // Inner ring (default gap = 125 for standard ring spells like Veigar E)
                float innerRadius = radius - 125.0f;
                if (innerRadius > 10.0f) {
                    SDK::Drawing::DrawCircle(tracked.EndPos, innerRadius, edgeColor, borderWidth);
                }

                // Fill ring area (between outer and inner)
                SDK::CirclePoly outerCircle(tracked.EndPos, radius, 32);
                SDK::Drawing::DrawPolygon(outerCircle.Points, tracked.EndPos.y, 0, 0, solidFill);
                return true;
            }
            case EzEvade::SkillshotType::Arc:
            case EzEvade::SkillshotType::MissileArc: {
                // ====================================================
                // Arc: circle at midpoint + direction line
                // ====================================================
                if (tracked.StartPos.IsZero() || tracked.EndPos.IsZero()) {
                    return false;
                }

                Vec3 midPoint;
                midPoint.x = (tracked.StartPos.x + tracked.EndPos.x) / 2.0f;
                midPoint.y = drawY;
                midPoint.z = (tracked.StartPos.z + tracked.EndPos.z) / 2.0f;

                // Midpoint circle with fill
                SDK::Drawing::DrawCircle(midPoint, radius, edgeColor, borderWidth);
                SDK::CirclePoly arcCircle(midPoint, radius, 28);
                SDK::Drawing::DrawPolygon(arcCircle.Points, drawY, 0, 0, solidFill);

                // Direction line from start to end
                SDK::Drawing::DrawLine3D(tracked.StartPos, tracked.EndPos, ApplyAlpha(edgeColor, 150), 1.5f);
                return true;
            }
            default:
                return false;
            }
        }
    };

} // namespace Plugins::Evade
