#pragma once

#include "../Engine/Evader.h"
#include "../Engine/SkillshotDetector.h"

#include <algorithm>
#include <chrono>
#include <cfloat>
#include <cmath>
#include <memory>

namespace Plugins::KuroEvade::Benchmarking {

struct BenchmarkResult {
    int Iterations = 0;
    int PlansFound = 0;
    int LastCandidateCount = 0;
    int LastGeneratedCandidateCount = 0;
    int LastGradientSteps = 0;
    int TotalGradientSteps = 0;
    int LastOuterRingExits = 0;
    int LastInnerRingShelters = 0;
    int LastBaselineThreats = 0;
    int LastRemainingThreats = 0;
    int CoverageImprovingPlans = 0;
    int SkillshotDatabaseEntries = 0;
    int EvadeSpellDatabaseEntries = 0;
    int InvalidDatabaseEntries = 0;
    int CollisionProfileEntries = 0;
    int MultiHitCollisionEntries = 0;
    int ContinuationCollisionEntries = 0;
    int BouncingExplosionEntries = 0;
    int ProjectileWallEntries = 0;
    int EndExplosionEntries = 0;
    int SpecialGeometryEntries = 0;
    int CollisionRegressionChecks = 0;
    int CollisionRegressionPassed = 0;
    int DynamicCasterRegressionChecks = 0;
    int DynamicCasterRegressionPassed = 0;
    double TotalMicroseconds = 0.0;
    double AverageMicroseconds = 0.0;
    double MinimumMicroseconds = 0.0;
    double MaximumMicroseconds = 0.0;
    double AverageGradientSteps = 0.0;
    float LastWallClearance = 0.0f;
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
                spell.Runtime.Range < 0 || spell.Runtime.Radius < 0 ||
                (!spell.MissileSpellName.empty() &&
                 !spell.Runtime.CanBeRemoved) ||
                spell.CollisionTargetLimit < 1 ||
                (spell.CollisionTargetLimit > 1 &&
                 spell.CollisionObjects.empty()) ||
                (spell.HasEndExplosion && spell.SecondaryRadius <= 0.0f) ||
                spell.EndExplosionMinimumTravelDistance < 0.0f ||
                spell.EndExplosionDelay < 0 ||
                spell.EndExplosionDuration < 0 ||
                spell.EndExplosionMediumTravelDistance < 0.0f ||
                spell.EndExplosionFarTravelDistance < 0.0f ||
                spell.EndExplosionRadiusMedium < 0.0f ||
                spell.EndExplosionRadiusFar < 0.0f ||
                ((spell.EndExplosionRadiusMedium > 0.0f ||
                  spell.EndExplosionRadiusFar > 0.0f) &&
                 (!spell.HasEndExplosion ||
                  spell.EndExplosionMediumTravelDistance <= 0.0f ||
                  spell.EndExplosionFarTravelDistance <=
                      spell.EndExplosionMediumTravelDistance ||
                  spell.EndExplosionRadiusMedium < spell.SecondaryRadius ||
                  spell.EndExplosionRadiusFar <
                      spell.EndExplosionRadiusMedium)) ||
                (spell.EndExplosionMinimumTravelDistance > 0.0f &&
                 !spell.HasEndExplosion) ||
                (spell.EndExplosionDuration > 0 &&
                 !spell.HasEndExplosion) ||
                (spell.EndExplosionRequiresCollision &&
                 (!spell.HasEndExplosion ||
                  spell.CollisionObjects.empty())) ||
                (spell.EndExplosionRequiresCollision &&
                 spell.EndExplosionRequiresUnitCollision) ||
                (spell.EndExplosionOnProjectileWall &&
                 (!spell.HasEndExplosion ||
                  !spell.EndExplosionRequiresCollision ||
                  std::find(spell.CollisionObjects.begin(),
                            spell.CollisionObjects.end(),
                            Database::CollisionObjectType::EnemyYasuoWall) ==
                      spell.CollisionObjects.end())) ||
                ((spell.EndExplosionFollowsUnit ||
                  spell.EndExplosionDetonatesOnUnitDeath) &&
                 (!spell.HasEndExplosion ||
                  !spell.EndExplosionAtUnitCenter ||
                  std::find(spell.CollisionObjects.begin(),
                            spell.CollisionObjects.end(),
                            Database::CollisionObjectType::EnemyChampions) ==
                      spell.CollisionObjects.end())) ||
                spell.CollisionInitialRange < 0.0f ||
                spell.CollisionContinuationDistance < 0.0f ||
                spell.CollisionContinuationRange < 0.0f ||
                spell.CollisionContinuationRadius < 0.0f ||
                spell.CollisionBounceDistance < 0.0f ||
                spell.CollisionBounceDistanceNonChampion < 0.0f ||
                spell.EndExplosionRadiusNonChampion < 0.0f ||
                spell.EndExplosionDelayNonChampion < -1 ||
                spell.EndExplosionForwardLength < 0.0f ||
                spell.EndExplosionBackwardLength < 0.0f ||
                spell.EndExplosionSideLength < 0.0f ||
                spell.EndExplosionLongitudinalRadius < 0.0f ||
                spell.EndExplosionSideRadius < 0.0f ||
                (spell.EndExplosionCross &&
                 (!spell.HasEndExplosion ||
                  !spell.EndExplosionRequiresUnitCollision ||
                  (spell.EndExplosionForwardLength <= 0.0f &&
                   spell.EndExplosionBackwardLength <= 0.0f &&
                   spell.EndExplosionSideLength <= 0.0f))) ||
                (spell.CollisionBounceDistance > 0.0f &&
                 (!spell.HasEndExplosion ||
                  !spell.EndExplosionRequiresUnitCollision ||
                  spell.CollisionObjects.empty())) ||
                (spell.EndExplosionRequiresUnitCollision &&
                 (!spell.HasEndExplosion || spell.CollisionObjects.empty()))) {
                ++result.InvalidDatabaseEntries;
            }
            if (!spell.CollisionObjects.empty()) {
                ++result.CollisionProfileEntries;
            }
            if (spell.CollisionTargetLimit > 1) {
                ++result.MultiHitCollisionEntries;
            }
            if (spell.CollisionInitialRange > 0.0f) {
                ++result.ContinuationCollisionEntries;
            }
            if (spell.CollisionBounceDistance > 0.0f) {
                ++result.BouncingExplosionEntries;
            }
            if (std::find(spell.Runtime.CollisionObjects.begin(),
                          spell.Runtime.CollisionObjects.end(),
                          SDK::CollisionableObjects::YasuoWall) !=
                spell.Runtime.CollisionObjects.end()) {
                ++result.ProjectileWallEntries;
            }
            if (spell.HasEndExplosion) {
                ++result.EndExplosionEntries;
            }
            if (spell.IsSpecial) {
                ++result.SpecialGeometryEntries;
            }
        }
        for (const Database::EvadeSpellData& spell : evadeEntries) {
            if (spell.ChampionName.empty() || spell.Name.empty()) {
                ++result.InvalidDatabaseEntries;
            }
        }

        const auto collisionCheck = [&](bool passed) {
            ++result.CollisionRegressionChecks;
            if (passed) {
                ++result.CollisionRegressionPassed;
            }
        };
        const auto dynamicCasterCheck = [&](bool passed) {
            ++result.DynamicCasterRegressionChecks;
            if (passed) {
                ++result.DynamicCasterRegressionPassed;
            }
        };
        const auto unitEvent = [](float distance) {
            return SourceCollisionEvent{ distance, Vec2(distance, 1.0f),
                SourceCollisionKind::Unit, static_cast<int>(distance) };
        };
        const auto terminalEvent = [](float distance,
                                      SourceCollisionKind kind) {
            return SourceCollisionEvent{ distance, Vec2(distance, 1.0f),
                kind, 0 };
        };
        {
            const auto collision = SourceCollision::Resolve(
                { unitEvent(100.0f) }, 1);
            collisionCheck(collision.Stopped &&
                std::abs(collision.Stop.Distance - 100.0f) <= 0.01f);
        }
        {
            const auto collision = SourceCollision::Resolve(
                { unitEvent(100.0f) }, 2);
            collisionCheck(!collision.Stopped && collision.UnitHits == 1);
        }
        {
            const auto collision = SourceCollision::Resolve(
                { unitEvent(200.0f), unitEvent(100.0f) }, 2);
            collisionCheck(collision.Stopped && collision.UnitHits == 2 &&
                std::abs(collision.Stop.Distance - 200.0f) <= 0.01f);
        }
        {
            const auto collision = SourceCollision::Resolve(
                { unitEvent(100.0f), unitEvent(200.0f),
                  terminalEvent(150.0f,
                      SourceCollisionKind::ProjectileWall) }, 2);
            collisionCheck(collision.Stopped &&
                collision.Stop.Kind == SourceCollisionKind::ProjectileWall &&
                std::abs(collision.Stop.Distance - 150.0f) <= 0.01f);
        }
        collisionCheck(SourceCollision::IsTerminalBeforeContinuation(
                           SourceCollisionKind::Terrain) &&
            SourceCollision::IsTerminalBeforeContinuation(
                SourceCollisionKind::ProjectileWall) &&
            !SourceCollision::IsTerminalBeforeContinuation(
                SourceCollisionKind::Unit));

        // These lookups deliberately do not receive a caster champion. Sylas,
        // possessed Viego and Mel-reflected missiles must resolve the original
        // spell profile by runtime name alone.
        const Database::SpellData* stolenUltimate =
            SourceSkillshotDetector::LookupBySpellName(
                "EnchantedCrystalArrow");
        dynamicCasterCheck(stolenUltimate &&
            stolenUltimate->Runtime.Slot == SDK::SpellSlot::R &&
            _stricmp(stolenUltimate->CharacterName.c_str(), "Ashe") == 0);
        const Database::SpellData* possessedBasic =
            SourceSkillshotDetector::LookupBySpellName("LuxLightBinding");
        dynamicCasterCheck(possessedBasic &&
            possessedBasic->Runtime.Slot == SDK::SpellSlot::Q &&
            _stricmp(possessedBasic->CharacterName.c_str(), "Lux") == 0);
        const Database::SpellData* reflectedProjectile =
            SourceSkillshotDetector::LookupByMissileName(
                "LuxLightBindingMis");
        const auto hasBarrier = [&](SDK::CollisionableObjects object) {
            return reflectedProjectile &&
                std::find(
                    reflectedProjectile->Runtime.CollisionObjects.begin(),
                    reflectedProjectile->Runtime.CollisionObjects.end(),
                    object) !=
                reflectedProjectile->Runtime.CollisionObjects.end();
        };
        dynamicCasterCheck(reflectedProjectile &&
            hasBarrier(SDK::CollisionableObjects::YasuoWall) &&
            hasBarrier(SDK::CollisionableObjects::SamiraWall) &&
            hasBarrier(SDK::CollisionableObjects::MelWall));
        Vec2 barrierContact;
        float barrierDistance = FLT_MAX;
        dynamicCasterCheck(SourceCollision::FirstCircleContact(
                Vec2(100.0f, 100.0f), Vec2(1100.0f, 100.0f),
                Vec2(600.0f, 100.0f), 200.0f,
                barrierContact, barrierDistance) &&
            barrierContact.Distance(Vec2(400.0f, 100.0f)) <= 0.01f &&
            std::abs(barrierDistance - 300.0f) <= 0.01f);

        const auto findSpell = [&](const char* champion, const char* spellName)
                -> const Database::SpellData* {
            for (const Database::SpellData& spell : spellEntries) {
                if (_stricmp(spell.CharacterName.c_str(), champion) == 0 &&
                    _stricmp(spell.SpellName.c_str(), spellName) == 0) {
                    return &spell;
                }
            }
            return nullptr;
        };
        const Database::SpellData* jayce = findSpell("Jayce", "JayceShockBlast");
        collisionCheck(jayce && jayce->HasEndExplosion &&
            jayce->Runtime.CanBeRemoved &&
            std::abs(jayce->SecondaryRadius - 175.0f) <= 0.01f);
        const Database::SpellData* lux = findSpell("Lux", "LuxLightBinding");
        collisionCheck(lux && lux->CollisionTargetLimit == 2 &&
            lux->MissileSpellName == "LuxLightBindingMis");
        const Database::SpellData* alistar = findSpell("Alistar", "Pulverize");
        collisionCheck(alistar && !alistar->DisabledByDefault &&
            alistar->Delay == 250 && alistar->FollowCaster);
        const Database::SpellData* ornn = findSpell("Ornn", "OrnnE");
        collisionCheck(ornn && ornn->IsSpecial &&
            std::abs(ornn->Radius - 175.0f) <= 0.01f);
        const Database::SpellData* samiraQ = findSpell("Samira", "SamiraQGun");
        const Database::SpellData* samiraE = findSpell("Samira", "SamiraE");
        collisionCheck(samiraQ && samiraQ->IsSpecial && samiraE &&
            samiraE->IsSpecial);
        const Database::SpellData* veigar = findSpell("Veigar", "VeigarBalefulStrike");
        collisionCheck(veigar && veigar->CollisionTargetLimit == 2);
        const Database::SpellData* hwei = findSpell("Hwei", "HweiQQ");
        collisionCheck(hwei && hwei->HasEndExplosion &&
            std::abs(hwei->SecondaryRadius - 200.0f) <= 0.01f);
        const Database::SpellData* hweiR = findSpell("Hwei", "HweiR");
        collisionCheck(hweiR && hweiR->Runtime.MissileSpeed == 1400 &&
            hweiR->Runtime.Range == 1340 && hweiR->Delay == 250 &&
            hweiR->HasEndExplosion &&
            hweiR->EndExplosionRequiresUnitCollision &&
            hweiR->EndExplosionAtUnitCenter &&
            hweiR->EndExplosionFollowsUnit &&
            hweiR->EndExplosionDetonatesOnUnitDeath &&
            hweiR->EndExplosionDelay == 3000 &&
            std::abs(hweiR->SecondaryRadius - 500.0f) <= 0.01f);
        const Database::SpellData* senna = findSpell("Senna", "SennaW");
        collisionCheck(senna && senna->MissileSpellName == "SennaWMissile" &&
            senna->EndExplosionRequiresUnitCollision &&
            senna->EndExplosionAtUnitCenter &&
            senna->EndExplosionFollowsUnit &&
            senna->EndExplosionDetonatesOnUnitDeath &&
            senna->EndExplosionDelay == 1000);
        const Database::SpellData* bard = findSpell("Bard", "BardQ");
        collisionCheck(bard && bard->CollisionTargetLimit == 2 &&
            std::abs(bard->CollisionInitialRange - 850.0f) <= 0.01f &&
            std::abs(bard->CollisionContinuationDistance - 300.0f) <= 0.01f &&
            bard->CollisionContinuationStopsOnTerrain);
        const Database::SpellData* lissandra = findSpell("Lissandra", "LissandraQ");
        collisionCheck(lissandra && lissandra->MissileSpellName == "LissandraQMissile" &&
            std::abs(lissandra->CollisionContinuationRange - 950.0f) <= 0.01f &&
            std::abs(lissandra->CollisionContinuationRadius - 90.0f) <= 0.01f);
        const Database::SpellData* milio = findSpell("Milio", "MilioQ");
        if (milio) {
            const SourceCollisionSecondaryProfile championProfile =
                SourceCollision::ResolveSecondaryProfile(*milio, true);
            const SourceCollisionSecondaryProfile minionProfile =
                SourceCollision::ResolveSecondaryProfile(*milio, false);
            collisionCheck(milio->HasEndExplosion &&
                milio->EndExplosionRequiresUnitCollision &&
                std::abs(championProfile.BounceDistance - 140.0f) <= 0.01f &&
                std::abs(championProfile.ExplosionRadius - 250.0f) <= 0.01f &&
                championProfile.ExplosionDelay == 800 &&
                std::abs(minionProfile.BounceDistance - 340.0f) <= 0.01f &&
                std::abs(minionProfile.ExplosionRadius - 275.0f) <= 0.01f &&
                minionProfile.ExplosionDelay == 900);
        } else {
            collisionCheck(false);
        }
        const Database::SpellData* aatrox = findSpell("Aatrox", "AatroxW");
        const Database::SpellData* ezrealR = findSpell("Ezreal", "EzrealR");
        const auto hasProjectileWall = [](const Database::SpellData* spell) {
            return spell && std::find(
                spell->Runtime.CollisionObjects.begin(),
                spell->Runtime.CollisionObjects.end(),
                SDK::CollisionableObjects::YasuoWall) !=
                    spell->Runtime.CollisionObjects.end();
        };
        collisionCheck(hasProjectileWall(aatrox) &&
            hasProjectileWall(ezrealR) && hasProjectileWall(hweiR));
        const Database::SpellData* akshanQ =
            findSpell("Akshan", "AkshanQ");
        const Database::SpellData* caitlynQ = findSpell(
            "Caitlyn", "CaitlynPiltoverPeacemaker");
        const Database::SpellData* seraphineR =
            findSpell("Seraphine", "SeraphineR");
        collisionCheck(hasProjectileWall(akshanQ) &&
            hasProjectileWall(caitlynQ) &&
            hasProjectileWall(seraphineR));
        collisionCheck(
            hasProjectileWall(findSpell("Diana", "DianaQ")) &&
            hasProjectileWall(findSpell("Galio", "GalioQ")) &&
            hasProjectileWall(findSpell("Gragas", "GragasQ")) &&
            hasProjectileWall(findSpell("Gragas", "GragasR")) &&
            hasProjectileWall(findSpell("Graves", "GravesSmokeGrenade")) &&
            hasProjectileWall(findSpell("Heimerdinger", "HeimerdingerE")) &&
            hasProjectileWall(findSpell("Neeko", "NeekoQ")));
        const Database::SpellData* ziggsQ = findSpell("Ziggs", "ZiggsQ");
        const Database::SpellData* ziggsBounce1 =
            findSpell("Ziggs", "ZiggsQBounce1");
        const Database::SpellData* ziggsBounce2 =
            findSpell("Ziggs", "ZiggsQBounce2");
        collisionCheck(ziggsQ && ziggsBounce1 && ziggsBounce2 &&
            ziggsQ->MissileSpellName == "ZiggsQSpell" &&
            ziggsBounce1->MissileSpellName == "ZiggsQSpell2" &&
            ziggsBounce2->MissileSpellName == "ZiggsQSpell3" &&
            !ziggsQ->DontProcess && ziggsBounce1->DontProcess &&
            ziggsBounce2->DontProcess &&
            std::abs(ziggsQ->Radius - 240.0f) <= 0.01f &&
            std::abs(ziggsBounce1->Radius - 240.0f) <= 0.01f &&
            std::abs(ziggsBounce2->Radius - 240.0f) <= 0.01f &&
            hasProjectileWall(ziggsQ) &&
            hasProjectileWall(ziggsBounce1) &&
            hasProjectileWall(ziggsBounce2));
        collisionCheck(
            SpecialSpells::HasProjectileTerminationDependents("ZiggsQ") &&
            SpecialSpells::HasProjectileTerminationDependents(
                "ZiggsQBounce1") &&
            !SpecialSpells::HasProjectileTerminationDependents(
                "ZiggsQBounce2") &&
            SpecialSpells::IsProjectileTerminationDependent(
                "ZiggsQ", "ZiggsQBounce1") &&
            SpecialSpells::IsProjectileTerminationDependent(
                "ZiggsQ", "ZiggsQBounce2") &&
            SpecialSpells::IsProjectileTerminationDependent(
                "ZiggsQBounce1", "ZiggsQBounce2") &&
            !SpecialSpells::IsProjectileTerminationDependent(
                "ZiggsQBounce1", "ZiggsQBounce1"));
        const auto hasUnitCollision = [](const Database::SpellData* spell,
                                         SDK::CollisionableObjects object) {
            return spell && std::find(
                spell->Runtime.CollisionObjects.begin(),
                spell->Runtime.CollisionObjects.end(), object) !=
                    spell->Runtime.CollisionObjects.end();
        };
        const auto hasAuthoredCollision = [](const Database::SpellData* spell,
                Database::CollisionObjectType object) {
            return spell && std::find(
                spell->CollisionObjects.begin(),
                spell->CollisionObjects.end(), object) !=
                    spell->CollisionObjects.end();
        };
        const Database::SpellData* ashe =
            findSpell("Ashe", "EnchantedCrystalArrow");
        collisionCheck(ashe && ashe->HasEndExplosion &&
            ashe->EndExplosionRequiresUnitCollision &&
            ashe->EndExplosionAtUnitCenter &&
            std::abs(ashe->SecondaryRadius - 400.0f) <= 0.01f &&
            hasUnitCollision(ashe, SDK::CollisionableObjects::Heroes) &&
            hasProjectileWall(ashe));
        const Database::SpellData* apheliosR =
            findSpell("Aphelios", "ApheliosR");
        collisionCheck(apheliosR &&
            apheliosR->MissileSpellName == "ApheliosRMis" &&
            apheliosR->Runtime.MissileSpeed == 2050 &&
            apheliosR->Delay == 500 &&
            apheliosR->HasEndExplosion &&
            apheliosR->EndExplosionRequiresUnitCollision &&
            std::abs(apheliosR->SecondaryRadius - 300.0f) <= 0.01f &&
            hasUnitCollision(apheliosR,
                SDK::CollisionableObjects::Heroes) &&
            !hasUnitCollision(apheliosR,
                SDK::CollisionableObjects::Minions) &&
            hasProjectileWall(apheliosR));
        const Database::SpellData* fizzR = findSpell("Fizz", "FizzR");
        collisionCheck(fizzR && fizzR->MissileSpellName == "FizzRMissile" &&
            fizzR->Runtime.MissileSpeed == 1300 && fizzR->Delay == 250 &&
            fizzR->HasEndExplosion && fizzR->EndExplosionAtUnitCenter &&
            fizzR->EndExplosionFollowsUnit &&
            fizzR->EndExplosionDelay == 2000 &&
            std::abs(fizzR->SecondaryRadius - 200.0f) <= 0.01f &&
            std::abs(fizzR->EndExplosionRadiusMedium - 325.0f) <= 0.01f &&
            std::abs(fizzR->EndExplosionRadiusFar - 450.0f) <= 0.01f &&
            hasUnitCollision(fizzR, SDK::CollisionableObjects::Heroes) &&
            !hasUnitCollision(fizzR, SDK::CollisionableObjects::Minions) &&
            hasProjectileWall(fizzR));
        if (fizzR) {
            SDK::SpellDatabaseEntry entry = fizzR->Runtime;
            entry.SpellType = SDK::SpellType::SkillshotMissileLine;
            auto native = std::make_shared<SDK::SkillshotMissileLine>(entry);
            native->StartPosition = Vec2(100.0f, 100.0f);
            native->EndPosition = Vec2(1400.0f, 100.0f);
            native->Direction = Vec2(1.0f, 0.0f);
            SourceSkillshot test(native, *fizzR,
                SourceDetectionType::Simulated, -3);
            test.CollisionEnd = Vec2(400.0f, 100.0f);
            const float nearRadius = test.EndExplosionBaseRadius();
            test.CollisionEnd = Vec2(700.0f, 100.0f);
            const float mediumRadius = test.EndExplosionBaseRadius();
            test.CollisionEnd = Vec2(1100.0f, 100.0f);
            const float farRadius = test.EndExplosionBaseRadius();
            collisionCheck(std::abs(nearRadius - 200.0f) <= 0.01f &&
                std::abs(mediumRadius - 325.0f) <= 0.01f &&
                std::abs(farRadius - 450.0f) <= 0.01f);
        } else {
            collisionCheck(false);
        }
        const Database::SpellData* jhinW = findSpell("Jhin", "JhinW");
        collisionCheck(jhinW &&
            hasUnitCollision(jhinW, SDK::CollisionableObjects::Heroes) &&
            !hasUnitCollision(jhinW, SDK::CollisionableObjects::Minions) &&
            hasProjectileWall(jhinW));
        const Database::SpellData* jinxR = findSpell("Jinx", "JinxR");
        collisionCheck(jinxR && jinxR->HasEndExplosion &&
            jinxR->EndExplosionRequiresUnitCollision &&
            jinxR->EndExplosionAtUnitCenter &&
            std::abs(jinxR->SecondaryRadius - 400.0f) <= 0.01f &&
            hasUnitCollision(jinxR, SDK::CollisionableObjects::Heroes) &&
            !hasUnitCollision(jinxR, SDK::CollisionableObjects::Minions) &&
            hasProjectileWall(jinxR));
        const Database::SpellData* poppyR =
            findSpell("Poppy", "PoppyRSpell");
        collisionCheck(poppyR && poppyR->HasEndExplosion &&
            poppyR->EndExplosionRequiresUnitCollision &&
            poppyR->EndExplosionAtUnitCenter &&
            std::abs(poppyR->SecondaryRadius - 225.0f) <= 0.01f &&
            poppyR->Runtime.MissileSpeed == 2500 &&
            hasUnitCollision(poppyR, SDK::CollisionableObjects::Heroes) &&
            hasProjectileWall(poppyR));
        const Database::SpellData* kledQ = findSpell("Kled", "KledQ");
        collisionCheck(kledQ &&
            hasAuthoredCollision(kledQ,
                Database::CollisionObjectType::EnemyLargeMonsters) &&
            !hasAuthoredCollision(kledQ,
                Database::CollisionObjectType::EnemyMinions) &&
            hasUnitCollision(kledQ, SDK::CollisionableObjects::Heroes) &&
            hasUnitCollision(kledQ, SDK::CollisionableObjects::Minions));
        const Database::SpellData* nautilusQ =
            findSpell("Nautilus", "NautilusAnchorDrag");
        collisionCheck(nautilusQ &&
            hasAuthoredCollision(nautilusQ,
                Database::CollisionObjectType::EnemyMinions) &&
            hasAuthoredCollision(nautilusQ,
                Database::CollisionObjectType::Terrain) &&
            hasUnitCollision(nautilusQ,
                SDK::CollisionableObjects::Minions) &&
            hasUnitCollision(nautilusQ,
                SDK::CollisionableObjects::Walls));
        const Database::SpellData* kayleQ = findSpell("Kayle", "KayleQ");
        collisionCheck(kayleQ &&
            kayleQ->MissileSpellName == "KayleQMis" &&
            kayleQ->Delay == 264 && kayleQ->EndExplosionCross &&
            kayleQ->EndExplosionRequiresUnitCollision &&
            std::abs(kayleQ->SecondaryRadius - 100.0f) <= 0.01f &&
            std::abs(kayleQ->EndExplosionCenterOffset - 100.0f) <= 0.01f &&
            std::abs(kayleQ->EndExplosionForwardLength - 400.0f) <= 0.01f &&
            std::abs(kayleQ->EndExplosionBackwardLength - 100.0f) <= 0.01f &&
            std::abs(kayleQ->EndExplosionSideLength - 150.0f) <= 0.01f &&
            std::abs(kayleQ->EndExplosionLongitudinalRadius - 45.0f) <= 0.01f &&
            std::abs(kayleQ->EndExplosionSideRadius - 62.5f) <= 0.01f &&
            hasProjectileWall(kayleQ));
        if (kayleQ) {
            SDK::SpellDatabaseEntry entry = kayleQ->Runtime;
            entry.SpellType = SDK::SpellType::SkillshotMissileLine;
            auto native = std::make_shared<SDK::SkillshotMissileLine>(entry);
            native->StartPosition = Vec2(100.0f, 100.0f);
            native->EndPosition = Vec2(1000.0f, 100.0f);
            native->Direction = Vec2(1.0f, 0.0f);
            native->StartTime = SDK::Variables::TickCount();
            SourceSkillshot test(native, *kayleQ,
                SourceDetectionType::Simulated, -1);
            test.OriginalEnd = native->EndPosition;
            test.CollisionEnd = Vec2(500.0f, 100.0f);
            test.CollisionUnitCenter = test.CollisionEnd;
            test.CollisionStopped = true;
            test.CollisionKind = SourceCollisionKind::Unit;
            const EvadeSettings neutralSettings{};
            collisionCheck(test.EndExplosionCenter().DistanceSqr(
                               Vec2(600.0f, 100.0f)) <= 0.01f &&
                test.EndExplosionContains(
                    Vec2(990.0f, 100.0f), 0.0f, neutralSettings) &&
                test.EndExplosionContains(
                    Vec2(600.0f, 295.0f), 0.0f, neutralSettings) &&
                !test.EndExplosionContains(
                    Vec2(600.0f, 400.0f), 0.0f, neutralSettings) &&
                test.EndExplosionPolygons().size() == 5);
        } else {
            collisionCheck(false);
        }
        const Database::SpellData* lilliaLob = findSpell("Lillia", "LilliaE");
        const Database::SpellData* lilliaRoll =
            findSpell("Lillia", "LilliaERollingMissile");
        collisionCheck(lilliaLob && lilliaRoll &&
            lilliaLob->MissileSpellName == "LilliaE" &&
            lilliaLob->Type == Database::SkillShotType::SkillshotCircle &&
            lilliaLob->Runtime.MissileSpeed == 1400 &&
            std::abs(lilliaLob->Radius - 150.0f) <= 0.01f &&
            hasProjectileWall(lilliaLob) &&
            lilliaRoll->MissileSpellName == "LilliaERollingMissile" &&
            lilliaRoll->Runtime.MissileSpeed == 1150 &&
            lilliaRoll->EndExplosionRequiresCollision &&
            lilliaRoll->EndExplosionOnProjectileWall &&
            std::abs(lilliaRoll->SecondaryRadius - 150.0f) <= 0.01f &&
            hasAuthoredCollision(lilliaRoll,
                Database::CollisionObjectType::Terrain) &&
            hasProjectileWall(lilliaRoll));
        if (lilliaRoll) {
            SDK::SpellDatabaseEntry entry = lilliaRoll->Runtime;
            entry.SpellType = SDK::SpellType::SkillshotMissileLine;
            auto native = std::make_shared<SDK::SkillshotMissileLine>(entry);
            native->StartPosition = Vec2(100.0f, 100.0f);
            native->EndPosition = Vec2(1000.0f, 100.0f);
            native->Direction = Vec2(1.0f, 0.0f);
            SourceSkillshot test(native, *lilliaRoll,
                SourceDetectionType::Simulated, -2);
            test.CollisionEnd = Vec2(500.0f, 100.0f);
            test.CollisionStopped = true;
            test.CollisionKind = SourceCollisionKind::Terrain;
            const bool terrainExplosion = test.HasEndExplosionArea();
            test.CollisionKind = SourceCollisionKind::ProjectileWall;
            const bool projectileWallExplosion = test.HasEndExplosionArea();
            test.CollisionStopped = false;
            test.CollisionKind = SourceCollisionKind::None;
            collisionCheck(terrainExplosion && projectileWallExplosion &&
                !test.HasEndExplosionArea());
        } else {
            collisionCheck(false);
        }
        if (lilliaLob) {
            SDK::SpellDatabaseEntry entry = lilliaLob->Runtime;
            entry.SpellType = SDK::SpellType::SkillshotMissileCircle;
            auto native = std::make_shared<SDK::SkillshotMissileCircle>(entry);
            native->StartPosition = Vec2(100.0f, 100.0f);
            native->EndPosition = Vec2(700.0f, 100.0f);
            native->Direction = Vec2(1.0f, 0.0f);
            SourceSkillshot test(native, *lilliaLob,
                SourceDetectionType::Simulated, -4);
            test.CollisionEnd = Vec2(400.0f, 100.0f);
            test.CollisionStopped = true;
            test.CollisionKind = SourceCollisionKind::ProjectileWall;
            const EvadeSettings neutralSettings{};
            collisionCheck(test.ProjectileWallSuppressesEndpointHazard() &&
                !test.ContainsStatic(test.CollisionEnd, 0.0f,
                    neutralSettings) &&
                test.HitTime(test.CollisionEnd, neutralSettings) == FLT_MAX &&
                test.EvadeBoundaries(0.0f, 0.0f,
                    neutralSettings).empty());
        } else {
            collisionCheck(false);
        }
        const Database::SpellData* khazix = findSpell("Khazix", "KhazixW");
        collisionCheck(khazix && khazix->HasEndExplosion &&
            khazix->EndExplosionRequiresUnitCollision &&
            std::abs(khazix->SecondaryRadius - 275.0f) <= 0.01f);
        const Database::SpellData* sejuani = findSpell("Sejuani", "SejuaniR");
        collisionCheck(sejuani && sejuani->HasEndExplosion &&
            sejuani->EndExplosionAtUnitCenter &&
            !sejuani->EndExplosionFollowsUnit &&
            std::abs(sejuani->EndExplosionMinimumTravelDistance -
                400.0f) <= 0.01f &&
            sejuani->EndExplosionDuration == 1500 &&
            std::abs(sejuani->SecondaryRadius - 400.0f) <= 0.01f);
        const Database::SpellData* velkozSplit =
            findSpell("VelKoz", "VelkozQSplit");
        collisionCheck(velkozSplit &&
            velkozSplit->MissileSpellName == "VelkozQMissileSplit" &&
            velkozSplit->Runtime.MissileSpeed == 2100 &&
            velkozSplit->Runtime.Range == 1100);
        result.MinimumMicroseconds = DBL_MAX;
        Vec2 desired = !m_endPoint.IsZero()
            ? m_endPoint
            : SDK::Game::CursorPos().To2D();
        const Vec2 hero = player.Position().To2D();
        if (desired.IsZero() || !desired.IsValid()) {
            desired = hero;
        }
        result.LastBaselineThreats = SourceEvader::CountPathThreats(
            { hero, desired }, settings.CrossingTimeOffset,
            std::max(50.0f, player.MoveSpeed()), 0,
            player.BoundingRadius(), skillshots, settings);

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
            result.LastRemainingThreats = plan.HasCandidate
                ? plan.Best.PathThreatCount
                : result.LastBaselineThreats;
            result.CoverageImprovingPlans +=
                result.LastRemainingThreats < result.LastBaselineThreats
                    ? 1
                    : 0;
            result.LastCandidateCount =
                static_cast<int>(plan.Candidates.size());
            result.LastGeneratedCandidateCount =
                plan.GeneratedCandidateCount;
            result.LastGradientSteps = plan.GradientSteps;
            result.TotalGradientSteps += plan.GradientSteps;
            result.LastWallClearance = plan.Found
                ? plan.Best.WallClearance
                : 0.0f;
            result.LastOuterRingExits = plan.Found
                ? plan.Best.OuterRingExits
                : 0;
            result.LastInnerRingShelters = plan.Found
                ? plan.Best.InnerRingShelters
                : 0;
        }
        result.AverageMicroseconds =
            result.TotalMicroseconds / static_cast<double>(result.Iterations);
        result.AverageGradientSteps =
            static_cast<double>(result.TotalGradientSteps) /
            static_cast<double>(result.Iterations);
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
        const SDK::AIHeroClient player = GameObjects::Player();
        if (!player.IsValid()) {
            return false;
        }

        Vec2 start = m_startPoint;
        Vec2 end = m_endPoint;
        if (start.IsZero()) {
            start = SDK::Game::CursorPos().To2D();
        }
        if (end.IsZero()) {
            end = player.Position().To2D();
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
