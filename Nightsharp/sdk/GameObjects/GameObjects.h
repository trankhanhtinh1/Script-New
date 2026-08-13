#pragma once

// ============================================================================
// SDK::GameObjects — snapshot-based object store.
// ----------------------------------------------------------------------------

#include "ObjectManager.h"
#include "StructureScan.h"
#include "FrameSnapshot.h"
#include "../Events/Events.h"
#include "../Enumerations/ChampionId.h"
#include "../Utils/HashUtils.h"
#include "../../CrashTrace.h"
#include "../../DebugLog.h"
#include "../../SectionProfiler.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace SDK::GameObjects {

namespace detail {

    inline std::recursive_mutex g_mutex;
    using Lock = std::lock_guard<std::recursive_mutex>;

    // ---------------------------- storage ----------------------------------
    inline std::vector<GameObject> GameObjectsList;
    inline std::vector<AttackableUnit> AttackableUnitsList;
    inline std::vector<AIBaseClient> AllyList;
    inline std::vector<AIBaseClient> EnemyList;

    inline std::vector<AIHeroClient> HeroesList;
    inline std::vector<AIHeroClient> AllyHeroesList;
    inline std::vector<AIHeroClient> EnemyHeroesList;

    inline std::vector<AIMinionClient> MinionsList;
    inline std::vector<AIMinionClient> AllyMinionsList;
    inline std::vector<AIMinionClient> EnemyMinionsList;
    inline std::vector<AIMinionClient> AllyLaneMinionsList;
    inline std::vector<AIMinionClient> EnemyLaneMinionsList;
    inline std::vector<AIMinionClient> AllySpecialMinionsList;
    inline std::vector<AIMinionClient> EnemySpecialMinionsList;
    inline std::vector<AIMinionClient> AllyIgnoredMinionsList;
    inline std::vector<AIMinionClient> EnemyIgnoredMinionsList;
    inline std::vector<AIMinionClient> WardsList;
    inline std::vector<AIMinionClient> AllyWardsList;
    inline std::vector<AIMinionClient> EnemyWardsList;
    inline std::vector<AIMinionClient> JungleList;
    inline std::vector<AIMinionClient> JungleSmallList;
    inline std::vector<AIMinionClient> JungleLargeList;
    inline std::vector<AIMinionClient> JungleLegendaryList;
    inline std::vector<AIMinionClient> PlantsList;
    inline std::vector<AIMinionClient> ClonesList;
    inline std::vector<AIMinionClient> AllyClonesList;
    inline std::vector<AIMinionClient> EnemyClonesList;
    inline std::vector<AIMinionClient> PetsList;
    inline std::vector<AIMinionClient> AllyPetsList;
    inline std::vector<AIMinionClient> EnemyPetsList;

    // REMOVED: Turret/Inhibitor/Nexus class disabled by user request
    // inline std::vector<AITurretClient> TurretsList;
    // inline std::vector<AITurretClient> AllyTurretsList;
    // inline std::vector<AITurretClient> EnemyTurretsList;
    //
    // inline std::vector<BarracksDampenerClient> InhibitorsList;
    // inline std::vector<BarracksDampenerClient> AllyInhibitorsList;
    // inline std::vector<BarracksDampenerClient> EnemyInhibitorsList;
    // inline std::vector<HQClient> NexusList;
    // inline HQClient AllyNexusObject;
    // inline HQClient EnemyNexusObject;

    // Not classified yet: no vtable RVAs dumped for ShopClient / Obj_SpawnPoint /
    // EffectEmitter. Kept (empty) so the public API surface stays identical.
    // To enable: dump the vtable RVA live (see Offset::StructureVTable notes)
    // and add a branch in RefreshStructures.
    // inline std::vector<ShopClient> ShopsList;
    // inline std::vector<ShopClient> AllyShopsList;
    // inline std::vector<ShopClient> EnemyShopsList;
    inline std::vector<Obj_SpawnPoint> SpawnPointsList;
    inline std::vector<Obj_SpawnPoint> AllySpawnPointsList;
    inline std::vector<Obj_SpawnPoint> EnemySpawnPointsList;
    inline std::vector<EffectEmitter> ParticleEmittersList;
    inline std::vector<MissileClient> MissilesList;

    inline AIHeroClient PlayerObject;
    inline ChampionId PlayerChampionIdObject = ChampionId::Unknown;
    inline bool PlayerChampionIdCached = false;
    inline bool Initialized = false;
    inline bool SeedRetryPending = true;
    inline bool SeedInProgress = false;
    // Invalid objects discovered by cache pruning are queued while g_mutex is
    // held and dispatched on the next update after the lock is released.
    inline std::vector<::Core::Objects::ObjectHandle> PendingPrunedDeletes;


    // ------------------------- string helpers ------------------------------
    inline std::string ToLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    // BestName: use handle-cached CharacterName/Name rather than a raw memory
    // read on every call — avoids the ReadName syscall per-object per-frame.
    inline std::string BestNameOf(const AIMinionClient& minion) {
        std::string name = minion.CharacterName();
        if (name.empty()) name = minion.Name();
        return ToLower(std::move(name));
    }

    // Legacy overload kept for the few internal callers that only have an address.
    inline std::string BestName(uintptr_t address) {
        AIMinionClient tmp(address);
        return BestNameOf(tmp);
    }

    inline bool ContainsAny(const std::string& value, std::initializer_list<const char*> needles) {
        if (value.empty()) return false;
        for (const auto* needle : needles) {
            if (!needle || !needle[0]) continue;
            auto it = std::search(
                value.begin(), value.end(),
                needle, needle + std::strlen(needle),
                [](char c1, char c2) {
                    return std::tolower(static_cast<unsigned char>(c1)) ==
                           std::tolower(static_cast<unsigned char>(c2));
                }
            );
            if (it != value.end()) return true;
        }
        return false;
    }

    inline bool EqualsAny(const std::string& value, std::initializer_list<const char*> needles) {
        if (value.empty()) return false;
        for (const auto* needle : needles) {
            if (needle && _stricmp(value.c_str(), needle) == 0) {
                return true;
            }
        }
        return false;
    }

    // --------------------------- misc helpers ------------------------------
    inline void PopulateStatic(
        uintptr_t address,
        std::uint32_t index,
        ::Core::Objects::ObjectType type) {
        if (!Globals::IsValidPtr(address)) {
            return;
        }
        SDK::StaticStringCache::Populate(address, index & 0xFFFFu, type);
    }

    inline void PopulateStatic(uintptr_t address, ::Core::Objects::ObjectType type) {
        if (!Globals::IsValidPtr(address)) {
            return;
        }
        PopulateStatic(address, ::Core::Objects::ReadIndex(address), type);
    }

    template <typename TObject>
    inline void PopulateStatic(const TObject& object) {
        PopulateStatic(object.Address(), object.Type());
    }

    inline int TeamValue(const GameObject& object) {
        return static_cast<int>(object.Team());
    }

    inline int PlayerTeam() {
        static int s_team = 0;
        static bool s_resolved = false;
        if (!s_resolved) {
            const auto player = SDK::ObjectManager::Player();
            if (player.IsValid()) {
                s_team = TeamValue(player);
                if (s_team != 0) {
                    s_resolved = true;
                }
            }
        }
        return s_team;
    }

    // ------------------------ minion classification ------------------------
    // Proven predicates carried over from the previous implementation (minion
    // detection has always worked); mirrors EnsoulSharp's MinionTypes /
    // JungleType flag checks.
    inline bool IsKnownJunglePlantName(const std::string& name) {
        return ContainsAny(name, {
            "sru_plant",
            "plant_satchel", "plant_health", "plant_vision",
            "plantsatchel", "planthealth", "plantvision",
            "blastcone", "blast_cone",
            "honeyfruit", "honey_fruit",
            "scryer", "scryersbloom", "scryers_bloom",
            "hiddenminionplantdemon", "planthealthmirrored",
            "plantmasterminion", "minimapicon"
        });
    }

    inline bool IsKnownJungleMonsterName(const std::string& name) {
        if (IsKnownJunglePlantName(name)) {
            return false;
        }
        return ContainsAny(name, {
            "sru_baron", "sru_dragon", "sru_riftherald", "voidgrub", "sru_horde",
            "sru_atakhan", "atakhan", "sru_sentinel", "sru_blue",
            "sru_red", "sru_gromp", "sru_krug", "sru_murkwolf",
            "sru_razorbeak", "sru_crab", "sru_riftscuttler"
        });
    }

    inline bool IsLaneMinionName(const std::string& name) {
        return ContainsAny(name, {
            "sru_chaosminion", "sru_orderminion",
            "ha_chaosminion", "ha_orderminion"
        });
    }

    inline bool IsWardObject(const AIMinionClient& minion, const std::string& name) {
        if (!minion.IsValid() || minion.IsDead()) {
            return false;
        }
        return HasFlag(minion.GetMinionType(), MinionTypes::Ward) ||
               ContainsAny(name, {
                   "ward", "jammerdevice", "trinket", "sightward", "visionward"
               });
    }

    inline bool IsWardObject(const AIMinionClient& minion) {
        return IsWardObject(minion, BestNameOf(minion));
    }

    inline bool IsPlantObject(const AIMinionClient& minion, const std::string& name) {
        if (!minion.IsValid() || minion.IsDead()) {
            return false;
        }
        const float maxHp = minion.MaxHealth();
        return minion.GetJungleType() == JungleType::Plant ||
               IsKnownJunglePlantName(name) ||
               (TeamValue(minion) == 300 && maxHp > 0.0f && maxHp <= 6.0f);
    }

    inline bool IsPlantObject(const AIMinionClient& minion) {
        return IsPlantObject(minion, BestNameOf(minion));
    }

    inline bool IsJungleObject(const AIMinionClient& minion, const std::string& name) {
        if (!minion.IsValid() || minion.IsDead()) {
            return false;
        }
        if (TeamValue(minion) != 300 || IsPlantObject(minion, name)) {
            return false;
        }
        const float maxHp = minion.MaxHealth();
        if (maxHp <= 6.0f) {
            return false;
        }
        return minion.IsJungle() || IsKnownJungleMonsterName(name);
    }

    inline bool IsJungleObject(const AIMinionClient& minion) {
        return IsJungleObject(minion, BestNameOf(minion));
    }

    inline bool IsLaneMinionObject(const AIMinionClient& minion, const std::string& name) {
        if (!minion.IsValid() || minion.IsDead()) {
            return false;
        }
        if (TeamValue(minion) == 300 || IsPlantObject(minion, name)) {
            return false;
        }
        const float maxHp = minion.MaxHealth();
        if (maxHp <= 0.0f || maxHp >= 10000.0f) {
            return false;
        }
        if (IsKnownJungleMonsterName(name)) {
            return false;
        }
        const MinionTypes type = minion.GetMinionType();
        return HasFlag(type, MinionTypes::Melee) ||
               HasFlag(type, MinionTypes::Ranged) ||
               HasFlag(type, MinionTypes::Siege) ||
               HasFlag(type, MinionTypes::Super) ||
               IsLaneMinionName(name);
    }

    inline bool IsLaneMinionObject(const AIMinionClient& minion) {
        return IsLaneMinionObject(minion, BestNameOf(minion));
    }

    inline bool IsCloneObject(const AIMinionClient& minion, const std::string& name) {
        if (!minion.IsValid() || minion.IsDead()) {
            return false;
        }
        return minion.IsClone() || EqualsAny(name, {
            "leblanc", "monkeyking", "neeko", "shaco"
        });
    }

    inline bool IsCloneObject(const AIMinionClient& minion) {
        return IsCloneObject(minion, BestNameOf(minion));
    }

    inline bool IsPetObject(const AIMinionClient& minion, const std::string& name) {
        return minion.IsValid() && !minion.IsDead() &&
               !IsPlantObject(minion, name) && minion.IsPet();
    }

    inline bool IsPetObject(const AIMinionClient& minion) {
        return IsPetObject(minion, BestNameOf(minion));
    }

    inline bool IsSpecialMinionObject(const AIMinionClient& minion, const std::string& name) {
        if (!minion.IsValid() || minion.IsDead()) {
            return false;
        }
        return EqualsAny(name, {
            "annietibbers", "elisespiderling", "heimertyellow",
            "heimertblue", "ivernminion", "malzaharvoidling",
            "shacobox", "teemomushroom", "yorickghoulmelee",
            "yorickbigghoul", "zyrathornplant", "zyragraspingplant",
            "azirsolider", "azirsoldier"
        });
    }

    inline bool IsSpecialMinionObject(const AIMinionClient& minion) {
        return IsSpecialMinionObject(minion, BestNameOf(minion));
    }

    inline bool IsIgnoredMinionObject(const AIMinionClient& minion, const std::string& name) {
        return minion.IsValid() && EqualsAny(name, {
            "jarvanivstandard"
        });
    }

    inline bool IsIgnoredMinionObject(const AIMinionClient& minion) {
        return IsIgnoredMinionObject(minion, BestNameOf(minion));
    }



    template <typename T>
    inline bool ContainsByNetworkId(const std::vector<T>& vec, int netId) {
        if (netId == 0 || vec.empty()) return false;
        for (const auto& item : vec) {
            if (static_cast<int>(item.CachedNetworkId()) == netId) return true;
        }
        return false;
    }

    inline bool IsGenericObjectType(::Core::Objects::ObjectType type) {
        return type == ::Core::Objects::ObjectType::Unknown ||
               type == ::Core::Objects::ObjectType::GameObject;
    }

    template <typename T>
    inline void PushUniqueByNetworkId(std::vector<T>& vec, const T& obj) {
        const int netId = static_cast<int>(obj.CachedNetworkId());
        if (netId != 0) {
            for (auto& existing : vec) {
                if (static_cast<int>(existing.CachedNetworkId()) != netId) {
                    continue;
                }

                const auto existingType = existing.Handle().type;
                const auto incomingType = obj.Handle().type;
                if (!IsGenericObjectType(existingType) ||
                    IsGenericObjectType(incomingType)) {
                    // Refresh the address/identity for an existing typed entry,
                    // but never downgrade it because of a later generic hint.
                    if (!IsGenericObjectType(incomingType)) {
                        existing = obj;
                    }
                } else {
                    existing = obj;
                }
                return;
            }
        }
        vec.push_back(obj);
    }

    template <typename T>
    inline void EraseByNetworkId(std::vector<T>& vec, int netId) {
        if (vec.empty()) return;
        vec.erase(std::remove_if(vec.begin(), vec.end(), [netId](const T& obj) {
            return static_cast<int>(obj.CachedNetworkId()) == netId;
        }), vec.end());
    }

    inline bool HasLiveIdentity(const GameObject& object) {
        const auto handle = object.Handle();
        if (!handle.HasAddress() || !handle.HasIdentity()) {
            return false;
        }

        // Identity-aware without walking the game's mutable NetworkId tree.
        // Globals::Read is SEH-guarded; a freed address or a pool-reused slot
        // fails closed instead of dereferencing native tree nodes repeatedly.
        if (::Core::Objects::ReadNetworkId(handle.address) != handle.networkId) {
            return false;
        }
        return handle.index == 0 || handle.index == 0xFFFFFFFFu ||
               ::Core::Objects::ReadIndex(handle.address) == handle.index;
    }

    inline bool LostRuntimeName(const GameObject& object) {
        const auto handle = object.Handle();
        const uint32_t index = handle.index & 0xFFFFu;
        if (!handle.HasAddress() ||
            index >= static_cast<uint32_t>(StaticStringCache::kMaxIndex)) {
            return false;
        }

        std::string cachedName;
        std::string cachedCharacterName;
        const bool previouslyNamed =
            StaticStringCache::CopyString(
                index, false, cachedName, false) ||
            StaticStringCache::CopyString(
                index, true, cachedCharacterName, false);
        if (!previouslyNamed) {
            return false;
        }

        char runtimeName[96] = {};
        char runtimeCharacterName[96] = {};
        const bool hasRuntimeName =
            ::Core::Objects::ReadName(
                handle.address,
                runtimeName,
                static_cast<int>(sizeof(runtimeName))) &&
            runtimeName[0];
        const bool hasRuntimeCharacterName =
            ::Core::Objects::ReadCharacterName(
                handle.address,
                runtimeCharacterName,
                static_cast<int>(sizeof(runtimeCharacterName))) &&
            runtimeCharacterName[0];
        return !hasRuntimeName && !hasRuntimeCharacterName;
    }

    inline void QueuePrunedDelete(const GameObject& object) {
        auto handle = object.Handle();
        if (!handle.HasIdentity()) {
            return;
        }
        const bool queued = std::any_of(
            PendingPrunedDeletes.begin(), PendingPrunedDeletes.end(),
            [&handle](const ::Core::Objects::ObjectHandle& existing) {
                return (handle.networkId != 0 &&
                        existing.networkId == handle.networkId) ||
                       (handle.index != 0 && handle.index != 0xFFFFFFFFu &&
                        existing.index == handle.index);
            });
        if (queued) {
            return;
        }
        // Never expose a freed address to lifecycle subscribers.
        handle.address = 0;
        PendingPrunedDeletes.push_back(handle);
    }

    template <typename T>
    inline void CleanInvalid(std::vector<T>& vec, bool queueDelete = false) {
        if (vec.empty()) return;
        vec.erase(std::remove_if(vec.begin(), vec.end(), [queueDelete](const T& obj) {
            const GameObject base(obj.Handle());
            const bool liveIdentity = HasLiveIdentity(base);
            const bool lostRuntimeName = liveIdentity && LostRuntimeName(base);
            if (liveIdentity && !lostRuntimeName) {
                return false;
            }
            if (queueDelete) {
                QueuePrunedDelete(base);
            }
            const auto handle = base.Handle();
            ::Core::ObjectManager::TypeCache::Invalidate(handle.address);
            return true;
        }), vec.end());
    }

    inline void CleanInvalidObjects() {
        // Every classified object is also present in GameObjectsList. Queue the
        // synthetic delete only from this canonical list to avoid duplicates
        // when the same identity is removed from its typed/team lists.
        CleanInvalid(GameObjectsList, true);
        CleanInvalid(AttackableUnitsList);
        CleanInvalid(AllyList);
        CleanInvalid(EnemyList);

        CleanInvalid(HeroesList);
        CleanInvalid(AllyHeroesList);
        CleanInvalid(EnemyHeroesList);

        CleanInvalid(MinionsList);
        CleanInvalid(AllyMinionsList);
        CleanInvalid(EnemyMinionsList);
        CleanInvalid(AllyLaneMinionsList);
        CleanInvalid(EnemyLaneMinionsList);
        CleanInvalid(AllySpecialMinionsList);
        CleanInvalid(EnemySpecialMinionsList);
        CleanInvalid(AllyIgnoredMinionsList);
        CleanInvalid(EnemyIgnoredMinionsList);
        CleanInvalid(WardsList);
        CleanInvalid(AllyWardsList);
        CleanInvalid(EnemyWardsList);
        CleanInvalid(JungleList);
        CleanInvalid(JungleSmallList);
        CleanInvalid(JungleLargeList);
        CleanInvalid(JungleLegendaryList);
        CleanInvalid(PlantsList);
        CleanInvalid(ClonesList);
        CleanInvalid(AllyClonesList);
        CleanInvalid(EnemyClonesList);
        CleanInvalid(PetsList);
        CleanInvalid(AllyPetsList);
        CleanInvalid(EnemyPetsList);

        // REMOVED: Turret/Inhibitor/Nexus class disabled by user request
        // CleanInvalid(TurretsList);
        // CleanInvalid(AllyTurretsList);
        // CleanInvalid(EnemyTurretsList);
        //
        // CleanInvalid(InhibitorsList);
        // CleanInvalid(AllyInhibitorsList);
        // CleanInvalid(EnemyInhibitorsList);
        // CleanInvalid(NexusList);
        // if (!AllyNexusObject.IsValid()) AllyNexusObject = {};
        // if (!EnemyNexusObject.IsValid()) EnemyNexusObject = {};

        // CleanInvalid(ShopsList);
        // CleanInvalid(AllyShopsList);
        // CleanInvalid(EnemyShopsList);
        CleanInvalid(SpawnPointsList);
        CleanInvalid(AllySpawnPointsList);
        CleanInvalid(EnemySpawnPointsList);
        CleanInvalid(ParticleEmittersList);
        CleanInvalid(MissilesList);

        // Keep original names available until every typed/team list has had a
        // chance to apply LostRuntimeName(). Clear only after the full prune.
        for (const auto& handle : PendingPrunedDeletes) {
            StaticStringCache::Clear(handle.index & 0xFFFFu);
        }
    }

    inline void OnObjectAdd(const GameObject& object,
                            bool runCleanup = true,
                            bool populateStaticCache = true,
                            bool inferGeneric = true,
                            bool queryDynamicState = true) {
        Lock lk(g_mutex);
        static DWORD s_lastCleanTick = 0;
        const DWORD now = GetTickCount();
        if (runCleanup && now - s_lastCleanTick > 100) {
            s_lastCleanTick = now;
            CleanInvalidObjects();
        }

        if (!object.IsValid()) return;

        auto classifiedHandle = object.Handle();
        if (!classifiedHandle.HasIdentity()) {
            return;
        }

        auto classifiedType = classifiedHandle.type;
        if (IsGenericObjectType(classifiedType)) {
            auto upgraded =
                ::Core::ObjectManager::TypeCache::Lookup(classifiedHandle.address);
            if (upgraded == ::Core::Objects::ObjectType::Unknown && inferGeneric) {
                upgraded =
                    ::Core::ObjectManager::InferType(classifiedHandle.address);
            }
            if (!IsGenericObjectType(upgraded)) {
                classifiedType = upgraded;
                classifiedHandle.type = upgraded;
            }
        }
        GameObject classifiedObject(classifiedHandle);
        const int netId = static_cast<int>(classifiedHandle.networkId);

        bool replacedExisting = false;
        for (auto& existing : GameObjectsList) {
            if (static_cast<int>(existing.CachedNetworkId()) != netId) {
                continue;
            }

            const auto existingType = existing.Handle().type;
            if (!IsGenericObjectType(existingType) &&
                IsGenericObjectType(classifiedType)) {
                // Preserve an already-specific classification when a generic
                // lifecycle hint for the same object arrives later.
                classifiedObject = existing;
                classifiedType = existingType;
                classifiedHandle = existing.Handle();
            } else {
                // Replace the base entry with the freshest handle. In
                // particular this upgrades a seed-time GameObject fallback to
                // AIHeroClient/AIMinionClient instead of returning early.
                existing = classifiedObject;
            }
            replacedExisting = true;
            break;
        }
        if (!replacedExisting) {
            GameObjectsList.push_back(classifiedObject);
        }

        ::Core::ObjectManager::TypeCache::Store(
            classifiedHandle.address,
            classifiedType);
        if (populateStaticCache) {
            PopulateStatic(classifiedObject);
        }

        if (::Core::Objects::IsAttackable(classifiedType)) {
            PushUniqueByNetworkId(
                AttackableUnitsList,
                AttackableUnit(classifiedObject.Handle()));
        }

        const int myTeam = PlayerTeam();
        const int team = TeamValue(classifiedObject);
        const bool ally = myTeam != 0 && team == myTeam;
        const bool enemy = team != 0 && team != 300 && !ally;

        const ::Core::Objects::ObjectType type = classifiedType;

        switch (type) {
        case ::Core::Objects::ObjectType::AIHeroClient: {
            const AIHeroClient hero(classifiedObject.Handle());
            PushUniqueByNetworkId(HeroesList, hero);
            if (ally) {
                PushUniqueByNetworkId(AllyHeroesList, hero);
                PushUniqueByNetworkId(AllyList, AIBaseClient(classifiedObject.Handle()));
            } else if (enemy) {
                PushUniqueByNetworkId(EnemyHeroesList, hero);
                PushUniqueByNetworkId(EnemyList, AIBaseClient(classifiedObject.Handle()));
            }
            break;
        }
        case ::Core::Objects::ObjectType::AIMinionClient:
        case ::Core::Objects::ObjectType::NeutralMinionCampClient: {
            const AIMinionClient minion(classifiedObject.Handle());
            // Bulk seed runs inside the native update callback. IsDead() calls
            // the game's IsAlive function, so defer that dynamic query there;
            // create events and regular consumers retain the normal behavior.
            if (queryDynamicState && minion.IsDead()) break;

            const std::string name = BestNameOf(minion);

            if (IsWardObject(minion, name)) {
                PushUniqueByNetworkId(WardsList, minion);
                if (enemy) PushUniqueByNetworkId(EnemyWardsList, minion);
                else PushUniqueByNetworkId(AllyWardsList, minion);
                break;
            }

            if (IsPlantObject(minion, name)) {
                PushUniqueByNetworkId(PlantsList, minion);
                break;
            }

            if (IsJungleObject(minion, name)) {
                PushUniqueByNetworkId(JungleList, minion);
                const JungleType jungleType = minion.GetJungleType();
                if (jungleType == JungleType::Small) {
                    PushUniqueByNetworkId(JungleSmallList, minion);
                } else if (jungleType == JungleType::Legendary || jungleType == JungleType::Epic) {
                    PushUniqueByNetworkId(JungleLegendaryList, minion);
                } else {
                    PushUniqueByNetworkId(JungleLargeList, minion);
                }
                break;
            }

            if (IsCloneObject(minion, name)) {
                PushUniqueByNetworkId(ClonesList, minion);
                if (enemy) PushUniqueByNetworkId(EnemyClonesList, minion);
                else PushUniqueByNetworkId(AllyClonesList, minion);
                break;
            }

            if (IsPetObject(minion, name)) {
                PushUniqueByNetworkId(PetsList, minion);
                if (enemy) PushUniqueByNetworkId(EnemyPetsList, minion);
                else PushUniqueByNetworkId(AllyPetsList, minion);
                break;
            }

            if (IsSpecialMinionObject(minion, name)) {
                if (enemy) PushUniqueByNetworkId(EnemySpecialMinionsList, minion);
                else PushUniqueByNetworkId(AllySpecialMinionsList, minion);
                break;
            }

            if (IsIgnoredMinionObject(minion, name)) {
                if (enemy) PushUniqueByNetworkId(EnemyIgnoredMinionsList, minion);
                else PushUniqueByNetworkId(AllyIgnoredMinionsList, minion);
                break;
            }

            if (IsLaneMinionObject(minion, name)) {
                PushUniqueByNetworkId(MinionsList, minion);
                if (enemy) {
                    PushUniqueByNetworkId(EnemyMinionsList, minion);
                    PushUniqueByNetworkId(EnemyLaneMinionsList, minion);
                    PushUniqueByNetworkId(EnemyList, AIBaseClient(classifiedObject.Handle()));
                } else {
                    PushUniqueByNetworkId(AllyMinionsList, minion);
                    PushUniqueByNetworkId(AllyLaneMinionsList, minion);
                    PushUniqueByNetworkId(AllyList, AIBaseClient(classifiedObject.Handle()));
                }
                break;
            }
            break;
        }
        // REMOVED: Turret/Inhibitor/Nexus class disabled by user request
        // case ::Core::Objects::ObjectType::AITurretClient: {
        //     const AITurretClient turret(classifiedObject.Handle());
        //     PushUniqueByNetworkId(TurretsList, turret);
        //     if (ally) PushUniqueByNetworkId(AllyTurretsList, turret);
        //     else if (enemy) PushUniqueByNetworkId(EnemyTurretsList, turret);
        //     break;
        // }
        // case ::Core::Objects::ObjectType::BarracksDampenerClient: {
        //     const BarracksDampenerClient inhib(classifiedObject.Handle());
        //     PushUniqueByNetworkId(InhibitorsList, inhib);
        //     if (ally) PushUniqueByNetworkId(AllyInhibitorsList, inhib);
        //     else if (enemy) PushUniqueByNetworkId(EnemyInhibitorsList, inhib);
        //     break;
        // }
        // case ::Core::Objects::ObjectType::HQClient: {
        //     const HQClient nexus(classifiedObject.Handle());
        //     PushUniqueByNetworkId(NexusList, nexus);
        //     if (ally) AllyNexusObject = nexus;
        //     else if (enemy) EnemyNexusObject = nexus;
        //     break;
        // }
        case ::Core::Objects::ObjectType::MissileClient: {
            const MissileClient missile(classifiedObject.Handle());
            PushUniqueByNetworkId(MissilesList, missile);
            break;
        }
        default:
            break;
        }
    }

    inline void OnObjectDelete(int netId, ::Core::Objects::ObjectType type = ::Core::Objects::ObjectType::Unknown) {
        if (netId == 0) return;
        Lock lk(g_mutex);

        EraseByNetworkId(GameObjectsList, netId);
        EraseByNetworkId(AttackableUnitsList, netId);

        switch (type) {
        case ::Core::Objects::ObjectType::AIHeroClient:
            EraseByNetworkId(AllyList, netId);
            EraseByNetworkId(EnemyList, netId);
            EraseByNetworkId(HeroesList, netId);
            EraseByNetworkId(AllyHeroesList, netId);
            EraseByNetworkId(EnemyHeroesList, netId);
            break;

        case ::Core::Objects::ObjectType::AIMinionClient:
        case ::Core::Objects::ObjectType::NeutralMinionCampClient:
            EraseByNetworkId(AllyList, netId);
            EraseByNetworkId(EnemyList, netId);
            EraseByNetworkId(MinionsList, netId);
            EraseByNetworkId(AllyMinionsList, netId);
            EraseByNetworkId(EnemyMinionsList, netId);
            EraseByNetworkId(AllyLaneMinionsList, netId);
            EraseByNetworkId(EnemyLaneMinionsList, netId);
            EraseByNetworkId(AllySpecialMinionsList, netId);
            EraseByNetworkId(EnemySpecialMinionsList, netId);
            EraseByNetworkId(AllyIgnoredMinionsList, netId);
            EraseByNetworkId(EnemyIgnoredMinionsList, netId);
            EraseByNetworkId(WardsList, netId);
            EraseByNetworkId(AllyWardsList, netId);
            EraseByNetworkId(EnemyWardsList, netId);
            EraseByNetworkId(JungleList, netId);
            EraseByNetworkId(JungleSmallList, netId);
            EraseByNetworkId(JungleLargeList, netId);
            EraseByNetworkId(JungleLegendaryList, netId);
            EraseByNetworkId(PlantsList, netId);
            EraseByNetworkId(ClonesList, netId);
            EraseByNetworkId(AllyClonesList, netId);
            EraseByNetworkId(EnemyClonesList, netId);
            EraseByNetworkId(PetsList, netId);
            EraseByNetworkId(AllyPetsList, netId);
            EraseByNetworkId(EnemyPetsList, netId);
            break;

        // REMOVED: Turret/Inhibitor/Nexus class disabled by user request
        // case ::Core::Objects::ObjectType::AITurretClient:
        //     EraseByNetworkId(TurretsList, netId);
        //     EraseByNetworkId(AllyTurretsList, netId);
        //     EraseByNetworkId(EnemyTurretsList, netId);
        //     break;
        //
        // case ::Core::Objects::ObjectType::BarracksDampenerClient:
        //     EraseByNetworkId(InhibitorsList, netId);
        //     EraseByNetworkId(AllyInhibitorsList, netId);
        //     EraseByNetworkId(EnemyInhibitorsList, netId);
        //     break;
        //
        // case ::Core::Objects::ObjectType::HQClient:
        //     EraseByNetworkId(NexusList, netId);
        //     if (static_cast<int>(AllyNexusObject.Handle().networkId) == netId) AllyNexusObject = {};
        //     if (static_cast<int>(EnemyNexusObject.Handle().networkId) == netId) EnemyNexusObject = {};
        //     break;

        case ::Core::Objects::ObjectType::MissileClient:
            EraseByNetworkId(MissilesList, netId);
            break;

        default:
            EraseByNetworkId(AllyList, netId);
            EraseByNetworkId(EnemyList, netId);
            EraseByNetworkId(HeroesList, netId);
            EraseByNetworkId(AllyHeroesList, netId);
            EraseByNetworkId(EnemyHeroesList, netId);
            EraseByNetworkId(MinionsList, netId);
            EraseByNetworkId(AllyMinionsList, netId);
            EraseByNetworkId(EnemyMinionsList, netId);
            EraseByNetworkId(AllyLaneMinionsList, netId);
            EraseByNetworkId(EnemyLaneMinionsList, netId);
            EraseByNetworkId(AllySpecialMinionsList, netId);
            EraseByNetworkId(EnemySpecialMinionsList, netId);
            EraseByNetworkId(AllyIgnoredMinionsList, netId);
            EraseByNetworkId(EnemyIgnoredMinionsList, netId);
            EraseByNetworkId(WardsList, netId);
            EraseByNetworkId(AllyWardsList, netId);
            EraseByNetworkId(EnemyWardsList, netId);
            EraseByNetworkId(JungleList, netId);
            EraseByNetworkId(JungleSmallList, netId);
            EraseByNetworkId(JungleLargeList, netId);
            EraseByNetworkId(JungleLegendaryList, netId);
            EraseByNetworkId(PlantsList, netId);
            EraseByNetworkId(ClonesList, netId);
            EraseByNetworkId(AllyClonesList, netId);
            EraseByNetworkId(EnemyClonesList, netId);
            EraseByNetworkId(PetsList, netId);
            EraseByNetworkId(AllyPetsList, netId);
            EraseByNetworkId(EnemyPetsList, netId);
            // REMOVED: Turret/Inhibitor/Nexus class disabled by user request
            // EraseByNetworkId(TurretsList, netId);
            // EraseByNetworkId(AllyTurretsList, netId);
            // EraseByNetworkId(EnemyTurretsList, netId);
            // EraseByNetworkId(InhibitorsList, netId);
            // EraseByNetworkId(AllyInhibitorsList, netId);
            // EraseByNetworkId(EnemyInhibitorsList, netId);
            // EraseByNetworkId(NexusList, netId);
            EraseByNetworkId(MissilesList, netId);
            break;
        }
    }

    inline void Clear() {
        Lock lk(g_mutex);
        GameObjectsList.clear();
        AttackableUnitsList.clear();
        AllyList.clear();
        EnemyList.clear();
        HeroesList.clear();
        AllyHeroesList.clear();
        EnemyHeroesList.clear();
        MinionsList.clear();
        AllyMinionsList.clear();
        EnemyMinionsList.clear();
        AllyLaneMinionsList.clear();
        EnemyLaneMinionsList.clear();
        AllySpecialMinionsList.clear();
        EnemySpecialMinionsList.clear();
        AllyIgnoredMinionsList.clear();
        EnemyIgnoredMinionsList.clear();
        WardsList.clear();
        AllyWardsList.clear();
        EnemyWardsList.clear();
        JungleList.clear();
        JungleSmallList.clear();
        JungleLargeList.clear();
        JungleLegendaryList.clear();
        PlantsList.clear();
        ClonesList.clear();
        AllyClonesList.clear();
        EnemyClonesList.clear();
        PetsList.clear();
        AllyPetsList.clear();
        EnemyPetsList.clear();
        // REMOVED: Turret/Inhibitor/Nexus class disabled by user request
        // TurretsList.clear();
        // AllyTurretsList.clear();
        // EnemyTurretsList.clear();
        // InhibitorsList.clear();
        // AllyInhibitorsList.clear();
        // EnemyInhibitorsList.clear();
        // NexusList.clear();
        // AllyNexusObject = {};
        // EnemyNexusObject = {};
        // ShopsList.clear();
        // AllyShopsList.clear();
        // EnemyShopsList.clear();
        SpawnPointsList.clear();
        AllySpawnPointsList.clear();
        EnemySpawnPointsList.clear();
        ParticleEmittersList.clear();
        MissilesList.clear();
        PendingPrunedDeletes.clear();
        PlayerObject = {};
        PlayerChampionIdObject = ChampionId::Unknown;
        PlayerChampionIdCached = false;
    }

    inline bool SeedManagersReady() {
        const auto& ctx = CoreRuntime::GetContext();
        return Globals::IsValidPtr(ctx.objectManager) &&
               Globals::IsValidPtr(ctx.localPlayer) &&
               Globals::IsValidPtr(ctx.heroManager) &&
               Globals::IsValidPtr(ctx.minionManager);
    }

    inline bool TryBeginSeed() {
        Lock lk(g_mutex);
        if (!SeedRetryPending || SeedInProgress) {
            return false;
        }
        SeedRetryPending = false;
        SeedInProgress = true;
        return true;
    }

    inline void FinishSeed(bool complete) {
        Lock lk(g_mutex);
        SeedInProgress = false;
        SeedRetryPending = !complete;
    }

    inline bool SeedAllGameObjectsImpl() {
        if (!SeedManagersReady()) {
            const auto& ctx = CoreRuntime::GetContext();
            static DWORD s_lastDeferredLogTick = 0;
            const DWORD now = GetTickCount();
            if (now - s_lastDeferredLogTick >= 1000) {
                s_lastDeferredLogTick = now;
                NightSharpDebug::Logf(
                    "[GameObjects] Seed deferred managers object=%d player=%d heroes=%d minions=%d",
                    Globals::IsValidPtr(ctx.objectManager) ? 1 : 0,
                    Globals::IsValidPtr(ctx.localPlayer) ? 1 : 0,
                    Globals::IsValidPtr(ctx.heroManager) ? 1 : 0,
                    Globals::IsValidPtr(ctx.minionManager) ? 1 : 0);
            }
            return false;
        }

        const AIHeroClient player = SDK::ObjectManager::Player();
        const auto playerHandle = player.Handle();

        // Keep manager scratch buffers off the native update-hook stack.
        std::vector<uintptr_t> heroEntries(128);
        const int rawHeroManagerCount = ::Core::ObjectManager::EnumerateHeroes(
            heroEntries.data(),
            static_cast<int>(heroEntries.size()));
        std::vector<uintptr_t> minionEntries(8192);
        const int rawMinionManagerCount = ::Core::ObjectManager::EnumerateMinions(
            minionEntries.data(),
            static_cast<int>(minionEntries.size()));

        std::vector<GameObject> managerHeroes;
        managerHeroes.reserve(rawHeroManagerCount > 0
            ? static_cast<std::size_t>(rawHeroManagerCount)
            : 0);
        for (int i = 0; i < rawHeroManagerCount; ++i) {
            auto handle = ::Core::ObjectManager::MakeHandle(
                heroEntries[static_cast<std::size_t>(i)],
                ::Core::Objects::ObjectType::AIHeroClient);
            if (handle.HasAddress() && handle.HasIdentity()) {
                managerHeroes.emplace_back(handle);
            }
        }

        std::vector<GameObject> managerMinions;
        managerMinions.reserve(rawMinionManagerCount > 0
            ? static_cast<std::size_t>(rawMinionManagerCount)
            : 0);
        for (int i = 0; i < rawMinionManagerCount; ++i) {
            auto handle = ::Core::ObjectManager::MakeHandle(
                minionEntries[static_cast<std::size_t>(i)],
                ::Core::Objects::ObjectType::AIMinionClient);
            if (handle.HasAddress() && handle.HasIdentity()) {
                managerMinions.emplace_back(handle);
            }
        }

        const int expectedHeroCount = static_cast<int>(managerHeroes.size());
        const int expectedMinionCount = static_cast<int>(managerMinions.size());

        if (!playerHandle.HasAddress() || !playerHandle.HasIdentity() ||
            expectedHeroCount <= 0) {
            NightSharpDebug::Logf(
                "[GameObjects] Seed deferred player=%d expectedHeroes=%d expectedMinions=%d",
                playerHandle.HasAddress() && playerHandle.HasIdentity() ? 1 : 0,
                expectedHeroCount,
                expectedMinionCount);
            return false;
        }

        // A seed is a one-shot bootstrap operation, so it must not inherit a
        // possibly empty per-frame ObjectManager::Get<T>() memo.
        NightSharpDebug::Logf(
            "[GameObjects] Seed begin managerHeroes=%d managerMinions=%d",
            expectedHeroCount,
            expectedMinionCount);
        const auto rawObjects = SDK::ObjectManager::GetUncached<GameObject>();
        NightSharpDebug::Logf(
            "[GameObjects] Seed raw enumeration complete count=%zu",
            rawObjects.size());
        if (rawObjects.empty()) {
            NightSharpDebug::Logf(
                "[GameObjects] Seed raw=0 heroes=0/%d minions=0/%d player=0 complete=0",
                expectedHeroCount,
                expectedMinionCount);
            return false;
        }

        std::size_t seededHeroCount = 0;
        std::size_t seededMinionCount = 0;
        bool playerSeeded = false;
        bool complete = false;
        {
            Lock lk(g_mutex);
            Clear();
            PlayerObject = player;

            for (const auto& obj : rawObjects) {
                // GetUncached already classified the raw object. Do not repeat
                // manager scans or mutate StaticStringCache from the game hook.
                OnObjectAdd(obj, false, false, false, false);
            }

            // The raw pass remains authoritative for all objects. Typed manager
            // entries then reconcile/upgrade duplicate NetworkIds, guaranteeing
            // heroes/minions are not lost because a generic handle arrived first.
            for (const auto& hero : managerHeroes) {
                OnObjectAdd(hero, false, false, false, false);
            }
            for (const auto& minion : managerMinions) {
                OnObjectAdd(minion, false, false, false, false);
            }

            seededHeroCount = HeroesList.size();
            for (const auto& seeded : GameObjectsList) {
                const auto type = seeded.Handle().type;
                if (type == ::Core::Objects::ObjectType::AIMinionClient ||
                    type == ::Core::Objects::ObjectType::NeutralMinionCampClient) {
                    ++seededMinionCount;
                }
            }
            playerSeeded = ContainsByNetworkId(
                HeroesList,
                static_cast<int>(playerHandle.networkId));
            complete = playerSeeded &&
                       seededHeroCount >= static_cast<std::size_t>(expectedHeroCount);
        }

        NightSharpDebug::Logf(
            "[GameObjects] Seed raw=%zu heroes=%zu minions=%zu expectedHeroes=%d expectedMinions=%d player=%d complete=%d",
            rawObjects.size(),
            seededHeroCount,
            seededMinionCount,
            expectedHeroCount,
            expectedMinionCount,
            playerSeeded ? 1 : 0,
            complete ? 1 : 0);
        return complete;
    }

    inline bool SeedAllGameObjects() {
        if (!TryBeginSeed()) {
            return false;
        }

        bool complete = false;
        try {
            complete = SeedAllGameObjectsImpl();
        } catch (...) {
            NightSharpDebug::Logf(
                "[GameObjects] Seed aborted by C++ exception; retrying next frame");
        }
        FinishSeed(complete);
        return complete;
    }

    inline bool SeedNeedsRetry() {
        Lock lk(g_mutex);
        return SeedRetryPending;
    }

    inline void ResetSeedState() {
        Lock lk(g_mutex);
        SeedRetryPending = true;
        SeedInProgress = false;
    }

    inline void RetrySeedOnNextUpdate() {
        if (SeedNeedsRetry()) {
            (void)SeedAllGameObjects();
        }
    }

    // Copy-out helper: every public accessor funnels through this so the
    // "no shared reference ever escapes" rule is enforced in one place.
    template <typename T>
    inline std::vector<T> Snapshot(const std::vector<T>& list) {
        Lock lk(g_mutex);
        return list;
    }

    // Zero-allocation copy-out: fills the caller-owned span under the lock and
    // returns how many elements were written. The caller keeps a preallocated
    // buffer (e.g. a reserved vector) so steady-state observation incurs no
    // heap allocation.
    template <typename T>
    inline std::size_t SnapshotInto(const std::vector<T>& list, std::span<T> output) {
        Lock lk(g_mutex);
        const std::size_t count = std::min(list.size(), output.size());
        std::copy_n(list.begin(), count, output.begin());
        return count;
    }

    // ------------------- lifecycle events (EnsoulSharp-style) ---------------
    // Backing for GameObjects::OnCreate / OnDelete. Handlers are plain function
    // pointers (EnsoulSharp's GameObjectCreate delegate carries the sender).
    using LifecycleHandler = void(*)(const GameObject&);
    inline std::vector<LifecycleHandler> CreateHandlers;
    inline std::vector<LifecycleHandler> DeleteHandlers;
    inline bool NativeLifecycleHooked = false;
    inline bool NativeUpdateHooked = false;

    inline void DispatchPrunedDeletes() {
        std::vector<::Core::Objects::ObjectHandle> deleted;
        std::vector<LifecycleHandler> handlers;
        {
            Lock lk(g_mutex);
            if (PendingPrunedDeletes.empty()) {
                return;
            }
            deleted.swap(PendingPrunedDeletes);
            handlers = DeleteHandlers;
        }

        for (const auto& handle : deleted) {
            const GameObject object(handle);
            for (size_t index = 0; index < handlers.size(); ++index) {
                const auto handler = handlers[index];
                if (!handler) {
                    continue;
                }
                const auto perfStart = NightSharpPerf::Now();
                handler(object);
                const double ms = NightSharpPerf::MsSince(perfStart);
                NightSharpPerf::AddEventHandlerTiming(
                    "GameObjects::OnDelete(Prune)",
                    static_cast<int>(index),
                    reinterpret_cast<const void*>(handler),
                    ms);
            }
        }
    }

    inline GameObject ObjectFromArgs(const SDK::Events::ObjectEventArgs& args) {
        ::Core::Objects::ObjectHandle handle{};
        handle.address   = args.Sender.Ptr;
        handle.index     = args.Sender.Index;
        handle.networkId = args.Sender.NetworkId;
        handle.type      = args.Sender.Type;
        return GameObject(handle);
    }

    inline void DispatchLifecycle(const char* eventName,
                                  std::vector<LifecycleHandler>& source,
                                  const SDK::Events::ObjectEventArgs& args) {
        if (!args.Sender.IsValid()) {
            return;
        }
        std::vector<LifecycleHandler> handlers;
        {
            Lock lk(g_mutex);
            handlers = source; // copy so a handler may (un)subscribe re-entrantly
        }
        const GameObject object = ObjectFromArgs(args);
        for (size_t i = 0; i < handlers.size(); ++i) {
            const auto handler = handlers[i];
            if (handler) {
                const auto perfStart = NightSharpPerf::Now();
                handler(object);
                const double ms = NightSharpPerf::MsSince(perfStart);
                NightSharpPerf::AddEventHandlerTiming(
                    eventName, static_cast<int>(i), reinterpret_cast<const void*>(handler), ms);
            }
        }
    }

    inline void OnNativeObjectCreate(const SDK::Events::ObjectEventArgs& args) {
        NS_PROFILE("GameObjects::OnNativeObjectCreate");
        if (args.Sender.IsValid()) {
            const auto start = NightSharpPerf::Now();
            OnObjectAdd(ObjectFromArgs(args));
            const double ms = NightSharpPerf::MsSince(start);
            NightSharpPerf::AddEventHandlerTiming("GameObjects::OnObjectAdd", 0, reinterpret_cast<const void*>(&OnObjectAdd), ms);
        }
        DispatchLifecycle("GameObjects::OnCreate", CreateHandlers, args);
    }

    inline void OnNativeObjectDelete(const SDK::Events::ObjectEventArgs& args) {
        NS_PROFILE("GameObjects::OnNativeObjectDelete");
        if (args.Sender.IsValid()) {
            {
                // Prefer the authoritative native delete if it arrives before
                // the queued prune fallback is dispatched.
                Lock lk(g_mutex);
                PendingPrunedDeletes.erase(
                    std::remove_if(
                        PendingPrunedDeletes.begin(), PendingPrunedDeletes.end(),
                        [&args](const ::Core::Objects::ObjectHandle& handle) {
                            return handle.networkId == args.Sender.NetworkId;
                        }),
                    PendingPrunedDeletes.end());
            }
            StaticStringCache::Clear(static_cast<std::uint32_t>(args.Sender.Index & 0xFFFFu));
            const auto start = NightSharpPerf::Now();
            OnObjectDelete(static_cast<int>(args.Sender.NetworkId), args.Sender.Type);
            const double ms = NightSharpPerf::MsSince(start);
            NightSharpPerf::AddEventHandlerTiming("GameObjects::OnObjectDelete", 0, reinterpret_cast<const void*>(&OnObjectDelete), ms);
        }
        DispatchLifecycle("GameObjects::OnDelete", DeleteHandlers, args);
    }

    inline void OnNativeGameUpdate(const SDK::Events::GameUpdateEventArgs&) {
        NS_PROFILE("GameObjects::SeedRetry");
        DispatchPrunedDeletes();
        RetrySeedOnNextUpdate();
    }

    // Subscribe our forwarders to the (deferred, update-tick) native lifecycle
    // events exactly once. Kept out from under g_mutex while calling into
    // SDK::Events so the two subsystems' locks never nest.
    inline void EnsureNativeLifecycleHooked() {
        {
            Lock lk(g_mutex);
            if (NativeLifecycleHooked) {
                return;
            }
            NativeLifecycleHooked = true;
            NativeUpdateHooked = true;
            SeedRetryPending = true;
            SeedInProgress = false;
        }
        SDK::Events::AddOnGameUpdate(&OnNativeGameUpdate);
        SDK::Events::AddOnCreateObject(&OnNativeObjectCreate);
        SDK::Events::AddOnDeleteObject(&OnNativeObjectDelete);
        (void)SeedAllGameObjects();
    }

    inline void ReleaseNativeLifecycleHook() {
        bool wasHooked = false;
        bool wasUpdateHooked = false;
        {
            Lock lk(g_mutex);
            wasHooked = NativeLifecycleHooked;
            wasUpdateHooked = NativeUpdateHooked;
            NativeLifecycleHooked = false;
            NativeUpdateHooked = false;
            SeedRetryPending = true;
            SeedInProgress = false;
            CreateHandlers.clear();
            DeleteHandlers.clear();
            PendingPrunedDeletes.clear();
        }
        if (wasUpdateHooked) {
            SDK::Events::RemoveOnGameUpdate(&OnNativeGameUpdate);
        }
        if (wasHooked) {
            SDK::Events::RemoveOnCreateObject(&OnNativeObjectCreate);
            SDK::Events::RemoveOnDeleteObject(&OnNativeObjectDelete);
        }
    }

} // namespace detail

// ------------------------------- lifecycle ----------------------------------
inline void Initialize() {
    if (detail::Initialized) {
        return;
    }
    ::Core::ObjectManager::TypeCache::InvalidateAll();
    detail::ResetSeedState();
    detail::Initialized = true;
    SDK::GameObject::WarmPlayerTeamCache();
    // EnsoulSharp-style: keep the lists event-fresh. Delivery is deferred to the
    // update tick, so this is safe (see the threading-model note at the top).
    detail::EnsureNativeLifecycleHooked();
}

inline void Shutdown() {
    detail::ReleaseNativeLifecycleHook();
    detail::Clear();
    ::Core::ObjectManager::TypeCache::InvalidateAll();
    detail::Initialized = false;
}

// Cheap cached-membership query shared by consumers that need to agree with
// the AllGameObjects facade. It performs no ObjectManager scan/native read.
inline bool ContainsNetworkId(int networkId) {
    if (networkId == 0) {
        return false;
    }
    Initialize();
    detail::Lock lk(detail::g_mutex);
    return std::any_of(
        detail::GameObjectsList.begin(), detail::GameObjectsList.end(),
        [networkId](const GameObject& object) {
            return static_cast<int>(object.CachedNetworkId()) == networkId;
        });
}

// --------------------- lifecycle events (EnsoulSharp-style) ------------------
// GameObjects::OnCreate / OnDelete — mirrors EnsoulSharp.SDK's GameObject.OnCreate
// / OnDelete. The handler is fired on the update tick for every object the game
// creates / destroys and receives the base GameObject; filter with sender.Type()
// or wrap it (e.g. AITurretClient(sender.Handle())) just like EnsoulSharp's
// `sender as AITurretClient`. Add*/Remove* return true on success; On* is the
// EnsoulSharp `+=` alias for Add*.
inline bool AddOnCreate(void(*handler)(const GameObject&)) {
    if (!handler) {
        return false;
    }
    detail::EnsureNativeLifecycleHooked();
    detail::Lock lk(detail::g_mutex);
    for (const auto existing : detail::CreateHandlers) {
        if (existing == handler) {
            return true;
        }
    }
    detail::CreateHandlers.push_back(handler);
    return true;
}

inline bool RemoveOnCreate(void(*handler)(const GameObject&)) {
    detail::Lock lk(detail::g_mutex);
    const auto before = detail::CreateHandlers.size();
    detail::CreateHandlers.erase(
        std::remove(detail::CreateHandlers.begin(), detail::CreateHandlers.end(), handler),
        detail::CreateHandlers.end());
    return detail::CreateHandlers.size() != before;
}

inline bool OnCreate(void(*handler)(const GameObject&)) { return AddOnCreate(handler); }

inline bool AddOnDelete(void(*handler)(const GameObject&)) {
    if (!handler) {
        return false;
    }
    detail::EnsureNativeLifecycleHooked();
    detail::Lock lk(detail::g_mutex);
    for (const auto existing : detail::DeleteHandlers) {
        if (existing == handler) {
            return true;
        }
    }
    detail::DeleteHandlers.push_back(handler);
    return true;
}

inline bool RemoveOnDelete(void(*handler)(const GameObject&)) {
    detail::Lock lk(detail::g_mutex);
    const auto before = detail::DeleteHandlers.size();
    detail::DeleteHandlers.erase(
        std::remove(detail::DeleteHandlers.begin(), detail::DeleteHandlers.end(), handler),
        detail::DeleteHandlers.end());
    return detail::DeleteHandlers.size() != before;
}

inline bool OnDelete(void(*handler)(const GameObject&)) { return AddOnDelete(handler); }

inline void EnsureInitialized() {
    Initialize();
    // Resolve PlayerObject once on first call; afterward re-use the cached handle.
    // ObjectManager::Player() builds a 92-read snapshot every call, so we only
    // pay that cost once (or when the player handle is not yet valid).
    if (!detail::PlayerObject.IsValid()) {
        detail::PlayerObject = SDK::ObjectManager::Player();
    }
}

inline AIHeroClient Player() {
    EnsureInitialized();
    return detail::PlayerObject;
}

// Champion id of the local player, resolved via FNV-1a hashes of the handle
// character names and cached once per game. The hot plugins re-ask this every
// frame (CanAttack/CanMove/evade/activator passes); a straight
// ChampionIdFromName() call does up to ~500 _stricmp per lookup, so this is a
// per-frame FPS hotspot that must only pay once.
inline ChampionId PlayerChampionId() {
    EnsureInitialized();
    if (detail::PlayerChampionIdCached) {
        return detail::PlayerChampionIdObject;
    }
    const AIHeroClient player = detail::PlayerObject;
    if (player.IsValid()) {
        detail::PlayerChampionIdObject =
            ChampionIdFromName(player.CharacterName().c_str());
    }
    detail::PlayerChampionIdCached = true;
    return detail::PlayerChampionIdObject;
}

// ------------------------------- accessors ----------------------------------
inline std::vector<AIHeroClient> Heroes() {
    return detail::Snapshot(detail::HeroesList);
}

inline std::size_t HeroesInto(std::span<AIHeroClient> output) {
    return detail::SnapshotInto(detail::HeroesList, output);
}

inline std::vector<AIHeroClient> AllyHeroes() {
    return detail::Snapshot(detail::AllyHeroesList);
}

inline std::size_t AllyHeroesInto(std::span<AIHeroClient> output) {
    return detail::SnapshotInto(detail::AllyHeroesList, output);
}

inline std::vector<AIHeroClient> EnemyHeroes() {
    return detail::Snapshot(detail::EnemyHeroesList);
}

inline std::size_t EnemyHeroesInto(std::span<AIHeroClient> output) {
    return detail::SnapshotInto(detail::EnemyHeroesList, output);
}

inline std::vector<AIMinionClient> Minions() {
    return detail::Snapshot(detail::MinionsList);
}

inline std::size_t MinionsInto(std::span<AIMinionClient> output) {
    return detail::SnapshotInto(detail::MinionsList, output);
}

inline std::vector<AIMinionClient> AllyMinions() {
    return detail::Snapshot(detail::AllyMinionsList);
}

inline std::size_t AllyMinionsInto(std::span<AIMinionClient> output) {
    return detail::SnapshotInto(detail::AllyMinionsList, output);
}

inline std::vector<AIMinionClient> EnemyMinions() {
    return detail::Snapshot(detail::EnemyMinionsList);
}

inline std::size_t EnemyMinionsInto(std::span<AIMinionClient> output) {
    return detail::SnapshotInto(detail::EnemyMinionsList, output);
}

inline std::vector<AIMinionClient> AllyLaneMinions() { return detail::Snapshot(detail::AllyLaneMinionsList); }
inline std::vector<AIMinionClient> EnemyLaneMinions() { return detail::Snapshot(detail::EnemyLaneMinionsList); }

inline std::size_t AllyLaneMinionsInto(std::span<AIMinionClient> output) {
    return detail::SnapshotInto(detail::AllyLaneMinionsList, output);
}
inline std::size_t EnemyLaneMinionsInto(std::span<AIMinionClient> output) {
    return detail::SnapshotInto(detail::EnemyLaneMinionsList, output);
}
inline std::vector<AIMinionClient> AllySpecialMinions() { return detail::Snapshot(detail::AllySpecialMinionsList); }
inline std::vector<AIMinionClient> EnemySpecialMinions() { return detail::Snapshot(detail::EnemySpecialMinionsList); }
inline std::size_t AllySpecialMinionsInto(std::span<AIMinionClient> output) {
    return detail::SnapshotInto(detail::AllySpecialMinionsList, output);
}
inline std::size_t EnemySpecialMinionsInto(std::span<AIMinionClient> output) {
    return detail::SnapshotInto(detail::EnemySpecialMinionsList, output);
}
inline std::vector<AIMinionClient> AllyIgnoredMinions() { return detail::Snapshot(detail::AllyIgnoredMinionsList); }
inline std::vector<AIMinionClient> EnemyIgnoredMinions() { return detail::Snapshot(detail::EnemyIgnoredMinionsList); }
inline std::size_t EnemyIgnoredMinionsInto(std::span<AIMinionClient> output) {
    return detail::SnapshotInto(detail::EnemyIgnoredMinionsList, output);
}
inline std::vector<AIMinionClient> Wards() { return detail::Snapshot(detail::WardsList); }

inline std::size_t WardsInto(std::span<AIMinionClient> output) {
    return detail::SnapshotInto(detail::WardsList, output);
}
inline std::vector<AIMinionClient> AllyWards() { return detail::Snapshot(detail::AllyWardsList); }
inline std::vector<AIMinionClient> EnemyWards() { return detail::Snapshot(detail::EnemyWardsList); }
inline std::size_t EnemyWardsInto(std::span<AIMinionClient> output) {
    return detail::SnapshotInto(detail::EnemyWardsList, output);
}
inline std::vector<AIMinionClient> Jungle() { return detail::Snapshot(detail::JungleList); }
inline std::size_t JungleInto(std::span<AIMinionClient> output) {
    return detail::SnapshotInto(detail::JungleList, output);
}
inline std::size_t PlantsInto(std::span<AIMinionClient> output) {
    return detail::SnapshotInto(detail::PlantsList, output);
}

// ------------------------ zero-allocation frame snapshots -------------------
// Each (T, Fill) specialization retains its capacity and publishes one settled
// snapshot per simulation frame. The generic three-parameter implementation is
// isolated in FrameSnapshot.h so fake fills and frame keys can exercise the
// truncation boundary without pulling in the runtime object layer.
template <typename T, std::size_t (*Fill)(std::span<T>)>
inline const std::vector<T>& FrameSnapshot() {
    return FrameSnapshot<T, Fill, &::CoreAiManager::FrameCacheKey>();
}

inline const std::vector<AIHeroClient>& AllyHeroesFrame() {
    return FrameSnapshot<AIHeroClient, &AllyHeroesInto>();
}
inline const std::vector<AIHeroClient>& HeroesFrame() {
    return FrameSnapshot<AIHeroClient, &HeroesInto>();
}
inline const std::vector<AIHeroClient>& EnemyHeroesFrame() {
    return FrameSnapshot<AIHeroClient, &EnemyHeroesInto>();
}
inline const std::vector<AIMinionClient>& MinionsFrame() {
    return FrameSnapshot<AIMinionClient, &MinionsInto>();
}
inline const std::vector<AIMinionClient>& AllyLaneMinionsFrame() {
    return FrameSnapshot<AIMinionClient, &AllyLaneMinionsInto>();
}
inline const std::vector<AIMinionClient>& EnemyLaneMinionsFrame() {
    return FrameSnapshot<AIMinionClient, &EnemyLaneMinionsInto>();
}
inline const std::vector<AIMinionClient>& AllyMinionsFrame() {
    return FrameSnapshot<AIMinionClient, &AllyMinionsInto>();
}
inline const std::vector<AIMinionClient>& EnemyMinionsFrame() {
    return FrameSnapshot<AIMinionClient, &EnemyMinionsInto>();
}
inline const std::vector<AIMinionClient>& JungleFrame() {
    return FrameSnapshot<AIMinionClient, &JungleInto>();
}
inline const std::vector<AIMinionClient>& PlantsFrame() {
    return FrameSnapshot<AIMinionClient, &PlantsInto>();
}
inline const std::vector<AIMinionClient>& EnemySpecialMinionsFrame() {
    return FrameSnapshot<AIMinionClient, &EnemySpecialMinionsInto>();
}
inline const std::vector<AIMinionClient>& AllySpecialMinionsFrame() {
    return FrameSnapshot<AIMinionClient, &AllySpecialMinionsInto>();
}
inline const std::vector<AIMinionClient>& EnemyWardsFrame() {
    return FrameSnapshot<AIMinionClient, &EnemyWardsInto>();
}
inline std::size_t EnemyClonesInto(std::span<AIMinionClient> output) {
    return detail::SnapshotInto(detail::EnemyClonesList, output);
}
inline const std::vector<AIMinionClient>& EnemyClonesFrame() {
    return FrameSnapshot<AIMinionClient, &EnemyClonesInto>();
}
inline std::size_t EnemyPetsInto(std::span<AIMinionClient> output) {
    return detail::SnapshotInto(detail::EnemyPetsList, output);
}
inline const std::vector<AIMinionClient>& EnemyPetsFrame() {
    return FrameSnapshot<AIMinionClient, &EnemyPetsInto>();
}
inline std::vector<AIMinionClient> JungleMinions() { return Jungle(); }
inline std::vector<AIMinionClient> JungleSmall() { return detail::Snapshot(detail::JungleSmallList); }
inline std::vector<AIMinionClient> JungleLarge() { return detail::Snapshot(detail::JungleLargeList); }
inline std::vector<AIMinionClient> JungleLegendary() { return detail::Snapshot(detail::JungleLegendaryList); }

inline std::size_t JungleLargeInto(std::span<AIMinionClient> output) {
    return detail::SnapshotInto(detail::JungleLargeList, output);
}
inline std::size_t JungleLegendaryInto(std::span<AIMinionClient> output) {
    return detail::SnapshotInto(detail::JungleLegendaryList, output);
}
inline std::vector<AIMinionClient> SmallJungle() { return JungleSmall(); }
inline std::vector<AIMinionClient> LargeJungle() { return JungleLarge(); }
inline std::vector<AIMinionClient> EpicJungle() { return JungleLegendary(); }
inline std::vector<AIMinionClient> Plants() { return detail::Snapshot(detail::PlantsList); }
inline std::vector<AIMinionClient> JunglePlants() { return Plants(); }
inline std::vector<AIMinionClient> Clones() { return detail::Snapshot(detail::ClonesList); }
inline std::vector<AIMinionClient> AllyClones() { return detail::Snapshot(detail::AllyClonesList); }
inline std::vector<AIMinionClient> EnemyClones() { return detail::Snapshot(detail::EnemyClonesList); }
inline std::vector<AIMinionClient> Pets() { return detail::Snapshot(detail::PetsList); }
inline std::vector<AIMinionClient> AllyPets() { return detail::Snapshot(detail::AllyPetsList); }
inline std::vector<AIMinionClient> EnemyPets() { return detail::Snapshot(detail::EnemyPetsList); }

// REMOVED: Turret object contents disabled by user request. Keep API names.
inline std::vector<AITurretClient> Turrets() { return {}; }
inline std::vector<AITurretClient> AllyTurrets() { return {}; }
inline std::vector<AITurretClient> EnemyTurrets() { return {}; }

// REMOVED: Turret/Inhibitor/Nexus class disabled by user request
// inline std::vector<BarracksDampenerClient> Inhibitors() {
//     return detail::Snapshot(detail::InhibitorsList);
// }
// inline std::vector<BarracksDampenerClient> AllyInhibitors() {
//     return detail::Snapshot(detail::AllyInhibitorsList);
// }
// inline std::vector<BarracksDampenerClient> EnemyInhibitors() {
//     return detail::Snapshot(detail::EnemyInhibitorsList);
// }
//
// inline std::vector<HQClient> Nexuses() {
//     return detail::Snapshot(detail::NexusList);
// }
// inline HQClient AllyNexus() {
//     detail::Lock lk(detail::g_mutex);
//     return detail::AllyNexusObject;
// }
// inline HQClient EnemyNexus() {
//     detail::Lock lk(detail::g_mutex);
//     return detail::EnemyNexusObject;
// }
//
// Compatibility aliases (previous API); same snapshot data.
inline std::vector<AITurretClient> ScanTurrets() { return {}; }
// inline std::vector<BarracksDampenerClient> ScanInhibitors() { return Inhibitors(); }
// inline std::vector<HQClient> ScanNexuses() { return Nexuses(); }

// REMOVED: Shop object contents disabled by user request. Keep API names.
inline std::vector<ShopClient> Shops() { return {}; }
inline std::vector<ShopClient> AllyShops() { return {}; }
inline std::vector<ShopClient> EnemyShops() { return {}; }

inline std::vector<Obj_SpawnPoint> SpawnPoints() { return detail::Snapshot(detail::SpawnPointsList); }
inline std::vector<Obj_SpawnPoint> AllySpawnPoints() { return detail::Snapshot(detail::AllySpawnPointsList); }
inline std::vector<Obj_SpawnPoint> EnemySpawnPoints() { return detail::Snapshot(detail::EnemySpawnPointsList); }

inline std::vector<EffectEmitter> ParticleEmitters() { return detail::Snapshot(detail::ParticleEmittersList); }
inline std::vector<MissileClient> Missiles() {
    return detail::Snapshot(detail::MissilesList);
}
inline std::size_t MissilesInto(std::span<MissileClient> output) {
    return detail::SnapshotInto(detail::MissilesList, output);
}
inline std::vector<GameObject> AllGameObjects() { return detail::Snapshot(detail::GameObjectsList); }
inline std::vector<AttackableUnit> AttackableUnits() { return detail::Snapshot(detail::AttackableUnitsList); }
inline std::vector<AIBaseClient> Ally() { return detail::Snapshot(detail::AllyList); }
inline std::vector<AIBaseClient> Enemy() { return detail::Snapshot(detail::EnemyList); }

inline bool Compare(const GameObject& gameObject, const GameObject& object) {
    return gameObject.Compare(object);
}

inline Vec3 PlayerPosition() {
    const AIHeroClient player = Player();
    return player.IsValid() ? player.Position() : Vec3{};
}

template <typename T>
inline std::vector<T> Get() {
    if constexpr (std::is_same_v<T, GameObject>) {
        return AllGameObjects();
    } else if constexpr (std::is_same_v<T, AttackableUnit>) {
        return AttackableUnits();
    } else if constexpr (std::is_same_v<T, AIBaseClient>) {
        detail::Lock lk(detail::g_mutex);
        std::vector<T> result;
        result.reserve(detail::AllyList.size() + detail::EnemyList.size());
        result.insert(result.end(), detail::AllyList.begin(), detail::AllyList.end());
        result.insert(result.end(), detail::EnemyList.begin(), detail::EnemyList.end());
        return result;
    } else if constexpr (std::is_same_v<T, AIHeroClient>) {
        return Heroes();
    } else if constexpr (std::is_same_v<T, AIMinionClient>) {
        detail::Lock lk(detail::g_mutex);
        std::vector<T> result;
        result.reserve(detail::MinionsList.size() + detail::WardsList.size() +
                       detail::JungleList.size() + detail::PlantsList.size() +
                       detail::PetsList.size() + detail::ClonesList.size());
        result.insert(result.end(), detail::MinionsList.begin(), detail::MinionsList.end());
        result.insert(result.end(), detail::WardsList.begin(), detail::WardsList.end());
        result.insert(result.end(), detail::JungleList.begin(), detail::JungleList.end());
        result.insert(result.end(), detail::PlantsList.begin(), detail::PlantsList.end());
        result.insert(result.end(), detail::PetsList.begin(), detail::PetsList.end());
        result.insert(result.end(), detail::ClonesList.begin(), detail::ClonesList.end());
        return result;
    // REMOVED: Turret/Inhibitor/Nexus class disabled by user request
    // } else if constexpr (std::is_same_v<T, AITurretClient>) {
    //     return Turrets();
    // } else if constexpr (std::is_same_v<T, BarracksDampenerClient>) {
    //     return Inhibitors();
    // } else if constexpr (std::is_same_v<T, HQClient>) {
    //     return Nexuses();
    } else if constexpr (std::is_same_v<T, EffectEmitter>) {
        return ParticleEmitters();
    } else if constexpr (std::is_same_v<T, MissileClient>) {
        return Missiles();
    // } else if constexpr (std::is_same_v<T, ShopClient>) {
    //     return Shops();
    } else if constexpr (std::is_same_v<T, Obj_SpawnPoint>) {
        return SpawnPoints();
    } else {
        return {};
    }
}

template <typename T>
inline std::vector<T> GetNative() {
    return SDK::ObjectManager::Get<T>();
}

template <typename T>
inline T GetUnitByNetworkId(int networkId) {
    return SDK::ObjectManager::GetUnitByNetworkId<T>(networkId);
}

} // namespace SDK::GameObjects

namespace SDK::ObjectManager {
    inline std::vector<AIHeroClient> Heroes() { return SDK::GameObjects::Heroes(); }
    inline std::vector<AIHeroClient> AllyHeroes() { return SDK::GameObjects::AllyHeroes(); }
    inline std::vector<AIHeroClient> EnemyHeroes() { return SDK::GameObjects::EnemyHeroes(); }
    inline std::vector<AIMinionClient> Minions() { return SDK::GameObjects::Minions(); }
    inline std::vector<AIMinionClient> AllyMinions() { return SDK::GameObjects::AllyMinions(); }
    inline std::vector<AIMinionClient> EnemyMinions() { return SDK::GameObjects::EnemyMinions(); }
    inline std::vector<AIMinionClient> AllyClones() { return SDK::GameObjects::AllyClones(); }
    inline std::vector<AIMinionClient> EnemyClones() { return SDK::GameObjects::EnemyClones(); }
    inline std::vector<AIMinionClient> Clones() { return SDK::GameObjects::Clones(); }
    inline std::vector<AIMinionClient> AllyPets() { return SDK::GameObjects::AllyPets(); }
    inline std::vector<AIMinionClient> EnemyPets() { return SDK::GameObjects::EnemyPets(); }
    inline std::vector<AIMinionClient> Pets() { return SDK::GameObjects::Pets(); }
    inline std::vector<AIMinionClient> Jungle() { return SDK::GameObjects::Jungle(); }
    inline std::vector<AIMinionClient> JungleMinions() { return SDK::GameObjects::JungleMinions(); }
    inline std::vector<AIMinionClient> SmallJungle() { return SDK::GameObjects::SmallJungle(); }
    inline std::vector<AIMinionClient> LargeJungle() { return SDK::GameObjects::LargeJungle(); }
    inline std::vector<AIMinionClient> EpicJungle() { return SDK::GameObjects::EpicJungle(); }
    inline std::vector<AIMinionClient> Plants() { return SDK::GameObjects::Plants(); }
    inline std::vector<AIMinionClient> Wards() { return SDK::GameObjects::Wards(); }
    // REMOVED: Turret object contents disabled by user request. Keep API names.
    inline std::vector<AITurretClient> Turrets() { return {}; }
    inline std::vector<AITurretClient> AllyTurrets() { return {}; }
    inline std::vector<AITurretClient> EnemyTurrets() { return {}; }
    inline std::vector<ShopClient> Shops() { return {}; }
    inline std::vector<ShopClient> AllyShops() { return {}; }
    inline std::vector<ShopClient> EnemyShops() { return {}; }
    // inline std::vector<BarracksDampenerClient> EnemyInhibitors() { return SDK::GameObjects::EnemyInhibitors(); }
    // inline HQClient EnemyNexus() { return SDK::GameObjects::EnemyNexus(); }
    inline std::vector<MissileClient> Missiles() { return SDK::GameObjects::Missiles(); }
    inline std::vector<GameObject> AllObjects() { return SDK::GameObjects::AllGameObjects(); }
}

namespace SDK {

inline int AIBaseClient::CountAllyHeroesInRange(float range) const {
    int count = 0;
    const float rangeSqr = range * range;
    for (const auto& hero : GameObjects::AllyHeroesFrame()) {
        if (!hero.IsValid() || hero.IsDead() || hero.IsInvulnerable() || hero.Compare(*this)) {
            continue;
        }
        if (hero.Position().DistanceSqr2D(Position()) <= rangeSqr) {
            ++count;
        }
    }
    return count;
}

inline int AIBaseClient::CountEnemyHeroesInRange(float range) const {
    int count = 0;
    const float rangeSqr = range * range;
    for (const auto& hero : GameObjects::EnemyHeroesFrame()) {
        if (!hero.IsValid() || hero.IsDead() || hero.IsInvulnerable() || hero.Compare(*this)) {
            continue;
        }
        if (hero.Position().DistanceSqr2D(Position()) <= rangeSqr) {
            ++count;
        }
    }
    return count;
}

// REMOVED: Turret/Inhibitor/Nexus class disabled by user request
inline bool AIBaseClient::IsUnderAllyTurret() const {
    // for (const auto& turret : GameObjects::AllyTurrets()) {
    //     if (!turret.IsValid() || turret.IsDead()) {
    //         continue;
    //     }
    //     const float range = turret.AttackRange() + turret.BoundingRadius() + BoundingRadius();
    //     if (turret.Position().DistanceSqr2D(Position()) <= range * range) {
    //         return true;
    //     }
    // }
    return false;
}

inline bool AIBaseClient::IsUnderEnemyTurret() const {
    // for (const auto& turret : GameObjects::EnemyTurrets()) {
    //     if (!turret.IsValid() || turret.IsDead()) {
    //         continue;
    //     }
    //     const float range = turret.AttackRange() + turret.BoundingRadius() + BoundingRadius();
    //     if (turret.Position().DistanceSqr2D(Position()) <= range * range) {
    //         return true;
    //     }
    // }
    return false;
}

} // namespace SDK
