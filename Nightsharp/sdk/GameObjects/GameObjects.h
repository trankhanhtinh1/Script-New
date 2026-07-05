#pragma once

// ============================================================================
// SDK::GameObjects — snapshot-based object store (EnsoulSharp-modeled).
// ----------------------------------------------------------------------------
// Modeled on EnsoulSharp.SDK.GameObjects (decompiled via ILSpy): objects are
// classified by their RUNTIME TYPE — EnsoulSharp does `sender as AITurretClient
// / BarracksDampenerClient / HQClient`, we match the game's C++ vtable pointer
// (obj+0x0, Offset::StructureVTable) because the client strips RTTI. Model-name
// matching is NEVER used for classification: particles/audio emitters share
// name substrings ("TurretOvergrowth", "SRUAP_Chaos_Inhibitor_Idle1") but have
// different vtables and a smaller layout, and reading deep fields on them is
// what crashed the process.
//
// THREADING MODEL — how we get EnsoulSharp's OnCreate/OnDelete safely:
// EnsoulSharp maintains its sets from GameObject.OnCreate/OnDelete, fired
// synchronously in its managed host. A naive native port subscribed on the
// GAME thread and mutated the shared lists while the render thread iterated
// them — that concurrent mutation corrupted the heap and fail-fasted the
// process (no SEH/VEH/log). This store avoids that WITHOUT giving up the
// events, by only ever touching shared state on ONE thread:
//   * The native create/delete hooks merely ENQUEUE (SDK::Events, under an SRW
//     lock); the queue is drained on the update tick (FlushObjectLifecycle-
//     Events). So GameObjects::OnCreate/OnDelete handlers — and the cache
//     invalidation they trigger — run on the update thread, never concurrently
//     with rendering.
//   * Lists are still rebuilt from the game's own managers under one recursive
//     mutex; a create/delete just invalidates the affected cache so the next
//     accessor reclassifies (the net effect of EnsoulSharp's set add/remove).
//   * Every public accessor returns a COPY taken under that mutex, so no
//     reference to shared storage ever escapes to another thread.
// ============================================================================

#include "ObjectManager.h"
#include "StructureScan.h"
#include "../Events/Events.h" // kept for include-order compatibility (SDK.h)

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

    inline DWORD HeroesTick = 0;
    inline DWORD MinionsTick = 0;
    inline DWORD StructuresTick = 0;
    inline DWORD MissilesTick = 0;
    inline DWORD AggregateTick = 0;

    constexpr DWORD kHeroesCacheMs = 250;
    constexpr DWORD kMinionsCacheMs = 60;
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

    // --------------------------- refreshers --------------------------------
    inline void RefreshHeroes() {
        if (!Expired(HeroesTick, kHeroesCacheMs)) {
            return;
        }
        Lock lk(g_mutex);
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
            HeroesList.push_back(hero);
            const int team = TeamValue(hero);
            if (myTeam != 0 && team == myTeam) {
                AllyHeroesList.push_back(hero);
            } else if (team != 0 && team != 300) {
                EnemyHeroesList.push_back(hero);
            }
        }
    }

    inline void RefreshMinions() {
        if (!Expired(MinionsTick, kMinionsCacheMs)) {
            return;
        }
        Lock lk(g_mutex);
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
                WardsList.push_back(minion);
                if (enemy) {
                    EnemyWardsList.push_back(minion);
                } else {
                    AllyWardsList.push_back(minion);
                }
                continue;
            }

            if (IsPlantObject(minion)) {
                PlantsList.push_back(minion);
                continue;
            }

            if (IsJungleObject(minion)) {
                JungleList.push_back(minion);
                const JungleType jungleType = minion.GetJungleType();
                if (jungleType == JungleType::Small) {
                    JungleSmallList.push_back(minion);
                } else if (jungleType == JungleType::Legendary || jungleType == JungleType::Epic) {
                    JungleLegendaryList.push_back(minion);
                } else {
                    JungleLargeList.push_back(minion);
                }
                continue;
            }

            if (IsCloneObject(minion)) {
                ClonesList.push_back(minion);
                if (enemy) {
                    EnemyClonesList.push_back(minion);
                } else {
                    AllyClonesList.push_back(minion);
                }
                continue;
            }

            if (IsPetObject(minion)) {
                PetsList.push_back(minion);
                if (enemy) {
                    EnemyPetsList.push_back(minion);
                } else {
                    AllyPetsList.push_back(minion);
                }
                continue;
            }

            if (IsSpecialMinionObject(minion)) {
                if (enemy) {
                    EnemySpecialMinionsList.push_back(minion);
                } else {
                    AllySpecialMinionsList.push_back(minion);
                }
                continue;
            }

            if (IsIgnoredMinionObject(minion)) {
                if (enemy) {
                    EnemyIgnoredMinionsList.push_back(minion);
                } else {
                    AllyIgnoredMinionsList.push_back(minion);
                }
                continue;
            }

            if (!IsLaneMinionObject(minion)) {
                continue;
            }

            MinionsList.push_back(minion);
            if (enemy) {
                EnemyMinionsList.push_back(minion);
                EnemyLaneMinionsList.push_back(minion);
            } else {
                AllyMinionsList.push_back(minion);
                AllyLaneMinionsList.push_back(minion);
            }
        }
    }

    // Fountain (shrine) turrets are excluded from the turret lists exactly like
    // EnsoulSharp's `!x.IsFountainTurret()` filter: they are invulnerable and
    // must never be treated as attackable structures.
    inline bool IsFountainTurretName(const char* name) {
        if (!name || !name[0]) {
            return false;
        }
        const std::string lowered = ToLower(name);
        return lowered.find("shrine") != std::string::npos ||
               lowered.find("nexustower") != std::string::npos ||
               lowered.find("fountain") != std::string::npos;
    }

    constexpr int kStructScanMax = 4096;

    inline ::Core::Objects::ObjectHandle HandleFromProbe(
        uintptr_t address,
        const StructureInfo& info,
        ::Core::Objects::ObjectType type) {
        ::Core::Objects::ObjectHandle handle{};
        handle.address = address;
        handle.index = info.index;
        handle.networkId = info.networkId;
        handle.type = type;
        return handle;
    }

    // Turrets / inhibitors / nexus in ONE pass over the object array.
    // Classification is done by vtable inside ProbeStructureGuarded — the
    // native equivalent of EnsoulSharp's `sender as AITurretClient` — so
    // particles and effects are rejected after a single obj+0x0 read.
    inline void RefreshStructures() {
        if (!Expired(StructuresTick, kStructuresCacheMs)) {
            return;
        }
        Lock lk(g_mutex);
        StructuresTick = NowMs();
        InvalidateAggregate();

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

        const int myTeam = PlayerTeam();
        static uintptr_t entries[kStructScanMax]; // guarded by g_mutex
        const int count = ::Core::ObjectManager::EnumerateAll(entries, kStructScanMax);

        StructureInfo info;
        for (int i = 0; i < count; ++i) {
            if (!ProbeStructureGuarded(entries[i], info) ||
                info.kind == StructureKind::None) {
                continue;
            }

            const uintptr_t address = entries[i];
            const int team = static_cast<int>(info.team);
            const bool ally = myTeam != 0 && team == myTeam;
            const bool enemy = team != 0 && team != 300 && !ally;

            switch (info.kind) {
            case StructureKind::Turret: {
                if (IsFountainTurretName(info.name)) {
                    break;
                }
                const AITurretClient turret(
                    HandleFromProbe(address, info, ::Core::Objects::ObjectType::AITurretClient));
                TurretsList.push_back(turret);
                if (ally) {
                    AllyTurretsList.push_back(turret);
                } else if (enemy) {
                    EnemyTurretsList.push_back(turret);
                }
                break;
            }
            case StructureKind::Inhibitor: {
                const BarracksDampenerClient inhibitor(
                    HandleFromProbe(address, info, ::Core::Objects::ObjectType::BarracksDampenerClient));
                InhibitorsList.push_back(inhibitor);
                if (ally) {
                    AllyInhibitorsList.push_back(inhibitor);
                } else if (enemy) {
                    EnemyInhibitorsList.push_back(inhibitor);
                }
                break;
            }
            case StructureKind::Nexus: {
                const HQClient nexus(
                    HandleFromProbe(address, info, ::Core::Objects::ObjectType::HQClient));
                NexusList.push_back(nexus);
                if (ally) {
                    AllyNexusObject = nexus;
                } else if (enemy) {
                    EnemyNexusObject = nexus;
                }
                break;
            }
            default:
                break;
            }
        }
    }

    inline void RefreshMissiles() {
        if (!Expired(MissilesTick, kMissilesCacheMs)) {
            return;
        }
        Lock lk(g_mutex);
        MissilesTick = NowMs();
        InvalidateAggregate();

        MissilesList.clear();
        auto missiles = SDK::ObjectManager::Get<MissileClient>();
        for (auto& missile : missiles) {
            PopulateStatic(missile);
            MissilesList.push_back(missile);
        }
    }

    inline void RefreshAggregate() {
        if (!Expired(AggregateTick, kAggregateCacheMs)) {
            return;
        }
        Lock lk(g_mutex);

        RefreshHeroes();
        RefreshMinions();
        RefreshStructures();

        AggregateTick = NowMs();
        GameObjectsList.clear();
        AttackableUnitsList.clear();
        AllyList.clear();
        EnemyList.clear();

        auto all = SDK::ObjectManager::Get<GameObject>();
        for (auto& object : all) {
            PopulateStatic(object);
            if (!object.IsValid()) {
                continue;
            }
            GameObjectsList.push_back(object);
            if (::Core::Objects::IsAttackable(object.Type())) {
                AttackableUnitsList.push_back(AttackableUnit(object.Handle()));
            }
        }

        auto appendBase = [](std::vector<AIBaseClient>& out, const auto& list) {
            for (const auto& object : list) {
                out.push_back(AIBaseClient(object.Handle()));
            }
        };
        appendBase(AllyList, AllyHeroesList);
        appendBase(AllyList, AllyMinionsList);

        appendBase(EnemyList, EnemyHeroesList);
        appendBase(EnemyList, EnemyMinionsList);
        // Structures are deliberately NOT merged into the aggregate Ally/Enemy
        // base lists: those feed the target selector's generic paths, which are
        // tuned for units. Structure consumers use the dedicated typed accessors
        // (Turrets()/Inhibitors()/Nexuses(), orbwalker GetStructureTarget).
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
        HeroesTick = MinionsTick = StructuresTick = MissilesTick = AggregateTick = 0;
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

    // Invalidate only the caches the (de)created object can affect, so the next
    // typed accessor re-runs classification and the list reflects the change —
    // the net effect EnsoulSharp gets by add/removing from its hash sets. Just
    // a few DWORD writes; the actual rebuild is still paid lazily, at most once
    // per accessor call, so a burst of particle/missile creates costs nothing
    // until something is read.
    inline void InvalidateForType(::Core::Objects::ObjectType type) {
        InvalidateAggregate();
        switch (type) {
        case ::Core::Objects::ObjectType::AIHeroClient:
            HeroesTick = 0;
            break;
        case ::Core::Objects::ObjectType::AIMinionClient:
        case ::Core::Objects::ObjectType::NeutralMinionCampClient:
            MinionsTick = 0;
            break;
        case ::Core::Objects::ObjectType::AITurretClient:
        case ::Core::Objects::ObjectType::BarracksDampenerClient:
        case ::Core::Objects::ObjectType::HQClient:
            StructuresTick = 0;
            break;
        case ::Core::Objects::ObjectType::MissileClient:
            MissilesTick = 0;
            break;
        default:
            // Particles / effects / props: only the aggregate lists can hold
            // them, and those already re-scan on InvalidateAggregate().
            break;
        }
    }

    inline void DispatchLifecycle(std::vector<LifecycleHandler>& source,
                                  const SDK::Events::ObjectEventArgs& args) {
        if (!args.Sender.IsValid()) {
            return;
        }
        std::vector<LifecycleHandler> handlers;
        {
            Lock lk(g_mutex);
            InvalidateForType(args.Sender.Type);
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
        DispatchLifecycle(CreateHandlers, args);
    }

    inline void OnNativeObjectDelete(const SDK::Events::ObjectEventArgs& args) {
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
// All list accessors return copies taken under the store mutex (see header).
inline std::vector<AIHeroClient> Heroes() { EnsureInitialized(); detail::RefreshHeroes(); return detail::Snapshot(detail::HeroesList); }
inline std::vector<AIHeroClient> AllyHeroes() { EnsureInitialized(); detail::RefreshHeroes(); return detail::Snapshot(detail::AllyHeroesList); }
inline std::vector<AIHeroClient> EnemyHeroes() { EnsureInitialized(); detail::RefreshHeroes(); return detail::Snapshot(detail::EnemyHeroesList); }

inline std::vector<AIMinionClient> Minions() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::MinionsList); }
inline std::vector<AIMinionClient> AllyMinions() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::AllyMinionsList); }
inline std::vector<AIMinionClient> EnemyMinions() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::EnemyMinionsList); }
inline std::vector<AIMinionClient> AllyLaneMinions() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::AllyLaneMinionsList); }
inline std::vector<AIMinionClient> EnemyLaneMinions() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::EnemyLaneMinionsList); }
inline std::vector<AIMinionClient> AllySpecialMinions() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::AllySpecialMinionsList); }
inline std::vector<AIMinionClient> EnemySpecialMinions() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::EnemySpecialMinionsList); }
inline std::vector<AIMinionClient> AllyIgnoredMinions() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::AllyIgnoredMinionsList); }
inline std::vector<AIMinionClient> EnemyIgnoredMinions() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::EnemyIgnoredMinionsList); }
inline std::vector<AIMinionClient> Wards() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::WardsList); }
inline std::vector<AIMinionClient> AllyWards() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::AllyWardsList); }
inline std::vector<AIMinionClient> EnemyWards() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::EnemyWardsList); }
inline std::vector<AIMinionClient> Jungle() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::JungleList); }
inline std::vector<AIMinionClient> JungleMinions() { return Jungle(); }
inline std::vector<AIMinionClient> JungleSmall() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::JungleSmallList); }
inline std::vector<AIMinionClient> JungleLarge() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::JungleLargeList); }
inline std::vector<AIMinionClient> JungleLegendary() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::JungleLegendaryList); }
inline std::vector<AIMinionClient> SmallJungle() { return JungleSmall(); }
inline std::vector<AIMinionClient> LargeJungle() { return JungleLarge(); }
inline std::vector<AIMinionClient> EpicJungle() { return JungleLegendary(); }
inline std::vector<AIMinionClient> Plants() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::PlantsList); }
inline std::vector<AIMinionClient> JunglePlants() { return Plants(); }
inline std::vector<AIMinionClient> Clones() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::ClonesList); }
inline std::vector<AIMinionClient> AllyClones() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::AllyClonesList); }
inline std::vector<AIMinionClient> EnemyClones() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::EnemyClonesList); }
inline std::vector<AIMinionClient> Pets() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::PetsList); }
inline std::vector<AIMinionClient> AllyPets() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::AllyPetsList); }
inline std::vector<AIMinionClient> EnemyPets() { EnsureInitialized(); detail::RefreshMinions(); return detail::Snapshot(detail::EnemyPetsList); }

inline std::vector<AITurretClient> Turrets() { EnsureInitialized(); detail::RefreshStructures(); return detail::Snapshot(detail::TurretsList); }
inline std::vector<AITurretClient> AllyTurrets() { EnsureInitialized(); detail::RefreshStructures(); return detail::Snapshot(detail::AllyTurretsList); }
inline std::vector<AITurretClient> EnemyTurrets() { EnsureInitialized(); detail::RefreshStructures(); return detail::Snapshot(detail::EnemyTurretsList); }

inline std::vector<BarracksDampenerClient> Inhibitors() { EnsureInitialized(); detail::RefreshStructures(); return detail::Snapshot(detail::InhibitorsList); }
inline std::vector<BarracksDampenerClient> AllyInhibitors() { EnsureInitialized(); detail::RefreshStructures(); return detail::Snapshot(detail::AllyInhibitorsList); }
inline std::vector<BarracksDampenerClient> EnemyInhibitors() { EnsureInitialized(); detail::RefreshStructures(); return detail::Snapshot(detail::EnemyInhibitorsList); }
inline std::vector<HQClient> Nexuses() { EnsureInitialized(); detail::RefreshStructures(); return detail::Snapshot(detail::NexusList); }
inline HQClient AllyNexus() { EnsureInitialized(); detail::RefreshStructures(); detail::Lock lk(detail::g_mutex); return detail::AllyNexusObject; }
inline HQClient EnemyNexus() { EnsureInitialized(); detail::RefreshStructures(); detail::Lock lk(detail::g_mutex); return detail::EnemyNexusObject; }

// Compatibility aliases (previous API); same snapshot data.
inline std::vector<AITurretClient> ScanTurrets() { return Turrets(); }
inline std::vector<BarracksDampenerClient> ScanInhibitors() { return Inhibitors(); }
inline std::vector<HQClient> ScanNexuses() { return Nexuses(); }

inline std::vector<ShopClient> Shops() { EnsureInitialized(); detail::RefreshStructures(); return detail::Snapshot(detail::ShopsList); }
inline std::vector<ShopClient> AllyShops() { EnsureInitialized(); detail::RefreshStructures(); return detail::Snapshot(detail::AllyShopsList); }
inline std::vector<ShopClient> EnemyShops() { EnsureInitialized(); detail::RefreshStructures(); return detail::Snapshot(detail::EnemyShopsList); }
inline std::vector<Obj_SpawnPoint> SpawnPoints() { EnsureInitialized(); detail::RefreshStructures(); return detail::Snapshot(detail::SpawnPointsList); }
inline std::vector<Obj_SpawnPoint> AllySpawnPoints() { EnsureInitialized(); detail::RefreshStructures(); return detail::Snapshot(detail::AllySpawnPointsList); }
inline std::vector<Obj_SpawnPoint> EnemySpawnPoints() { EnsureInitialized(); detail::RefreshStructures(); return detail::Snapshot(detail::EnemySpawnPointsList); }
inline std::vector<EffectEmitter> ParticleEmitters() { EnsureInitialized(); detail::RefreshStructures(); return detail::Snapshot(detail::ParticleEmittersList); }
inline std::vector<MissileClient> Missiles() { EnsureInitialized(); detail::RefreshMissiles(); return detail::Snapshot(detail::MissilesList); }

inline std::vector<GameObject> AllGameObjects() { EnsureInitialized(); detail::RefreshAggregate(); return detail::Snapshot(detail::GameObjectsList); }
inline std::vector<AttackableUnit> AttackableUnits() { EnsureInitialized(); detail::RefreshAggregate(); return detail::Snapshot(detail::AttackableUnitsList); }
inline std::vector<AIBaseClient> Ally() { EnsureInitialized(); detail::RefreshAggregate(); return detail::Snapshot(detail::AllyList); }
inline std::vector<AIBaseClient> Enemy() { EnsureInitialized(); detail::RefreshAggregate(); return detail::Snapshot(detail::EnemyList); }

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
