#pragma once

#include "ObjectManager.h"
#include "../../Core/CoreEvents.h"

#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SDK::Events {
using ObjectEventArgs = ::Core::Events::ObjectEventArgs;
inline bool AddOnCreateObject(void(*handler)(const ObjectEventArgs&));
inline bool RemoveOnCreateObject(void(*handler)(const ObjectEventArgs&));
inline bool AddOnDeleteObject(void(*handler)(const ObjectEventArgs&));
inline bool RemoveOnDeleteObject(void(*handler)(const ObjectEventArgs&));
} // namespace SDK::Events

namespace SDK::GameObjects {

namespace detail {
    template <typename T>
    class IndexedObjectList {
    public:
        using iterator = typename std::vector<T>::iterator;
        using const_iterator = typename std::vector<T>::const_iterator;

        bool Add(const T& object) {
            const uintptr_t address = object.Address();
            return Add(address, object);
        }

        bool Add(uintptr_t address, const T& object) {
            if (address == 0 || indexByAddress_.find(address) != indexByAddress_.end()) {
                return false;
            }

            indexByAddress_.emplace(address, items_.size());
            addresses_.push_back(address);
            items_.push_back(object);
            return true;
        }

        bool Remove(uintptr_t address) {
            if (address == 0) {
                return false;
            }

            const auto it = indexByAddress_.find(address);
            if (it == indexByAddress_.end()) {
                return false;
            }

            const std::size_t index = it->second;
            const std::size_t last = items_.size() - 1;
            if (index != last) {
                items_[index] = std::move(items_[last]);
                const uintptr_t movedAddress = addresses_[last];
                addresses_[index] = movedAddress;
                indexByAddress_[movedAddress] = index;
            }

            items_.pop_back();
            addresses_.pop_back();
            indexByAddress_.erase(address);
            return true;
        }

        bool Contains(uintptr_t address) const {
            return address != 0 && indexByAddress_.find(address) != indexByAddress_.end();
        }

        void clear() {
            items_.clear();
            addresses_.clear();
            indexByAddress_.clear();
        }

        std::size_t size() const { return items_.size(); }
        bool empty() const { return items_.empty(); }
        iterator begin() { return items_.begin(); }
        iterator end() { return items_.end(); }
        const_iterator begin() const { return items_.begin(); }
        const_iterator end() const { return items_.end(); }

        const std::vector<T>& Items() const { return items_; }
        std::vector<T>& Items() { return items_; }

    private:
        std::vector<T> items_;
        std::vector<uintptr_t> addresses_;
        std::unordered_map<uintptr_t, std::size_t> indexByAddress_;
    };

    inline IndexedObjectList<GameObject> GameObjectsList;
    inline IndexedObjectList<AttackableUnit> AttackableUnitsList;
    inline IndexedObjectList<AIBaseClient> AllyList;
    inline IndexedObjectList<AIBaseClient> EnemyList;

    inline IndexedObjectList<AIHeroClient> HeroesList;
    inline IndexedObjectList<AIHeroClient> AllyHeroesList;
    inline IndexedObjectList<AIHeroClient> EnemyHeroesList;

    inline IndexedObjectList<AIMinionClient> MinionsList;
    inline IndexedObjectList<AIMinionClient> AllyMinionsList;
    inline IndexedObjectList<AIMinionClient> EnemyMinionsList;
    inline IndexedObjectList<AIMinionClient> AllyLaneMinionsList;
    inline IndexedObjectList<AIMinionClient> EnemyLaneMinionsList;
    inline IndexedObjectList<AIMinionClient> AllySpecialMinionsList;
    inline IndexedObjectList<AIMinionClient> EnemySpecialMinionsList;
    inline IndexedObjectList<AIMinionClient> AllyIgnoredMinionsList;
    inline IndexedObjectList<AIMinionClient> EnemyIgnoredMinionsList;
    inline IndexedObjectList<AIMinionClient> WardsList;
    inline IndexedObjectList<AIMinionClient> AllyWardsList;
    inline IndexedObjectList<AIMinionClient> EnemyWardsList;
    inline IndexedObjectList<AIMinionClient> JungleList;
    inline IndexedObjectList<AIMinionClient> JungleSmallList;
    inline IndexedObjectList<AIMinionClient> JungleLargeList;
    inline IndexedObjectList<AIMinionClient> JungleLegendaryList;
    inline IndexedObjectList<AIMinionClient> PlantsList;
    inline IndexedObjectList<AIMinionClient> ClonesList;
    inline IndexedObjectList<AIMinionClient> AllyClonesList;
    inline IndexedObjectList<AIMinionClient> EnemyClonesList;
    inline IndexedObjectList<AIMinionClient> PetsList;
    inline IndexedObjectList<AIMinionClient> AllyPetsList;
    inline IndexedObjectList<AIMinionClient> EnemyPetsList;

    inline IndexedObjectList<AITurretClient> TurretsList;
    inline IndexedObjectList<AITurretClient> AllyTurretsList;
    inline IndexedObjectList<AITurretClient> EnemyTurretsList;

    inline IndexedObjectList<BarracksDampenerClient> InhibitorsList;
    inline IndexedObjectList<BarracksDampenerClient> AllyInhibitorsList;
    inline IndexedObjectList<BarracksDampenerClient> EnemyInhibitorsList;
    inline IndexedObjectList<HQClient> NexusList;
    inline HQClient AllyNexusObject;
    inline HQClient EnemyNexusObject;
    inline uintptr_t AllyNexusAddress = 0;
    inline uintptr_t EnemyNexusAddress = 0;
    inline IndexedObjectList<ShopClient> ShopsList;
    inline IndexedObjectList<ShopClient> AllyShopsList;
    inline IndexedObjectList<ShopClient> EnemyShopsList;
    inline IndexedObjectList<Obj_SpawnPoint> SpawnPointsList;
    inline IndexedObjectList<Obj_SpawnPoint> AllySpawnPointsList;
    inline IndexedObjectList<Obj_SpawnPoint> EnemySpawnPointsList;
    inline IndexedObjectList<EffectEmitter> ParticleEmittersList;
    inline IndexedObjectList<MissileClient> MissilesList;

    inline AIHeroClient PlayerObject;
    inline bool Initialized = false;
    inline constexpr bool kObjectEventsEnabled = true;
    inline DWORD HeroesTick = 0;
    inline DWORD MinionsTick = 0;
    inline DWORD TurretsTick = 0;
    inline DWORD StructuresTick = 0;
    inline DWORD MissilesTick = 0;
    inline DWORD AggregateTick = 0;

    constexpr DWORD kHeroesCacheMs = 250;
    constexpr DWORD kMinionsCacheMs = 60;
    constexpr DWORD kTurretsCacheMs = 250;
    constexpr DWORD kStructuresCacheMs = 250;
    constexpr DWORD kMissilesCacheMs = 80;
    constexpr DWORD kAggregateCacheMs = 250;

    inline DWORD NowMs() {
        return ::GetTickCount();
    }

    inline bool Expired(DWORD tick, DWORD interval) {
        return tick == 0 || NowMs() - tick >= interval;
    }

    inline void InvalidateAggregate() {
        AggregateTick = 0;
    }

    inline std::string ToLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    inline std::string ReadRuntimeName(uintptr_t address, bool characterName) {
        char buf[96] = {};
        const bool ok = characterName
            ? ::Core::Objects::ReadCharacterName(address, buf, static_cast<int>(sizeof(buf)))
            : ::Core::Objects::ReadName(address, buf, static_cast<int>(sizeof(buf)));
        return ok ? std::string(buf) : std::string();
    }

    inline std::string BestName(uintptr_t address) {
        std::string name = ReadRuntimeName(address, true);
        if (name.empty()) {
            name = ReadRuntimeName(address, false);
        }
        return ToLower(std::move(name));
    }

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
        const auto player = SDK::ObjectManager::Player();
        return player.IsValid() ? TeamValue(player) : 0;
    }

    template <typename T>
    inline bool ContainsAddress(const IndexedObjectList<T>& list, uintptr_t address) {
        return list.Contains(address);
    }

    template <typename T>
    inline bool AddUnique(IndexedObjectList<T>& list, const T& object) {
        return list.Add(object);
    }

    template <typename T>
    inline bool AddKnownAddress(IndexedObjectList<T>& list, uintptr_t address, const T& object) {
        return list.Add(address, object);
    }

    template <typename T>
    inline bool RemoveByAddress(IndexedObjectList<T>& list, uintptr_t address) {
        return list.Remove(address);
    }

    template <typename T>
    inline bool RemoveEnemyThenAlly(
        IndexedObjectList<T>& enemyList,
        IndexedObjectList<T>& allyList,
        uintptr_t address) {
        if (RemoveByAddress(enemyList, address)) {
            return true;
        }
        return RemoveByAddress(allyList, address);
    }

    inline bool ContainsAny(const std::string& value, std::initializer_list<const char*> needles) {
        for (const auto* needle : needles) {
            if (!needle) {
                continue;
            }
            if (value.find(ToLower(needle)) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    inline ::Core::Objects::ObjectHandle MakeEventHandle(
        const SDK::Events::ObjectEventArgs& args,
        ::Core::Objects::ObjectType type) {
        ::Core::Objects::ObjectHandle handle{};
        handle.address = args.Sender.Ptr;
        handle.index = args.Sender.Index;
        handle.networkId = args.Sender.NetworkId;
        handle.type = type;
        return handle;
    }

    inline ::Core::Objects::ObjectType ResolveCreateType(
        const SDK::Events::ObjectEventArgs& args) {
        if (args.Sender.Type != ::Core::Objects::ObjectType::Unknown) {
            return args.Sender.Type;
        }
        return ::Core::ObjectManager::InferType(args.Sender.Ptr);
    }

    inline std::uint32_t ResolveCreateTeam(
        const SDK::Events::ObjectEventArgs& args,
        uintptr_t address) {
        if (args.Sender.Team != 0) {
            return args.Sender.Team;
        }
        return ::Core::Objects::ReadTeamValue(address);
    }

    inline std::string ResolveCreateName(
        const SDK::Events::ObjectEventArgs& args,
        uintptr_t address) {
        if (args.Sender.CharacterName[0]) {
            return ToLower(args.Sender.CharacterName);
        }
        if (args.Sender.Name[0]) {
            return ToLower(args.Sender.Name);
        }
        return BestName(address);
    }

    inline bool EqualsAny(const std::string& value, std::initializer_list<const char*> needles) {
        for (const auto* needle : needles) {
            if (needle && value == ToLower(needle)) {
                return true;
            }
        }
        return false;
    }

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
            "sru_baron", "sru_dragon", "sru_riftherald", "voidgrub",
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

    inline bool IsWardObject(const AIMinionClient& minion) {
        if (!minion.IsValid() || minion.IsDead()) {
            return false;
        }
        const std::string name = BestName(minion.Address());
        return HasFlag(minion.GetMinionType(), MinionTypes::Ward) ||
               ContainsAny(name, {
                   "ward", "jammerdevice", "trinket", "sightward", "visionward"
               });
    }

    inline bool IsPlantObject(const AIMinionClient& minion) {
        if (!minion.IsValid() || minion.IsDead()) {
            return false;
        }
        const std::string name = BestName(minion.Address());
        const float maxHp = minion.MaxHealth();
        return minion.GetJungleType() == JungleType::Plant ||
               IsKnownJunglePlantName(name) ||
               (TeamValue(minion) == 300 && maxHp > 0.0f && maxHp <= 6.0f);
    }

    inline bool IsJungleObject(const AIMinionClient& minion) {
        if (!minion.IsValid() || minion.IsDead()) {
            return false;
        }
        if (TeamValue(minion) != 300 || IsPlantObject(minion)) {
            return false;
        }
        const float maxHp = minion.MaxHealth();
        if (maxHp <= 6.0f) {
            return false;
        }
        const std::string name = BestName(minion.Address());
        return minion.IsJungle() || IsKnownJungleMonsterName(name);
    }

    inline bool IsLaneMinionObject(const AIMinionClient& minion) {
        if (!minion.IsValid() || minion.IsDead()) {
            return false;
        }
        if (TeamValue(minion) == 300 || IsPlantObject(minion)) {
            return false;
        }
        const float maxHp = minion.MaxHealth();
        if (maxHp <= 0.0f || maxHp >= 10000.0f) {
            return false;
        }
        const std::string name = BestName(minion.Address());
        if (IsKnownJungleMonsterName(name)) {
            return false;
        }
        return minion.IsMinion() || IsLaneMinionName(name);
    }

    inline bool IsCloneObject(const AIMinionClient& minion) {
        if (!minion.IsValid() || minion.IsDead()) {
            return false;
        }
        return minion.IsClone() || EqualsAny(BestName(minion.Address()), {
            "leblanc", "monkeyking", "neeko", "shaco"
        });
    }

    inline bool IsPetObject(const AIMinionClient& minion) {
        return minion.IsValid() && !minion.IsDead() &&
               !IsPlantObject(minion) && minion.IsPet();
    }

    inline bool IsSpecialMinionObject(const AIMinionClient& minion) {
        if (!minion.IsValid() || minion.IsDead()) {
            return false;
        }
        return EqualsAny(BestName(minion.Address()), {
            "annietibbers", "elisespiderling", "heimertyellow",
            "heimertblue", "ivernminion", "malzaharvoidling",
            "shacobox", "teemomushroom", "yorickghoulmelee",
            "yorickbigghoul", "zyrathornplant", "zyragraspingplant"
        });
    }

    inline bool IsIgnoredMinionObject(const AIMinionClient& minion) {
        return minion.IsValid() && EqualsAny(BestName(minion.Address()), {
            "jarvanivstandard"
        });
    }

    inline void ClearAggregateOnly() {
        GameObjectsList.clear();
        AttackableUnitsList.clear();
        AllyList.clear();
        EnemyList.clear();
    }

    inline void RefreshHeroes() {
        if (!Expired(HeroesTick, kHeroesCacheMs)) {
            return;
        }
        HeroesTick = NowMs();
        InvalidateAggregate();

        PlayerObject = SDK::ObjectManager::Player();
        SDK::GameObject::WarmPlayerTeamCache();

        HeroesList.clear();
        AllyHeroesList.clear();
        EnemyHeroesList.clear();

        const int myTeam = PlayerTeam();
        auto heroes = SDK::ObjectManager::Get<AIHeroClient>();
        for (auto& hero : heroes) {
            PopulateStatic(hero);
            if (!hero.IsValid()) {
                continue;
            }
            AddUnique(HeroesList, hero);
            const int team = TeamValue(hero);
            if (myTeam != 0 && team == myTeam) {
                AddUnique(AllyHeroesList, hero);
            } else if (team != 0 && team != 300) {
                AddUnique(EnemyHeroesList, hero);
            }
        }
    }

    inline void RefreshMinions() {
        if (!Expired(MinionsTick, kMinionsCacheMs)) {
            return;
        }
        MinionsTick = NowMs();
        InvalidateAggregate();

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

        const int myTeam = PlayerTeam();
        auto minions = SDK::ObjectManager::Get<AIMinionClient>();
        for (auto& minion : minions) {
            PopulateStatic(minion);
            if (!minion.IsValid() || minion.IsDead()) {
                continue;
            }

            const int team = TeamValue(minion);
            const bool ally = myTeam != 0 && team == myTeam;
            const bool enemy = team != 0 && team != 300 && !ally;

            if (IsWardObject(minion)) {
                AddUnique(WardsList, minion);
                if (enemy) {
                    AddUnique(EnemyWardsList, minion);
                } else {
                    AddUnique(AllyWardsList, minion);
                }
                continue;
            }

            if (IsPlantObject(minion)) {
                AddUnique(PlantsList, minion);
                continue;
            }

            if (IsJungleObject(minion)) {
                AddUnique(JungleList, minion);
                const JungleType jungleType = minion.GetJungleType();
                if (jungleType == JungleType::Small) {
                    AddUnique(JungleSmallList, minion);
                } else if (jungleType == JungleType::Legendary || jungleType == JungleType::Epic) {
                    AddUnique(JungleLegendaryList, minion);
                } else {
                    AddUnique(JungleLargeList, minion);
                }
                continue;
            }

            if (IsCloneObject(minion)) {
                AddUnique(ClonesList, minion);
                if (enemy) {
                    AddUnique(EnemyClonesList, minion);
                } else {
                    AddUnique(AllyClonesList, minion);
                }
                continue;
            }

            if (IsPetObject(minion)) {
                AddUnique(PetsList, minion);
                if (enemy) {
                    AddUnique(EnemyPetsList, minion);
                } else {
                    AddUnique(AllyPetsList, minion);
                }
                continue;
            }

            if (IsSpecialMinionObject(minion)) {
                if (enemy) {
                    AddUnique(EnemySpecialMinionsList, minion);
                } else {
                    AddUnique(AllySpecialMinionsList, minion);
                }
                continue;
            }

            if (IsIgnoredMinionObject(minion)) {
                if (enemy) {
                    AddUnique(EnemyIgnoredMinionsList, minion);
                } else {
                    AddUnique(AllyIgnoredMinionsList, minion);
                }
                continue;
            }

            if (!IsLaneMinionObject(minion)) {
                continue;
            }

            AddUnique(MinionsList, minion);
            if (enemy) {
                AddUnique(EnemyMinionsList, minion);
                AddUnique(EnemyLaneMinionsList, minion);
            } else {
                AddUnique(AllyMinionsList, minion);
                AddUnique(AllyLaneMinionsList, minion);
            }
        }
    }

    inline void RefreshTurrets() {
        if (!Expired(TurretsTick, kTurretsCacheMs)) {
            return;
        }
        TurretsTick = NowMs();
        InvalidateAggregate();

        TurretsList.clear();
        AllyTurretsList.clear();
        EnemyTurretsList.clear();

        const int myTeam = PlayerTeam();
        auto turrets = SDK::ObjectManager::Get<AITurretClient>();
        for (auto& turret : turrets) {
            PopulateStatic(turret);
            if (!turret.IsValid()) {
                continue;
            }
            AddUnique(TurretsList, turret);
            const int team = TeamValue(turret);
            if (myTeam != 0 && team == myTeam) {
                AddUnique(AllyTurretsList, turret);
            } else if (team != 0 && team != 300) {
                AddUnique(EnemyTurretsList, turret);
            }
        }
    }

    inline void RefreshStructures() {
        if (!Expired(StructuresTick, kStructuresCacheMs)) {
            return;
        }
        StructuresTick = NowMs();
        InvalidateAggregate();

        InhibitorsList.clear();
        AllyInhibitorsList.clear();
        EnemyInhibitorsList.clear();
        NexusList.clear();
        AllyNexusObject = {};
        EnemyNexusObject = {};
        AllyNexusAddress = 0;
        EnemyNexusAddress = 0;
        ShopsList.clear();
        AllyShopsList.clear();
        EnemyShopsList.clear();
        SpawnPointsList.clear();
        AllySpawnPointsList.clear();
        EnemySpawnPointsList.clear();
        ParticleEmittersList.clear();

        const int myTeam = PlayerTeam();

        auto addTeamObject = [&](auto& allList, auto& allyList, auto& enemyList, auto& object) {
            PopulateStatic(object);
            if (!object.IsValid()) {
                return;
            }
            AddUnique(allList, object);
            const int team = TeamValue(object);
            if (myTeam != 0 && team == myTeam) {
                AddUnique(allyList, object);
            } else if (team != 0 && team != 300) {
                AddUnique(enemyList, object);
            }
        };

        auto inhibitors = SDK::ObjectManager::Get<BarracksDampenerClient>();
        for (auto& inhibitor : inhibitors) {
            addTeamObject(InhibitorsList, AllyInhibitorsList, EnemyInhibitorsList, inhibitor);
        }

        auto nexuses = SDK::ObjectManager::Get<HQClient>();
        for (auto& nexus : nexuses) {
            PopulateStatic(nexus);
            if (!nexus.IsValid()) {
                continue;
            }
            AddUnique(NexusList, nexus);
            const int team = TeamValue(nexus);
            if (myTeam != 0 && team == myTeam) {
                AllyNexusObject = nexus;
                AllyNexusAddress = nexus.Address();
            } else if (team != 0 && team != 300) {
                EnemyNexusObject = nexus;
                EnemyNexusAddress = nexus.Address();
            }
        }

        auto shops = SDK::ObjectManager::Get<ShopClient>();
        for (auto& shop : shops) {
            addTeamObject(ShopsList, AllyShopsList, EnemyShopsList, shop);
        }

        auto spawns = SDK::ObjectManager::Get<Obj_SpawnPoint>();
        for (auto& spawn : spawns) {
            addTeamObject(SpawnPointsList, AllySpawnPointsList, EnemySpawnPointsList, spawn);
        }

        auto effects = SDK::ObjectManager::Get<EffectEmitter>();
        for (auto& effect : effects) {
            PopulateStatic(effect);
            AddUnique(ParticleEmittersList, effect);
        }
    }

    inline void RefreshMissiles() {
        if (!Expired(MissilesTick, kMissilesCacheMs)) {
            return;
        }
        MissilesTick = NowMs();
        InvalidateAggregate();

        MissilesList.clear();
        auto missiles = SDK::ObjectManager::Get<MissileClient>();
        for (auto& missile : missiles) {
            PopulateStatic(missile);
            AddUnique(MissilesList, missile);
        }
    }

    inline void RefreshAggregate() {
        if (!Expired(AggregateTick, kAggregateCacheMs)) {
            return;
        }

        RefreshHeroes();
        RefreshMinions();
        RefreshTurrets();
        RefreshStructures();

        AggregateTick = NowMs();
        ClearAggregateOnly();

        auto all = SDK::ObjectManager::Get<GameObject>();
        for (auto& object : all) {
            PopulateStatic(object);
            if (!object.IsValid()) {
                continue;
            }
            AddUnique(GameObjectsList, object);
            if (::Core::Objects::IsAttackable(object.Type())) {
                AddUnique(AttackableUnitsList, AttackableUnit(object.Handle()));
            }
        }

        auto appendBase = [](auto& out, const auto& list) {
            for (const auto& object : list) {
                AddUnique(out, AIBaseClient(object.Handle()));
            }
        };
        appendBase(AllyList, AllyHeroesList);
        appendBase(AllyList, AllyMinionsList);
        appendBase(AllyList, AllyTurretsList);
        appendBase(AllyList, AllyInhibitorsList);

        appendBase(EnemyList, EnemyHeroesList);
        appendBase(EnemyList, EnemyMinionsList);
        appendBase(EnemyList, EnemyTurretsList);
        appendBase(EnemyList, EnemyInhibitorsList);
    }

    inline void OnCreate(const SDK::Events::ObjectEventArgs& args) {
        const uintptr_t address = args.Sender.Ptr;
        if (!Globals::IsValidPtr(address)) {
            return;
        }

        const auto type = ResolveCreateType(args);
        auto handle = MakeEventHandle(args, type);
        if (handle.index == 0) {
            handle.index = ::Core::Objects::ReadIndex(address);
        }
        if (!handle.HasIdentity()) {
            handle.networkId = ::Core::Objects::ReadNetworkId(address);
        }
        if (!handle.HasIdentity()) {
            return;
        }

        PopulateStatic(address, handle.index, type);

        const GameObject sender(handle);
        AddKnownAddress(GameObjectsList, address, sender);

        const bool attackable = ::Core::Objects::IsAttackable(type);
        if (attackable) {
            AddKnownAddress(AttackableUnitsList, address, AttackableUnit(handle));
        }

        const std::uint32_t team = ResolveCreateTeam(args, address);
        const int myTeam = PlayerTeam();
        const bool ally = myTeam != 0 && static_cast<int>(team) == myTeam;
        const bool enemy = team != 0 && team != 300 && !ally;

        auto addBase = [&](auto& list) {
            AddKnownAddress(list, address, AIBaseClient(handle));
        };

        auto addTeamObject = [&](auto& allList, auto& allyList, auto& enemyList, const auto& object) {
            AddKnownAddress(allList, address, object);
            if (enemy) {
                AddKnownAddress(enemyList, address, object);
                addBase(EnemyList);
            } else if (ally) {
                AddKnownAddress(allyList, address, object);
                addBase(AllyList);
            }
        };

        switch (type) {
        case ::Core::Objects::ObjectType::AIHeroClient: {
            const AIHeroClient hero(handle);
            addTeamObject(HeroesList, AllyHeroesList, EnemyHeroesList, hero);
            return;
        }

        case ::Core::Objects::ObjectType::AIMinionClient: {
            const AIMinionClient minion(handle);
            const std::string name = ResolveCreateName(args, address);
            const MinionTypes minionType = minion.GetMinionType();
            const JungleType jungleType = minion.GetJungleType();
            const float maxHp = Globals::Read<float>(address + Offset::AttackableUnit::MaxHP);

            const bool isWard =
                HasFlag(minionType, MinionTypes::Ward) ||
                ContainsAny(name, { "ward", "jammerdevice", "trinket", "sightward", "visionward" });
            const bool isPlant =
                jungleType == JungleType::Plant ||
                IsKnownJunglePlantName(name) ||
                (team == 300 && maxHp > 0.0f && maxHp <= 6.0f);
            const bool isKnownJungle = IsKnownJungleMonsterName(name);
            const bool isJungle =
                team == 300 &&
                !isPlant &&
                maxHp > 6.0f &&
                ((jungleType != JungleType::Unknown && jungleType != JungleType::Plant) || isKnownJungle);
            const bool isLaneMinion =
                team != 300 &&
                !isPlant &&
                maxHp > 0.0f &&
                maxHp < 10000.0f &&
                !isKnownJungle &&
                (HasFlag(minionType, MinionTypes::Melee) ||
                 HasFlag(minionType, MinionTypes::Ranged) ||
                 IsLaneMinionName(name));
            const bool isClone =
                args.Sender.IsClone ||
                EqualsAny(name, { "leblanc", "monkeyking", "neeko", "shaco" });
            const bool isPet =
                args.Sender.IsPet ||
                (!isPlant &&
                 !isClone &&
                 minion.GetMinionClass() == ::Core::Objects::MinionClass::Pet);
            const bool isSpecial = EqualsAny(name, {
                "annietibbers", "elisespiderling", "heimertyellow",
                "heimertblue", "ivernminion", "malzaharvoidling",
                "shacobox", "teemomushroom", "yorickghoulmelee",
                "yorickbigghoul", "zyrathornplant", "zyragraspingplant"
            });
            const bool isIgnored = EqualsAny(name, { "jarvanivstandard" });

            if (team != 300) {
                if (isWard) {
                    AddKnownAddress(WardsList, address, minion);
                    if (enemy) {
                        AddKnownAddress(EnemyWardsList, address, minion);
                    } else if (ally) {
                        AddKnownAddress(AllyWardsList, address, minion);
                    }
                } else if (isLaneMinion) {
                    AddKnownAddress(MinionsList, address, minion);
                    if (enemy) {
                        AddKnownAddress(EnemyMinionsList, address, minion);
                        AddKnownAddress(EnemyLaneMinionsList, address, minion);
                        addBase(EnemyList);
                    } else if (ally) {
                        AddKnownAddress(AllyMinionsList, address, minion);
                        AddKnownAddress(AllyLaneMinionsList, address, minion);
                        addBase(AllyList);
                    }
                } else if (isSpecial) {
                    if (enemy) {
                        AddKnownAddress(EnemySpecialMinionsList, address, minion);
                    } else if (ally) {
                        AddKnownAddress(AllySpecialMinionsList, address, minion);
                    }
                } else if (isIgnored) {
                    if (enemy) {
                        AddKnownAddress(EnemyIgnoredMinionsList, address, minion);
                    } else if (ally) {
                        AddKnownAddress(AllyIgnoredMinionsList, address, minion);
                    }
                } else if (isClone) {
                    AddKnownAddress(ClonesList, address, minion);
                    if (enemy) {
                        AddKnownAddress(EnemyClonesList, address, minion);
                    } else if (ally) {
                        AddKnownAddress(AllyClonesList, address, minion);
                    }
                } else if (isPet) {
                    AddKnownAddress(PetsList, address, minion);
                    if (enemy) {
                        AddKnownAddress(EnemyPetsList, address, minion);
                    } else if (ally) {
                        AddKnownAddress(AllyPetsList, address, minion);
                    }
                }
            } else if (!EqualsAny(name, { "wardcorpse" })) {
                if (isPlant) {
                    AddKnownAddress(PlantsList, address, minion);
                } else if (isJungle) {
                    AddKnownAddress(JungleList, address, minion);
                    if (jungleType == JungleType::Small) {
                        AddKnownAddress(JungleSmallList, address, minion);
                    } else if (jungleType == JungleType::Legendary || jungleType == JungleType::Epic) {
                        AddKnownAddress(JungleLegendaryList, address, minion);
                    } else {
                        AddKnownAddress(JungleLargeList, address, minion);
                    }
                }
            }
            return;
        }

        case ::Core::Objects::ObjectType::EffectEmitter:
            AddKnownAddress(ParticleEmittersList, address, EffectEmitter(handle));
            return;

        case ::Core::Objects::ObjectType::AITurretClient:
            addTeamObject(TurretsList, AllyTurretsList, EnemyTurretsList, AITurretClient(handle));
            return;

        case ::Core::Objects::ObjectType::ShopClient:
            AddKnownAddress(ShopsList, address, ShopClient(handle));
            if (enemy) {
                AddKnownAddress(EnemyShopsList, address, ShopClient(handle));
            } else if (ally) {
                AddKnownAddress(AllyShopsList, address, ShopClient(handle));
            }
            return;

        case ::Core::Objects::ObjectType::Obj_SpawnPoint:
            AddKnownAddress(SpawnPointsList, address, Obj_SpawnPoint(handle));
            if (enemy) {
                AddKnownAddress(EnemySpawnPointsList, address, Obj_SpawnPoint(handle));
            } else if (ally) {
                AddKnownAddress(AllySpawnPointsList, address, Obj_SpawnPoint(handle));
            }
            return;

        case ::Core::Objects::ObjectType::BarracksDampenerClient:
            addTeamObject(InhibitorsList, AllyInhibitorsList, EnemyInhibitorsList, BarracksDampenerClient(handle));
            return;

        case ::Core::Objects::ObjectType::HQClient: {
            const HQClient nexus(handle);
            AddKnownAddress(NexusList, address, nexus);
            if (enemy) {
                EnemyNexusObject = nexus;
                EnemyNexusAddress = address;
            } else if (ally) {
                AllyNexusObject = nexus;
                AllyNexusAddress = address;
            }
            return;
        }

        default:
            return;
        }
    }

    inline void OnDelete(const SDK::Events::ObjectEventArgs& args) {
        const uintptr_t address = args.Sender.Ptr;
        if (address == 0) {
            return;
        }

        RemoveByAddress(GameObjectsList, address);
        RemoveByAddress(AttackableUnitsList, address);

        RemoveByAddress(HeroesList, address);
        RemoveEnemyThenAlly(EnemyHeroesList, AllyHeroesList, address);

        RemoveByAddress(MinionsList, address);
        RemoveEnemyThenAlly(EnemyMinionsList, AllyMinionsList, address);
        RemoveEnemyThenAlly(EnemyLaneMinionsList, AllyLaneMinionsList, address);
        RemoveEnemyThenAlly(EnemySpecialMinionsList, AllySpecialMinionsList, address);
        RemoveEnemyThenAlly(EnemyIgnoredMinionsList, AllyIgnoredMinionsList, address);

        RemoveByAddress(WardsList, address);
        RemoveEnemyThenAlly(EnemyWardsList, AllyWardsList, address);

        RemoveByAddress(JungleList, address);
        RemoveByAddress(JungleSmallList, address);
        RemoveByAddress(JungleLargeList, address);
        RemoveByAddress(JungleLegendaryList, address);
        RemoveByAddress(PlantsList, address);

        RemoveByAddress(ClonesList, address);
        RemoveEnemyThenAlly(EnemyClonesList, AllyClonesList, address);

        RemoveByAddress(PetsList, address);
        RemoveEnemyThenAlly(EnemyPetsList, AllyPetsList, address);

        RemoveByAddress(TurretsList, address);
        RemoveEnemyThenAlly(EnemyTurretsList, AllyTurretsList, address);

        RemoveByAddress(InhibitorsList, address);
        RemoveEnemyThenAlly(EnemyInhibitorsList, AllyInhibitorsList, address);

        RemoveByAddress(ShopsList, address);
        RemoveEnemyThenAlly(EnemyShopsList, AllyShopsList, address);

        RemoveByAddress(SpawnPointsList, address);
        RemoveEnemyThenAlly(EnemySpawnPointsList, AllySpawnPointsList, address);

        RemoveByAddress(ParticleEmittersList, address);
        RemoveByAddress(MissilesList, address);

        RemoveByAddress(NexusList, address);
        if (EnemyNexusAddress == address) {
            EnemyNexusObject = {};
            EnemyNexusAddress = 0;
        } else if (AllyNexusAddress == address) {
            AllyNexusObject = {};
            AllyNexusAddress = 0;
        }

        RemoveEnemyThenAlly(EnemyList, AllyList, address);
    }

    inline void Clear() {
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
        TurretsList.clear();
        AllyTurretsList.clear();
        EnemyTurretsList.clear();
        InhibitorsList.clear();
        AllyInhibitorsList.clear();
        EnemyInhibitorsList.clear();
        NexusList.clear();
        AllyNexusObject = {};
        EnemyNexusObject = {};
        AllyNexusAddress = 0;
        EnemyNexusAddress = 0;
        ShopsList.clear();
        AllyShopsList.clear();
        EnemyShopsList.clear();
        SpawnPointsList.clear();
        AllySpawnPointsList.clear();
        EnemySpawnPointsList.clear();
        ParticleEmittersList.clear();
        MissilesList.clear();
        PlayerObject = {};
        HeroesTick = MinionsTick = TurretsTick = StructuresTick = MissilesTick = AggregateTick = 0;
    }

} // namespace detail

inline void Initialize() {
    if (detail::Initialized) {
        return;
    }
    detail::Initialized = true;
    SDK::GameObject::WarmPlayerTeamCache();
    if constexpr (detail::kObjectEventsEnabled) {
        SDK::Events::AddOnCreateObject(&detail::OnCreate);
        SDK::Events::AddOnDeleteObject(&detail::OnDelete);
    }
}

inline void Shutdown() {
    if constexpr (detail::kObjectEventsEnabled) {
        SDK::Events::RemoveOnCreateObject(&detail::OnCreate);
        SDK::Events::RemoveOnDeleteObject(&detail::OnDelete);
    }
    detail::Clear();
    detail::Initialized = false;
}

inline void EnsureInitialized() {
    Initialize();
    detail::PlayerObject = SDK::ObjectManager::Player();
}

inline AIHeroClient Player() {
    EnsureInitialized();
    detail::PlayerObject = SDK::ObjectManager::Player();
    return detail::PlayerObject;
}

inline const std::vector<AIHeroClient>& Heroes() { EnsureInitialized(); detail::RefreshHeroes(); return detail::HeroesList.Items(); }
inline const std::vector<AIHeroClient>& AllyHeroes() { EnsureInitialized(); detail::RefreshHeroes(); return detail::AllyHeroesList.Items(); }
inline const std::vector<AIHeroClient>& EnemyHeroes() { EnsureInitialized(); detail::RefreshHeroes(); return detail::EnemyHeroesList.Items(); }

inline const std::vector<AIMinionClient>& Minions() { EnsureInitialized(); detail::RefreshMinions(); return detail::MinionsList.Items(); }
inline const std::vector<AIMinionClient>& AllyMinions() { EnsureInitialized(); detail::RefreshMinions(); return detail::AllyMinionsList.Items(); }
inline const std::vector<AIMinionClient>& EnemyMinions() { EnsureInitialized(); detail::RefreshMinions(); return detail::EnemyMinionsList.Items(); }
inline const std::vector<AIMinionClient>& AllyLaneMinions() { EnsureInitialized(); detail::RefreshMinions(); return detail::AllyLaneMinionsList.Items(); }
inline const std::vector<AIMinionClient>& EnemyLaneMinions() { EnsureInitialized(); detail::RefreshMinions(); return detail::EnemyLaneMinionsList.Items(); }
inline const std::vector<AIMinionClient>& AllySpecialMinions() { EnsureInitialized(); detail::RefreshMinions(); return detail::AllySpecialMinionsList.Items(); }
inline const std::vector<AIMinionClient>& EnemySpecialMinions() { EnsureInitialized(); detail::RefreshMinions(); return detail::EnemySpecialMinionsList.Items(); }
inline const std::vector<AIMinionClient>& AllyIgnoredMinions() { EnsureInitialized(); detail::RefreshMinions(); return detail::AllyIgnoredMinionsList.Items(); }
inline const std::vector<AIMinionClient>& EnemyIgnoredMinions() { EnsureInitialized(); detail::RefreshMinions(); return detail::EnemyIgnoredMinionsList.Items(); }
inline const std::vector<AIMinionClient>& Wards() { EnsureInitialized(); detail::RefreshMinions(); return detail::WardsList.Items(); }
inline const std::vector<AIMinionClient>& AllyWards() { EnsureInitialized(); detail::RefreshMinions(); return detail::AllyWardsList.Items(); }
inline const std::vector<AIMinionClient>& EnemyWards() { EnsureInitialized(); detail::RefreshMinions(); return detail::EnemyWardsList.Items(); }
inline const std::vector<AIMinionClient>& Jungle() { EnsureInitialized(); detail::RefreshMinions(); return detail::JungleList.Items(); }
inline const std::vector<AIMinionClient>& JungleMinions() { return Jungle(); }
inline const std::vector<AIMinionClient>& JungleSmall() { EnsureInitialized(); detail::RefreshMinions(); return detail::JungleSmallList.Items(); }
inline const std::vector<AIMinionClient>& JungleLarge() { EnsureInitialized(); detail::RefreshMinions(); return detail::JungleLargeList.Items(); }
inline const std::vector<AIMinionClient>& JungleLegendary() { EnsureInitialized(); detail::RefreshMinions(); return detail::JungleLegendaryList.Items(); }
inline const std::vector<AIMinionClient>& SmallJungle() { return JungleSmall(); }
inline const std::vector<AIMinionClient>& LargeJungle() { return JungleLarge(); }
inline const std::vector<AIMinionClient>& EpicJungle() { return JungleLegendary(); }
inline const std::vector<AIMinionClient>& Plants() { EnsureInitialized(); detail::RefreshMinions(); return detail::PlantsList.Items(); }
inline const std::vector<AIMinionClient>& JunglePlants() { return Plants(); }
inline const std::vector<AIMinionClient>& Clones() { EnsureInitialized(); detail::RefreshMinions(); return detail::ClonesList.Items(); }
inline const std::vector<AIMinionClient>& AllyClones() { EnsureInitialized(); detail::RefreshMinions(); return detail::AllyClonesList.Items(); }
inline const std::vector<AIMinionClient>& EnemyClones() { EnsureInitialized(); detail::RefreshMinions(); return detail::EnemyClonesList.Items(); }
inline const std::vector<AIMinionClient>& Pets() { EnsureInitialized(); detail::RefreshMinions(); return detail::PetsList.Items(); }
inline const std::vector<AIMinionClient>& AllyPets() { EnsureInitialized(); detail::RefreshMinions(); return detail::AllyPetsList.Items(); }
inline const std::vector<AIMinionClient>& EnemyPets() { EnsureInitialized(); detail::RefreshMinions(); return detail::EnemyPetsList.Items(); }

inline const std::vector<AITurretClient>& Turrets() { EnsureInitialized(); detail::RefreshTurrets(); return detail::TurretsList.Items(); }
inline const std::vector<AITurretClient>& AllyTurrets() { EnsureInitialized(); detail::RefreshTurrets(); return detail::AllyTurretsList.Items(); }
inline const std::vector<AITurretClient>& EnemyTurrets() { EnsureInitialized(); detail::RefreshTurrets(); return detail::EnemyTurretsList.Items(); }

inline const std::vector<BarracksDampenerClient>& Inhibitors() { EnsureInitialized(); detail::RefreshStructures(); return detail::InhibitorsList.Items(); }
inline const std::vector<BarracksDampenerClient>& AllyInhibitors() { EnsureInitialized(); detail::RefreshStructures(); return detail::AllyInhibitorsList.Items(); }
inline const std::vector<BarracksDampenerClient>& EnemyInhibitors() { EnsureInitialized(); detail::RefreshStructures(); return detail::EnemyInhibitorsList.Items(); }
inline const std::vector<HQClient>& Nexuses() { EnsureInitialized(); detail::RefreshStructures(); return detail::NexusList.Items(); }
inline HQClient AllyNexus() { EnsureInitialized(); detail::RefreshStructures(); return detail::AllyNexusObject; }
inline HQClient EnemyNexus() { EnsureInitialized(); detail::RefreshStructures(); return detail::EnemyNexusObject; }
inline const std::vector<ShopClient>& Shops() { EnsureInitialized(); detail::RefreshStructures(); return detail::ShopsList.Items(); }
inline const std::vector<ShopClient>& AllyShops() { EnsureInitialized(); detail::RefreshStructures(); return detail::AllyShopsList.Items(); }
inline const std::vector<ShopClient>& EnemyShops() { EnsureInitialized(); detail::RefreshStructures(); return detail::EnemyShopsList.Items(); }
inline const std::vector<Obj_SpawnPoint>& SpawnPoints() { EnsureInitialized(); detail::RefreshStructures(); return detail::SpawnPointsList.Items(); }
inline const std::vector<Obj_SpawnPoint>& AllySpawnPoints() { EnsureInitialized(); detail::RefreshStructures(); return detail::AllySpawnPointsList.Items(); }
inline const std::vector<Obj_SpawnPoint>& EnemySpawnPoints() { EnsureInitialized(); detail::RefreshStructures(); return detail::EnemySpawnPointsList.Items(); }
inline const std::vector<EffectEmitter>& ParticleEmitters() { EnsureInitialized(); detail::RefreshStructures(); return detail::ParticleEmittersList.Items(); }
inline const std::vector<MissileClient>& Missiles() { EnsureInitialized(); detail::RefreshMissiles(); return detail::MissilesList.Items(); }

inline const std::vector<GameObject>& AllGameObjects() { EnsureInitialized(); detail::RefreshAggregate(); return detail::GameObjectsList.Items(); }
inline const std::vector<AttackableUnit>& AttackableUnits() { EnsureInitialized(); detail::RefreshAggregate(); return detail::AttackableUnitsList.Items(); }
inline const std::vector<AIBaseClient>& Ally() { EnsureInitialized(); detail::RefreshAggregate(); return detail::AllyList.Items(); }
inline const std::vector<AIBaseClient>& Enemy() { EnsureInitialized(); detail::RefreshAggregate(); return detail::EnemyList.Items(); }

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
        std::vector<T> result;
        const auto& ally = Ally();
        const auto& enemy = Enemy();
        result.reserve(ally.size() + enemy.size());
        result.insert(result.end(), ally.begin(), ally.end());
        result.insert(result.end(), enemy.begin(), enemy.end());
        return result;
    } else if constexpr (std::is_same_v<T, AIHeroClient>) {
        return Heroes();
    } else if constexpr (std::is_same_v<T, AIMinionClient>) {
        std::vector<T> result = Minions();
        const auto& wards = Wards();
        const auto& jungle = Jungle();
        const auto& plants = Plants();
        const auto& pets = Pets();
        const auto& clones = Clones();
        result.insert(result.end(), wards.begin(), wards.end());
        result.insert(result.end(), jungle.begin(), jungle.end());
        result.insert(result.end(), plants.begin(), plants.end());
        result.insert(result.end(), pets.begin(), pets.end());
        result.insert(result.end(), clones.begin(), clones.end());
        return result;
    } else if constexpr (std::is_same_v<T, AITurretClient>) {
        return Turrets();
    } else if constexpr (std::is_same_v<T, BarracksDampenerClient>) {
        return Inhibitors();
    } else if constexpr (std::is_same_v<T, HQClient>) {
        return Nexuses();
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

} // namespace SDK::GameObjects

namespace SDK::ObjectManager {
    inline const std::vector<AIHeroClient>& Heroes() { return SDK::GameObjects::Heroes(); }
    inline const std::vector<AIHeroClient>& AllyHeroes() { return SDK::GameObjects::AllyHeroes(); }
    inline const std::vector<AIHeroClient>& EnemyHeroes() { return SDK::GameObjects::EnemyHeroes(); }
    inline const std::vector<AIMinionClient>& Minions() { return SDK::GameObjects::Minions(); }
    inline const std::vector<AIMinionClient>& AllyMinions() { return SDK::GameObjects::AllyMinions(); }
    inline const std::vector<AIMinionClient>& EnemyMinions() { return SDK::GameObjects::EnemyMinions(); }
    inline const std::vector<AIMinionClient>& AllyClones() { return SDK::GameObjects::AllyClones(); }
    inline const std::vector<AIMinionClient>& EnemyClones() { return SDK::GameObjects::EnemyClones(); }
    inline const std::vector<AIMinionClient>& Clones() { return SDK::GameObjects::Clones(); }
    inline const std::vector<AIMinionClient>& AllyPets() { return SDK::GameObjects::AllyPets(); }
    inline const std::vector<AIMinionClient>& EnemyPets() { return SDK::GameObjects::EnemyPets(); }
    inline const std::vector<AIMinionClient>& Pets() { return SDK::GameObjects::Pets(); }
    inline const std::vector<AIMinionClient>& Jungle() { return SDK::GameObjects::Jungle(); }
    inline const std::vector<AIMinionClient>& JungleMinions() { return SDK::GameObjects::JungleMinions(); }
    inline const std::vector<AIMinionClient>& SmallJungle() { return SDK::GameObjects::SmallJungle(); }
    inline const std::vector<AIMinionClient>& LargeJungle() { return SDK::GameObjects::LargeJungle(); }
    inline const std::vector<AIMinionClient>& EpicJungle() { return SDK::GameObjects::EpicJungle(); }
    inline const std::vector<AIMinionClient>& Plants() { return SDK::GameObjects::Plants(); }
    inline const std::vector<AIMinionClient>& Wards() { return SDK::GameObjects::Wards(); }
    inline const std::vector<AITurretClient>& Turrets() { return SDK::GameObjects::Turrets(); }
    inline const std::vector<AITurretClient>& AllyTurrets() { return SDK::GameObjects::AllyTurrets(); }
    inline const std::vector<AITurretClient>& EnemyTurrets() { return SDK::GameObjects::EnemyTurrets(); }
    inline const std::vector<BarracksDampenerClient>& EnemyInhibitors() { return SDK::GameObjects::EnemyInhibitors(); }
    inline HQClient EnemyNexus() { return SDK::GameObjects::EnemyNexus(); }
    inline const std::vector<MissileClient>& Missiles() { return SDK::GameObjects::Missiles(); }
    inline const std::vector<GameObject>& AllObjects() { return SDK::GameObjects::AllGameObjects(); }
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

inline bool AIBaseClient::IsUnderAllyTurret() const {
    for (const auto& turret : GameObjects::AllyTurrets()) {
        if (!turret.IsValid() || turret.IsDead()) {
            continue;
        }
        const float range = turret.AttackRange() + turret.BoundingRadius() + BoundingRadius();
        if (turret.Position().DistanceSqr2D(Position()) <= range * range) {
            return true;
        }
    }
    return false;
}

inline bool AIBaseClient::IsUnderEnemyTurret() const {
    for (const auto& turret : GameObjects::EnemyTurrets()) {
        if (!turret.IsValid() || turret.IsDead()) {
            continue;
        }
        const float range = turret.AttackRange() + turret.BoundingRadius() + BoundingRadius();
        if (turret.Position().DistanceSqr2D(Position()) <= range * range) {
            return true;
        }
    }
    return false;
}

} // namespace SDK
