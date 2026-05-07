#pragma once

// ============================================================================
// ObjectManager — top-level enumerator namespace (+ legacy `GameObjects::` alias)
// ============================================================================
// Pulls in every GameObjects/*.h leaf and exposes:
//   * `ObjectManager::Player()`                      — local `AIHeroClient`.
//   * `ObjectManager::UnderMouse()`                  — GameObject cursor hit.
//   * `ObjectManager::Heroes / AllyHeroes / EnemyHeroes`
//   * `ObjectManager::AllyMinions / EnemyMinions / JungleMinions / Plants
//     / Pets / Wards / Barrels / SpecialMinions / Clones`
//   * `ObjectManager::AllyTurrets / EnemyTurrets / Missiles / AllObjects`
//   * `ObjectManager::GetByNetId / GetByIndex` for single-entity resolves.
//
// The legacy `namespace GameObjects { … }` aliases at the bottom preserve the
// spelling used by old EnsoulSharp-style scripts (`GameObjects::Heroes()`).
// Both namespaces return the SAME vectors — they are thin forwarders, not
// separate snapshots, so using one or the other has no perf impact.
//
// Enumeration paths:
//   * Heroes / minions / turrets / missiles go through `detail::
//     ReadManagerListLegacy` (reads ObjectManagerInnerLayout lists via raw
//     offsets) or `CoreAPI::Objects::Enumerate*` (SEH-guarded wrappers around
//     the real VMT-walked iterator).
//   * `AllObjects` falls back to `ReadAllObjectsLegacy` which tries
//     `GetFirstObject / GetNextObject` first, then a raw array read as a
//     safety net on builds where the VMT iterator was stripped.
//   * Classification into "is this minion a lane minion / jungle / plant /
//     pet / ward" lives in `detail::Is*Object` helpers; they combine
//     `RuntimeAPI` pattern matches with name sniffing to handle the long
//     tail of special summons (Tibbers, Shaco boxes, Yorick ghouls, …).
// ============================================================================

#include "../../core/CoreAPI.h"
#include "../../core/CoreBypass.h"
#include "../../core/Globals.h"
#include "../../core/offset.h"
#include "AIHeroClient.h"
#include "AIMinionClient.h"
#include "AITurretClient.h"
#include "MissileClient.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace SDK {
namespace ObjectManager {

namespace detail {

inline int ReadManagerListLegacy(uintptr_t globalRva, uintptr_t* out, int maxOut, int hardCap) {
    if (!out || maxOut <= 0 || !Globals::base || !globalRva) {
        return 0;
    }

    const uintptr_t mgr = Globals::Read<uintptr_t>(Globals::base + globalRva);
    if (!Globals::IsValidPtr(mgr)) {
        return 0;
    }

    const uintptr_t list = Globals::Read<uintptr_t>(mgr + 0x8);
    const int count = Globals::Read<int>(mgr + 0x10);
    if (!Globals::IsValidPtr(list) || count <= 0) {
        return 0;
    }

    const int safeCount = std::min(count, std::min(maxOut, hardCap));
    if (safeCount <= 0) {
        return 0;
    }

    return Globals::ReadPtrArray(list, safeCount, out, maxOut);
}

inline int ReadAllObjectsLegacy(uintptr_t* out, int maxOut) {
    if (!out || maxOut <= 0 || !Globals::base) {
        return 0;
    }

    const uintptr_t mgr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::ObjectManager);
    if (!Globals::IsValidPtr(mgr)) {
        return 0;
    }

    using fnGetFirst = uintptr_t(__cdecl*)(uintptr_t);
    using fnGetNext = uintptr_t(__cdecl*)(uintptr_t, uintptr_t);

    const auto getFirst = reinterpret_cast<fnGetFirst>(Globals::base + Offset::Function::GetFirstObject);
    const auto getFirstAlt = reinterpret_cast<fnGetFirst>(Globals::base + Offset::Function::GetFirstObjectAlt);
    const auto getNext = reinterpret_cast<fnGetNext>(Globals::base + Offset::Function::GetNextObject);

    auto iterateFrom = [&](fnGetFirst starter) -> int {
        if (!starter || !getNext) {
            return 0;
        }

        int count = 0;
        __try {
            CoreBypass::MainloopCheck();
            uintptr_t obj = starter(mgr);
            while (Globals::IsValidPtr(obj) && count < maxOut) {
                out[count++] = obj;
                obj = getNext(mgr, obj);
            }
        }
        __except (1) {
            return count;
        }
        return count;
    };

    int count = iterateFrom(getFirst);
    if (count == 0 && getFirstAlt && getFirstAlt != getFirst) {
        count = iterateFrom(getFirstAlt);
    }
    if (count > 0) {
        return count;
    }

    const uintptr_t list = Globals::Read<uintptr_t>(mgr + 0x8);
    const int listCount = Globals::Read<int>(mgr + 0x10);
    if (!Globals::IsValidPtr(list) || listCount <= 0) {
        return 0;
    }

    const int safeCount = std::min(listCount, maxOut);
    return Globals::ReadPtrArray(list, safeCount, out, maxOut);
}

inline std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

inline std::string BestName(const GameObject& obj) {
    std::string name = obj.CharacterName();
    if (name.empty()) {
        name = obj.Name();
    }
    return ToLower(std::move(name));
}

inline bool IsJunglePlantName(const std::string& lowerName) {
    return lowerName.find("sru_plant") != std::string::npos ||
           lowerName.find("hiddenminionplantdemon") != std::string::npos ||
           lowerName.find("planthealthmirrored") != std::string::npos ||
           lowerName.find("plantmasterminion") != std::string::npos ||
           lowerName.find("minimapicon") != std::string::npos;
}

inline bool IsKnownJungleMonsterName(const std::string& lowerName) {
    static const char* known[] = {
        "sru_baron", "sru_dragon", "sru_riftherald", "voidgrub", "sru_atakhan",
        "sru_blue", "sru_red", "sru_gromp", "sru_krug", "sru_murkwolf",
        "sru_razorbeak", "sru_crab", "sru_riftscuttler"
    };

    if (IsJunglePlantName(lowerName)) {
        return false;
    }

    for (const auto* token : known) {
        if (lowerName.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

inline bool IsLaneMinionName(const std::string& lowerName) {
    return lowerName.find("sru_chaosminion") != std::string::npos ||
           lowerName.find("sru_orderminion") != std::string::npos ||
           lowerName.find("ha_chaosminion") != std::string::npos ||
           lowerName.find("ha_orderminion") != std::string::npos;
}

inline bool IsLaneMinionObject(const AIMinionClient& obj) {
    if (!obj.IsValid() || !obj.IsAlive()) {
        return false;
    }

    if (obj.Team() == 300) {
        return false;
    }

    const float maxHP = obj.MaxHealth();
    if (maxHP <= 0.0f || maxHP >= 10000.0f) {
        return false;
    }

    const std::string lowerName = BestName(obj);
    if (IsJunglePlantName(lowerName) || IsKnownJungleMonsterName(lowerName)) {
        return false;
    }

    if (IsLaneMinionName(lowerName)) {
        return true;
    }

    if (obj.IsHero() || obj.IsTurret()) {
        return false;
    }

    if (obj.IsPlant() || obj.IsPet() || obj.IsJungleMonster()) {
        return false;
    }

    return obj.IsLaneMinion() || obj.IsMinion();
}

inline bool IsPlantObject(const AIMinionClient& obj) {
    if (!obj.IsValid() || !obj.IsAlive()) {
        return false;
    }

    const float maxHP = obj.MaxHealth();
    const std::string lowerName = BestName(obj);
    return obj.IsPlant() || IsJunglePlantName(lowerName) || (maxHP > 0.0f && maxHP <= 6.0f);
}

inline bool IsJungleObject(const AIMinionClient& obj) {
    if (!obj.IsValid() || !obj.IsAlive()) {
        return false;
    }

    if (obj.Team() != 300) {
        return false;
    }

    if (IsPlantObject(obj)) {
        return false;
    }

    const float maxHP = obj.MaxHealth();
    if (maxHP <= 6.0f) {
        return false;
    }

    const std::string lowerName = BestName(obj);
    return obj.IsJungleMonster() || IsKnownJungleMonsterName(lowerName);
}

inline bool IsPetObject(const AIMinionClient& obj) {
    if (!obj.IsValid() || !obj.IsAlive()) {
        return false;
    }

    if (obj.IsPlant()) {
        return false;
    }

    return obj.IsPet();
}

inline bool IsWardObject(const AIMinionClient& obj) {
    if (!obj.IsValid() || !obj.IsAlive()) {
        return false;
    }
    const std::string lowerName = BestName(obj);
    return lowerName.find("ward") != std::string::npos ||
           lowerName.find("jammerdevice") != std::string::npos;
}

inline bool IsBarrelObject(const AIMinionClient& obj) {
    if (!obj.IsValid() || !obj.IsAlive()) {
        return false;
    }
    const std::string lowerName = BestName(obj);
    return lowerName.find("gangplankbarrel") != std::string::npos;
}

inline bool IsSpecialMinionObject(const AIMinionClient& obj) {
    if (!obj.IsValid() || !obj.IsAlive()) {
        return false;
    }
    const std::string lowerName = BestName(obj);
    static const char* specials[] = {
        "annietibbers", "elisespiderling", "heimertyellow", "heimertblue",
        "ivernminion", "malzaharvoidling", "shacobox", "teemomushroom",
        "yorickghoulmelee", "yorickbigghoul", "zyrathornplant", "zyragraspingplant"
    };
    for (const auto& s : specials) {
        if (lowerName == s) return true;
    }
    return false;
}

inline bool IsCloneObject(const AIMinionClient& obj) {
    if (!obj.IsValid() || !obj.IsAlive()) {
        return false;
    }
    const std::string lowerName = BestName(obj);
    static const char* clones[] = { "leblanc", "monkeyking", "neeko", "shaco" };
    for (const auto& c : clones) {
        if (lowerName == c) return true;
    }
    return false;
}

inline bool ContainsAddress(const std::vector<AIMinionClient>& list, uintptr_t address) {
    return std::any_of(list.begin(), list.end(), [address](const AIMinionClient& obj) {
        return obj.Address() == address;
    });
}

} // namespace detail

inline AIHeroClient Player() {
    return AIHeroClient(CoreAPI::Objects::GetLocalPlayer());
}

inline GameObject UnderMouse() {
    return GameObject(CoreAPI::Objects::GetUnderMouseObject());
}

inline std::vector<AIHeroClient> Heroes() {
    uintptr_t buffer[64] = {};
    const int count = detail::ReadManagerListLegacy(Offset::Global::HeroManager, buffer, 64, 64);
    std::vector<AIHeroClient> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        AIHeroClient hero(buffer[i]);
        if (!hero.IsValid()) {
            continue;
        }
        out.emplace_back(buffer[i]);
    }
    return out;
}

inline GameObject GetByNetId(int netId) {
    if (netId <= 0) {
        return GameObject();
    }

    uintptr_t buffer[4096] = {};
    const int count = detail::ReadAllObjectsLegacy(buffer, 4096);
    for (int i = 0; i < count; ++i) {
        GameObject obj(buffer[i]);
        if (obj.IsValid() && obj.NetworkId() == netId) {
            return obj;
        }
    }
    return GameObject();
}

inline GameObject GetByIndex(int index) {
    if (index <= 0) {
        return GameObject();
    }

    uintptr_t buffer[4096] = {};
    const int count = detail::ReadAllObjectsLegacy(buffer, 4096);
    for (int i = 0; i < count; ++i) {
        GameObject obj(buffer[i]);
        if (obj.IsValid() && obj.Index() == index) {
            return obj;
        }
    }
    return GameObject();
}

inline std::vector<AIHeroClient> AllyHeroes() {
    const int myTeam = Player().Team();
    uintptr_t buffer[64] = {};
    const int count = detail::ReadManagerListLegacy(Offset::Global::HeroManager, buffer, 64, 64);
    std::vector<AIHeroClient> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        AIHeroClient hero(buffer[i]);
        if (!hero.IsValid() || hero.Team() != myTeam) {
            continue;
        }
        out.emplace_back(buffer[i]);
    }
    return out;
}

inline std::vector<AIHeroClient> EnemyHeroes() {
    const int myTeam = Player().Team();
    uintptr_t buffer[64] = {};
    const int count = detail::ReadManagerListLegacy(Offset::Global::HeroManager, buffer, 64, 64);
    std::vector<AIHeroClient> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        AIHeroClient hero(buffer[i]);
        if (!hero.IsValid() || hero.Team() == myTeam || hero.Team() == 300) {
            continue;
        }
        out.emplace_back(buffer[i]);
    }
    return out;
}

// ── AllyMinions — use RuntimeAPI directly (matches old NightSharp) ──
// Old NightSharp: FillMinions() returns ALL from MinionManager, then
// caller checks obj.IsLaneMinion() which delegates to CoreClassification::Classify()
inline std::vector<AIMinionClient> AllyMinions() {
    static std::vector<AIMinionClient> s_cache = {};
    static DWORD s_lastCacheTick = 0;
    const DWORD now = GetTickCount();
    if (now - s_lastCacheTick < 60) {
        return s_cache;
    }
    s_lastCacheTick = now;

    const int myTeam = Player().Team();
    uintptr_t buffer[512] = {};
    const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
    std::vector<AIMinionClient> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        if (!Globals::IsValidPtr(buffer[i])) continue;
        AIMinionClient minion(buffer[i]);
        if (!detail::IsLaneMinionObject(minion)) continue;
        if (minion.Team() != myTeam) continue;
        out.emplace_back(buffer[i]);
    }

    if (out.empty()) {
        uintptr_t all[4096] = {};
        const int allCount = detail::ReadAllObjectsLegacy(all, 4096);
        for (int i = 0; i < allCount; ++i) {
            if (!Globals::IsValidPtr(all[i]) || detail::ContainsAddress(out, all[i])) continue;
            AIMinionClient minion(all[i]);
            if (!detail::IsLaneMinionObject(minion)) continue;
            if (minion.Team() != myTeam) continue;
            out.emplace_back(all[i]);
        }
    }
    s_cache = out;
    return out;
}

inline std::vector<AIMinionClient> EnemyMinions() {
    static std::vector<AIMinionClient> s_cache = {};
    static DWORD s_lastCacheTick = 0;
    const DWORD now = GetTickCount();
    if (now - s_lastCacheTick < 60) {
        return s_cache;
    }
    s_lastCacheTick = now;

    const int myTeam = Player().Team();
    uintptr_t buffer[512] = {};
    const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
    std::vector<AIMinionClient> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        if (!Globals::IsValidPtr(buffer[i])) continue;
        AIMinionClient minion(buffer[i]);
        if (!detail::IsLaneMinionObject(minion)) continue;
        const int team = minion.Team();
        if (team == myTeam || team == 300) continue;
        out.emplace_back(buffer[i]);
    }

    if (out.empty()) {
        uintptr_t all[4096] = {};
        const int allCount = detail::ReadAllObjectsLegacy(all, 4096);
        for (int i = 0; i < allCount; ++i) {
            if (!Globals::IsValidPtr(all[i]) || detail::ContainsAddress(out, all[i])) continue;
            AIMinionClient minion(all[i]);
            if (!detail::IsLaneMinionObject(minion)) continue;
            const int team = minion.Team();
            if (team == myTeam || team == 300) continue;
            out.emplace_back(all[i]);
        }
    }
    s_cache = out;
    return out;
}

inline std::vector<AIMinionClient> JungleMinions() {
    uintptr_t buffer[512] = {};
    const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
    std::vector<AIMinionClient> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        if (!Globals::IsValidPtr(buffer[i])) continue;
        const auto t = CoreClassification::Classify(buffer[i]);
        if (t != CoreClassification::ObjectType::JungleMonster
            && t != CoreClassification::ObjectType::JungleBig
            && t != CoreClassification::ObjectType::JungleEpic
            && t != CoreClassification::ObjectType::Scuttle) continue;
        AIMinionClient minion(buffer[i]);
        if (!minion.IsAlive()) continue;
        out.emplace_back(buffer[i]);
    }
    return out;
}

// ── Tier-filtered jungle enumerators ────────────────────────────────────────
// Each helper walks the JungleMinions() list once and filters by
// `AIMinionClient::GetJungleTier()`. Cheap enough to call every frame — the
// underlying pointer array is always bounded by the map size (≈ 40 camps).
inline std::vector<AIMinionClient> JungleMinionsByTier(JungleTier tier) {
    auto camps = JungleMinions();
    std::vector<AIMinionClient> out;
    out.reserve(camps.size());
    for (auto& m : camps) {
        if (m.GetJungleTier() == tier) {
            out.emplace_back(m);
        }
    }
    return out;
}

inline std::vector<AIMinionClient> SmallJungleMinions() { return JungleMinionsByTier(JungleTier::Small); }
inline std::vector<AIMinionClient> LargeJungleMinions() { return JungleMinionsByTier(JungleTier::Large); }
inline std::vector<AIMinionClient> EpicJungleMinions() { return JungleMinionsByTier(JungleTier::Epic); }

inline std::vector<AIMinionClient> Plants() {
    uintptr_t buffer[512] = {};
    const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
    std::vector<AIMinionClient> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        AIMinionClient minion(buffer[i]);
        if (!detail::IsPlantObject(minion)) {
            continue;
        }
        out.emplace_back(buffer[i]);
    }
    return out;
}

inline std::vector<AIMinionClient> Pets() {
    uintptr_t buffer[512] = {};
    const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
    std::vector<AIMinionClient> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        AIMinionClient minion(buffer[i]);
        if (!detail::IsPetObject(minion)) {
            continue;
        }
        out.emplace_back(buffer[i]);
    }
    return out;
}

inline std::vector<AIMinionClient> Wards() {
    uintptr_t buffer[512] = {};
    const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
    std::vector<AIMinionClient> out;
    out.reserve(16);
    for (int i = 0; i < count; ++i) {
        AIMinionClient minion(buffer[i]);
        if (!detail::IsWardObject(minion)) {
            continue;
        }
        out.emplace_back(buffer[i]);
    }
    return out;
}

inline std::vector<AIMinionClient> Barrels() {
    uintptr_t buffer[512] = {};
    const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
    std::vector<AIMinionClient> out;
    out.reserve(8);
    for (int i = 0; i < count; ++i) {
        AIMinionClient minion(buffer[i]);
        if (!detail::IsBarrelObject(minion)) {
            continue;
        }
        out.emplace_back(buffer[i]);
    }
    return out;
}

inline std::vector<AITurretClient> AllyTurrets() {
    uintptr_t buffer[64] = {};
    const int count = CoreAPI::Objects::EnumerateAllyTurrets(buffer, 64);
    std::vector<AITurretClient> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        out.emplace_back(buffer[i]);
    }
    return out;
}

inline std::vector<AIMinionClient> SpecialMinions() {
    uintptr_t buffer[512] = {};
    const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
    std::vector<AIMinionClient> out;
    out.reserve(16);
    const int myTeam = Player().Team();
    for (int i = 0; i < count; ++i) {
        AIMinionClient minion(buffer[i]);
        if (!detail::IsSpecialMinionObject(minion)) continue;
        if (minion.Team() == myTeam) continue;  // enemy specials only
        out.emplace_back(buffer[i]);
    }
    return out;
}

inline std::vector<AIMinionClient> Clones() {
    uintptr_t buffer[512] = {};
    const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
    std::vector<AIMinionClient> out;
    out.reserve(8);
    const int myTeam = Player().Team();
    for (int i = 0; i < count; ++i) {
        AIMinionClient minion(buffer[i]);
        if (!detail::IsCloneObject(minion)) continue;
        if (minion.Team() == myTeam) continue;  // enemy clones only
        out.emplace_back(buffer[i]);
    }
    return out;
}

inline std::vector<AITurretClient> EnemyTurrets() {
    uintptr_t buffer[64] = {};
    const int count = CoreAPI::Objects::EnumerateEnemyTurrets(buffer, 64);
    std::vector<AITurretClient> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        out.emplace_back(buffer[i]);
    }
    return out;
}

// ── Phase 2 (Apr 26/2026) SDK stubs ──────────────────────────────────────
// Orbwalker iterates EnemyInhibitors / EnemyNexus after turrets. These objects
// can be present in MinionManager or the global object iterator depending on
// game state, so both paths are scanned and cached briefly.
inline std::vector<AITurretClient> EnemyInhibitors() {
    static std::vector<AITurretClient> s_cache = {};
    static DWORD s_lastCacheTick = 0;
    const DWORD now = GetTickCount();
    if (now - s_lastCacheTick < 250) {
        return s_cache;
    }
    s_lastCacheTick = now;

    const int myTeam = Player().Team();
    std::vector<AITurretClient> out;
    out.reserve(3);

    auto appendCandidate = [&](uintptr_t address) {
        if (!Globals::IsValidPtr(address)) return;
        if (std::any_of(out.begin(), out.end(), [address](const AITurretClient& obj) {
            return obj.Address() == address;
        })) {
            return;
        }

        GameObject obj(address);
        if (!obj.IsValid() || obj.Team() == myTeam || obj.Team() == 300) return;
        if (CoreClassification::Classify(address) != CoreClassification::ObjectType::Inhibitor) return;
        out.emplace_back(address);
    };

    uintptr_t minionBuffer[512] = {};
    const int minionCount = detail::ReadManagerListLegacy(Offset::Global::MinionManager, minionBuffer, 512, 2000);
    for (int i = 0; i < minionCount; ++i) {
        appendCandidate(minionBuffer[i]);
    }

    uintptr_t allBuffer[4096] = {};
    const int allCount = detail::ReadAllObjectsLegacy(allBuffer, 4096);
    for (int i = 0; i < allCount; ++i) {
        appendCandidate(allBuffer[i]);
    }

    s_cache = out;
    return s_cache;
}
// Single enemy nexus (per side). Real implementation would scan AllObjects
// for the nexus name; default-constructed returns IsValid()==false so the
// orbwalker structure-target loop skips this branch cleanly.
inline AITurretClient EnemyNexus() {
    static AITurretClient s_cache = {};
    static DWORD s_lastCacheTick = 0;
    const DWORD now = GetTickCount();
    if (now - s_lastCacheTick < 250) {
        return s_cache;
    }
    s_lastCacheTick = now;

    const int myTeam = Player().Team();
    s_cache = {};

    auto tryCandidate = [&](uintptr_t address) -> bool {
        if (!Globals::IsValidPtr(address)) return false;
        GameObject obj(address);
        if (!obj.IsValid() || obj.Team() == myTeam || obj.Team() == 300) return false;
        if (CoreClassification::Classify(address) != CoreClassification::ObjectType::Nexus) return false;
        s_cache = AITurretClient(address);
        return true;
    };

    uintptr_t minionBuffer[512] = {};
    const int minionCount = detail::ReadManagerListLegacy(Offset::Global::MinionManager, minionBuffer, 512, 2000);
    for (int i = 0; i < minionCount; ++i) {
        if (tryCandidate(minionBuffer[i])) return s_cache;
    }

    uintptr_t allBuffer[4096] = {};
    const int allCount = detail::ReadAllObjectsLegacy(allBuffer, 4096);
    for (int i = 0; i < allCount; ++i) {
        if (tryCandidate(allBuffer[i])) return s_cache;
    }

    return s_cache;
}

inline std::vector<MissileClient> Missiles() {
    uintptr_t buffer[1024] = {};
    const int count = CoreAPI::Objects::EnumerateMissiles(buffer, 1024);
    std::vector<MissileClient> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        out.emplace_back(buffer[i]);
    }
    return out;
}

inline std::vector<GameObject> AllObjects() {
    uintptr_t buffer[4096] = {};
    const int count = detail::ReadAllObjectsLegacy(buffer, 4096);
    std::vector<GameObject> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        out.emplace_back(buffer[i]);
    }
    return out;
}

} // namespace ObjectManager

// Legacy alias namespace — forwards every call to ObjectManager:: above.
namespace GameObjects {
    inline AIHeroClient Player() { return ObjectManager::Player(); }
    inline std::vector<AIHeroClient> Heroes() { return ObjectManager::Heroes(); }
    inline std::vector<AIHeroClient> AllyHeroes() { return ObjectManager::AllyHeroes(); }
    inline std::vector<AIHeroClient> EnemyHeroes() { return ObjectManager::EnemyHeroes(); }
    inline std::vector<AIMinionClient> AllyMinions() { return ObjectManager::AllyMinions(); }
    inline std::vector<AIMinionClient> EnemyMinions() { return ObjectManager::EnemyMinions(); }
    inline std::vector<AIMinionClient> Jungle() { return ObjectManager::JungleMinions(); }
    inline std::vector<AIMinionClient> JungleMinions() { return ObjectManager::JungleMinions(); }
    inline std::vector<AIMinionClient> SmallJungle() { return ObjectManager::SmallJungleMinions(); }
    inline std::vector<AIMinionClient> LargeJungle() { return ObjectManager::LargeJungleMinions(); }
    inline std::vector<AIMinionClient> EpicJungle() { return ObjectManager::EpicJungleMinions(); }
    inline std::vector<AIMinionClient> Plants() { return ObjectManager::Plants(); }
    inline std::vector<AIMinionClient> JunglePlants() { return ObjectManager::Plants(); }
    inline std::vector<AIMinionClient> Pets() { return ObjectManager::Pets(); }
    inline std::vector<AIMinionClient> Wards() { return ObjectManager::Wards(); }
    inline std::vector<AIMinionClient> Barrels() { return ObjectManager::Barrels(); }
    inline std::vector<AIMinionClient> SpecialMinions() { return ObjectManager::SpecialMinions(); }
    inline std::vector<AIMinionClient> Clones() { return ObjectManager::Clones(); }
    inline std::vector<AITurretClient> AllyTurrets() { return ObjectManager::AllyTurrets(); }
    inline std::vector<AITurretClient> EnemyTurrets() { return ObjectManager::EnemyTurrets(); }
    inline std::vector<MissileClient> Missiles() { return ObjectManager::Missiles(); }
    inline std::vector<GameObject> AllObjects() { return ObjectManager::AllObjects(); }
}

} // namespace SDK
