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
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
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
        if (m_collisionEnabled && !enabled) {
            for (const auto& skillshot : m_skillshots) {
                if (skillshot && !skillshot->ProjectileTerminated) {
                    ResetCollision(*skillshot);
                }
            }
        }
        m_collisionEnabled = enabled;
    }

    void SetCollisionTypes(bool minions, bool heroes, bool projectileWalls) {
        m_minionCollision = minions;
        m_heroCollision = heroes;
        m_projectileWallCollision = projectileWalls;
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
    void ReconcileLifetimes(int now) {
        std::vector<SDK::Events::ObjectEventArgs> terminations;
        std::unordered_set<int> missingIds;
        terminations.reserve(m_skillshots.size());

        for (const SkillshotPtr& skillshot : m_skillshots) {
            if (!skillshot || !skillshot->Native ||
                skillshot->MissileNetworkId == 0) {
                continue;
            }
            auto* missile = dynamic_cast<SDK::SkillshotMissile*>(
                skillshot->Native.get());
            if (!missile) {
                continue;
            }

            const int missileId = skillshot->MissileNetworkId;
            if (!SDK::GameObjects::IsNetworkIdAlive(
                    static_cast<std::uint32_t>(missileId))) {
                if (!missingIds.insert(missileId).second) {
                    continue;
                }
                SDK::Events::ObjectEventArgs event{};
                event.Sender.NetworkId = static_cast<std::uint32_t>(missileId);
                event.Sender.Type =
                    ::Core::Objects::ObjectType::MissileClient;
                event.Sender.Position =
                    Vec3::From2D(skillshot->LastMissilePosition);
                event.Sender.IdentityOnly = true;
                event.MissileNetworkId =
                    static_cast<std::uint32_t>(missileId);
                event.TargetNetworkId =
                    static_cast<std::uint32_t>(
                        std::max(0, skillshot->LastMissileTargetNetworkId));
                event.Target.NetworkId = event.TargetNetworkId;
                event.Target.IdentityOnly = event.TargetNetworkId != 0;
                if (skillshot->Native->Caster.IsValid()) {
                    const int casterId =
                        skillshot->Native->Caster.NetworkId();
                    event.SourceNetworkId =
                        static_cast<std::uint32_t>(std::max(0, casterId));
                    event.Source.NetworkId = event.SourceNetworkId;
                    event.Source.IdentityOnly = event.SourceNetworkId != 0;
                }
                terminations.push_back(event);
                continue;
            }

            const SDK::MissileClient liveMissile =
                SDK::GameObjects::GetUnitByNetworkId<SDK::MissileClient>(
                    missileId);
            if (liveMissile.IsValid()) {
                missile->Missile = liveMissile;
                skillshot->LastMissilePosition =
                    liveMissile.Position().To2D();
                skillshot->LastMissileTargetNetworkId =
                    liveMissile.TargetNetworkId();
                skillshot->LastMissileSeenTick = now;
            }
        }

        for (const auto& event : terminations) {
            ProcessMissileTermination(event);
        }

        for (auto it = m_traps.begin(); it != m_traps.end();) {
            if (it->first != 0 &&
                !SDK::GameObjects::IsNetworkIdAlive(
                    static_cast<std::uint32_t>(it->first))) {
                const SkillshotPtr trap = it->second;
                m_skillshots.erase(
                    std::remove(m_skillshots.begin(), m_skillshots.end(), trap),
                    m_skillshots.end());
                it = m_traps.erase(it);
            } else {
                ++it;
            }
        }
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
        ReconcileLifetimes(now);

        m_skillshots.erase(std::remove_if(
            m_skillshots.begin(), m_skillshots.end(),
            [&](const SkillshotPtr& skillshot) {
                if (!skillshot || !skillshot->Native) {
                    return true;
                }
                UpdateAttachedExplosion(*skillshot, now);
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
                // ReconcileLifetimes refreshes live handles before this pass.
                // Terminated projectiles keep frozen geometry only.
                if (!skillshot->ProjectileTerminated) {
                    skillshot->Native->Game_OnUpdate();
                }
                const auto* missile =
                    dynamic_cast<const SDK::SkillshotMissile*>(
                        skillshot->Native.get());
                // A predicted blocker can move away just before impact. Do
                // not expire a still-live authoritative missile from the
                // shorter predicted collision time.
                const bool hasLiveMissile = missile &&
                    skillshot->MissileNetworkId != 0 &&
                    SDK::GameObjects::IsNetworkIdAlive(
                        static_cast<std::uint32_t>(
                            skillshot->MissileNetworkId)) &&
                    !skillshot->ProjectileTerminated;
                return !hasLiveMissile && !skillshot->IsActive(now);
            }), m_skillshots.end());
        SpecialSpells::EndUpdate();

        if (m_collisionEnabled && now - m_lastCollisionTick >= 50) {
            for (const auto& skillshot : m_skillshots) {
                if (skillshot) {
                    SourceCollision::Update(*skillshot, m_minionCollision,
                                            m_heroCollision,
                                            m_projectileWallCollision);
                }
            }
            m_lastCollisionTick = now;
        }
        CleanupTraps();
    }

    static const Database::SpellData* LookupBySpellName(const char* name) {
        return FindBySpellName(name);
    }

    static const Database::SpellData* LookupByMissileName(const char* name) {
        return FindByMissileName(name);
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
                   SDK::MissileClient(), extra.OverrideStartTick,
                   extra.AllowDuplicate);
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
        SDK::AIBaseClient caster = GameObjects::GetUnitByNetworkId<SDK::AIBaseClient>(
            static_cast<int>(args.Sender.NetworkId));
        if (!IsValidCaster(caster)) {
            return;
        }
        const auto* data = FindCastSpellData(caster, args.Slot);
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

        // Missile ownership is authoritative. Mel destroys the incoming
        // projectile and recreates it as a new missile cast by herself; some
        // object-create payloads can still retain the original source object.
        SDK::AIBaseClient caster;
        if (missile.CasterNetworkId() != 0) {
            caster = GameObjects::GetUnitByNetworkId<SDK::AIBaseClient>(
                missile.CasterNetworkId());
        }
        if (!caster.IsValid()) {
            caster = MakeCaster(args.Source);
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

    void ProcessMissileTermination(const SDK::Events::ObjectEventArgs& args) {
        struct ProjectileTerminationCancellation {
            int CasterNetworkId = 0;
            int StartTick = 0;
            std::string PrimarySpellName;
        };
        std::vector<ProjectileTerminationCancellation> cancellations;
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
                if (!matches || (!skillshot->SpellData().CanBeRemoved &&
                                 !skillshot->SpellData().ForceRemove)) {
                    return false;
                }

                const Vec2 intendedEnd = skillshot->CollisionEnd.IsZero()
                    ? skillshot->Native->EndPosition
                    : skillshot->CollisionEnd;
                const Vec2 impact = args.Sender.Position.To2D();
                if (!impact.IsZero() && impact.IsValid()) {
                    skillshot->CollisionEnd = impact;
                    skillshot->Native->EndPosition = impact;
                }

                // A missile can be created and destroyed between two frame
                // reconciliations. Prefer the last observed position so
                // collision-triggered explosions are not silently lost.
                SDK::AIBaseClient collisionTarget = MakeCaster(args.Target);
                if (!collisionTarget.IsValid()) {
                    collisionTarget = FindImpactUnit(*skillshot, impact);
                }
                if (collisionTarget.IsValid()) {
                    const Vec2 center = collisionTarget.Position().To2D();
                    skillshot->CollisionKind = SourceCollisionKind::Unit;
                    skillshot->CollisionStopped = true;
                    skillshot->CollisionHitCount = std::max(
                        1, skillshot->CollisionHitCount);
                    SourceCollision::ApplyUnitImpactProfile(*skillshot,
                        { 0.0f, impact, SourceCollisionKind::Unit,
                          collisionTarget.NetworkId(), center,
                          collisionTarget.IsHero() });
                    if (skillshot->Data.EndExplosionDetonatesOnUnitDeath &&
                        collisionTarget.IsDead()) {
                        skillshot->CollisionEndExplosionDelay = 0;
                    }
                } else if (args.TargetNetworkId != 0 ||
                           args.Target.IsValid()) {
                    Vec2 center = args.Target.Position.To2D();
                    if (center.IsZero() || !center.IsValid()) {
                        center = impact;
                    }
                    const int targetId = args.TargetNetworkId != 0
                        ? static_cast<int>(args.TargetNetworkId)
                        : static_cast<int>(args.Target.NetworkId);
                    const bool isChampion = args.Target.Type ==
                        ::Core::Objects::ObjectType::AIHeroClient;
                    skillshot->CollisionKind = SourceCollisionKind::Unit;
                    skillshot->CollisionStopped = true;
                    skillshot->CollisionHitCount = std::max(
                        1, skillshot->CollisionHitCount);
                    SourceCollision::ApplyUnitImpactProfile(*skillshot,
                        { 0.0f, impact, SourceCollisionKind::Unit,
                          targetId, center, isChampion });
                    if (skillshot->Data.EndExplosionDetonatesOnUnitDeath &&
                        args.Target.IsDead) {
                        skillshot->CollisionEndExplosionDelay = 0;
                    }
                }
                if (!collisionTarget.IsValid() &&
                    skillshot->CollisionKind != SourceCollisionKind::Unit &&
                    IsImpactOnProjectileWall(*skillshot, impact)) {
                    skillshot->CollisionKind =
                        SourceCollisionKind::ProjectileWall;
                    skillshot->CollisionStopped = true;
                    skillshot->CollisionUnitNetworkId = 0;
                    skillshot->CollisionUnitCenter = {};
                    skillshot->CollisionExplosionCenter = {};
                }
                const bool terminatedBeforeSegmentEnd =
                    !impact.IsZero() && impact.IsValid() &&
                    !intendedEnd.IsZero() && intendedEnd.IsValid() &&
                    impact.Distance(intendedEnd) > std::max(
                        90.0f, skillshot->RawRadius() * 0.5f);
                if (SpecialSpells::HasProjectileTerminationDependents(
                        skillshot->SpellData().SpellName) &&
                    (skillshot->CollisionKind ==
                         SourceCollisionKind::ProjectileWall ||
                     terminatedBeforeSegmentEnd)) {
                    cancellations.push_back({
                        skillshot->Native->Caster.NetworkId(),
                        skillshot->StartTick(),
                        skillshot->SpellData().SpellName });
                }
                const bool remainActiveOnGround = skillshot->IsActive() ||
                    skillshot->ExtraDurationMs() > 0;
                if (!skillshot->HasEndExplosionArea() && !remainActiveOnGround) {
                    return true;
                }
                skillshot->ProjectileTerminated = true;
                skillshot->ProjectileTerminationTick =
                    SDK::Variables::TickCount();
                skillshot->MissileNetworkId = 0;
                SpecialSpells::RefreshSkillshotGeometry(*skillshot->Native);
                return false;
            }), m_skillshots.end());
        if (!cancellations.empty()) {
            m_skillshots.erase(std::remove_if(
                m_skillshots.begin(), m_skillshots.end(),
                [&](const SkillshotPtr& skillshot) {
                    if (!skillshot || !skillshot->Native) {
                        return true;
                    }
                    for (const auto& cancellation : cancellations) {
                        if (skillshot->Native->Caster.NetworkId() !=
                                cancellation.CasterNetworkId ||
                            std::abs(skillshot->StartTick() -
                                cancellation.StartTick) > 5000) {
                            continue;
                        }
                        if (SpecialSpells::IsProjectileTerminationDependent(
                                cancellation.PrimarySpellName,
                                skillshot->SpellData().SpellName)) {
                            return true;
                        }
                    }
                    return false;
                }), m_skillshots.end());
        }
        CleanupTraps();
    }

    void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
        if (!m_enhancedDetection) {
            return;
        }
        if (!args.Sender.Ptr) {
            return;
        }
        if (args.Sender.Type != ::Core::Objects::ObjectType::AIMinionClient &&
            args.Sender.Type != ::Core::Objects::ObjectType::EffectEmitter) {
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
            if (!hero.IsValid()) {
                continue;
            }
            const std::string heroName =
                EvadeUtils::GetObjectCharacterName(hero);
            const SDK::ChampionId heroChampionId =
                SDK::ChampionIdFromName(heroName.c_str());
            if (data->IsGlobal ||
                (data->ChampionId != SDK::ChampionId::Unknown &&
                 heroChampionId == data->ChampionId)) {
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


private:
    SkillshotList m_skillshots;
    std::unordered_map<int, SkillshotPtr> m_traps;
    int m_nextId = 0;
    int m_lastCollisionTick = 0;
    bool m_collisionEnabled = true;
    bool m_minionCollision = true;
    bool m_heroCollision = true;
    bool m_projectileWallCollision = true;
    bool m_fowEnabled = true;
    bool m_sameTeam = false;
    bool m_enhancedDetection = true;

    bool IsValidCaster(const SDK::AIBaseClient& caster) const {
        if (!caster.IsValid()) {
            return false;
        }
        const auto player = GameObjects::Player();
        if (player.IsValid() && caster.NetworkId() == player.NetworkId()) {
            return m_sameTeam;
        }
        return !caster.IsAlly() || m_sameTeam;
    }

    static SDK::AIBaseClient MakeCaster(const ::Core::Events::ObjectInfo& info) {
        if (info.NetworkId != 0 && info.NetworkId != 0xFFFFFFFFu) {
            auto caster = GameObjects::GetUnitByNetworkId<SDK::AIBaseClient>(
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

    static SDK::AIBaseClient FindImpactUnit(
            const SourceSkillshot& skillshot,
            const Vec2& impact) {
        if (!skillshot.Native || impact.IsZero() || !impact.IsValid()) {
            return {};
        }
        const auto has = [&](SDK::CollisionableObjects type) {
            return std::find(
                skillshot.SpellData().CollisionObjects.begin(),
                skillshot.SpellData().CollisionObjects.end(), type) !=
                    skillshot.SpellData().CollisionObjects.end();
        };
        const auto hasAuthored = [&](Database::CollisionObjectType type) {
            return std::find(skillshot.Data.CollisionObjects.begin(),
                             skillshot.Data.CollisionObjects.end(), type) !=
                   skillshot.Data.CollisionObjects.end();
        };

        SDK::AIBaseClient best;
        float bestGap = FLT_MAX;
        const auto consider = [&](const SDK::AIBaseClient& unit) {
            if (!unit.IsValid() || unit.NetworkId() == 0 ||
                unit.NetworkId() == skillshot.Native->Caster.NetworkId()) {
                return;
            }
            const Vec2 center = unit.Position().To2D();
            if (center.IsZero() || !center.IsValid()) {
                return;
            }
            const float gap = impact.Distance(center) -
                std::max(0.0f, unit.BoundingRadius());
            if (gap > skillshot.RawRadius() + 45.0f || gap >= bestGap) {
                return;
            }
            best = unit;
            bestGap = gap;
        };

        const bool casterIsAlly = skillshot.Native->Caster.IsAlly();
        if (has(SDK::CollisionableObjects::Heroes)) {
            if (casterIsAlly) {
                for (const auto& hero : SDK::GameObjects::EnemyHeroes()) {
                    consider(SDK::AIBaseClient(hero.Handle()));
                }
            } else {
                for (const auto& hero : SDK::GameObjects::AllyHeroes()) {
                    consider(SDK::AIBaseClient(hero.Handle()));
                }
                const auto player = GameObjects::Player();
                if (player.IsValid()) {
                    consider(SDK::AIBaseClient(player.Handle()));
                }
            }
        }

        if (has(SDK::CollisionableObjects::Minions)) {
            const bool laneMinions = hasAuthored(
                Database::CollisionObjectType::EnemyMinions);
            const bool largeMonstersOnly = hasAuthored(
                    Database::CollisionObjectType::EnemyLargeMonsters) &&
                !laneMinions;
            if (laneMinions && !skillshot.Data.CollisionExceptMini) {
                if (casterIsAlly) {
                    for (const auto& minion : SDK::GameObjects::EnemyMinions()) {
                        consider(SDK::AIBaseClient(minion.Handle()));
                    }
                } else {
                    for (const auto& minion : SDK::GameObjects::AllyMinions()) {
                        consider(SDK::AIBaseClient(minion.Handle()));
                    }
                }
            }
            for (const auto& minion : SDK::GameObjects::Jungle()) {
                if (largeMonstersOnly) {
                    const SDK::JungleType type = minion.GetJungleType();
                    if (type != SDK::JungleType::Large &&
                        type != SDK::JungleType::Epic &&
                        type != SDK::JungleType::Legendary) {
                        continue;
                    }
                }
                consider(SDK::AIBaseClient(minion.Handle()));
            }
        }
        return best;
    }

    static bool IsImpactOnProjectileWall(
            const SourceSkillshot& skillshot,
            const Vec2& impact) {
        if (!skillshot.Native || impact.IsZero() || !impact.IsValid()) {
            return false;
        }
        const auto has = [&](SDK::CollisionableObjects type) {
            return std::find(skillshot.SpellData().CollisionObjects.begin(),
                             skillshot.SpellData().CollisionObjects.end(),
                             type) !=
                   skillshot.SpellData().CollisionObjects.end();
        };
        const bool yasuo = has(SDK::CollisionableObjects::YasuoWall);
        const bool samira = has(SDK::CollisionableObjects::SamiraWall);
        const bool mel = has(SDK::CollisionableObjects::MelWall);
        if (!yasuo && !samira && !mel) {
            return false;
        }
        const SDK::GameObjectTeam casterTeam =
            skillshot.Native->Caster.IsValid()
                ? skillshot.Native->Caster.Team()
                : SDK::GameObjectTeam::Unknown;
        if (yasuo) {
            for (const auto& wall : SDK::YasuoWallTracker::ActiveWalls()) {
                const SDK::GameObject wallObject(wall.main);
                const SDK::GameObjectTeam wallTeam = wallObject.IsValid()
                    ? wallObject.Team()
                    : SDK::GameObjectTeam::Unknown;
                if (wallTeam != SDK::GameObjectTeam::Unknown &&
                    casterTeam != SDK::GameObjectTeam::Unknown &&
                    wallTeam == casterTeam) {
                    continue;
                }
                if (SourceGeometry::PointSegmentDistance(
                        impact, wall.start.To2D(), wall.end.To2D()) <=
                    skillshot.RawRadius() + 100.0f) {
                    return true;
                }
            }
        }
        return SourceCollision::IsNearActiveCircularBarrier(
            skillshot.Native->Caster, impact,
            skillshot.RawRadius() + 35.0f, samira, mel);
    }

    static void UpdateAttachedExplosion(SourceSkillshot& skillshot,
                                        int now) {
        if (!skillshot.ProjectileTerminated ||
            !skillshot.Data.EndExplosionAtUnitCenter ||
            skillshot.CollisionUnitNetworkId == 0) {
            return;
        }
        const SDK::AIBaseClient target =
            GameObjects::GetUnitByNetworkId<SDK::AIBaseClient>(
                skillshot.CollisionUnitNetworkId);
        if (!target.IsValid()) {
            return;
        }
        if (skillshot.Data.EndExplosionFollowsUnit) {
            const Vec2 center = target.Position().To2D();
            if (!center.IsZero() && center.IsValid()) {
                skillshot.CollisionUnitCenter = center;
            }
        }
        // Senna W detonates immediately if its attached target dies. Apply the
        // same timing transition once rather than leaving a stale one-second
        // danger timer at the corpse's old position.
        if (skillshot.Data.EndExplosionDetonatesOnUnitDeath &&
            target.IsDead() && now < skillshot.EndExplosionImpactTick()) {
            skillshot.ProjectileTerminationTick = now;
            skillshot.CollisionEndExplosionDelay = 0;
        }
    }

    static void ResetCollision(SourceSkillshot& skillshot) {
        if (!skillshot.Native || skillshot.OriginalEnd.IsZero()) {
            return;
        }
        const bool geometryChanged = skillshot.Native->EndPosition.DistanceSqr(
            skillshot.OriginalEnd) > 1.0f ||
            skillshot.Native->SData.Radius != skillshot.Data.Runtime.Radius;
        skillshot.CollisionEnd = skillshot.OriginalEnd;
        skillshot.CollisionUnitCenter = {};
        skillshot.CollisionExplosionCenter = {};
        skillshot.CollisionKind = SourceCollisionKind::None;
        skillshot.CollisionUnitNetworkId = 0;
        skillshot.CollisionHitCount = 0;
        skillshot.CollisionStopped = false;
        skillshot.CollisionEndExplosionRadius = 0.0f;
        skillshot.CollisionEndExplosionDelay = -1;
        skillshot.PendingUnitCollisions.clear();
        skillshot.ConsumedCollisionUnits.clear();
        skillshot.LastConsumedCollisionPoint = {};
        skillshot.TerrainCollisionCached = false;
        skillshot.TerrainCollisionPathStart = {};
        skillshot.TerrainCollisionPathEnd = {};
        skillshot.TerrainCollisionPoint = {};
        skillshot.TerrainCollisionProbeRadius = -1.0f;
        skillshot.Native->SData.Radius = skillshot.Data.Runtime.Radius;
        if (!geometryChanged) {
            return;
        }
        skillshot.Native->EndPosition = skillshot.OriginalEnd;
        const Vec2 direction = (skillshot.OriginalEnd -
            skillshot.Native->StartPosition).Normalized();
        if (!direction.IsZero()) {
            skillshot.Native->Direction = direction;
        }
        SpecialSpells::RefreshSkillshotGeometry(*skillshot.Native);
    }

    static bool ContainsInsensitive(std::string_view text, std::string_view value) {
        if (text.empty() || value.empty()) {
            return false;
        }
        auto it = std::search(
            text.begin(), text.end(),
            value.begin(), value.end(),
            [](char c1, char c2) {
                return std::tolower(static_cast<unsigned char>(c1)) ==
                       std::tolower(static_cast<unsigned char>(c2));
            }
        );
        return it != text.end();
    }

    static bool IsBasicAttackName(const char* name) {
        if (!name || !name[0]) {
            return false;
        }
        std::string_view value(name);
        return ContainsInsensitive(value, "basicattack") ||
               ContainsInsensitive(value, "critattack") ||
               SDK::Orbwalker::IsAutoAttack(name);
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

    static bool IsUtilityName(const char* name) {
        if (!name || !name[0]) {
            return false;
        }
        return ContainsInsensitive(name, "summoner") || ContainsInsensitive(name, "item");
    }

    static const Database::SpellData* FindProcessSpellData(
        const SDK::Events::ProcessSpellEventArgs& args) {
        if (args.IsAutoAttack ||
            IsBasicAttackName(args.SpellName) ||
            IsBasicAttackName(args.PayloadSpellName) ||
            IsBasicAttackName(args.ScriptName) ||
            IsBasicAttackName(args.SpellSlotName) ||
            IsBasicAttackName(args.MissileName) ||
            IsBasicAttackName(args.PayloadMissileName)) {
            return nullptr;
        }

        const char* names[] = {
            args.SpellName,
            args.PayloadSpellName,
            args.ScriptName,
            args.MissileName,
            args.PayloadMissileName,
        };

        bool utilitySpell = false;
        for (const char* name : names) {
            if (name && name[0] && IsUtilityName(name)) {
                utilitySpell = true;
                break;
            }
        }

        for (const char* name : names) {
            if (!name || !name[0]) {
                continue;
            }
            if (utilitySpell && !IsUtilityName(name)) {
                continue;
            }
            if (const auto* data = FindBySpellName(name);
                data && !data->DontProcess) {
                return data;
            }
            if (const auto* data = FindByMissileName(name);
                data && !data->DontProcess) {
                return data;
            }
        }

        return nullptr;
    }

    static const Database::SpellData* FindUniqueByChampionAndSlot(
        SDK::ChampionId championId, int slot) {
        if (championId == SDK::ChampionId::Unknown ||
            slot < 0 || slot > 3) {
            return nullptr;
        }
        const SDK::SpellSlot target = static_cast<SDK::SpellSlot>(slot);
        const Database::SpellData* result = nullptr;
        for (const auto& entry : Database::SpellDatabase::Spells()) {
            if (entry.DontProcess || entry.IsGlobal ||
                entry.ChampionId != championId ||
                entry.Runtime.Slot != target) {
                continue;
            }
            if (result) {
                return nullptr;
            }
            result = &entry;
        }
        return result;
    }

    static const Database::SpellData* FindCastSpellData(
            const SDK::AIBaseClient& caster,
            int slot) {
        if (!caster.IsValid() || slot < 0 || slot > 3) {
            return nullptr;
        }

        // Sylas and Viego dynamically replace a spellbook slot. Resolve the
        // live slot resource before falling back to champion+slot so the
        // low-level CastSpell event remains useful even when ProcessSpell is
        // unavailable. This also covers any future spell-copy mechanic.
        const SDK::SpellDataInstClient instance = caster.Spellbook().GetSpell(
            static_cast<SDK::SpellSlot>(slot));
        if (instance.IsValid()) {
            const std::string names[] = {
                instance.Name(),
                instance.ScriptName(),
            };
            for (const std::string& name : names) {
                if (const auto* data = FindBySpellName(name.c_str());
                    data && !data->DontProcess) {
                    return data;
                }
                if (const auto* data = FindByMissileName(name.c_str());
                    data && !data->DontProcess) {
                    return data;
                }
            }
        }

        return nullptr;
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
        return caster.IsValid() ? caster.Position().To2D() : Vec2();
    }

    static Vec2 ResolveDirection(const SDK::AIBaseClient& caster,
                                 const Vec2& start,
                                 const Vec2& end) {
        Vec2 direction = (end - start).Normalized();
        if (direction.IsZero() && caster.IsValid()) {
            direction = caster.Direction().To2D().Normalized();
        }
        if (direction.IsZero()) {
            const auto player = GameObjects::Player();
            if (player.IsValid()) {
                direction = (player.Position().To2D() - start).Normalized();
            }
        }
        return direction.IsZero() ? Vec2(1.0f, 0.0f) : direction;
    }

    static Vec2 ResolveEnd(const Database::SpellData& data,
                           const SDK::AIBaseClient& caster,
                           const Vec2& start,
                           const Vec2& primary,
                           const Vec2& secondary) {
        Vec2 end = !primary.IsZero() ? primary : secondary;
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
            runtime->LastMissilePosition =
                missile->Missile.Position().To2D();
            runtime->LastMissileTargetNetworkId =
                missile->Missile.TargetNetworkId();
            runtime->LastMissileSeenTick = SDK::Variables::TickCount();
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
        const auto player = GameObjects::Player();
        if (!source.HasTrap && player.IsValid() &&
            player.Position().To2D().Distance(start) > range + 1200.0f) {
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
                    existing->Native->Caster.NetworkId() !=
                        candidate->Native->Caster.NetworkId()) {
                    continue;
                }
                const bool sameSpell = SameText(
                    existing->SpellData().SpellName,
                    candidate->SpellData().SpellName);
                const bool sameDetectionGroup =
                    !existing->Data.DetectionGroup.empty() &&
                    !candidate->Data.DetectionGroup.empty() &&
                    SameText(existing->Data.DetectionGroup,
                             candidate->Data.DetectionGroup);
                if (!sameSpell && !sameDetectionGroup) {
                    continue;
                }
                const int delta = std::abs(existing->StartTick() - candidate->StartTick());
                const float angle = SDK::Skillshot::AngleBetween(
                    existing->Direction(), candidate->Direction());
                const float startDistance = existing->Start().Distance(candidate->Start());
                const bool delayedInternalMissile = sameSpell &&
                    existing->Data.DontProcess &&
                    candidate->Data.DontProcess && delta <= 3500 &&
                    angle <= 12.0f && startDistance <= 150.0f;
                if (candidate->DetectionType == SourceDetectionType::MissileCreate &&
                    ((delta <= 450 && angle <= 12.0f &&
                      startDistance <= 350.0f) ||
                     delayedInternalMissile)) {
                    existing->Data = candidate->Data;
                    existing->DetectionType = candidate->DetectionType;
                    existing->Native->SData = candidate->Native->SData;
                    existing->Native->StartPosition = candidate->Native->StartPosition;
                    existing->Native->EndPosition = candidate->Native->EndPosition;
                    existing->Native->Direction = candidate->Native->Direction;
                    existing->Native->StartTime = candidate->Native->StartTime;
                    existing->OriginalEnd = candidate->OriginalEnd;
                    existing->CollisionEnd = candidate->CollisionEnd;
                    existing->CollisionUnitCenter = candidate->CollisionUnitCenter;
                    existing->CollisionExplosionCenter =
                        candidate->CollisionExplosionCenter;
                    existing->CollisionKind = candidate->CollisionKind;
                    existing->CollisionUnitNetworkId =
                        candidate->CollisionUnitNetworkId;
                    existing->CollisionHitCount = candidate->CollisionHitCount;
                    existing->CollisionStopped = candidate->CollisionStopped;
                    existing->CollisionEndExplosionRadius =
                        candidate->CollisionEndExplosionRadius;
                    existing->CollisionEndExplosionDelay =
                        candidate->CollisionEndExplosionDelay;
                    existing->TerrainCollisionCached = false;
                    existing->TerrainCollisionPathStart = {};
                    existing->TerrainCollisionPathEnd = {};
                    existing->TerrainCollisionPoint = {};
                    existing->TerrainCollisionProbeRadius = -1.0f;
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
            const Vec2 position = native.Caster.Position().To2D();
            if (SDK::IsCircleSpellType(native.SData.SpellType)) {
                native.EndPosition = position;
            } else {
                native.StartPosition = position;
                native.EndPosition = position + native.Direction *
                    static_cast<float>(std::max(1, native.SData.Range));
            }
            skillshot.OriginalEnd = native.EndPosition;
            skillshot.CollisionEnd = native.EndPosition;
            skillshot.CollisionUnitCenter = {};
            skillshot.CollisionExplosionCenter = {};
            skillshot.CollisionKind = SourceCollisionKind::None;
            skillshot.CollisionUnitNetworkId = 0;
            skillshot.CollisionHitCount = 0;
            skillshot.CollisionStopped = false;
            skillshot.CollisionEndExplosionRadius = 0.0f;
            skillshot.CollisionEndExplosionDelay = -1;
            skillshot.PendingUnitCollisions.clear();
            skillshot.ConsumedCollisionUnits.clear();
            skillshot.LastConsumedCollisionPoint = {};
            skillshot.TerrainCollisionCached = false;
            SpecialSpells::RefreshSkillshotGeometry(native);
        }
        if (native.SData.MissileFollowsCaster && native.Caster.IsValid() &&
            native.Caster.IsVisible()) {
            native.EndPosition = native.Caster.Position().To2D();
            native.Direction = (native.EndPosition - native.StartPosition).Normalized();
            skillshot.OriginalEnd = native.EndPosition;
            skillshot.CollisionEnd = native.EndPosition;
            skillshot.CollisionUnitCenter = {};
            skillshot.CollisionExplosionCenter = {};
            skillshot.CollisionKind = SourceCollisionKind::None;
            skillshot.CollisionUnitNetworkId = 0;
            skillshot.CollisionHitCount = 0;
            skillshot.CollisionStopped = false;
            skillshot.CollisionEndExplosionRadius = 0.0f;
            skillshot.CollisionEndExplosionDelay = -1;
            skillshot.PendingUnitCollisions.clear();
            skillshot.ConsumedCollisionUnits.clear();
            skillshot.LastConsumedCollisionPoint = {};
            skillshot.TerrainCollisionCached = false;
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
