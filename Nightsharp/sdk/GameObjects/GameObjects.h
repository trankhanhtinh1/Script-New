#pragma once

// ============================================================================
// SDK::GameObjects — snapshot-based object store.
// ----------------------------------------------------------------------------

#include "ObjectManager.h"
#include "StructureScan.h"
#include "../Events/Events.h"
#include "../../CrashTrace.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <mutex>
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
    inline std::vector<ShopClient> ShopsList;
    inline std::vector<ShopClient> AllyShopsList;
    inline std::vector<ShopClient> EnemyShopsList;
    inline std::vector<Obj_SpawnPoint> SpawnPointsList;
    inline std::vector<Obj_SpawnPoint> AllySpawnPointsList;
    inline std::vector<Obj_SpawnPoint> EnemySpawnPointsList;
    inline std::vector<EffectEmitter> ParticleEmittersList;
    inline std::vector<MissileClient> MissilesList;

    inline AIHeroClient PlayerObject;
    inline bool Initialized = false;


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
            "sru_plant", "hiddenminionplantdemon",
            "planthealthmirrored", "plantmasterminion", "minimapicon",
            "plant_satchel", "plant_health", "plant_vision"
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
        return minion.IsMinion() || IsLaneMinionName(name);
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
            "yorickbigghoul", "zyrathornplant", "zyragraspingplant"
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
            if (static_cast<int>(item.Handle().networkId) == netId) return true;
        }
        return false;
    }

    template <typename T>
    inline void PushUniqueByNetworkId(std::vector<T>& vec, const T& obj) {
        const int netId = static_cast<int>(obj.Handle().networkId);
        if (netId != 0 && ContainsByNetworkId(vec, netId)) {
            return;
        }
        vec.push_back(obj);
    }

    template <typename T>
    inline void EraseByNetworkId(std::vector<T>& vec, int netId) {
        if (vec.empty()) return;
        vec.erase(std::remove_if(vec.begin(), vec.end(), [netId](const T& obj) {
            return static_cast<int>(obj.Handle().networkId) == netId;
        }), vec.end());
    }

    template <typename T>
    inline void CleanInvalid(std::vector<T>& vec) {
        if (vec.empty()) return;
        vec.erase(std::remove_if(vec.begin(), vec.end(), [](const T& obj) {
            return !obj.IsValid();
        }), vec.end());
    }

    inline void CleanInvalidObjects() {
        CleanInvalid(GameObjectsList);
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

        CleanInvalid(ShopsList);
        CleanInvalid(AllyShopsList);
        CleanInvalid(EnemyShopsList);
        CleanInvalid(SpawnPointsList);
        CleanInvalid(AllySpawnPointsList);
        CleanInvalid(EnemySpawnPointsList);
        CleanInvalid(ParticleEmittersList);
        CleanInvalid(MissilesList);
    }

    inline void OnObjectAdd(const GameObject& object) {
        if (!object.IsValid()) return;
        Lock lk(g_mutex);
        CleanInvalidObjects();

        PopulateStatic(object);

        const int netId = object.NetworkId();
        if (netId != 0 && ContainsByNetworkId(GameObjectsList, netId)) {
            return;
        }
        PushUniqueByNetworkId(GameObjectsList, object);

        if (::Core::Objects::IsAttackable(object.Type())) {
            PushUniqueByNetworkId(AttackableUnitsList, AttackableUnit(object.Handle()));
        }

        const int myTeam = PlayerTeam();
        const int team = TeamValue(object);
        const bool ally = myTeam != 0 && team == myTeam;
        const bool enemy = team != 0 && team != 300 && !ally;

        const ::Core::Objects::ObjectType type = object.Type();

        switch (type) {
        case ::Core::Objects::ObjectType::AIHeroClient: {
            const AIHeroClient hero(object.Handle());
            PushUniqueByNetworkId(HeroesList, hero);
            if (ally) {
                PushUniqueByNetworkId(AllyHeroesList, hero);
                PushUniqueByNetworkId(AllyList, AIBaseClient(object.Handle()));
            } else if (enemy) {
                PushUniqueByNetworkId(EnemyHeroesList, hero);
                PushUniqueByNetworkId(EnemyList, AIBaseClient(object.Handle()));
            }
            break;
        }
        case ::Core::Objects::ObjectType::AIMinionClient:
        case ::Core::Objects::ObjectType::NeutralMinionCampClient: {
            const AIMinionClient minion(object.Handle());
            if (minion.IsDead()) break;

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
                    PushUniqueByNetworkId(EnemyList, AIBaseClient(object.Handle()));
                } else {
                    PushUniqueByNetworkId(AllyMinionsList, minion);
                    PushUniqueByNetworkId(AllyLaneMinionsList, minion);
                    PushUniqueByNetworkId(AllyList, AIBaseClient(object.Handle()));
                }
            }
            break;
        }
        // REMOVED: Turret/Inhibitor/Nexus class disabled by user request
        // case ::Core::Objects::ObjectType::AITurretClient: {
        //     const AITurretClient turret(object.Handle());
        //     PushUniqueByNetworkId(TurretsList, turret);
        //     if (ally) PushUniqueByNetworkId(AllyTurretsList, turret);
        //     else if (enemy) PushUniqueByNetworkId(EnemyTurretsList, turret);
        //     break;
        // }
        // case ::Core::Objects::ObjectType::BarracksDampenerClient: {
        //     const BarracksDampenerClient inhib(object.Handle());
        //     PushUniqueByNetworkId(InhibitorsList, inhib);
        //     if (ally) PushUniqueByNetworkId(AllyInhibitorsList, inhib);
        //     else if (enemy) PushUniqueByNetworkId(EnemyInhibitorsList, inhib);
        //     break;
        // }
        // case ::Core::Objects::ObjectType::HQClient: {
        //     const HQClient nexus(object.Handle());
        //     PushUniqueByNetworkId(NexusList, nexus);
        //     if (ally) AllyNexusObject = nexus;
        //     else if (enemy) EnemyNexusObject = nexus;
        //     break;
        // }
        case ::Core::Objects::ObjectType::MissileClient: {
            const MissileClient missile(object.Handle());
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
        ShopsList.clear();
        AllyShopsList.clear();
        EnemyShopsList.clear();
        SpawnPointsList.clear();
        AllySpawnPointsList.clear();
        EnemySpawnPointsList.clear();
        ParticleEmittersList.clear();
        MissilesList.clear();
        PlayerObject = {};
    }

    inline void SeedAllGameObjects() {
        Lock lk(g_mutex);
        Clear();
        for (const auto& obj : SDK::ObjectManager::Get<GameObject>()) {
            OnObjectAdd(obj);
        }
    }

    // Copy-out helper: every public accessor funnels through this so the
    // "no shared reference ever escapes" rule is enforced in one place.
    template <typename T>
    inline std::vector<T> Snapshot(const std::vector<T>& list) {
        Lock lk(g_mutex);
        return list;
    }

    // ------------------- lifecycle events (EnsoulSharp-style) ---------------
    // Backing for GameObjects::OnCreate / OnDelete. Handlers are plain function
    // pointers (EnsoulSharp's GameObjectCreate delegate carries the sender).
    using LifecycleHandler = void(*)(const GameObject&);
    inline std::vector<LifecycleHandler> CreateHandlers;
    inline std::vector<LifecycleHandler> DeleteHandlers;
    inline bool NativeLifecycleHooked = false;

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
        if (args.Sender.IsValid()) {
            const auto start = NightSharpPerf::Now();
            OnObjectAdd(ObjectFromArgs(args));
            const double ms = NightSharpPerf::MsSince(start);
            NightSharpPerf::AddEventHandlerTiming("GameObjects::OnObjectAdd", 0, reinterpret_cast<const void*>(&OnObjectAdd), ms);
        }
        DispatchLifecycle("GameObjects::OnCreate", CreateHandlers, args);
    }

    inline void OnNativeObjectDelete(const SDK::Events::ObjectEventArgs& args) {
        if (args.Sender.IsValid()) {
            StaticStringCache::Clear(static_cast<std::uint32_t>(args.Sender.Index & 0xFFFFu));
            const auto start = NightSharpPerf::Now();
            OnObjectDelete(static_cast<int>(args.Sender.NetworkId), args.Sender.Type);
            const double ms = NightSharpPerf::MsSince(start);
            NightSharpPerf::AddEventHandlerTiming("GameObjects::OnObjectDelete", 0, reinterpret_cast<const void*>(&OnObjectDelete), ms);
        }
        DispatchLifecycle("GameObjects::OnDelete", DeleteHandlers, args);
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
        }
        SDK::Events::AddOnCreateObject(&OnNativeObjectCreate);
        SDK::Events::AddOnDeleteObject(&OnNativeObjectDelete);
        SeedAllGameObjects();
    }

    inline void ReleaseNativeLifecycleHook() {
        bool wasHooked = false;
        {
            Lock lk(g_mutex);
            wasHooked = NativeLifecycleHooked;
            NativeLifecycleHooked = false;
            CreateHandlers.clear();
            DeleteHandlers.clear();
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
    detail::Initialized = true;
    SDK::GameObject::WarmPlayerTeamCache();
    // EnsoulSharp-style: keep the lists event-fresh. Delivery is deferred to the
    // update tick, so this is safe (see the threading-model note at the top).
    detail::EnsureNativeLifecycleHooked();
}

inline void Shutdown() {
    detail::ReleaseNativeLifecycleHook();
    detail::Clear();
    detail::Initialized = false;
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

// ------------------------------- accessors ----------------------------------
inline std::vector<AIHeroClient> Heroes() {
    return detail::Snapshot(detail::HeroesList);
}

inline std::vector<AIHeroClient> AllyHeroes() {
    return detail::Snapshot(detail::AllyHeroesList);
}

inline std::vector<AIHeroClient> EnemyHeroes() {
    return detail::Snapshot(detail::EnemyHeroesList);
}

inline std::vector<AIMinionClient> Minions() {
    return detail::Snapshot(detail::MinionsList);
}

inline std::vector<AIMinionClient> AllyMinions() {
    return detail::Snapshot(detail::AllyMinionsList);
}

inline std::vector<AIMinionClient> EnemyMinions() {
    return detail::Snapshot(detail::EnemyMinionsList);
}

inline std::vector<AIMinionClient> AllyLaneMinions() { return detail::Snapshot(detail::AllyLaneMinionsList); }
inline std::vector<AIMinionClient> EnemyLaneMinions() { return detail::Snapshot(detail::EnemyLaneMinionsList); }
inline std::vector<AIMinionClient> AllySpecialMinions() { return detail::Snapshot(detail::AllySpecialMinionsList); }
inline std::vector<AIMinionClient> EnemySpecialMinions() { return detail::Snapshot(detail::EnemySpecialMinionsList); }
inline std::vector<AIMinionClient> AllyIgnoredMinions() { return detail::Snapshot(detail::AllyIgnoredMinionsList); }
inline std::vector<AIMinionClient> EnemyIgnoredMinions() { return detail::Snapshot(detail::EnemyIgnoredMinionsList); }
inline std::vector<AIMinionClient> Wards() { return detail::Snapshot(detail::WardsList); }
inline std::vector<AIMinionClient> AllyWards() { return detail::Snapshot(detail::AllyWardsList); }
inline std::vector<AIMinionClient> EnemyWards() { return detail::Snapshot(detail::EnemyWardsList); }
inline std::vector<AIMinionClient> Jungle() { return detail::Snapshot(detail::JungleList); }
inline std::vector<AIMinionClient> JungleMinions() { return Jungle(); }
inline std::vector<AIMinionClient> JungleSmall() { return detail::Snapshot(detail::JungleSmallList); }
inline std::vector<AIMinionClient> JungleLarge() { return detail::Snapshot(detail::JungleLargeList); }
inline std::vector<AIMinionClient> JungleLegendary() { return detail::Snapshot(detail::JungleLegendaryList); }
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

// REMOVED: Turret/Inhibitor/Nexus class disabled by user request
// inline std::vector<AITurretClient> Turrets() {
//     return detail::Snapshot(detail::TurretsList);
// }
// inline std::vector<AITurretClient> AllyTurrets() {
//     return detail::Snapshot(detail::AllyTurretsList);
// }
// inline std::vector<AITurretClient> EnemyTurrets() {
//     return detail::Snapshot(detail::EnemyTurretsList);
// }
//
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
// // Compatibility aliases (previous API); same snapshot data.
// inline std::vector<AITurretClient> ScanTurrets() { return Turrets(); }
// inline std::vector<BarracksDampenerClient> ScanInhibitors() { return Inhibitors(); }
// inline std::vector<HQClient> ScanNexuses() { return Nexuses(); }

inline std::vector<ShopClient> Shops() { return detail::Snapshot(detail::ShopsList); }
inline std::vector<ShopClient> AllyShops() { return detail::Snapshot(detail::AllyShopsList); }
inline std::vector<ShopClient> EnemyShops() { return detail::Snapshot(detail::EnemyShopsList); }

inline std::vector<Obj_SpawnPoint> SpawnPoints() { return detail::Snapshot(detail::SpawnPointsList); }
inline std::vector<Obj_SpawnPoint> AllySpawnPoints() { return detail::Snapshot(detail::AllySpawnPointsList); }
inline std::vector<Obj_SpawnPoint> EnemySpawnPoints() { return detail::Snapshot(detail::EnemySpawnPointsList); }

inline std::vector<EffectEmitter> ParticleEmitters() { return detail::Snapshot(detail::ParticleEmittersList); }
inline std::vector<MissileClient> Missiles() {
    return detail::Snapshot(detail::MissilesList);
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
    } else if constexpr (std::is_same_v<T, ShopClient>) {
        return Shops();
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
    // REMOVED: Turret/Inhibitor/Nexus class disabled by user request
    // inline std::vector<AITurretClient> Turrets() { return SDK::GameObjects::Turrets(); }
    // inline std::vector<AITurretClient> AllyTurrets() { return SDK::GameObjects::AllyTurrets(); }
    // inline std::vector<AITurretClient> EnemyTurrets() { return SDK::GameObjects::EnemyTurrets(); }
    // inline std::vector<BarracksDampenerClient> EnemyInhibitors() { return SDK::GameObjects::EnemyInhibitors(); }
    // inline HQClient EnemyNexus() { return SDK::GameObjects::EnemyNexus(); }
    inline std::vector<MissileClient> Missiles() { return SDK::GameObjects::Missiles(); }
    inline std::vector<GameObject> AllObjects() { return SDK::GameObjects::AllGameObjects(); }
}

namespace SDK {

inline int AIBaseClient::CountAllyHeroesInRange(float range) const {
    int count = 0;
    const float rangeSqr = range * range;
    for (const auto& hero : GameObjects::AllyHeroes()) {
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
    for (const auto& hero : GameObjects::EnemyHeroes()) {
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
