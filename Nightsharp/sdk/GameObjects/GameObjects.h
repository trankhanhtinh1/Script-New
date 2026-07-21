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

    inline std::vector<AITurretClient> TurretsList;
    inline std::vector<AITurretClient> AllyTurretsList;
    inline std::vector<AITurretClient> EnemyTurretsList;

    inline std::vector<BarracksDampenerClient> InhibitorsList;
    inline std::vector<BarracksDampenerClient> AllyInhibitorsList;
    inline std::vector<BarracksDampenerClient> EnemyInhibitorsList;
    inline std::vector<HQClient> NexusList;
    inline HQClient AllyNexusObject;
    inline HQClient EnemyNexusObject;

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

    inline bool EqualsAny(const std::string& value, std::initializer_list<const char*> needles) {
        for (const auto* needle : needles) {
            if (needle && value == ToLower(needle)) {
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
        const auto player = SDK::ObjectManager::Player();
        return player.IsValid() ? TeamValue(player) : 0;
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



    template <typename T>
    inline bool ContainsByNetworkId(const std::vector<T>& vec, int netId) {
        if (netId == 0) return false;
        for (const auto& item : vec) {
            if (item.NetworkId() == netId) return true;
        }
        return false;
    }

    template <typename T>
    inline void PushUniqueByNetworkId(std::vector<T>& vec, const T& obj) {
        const int netId = obj.NetworkId();
        if (netId != 0 && ContainsByNetworkId(vec, netId)) {
            return;
        }
        vec.push_back(obj);
    }

    template <typename T>
    inline void EraseByNetworkId(std::vector<T>& vec, int netId) {
        vec.erase(std::remove_if(vec.begin(), vec.end(), [netId](const T& obj) {
            return obj.NetworkId() == netId;
        }), vec.end());
    }

    inline void OnObjectAdd(const GameObject& object) {
        if (!object.IsValid()) return;
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

            if (IsWardObject(minion)) {
                PushUniqueByNetworkId(WardsList, minion);
                if (enemy) PushUniqueByNetworkId(EnemyWardsList, minion);
                else PushUniqueByNetworkId(AllyWardsList, minion);
                break;
            }

            if (IsPlantObject(minion)) {
                PushUniqueByNetworkId(PlantsList, minion);
                break;
            }

            if (IsJungleObject(minion)) {
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

            if (IsCloneObject(minion)) {
                PushUniqueByNetworkId(ClonesList, minion);
                if (enemy) PushUniqueByNetworkId(EnemyClonesList, minion);
                else PushUniqueByNetworkId(AllyClonesList, minion);
                break;
            }

            if (IsPetObject(minion)) {
                PushUniqueByNetworkId(PetsList, minion);
                if (enemy) PushUniqueByNetworkId(EnemyPetsList, minion);
                else PushUniqueByNetworkId(AllyPetsList, minion);
                break;
            }

            if (IsSpecialMinionObject(minion)) {
                if (enemy) PushUniqueByNetworkId(EnemySpecialMinionsList, minion);
                else PushUniqueByNetworkId(AllySpecialMinionsList, minion);
                break;
            }

            if (IsIgnoredMinionObject(minion)) {
                if (enemy) PushUniqueByNetworkId(EnemyIgnoredMinionsList, minion);
                else PushUniqueByNetworkId(AllyIgnoredMinionsList, minion);
                break;
            }

            if (IsLaneMinionObject(minion)) {
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
        case ::Core::Objects::ObjectType::AITurretClient: {
            const AITurretClient turret(object.Handle());
            PushUniqueByNetworkId(TurretsList, turret);
            if (ally) PushUniqueByNetworkId(AllyTurretsList, turret);
            else if (enemy) PushUniqueByNetworkId(EnemyTurretsList, turret);
            break;
        }
        case ::Core::Objects::ObjectType::BarracksDampenerClient: {
            const BarracksDampenerClient inhib(object.Handle());
            PushUniqueByNetworkId(InhibitorsList, inhib);
            if (ally) PushUniqueByNetworkId(AllyInhibitorsList, inhib);
            else if (enemy) PushUniqueByNetworkId(EnemyInhibitorsList, inhib);
            break;
        }
        case ::Core::Objects::ObjectType::HQClient: {
            const HQClient nexus(object.Handle());
            PushUniqueByNetworkId(NexusList, nexus);
            if (ally) AllyNexusObject = nexus;
            else if (enemy) EnemyNexusObject = nexus;
            break;
        }
        case ::Core::Objects::ObjectType::MissileClient: {
            const MissileClient missile(object.Handle());
            PushUniqueByNetworkId(MissilesList, missile);
            break;
        }
        default:
            break;
        }
    }

    inline void OnObjectDelete(int netId) {
        if (netId == 0) return;
        EraseByNetworkId(GameObjectsList, netId);
        EraseByNetworkId(AttackableUnitsList, netId);
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
        EraseByNetworkId(TurretsList, netId);
        EraseByNetworkId(AllyTurretsList, netId);
        EraseByNetworkId(EnemyTurretsList, netId);
        EraseByNetworkId(InhibitorsList, netId);
        EraseByNetworkId(AllyInhibitorsList, netId);
        EraseByNetworkId(EnemyInhibitorsList, netId);
        EraseByNetworkId(NexusList, netId);
        EraseByNetworkId(MissilesList, netId);
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
        TurretsList.clear();
        AllyTurretsList.clear();
        EnemyTurretsList.clear();
        InhibitorsList.clear();
        AllyInhibitorsList.clear();
        EnemyInhibitorsList.clear();
        NexusList.clear();
        AllyNexusObject = {};
        EnemyNexusObject = {};
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

    inline void DispatchLifecycle(std::vector<LifecycleHandler>& source,
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
        for (const auto handler : handlers) {
            if (handler) {
                handler(object);
            }
        }
    }

    inline void OnNativeObjectCreate(const SDK::Events::ObjectEventArgs& args) {
        if (args.Sender.IsValid()) {
            OnObjectAdd(ObjectFromArgs(args));
        }
        DispatchLifecycle(CreateHandlers, args);
    }

    inline void OnNativeObjectDelete(const SDK::Events::ObjectEventArgs& args) {
        if (args.Sender.IsValid()) {
            OnObjectDelete(static_cast<int>(args.Sender.NetworkId));
        }
        DispatchLifecycle(DeleteHandlers, args);
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
    detail::PlayerObject = SDK::ObjectManager::Player();
}

inline AIHeroClient Player() {
    EnsureInitialized();
    detail::PlayerObject = SDK::ObjectManager::Player();
    return detail::PlayerObject;
}

// ------------------------------- accessors ----------------------------------
inline std::vector<AIHeroClient> Heroes() {
    return SDK::ObjectManager::Get<AIHeroClient>();
}

inline std::vector<AIHeroClient> AllyHeroes() {
    std::vector<AIHeroClient> result;
    const int myTeam = detail::PlayerTeam();
    const auto heroes = SDK::ObjectManager::Get<AIHeroClient>();
    result.reserve(heroes.size());
    for (const auto& hero : heroes) {
        if (!hero.IsValid()) continue;
        const int team = detail::TeamValue(hero);
        if (myTeam != 0 && team == myTeam) {
            result.push_back(hero);
        }
    }
    return result;
}

inline std::vector<AIHeroClient> EnemyHeroes() {
    std::vector<AIHeroClient> result;
    const int myTeam = detail::PlayerTeam();
    const auto heroes = SDK::ObjectManager::Get<AIHeroClient>();
    result.reserve(heroes.size());
    for (const auto& hero : heroes) {
        if (!hero.IsValid()) continue;
        const int team = detail::TeamValue(hero);
        if (team != 0 && team != 300 && (myTeam == 0 || team != myTeam)) {
            result.push_back(hero);
        }
    }
    return result;
}

inline std::vector<AIMinionClient> Minions() {
    return SDK::ObjectManager::Get<AIMinionClient>();
}

inline std::vector<AIMinionClient> AllyMinions() {
    std::vector<AIMinionClient> result;
    const int myTeam = detail::PlayerTeam();
    const auto minions = SDK::ObjectManager::Get<AIMinionClient>();
    result.reserve(minions.size());
    for (const auto& minion : minions) {
        if (!minion.IsValid()) continue;
        const int team = detail::TeamValue(minion);
        if (myTeam != 0 && team == myTeam) {
            result.push_back(minion);
        }
    }
    return result;
}

inline std::vector<AIMinionClient> EnemyMinions() {
    std::vector<AIMinionClient> result;
    const int myTeam = detail::PlayerTeam();
    const auto minions = SDK::ObjectManager::Get<AIMinionClient>();
    result.reserve(minions.size());
    for (const auto& minion : minions) {
        if (!minion.IsValid()) continue;
        const int team = detail::TeamValue(minion);
        if (team != 0 && team != 300 && (myTeam == 0 || team != myTeam)) {
            result.push_back(minion);
        }
    }
    return result;
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

inline std::vector<AITurretClient> Turrets() {
    return SDK::ObjectManager::Get<AITurretClient>();
}
inline std::vector<AITurretClient> AllyTurrets() {
    std::vector<AITurretClient> result;
    const int myTeam = detail::PlayerTeam();
    const auto turrets = SDK::ObjectManager::Get<AITurretClient>();
    result.reserve(turrets.size());
    for (const auto& turret : turrets) {
        if (!turret.IsValid()) continue;
        const int team = detail::TeamValue(turret);
        if (myTeam != 0 && team == myTeam) {
            result.push_back(turret);
        }
    }
    return result;
}
inline std::vector<AITurretClient> EnemyTurrets() {
    std::vector<AITurretClient> result;
    const int myTeam = detail::PlayerTeam();
    const auto turrets = SDK::ObjectManager::Get<AITurretClient>();
    result.reserve(turrets.size());
    for (const auto& turret : turrets) {
        if (!turret.IsValid()) continue;
        const int team = detail::TeamValue(turret);
        if (team != 0 && team != 300 && (myTeam == 0 || team != myTeam)) {
            result.push_back(turret);
        }
    }
    return result;
}

inline std::vector<BarracksDampenerClient> Inhibitors() {
    return SDK::ObjectManager::Get<BarracksDampenerClient>();
}
inline std::vector<BarracksDampenerClient> AllyInhibitors() {
    std::vector<BarracksDampenerClient> result;
    const int myTeam = detail::PlayerTeam();
    for (const auto& inhib : Inhibitors()) {
        if (inhib.IsValid() && myTeam != 0 && detail::TeamValue(inhib) == myTeam) {
            result.push_back(inhib);
        }
    }
    return result;
}
inline std::vector<BarracksDampenerClient> EnemyInhibitors() {
    std::vector<BarracksDampenerClient> result;
    const int myTeam = detail::PlayerTeam();
    for (const auto& inhib : Inhibitors()) {
        if (inhib.IsValid() && detail::TeamValue(inhib) != 300 && (myTeam == 0 || detail::TeamValue(inhib) != myTeam)) {
            result.push_back(inhib);
        }
    }
    return result;
}

inline std::vector<HQClient> Nexuses() {
    return SDK::ObjectManager::Get<HQClient>();
}
inline HQClient AllyNexus() {
    const int myTeam = detail::PlayerTeam();
    for (const auto& n : Nexuses()) {
        if (n.IsValid() && myTeam != 0 && detail::TeamValue(n) == myTeam) {
            return n;
        }
    }
    return {};
}
inline HQClient EnemyNexus() {
    const int myTeam = detail::PlayerTeam();
    for (const auto& n : Nexuses()) {
        if (n.IsValid() && (myTeam == 0 || detail::TeamValue(n) != myTeam)) {
            return n;
        }
    }
    return {};
}

// Compatibility aliases (previous API); same snapshot data.
inline std::vector<AITurretClient> ScanTurrets() { return Turrets(); }
inline std::vector<BarracksDampenerClient> ScanInhibitors() { return Inhibitors(); }
inline std::vector<HQClient> ScanNexuses() { return Nexuses(); }

inline std::vector<ShopClient> Shops() { return detail::Snapshot(detail::ShopsList); }
inline std::vector<ShopClient> AllyShops() { return detail::Snapshot(detail::AllyShopsList); }
inline std::vector<ShopClient> EnemyShops() { return detail::Snapshot(detail::EnemyShopsList); }

inline std::vector<Obj_SpawnPoint> SpawnPoints() { return detail::Snapshot(detail::SpawnPointsList); }
inline std::vector<Obj_SpawnPoint> AllySpawnPoints() { return detail::Snapshot(detail::AllySpawnPointsList); }
inline std::vector<Obj_SpawnPoint> EnemySpawnPoints() { return detail::Snapshot(detail::EnemySpawnPointsList); }

inline std::vector<EffectEmitter> ParticleEmitters() { return detail::Snapshot(detail::ParticleEmittersList); }
inline std::vector<MissileClient> Missiles() {
    return SDK::ObjectManager::Get<MissileClient>();
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
        std::vector<T> result;
        const auto ally = Ally();
        const auto enemy = Enemy();
        result.reserve(ally.size() + enemy.size());
        result.insert(result.end(), ally.begin(), ally.end());
        result.insert(result.end(), enemy.begin(), enemy.end());
        return result;
    } else if constexpr (std::is_same_v<T, AIHeroClient>) {
        return Heroes();
    } else if constexpr (std::is_same_v<T, AIMinionClient>) {
        std::vector<T> result = Minions();
        const auto wards = Wards();
        const auto jungle = Jungle();
        const auto plants = Plants();
        const auto pets = Pets();
        const auto clones = Clones();
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
    inline std::vector<AITurretClient> Turrets() { return SDK::GameObjects::Turrets(); }
    inline std::vector<AITurretClient> AllyTurrets() { return SDK::GameObjects::AllyTurrets(); }
    inline std::vector<AITurretClient> EnemyTurrets() { return SDK::GameObjects::EnemyTurrets(); }
    inline std::vector<BarracksDampenerClient> EnemyInhibitors() { return SDK::GameObjects::EnemyInhibitors(); }
    inline HQClient EnemyNexus() { return SDK::GameObjects::EnemyNexus(); }
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
