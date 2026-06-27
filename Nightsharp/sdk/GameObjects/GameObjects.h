#pragma once

#include "ObjectManager.h"
#include "../Events/Load.h"

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace SDK::GameObjects {

namespace detail {
    inline std::vector<AIHeroClient> AllyHeroesList;
    inline std::vector<BarracksDampenerClient> AllyInhibitorsList;
    inline std::vector<AIBaseClient> AllyList;
    inline std::vector<AIMinionClient> AllyMinionsList;
    inline std::vector<AIMinionClient> AllyClonesList;
    inline std::vector<AIMinionClient> AllyPetsList;
    inline std::vector<ShopClient> AllyShopsList;
    inline std::vector<Obj_SpawnPoint> AllySpawnPointsList;
    inline std::vector<AITurretClient> AllyTurretsList;
    inline std::vector<AIMinionClient> AllyWardsList;
    inline std::vector<AttackableUnit> AttackableUnitsList;
    inline std::vector<AIHeroClient> EnemyHeroesList;
    inline std::vector<BarracksDampenerClient> EnemyInhibitorsList;
    inline std::vector<AIBaseClient> EnemyList;
    inline std::vector<AIMinionClient> EnemyMinionsList;
    inline std::vector<AIMinionClient> EnemyClonesList;
    inline std::vector<AIMinionClient> EnemyPetsList;
    inline std::vector<ShopClient> EnemyShopsList;
    inline std::vector<Obj_SpawnPoint> EnemySpawnPointsList;
    inline std::vector<AITurretClient> EnemyTurretsList;
    inline std::vector<AIMinionClient> EnemyWardsList;
    inline std::vector<GameObject> GameObjectsList;
    inline std::vector<AIHeroClient> HeroesList;
    inline std::vector<BarracksDampenerClient> InhibitorsList;
    inline std::vector<AIMinionClient> JungleLargeList;
    inline std::vector<AIMinionClient> JungleLegendaryList;
    inline std::vector<AIMinionClient> JungleList;
    inline std::vector<AIMinionClient> JungleSmallList;
    inline std::vector<AIMinionClient> MinionsList;
    inline std::vector<AIMinionClient> ClonesList;
    inline std::vector<AIMinionClient> PetsList;
    inline std::vector<HQClient> NexusList;
    inline std::vector<EffectEmitter> ParticleEmittersList;
    inline std::vector<ShopClient> ShopsList;
    inline std::vector<Obj_SpawnPoint> SpawnPointsList;
    inline std::vector<AITurretClient> TurretsList;
    inline std::vector<AIMinionClient> WardsList;

    inline HQClient AllyNexusObject;
    inline HQClient EnemyNexusObject;
    inline AIHeroClient PlayerObject;
    inline bool Initialized = false;
    inline bool Loaded = false;
    inline float LastMinionRoleRefresh = -1.0f;
    inline std::size_t NextMinionRoleRefreshIndex = 0;

    template <typename T>
    inline bool Contains(const std::vector<T>& list, const GameObject& object) {
        return std::find_if(list.begin(), list.end(), [&](const T& entry) {
            return entry.Compare(object);
        }) != list.end();
    }

    template <typename T>
    inline void AddUnique(std::vector<T>& list, const T& object) {
        if (!object.IsValid() || Contains(list, object)) {
            return;
        }
        list.push_back(object);
    }

    template <typename T>
    inline void RemoveMatching(std::vector<T>& list, const GameObject& object) {
        list.erase(
            std::remove_if(list.begin(), list.end(), [&](const T& entry) {
                return entry.Compare(object);
            }),
            list.end());
    }

    inline bool IsWard(const AIMinionClient& minion) {
        return HasFlag(minion.GetMinionType(), MinionTypes::Ward);
    }

    inline bool IsWardCorpse(const AIMinionClient& minion) {
        return minion.Name() == "WardCorpse" || minion.CharacterName() == "WardCorpse";
    }

    inline void Clear() {
        AllyHeroesList.clear();
        AllyInhibitorsList.clear();
        AllyList.clear();
        AllyMinionsList.clear();
        AllyClonesList.clear();
        AllyPetsList.clear();
        AllyShopsList.clear();
        AllySpawnPointsList.clear();
        AllyTurretsList.clear();
        AllyWardsList.clear();
        AttackableUnitsList.clear();
        EnemyHeroesList.clear();
        EnemyInhibitorsList.clear();
        EnemyList.clear();
        EnemyMinionsList.clear();
        EnemyClonesList.clear();
        EnemyPetsList.clear();
        EnemyShopsList.clear();
        EnemySpawnPointsList.clear();
        EnemyTurretsList.clear();
        EnemyWardsList.clear();
        GameObjectsList.clear();
        HeroesList.clear();
        InhibitorsList.clear();
        JungleLargeList.clear();
        JungleLegendaryList.clear();
        JungleList.clear();
        JungleSmallList.clear();
        MinionsList.clear();
        ClonesList.clear();
        PetsList.clear();
        NexusList.clear();
        ParticleEmittersList.clear();
        ShopsList.clear();
        SpawnPointsList.clear();
        TurretsList.clear();
        WardsList.clear();
        AllyNexusObject = HQClient();
        EnemyNexusObject = HQClient();
        PlayerObject = AIHeroClient();
        LastMinionRoleRefresh = -1.0f;
        NextMinionRoleRefreshIndex = 0;
        Loaded = false;
    }

    inline void AddHero(const AIHeroClient& hero) {
        AddUnique(HeroesList, hero);
        if (hero.IsEnemy()) {
            AddUnique(EnemyHeroesList, hero);
            AddUnique(EnemyList, AIBaseClient(hero.Handle()));
        } else {
            AddUnique(AllyHeroesList, hero);
            AddUnique(AllyList, AIBaseClient(hero.Handle()));
        }
    }

    inline void AddMinionRole(const AIMinionClient& minion) {
        if (minion.IsClone()) {
            AddUnique(ClonesList, minion);
            if (minion.IsEnemy()) {
                AddUnique(EnemyClonesList, minion);
            } else {
                AddUnique(AllyClonesList, minion);
            }
        } else if (minion.IsPet()) {
            AddUnique(PetsList, minion);
            if (minion.IsEnemy()) {
                AddUnique(EnemyPetsList, minion);
            } else {
                AddUnique(AllyPetsList, minion);
            }
        }
    }

    inline void AddMinion(const AIMinionClient& minion) {
        if (minion.Team() != GameObjectTeam::Neutral) {
            if (IsWard(minion)) {
                AddUnique(WardsList, minion);
                if (minion.IsEnemy()) {
                    AddUnique(EnemyWardsList, minion);
                } else {
                    AddUnique(AllyWardsList, minion);
                }
            } else {
                AddUnique(MinionsList, minion);
                AddMinionRole(minion);
                if (minion.IsEnemy()) {
                    AddUnique(EnemyMinionsList, minion);
                    AddUnique(EnemyList, AIBaseClient(minion.Handle()));
                } else {
                    AddUnique(AllyMinionsList, minion);
                    AddUnique(AllyList, AIBaseClient(minion.Handle()));
                }
            }
            return;
        }

        if (IsWardCorpse(minion)) {
            return;
        }

        switch (minion.GetJungleType()) {
        case JungleType::Small:
            AddUnique(JungleSmallList, minion);
            break;
        case JungleType::Large:
            AddUnique(JungleLargeList, minion);
            break;
        case JungleType::Legendary:
            AddUnique(JungleLegendaryList, minion);
            break;
        default:
            break;
        }
        AddUnique(JungleList, minion);
    }

    // The common object-create hook can run before a newly constructed
    // AIMinionClient has received its CharacterName/MinionClass replication.
    // Re-evaluate only the clone/pet role lists after creation so delayed
    // fields cannot permanently classify a clone as an ordinary minion.
    inline void RefreshMinionRoles() {
        const float now = CoreRuntime::GetContext().gameTime;
        if (now <= 0.0f) {
            return;
        }
        if (now > 0.0f && LastMinionRoleRefresh >= 0.0f &&
            now - LastMinionRoleRefresh < 0.25f) {
            return;
        }
        LastMinionRoleRefresh = now;

        constexpr std::size_t kMaxRoleRefreshPerTick = 8;
        const std::size_t count = MinionsList.size();
        if (count == 0) {
            NextMinionRoleRefreshIndex = 0;
            return;
        }

        if (NextMinionRoleRefreshIndex >= count) {
            NextMinionRoleRefreshIndex = 0;
        }

        const std::size_t limit = std::min(kMaxRoleRefreshPerTick, count);
        for (std::size_t i = 0; i < limit; ++i) {
            const auto& minion = MinionsList[NextMinionRoleRefreshIndex];
            if (minion.IsValid()) {
                AddMinionRole(minion);
            }

            ++NextMinionRoleRefreshIndex;
            if (NextMinionRoleRefreshIndex >= count) {
                NextMinionRoleRefreshIndex = 0;
            }
        }
    }

    inline void AddTurret(const AITurretClient& turret) {
        AddUnique(TurretsList, turret);
        if (turret.IsEnemy()) {
            AddUnique(EnemyTurretsList, turret);
            AddUnique(EnemyList, AIBaseClient(turret.Handle()));
        } else {
            AddUnique(AllyTurretsList, turret);
            AddUnique(AllyList, AIBaseClient(turret.Handle()));
        }
    }

    inline void AddShop(const ShopClient& shop) {
        AddUnique(ShopsList, shop);
        if (shop.IsAlly()) {
            AddUnique(AllyShopsList, shop);
        } else {
            AddUnique(EnemyShopsList, shop);
        }
    }

    inline void AddSpawnPoint(const Obj_SpawnPoint& spawnPoint) {
        AddUnique(SpawnPointsList, spawnPoint);
        if (spawnPoint.IsAlly()) {
            AddUnique(AllySpawnPointsList, spawnPoint);
        } else {
            AddUnique(EnemySpawnPointsList, spawnPoint);
        }
    }

    inline void AddInhibitor(const BarracksDampenerClient& inhibitor) {
        AddUnique(InhibitorsList, inhibitor);
        if (inhibitor.IsAlly()) {
            AddUnique(AllyInhibitorsList, inhibitor);
        } else {
            AddUnique(EnemyInhibitorsList, inhibitor);
        }
    }

    inline void AddNexus(const HQClient& nexus) {
        AddUnique(NexusList, nexus);
        if (nexus.IsAlly()) {
            AllyNexusObject = nexus;
        } else {
            EnemyNexusObject = nexus;
        }
    }

    inline void AddObject(
        uintptr_t address,
        ::Core::Objects::ObjectType knownType =
            ::Core::Objects::ObjectType::Unknown) {
        if (!Globals::IsValidPtr(address)) {
            return;
        }

        const auto type =
            knownType == ::Core::Objects::ObjectType::Unknown
                ? SDK::ObjectManager::detail::InferExtendedType(address)
                : knownType;
        GameObject gameObject(::Core::ObjectManager::MakeHandle(address, type));
        AddUnique(GameObjectsList, gameObject);

        if (::Core::Objects::IsAttackable(type)) {
            AddUnique(AttackableUnitsList, AttackableUnit(gameObject.Handle()));
        }

        switch (type) {
        case ::Core::Objects::ObjectType::AIHeroClient:
            AddHero(AIHeroClient(gameObject.Handle()));
            break;
        case ::Core::Objects::ObjectType::AIMinionClient:
            AddMinion(AIMinionClient(gameObject.Handle()));
            break;
        case ::Core::Objects::ObjectType::AITurretClient:
            AddTurret(AITurretClient(gameObject.Handle()));
            break;
        case ::Core::Objects::ObjectType::EffectEmitter:
            AddUnique(ParticleEmittersList, EffectEmitter(gameObject.Handle()));
            break;
        case ::Core::Objects::ObjectType::ShopClient:
            AddShop(ShopClient(gameObject.Handle()));
            break;
        case ::Core::Objects::ObjectType::Obj_SpawnPoint:
            AddSpawnPoint(Obj_SpawnPoint(gameObject.Handle()));
            break;
        case ::Core::Objects::ObjectType::BarracksDampenerClient:
            AddInhibitor(BarracksDampenerClient(gameObject.Handle()));
            break;
        case ::Core::Objects::ObjectType::HQClient:
            AddNexus(HQClient(gameObject.Handle()));
            break;
        default:
            break;
        }
    }

    inline void RemoveObject(const GameObject& object) {
        RemoveMatching(GameObjectsList, object);
        RemoveMatching(AttackableUnitsList, object);
        RemoveMatching(HeroesList, object);
        RemoveMatching(AllyHeroesList, object);
        RemoveMatching(EnemyHeroesList, object);
        RemoveMatching(MinionsList, object);
        RemoveMatching(ClonesList, object);
        RemoveMatching(PetsList, object);
        RemoveMatching(AllyMinionsList, object);
        RemoveMatching(AllyClonesList, object);
        RemoveMatching(AllyPetsList, object);
        RemoveMatching(EnemyMinionsList, object);
        RemoveMatching(EnemyClonesList, object);
        RemoveMatching(EnemyPetsList, object);
        RemoveMatching(WardsList, object);
        RemoveMatching(AllyWardsList, object);
        RemoveMatching(EnemyWardsList, object);
        RemoveMatching(JungleList, object);
        RemoveMatching(JungleSmallList, object);
        RemoveMatching(JungleLargeList, object);
        RemoveMatching(JungleLegendaryList, object);
        RemoveMatching(TurretsList, object);
        RemoveMatching(AllyTurretsList, object);
        RemoveMatching(EnemyTurretsList, object);
        RemoveMatching(AllyList, object);
        RemoveMatching(EnemyList, object);
        RemoveMatching(ParticleEmittersList, object);
        RemoveMatching(ShopsList, object);
        RemoveMatching(AllyShopsList, object);
        RemoveMatching(EnemyShopsList, object);
        RemoveMatching(SpawnPointsList, object);
        RemoveMatching(AllySpawnPointsList, object);
        RemoveMatching(EnemySpawnPointsList, object);
        RemoveMatching(InhibitorsList, object);
        RemoveMatching(AllyInhibitorsList, object);
        RemoveMatching(EnemyInhibitorsList, object);
        RemoveMatching(NexusList, object);

        if (AllyNexusObject.Compare(object)) {
            AllyNexusObject = HQClient();
        }
        if (EnemyNexusObject.Compare(object)) {
            EnemyNexusObject = HQClient();
        }
    }

    inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
        if (!Loaded || !args.Sender.IsValid()) {
            return;
        }

        AddObject(args.Sender.Ptr, args.Sender.Type);
    }

    inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
        if (!Loaded || !args.Sender.IsValid()) {
            return;
        }

        RemoveObject(GameObject(::Core::ObjectManager::MakeHandle(
            args.Sender.Ptr,
            args.Sender.Type)));
    }

    inline void Rebuild() {
        Clear();
        PlayerObject = SDK::ObjectManager::Player();

        for (const auto& object : SDK::ObjectManager::Get<GameObject>()) {
            AddObject(object.Address());
        }
        Loaded = true;
    }

    inline void OnLoad() {
        Rebuild();
    }

    inline void OnGameUpdate(const SDK::Events::GameUpdateEventArgs&) {
        RefreshMinionRoles();
    }
} // namespace detail

inline void Initialize() {
    if (detail::Initialized) {
        return;
    }

    detail::Initialized = true;
    SDK::Events::AddOnCreateObject(&detail::OnObjectCreate);
    SDK::Events::AddOnDeleteObject(&detail::OnObjectDelete);
    SDK::Events::AddOnLoad([](const SDK::Events::LoadEventArgs&) {
        detail::OnLoad();
        SDK::Events::AddOnGameUpdate(&detail::OnGameUpdate);
    });
}

inline void Shutdown() {
    detail::Clear();
    detail::Initialized = false;
}

inline void EnsureInitialized() {
    Initialize();
    if (!detail::Loaded && CoreRuntime::IsReady() && CoreRuntime::GetContext().gameTime > 0.0f) {
        detail::Rebuild();
    }
}

inline const std::vector<GameObject>& AllGameObjects() { EnsureInitialized(); return detail::GameObjectsList; }
inline const std::vector<AIBaseClient>& Ally() { EnsureInitialized(); return detail::AllyList; }
inline const std::vector<AIHeroClient>& AllyHeroes() { EnsureInitialized(); return detail::AllyHeroesList; }
inline const std::vector<BarracksDampenerClient>& AllyInhibitors() { EnsureInitialized(); return detail::AllyInhibitorsList; }
inline const std::vector<AIMinionClient>& AllyMinions() { EnsureInitialized(); return detail::AllyMinionsList; }
inline const std::vector<AIMinionClient>& AllyClones() { EnsureInitialized(); return detail::AllyClonesList; }
inline const std::vector<AIMinionClient>& AllyPets() { EnsureInitialized(); return detail::AllyPetsList; }
inline HQClient AllyNexus() { EnsureInitialized(); return detail::AllyNexusObject; }
inline const std::vector<ShopClient>& AllyShops() { EnsureInitialized(); return detail::AllyShopsList; }
inline const std::vector<Obj_SpawnPoint>& AllySpawnPoints() { EnsureInitialized(); return detail::AllySpawnPointsList; }
inline const std::vector<AITurretClient>& AllyTurrets() { EnsureInitialized(); return detail::AllyTurretsList; }
inline const std::vector<AIMinionClient>& AllyWards() { EnsureInitialized(); return detail::AllyWardsList; }
inline const std::vector<AttackableUnit>& AttackableUnits() { EnsureInitialized(); return detail::AttackableUnitsList; }
inline const std::vector<AIBaseClient>& Enemy() { EnsureInitialized(); return detail::EnemyList; }
inline const std::vector<AIHeroClient>& EnemyHeroes() { EnsureInitialized(); return detail::EnemyHeroesList; }
inline const std::vector<BarracksDampenerClient>& EnemyInhibitors() { EnsureInitialized(); return detail::EnemyInhibitorsList; }
inline const std::vector<AIMinionClient>& EnemyMinions() { EnsureInitialized(); return detail::EnemyMinionsList; }
inline const std::vector<AIMinionClient>& EnemyClones() { EnsureInitialized(); return detail::EnemyClonesList; }
inline const std::vector<AIMinionClient>& EnemyPets() { EnsureInitialized(); return detail::EnemyPetsList; }
inline HQClient EnemyNexus() { EnsureInitialized(); return detail::EnemyNexusObject; }
inline const std::vector<ShopClient>& EnemyShops() { EnsureInitialized(); return detail::EnemyShopsList; }
inline const std::vector<Obj_SpawnPoint>& EnemySpawnPoints() { EnsureInitialized(); return detail::EnemySpawnPointsList; }
inline const std::vector<AITurretClient>& EnemyTurrets() { EnsureInitialized(); return detail::EnemyTurretsList; }
inline const std::vector<AIMinionClient>& EnemyWards() { EnsureInitialized(); return detail::EnemyWardsList; }
inline const std::vector<AIHeroClient>& Heroes() { EnsureInitialized(); return detail::HeroesList; }
inline const std::vector<BarracksDampenerClient>& Inhibitors() { EnsureInitialized(); return detail::InhibitorsList; }
inline const std::vector<AIMinionClient>& Jungle() { EnsureInitialized(); return detail::JungleList; }
inline const std::vector<AIMinionClient>& JungleLarge() { EnsureInitialized(); return detail::JungleLargeList; }
inline const std::vector<AIMinionClient>& JungleLegendary() { EnsureInitialized(); return detail::JungleLegendaryList; }
inline const std::vector<AIMinionClient>& JungleSmall() { EnsureInitialized(); return detail::JungleSmallList; }
inline const std::vector<AIMinionClient>& Minions() { EnsureInitialized(); return detail::MinionsList; }
inline const std::vector<AIMinionClient>& Clones() { EnsureInitialized(); return detail::ClonesList; }
inline const std::vector<AIMinionClient>& Pets() { EnsureInitialized(); return detail::PetsList; }
inline const std::vector<HQClient>& Nexuses() { EnsureInitialized(); return detail::NexusList; }
inline const std::vector<EffectEmitter>& ParticleEmitters() { EnsureInitialized(); return detail::ParticleEmittersList; }
inline AIHeroClient Player() { EnsureInitialized(); return detail::PlayerObject; }
inline const std::vector<ShopClient>& Shops() { EnsureInitialized(); return detail::ShopsList; }
inline const std::vector<Obj_SpawnPoint>& SpawnPoints() { EnsureInitialized(); return detail::SpawnPointsList; }
inline const std::vector<AITurretClient>& Turrets() { EnsureInitialized(); return detail::TurretsList; }
inline const std::vector<AIMinionClient>& Wards() { EnsureInitialized(); return detail::WardsList; }

inline bool Compare(const GameObject& gameObject, const GameObject& object) {
    return gameObject.Compare(object);
}

inline Vec3 PlayerPosition() {
    const AIHeroClient player = Player();
    return player.IsValid() ? player.Position() : Vec3{};
}

template <typename T>
inline std::vector<T> Get() {
    EnsureInitialized();
    if constexpr (std::is_same_v<T, GameObject>) {
        return detail::GameObjectsList;
    } else if constexpr (std::is_same_v<T, AttackableUnit>) {
        return detail::AttackableUnitsList;
    } else if constexpr (std::is_same_v<T, AIBaseClient>) {
        std::vector<T> result;
        result.reserve(detail::HeroesList.size() + detail::MinionsList.size() +
                       detail::WardsList.size() + detail::JungleList.size() +
                       detail::TurretsList.size());
        for (const auto& object : detail::HeroesList) result.emplace_back(object.Handle());
        for (const auto& object : detail::MinionsList) result.emplace_back(object.Handle());
        for (const auto& object : detail::WardsList) result.emplace_back(object.Handle());
        for (const auto& object : detail::JungleList) result.emplace_back(object.Handle());
        for (const auto& object : detail::TurretsList) result.emplace_back(object.Handle());
        return result;
    } else if constexpr (std::is_same_v<T, AIHeroClient>) {
        return detail::HeroesList;
    } else if constexpr (std::is_same_v<T, AIMinionClient>) {
        std::vector<T> result = detail::MinionsList;
        result.insert(result.end(), detail::WardsList.begin(), detail::WardsList.end());
        result.insert(result.end(), detail::JungleList.begin(), detail::JungleList.end());
        return result;
    } else if constexpr (std::is_same_v<T, AITurretClient>) {
        return detail::TurretsList;
    } else if constexpr (std::is_same_v<T, BarracksDampenerClient>) {
        return detail::InhibitorsList;
    } else if constexpr (std::is_same_v<T, HQClient>) {
        return detail::NexusList;
    } else if constexpr (std::is_same_v<T, EffectEmitter>) {
        return detail::ParticleEmittersList;
    } else if constexpr (std::is_same_v<T, ShopClient>) {
        return detail::ShopsList;
    } else if constexpr (std::is_same_v<T, Obj_SpawnPoint>) {
        return detail::SpawnPointsList;
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
    inline const std::vector<AIHeroClient>& AllyHeroes() { return SDK::GameObjects::AllyHeroes(); }
    inline const std::vector<AIHeroClient>& EnemyHeroes() { return SDK::GameObjects::EnemyHeroes(); }
    inline const std::vector<AIMinionClient>& AllyMinions() { return SDK::GameObjects::AllyMinions(); }
    inline const std::vector<AIMinionClient>& AllyClones() { return SDK::GameObjects::AllyClones(); }
    inline const std::vector<AIMinionClient>& AllyPets() { return SDK::GameObjects::AllyPets(); }
    inline const std::vector<AIMinionClient>& EnemyMinions() { return SDK::GameObjects::EnemyMinions(); }
    inline const std::vector<AIMinionClient>& EnemyClones() { return SDK::GameObjects::EnemyClones(); }
    inline const std::vector<AIMinionClient>& EnemyPets() { return SDK::GameObjects::EnemyPets(); }
    inline const std::vector<AITurretClient>& AllyTurrets() { return SDK::GameObjects::AllyTurrets(); }
    inline const std::vector<AITurretClient>& EnemyTurrets() { return SDK::GameObjects::EnemyTurrets(); }
    inline const std::vector<AIMinionClient>& Minions() { return SDK::GameObjects::Minions(); }
    inline const std::vector<AIMinionClient>& Clones() { return SDK::GameObjects::Clones(); }
    inline const std::vector<AIMinionClient>& Pets() { return SDK::GameObjects::Pets(); }
    inline const std::vector<AIHeroClient>& Heroes() { return SDK::GameObjects::Heroes(); }
    inline const std::vector<AITurretClient>& Turrets() { return SDK::GameObjects::Turrets(); }
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

// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
