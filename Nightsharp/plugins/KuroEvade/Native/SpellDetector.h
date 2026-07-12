#pragma once


#include "SpecialSpells/SpecialSpellProcessor.h"
#include "SpellDatabase.h"
#include "EvadeUtils.h"

#include "../../../Core/CoreNavGrid.h"
#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Plugins::KuroEvade {

class SpellDetector {
public:
    using SkillshotPtr = std::shared_ptr<SDK::Skillshot>;
    using SkillshotList = std::vector<SkillshotPtr>;
    using SpellEnabledPredicate = std::function<bool(const Generated::SpellDataEntry&)>;

    void SetSpellEnabledPredicate(SpellEnabledPredicate predicate) {
        m_spellEnabledPredicate = std::move(predicate);
    }

    void SetCollisionEnabled(bool enabled) {
        m_checkCollisions = enabled;
    }

    void SetFowEnabled(bool enabled) {
        m_dodgeFow = enabled;
    }

    void SetDevSameTeam(bool enabled) {
        m_devSameTeam = enabled;
    }

    void AddSimulatedSkillshot(const SkillshotPtr& skillshot, const Generated::SpellDataEntry& data) {
        AddSkillshot(skillshot, data, true);
    }

    void Clear() {
        m_skillshots.clear();
        m_traps.clear();
        m_originalEnds.clear();
        m_skillshotData.clear();
        m_lastCollisionTick = 0;
        SpecialSpells::ClearState();
    }

    void Update() {
        SpecialSpells::BeginUpdate();
        m_skillshots.erase(
            std::remove_if(m_skillshots.begin(), m_skillshots.end(), [&](const SkillshotPtr& skillshot) {
                if (skillshot) {
                    UpdateFollowCaster(*skillshot);
                    UpdateMissileFollowsUnit(*skillshot);
                }
                return !skillshot ||
                       !SpecialSpells::UpdateSkillshot(*skillshot) ||
                       IsExpired(skillshot);
            }),
            m_skillshots.end());
        SpecialSpells::EndUpdate();

        for (const auto& skillshot : m_skillshots) {
            if (skillshot) {
                skillshot->Game_OnUpdate();
            }
        }

        const int now = SDK::Variables::TickCount();
        if (m_checkCollisions && now - m_lastCollisionTick >= 100) {
            CheckSpellCollisions();
            m_lastCollisionTick = now;
        }
        CleanupAuxiliaryState();
    }

    SkillshotList& Skillshots() {
        return m_skillshots;
    }

    const SkillshotList& Skillshots() const {
        return m_skillshots;
    }

    int RemoveAtPosition(const Vec2& position, float extraRadius = 50.0f) {
        int removed = 0;
        m_skillshots.erase(
            std::remove_if(m_skillshots.begin(), m_skillshots.end(), [&](const SkillshotPtr& skillshot) {
                if (!skillshot ||
                    (!IsPersistentTrap(skillshot) && skillshot->SData.Range <= 9000) ||
                    !ContainsPosition(*skillshot, position, extraRadius)) {
                    return false;
                }
                ++removed;
                return true;
            }),
            m_skillshots.end());
        if (removed > 0) {
            CleanupAuxiliaryState();
        }
        return removed;
    }

    static const Generated::SpellDataEntry* LookupBySpellName(const char* name) {
        return FindBySpellName(name);
    }

    void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
        SDK::AIBaseClient caster = MakeCaster(args.Sender);
        if (caster.IsValid() && caster.IsHero() && !args.IsAutoAttack) {
            SDK::Orbwalker::DebugPrint("[SpellDetector] Caster: %s | Spell: %s | Slot: %d",
                                       EvadeUtils::GetObjectCharacterName(caster).c_str(), args.SpellName, args.Slot);
        }

        if (!caster.IsValid() || (caster.IsAlly() && !m_devSameTeam)) {
            return;
        }

        const auto* data = FindProcessSpellData(args, caster);
        if (!data || data->UsePacket || !IsSpellEnabled(*data)) {
            return;
        }
        auto specialResult = SpecialSpells::ProcessCast(
            caster, args, *data, &SpellDetector::LookupBySpellName);

        for (const std::string& spellName : specialResult.RemoveSpellNames) {
            RemoveDetectedSpell(caster.NetworkId(), spellName.c_str());
        }

        for (const auto& extra : specialResult.ExtraSpells) {
            if (!IsSpellEnabled(extra.Data)) {
                continue;
            }
            CreateSpellData(caster,
                            extra.Start,
                            extra.End,
                            extra.Data,
                            SDK::SkillshotDetectionType::ProcessSpell,
                            SDK::MissileClient(),
                            extra.OverrideStartTick);
        }

        if (specialResult.NoProcess) {
            return;
        }

        if (specialResult.Data.NoTarget &&
            args.TargetNetworkId != 0 &&
            args.TargetNetworkId == static_cast<std::uint32_t>(caster.NetworkId())) {
            return;
        }

        const auto player = SDK::ObjectManager::Player();
        const Vec2 resolvedStart = ResolveStartPosition(caster, args.StartPosition.To2D());
        if (resolvedStart.IsZero()) {
            return;
        }
        const Vec2 resolvedEnd = ResolveEndPosition(
            specialResult.Data,
            caster,
            resolvedStart,
            args.EndPosition.To2D(),
            args.CastPosition.To2D(),
            player.IsValid() ? player.ServerPosition().To2D() : Vec2());
        const Vector3 startWorld = Vec3::From2D(resolvedStart, args.StartPosition.y);
        const Vector3 endWorld = Vec3::From2D(resolvedEnd, args.EndPosition.y);

        if (specialResult.Data.MultipleNumber != -1 && specialResult.Data.MultipleNumber > 1) {
            Vec2 baseDirection = ResolveDirection(caster, resolvedStart, resolvedEnd, player);

            const int half = (specialResult.Data.MultipleNumber - 1) / 2;
            for (int i = -half; i <= half; ++i) {
                const Vec2 direction = SDK::Prediction::Vec2Ext::Rotated(baseDirection, specialResult.Data.MultipleAngle * static_cast<float>(i));
                const Vector3 end = Vec3::From2D(
                    resolvedStart + direction * static_cast<float>(specialResult.Data.sdk.Range),
                    args.EndPosition.y);
                CreateSpellData(caster, startWorld, end, specialResult.Data,
                                SDK::SkillshotDetectionType::ProcessSpell,
                                SDK::MissileClient(), 0, true);
            }
            return;
        }

        CreateSpellData(caster, startWorld, endWorld, specialResult.Data, SDK::SkillshotDetectionType::ProcessSpell);
    }

    void OnProcessCastSpell(const SDK::Events::CastSpellEventArgs& args) {
        if (!args.Sender.IsValid()) {
            return;
        }
        const auto caster = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
            static_cast<int>(args.Sender.NetworkId));
        if (caster.IsValid() && caster.IsHero()) {
            SDK::Orbwalker::DebugPrint("[SpellDetector][CastSpell] Caster: %s | Slot: %d",
                                       EvadeUtils::GetObjectCharacterName(caster).c_str(), args.Slot);
        }

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || args.Sender.NetworkId == static_cast<uint32_t>(player.NetworkId())) {
            return;
        }
        if (caster.IsValid() && caster.IsAlly() && !m_devSameTeam) {
            return;
        }
        const auto* data = FindUniqueByChampionAndSlot(EvadeUtils::GetObjectCharacterName(caster).c_str(), args.Slot);
        if (!data && caster.IsValid()) {
            data = FindUniqueByChampionAndSlot(EvadeUtils::GetObjectCharacterName(caster).c_str(), args.Slot);
        }
        if (!data || !IsSpellEnabled(*data)) {
            return;
        }
        Vec2 startPos = args.StartPosition.To2D();
        if (startPos.IsZero() && caster.IsValid()) {
            startPos = caster.Position().To2D();
        }
        if (startPos.IsZero()) {
            return;
        }
        Vec2 endPos = ResolveEndPosition(
            *data,
            caster,
            startPos,
            args.EndPosition.To2D(),
            Vec2(),
            player.ServerPosition().To2D());
        if (endPos.IsZero()) {
            return;
        }
        CreateSpellData(caster,
                        Vec3::From2D(startPos, args.StartPosition.y),
                        Vec3::From2D(endPos, args.EndPosition.y),
                        *data,
                        SDK::SkillshotDetectionType::ProcessSpell);
    }

    void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
        SDK::MissileClient missile(args.Sender.Ptr);
        if (!missile.IsValid()) {
            return;
        }

        const char* eventName = args.MissileName[0] ? args.MissileName : args.SpellName;
        const std::string runtimeName = missile.SpellName();
        const char* missileName = eventName[0] ? eventName : runtimeName.c_str();

        if (IsBasicAttackName(missileName) ||
            IsBasicAttackName(args.SpellName) ||
            IsBasicAttackName(runtimeName.c_str())) {
            return;
        }

        SDK::AIBaseClient caster = MakeCaster(args.Source);
        if (!caster.IsValid() && missile.CasterNetworkId() != 0) {
            caster = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(missile.CasterNetworkId());
        }
        if (caster.IsValid() && caster.IsHero()) {
            SDK::Orbwalker::DebugPrint("[SpellDetector][Missile] Caster: %s | Missile: %s | Spell: %s",
                                       EvadeUtils::GetObjectCharacterName(caster).c_str(), missileName, args.SpellName);
        }

        const auto* data = FindByMissileName(missileName);
        if (!data && args.SpellName[0]) {
            data = FindBySpellName(args.SpellName);
        }
        if (!data && !runtimeName.empty()) {
            data = FindByMissileName(runtimeName.c_str());
        }
        if (!data || !IsSpellEnabled(*data)) {
            return;
        }


        if (!caster.IsValid() || (caster.IsAlly() && !m_devSameTeam)) {
            return;
        }
        if (!caster.IsVisible() && !m_dodgeFow) {
            return;
        }

        Vector3 start = args.StartPosition.IsZero() ? missile.StartPosition() : args.StartPosition;
        Vector3 end = args.EndPosition.IsZero() ? missile.EndPosition() : args.EndPosition;
        if (start.IsZero()) {
            start = caster.Position();
        }
        if (end.IsZero()) {
            end = start + caster.Direction() * static_cast<float>(std::max(1, data->sdk.Range));
        }

        Generated::SpellDataEntry missileData = *data;
        SpecialSpells::ProcessMissile(caster, missile, missileData);

        const int speed = std::max(1, missileData.sdk.MissileSpeed);
        int startTick = SDK::Variables::TickCount() - SDK::Game::Ping() / 2;
        if (!missileData.UsePacket) {
            startTick -= missileData.sdk.Delay;
        }
        startTick -= static_cast<int>(
            1000.0f * missile.Position().Distance(start) / static_cast<float>(speed));

        CreateSpellData(caster, start, end, missileData, SDK::SkillshotDetectionType::MissileCreate, missile, startTick);
    }

    void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
        m_skillshots.erase(
            std::remove_if(m_skillshots.begin(), m_skillshots.end(), [&](const SkillshotPtr& skillshot) {
                auto missileSkillshot = std::dynamic_pointer_cast<SDK::SkillshotMissile>(skillshot);
                if (!missileSkillshot || !missileSkillshot->Missile.IsValid()) {
                    return false;
                }

                const int missileNetworkId = missileSkillshot->Missile.NetworkId();
                return missileNetworkId == static_cast<int>(args.MissileNetworkId) ||
                       missileNetworkId == static_cast<int>(args.Sender.NetworkId);
            }),
            m_skillshots.end());
    }

    void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
        if (!args.Sender.Ptr) {
            return;
        }

        SDK::GameObject object(args.Sender.Ptr, args.Sender.Type);
        if (object.IsValid() && (!object.IsAlly() || m_devSameTeam)) {
            SDK::Orbwalker::DebugPrint("[SpellDetector][Object] Name: %s | CharName: %s",
                                       EvadeUtils::GetObjectName(object).c_str(), EvadeUtils::GetObjectCharacterName(object).c_str());
        }

        if (!object.IsValid() || (object.IsAlly() && !m_devSameTeam)) {
            return;
        }

        const int objectId = object.NetworkId() != 0
            ? object.NetworkId()
            : static_cast<int>(args.Sender.NetworkId);
        if (objectId == 0 || m_traps.find(objectId) != m_traps.end()) {
            return;
        }

        const std::string objectName = EvadeUtils::GetObjectName(object);
        const std::string characterName = EvadeUtils::GetObjectCharacterName(object);
        const auto* data = FindTrapData(objectName, characterName);
        if (!data || !IsSpellEnabled(*data)) {
            return;
        }

        SDK::AIBaseClient caster;
        for (const auto& enemy : SDK::GameObjects::EnemyHeroes()) {
            if (enemy.IsValid() &&
                _stricmp(EvadeUtils::GetObjectCharacterName(enemy).c_str(), data->sdk.ChampionName.c_str()) == 0) {
                caster = SDK::AIBaseClient(enemy.Handle());
                break;
            }
        }

        const Vector3 position = object.Position();
        auto trap = CreateSpellData(
            caster,
            position,
            position,
            *data,
            SDK::SkillshotDetectionType::MissileCreate,
            SDK::MissileClient(),
            SDK::Variables::TickCount(),
            true);
        if (trap) {
            m_traps[objectId] = trap;
        }
    }

    void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
        int objectId = static_cast<int>(args.Sender.NetworkId);
        if (objectId == 0 && args.Sender.Ptr) {
            objectId = SDK::GameObject(args.Sender.Ptr, args.Sender.Type).NetworkId();
        }

        const auto it = m_traps.find(objectId);
        if (it == m_traps.end()) {
            return;
        }

        const SkillshotPtr trap = it->second;
        m_traps.erase(it);
        m_skillshots.erase(
            std::remove(m_skillshots.begin(), m_skillshots.end(), trap),
            m_skillshots.end());
        if (trap) {
            m_originalEnds.erase(trap.get());
        }
    }

private:
    SkillshotList m_skillshots;
    SpellEnabledPredicate m_spellEnabledPredicate;
    std::unordered_map<int, SkillshotPtr> m_traps;
    std::unordered_map<const SDK::Skillshot*, Vec2> m_originalEnds;
    std::unordered_map<const SDK::Skillshot*, Generated::SpellDataEntry> m_skillshotData;
    int m_lastCollisionTick = 0;
    bool m_checkCollisions = false;
    bool m_dodgeFow = true;
    bool m_devSameTeam = false;

    bool IsSpellEnabled(const Generated::SpellDataEntry& data) const {
        return !m_spellEnabledPredicate || m_spellEnabledPredicate(data);
    }

    static bool IsFiniteMissileSpeed(const SDK::SpellDatabaseEntry& data) {
        return data.MissileSpeed > 0 && data.MissileSpeed != INT_MAX;
    }

    static bool UsesSourceObject(const SDK::SpellDatabaseEntry& data) {
        return !data.FromObject.empty() || !data.FromObjects.empty();
    }

    static Vec2 ResolveStartPosition(const SDK::AIBaseClient& caster, const Vec2& rawStart) {
        if (!rawStart.IsZero()) {
            return rawStart;
        }
        if (caster.IsValid()) {
            return caster.Position().To2D();
        }
        return {};
    }

    static Vec2 ResolveDirection(const SDK::AIBaseClient& caster,
                                 const Vec2& start,
                                 const Vec2& end,
                                 const SDK::AIHeroClient& player = SDK::AIHeroClient()) {
        Vec2 direction = (end - start).Normalized();
        if (direction.IsZero() && caster.IsValid()) {
            direction = caster.Direction().To2D().Normalized();
        }
        if (direction.IsZero() && player.IsValid()) {
            direction = (player.ServerPosition().To2D() - start).Normalized();
        }
        if (direction.IsZero()) {
            direction = Vec2(1.0f, 0.0f);
        }
        return direction;
    }

    static Vec2 ResolveEndPosition(const Generated::SpellDataEntry& data,
                                   const SDK::AIBaseClient& caster,
                                   const Vec2& start,
                                   const Vec2& primaryEnd,
                                   const Vec2& secondaryEnd,
                                   const Vec2& heroPos) {
        Vec2 end = primaryEnd;
        if (SDK::IsCircleSpellType(data.sdk.SpellType)) {
            if (!secondaryEnd.IsZero()) {
                end = secondaryEnd;
            }
        }
        if (end.IsZero()) {
            end = secondaryEnd;
        }
        if (end.IsZero()) {
            end = start;
        }

        if (SDK::IsLineSpellType(data.sdk.SpellType)) {
            const SDK::AIHeroClient player = SDK::ObjectManager::Player();
            const Vec2 direction = ResolveDirection(caster, start, end, player);
            return start + direction * static_cast<float>(std::max(1, data.sdk.Range));
        }

        return end;
    }

    static bool SameText(const std::string& lhs, const char* rhs) {
        return rhs && !lhs.empty() && _stricmp(lhs.c_str(), rhs) == 0;
    }

    static bool SameText(const std::string& lhs, const std::string& rhs) {
        return !lhs.empty() && !rhs.empty() && _stricmp(lhs.c_str(), rhs.c_str()) == 0;
    }

    static bool ContainsText(const std::vector<std::string>& values, const char* name) {
        if (!name || !name[0]) {
            return false;
        }
        for (const std::string& value : values) {
            if (SameText(value, name)) {
                return true;
            }
        }
        return false;
    }

    static bool ContainsInsensitive(const char* text, const char* value) {
        if (!text || !value || !text[0] || !value[0]) {
            return false;
        }

        std::string haystack(text);
        std::string needle(value);
        std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return haystack.find(needle) != std::string::npos;
    }

    static bool IsBasicAttackName(const char* name) {
        return ContainsInsensitive(name, "basicattack") || ContainsInsensitive(name, "attack");
    }

    static const Generated::SpellDataEntry* FindProcessSpellData(
        const SDK::Events::ProcessSpellEventArgs& args,
        const SDK::AIBaseClient& caster) {
        if (args.IsAutoAttack || IsBasicAttackName(args.SpellName) || IsBasicAttackName(args.PayloadSpellName)) {
            return nullptr;
        }

        const char* names[] = {
            args.SpellName,
            args.PayloadSpellName,
            args.ScriptName,
            args.SpellSlotName,
            args.MissileName,
            args.PayloadMissileName,
        };

        bool sawBasicAttackName = false;
        bool sawNonBasicName = false;
        for (const char* name : names) {
            if (!name || !name[0]) {
                continue;
            }
            if (IsBasicAttackName(name)) {
                sawBasicAttackName = true;
                continue;
            }
            sawNonBasicName = true;
            if (const auto* data = FindBySpellName(name)) {
                return data;
            }
            if (const auto* data = FindByMissileName(name)) {
                return data;
            }
        }

        if (args.IsAutoAttack || (sawBasicAttackName && !sawNonBasicName) ||
            args.Slot < 0 || args.Slot > 3) {
            return nullptr;
        }
        // Disabled unique slot fallback to prevent inaccurate ghost skillshot detection
        return nullptr;
    }

    static bool MatchesRegexInsensitive(const std::string& text, const std::string& pattern) {
        if (text.empty() || pattern.empty()) {
            return false;
        }
        try {
            return std::regex_search(text, std::regex(pattern, std::regex_constants::icase));
        } catch (const std::regex_error&) {
            return ContainsInsensitive(text.c_str(), pattern.c_str());
        }
    }

    static const Generated::SpellDataEntry* FindTrapData(const std::string& objectName,
                                                         const std::string& characterName) {
        for (const auto& entry : SpellDatabase::Spells()) {
            if (!entry.HasTrap) {
                continue;
            }
            if (!entry.TrapBaseName.empty() &&
                (ContainsInsensitive(objectName.c_str(), entry.TrapBaseName.c_str()) ||
                 ContainsInsensitive(characterName.c_str(), entry.TrapBaseName.c_str()))) {
                return &entry;
            }
            if (!entry.TrapTroyName.empty() &&
                (MatchesRegexInsensitive(objectName, entry.TrapTroyName) ||
                 MatchesRegexInsensitive(characterName, entry.TrapTroyName))) {
                return &entry;
            }
        }
        return nullptr;
    }

    const Generated::SpellDataEntry* FindSkillshotData(const SDK::Skillshot& skillshot) const {
        const auto cached = m_skillshotData.find(&skillshot);
        if (cached != m_skillshotData.end()) {
            return &cached->second;
        }

        const std::string casterName = skillshot.Caster.IsValid()
            ? EvadeUtils::GetObjectCharacterName(skillshot.Caster)
            : std::string();
        for (const auto& entry : SpellDatabase::Spells()) {
            if (!SameText(entry.sdk.SpellName, skillshot.SData.SpellName)) {
                continue;
            }
            if (!casterName.empty() &&
                _stricmp(entry.sdk.ChampionName.c_str(), "AllChampions") != 0 &&
                _stricmp(entry.sdk.ChampionName.c_str(), casterName.c_str()) != 0) {
                continue;
            }
            return &entry;
        }
        return nullptr;
    }

    bool IsExpired(const SkillshotPtr& skillshot) const {
        if (!skillshot) {
            return true;
        }
        if (IsPersistentTrap(skillshot)) {
            return false;
        }
        if (!skillshot->HasExpired()) {
            return false;
        }

        const auto* data = FindSkillshotData(*skillshot);
        const float extraEndTime = data ? std::max(0.0f, data->ExtraEndTime) : 0.0f;
        if (extraEndTime <= 0.0f) {
            return true;
        }

        int baseEndTick = skillshot->StartTime + 5000;
        if (skillshot->SData.MissileAccel == 0) {
            const float speed = std::max(
                1.0f, static_cast<float>(skillshot->SData.MissileSpeed));
            baseEndTick = skillshot->StartTime + skillshot->SData.Delay +
                static_cast<int>(1000.0f *
                    skillshot->StartPosition.Distance(skillshot->EndPosition) / speed);
        }
        return SDK::Variables::TickCount() >
            baseEndTick + static_cast<int>(extraEndTime);
    }

    bool IsPersistentTrap(const SkillshotPtr& skillshot) const {
        if (!skillshot) {
            return false;
        }
        for (const auto& entry : m_traps) {
            if (entry.second == skillshot) {
                return true;
            }
        }
        return false;
    }

    static bool ContainsPosition(const SDK::Skillshot& skillshot,
                                 const Vec2& position,
                                 float extraRadius) {
        const float radius =
            static_cast<float>(skillshot.SData.Radius) + std::max(0.0f, extraRadius);
        if (SDK::IsLineSpellType(skillshot.SData.SpellType)) {
            const auto proj = SDK::Prediction::Vec2Ext::ProjectOn(
                position, skillshot.StartPosition, skillshot.EndPosition);
            return proj.IsOnSegment && proj.SegmentPoint.Distance(position) <= radius;
        }
        if (SDK::IsCircleSpellType(skillshot.SData.SpellType)) {
            return skillshot.EndPosition.Distance(position) <= radius;
        }
        if (skillshot.Path.empty()) {
            return false;
        }
        return SDK::Clipper::PointInPolygon(
                   SDK::Clipper::IntPoint(position.x, position.y), skillshot.Path) == 1;
    }

    void UpdateFollowCaster(SDK::Skillshot& skillshot) {
        const auto* data = FindSkillshotData(skillshot);
        if (!data || !data->FollowCaster || !skillshot.Caster.IsValid()) {
            return;
        }

        const Vec2 casterPosition = skillshot.Caster.ServerPosition().To2D();
        Vec2 direction = skillshot.Direction;
        if (direction.IsZero()) {
            direction = skillshot.Caster.Direction().To2D().Normalized();
        }
        if (direction.IsZero()) {
            return;
        }

        if (SDK::IsCircleSpellType(skillshot.SData.SpellType)) {
            const float extra = static_cast<float>(std::max(0, data->sdk.ExtraRange));
            skillshot.EndPosition = casterPosition + direction * extra;
        } else {
            skillshot.StartPosition = casterPosition;
            skillshot.EndPosition =
                casterPosition + direction * static_cast<float>(data->sdk.Range);
        }
        m_originalEnds[&skillshot] = skillshot.EndPosition;
        SpecialSpells::RefreshSkillshotGeometry(skillshot);
    }

    void UpdateMissileFollowsUnit(SDK::Skillshot& skillshot) {
        if (!skillshot.SData.MissileFollowsCaster ||
            !skillshot.Caster.IsValid() ||
            !skillshot.Caster.IsVisible()) {
            return;
        }

        const Vec2 end = skillshot.Caster.ServerPosition().To2D();
        const Vec2 direction = (end - skillshot.StartPosition).Normalized();
        if (direction.IsZero()) {
            return;
        }

        skillshot.EndPosition = end;
        skillshot.Direction = direction;
        m_originalEnds[&skillshot] = end;
        SpecialSpells::RefreshSkillshotGeometry(skillshot);
    }

    static bool HasCollisionType(const SDK::SpellDatabaseEntry& data,
                                 SDK::CollisionableObjects type) {
        return std::find(data.CollisionObjects.begin(), data.CollisionObjects.end(), type) !=
               data.CollisionObjects.end();
    }

    bool CheckCollisionForSkillshot(const SkillshotPtr& skillshot) {
        if (!skillshot || !SDK::IsLineSpellType(skillshot->SData.SpellType)) {
            return false;
        }

        const auto* data = FindSkillshotData(*skillshot);
        if (!data || data->sdk.CollisionObjects.empty()) {
            return false;
        }

        const auto original = m_originalEnds.find(skillshot.get());
        const Vec2 originalEnd = original != m_originalEnds.end()
            ? original->second
            : skillshot->EndPosition;
        m_originalEnds[skillshot.get()] = originalEnd;

        Vec2 current = skillshot->StartPosition;
        if (const auto* missile = dynamic_cast<const SDK::SkillshotMissile*>(skillshot.get())) {
            current = missile->GetMissilePosition(0);
        }

        float closestDistance = FLT_MAX;
        float collisionRadius = 0.0f;
        Vec2 collisionPosition;
        Vec2 collidingUnitPosition;
        std::unordered_set<int> visited;

        if (HasCollisionType(data->sdk, SDK::CollisionableObjects::Walls)) {
            Vec3 wallHit;
            const float planeY = skillshot->Caster.IsValid()
                ? skillshot->Caster.Position().y
                : SDK::ObjectManager::Player().Position().y;
            if (CoreNavGrid::FindWallCollision(
                    Vec3::From2D(current, planeY),
                    Vec3::From2D(originalEnd, planeY),
                    wallHit,
                    15.0f)) {
                collisionPosition = wallHit.To2D();
                collidingUnitPosition = collisionPosition;
                closestDistance = current.Distance(collisionPosition);
            }
        }

        const auto consider = [&](const SDK::AIBaseClient& unit) {
            if (!unit.IsValid() || unit.IsDead() || unit.NetworkId() == 0 ||
                unit.NetworkId() == SDK::ObjectManager::Player().NetworkId() ||
                !visited.insert(unit.NetworkId()).second) {
                return;
            }

            const Vec2 unitPosition = unit.ServerPosition().To2D();
            const auto proj = SDK::Prediction::Vec2Ext::ProjectOn(
                unitPosition, current, originalEnd);
            if (!proj.IsOnSegment ||
                proj.SegmentPoint.Distance(unitPosition) >
                    static_cast<float>(skillshot->SData.Radius) + unit.BoundingRadius()) {
                return;
            }
            const Vec2 projection = proj.SegmentPoint;

            const float distance = current.Distance(projection);
            if (distance < closestDistance) {
                closestDistance = distance;
                collisionRadius = unit.BoundingRadius();
                collisionPosition = projection;
                collidingUnitPosition = unitPosition;
            }
        };

        if (HasCollisionType(data->sdk, SDK::CollisionableObjects::Heroes)) {
            for (const auto& hero : SDK::GameObjects::AllyHeroes()) {
                consider(SDK::AIBaseClient(hero.Handle()));
            }
        }
        if (HasCollisionType(data->sdk, SDK::CollisionableObjects::Minions)) {
            if (!data->CollisionExceptMini) {
                for (const auto& minion : SDK::GameObjects::AllyMinions()) {
                    consider(SDK::AIBaseClient(minion.Handle()));
                }
            }
            for (const auto& minion : SDK::GameObjects::Jungle()) {
                consider(SDK::AIBaseClient(minion.Handle()));
            }
        }

        if (closestDistance == FLT_MAX) {
            if (skillshot->EndPosition != originalEnd) {
                skillshot->EndPosition = originalEnd;
                SpecialSpells::RefreshSkillshotGeometry(*skillshot);
            }
            return false;
        }

        skillshot->EndPosition = collisionPosition;
        SpecialSpells::RefreshSkillshotGeometry(*skillshot);
        return current.Distance(collidingUnitPosition) <=
               static_cast<float>(skillshot->SData.Radius) + collisionRadius;
    }

    void CheckSpellCollisions() {
        m_skillshots.erase(
            std::remove_if(m_skillshots.begin(), m_skillshots.end(), [&](const SkillshotPtr& skillshot) {
                return CheckCollisionForSkillshot(skillshot);
            }),
            m_skillshots.end());
    }

    void CleanupAuxiliaryState() {
        std::unordered_set<const SDK::Skillshot*> active;
        active.reserve(m_skillshots.size());
        for (const auto& skillshot : m_skillshots) {
            if (skillshot) {
                active.insert(skillshot.get());
            }
        }

        for (auto it = m_originalEnds.begin(); it != m_originalEnds.end();) {
            if (active.find(it->first) == active.end()) {
                it = m_originalEnds.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = m_traps.begin(); it != m_traps.end();) {
            if (!it->second || active.find(it->second.get()) == active.end()) {
                it = m_traps.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = m_skillshotData.begin(); it != m_skillshotData.end();) {
            if (active.find(it->first) == active.end()) {
                it = m_skillshotData.erase(it);
            } else {
                ++it;
            }
        }
    }

    static const Generated::SpellDataEntry* FindBySpellName(const char* name) {
        if (!name || !name[0]) {
            return nullptr;
        }

        if (ContainsInsensitive(name, "KarthusLayWasteA") ||
            ContainsInsensitive(name, "KarthusLayWasteDeadA")) {
            for (const auto& entry : SpellDatabase::Spells()) {
                if (SameText(entry.sdk.SpellName, "KarthusLayWasteA1")) {
                    return &entry;
                }
            }
        }

        for (const auto& entry : SpellDatabase::Spells()) {
            if (SameText(entry.sdk.SpellName, name) ||
                ContainsText(entry.sdk.ExtraSpellNames, name)) {
                return &entry;
            }
        }
        return nullptr;
    }

    static const Generated::SpellDataEntry* FindByChampionAndSlot(const char* champ, int slot) {
        if (!champ || !champ[0] || slot < 0) {
            return nullptr;
        }
        SDK::SpellSlot targetSlot = SDK::SpellSlot::Unknown;
        switch (slot) {
            case 0: targetSlot = SDK::SpellSlot::Q; break;
            case 1: targetSlot = SDK::SpellSlot::W; break;
            case 2: targetSlot = SDK::SpellSlot::E; break;
            case 3: targetSlot = SDK::SpellSlot::R; break;
        }
        if (targetSlot == SDK::SpellSlot::Unknown) {
            return nullptr;
        }
        for (const auto& entry : SpellDatabase::Spells()) {
            if (ContainsInsensitive(entry.sdk.ChampionName.c_str(), champ) &&
                entry.sdk.Slot == targetSlot) {
                return &entry;
            }
        }
        return nullptr;
    }

    static const Generated::SpellDataEntry* FindUniqueByChampionAndSlot(const char* champ, int slot) {
        if (!champ || !champ[0] || slot < 0 || slot > 3) {
            return nullptr;
        }

        SDK::SpellSlot targetSlot = SDK::SpellSlot::Unknown;
        switch (slot) {
        case 0: targetSlot = SDK::SpellSlot::Q; break;
        case 1: targetSlot = SDK::SpellSlot::W; break;
        case 2: targetSlot = SDK::SpellSlot::E; break;
        case 3: targetSlot = SDK::SpellSlot::R; break;
        }

        const Generated::SpellDataEntry* match = nullptr;
        for (const auto& entry : SpellDatabase::Spells()) {
            if (_stricmp(entry.sdk.ChampionName.c_str(), champ) != 0 ||
                entry.sdk.Slot != targetSlot) {
                continue;
            }
            if (match) {
                // Slot-only packets cannot distinguish transformations,
                // returns, or multi-stage spells.  Waiting for an exact name
                // avoids creating a ghost skillshot with the wrong geometry.
                return nullptr;
            }
            match = &entry;
        }
        return match;
    }

    static const Generated::SpellDataEntry* FindByMissileName(const char* name) {
        if (!name || !name[0]) {
            return nullptr;
        }
        for (const auto& entry : SpellDatabase::Spells()) {
            if (SameText(entry.sdk.MissileSpellName, name) ||
                SameText(entry.sdk.SpellName, name) ||
                ContainsText(entry.sdk.ExtraMissileNames, name)) {
                return &entry;
            }
        }
        return nullptr;
    }

    static SDK::AIBaseClient MakeCaster(const ::Core::Events::ObjectInfo& info) {
        if (info.NetworkId != 0 && info.NetworkId != 0xFFFFFFFFu) {
            auto caster = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
                static_cast<int>(info.NetworkId));
            if (caster.IsValid()) {
                return caster;
            }
        }

        return info.Ptr ? SDK::AIBaseClient(info.Ptr, info.Type) : SDK::AIBaseClient();
    }

    static std::shared_ptr<SDK::Skillshot> CreateSkillshot(const SDK::SpellDatabaseEntry& entry) {
        switch (entry.SpellType) {
        case SDK::SpellType::SkillshotMissileArc:
            return std::make_shared<SDK::SkillshotMissileArc>(entry);
        case SDK::SpellType::SkillshotMissileCircle:
            return std::make_shared<SDK::SkillshotMissileCircle>(entry);
        case SDK::SpellType::SkillshotMissileCone:
            return std::make_shared<SDK::SkillshotMissileCone>(entry);
        case SDK::SpellType::SkillshotMissileLine:
            return std::make_shared<SDK::SkillshotMissileLine>(entry);
        case SDK::SpellType::SkillshotLine:
            return std::make_shared<SDK::SkillshotLine>(entry);
        case SDK::SpellType::SkillshotCircle:
            return std::make_shared<SDK::SkillshotCircle>(entry);
        case SDK::SpellType::SkillshotCone:
            return std::make_shared<SDK::SkillshotCone>(entry);
        case SDK::SpellType::SkillshotRing:
            return std::make_shared<SDK::SkillshotRing>(entry);
        default:
            return std::make_shared<SDK::SkillshotLine>(entry);
        }
    }

    bool AlreadyDetected(const SkillshotPtr& skillshot) {
        if (!skillshot) {
            return true;
        }

        for (const auto& detected : m_skillshots) {
            if (!detected ||
                detected->SData.SpellName != skillshot->SData.SpellName ||
                detected->Caster.NetworkId() != skillshot->Caster.NetworkId()) {
                continue;
            }

            const float angle = SDK::Skillshot::AngleBetween(
                skillshot->Direction, detected->Direction);
            const int startDelta = std::abs(skillshot->StartTime - detected->StartTime);
            const float startDistanceSqr =
                skillshot->StartPosition.DistanceSqr(detected->StartPosition);

            if (skillshot->DetectionType == SDK::SkillshotDetectionType::MissileCreate) {
                auto oldMissile = std::dynamic_pointer_cast<SDK::SkillshotMissile>(detected);
                auto newMissile = std::dynamic_pointer_cast<SDK::SkillshotMissile>(skillshot);
                if (!oldMissile || !newMissile || angle >= 12.0f ||
                    startDelta > 350 || startDistanceSqr > 122500.0f) {
                    continue;
                }

                const int oldMissileId = oldMissile->Missile.IsValid()
                    ? oldMissile->Missile.NetworkId()
                    : 0;
                const int newMissileId = newMissile->Missile.IsValid()
                    ? newMissile->Missile.NetworkId()
                    : 0;
                if (oldMissileId != 0 && newMissileId != 0 &&
                    oldMissileId != newMissileId) {
                    continue;
                }

                oldMissile->SData = newMissile->SData;
                oldMissile->Missile = newMissile->Missile;
                oldMissile->StartPosition = newMissile->StartPosition;
                oldMissile->EndPosition = newMissile->EndPosition;
                oldMissile->Direction = newMissile->Direction;
                oldMissile->StartTime = newMissile->StartTime;
                oldMissile->DetectionType = SDK::SkillshotDetectionType::MissileCreate;
                m_originalEnds[oldMissile.get()] = oldMissile->EndPosition;
                SpecialSpells::RefreshSkillshotGeometry(*oldMissile);
                return true;
            }

            // ProcessSpell/DoCast/ProcessCast can all report the same cast.
            // Keep that de-duplication window deliberately short so a real
            // rapid recast in the same direction is never swallowed.
            if (angle < 2.0f && startDelta <= 160 && startDistanceSqr <= 10000.0f) {
                return true;
            }
        }
        return false;
    }

    void AddSkillshot(const SkillshotPtr& skillshot,
                      const Generated::SpellDataEntry& data,
                      bool allowDuplicate = false) {
        if (!skillshot || (!allowDuplicate && AlreadyDetected(skillshot))) {
            return;
        }

        m_skillshots.push_back(skillshot);
        m_originalEnds[skillshot.get()] = skillshot->EndPosition;
        m_skillshotData[skillshot.get()] = data;
    }

    void RemoveDetectedSpell(int casterNetworkId, const char* spellName) {
        if (!spellName || !spellName[0]) {
            return;
        }

        m_skillshots.erase(
            std::remove_if(m_skillshots.begin(), m_skillshots.end(), [&](const SkillshotPtr& skillshot) {
                return skillshot &&
                       skillshot->Caster.NetworkId() == casterNetworkId &&
                       SameText(skillshot->SData.SpellName, spellName);
            }),
            m_skillshots.end());
    }

    SkillshotPtr CreateSpellData(const SDK::AIBaseClient& caster,
                                 const Vector3& startWorld,
                                 const Vector3& endWorld,
                                 const Generated::SpellDataEntry& data,
                                 SDK::SkillshotDetectionType detectionType,
                                 const SDK::MissileClient& missile = SDK::MissileClient(),
                                 int overrideStartTick = 0,
                                 bool allowDuplicate = false) {
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            return {};
        }

        const float range = static_cast<float>(std::max(1, data.sdk.Range));
        Vec2 start = ResolveStartPosition(caster, startWorld.To2D());
        if (start.IsZero()) {
            return {};
        }

        if (!data.HasTrap &&
            player.Position().Distance(Vec3::From2D(start, startWorld.y)) > range + 1000.0f) {
            return {};
        }

        Vec2 end = endWorld.To2D();
        if (!data.HasTrap && (end.IsZero() || end.DistanceSqr(start) < 25.0f)) {
            end = ResolveEndPosition(data, caster, start, end, Vec2(), player.ServerPosition().To2D());
        }
        const bool stationaryCircle =
            SDK::IsCircleSpellType(data.sdk.SpellType) &&
            start.DistanceSqr(end) <= 1.0f;
        Vec2 direction = ResolveDirection(caster, start, end, player);
        if (direction.IsZero()) {
            if (!stationaryCircle) {
                end = start + direction * range;
            } else {
                direction = Vec2(1.0f, 0.0f);
            }
        }

        if (!stationaryCircle &&
            (data.sdk.FixedRange ||
             (SDK::IsLineSpellType(data.sdk.SpellType) && !data.UseEndPosition) ||
             start.Distance(end) > range)) {
            end = start + direction * range;
        }

        if (data.sdk.ExtraRange != 0) {
            const float extra = std::min(
                static_cast<float>(data.sdk.ExtraRange),
                std::max(0.0f, range - end.Distance(start)));
            end = end + direction * extra;
        }

        if (!missile.IsValid()) {
            if (data.Invert) {
                end = start - direction * start.Distance(end);
            }
            if (data.Centered) {
                start = start - direction * (range / 2.0f);
                end = start + direction * range;
            }
            if (data.IsPerpendicular && data.SecondaryRadius > 0) {
                const Vec2 perpendicular = SDK::Prediction::Vec2Ext::Rotated(direction, 1.57079632679f).Normalized();
                start = end - perpendicular * static_cast<float>(data.SecondaryRadius);
                end = end + perpendicular * static_cast<float>(data.SecondaryRadius);
                direction = (end - start).Normalized();
            }
            if (data.IsHorizontal) {
                const Vec2 originalStart = startWorld.To2D();
                const Vec2 originalEnd = endWorld.To2D();
                start = originalStart.Extend(originalEnd, range);
                end = originalStart.Extend(originalEnd, -range);
                direction = (end - start).Normalized();
            }
        }

        SDK::SpellDatabaseEntry sdkEntry = data.sdk;
        if (sdkEntry.SpellType == SDK::SpellType::SkillshotLine &&
            IsFiniteMissileSpeed(sdkEntry)) {
            sdkEntry.SpellType = SDK::SpellType::SkillshotMissileLine;
        } else if (sdkEntry.SpellType == SDK::SpellType::SkillshotCircle &&
                   IsFiniteMissileSpeed(sdkEntry)) {
            sdkEntry.SpellType = SDK::SpellType::SkillshotMissileCircle;
        }
        sdkEntry.AvoidMaxRangeReduction = true;
        sdkEntry.FixedRange = false;
        sdkEntry.ExtraRange = 0;

        auto skillshot = CreateSkillshot(sdkEntry);
        if (!skillshot) {
            return {};
        }

        skillshot->DetectionType = detectionType;
        skillshot->Caster = caster;
        skillshot->StartPosition = start;
        skillshot->EndPosition = end;
        skillshot->StartTime = overrideStartTick != 0
            ? overrideStartTick
            : SDK::Variables::TickCount() - SDK::Game::Ping() / 2;

        if (missile.IsValid()) {
            if (auto missileSkillshot = std::dynamic_pointer_cast<SDK::SkillshotMissile>(skillshot)) {
                missileSkillshot->Missile = missile;
            }
        }

        if (skillshot->Process()) {
            if (UsesSourceObject(data.sdk) && !skillshot->StartPosition.IsZero()) {
                start = skillshot->StartPosition;
                direction = ResolveDirection(caster, start, end, player);
                if (!stationaryCircle) {
                    if (data.sdk.FixedRange ||
                        (SDK::IsLineSpellType(data.sdk.SpellType) && !data.UseEndPosition) ||
                        start.Distance(end) > range) {
                        end = start + direction * range;
                    }
                }
            }
            skillshot->StartPosition = start;
            skillshot->EndPosition = end;
            skillshot->Direction = (end - start).Normalized();
            SpecialSpells::RefreshSkillshotGeometry(*skillshot);
            AddSkillshot(skillshot, data, allowDuplicate);
            SDK::Orbwalker::DebugPrint("[SpellDetector][Detected] Spell: %s | Caster: %s | Type: %d | Range: %.1f",
                                       data.sdk.SpellName.c_str(), EvadeUtils::GetObjectCharacterName(caster).c_str(),
                                       static_cast<int>(detectionType), range);
            return skillshot;
        }
        return {};
    }
};

} // namespace Plugins::KuroEvade
