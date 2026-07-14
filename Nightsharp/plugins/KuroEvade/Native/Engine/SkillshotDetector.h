#pragma once

// Replacement for the old KuroEvade detector.  Event flow follows the
// supplied SkillshotDetector.cs (process-spell, missile and persistent object
// discovery) while the raw event decoding remains a NightSharp integration
// concern owned by KuroEvadePlugin.

#include "Collision.h"
#include "Skillshot.h"
#include "../Database/SpellDatabase.h"
#include "../Helpers/Utils.h"
#include "../SpecialSpells/SpecialSpellProcessor.h"

#include "../../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Plugins::KuroEvade {

class SourceSkillshotDetector final {
public:
    using SkillshotPtr = SourceSkillshotPtr;
    using SkillshotList = SourceSkillshotList;

    void SetCollisionEnabled(bool enabled) {
        m_collisionEnabled = enabled;
    }

    void SetCollisionTypes(bool minions, bool heroes, bool yasuoWall) {
        m_minionCollision = minions;
        m_heroCollision = heroes;
        m_yasuoCollision = yasuoWall;
    }

    void SetFowEnabled(bool enabled) {
        m_fowEnabled = enabled;
    }

    void SetEnhancedDetection(bool enabled) {
        m_enhancedDetection = enabled;
    }

    void SetDevSameTeam(bool enabled) {
        m_sameTeam = enabled;
    }

    SkillshotList& Skillshots() {
        return m_skillshots;
    }

    const SkillshotList& Skillshots() const {
        return m_skillshots;
    }

    void Clear() {
        m_skillshots.clear();
        m_traps.clear();
        m_nextId = 0;
        m_lastCollisionTick = 0;
        SpecialSpells::ClearState();
    }

    void AddSimulatedSkillshot(const std::shared_ptr<SDK::Skillshot>& native,
                               const Database::SpellData& data) {
        Add(CreateRuntime(native, data, SourceDetectionType::Simulated), true);
    }

    int RemoveAtPosition(const Vec2& position, float extraRadius = 50.0f) {
        EvadeSettings settings;
        settings.SkillShotsExtraRadius = std::max(0, static_cast<int>(extraRadius));
        int removed = 0;
        m_skillshots.erase(std::remove_if(
            m_skillshots.begin(), m_skillshots.end(),
            [&](const SkillshotPtr& skillshot) {
                if (!skillshot) {
                    return false;
                }
                if (!skillshot->ContainsStatic(position, 0.0f, settings)) {
                    return false;
                }
                ++removed;
                return true;
            }), m_skillshots.end());
        CleanupTraps();
        return removed;
    }

    void Update() {
        SpecialSpells::BeginUpdate();
        const int now = SDK::Variables::TickCount();
        m_skillshots.erase(std::remove_if(
            m_skillshots.begin(), m_skillshots.end(),
            [&](const SkillshotPtr& skillshot) {
                if (!skillshot || !skillshot->Native) {
                    return true;
                }
                UpdateDynamicGeometry(*skillshot);
                const Vec2 endBeforeSpecial = skillshot->Native->EndPosition;
                const bool collisionWasShortened =
                    !skillshot->OriginalEnd.IsZero() &&
                    skillshot->CollisionEnd.DistanceSqr(
                        skillshot->OriginalEnd) > 1.0f;
                if (!SpecialSpells::UpdateSkillshot(*skillshot->Native)) {
                    return true;
                }
                if (skillshot->Native->EndPosition.DistanceSqr(
                        endBeforeSpecial) > 1.0f) {
                    skillshot->OriginalEnd = skillshot->Native->EndPosition;
                    if (!collisionWasShortened) {
                        skillshot->CollisionEnd = skillshot->Native->EndPosition;
                    }
                }
                skillshot->Native->Game_OnUpdate();
                return !skillshot->IsActive(now);
            }), m_skillshots.end());
        SpecialSpells::EndUpdate();

        if (m_collisionEnabled && now - m_lastCollisionTick >= 50) {
            for (const auto& skillshot : m_skillshots) {
                if (skillshot) {
                    SourceCollision::Update(*skillshot, m_minionCollision,
                                            m_heroCollision, m_yasuoCollision);
                }
            }
            m_lastCollisionTick = now;
        }
        CleanupTraps();
    }

    static const Database::SpellData* LookupBySpellName(const char* name) {
        return FindBySpellName(name);
    }

    void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
        SDK::AIBaseClient caster = MakeCaster(args.Sender);
        if (!IsValidCaster(caster) || args.IsAutoAttack) {
            return;
        }

        const auto* source = FindProcessSpellData(args);
        if (!source || source->UsePacket) {
            return;
        }

        SpecialSpells::ProcessResult special = SpecialSpells::ProcessCast(
            caster, args, *source, &SourceSkillshotDetector::LookupBySpellName);
        for (const std::string& spellName : special.RemoveSpellNames) {
            RemoveDetectedSpell(caster.NetworkId(), spellName.c_str());
        }
        for (const auto& extra : special.ExtraSpells) {
            Create(caster, extra.Start, extra.End, extra.Data,
                   SourceDetectionType::ProcessSpell,
                   SDK::MissileClient(), extra.OverrideStartTick, false);
        }
        if (special.NoProcess) {
            return;
        }
        if (special.Data.NoTarget && args.TargetNetworkId != 0 &&
            args.TargetNetworkId == static_cast<std::uint32_t>(caster.NetworkId())) {
            return;
        }

        Vec2 start = ResolveStart(caster, args.StartPosition.To2D());
        Vec2 end = ResolveEnd(special.Data, caster, start,
            args.EndPosition.To2D(), args.CastPosition.To2D());
        if (start.IsZero() || end.IsZero()) {
            return;
        }

        if (special.Data.MultipleNumber > 1) {
            const Vec2 direction = ResolveDirection(caster, start, end);
            const int half = (special.Data.MultipleNumber - 1) / 2;
            for (int index = -half; index <= half; ++index) {
                const Vec2 rotated = SourceGeometry::Rotate(
                    direction, special.Data.MultipleAngle * static_cast<float>(index));
                Create(caster,
                       Vec3::From2D(start, args.StartPosition.y),
                       Vec3::From2D(start + rotated *
                           static_cast<float>(std::max(1, special.Data.Runtime.Range)),
                           args.EndPosition.y),
                       special.Data, SourceDetectionType::ProcessSpell,
                       SDK::MissileClient(), 0, true);
            }
            return;
        }

        Create(caster,
               Vec3::From2D(start, args.StartPosition.y),
               Vec3::From2D(end, args.EndPosition.y),
               special.Data, SourceDetectionType::ProcessSpell);
    }

    void OnProcessCastSpell(const SDK::Events::CastSpellEventArgs& args) {
        if (!args.Sender.IsValid()) {
            return;
        }
        SDK::AIBaseClient caster = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
            static_cast<int>(args.Sender.NetworkId));
        if (!IsValidCaster(caster)) {
            return;
        }
        const std::string champion = EvadeUtils::GetObjectCharacterName(caster);
        const auto* data = FindUniqueByChampionAndSlot(champion.c_str(), args.Slot);
        if (!data || data->UsePacket) {
            return;
        }
        const Vec2 start = ResolveStart(caster, args.StartPosition.To2D());
        const Vec2 end = ResolveEnd(*data, caster, start, args.EndPosition.To2D(), {});
        if (!start.IsZero() && !end.IsZero()) {
            Create(caster, Vec3::From2D(start, args.StartPosition.y),
                   Vec3::From2D(end, args.EndPosition.y), *data,
                   SourceDetectionType::ProcessSpell);
        }
    }

    void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
        if (!m_enhancedDetection) {
            return;
        }
        SDK::MissileClient missile(args.Sender.Ptr);
        if (!missile.IsValid()) {
            return;
        }

        const std::string runtimeName = missile.SpellName();
        const char* names[] = {
            args.MissileName,
            args.SpellName,
            runtimeName.c_str(),
        };
        const Database::SpellData* source = nullptr;
        for (const char* name : names) {
            if (IsBasicAttackName(name)) {
                return;
            }
            if (!source) {
                source = FindByMissileName(name);
            }
        }
        if (!source) {
            return;
        }

        SDK::AIBaseClient caster = MakeCaster(args.Source);
        if (!caster.IsValid() && missile.CasterNetworkId() != 0) {
            caster = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
                missile.CasterNetworkId());
        }
        if (!IsValidCaster(caster) || (!caster.IsVisible() && !m_fowEnabled)) {
            return;
        }

        Database::SpellData data = *source;
        SpecialSpells::ProcessMissile(caster, missile, data);
        Vec3 start = args.StartPosition.IsZero() ? missile.StartPosition() : args.StartPosition;
        Vec3 end = args.EndPosition.IsZero() ? missile.EndPosition() : args.EndPosition;
        if (start.IsZero()) {
            start = caster.Position();
        }
        if (end.IsZero()) {
            end = start + caster.Direction() *
                static_cast<float>(std::max(1, data.Runtime.Range));
        }

        const int speed = std::max(1, data.Runtime.MissileSpeed);
        int startTick = SDK::Variables::TickCount() - SDK::Game::Ping() / 2;
        if (!data.UsePacket && !data.Runtime.MissileDelayed) {
            startTick -= std::max(0, data.Runtime.Delay);
        }
        startTick -= static_cast<int>(1000.0f *
            missile.Position().Distance(start) / static_cast<float>(speed));
        Create(caster, start, end, data, SourceDetectionType::MissileCreate,
               missile, startTick);
    }

    void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
        const int firstId = static_cast<int>(args.MissileNetworkId);
        const int secondId = static_cast<int>(args.Sender.NetworkId);
        m_skillshots.erase(std::remove_if(
            m_skillshots.begin(), m_skillshots.end(),
            [&](const SkillshotPtr& skillshot) {
                if (!skillshot || !skillshot->Native) {
                    return true;
                }
                const auto* missile = dynamic_cast<const SDK::SkillshotMissile*>(
                    skillshot->Native.get());
                if (!missile) {
                    return false;
                }
                const int id = skillshot->MissileNetworkId != 0
                    ? skillshot->MissileNetworkId
                    : (missile->Missile.IsValid()
                        ? missile->Missile.NetworkId()
                        : 0);
                const bool matches = id == firstId || id == secondId;
                return matches && (skillshot->SpellData().CanBeRemoved ||
                                   skillshot->SpellData().ForceRemove);
            }), m_skillshots.end());
        CleanupTraps();
    }

    void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
        if (!m_enhancedDetection) {
            return;
        }
        if (!args.Sender.Ptr) {
            return;
        }
        SDK::GameObject object(args.Sender.Ptr, args.Sender.Type);
        if (!object.IsValid() || (object.IsAlly() && !m_sameTeam)) {
            return;
        }
        const int objectId = object.NetworkId() != 0
            ? object.NetworkId()
            : static_cast<int>(args.Sender.NetworkId);
        if (objectId == 0 || m_traps.find(objectId) != m_traps.end()) {
            return;
        }

        const auto* data = FindTrapData(EvadeUtils::GetObjectName(object),
                                        EvadeUtils::GetObjectCharacterName(object));
        if (!data) {
            return;
        }

        SDK::AIBaseClient caster;
        for (const auto& hero : SDK::GameObjects::EnemyHeroes()) {
            if (hero.IsValid() && SameText(
                    EvadeUtils::GetObjectCharacterName(hero), data->Runtime.ChampionName)) {
                caster = SDK::AIBaseClient(hero.Handle());
                break;
            }
        }
        SkillshotPtr trap = Create(caster, object.Position(), object.Position(),
            *data, SourceDetectionType::ObjectCreate, SDK::MissileClient(),
            SDK::Variables::TickCount(), true);
        if (trap) {
            trap->Persistent = true;
            trap->TrapObjectId = objectId;
            m_traps[objectId] = trap;
        }
    }

    void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
        int objectId = static_cast<int>(args.Sender.NetworkId);
        if (objectId == 0 && args.Sender.Ptr) {
            objectId = SDK::GameObject(args.Sender.Ptr, args.Sender.Type).NetworkId();
        }
        const auto found = m_traps.find(objectId);
        if (found == m_traps.end()) {
            return;
        }
        const SkillshotPtr trap = found->second;
        m_traps.erase(found);
        m_skillshots.erase(std::remove(m_skillshots.begin(), m_skillshots.end(), trap),
                           m_skillshots.end());
    }

private:
    SkillshotList m_skillshots;
    std::unordered_map<int, SkillshotPtr> m_traps;
    int m_nextId = 0;
    int m_lastCollisionTick = 0;
    bool m_collisionEnabled = true;
    bool m_minionCollision = false;
    bool m_heroCollision = false;
    bool m_yasuoCollision = true;
    bool m_fowEnabled = true;
    bool m_sameTeam = false;
    bool m_enhancedDetection = true;

    bool IsValidCaster(const SDK::AIBaseClient& caster) const {
        if (!caster.IsValid()) {
            return false;
        }
        const auto player = SDK::ObjectManager::Player();
        if (player.IsValid() && caster.NetworkId() == player.NetworkId()) {
            return m_sameTeam;
        }
        return !caster.IsAlly() || m_sameTeam;
    }

    static SDK::AIBaseClient MakeCaster(const ::Core::Events::ObjectInfo& info) {
        if (info.NetworkId != 0 && info.NetworkId != 0xFFFFFFFFu) {
            auto caster = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
                static_cast<int>(info.NetworkId));
            if (caster.IsValid()) {
                return caster;
            }
        }
        return info.Ptr
            ? SDK::AIBaseClient(info.Ptr, info.Type)
            : SDK::AIBaseClient();
    }

    static bool SameText(const std::string& lhs, const std::string& rhs) {
        return !lhs.empty() && !rhs.empty() && _stricmp(lhs.c_str(), rhs.c_str()) == 0;
    }

    static bool SameText(const std::string& lhs, const char* rhs) {
        return rhs && rhs[0] && SameText(lhs, std::string(rhs));
    }

    static bool ContainsInsensitive(const std::string& text, const std::string& value) {
        if (text.empty() || value.empty()) {
            return false;
        }
        std::string haystack = text;
        std::string needle = value;
        std::transform(haystack.begin(), haystack.end(), haystack.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(needle.begin(), needle.end(), needle.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return haystack.find(needle) != std::string::npos;
    }

    static bool IsBasicAttackName(const char* name) {
        if (!name || !name[0]) {
            return false;
        }
        const std::string value(name);
        return ContainsInsensitive(value, "basicattack") ||
               ContainsInsensitive(value, "critattack") ||
               SDK::Orbwalker::IsAutoAttack(value);
    }

    static bool ContainsName(const std::vector<std::string>& values,
                             const char* name) {
        if (!name || !name[0]) {
            return false;
        }
        return std::any_of(values.begin(), values.end(),
            [&](const std::string& value) { return SameText(value, name); });
    }

    static const Database::SpellData* FindBySpellName(const char* name) {
        if (!name || !name[0]) {
            return nullptr;
        }
        for (const auto& entry : Database::SpellDatabase::Spells()) {
            if (SameText(entry.Runtime.SpellName, name) ||
                ContainsName(entry.Runtime.ExtraSpellNames, name)) {
                return &entry;
            }
        }
        // Karthus Q exposes a numbered runtime name in some patches.
        if (ContainsInsensitive(name, "karthuslaywaste")) {
            for (const auto& entry : Database::SpellDatabase::Spells()) {
                if (ContainsInsensitive(entry.Runtime.SpellName, "karthuslaywaste")) {
                    return &entry;
                }
            }
        }
        return nullptr;
    }

    static const Database::SpellData* FindByMissileName(const char* name) {
        if (!name || !name[0]) {
            return nullptr;
        }
        for (const auto& entry : Database::SpellDatabase::Spells()) {
            if (SameText(entry.Runtime.MissileSpellName, name) ||
                ContainsName(entry.Runtime.ExtraMissileNames, name)) {
                return &entry;
            }
        }
        return nullptr;
    }

    static const Database::SpellData* FindProcessSpellData(
        const SDK::Events::ProcessSpellEventArgs& args) {
        const char* names[] = {
            args.SpellName,
            args.PayloadSpellName,
            args.ScriptName,
            args.SpellSlotName,
            args.MissileName,
            args.PayloadMissileName,
        };
        for (const char* name : names) {
            if (!name || !name[0] || IsBasicAttackName(name)) {
                continue;
            }
            if (const auto* data = FindBySpellName(name)) {
                return data;
            }
            if (const auto* data = FindByMissileName(name)) {
                return data;
            }
        }
        return nullptr;
    }

    static const Database::SpellData* FindUniqueByChampionAndSlot(
        const char* champion, int slot) {
        if (!champion || !champion[0] || slot < 0 || slot > 3) {
            return nullptr;
        }
        SDK::SpellSlot target = static_cast<SDK::SpellSlot>(slot);
        const Database::SpellData* result = nullptr;
        for (const auto& entry : Database::SpellDatabase::Spells()) {
            if (!SameText(entry.Runtime.ChampionName, champion) || entry.Runtime.Slot != target) {
                continue;
            }
            if (result) {
                return nullptr;
            }
            result = &entry;
        }
        return result;
    }

    static bool RegexMatch(const std::string& text, const std::string& pattern) {
        if (text.empty() || pattern.empty()) {
            return false;
        }
        try {
            return std::regex_search(text,
                std::regex(pattern, std::regex_constants::icase));
        } catch (const std::regex_error&) {
            return ContainsInsensitive(text, pattern);
        }
    }

    static const Database::SpellData* FindTrapData(
        const std::string& objectName,
        const std::string& characterName) {
        for (const auto& entry : Database::SpellDatabase::Spells()) {
            if (!entry.HasTrap) {
                continue;
            }
            if ((!entry.TrapBaseName.empty() &&
                 (ContainsInsensitive(objectName, entry.TrapBaseName) ||
                  ContainsInsensitive(characterName, entry.TrapBaseName))) ||
                (!entry.TrapTroyName.empty() &&
                 (RegexMatch(objectName, entry.TrapTroyName) ||
                  RegexMatch(characterName, entry.TrapTroyName)))) {
                return &entry;
            }
        }
        return nullptr;
    }

    static Vec2 ResolveStart(const SDK::AIBaseClient& caster, const Vec2& raw) {
        if (!raw.IsZero()) {
            return raw;
        }
        return caster.IsValid() ? caster.ServerPosition().To2D() : Vec2();
    }

    static Vec2 ResolveDirection(const SDK::AIBaseClient& caster,
                                 const Vec2& start,
                                 const Vec2& end) {
        Vec2 direction = (end - start).Normalized();
        if (direction.IsZero() && caster.IsValid()) {
            direction = caster.Direction().To2D().Normalized();
        }
        if (direction.IsZero()) {
            const auto player = SDK::ObjectManager::Player();
            if (player.IsValid()) {
                direction = (player.ServerPosition().To2D() - start).Normalized();
            }
        }
        return direction.IsZero() ? Vec2(1.0f, 0.0f) : direction;
    }

    static Vec2 ResolveEnd(const Database::SpellData& data,
                           const SDK::AIBaseClient& caster,
                           const Vec2& start,
                           const Vec2& primary,
                           const Vec2& secondary) {
        Vec2 end = SDK::IsCircleSpellType(data.Runtime.SpellType) && !secondary.IsZero()
            ? secondary
            : primary;
        if (end.IsZero()) {
            end = secondary;
        }
        if (end.IsZero()) {
            end = start;
        }

        const float range = static_cast<float>(std::max(1, data.Runtime.Range));
        Vec2 direction = ResolveDirection(caster, start, end);
        const bool stationaryCircle = SDK::IsCircleSpellType(data.Runtime.SpellType) &&
            start.DistanceSqr(end) <= 1.0f;
        if (!stationaryCircle &&
            (data.Runtime.FixedRange || start.Distance(end) > range ||
             (SDK::IsLineSpellType(data.Runtime.SpellType) && !data.UseEndPosition))) {
            end = start + direction * range;
        }
        if (data.Runtime.ExtraRange > 0 && !stationaryCircle) {
            end = end + direction * std::min(
                static_cast<float>(data.Runtime.ExtraRange),
                std::max(0.0f, range - start.Distance(end)));
        }
        return end;
    }

    static std::shared_ptr<SDK::Skillshot> CreateNative(
        const SDK::SpellDatabaseEntry& entry) {
        switch (entry.SpellType) {
        case SDK::SpellType::SkillshotMissileArc:
            return std::make_shared<SDK::SkillshotMissileArc>(entry);
        case SDK::SpellType::SkillshotMissileCircle:
            return std::make_shared<SDK::SkillshotMissileCircle>(entry);
        case SDK::SpellType::SkillshotMissileCone:
            return std::make_shared<SDK::SkillshotMissileCone>(entry);
        case SDK::SpellType::SkillshotMissileLine:
            return std::make_shared<SDK::SkillshotMissileLine>(entry);
        case SDK::SpellType::SkillshotCircle:
            return std::make_shared<SDK::SkillshotCircle>(entry);
        case SDK::SpellType::SkillshotCone:
            return std::make_shared<SDK::SkillshotCone>(entry);
        case SDK::SpellType::SkillshotRing:
            return std::make_shared<SDK::SkillshotRing>(entry);
        case SDK::SpellType::SkillshotArc:
            return std::make_shared<SDK::SkillshotMissileArc>(entry);
        case SDK::SpellType::SkillshotLine:
        default:
            return std::make_shared<SDK::SkillshotLine>(entry);
        }
    }

    SkillshotPtr CreateRuntime(const std::shared_ptr<SDK::Skillshot>& native,
                               const Database::SpellData& data,
                               SourceDetectionType detectionType) {
        if (!native) {
            return {};
        }
        auto runtime = std::make_shared<SourceSkillshot>(
            native, data, detectionType, ++m_nextId);
        runtime->FromFog = native->Caster.IsValid() && !native->Caster.IsVisible();
        if (const auto* missile = dynamic_cast<const SDK::SkillshotMissile*>(
                native.get()); missile && missile->Missile.IsValid()) {
            runtime->MissileNetworkId = missile->Missile.NetworkId();
        }
        return runtime;
    }

    SkillshotPtr Create(const SDK::AIBaseClient& caster,
                        const Vec3& rawStart,
                        const Vec3& rawEnd,
                        const Database::SpellData& source,
                        SourceDetectionType detectionType,
                        const SDK::MissileClient& missile = SDK::MissileClient(),
                        int overrideStartTick = 0,
                        bool allowDuplicate = false) {
        Vec2 start = ResolveStart(caster, rawStart.To2D());
        Vec2 end = ResolveEnd(source, caster, start, rawEnd.To2D(), {});
        if (start.IsZero() || end.IsZero()) {
            return {};
        }

        const float range = static_cast<float>(std::max(1, source.Runtime.Range));
        const auto player = SDK::ObjectManager::Player();
        if (!source.HasTrap && player.IsValid() &&
            player.ServerPosition().To2D().Distance(start) > range + 1200.0f) {
            return {};
        }

        Vec2 direction = ResolveDirection(caster, start, end);
        if (!missile.IsValid()) {
            if (source.Invert) {
                end = start - direction * start.Distance(end);
            }
            if (source.Centered) {
                start = start - direction * (range * 0.5f);
                end = start + direction * range;
            }
            if (source.IsPerpendicular && source.SecondaryRadius > 0) {
                const Vec2 perpendicular = SourceGeometry::Perpendicular(direction);
                const Vec2 center = end;
                start = center - perpendicular * static_cast<float>(source.SecondaryRadius);
                end = center + perpendicular * static_cast<float>(source.SecondaryRadius);
            }
            if (source.IsHorizontal) {
                const Vec2 center = end;
                const Vec2 perpendicular = SourceGeometry::Perpendicular(direction);
                start = center - perpendicular * range;
                end = center + perpendicular * range;
            }
        }

        SDK::SpellDatabaseEntry sdk = source.Runtime;
        const bool finite = sdk.MissileSpeed > 0 && sdk.MissileSpeed != INT_MAX;
        if (finite && sdk.SpellType == SDK::SpellType::SkillshotLine) {
            sdk.SpellType = SDK::SpellType::SkillshotMissileLine;
        } else if (finite && sdk.SpellType == SDK::SpellType::SkillshotCircle) {
            sdk.SpellType = SDK::SpellType::SkillshotMissileCircle;
        }
        sdk.AvoidMaxRangeReduction = true;
        sdk.FixedRange = false;
        sdk.ExtraRange = 0;

        auto native = CreateNative(sdk);
        if (!native) {
            return {};
        }
        native->DetectionType = detectionType == SourceDetectionType::MissileCreate
            ? SDK::SkillshotDetectionType::MissileCreate
            : SDK::SkillshotDetectionType::ProcessSpell;
        native->Caster = caster;
        native->StartPosition = start;
        native->EndPosition = end;
        native->Direction = (end - start).Normalized();
        native->StartTime = overrideStartTick != 0
            ? overrideStartTick
            : SDK::Variables::TickCount() - SDK::Game::Ping() / 2;
        if (missile.IsValid()) {
            if (auto* missileShape = dynamic_cast<SDK::SkillshotMissile*>(native.get())) {
                missileShape->Missile = missile;
            }
        }
        SpecialSpells::RefreshSkillshotGeometry(*native);

        SkillshotPtr runtime = CreateRuntime(native, source, detectionType);
        if (!runtime) {
            return {};
        }
        if (!Add(runtime, allowDuplicate)) {
            return {};
        }
        return runtime;
    }

    bool Add(const SkillshotPtr& candidate, bool allowDuplicate) {
        if (!candidate || !candidate->Native) {
            return false;
        }
        if (!allowDuplicate) {
            for (const SkillshotPtr& existing : m_skillshots) {
                if (!existing || !existing->Native ||
                    !SameText(existing->SpellData().SpellName,
                              candidate->SpellData().SpellName) ||
                    existing->Native->Caster.NetworkId() !=
                        candidate->Native->Caster.NetworkId()) {
                    continue;
                }
                const int delta = std::abs(existing->StartTick() - candidate->StartTick());
                const float angle = SDK::Skillshot::AngleBetween(
                    existing->Direction(), candidate->Direction());
                const float startDistance = existing->Start().Distance(candidate->Start());
                if (candidate->DetectionType == SourceDetectionType::MissileCreate &&
                    delta <= 450 && angle <= 12.0f && startDistance <= 350.0f) {
                    existing->Data = candidate->Data;
                    existing->DetectionType = candidate->DetectionType;
                    existing->Native->SData = candidate->Native->SData;
                    existing->Native->StartPosition = candidate->Native->StartPosition;
                    existing->Native->EndPosition = candidate->Native->EndPosition;
                    existing->Native->Direction = candidate->Native->Direction;
                    existing->Native->StartTime = candidate->Native->StartTime;
                    existing->OriginalEnd = candidate->OriginalEnd;
                    existing->CollisionEnd = candidate->CollisionEnd;
                    existing->MissileNetworkId = candidate->MissileNetworkId;
                    auto* oldMissile = dynamic_cast<SDK::SkillshotMissile*>(
                        existing->Native.get());
                    const auto* newMissile = dynamic_cast<const SDK::SkillshotMissile*>(
                        candidate->Native.get());
                    if (oldMissile && newMissile) {
                        oldMissile->Missile = newMissile->Missile;
                    }
                    SpecialSpells::RefreshSkillshotGeometry(*existing->Native);
                    return false;
                }
                if (delta <= 160 && angle <= 2.0f && startDistance <= 100.0f) {
                    return false;
                }
            }
        }
        m_skillshots.push_back(candidate);
        return true;
    }

    void RemoveDetectedSpell(int casterNetworkId, const char* spellName) {
        m_skillshots.erase(std::remove_if(
            m_skillshots.begin(), m_skillshots.end(),
            [&](const SkillshotPtr& skillshot) {
                return skillshot && skillshot->Native &&
                    skillshot->Native->Caster.NetworkId() == casterNetworkId &&
                    SameText(skillshot->SpellData().SpellName, spellName);
            }), m_skillshots.end());
    }

    void UpdateDynamicGeometry(SourceSkillshot& skillshot) {
        if (!skillshot.Native) {
            return;
        }
        SDK::Skillshot& native = *skillshot.Native;
        if (skillshot.Data.FollowCaster && native.Caster.IsValid()) {
            const Vec2 position = native.Caster.ServerPosition().To2D();
            if (SDK::IsCircleSpellType(native.SData.SpellType)) {
                native.EndPosition = position;
            } else {
                native.StartPosition = position;
                native.EndPosition = position + native.Direction *
                    static_cast<float>(std::max(1, native.SData.Range));
            }
            skillshot.OriginalEnd = native.EndPosition;
            skillshot.CollisionEnd = native.EndPosition;
            SpecialSpells::RefreshSkillshotGeometry(native);
        }
        if (native.SData.MissileFollowsCaster && native.Caster.IsValid() &&
            native.Caster.IsVisible()) {
            native.EndPosition = native.Caster.ServerPosition().To2D();
            native.Direction = (native.EndPosition - native.StartPosition).Normalized();
            skillshot.OriginalEnd = native.EndPosition;
            skillshot.CollisionEnd = native.EndPosition;
            SpecialSpells::RefreshSkillshotGeometry(native);
        }
    }

    void CleanupTraps() {
        std::unordered_set<const SourceSkillshot*> active;
        active.reserve(m_skillshots.size());
        for (const SkillshotPtr& skillshot : m_skillshots) {
            if (skillshot) {
                active.insert(skillshot.get());
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
};

} // namespace Plugins::KuroEvade
