#pragma once


#include "SpecialSpells/SpecialSpellProcessor.h"
#include "SpellDatabase.h"

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
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

    void Clear() {
        m_skillshots.clear();
        m_traps.clear();
        m_originalEnds.clear();
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
                       (!IsPersistentTrap(skillshot) && skillshot->HasExpired());
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
        const auto* data = FindBySpellName(args.SpellName);
        if (!data) {
            data = FindBySpellName(args.ScriptName);
        }
        if (!data) {
            data = FindBySpellName(args.PayloadSpellName);
        }
        if (!data || data->UsePacket || !IsSpellEnabled(*data)) {
            return;
        }

        SDK::AIBaseClient caster = MakeCaster(args.Sender);
        if (!caster.IsValid() || caster.IsAlly()) {
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

        if (specialResult.Data.MultipleNumber != -1 && specialResult.Data.MultipleNumber > 1) {
            Vec2 baseDirection = (args.EndPosition.To2D() - args.StartPosition.To2D()).Normalized();
            if (baseDirection.IsZero()) {
                baseDirection = caster.Direction().To2D().Normalized();
            }

            const int half = (specialResult.Data.MultipleNumber - 1) / 2;
            for (int i = -half; i <= half; ++i) {
                const Vec2 direction = SDK::Prediction::Vec2Ext::Rotated(baseDirection, specialResult.Data.MultipleAngle * static_cast<float>(i));
                const Vector3 end = Vec3::From2D(
                    args.StartPosition.To2D() + direction * static_cast<float>(specialResult.Data.sdk.Range),
                    args.EndPosition.y);
                CreateSpellData(caster, args.StartPosition, end, specialResult.Data, SDK::SkillshotDetectionType::ProcessSpell);
            }
            return;
        }

        CreateSpellData(caster, args.StartPosition, args.EndPosition, specialResult.Data, SDK::SkillshotDetectionType::ProcessSpell);
    }

    void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
        const char* missileName = args.MissileName[0] ? args.MissileName : args.SpellName;
        const auto* data = FindByMissileName(missileName);
        if (!data && args.SpellName[0]) {
            data = FindBySpellName(args.SpellName);
        }
        if (!data || !IsSpellEnabled(*data)) {
            return;
        }

        SDK::MissileClient missile(args.Sender.Ptr);
        if (!missile.IsValid()) {
            return;
        }

        SDK::AIBaseClient caster = MakeCaster(args.Source);
        if (!caster.IsValid() && missile.CasterNetworkId() != 0) {
            caster = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(missile.CasterNetworkId());
        }
        if (!caster.IsValid() || caster.IsAlly()) {
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
        if (!object.IsValid() || object.IsAlly()) {
            return;
        }

        const int objectId = object.NetworkId() != 0
            ? object.NetworkId()
            : static_cast<int>(args.Sender.NetworkId);
        if (objectId == 0 || m_traps.find(objectId) != m_traps.end()) {
            return;
        }

        const std::string objectName = object.Name();
        const std::string characterName = object.CharacterName();
        const auto* data = FindTrapData(objectName, characterName);
        if (!data || !IsSpellEnabled(*data)) {
            return;
        }

        SDK::AIBaseClient caster;
        for (const auto& enemy : SDK::GameObjects::EnemyHeroes()) {
            if (enemy.IsValid() &&
                _stricmp(enemy.CharacterName().c_str(), data->sdk.ChampionName.c_str()) == 0) {
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
    int m_lastCollisionTick = 0;
    bool m_checkCollisions = false;
    bool m_dodgeFow = true;

    bool IsSpellEnabled(const Generated::SpellDataEntry& data) const {
        return !m_spellEnabledPredicate || m_spellEnabledPredicate(data);
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

    static const Generated::SpellDataEntry* FindSkillshotData(const SDK::Skillshot& skillshot) {
        const std::string casterName = skillshot.Caster.IsValid()
            ? skillshot.Caster.CharacterName()
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

            if (SDK::Skillshot::AngleBetween(skillshot->Direction, detected->Direction) < 10.0f) {
                if (skillshot->DetectionType == SDK::SkillshotDetectionType::MissileCreate) {
                    auto oldMissile = std::dynamic_pointer_cast<SDK::SkillshotMissile>(detected);
                    auto newMissile = std::dynamic_pointer_cast<SDK::SkillshotMissile>(skillshot);
                    if (oldMissile && newMissile) {
                        oldMissile->Missile = newMissile->Missile;
                    }
                }
                return true;
            }
        }
        return false;
    }

    void AddSkillshot(const SkillshotPtr& skillshot, bool allowDuplicate = false) {
        if (!skillshot || (!allowDuplicate && AlreadyDetected(skillshot))) {
            return;
        }

        m_skillshots.push_back(skillshot);
        m_originalEnds[skillshot.get()] = skillshot->EndPosition;
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
        if (!data.HasTrap && player.Position().Distance(startWorld) > range + 1000.0f) {
            return {};
        }

        Vec2 start = startWorld.To2D();
        Vec2 end = endWorld.To2D();
        const bool stationaryCircle =
            data.sdk.SpellType == SDK::SpellType::SkillshotCircle &&
            start.DistanceSqr(end) <= 1.0f;
        Vec2 direction = (end - start).Normalized();
        if (direction.IsZero()) {
            if (!stationaryCircle) {
                direction = player.Direction().To2D().Normalized();
                if (direction.IsZero()) {
                    direction = Vec2(1.0f, 0.0f);
                }
                end = start + direction * range;
            } else {
                direction = Vec2(1.0f, 0.0f);
            }
        }

        if (!stationaryCircle && (start.Distance(end) > range || data.sdk.FixedRange)) {
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
            AddSkillshot(skillshot, allowDuplicate);
            return skillshot;
        }
        return {};
    }
};

} // namespace Plugins::KuroEvade
