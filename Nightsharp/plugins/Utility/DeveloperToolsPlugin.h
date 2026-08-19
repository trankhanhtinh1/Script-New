#pragma once

#include "../IPlugin.h"
#include "../../SDK/SDK.h"

#include "../../DebugLog.h"
#include "../../imgui/imgui.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <memory>

#include "DeveloperTools/IDeveloperTab.h"

namespace Plugins {

enum class GameObjectListType : int {
    UseCategoryFilters = 0,
    AllGameObjects,
    AttackableUnits,
    Ally,
    Enemy,
    Heroes,
    AllyHeroes,
    EnemyHeroes,
    Minions,
    AllyMinions,
    EnemyMinions,
    AllyLaneMinions,
    EnemyLaneMinions,
    AllySpecialMinions,
    EnemySpecialMinions,
    AllyIgnoredMinions,
    EnemyIgnoredMinions,
    Wards,
    AllyWards,
    EnemyWards,
    Jungle,
    JungleSmall,
    JungleLarge,
    JungleLegendary,
    Plants,
    Clones,
    AllyClones,
    EnemyClones,
    Pets,
    AllyPets,
    EnemyPets,
    Turrets,
    AllyTurrets,
    EnemyTurrets,
    Inhibitors,
    AllyInhibitors,
    EnemyInhibitors,
    Nexuses,
    AllyNexus,
    EnemyNexus,
    Shops,
    AllyShops,
    EnemyShops,
    SpawnPoints,
    AllySpawnPoints,
    EnemySpawnPoints,
    ParticleEmitters,
    Missiles,
    Player
};

} // namespace Plugins

// Forward declare IDeveloperTab
namespace Plugins::DevTools {
    class IDeveloperTab;
}

namespace Plugins {

class DeveloperToolsPlugin final : public IPlugin {
public:
    // Make variables referenced by sub-tabs public
    bool enabled_ = true;
    int maxRange_ = 400;
    int scanProviderIndex_ = 0; // 0 = ObjectManager, 1 = GameObjects Facade
    int lastFocusedScanTick_ = 0;
    int lastSnapshotTick_ = 0;
    
    struct ListOption {
        const char* Name;
        const char* DisplayName;
        GameObjectListType Type;
        bool Enabled;
        SDK::UI::MenuBool* MenuControl;
    };

    mutable ListOption listOptions_[48] = {
        { "AllGameObjects", "All Game Objects (AllGameObjects)", GameObjectListType::AllGameObjects, false, nullptr },
        { "AttackableUnits", "Attackable Units (AttackableUnits)", GameObjectListType::AttackableUnits, false, nullptr },
        { "Ally", "Allies (Ally)", GameObjectListType::Ally, false, nullptr },
        { "Enemy", "Enemies (Enemy)", GameObjectListType::Enemy, false, nullptr },
        { "Heroes", "Heroes (Heroes)", GameObjectListType::Heroes, false, nullptr },
        { "AllyHeroes", "Ally Heroes (AllyHeroes)", GameObjectListType::AllyHeroes, false, nullptr },
        { "EnemyHeroes", "Enemy Heroes (EnemyHeroes)", GameObjectListType::EnemyHeroes, false, nullptr },
        { "Minions", "Minions (Minions)", GameObjectListType::Minions, false, nullptr },
        { "AllyMinions", "Ally Minions (AllyMinions)", GameObjectListType::AllyMinions, false, nullptr },
        { "EnemyMinions", "Enemy Minions (EnemyMinions)", GameObjectListType::EnemyMinions, false, nullptr },
        { "AllyLaneMinions", "Ally Lane Minions (AllyLaneMinions)", GameObjectListType::AllyLaneMinions, false, nullptr },
        { "EnemyLaneMinions", "Enemy Lane Minions (EnemyLaneMinions)", GameObjectListType::EnemyLaneMinions, false, nullptr },
        { "AllySpecialMinions", "Ally Special Minions (AllySpecialMinions)", GameObjectListType::AllySpecialMinions, false, nullptr },
        { "EnemySpecialMinions", "Enemy Special Minions (EnemySpecialMinions)", GameObjectListType::EnemySpecialMinions, false, nullptr },
        { "AllyIgnoredMinions", "Ally Ignored Minions (AllyIgnoredMinions)", GameObjectListType::AllyIgnoredMinions, false, nullptr },
        { "EnemyIgnoredMinions", "Enemy Ignored Minions (EnemyIgnoredMinions)", GameObjectListType::EnemyIgnoredMinions, false, nullptr },
        { "Wards", "Wards (Wards)", GameObjectListType::Wards, false, nullptr },
        { "AllyWards", "Ally Wards (AllyWards)", GameObjectListType::AllyWards, false, nullptr },
        { "EnemyWards", "Enemy Wards (EnemyWards)", GameObjectListType::EnemyWards, false, nullptr },
        { "Jungle", "Jungle Minions (Jungle)", GameObjectListType::Jungle, false, nullptr },
        { "JungleSmall", "Jungle Small (JungleSmall)", GameObjectListType::JungleSmall, false, nullptr },
        { "JungleLarge", "Jungle Large (JungleLarge)", GameObjectListType::JungleLarge, false, nullptr },
        { "JungleLegendary", "Jungle Legendary (JungleLegendary)", GameObjectListType::JungleLegendary, false, nullptr },
        { "Plants", "Plants (Plants)", GameObjectListType::Plants, false, nullptr },
        { "Clones", "Clones (Clones)", GameObjectListType::Clones, false, nullptr },
        { "AllyClones", "Ally Clones (AllyClones)", GameObjectListType::AllyClones, false, nullptr },
        { "EnemyClones", "Enemy Clones (EnemyClones)", GameObjectListType::EnemyClones, false, nullptr },
        { "Pets", "Pets (Pets)", GameObjectListType::Pets, false, nullptr },
        { "AllyPets", "Ally Pets (AllyPets)", GameObjectListType::AllyPets, false, nullptr },
        { "EnemyPets", "Enemy Pets (EnemyPets)", GameObjectListType::EnemyPets, false, nullptr },
        { "Turrets", "Turrets (Turrets)", GameObjectListType::Turrets, false, nullptr },
        { "AllyTurrets", "Ally Turrets (AllyTurrets)", GameObjectListType::AllyTurrets, false, nullptr },
        { "EnemyTurrets", "Enemy Turrets (EnemyTurrets)", GameObjectListType::EnemyTurrets, false, nullptr },
        { "Inhibitors", "Inhibitors (Inhibitors)", GameObjectListType::Inhibitors, false, nullptr },
        { "AllyInhibitors", "Ally Inhibitors (AllyInhibitors)", GameObjectListType::AllyInhibitors, false, nullptr },
        { "EnemyInhibitors", "Enemy Inhibitors (EnemyInhibitors)", GameObjectListType::EnemyInhibitors, false, nullptr },
        { "Nexuses", "Nexuses (Nexuses)", GameObjectListType::Nexuses, false, nullptr },
        { "AllyNexus", "Ally Nexus (AllyNexus)", GameObjectListType::AllyNexus, false, nullptr },
        { "EnemyNexus", "Enemy Nexus (EnemyNexus)", GameObjectListType::EnemyNexus, false, nullptr },
        { "Shops", "Shops (Shops)", GameObjectListType::Shops, false, nullptr },
        { "AllyShops", "Ally Shops (AllyShops)", GameObjectListType::AllyShops, false, nullptr },
        { "EnemyShops", "Enemy Shops (EnemyShops)", GameObjectListType::EnemyShops, false, nullptr },
        { "SpawnPoints", "Spawn Points (SpawnPoints)", GameObjectListType::SpawnPoints, false, nullptr },
        { "AllySpawnPoints", "Ally Spawn Points (AllySpawnPoints)", GameObjectListType::AllySpawnPoints, false, nullptr },
        { "EnemySpawnPoints", "Enemy Spawn Points (EnemySpawnPoints)", GameObjectListType::EnemySpawnPoints, false, nullptr },
        { "ParticleEmitters", "Particle Emitters (ParticleEmitters)", GameObjectListType::ParticleEmitters, false, nullptr },
        { "Missiles", "Missiles (Missiles)", GameObjectListType::Missiles, false, nullptr },
        { "Player", "Player Object (Player)", GameObjectListType::Player, false, nullptr }
    };

    bool scanRawGameObjects_ = true;
    bool scanHeroes_ = true;
    bool scanMinions_ = true;
    bool scanTurrets_ = false;
    bool scanMissiles_ = true;
    bool filterClutter_ = true;
    std::unordered_map<std::uint32_t, int> trackedObjectTicks_;
    mutable std::mutex trackedObjectTicksMutex_;
    bool pKeyPressedLast_ = false;

    bool logEnabled_ = true;
    int  logSourceIndex_ = 0;
    bool logVerbose_ = true;
    bool logRaw_ = false;
    bool logSkipAutoAttacks_ = false;
    bool logToFile_ = true;
    bool logProcessSpell_ = true;
    bool logDoCast_ = true;
    bool logFinishCast_ = false;
    bool logSpellImpact_ = false;
    bool logCastSpell_ = false;
    bool logStopCast_ = false;
    bool logAnimation_ = false;
    bool logBuffAdd_ = false;
    bool logBuffRemove_ = false;
    bool logNewPath_ = false;
    char logNameFilter_[64] = {};
    int activeTabIdx_ = 0;
    std::vector<DevTools::ObjectSnapshot> snapshots_;
    std::set<std::uint32_t> openInspectWindows_;
    std::set<std::uint32_t> openEventLogWindows_;
    std::unordered_map<std::uint32_t, std::vector<DevTools::EventLogEntry>> objectEventLogs_;
    mutable std::mutex objectEventLogsMutex_;
    bool mKeyPressedLast_ = false;
    SDK::GameObject focusedObject_;
    mutable std::mutex focusedObjectMutex_;

    float TrackObjectAndGetAge(std::uint32_t netId, int now) {
        std::lock_guard<std::mutex> lock(trackedObjectTicksMutex_);
        const auto [it, inserted] = trackedObjectTicks_.try_emplace(netId, now);
        return inserted ? 0.0f : static_cast<float>(now - it->second) / 1000.0f;
    }

    void ForgetTrackedObject(std::uint32_t netId) {
        std::lock_guard<std::mutex> lock(trackedObjectTicksMutex_);
        trackedObjectTicks_.erase(netId);
    }

    void ClearTrackedObjects() {
        std::lock_guard<std::mutex> lock(trackedObjectTicksMutex_);
        trackedObjectTicks_.clear();
    }

    void PruneTrackedObjects(int now) {
        std::lock_guard<std::mutex> lock(trackedObjectTicksMutex_);
        if (trackedObjectTicks_.size() <= 128) return;
        for (auto it = trackedObjectTicks_.begin(); it != trackedObjectTicks_.end();) {
            if (now - it->second > 15000) {
                it = trackedObjectTicks_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void LogEventForObject(std::uint32_t netId, const std::string& eventName, const std::string& details) {
        const int tick = SDK::Game::TickCount();
        const float time = SDK::Game::Time();
        std::lock_guard<std::mutex> lock(objectEventLogsMutex_);
        auto it = objectEventLogs_.find(netId);
        if (it != objectEventLogs_.end()) {
            DevTools::EventLogEntry entry;
            entry.tick = tick;
            entry.time = time;
            entry.eventName = eventName;
            entry.details = details;
            
            it->second.push_back(entry);
            if (it->second.size() > 500) {
                it->second.erase(it->second.begin());
            }
        }
    }

    bool IsEventLogOpen(std::uint32_t netId) const {
        std::lock_guard<std::mutex> lock(objectEventLogsMutex_);
        return openEventLogWindows_.find(netId) != openEventLogWindows_.end();
    }

    void SetEventLogOpen(std::uint32_t netId, bool open) {
        std::lock_guard<std::mutex> lock(objectEventLogsMutex_);
        if (open) {
            openEventLogWindows_.insert(netId);
            objectEventLogs_[netId].clear();
        } else {
            openEventLogWindows_.erase(netId);
            objectEventLogs_.erase(netId);
        }
    }

    void ClearEventLog(std::uint32_t netId) {
        std::lock_guard<std::mutex> lock(objectEventLogsMutex_);
        auto it = objectEventLogs_.find(netId);
        if (it != objectEventLogs_.end()) {
            it->second.clear();
        }
    }

    std::vector<DevTools::EventLogEntry> CopyEventLog(std::uint32_t netId) const {
        std::lock_guard<std::mutex> lock(objectEventLogsMutex_);
        const auto it = objectEventLogs_.find(netId);
        return it != objectEventLogs_.end()
            ? it->second
            : std::vector<DevTools::EventLogEntry>{};
    }

    std::vector<std::uint32_t> CopyOpenEventLogWindows() const {
        std::lock_guard<std::mutex> lock(objectEventLogsMutex_);
        return { openEventLogWindows_.begin(), openEventLogWindows_.end() };
    }

    DevTools::ObjectSnapshot CreateSnapshot(const SDK::GameObject& obj) const {
        DevTools::ObjectSnapshot snap;
        const uintptr_t address = obj.Address();
        const auto type = obj.Type();
        const auto raw = ::Core::Objects::ReadSnapshot(address, type);

        snap.networkId = raw.handle.networkId;
        snap.address = address;
        snap.name = raw.name;
        snap.characterName = raw.characterName;
        snap.type = type;
        snap.team = static_cast<SDK::GameObjectTeam>(raw.team);
        snap.position = raw.position;
        snap.snapshotTick = SDK::Game::TickCount();
        snap.isUnderlyingValid = raw.handle.IsValid();

        const bool isHero = type == ::Core::Objects::ObjectType::AIHeroClient;
        const bool isMinion = type == ::Core::Objects::ObjectType::AIMinionClient;
        const bool supportsAIBaseData = isHero || isMinion;
        if (supportsAIBaseData) {
            snap.health = raw.health;
            snap.maxHealth = raw.maxHealth;
            snap.allShield = raw.allShield;
            snap.physShield = raw.physicalShield;
            snap.magShield = raw.magicalShield;
            snap.healthRegen = ::CoreAIHeroClient::HealthRegenRate(address);
            snap.mana = raw.mana;
            snap.maxMana = raw.maxMana;
            snap.armor = raw.armor;
            snap.spellBlock = raw.spellBlock;
            snap.attackDamage = raw.totalAttackDamage;
            snap.baseAD = raw.baseAttackDamage;
            snap.bonusAD = raw.totalAttackDamage - raw.baseAttackDamage;
            snap.abilityPower = raw.abilityPower;
            snap.attackRange = raw.attackRange;
            snap.moveSpeed = raw.moveSpeed;
            snap.attackSpeedMod = raw.attackSpeedMod;
            snap.level = raw.level;

            if (isHero) {
                const auto heroStats = ::CoreAIHeroClient::Read(address);
                snap.crit = heroStats.crit;
                snap.bonusArmor = heroStats.bonusArmor;
                snap.bonusSpellBlock = heroStats.bonusSpellBlock;
                snap.lethality = heroStats.physicalLethality;
                snap.flatArmorPen = heroStats.flatArmorPen;
                snap.percentArmorPen = heroStats.percentArmorPen;
                snap.flatMagicPen = heroStats.flatMagicPen;
                snap.percentMagicPen = heroStats.percentMagicPen;
            }

            uintptr_t buffs[256] = {};
            const int buffCount = CoreBuffs::Enumerate(address, buffs, 256);
            const float gameTime = SDK::Game::Time();
            char nameBuf[96] = {};
            for (int i = 0; i < buffCount; ++i) {
                CoreBuffs::BuffRef buff{ buffs[i] };
                if (!buff.ReadName(nameBuf, sizeof(nameBuf))) continue;

                DevTools::SnapshotBuff b;
                strncpy_s(b.name, nameBuf, _TRUNCATE);
                b.stacks = buff.GetStacks();
                b.count = b.stacks;
                b.type = buff.GetType();
                b.startTime = buff.GetStartTime();
                b.endTime = buff.GetEndTime();
                b.address = buff.address;
                b.live = buff.IsActive(gameTime);
                snap.buffs.push_back(b);
            }
        }

        if (isHero) {
            const float gameTime = SDK::Game::Time();

            static const SDK::SpellSlot slots[] = {
                SDK::SpellSlot::Q, SDK::SpellSlot::W, SDK::SpellSlot::E, SDK::SpellSlot::R,
                SDK::SpellSlot::Summoner1, SDK::SpellSlot::Summoner2,
                SDK::SpellSlot::Item1, SDK::SpellSlot::Item2, SDK::SpellSlot::Item3,
                SDK::SpellSlot::Item4, SDK::SpellSlot::Item5, SDK::SpellSlot::Item6,
                SDK::SpellSlot::Trinket
            };

            for (const auto& slot : slots) {
                SDK::SpellDataInstClient spell(address, slot);
                if (!spell.IsValid()) continue;

                DevTools::SnapshotSpell s;
                s.slot = slot;
                strncpy_s(s.name, spell.Name().c_str(), _TRUNCATE);
                s.level = spell.Level();
                s.ammo = spell.Ammo();
                s.maxAmmo = spell.MaxAmmo();
                s.cooldown = spell.Cooldown();
                const float cooldownExpires = spell.CooldownExpires();
                s.remainingCooldown = std::isfinite(cooldownExpires) &&
                                      cooldownExpires > gameTime &&
                                      cooldownExpires - gameTime < 100000.0f
                    ? cooldownExpires - gameTime
                    : 0.0f;
                s.manaCost = spell.ManaCost();
                if (static_cast<int>(slot) <= static_cast<int>(SDK::SpellSlot::R) &&
                    !spell.Learned()) {
                    s.state = static_cast<std::uint32_t>(CoreSpellBook::State_NotLearned);
                } else if (s.remainingCooldown > 0.0f) {
                    s.state = static_cast<std::uint32_t>(CoreSpellBook::State_Cooldown);
                } else {
                    s.state = static_cast<std::uint32_t>(CoreSpellBook::State_Ready);
                }
                snap.spells.push_back(s);
            }
        }

        return snap;
    }

    void TakeSnapshot(const SDK::GameObject& obj) {
        DevTools::ObjectSnapshot snap = CreateSnapshot(obj);
        if (!snap.isUnderlyingValid || snap.networkId == 0 ||
            snap.networkId == 0xFFFFFFFFu) {
            NightSharpDebug::Logf("[Dev] Snapshot skipped: focused object became invalid");
            return;
        }

        auto it = std::find_if(snapshots_.begin(), snapshots_.end(), [&](const DevTools::ObjectSnapshot& s) {
            return s.networkId == snap.networkId;
        });

        if (it != snapshots_.end()) {
            *it = snap;
            NightSharpDebug::Logf("[Dev] Updated snapshot for %s (NetId: %u)", snap.characterName.c_str(), snap.networkId);
        } else {
            snapshots_.push_back(snap);
            NightSharpDebug::Logf("[Dev] Added new snapshot for %s (NetId: %u)", snap.characterName.c_str(), snap.networkId);
        }
    }

    SDK::GameObject GetFocusedObject() const {
        std::lock_guard<std::mutex> lock(focusedObjectMutex_);
        return focusedObject_;
    }

    void SetFocusedObject(const SDK::GameObject& object) {
        std::lock_guard<std::mutex> lock(focusedObjectMutex_);
        focusedObject_ = object;
    }

    SDK::GameObject ScanFocusedObject() const {
        const auto player = SDK::ObjectManager::Player();
        const uintptr_t playerAddress = player.IsValid() ? player.Address() : 0;
        const Vec3 cursorPos = SDK::Game::CursorPos();
        if (!cursorPos.IsValid()) {
            return player;
        }
        const float rangeSqr = static_cast<float>(maxRange_ * maxRange_);

        SDK::GameObject closestObj{};
        float closestDistSqr = rangeSqr;

        for (const auto& obj : SDK::ObjectManager::Get<SDK::GameObject>()) {
            if (!obj.IsValid()) continue;
            const uintptr_t address = obj.Address();
            if (!Globals::IsValidPtr(address) || address == playerAddress) continue;

            const Vec3 position = ::Core::Objects::ReadPosition(address);
            if (!position.IsValid()) continue;
            const float distSqr = position.DistanceSqr(cursorPos);
            if (distSqr >= closestDistSqr) continue;

            const auto objectData = ::Core::Objects::ReadSnapshot(address, obj.Type());
            if (!objectData.handle.IsValid()) continue;

            const std::string name = objectData.name;
            const std::string charName = objectData.characterName;
            if (IsClutter(obj, name, charName)) continue;

            closestDistSqr = distSqr;
            closestObj = SDK::GameObject(objectData.handle);
        }

        if (closestObj.IsValid()) {
            return closestObj;
        }
        return player;
    }

    SDK::UI::Menu* menu_ = nullptr;
    SDK::UI::MenuBool* menuEnabled_ = nullptr;
    SDK::UI::MenuSlider* menuMaxRange_ = nullptr;
    SDK::UI::MenuList* menuProvider_ = nullptr;
    SDK::UI::MenuBool* menuScanAll_ = nullptr;
    SDK::UI::MenuBool* menuScanHeroes_ = nullptr;
    SDK::UI::MenuBool* menuScanMinions_ = nullptr;
    SDK::UI::MenuBool* menuScanTurrets_ = nullptr;
    SDK::UI::MenuBool* menuScanMissiles_ = nullptr;
    SDK::UI::MenuBool* menuFilterClutter_ = nullptr;
    SDK::UI::MenuRuntime* menuInspector_ = nullptr;
    SDK::UI::MenuBool* menuLogEnabled_ = nullptr;
    SDK::UI::MenuList* menuLogSource_ = nullptr;
    SDK::UI::MenuBool* menuLogVerbose_ = nullptr;
    SDK::UI::MenuBool* menuLogRaw_ = nullptr;
    SDK::UI::MenuBool* menuLogSkipAA_ = nullptr;
    SDK::UI::MenuBool* menuLogToFile_ = nullptr;
    SDK::UI::MenuBool* menuLogProcessSpell_ = nullptr;
    SDK::UI::MenuBool* menuLogDoCast_ = nullptr;
    SDK::UI::MenuBool* menuLogFinishCast_ = nullptr;
    SDK::UI::MenuBool* menuLogSpellImpact_ = nullptr;
    SDK::UI::MenuBool* menuLogCastSpell_ = nullptr;
    SDK::UI::MenuBool* menuLogStopCast_ = nullptr;
    SDK::UI::MenuBool* menuLogAnimation_ = nullptr;
    SDK::UI::MenuBool* menuLogBuffAdd_ = nullptr;
    SDK::UI::MenuBool* menuLogBuffRemove_ = nullptr;
    SDK::UI::MenuBool* menuLogNewPath_ = nullptr;

private:
    enum class TabCallbackKind : std::size_t {
        Load = 0,
        Update,
        Render,
        Menu,
        Copy,
        Event,
        Count
    };

    static constexpr std::size_t kTabCallbackKindCount =
        static_cast<std::size_t>(TabCallbackKind::Count);

    struct TabCallbackPerformance {
        std::uint64_t calls = 0;
        std::uint64_t overOneMs = 0;
        std::uint64_t overFourMs = 0;
        double lastMs = 0.0;
        double averageMs = 0.0;
        double smoothedMs = 0.0;
        double maxMs = 0.0;
    };

    struct TabPerformanceState {
        std::array<TabCallbackPerformance, kTabCallbackKindCount> callbacks = {};
        std::int64_t windowStartCounter = 0;
        double windowCpuMs = 0.0;
        double windowWorstMs = 0.0;
        std::uint64_t windowCalls = 0;
        double recentCpuMsPerSecond = 0.0;
        double recentWorstMs = 0.0;
        double recentCallsPerSecond = 0.0;
    };

    struct TabRuntimeState {
        int consecutiveFaults = 0;
        int retryAfterTick = 0;
        bool quarantined = false;
        DevTools::TabFaultCapture lastFault = {};
    };

    std::vector<std::unique_ptr<DevTools::IDeveloperTab>> tabs_;
    std::vector<TabRuntimeState> tabRuntimeStates_;
    mutable std::mutex tabRuntimeMutex_;
    std::vector<TabPerformanceState> tabPerformanceStates_;
    mutable std::mutex tabPerformanceMutex_;
    int profilerTargetFps_ = 60;
    mutable std::unordered_set<std::uintptr_t> objectCollectionAddresses_;
    static inline DeveloperToolsPlugin* s_instance = nullptr;

    static std::int64_t ProfilerCounterNow() noexcept {
        LARGE_INTEGER value{};
        return QueryPerformanceCounter(&value) ? value.QuadPart : 0;
    }

    static double ProfilerCounterToMs(std::int64_t start,
                                      std::int64_t end) noexcept {
        static const double frequency = []() noexcept {
            LARGE_INTEGER value{};
            return QueryPerformanceFrequency(&value) && value.QuadPart > 0
                ? static_cast<double>(value.QuadPart)
                : 0.0;
        }();
        if (frequency <= 0.0 || start <= 0 || end < start) {
            return 0.0;
        }
        return static_cast<double>(end - start) * 1000.0 / frequency;
    }

    static const char* TabCallbackKindName(TabCallbackKind kind) noexcept {
        switch (kind) {
        case TabCallbackKind::Load: return "Load";
        case TabCallbackKind::Update: return "Update";
        case TabCallbackKind::Render: return "Render";
        case TabCallbackKind::Menu: return "Menu";
        case TabCallbackKind::Copy: return "Copy";
        case TabCallbackKind::Event: return "Event";
        default: return "?";
        }
    }

    void InitializePerformanceProfiler(std::size_t tabCount) {
        const std::int64_t now = ProfilerCounterNow();
        std::lock_guard<std::mutex> lock(tabPerformanceMutex_);
        tabPerformanceStates_.assign(tabCount, {});
        for (auto& state : tabPerformanceStates_) {
            state.windowStartCounter = now;
        }
    }

    void RecordTabDuration(std::size_t index,
                           TabCallbackKind kind,
                           std::int64_t startCounter) {
        const std::int64_t endCounter = ProfilerCounterNow();
        const double elapsedMs = ProfilerCounterToMs(startCounter, endCounter);
        const std::size_t kindIndex = static_cast<std::size_t>(kind);
        if (kindIndex >= kTabCallbackKindCount ||
            !std::isfinite(elapsedMs)) {
            return;
        }

        std::lock_guard<std::mutex> lock(tabPerformanceMutex_);
        if (index >= tabPerformanceStates_.size()) {
            return;
        }
        auto& state = tabPerformanceStates_[index];
        auto& metric = state.callbacks[kindIndex];
        metric.calls++;
        metric.lastMs = elapsedMs;
        metric.averageMs += (elapsedMs - metric.averageMs) /
                            static_cast<double>(metric.calls);
        metric.smoothedMs = metric.calls == 1
            ? elapsedMs
            : metric.smoothedMs * 0.90 + elapsedMs * 0.10;
        metric.maxMs = std::max(metric.maxMs, elapsedMs);
        metric.overOneMs += elapsedMs >= 1.0 ? 1u : 0u;
        metric.overFourMs += elapsedMs >= 4.0 ? 1u : 0u;

        if (state.windowStartCounter == 0) {
            state.windowStartCounter = endCounter;
        }
        state.windowCpuMs += elapsedMs;
        state.windowWorstMs = std::max(state.windowWorstMs, elapsedMs);
        state.windowCalls++;
    }

    void RotatePerformanceWindows() {
        const std::int64_t now = ProfilerCounterNow();
        std::lock_guard<std::mutex> lock(tabPerformanceMutex_);
        for (auto& state : tabPerformanceStates_) {
            if (state.windowStartCounter == 0) {
                state.windowStartCounter = now;
                continue;
            }
            const double windowMs =
                ProfilerCounterToMs(state.windowStartCounter, now);
            if (windowMs < 1000.0) {
                continue;
            }
            const double scale = windowMs > 0.0 ? 1000.0 / windowMs : 0.0;
            state.recentCpuMsPerSecond = state.windowCpuMs * scale;
            state.recentCallsPerSecond =
                static_cast<double>(state.windowCalls) * scale;
            state.recentWorstMs = state.windowWorstMs;
            state.windowStartCounter = now;
            state.windowCpuMs = 0.0;
            state.windowWorstMs = 0.0;
            state.windowCalls = 0;
        }
    }

    std::vector<TabPerformanceState> CopyPerformanceProfiler() const {
        std::lock_guard<std::mutex> lock(tabPerformanceMutex_);
        return tabPerformanceStates_;
    }

    void ResetPerformanceProfiler() {
        InitializePerformanceProfiler(tabs_.size() + 1);
    }

    const char* PerformanceStateName(std::size_t index) const noexcept {
        return index < tabs_.size()
            ? tabs_[index]->GetTabName()
            : "Developer Tools total";
    }

    void DrawPerformanceProfiler() {
        if (!ImGui::CollapsingHeader(
                "Performance Profiler",
                ImGuiTreeNodeFlags_DefaultOpen)) {
            return;
        }

        const ImGuiIO& io = ImGui::GetIO();
        const double currentFps = io.Framerate > 1.0f
            ? static_cast<double>(io.Framerate)
            : 0.0;
        const double actualFrameMs = io.DeltaTime > 0.0f
            ? static_cast<double>(io.DeltaTime) * 1000.0
            : 0.0;

        ImGui::SetNextItemWidth(140.0f);
        ImGui::SliderInt("Target FPS", &profilerTargetFps_, 30, 240);
        profilerTargetFps_ = std::clamp(profilerTargetFps_, 30, 240);
        ImGui::SameLine();
        if (ImGui::Button("Reset profiler")) {
            ResetPerformanceProfiler();
        }

        const double targetFrameBudgetMs =
            1000.0 / static_cast<double>(profilerTargetFps_);
        const auto states = CopyPerformanceProfiler();
        double totalCpuMsPerSecond = states.size() > tabs_.size()
            ? states[tabs_.size()].recentCpuMsPerSecond
            : 0.0;
        if (states.size() <= tabs_.size()) {
            for (const auto& state : states) {
                totalCpuMsPerSecond += state.recentCpuMsPerSecond;
            }
        }
        const double totalMsPerFrame = currentFps > 0.0
            ? totalCpuMsPerSecond / currentFps
            : 0.0;

        ImGui::Text(
            "FPS: %.1f | frame: %.2f ms | target budget: %.2f ms | Developer Tools: %.3f ms/frame",
            currentFps,
            actualFrameMs,
            targetFrameBudgetMs,
            totalMsPerFrame);
        ImGui::TextDisabled(
            "Times are measured CPU wall-time. Risk compares recent tab cost and spikes with the target frame budget.");

        if (ImGui::BeginTable(
                "DeveloperTabPerformanceSummary",
                8,
                ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollX)) {
            ImGui::TableSetupColumn("Tab");
            ImGui::TableSetupColumn("Calls/s");
            ImGui::TableSetupColumn("CPU ms/s");
            ImGui::TableSetupColumn("ms/frame");
            ImGui::TableSetupColumn("Budget");
            ImGui::TableSetupColumn("Worst 1s");
            ImGui::TableSetupColumn("Risk");
            ImGui::TableSetupColumn("Samples");
            ImGui::TableHeadersRow();

            const std::size_t count = states.size();
            for (std::size_t index = 0; index < count; ++index) {
                const auto& state = states[index];
                const double msPerFrame = currentFps > 0.0
                    ? state.recentCpuMsPerSecond / currentFps
                    : 0.0;
                const double averageBudgetPercent = targetFrameBudgetMs > 0.0
                    ? msPerFrame * 100.0 / targetFrameBudgetMs
                    : 0.0;
                const double worstBudgetPercent = targetFrameBudgetMs > 0.0
                    ? state.recentWorstMs * 100.0 / targetFrameBudgetMs
                    : 0.0;

                std::uint64_t samples = 0;
                for (const auto& metric : state.callbacks) {
                    samples += metric.calls;
                }

                const char* risk = "Low";
                ImVec4 riskColor(0.35f, 0.90f, 0.45f, 1.0f);
                if (state.recentCallsPerSecond <= 0.0) {
                    risk = "Collecting";
                    riskColor = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
                } else if (averageBudgetPercent >= 20.0 ||
                           worstBudgetPercent >= 100.0) {
                    risk = "High";
                    riskColor = ImVec4(1.0f, 0.30f, 0.20f, 1.0f);
                } else if (averageBudgetPercent >= 5.0 ||
                           worstBudgetPercent >= 25.0) {
                    risk = "Watch";
                    riskColor = ImVec4(1.0f, 0.75f, 0.20f, 1.0f);
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(PerformanceStateName(index));
                ImGui::TableNextColumn();
                ImGui::Text("%.1f", state.recentCallsPerSecond);
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", state.recentCpuMsPerSecond);
                ImGui::TableNextColumn();
                ImGui::Text("%.4f", msPerFrame);
                ImGui::TableNextColumn();
                ImGui::Text("%.1f%%", averageBudgetPercent);
                ImGui::TableNextColumn();
                ImGui::Text("%.3f ms", state.recentWorstMs);
                ImGui::TableNextColumn();
                ImGui::TextColored(riskColor, "%s", risk);
                ImGui::TableNextColumn();
                ImGui::Text("%llu", static_cast<unsigned long long>(samples));
            }
            ImGui::EndTable();
        }

        if (ImGui::TreeNode("Per-callback timing")) {
            if (ImGui::BeginTable(
                    "DeveloperTabCallbackPerformance",
                    9,
                    ImGuiTableFlags_Borders |
                        ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_ScrollY,
                    ImVec2(0.0f, 240.0f))) {
                ImGui::TableSetupColumn("Tab");
                ImGui::TableSetupColumn("Callback");
                ImGui::TableSetupColumn("Calls");
                ImGui::TableSetupColumn("Last ms");
                ImGui::TableSetupColumn("Avg ms");
                ImGui::TableSetupColumn("EMA ms");
                ImGui::TableSetupColumn("Max ms");
                ImGui::TableSetupColumn(">=1 ms");
                ImGui::TableSetupColumn(">=4 ms");
                ImGui::TableHeadersRow();

                const std::size_t count = states.size();
                for (std::size_t index = 0; index < count; ++index) {
                    for (std::size_t kindIndex = 0;
                         kindIndex < kTabCallbackKindCount;
                         ++kindIndex) {
                        const auto& metric = states[index].callbacks[kindIndex];
                        if (metric.calls == 0) {
                            continue;
                        }
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(PerformanceStateName(index));
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(TabCallbackKindName(
                            static_cast<TabCallbackKind>(kindIndex)));
                        ImGui::TableNextColumn();
                        ImGui::Text("%llu", static_cast<unsigned long long>(metric.calls));
                        ImGui::TableNextColumn();
                        ImGui::Text("%.4f", metric.lastMs);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.4f", metric.averageMs);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.4f", metric.smoothedMs);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.4f", metric.maxMs);
                        ImGui::TableNextColumn();
                        ImGui::Text("%llu", static_cast<unsigned long long>(metric.overOneMs));
                        ImGui::TableNextColumn();
                        ImGui::Text("%llu", static_cast<unsigned long long>(metric.overFourMs));
                    }
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }

    bool CanRunTab(std::size_t index) const {
        if (index >= tabs_.size() || index >= tabRuntimeStates_.size() || !tabs_[index]) {
            return false;
        }
        const int now = SDK::Variables::TickCount();
        std::lock_guard<std::mutex> lock(tabRuntimeMutex_);
        const auto& state = tabRuntimeStates_[index];
        return !state.quarantined && now >= state.retryAfterTick;
    }

    TabRuntimeState CopyTabRuntimeState(std::size_t index) const {
        std::lock_guard<std::mutex> lock(tabRuntimeMutex_);
        return index < tabRuntimeStates_.size()
            ? tabRuntimeStates_[index]
            : TabRuntimeState{};
    }

    void RecordTabFault(std::size_t index,
                        const char* callback,
                        const DevTools::TabFaultCapture& fault) {
        if (index >= tabs_.size() || index >= tabRuntimeStates_.size()) {
            return;
        }

        int consecutiveFaults = 0;
        bool quarantined = false;
        const int retryAfterTick = SDK::Variables::TickCount() + 1000;
        {
            std::lock_guard<std::mutex> lock(tabRuntimeMutex_);
            auto& state = tabRuntimeStates_[index];
            state.lastFault = fault;
            state.retryAfterTick = retryAfterTick;
            state.consecutiveFaults++;
            state.quarantined = state.consecutiveFaults >= 3;
            consecutiveFaults = state.consecutiveFaults;
            quarantined = state.quarantined;
        }

        NightSharpDebug::Logf(
            "[DeveloperTools] isolated tab fault tab=%s callback=%s code=0x%08X address=0x%llX attempt=%d quarantined=%d",
            tabs_[index]->GetTabName(),
            callback ? callback : "?",
            static_cast<unsigned>(fault.code),
            static_cast<unsigned long long>(fault.address),
            consecutiveFaults,
            quarantined ? 1 : 0);
    }

    void ResetTabFault(std::size_t index) {
        if (index < tabRuntimeStates_.size()) {
            std::lock_guard<std::mutex> lock(tabRuntimeMutex_);
            tabRuntimeStates_[index] = {};
        }
    }

    void RunTabUpdate(std::size_t index) {
        if (!CanRunTab(index)) return;
        const std::int64_t startCounter = ProfilerCounterNow();
        DevTools::TabFaultCapture fault{};
        const bool succeeded =
            DevTools::InvokeTabUpdateGuarded(tabs_[index].get(), &fault);
        RecordTabDuration(index, TabCallbackKind::Update, startCounter);
        if (succeeded) {
            std::lock_guard<std::mutex> lock(tabRuntimeMutex_);
            tabRuntimeStates_[index].consecutiveFaults = 0;
            return;
        }
        RecordTabFault(index, "update", fault);
    }

    void RunTabRender(std::size_t index) {
        if (!CanRunTab(index)) return;
        const std::int64_t startCounter = ProfilerCounterNow();
        DevTools::TabFaultCapture fault{};
        const bool succeeded =
            DevTools::InvokeTabRenderGuarded(tabs_[index].get(), &fault);
        RecordTabDuration(index, TabCallbackKind::Render, startCounter);
        if (!succeeded) {
            RecordTabFault(index, "render", fault);
        }
    }

    void RunTabDraw(std::size_t index) {
        if (!CanRunTab(index)) return;
        const std::int64_t startCounter = ProfilerCounterNow();
        DevTools::TabFaultCapture fault{};
        const bool succeeded =
            DevTools::InvokeTabDrawGuarded(tabs_[index].get(), &fault);
        RecordTabDuration(index, TabCallbackKind::Menu, startCounter);
        if (!succeeded) {
            RecordTabFault(index, "menu", fault);
        }
    }

    void RunTabCopy(std::size_t index) {
        if (!CanRunTab(index)) return;
        const std::int64_t startCounter = ProfilerCounterNow();
        DevTools::TabFaultCapture fault{};
        const bool succeeded =
            DevTools::InvokeTabCopyGuarded(tabs_[index].get(), &fault);
        RecordTabDuration(index, TabCallbackKind::Copy, startCounter);
        if (!succeeded) {
            RecordTabFault(index, "copy", fault);
        }
    }

    template <typename TArgs>
    void DispatchTabEvent(void (DevTools::IDeveloperTab::*callback)(const TArgs&),
                          const TArgs& args,
                          const char* callbackName) {
        const std::int64_t totalStartCounter = ProfilerCounterNow();
        for (std::size_t index = 0; index < tabs_.size(); ++index) {
            if (!CanRunTab(index)) continue;
            const std::int64_t startCounter = ProfilerCounterNow();
            DevTools::TabFaultCapture fault{};
            const bool succeeded = DevTools::InvokeTabEventGuarded(
                tabs_[index].get(), callback, args, &fault);
            RecordTabDuration(index, TabCallbackKind::Event, startCounter);
            if (!succeeded) {
                RecordTabFault(index, callbackName, fault);
            }
        }
        RecordTabDuration(
            tabs_.size(), TabCallbackKind::Event, totalStartCounter);
    }

public:
    const char* GetName() const override { return "Developer Tools"; }
    const char* GetInternalId() const override { return "utility.developer_tools"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Utility; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override;
    void OnUnload() override;
    void OnUpdate() override;
    void OnRender() override;
    void OnMenu() override;

    static const std::string& GetObjectName(const SDK::GameObject& object) {
        if (!object.IsValid()) {
            static const std::string empty;
            return empty;
        }
        return object.Name();
    }

    static const std::string& GetObjectCharacterName(const SDK::GameObject& object) {
        if (!object.IsValid()) {
            static const std::string empty;
            return empty;
        }
        return object.CharacterName();
    }

    static void GetMissileSpeedAndRange(const SDK::GameObject& obj, float& speed, float& range) {
        speed = 0.0f;
        range = 0.0f;

        const uintptr_t a = obj.Address();
        if (!a) return;

        const uintptr_t spellData = Globals::Read<uintptr_t>(a + Offset::MissileClient::SpellDataPtr);
        if (!Globals::IsValidPtr(spellData)) return;

        const uintptr_t spellDataObj = Globals::Read<uintptr_t>(spellData + 0x00);
        if (!Globals::IsValidPtr(spellDataObj)) return;

        const uintptr_t resource = Globals::Read<uintptr_t>(spellDataObj + 0x60); // Offset::SpellDataLayout::DataResource
        if (!Globals::IsValidPtr(resource)) return;

        range = Globals::Read<float>(resource + 0x478); // Offset::SpellDataResourceLayout::ResCastRange
        speed = Globals::Read<float>(resource + 0x518); // Offset::SpellDataResourceLayout::ResMissileSpeed
    }

    static const char* TeamToString(const SDK::GameObject& obj) {
        if (!obj.IsValid()) return "Unknown";
        const uintptr_t address = obj.Address();
        const auto team = static_cast<SDK::GameObjectTeam>(
            ::Core::Objects::ReadTeamValue(address));
        const auto playerTeam = SDK::GameObject::CachedPlayerTeam();
        if (playerTeam != 0 && static_cast<std::uint32_t>(team) == playerTeam) return "Ally";
        if (playerTeam != 0 && team != SDK::GameObjectTeam::Unknown &&
            static_cast<std::uint32_t>(team) != playerTeam) return "Enemy";
        if (team == SDK::GameObjectTeam::Neutral) return "Neutral";
        if (team == SDK::GameObjectTeam::Order) return "Blue (100)";
        if (team == SDK::GameObjectTeam::Chaos) return "Red (200)";
        return "Unknown";
    }

    static void GetStatusString(const SDK::GameObject& obj, char* buf, size_t maxLen) {
        if (!buf || maxLen == 0) return;
        if (!obj.IsValid()) {
            std::snprintf(buf, maxLen, "Unknown");
            return;
        }

        const auto type = obj.Type();
        const bool supportsAIBaseData =
            type == ::Core::Objects::ObjectType::AIHeroClient ||
            type == ::Core::Objects::ObjectType::AIMinionClient;
        const auto data = ::Core::Objects::ReadSnapshot(obj.Address(), type);
        if (!data.handle.IsValid()) {
            std::snprintf(buf, maxLen, "Invalid/stale object");
            return;
        }

        if (!supportsAIBaseData) {
            std::snprintf(buf, maxLen, "%s",
                          data.isVisible ? "Visible object" : "Object in fog");
            return;
        }

        int len = 0;
        if (data.isDead) len += std::snprintf(buf + len, maxLen - len, "Dead");
        else len += std::snprintf(buf + len, maxLen - len, "Alive");

        if (!data.isVisible && len < static_cast<int>(maxLen))
            len += std::snprintf(buf + len, maxLen - len, ", Fog");
        if (!data.isTargetable && len < static_cast<int>(maxLen))
            len += std::snprintf(buf + len, maxLen - len, ", Untargetable");
        const float hpPercent = data.maxHealth > 0.0f
            ? data.health * 100.0f / data.maxHealth
            : 0.0f;
        if (len < static_cast<int>(maxLen))
            std::snprintf(buf + len, maxLen - len, ", HP:%.0f/%.0f(%.0f%%)",
                          data.health, data.maxHealth, hpPercent);
    }

    static const char* ObjectTypeToString(::Core::Objects::ObjectType type) {
        switch (type) {
        case ::Core::Objects::ObjectType::GameObject: return "GameObject";
        case ::Core::Objects::ObjectType::AIHeroClient: return "AIHeroClient";
        case ::Core::Objects::ObjectType::AIMinionClient: return "AIMinionClient";
        // case ::Core::Objects::ObjectType::AITurretClient: return "AITurretClient";
        case ::Core::Objects::ObjectType::MissileClient: return "MissileClient";
        case ::Core::Objects::ObjectType::BarracksDampenerClient: return "BarracksDampenerClient";
        case ::Core::Objects::ObjectType::HQClient: return "HQClient";
        // case ::Core::Objects::ObjectType::ShopClient: return "ShopClient";
        case ::Core::Objects::ObjectType::Obj_SpawnPoint: return "Obj_SpawnPoint";
        case ::Core::Objects::ObjectType::EffectEmitter: return "EffectEmitter";
        default: return "Unknown";
        }
    }

    static const char* SlotToString(int slot) {
        switch (slot) {
        case 0:  return "Q";
        case 1:  return "W";
        case 2:  return "E";
        case 3:  return "R";
        case 4:  return "D/Summoner1";
        case 5:  return "F/Summoner2";
        case 6:  return "Item1";
        case 7:  return "Item2";
        case 8:  return "Item3";
        case 9:  return "Item4";
        case 10: return "Item5";
        case 11: return "Item6";
        case 12: return "Trinket";
        case 13: return "Recall";
        case 64: return "BasicAttack";
        case -1: return "Unknown";
        default: return "Other";
        }
    }

    bool IsClutter(const SDK::GameObject& obj, const std::string& name, const std::string& charName) const {
        if (!filterClutter_) {
            return false;
        }
        if (name.empty() && charName.empty()) {
            return true;
        }
        if (name == "missile" || name.find("MoveTo") != std::string::npos ||
            charName.find("Grass") != std::string::npos || charName.find("FX") != std::string::npos ||
            charName.find("LevelProp") != std::string::npos || charName.find("emitter") != std::string::npos) {
            return true;
        }
        return false;
    }

    template <typename T>
    void AddUniqueObjectsFromSource(std::vector<SDK::GameObject>& dest, const std::vector<T>& source) const {
        for (const auto& obj : source) {
            if (!obj.IsValid()) continue;
            const std::uintptr_t address = obj.Address();
            if (address != 0 && objectCollectionAddresses_.insert(address).second) {
                dest.push_back(SDK::GameObject(obj.Handle()));
            }
        }
    }

    void BeginObjectCollection() const {
        objectCollectionAddresses_.clear();
        if (objectCollectionAddresses_.bucket_count() < 1024) {
            objectCollectionAddresses_.reserve(1024);
        }
    }

    void AddUniqueObject(std::vector<SDK::GameObject>& dest, const SDK::GameObject& obj) const {
        if (!obj.IsValid()) return;
        const std::uintptr_t address = obj.Address();
        if (address != 0 && objectCollectionAddresses_.insert(address).second) {
            dest.push_back(obj);
        }
    }

private:
    static void OnProcessSpellCast(const SDK::Events::ProcessSpellEventArgs& args);
    static void OnDoCastEvent(const SDK::Events::ProcessSpellEventArgs& args);
    static void OnFinishCastEvent(const SDK::Events::ProcessSpellEventArgs& args);
    static void OnSpellImpactEvent(const SDK::Events::ProcessSpellEventArgs& args);
    static void OnCastSpellEvent(const SDK::Events::CastSpellEventArgs& args);
    static void OnStopCastEvent(const SDK::Events::StopCastEventArgs& args);
    static void OnPlayAnimationEvent(const SDK::Events::PlayAnimationEventArgs& args);
    static void OnBuffAddEvent(const SDK::Events::BuffEventArgs& args);
    static void OnBuffRemoveEvent(const SDK::Events::BuffEventArgs& args);
    static void OnNewPathEvent(const SDK::Events::NewPathEventArgs& args);
    static void OnObjectDelete(const SDK::Events::ObjectEventArgs& args);

    static void OnMenuBridge(void* userData) {
        if (auto* self = static_cast<DeveloperToolsPlugin*>(userData)) {
            self->OnMenu();
        }
    }

    void SyncMenuSettings();
    void DestroyNativeMenu();
};

} // namespace Plugins

// Include Tab Implementations
#include "DeveloperTools/ObjectDetectorTab.h"
#include "DeveloperTools/EventLoggerTab.h"
#include "DeveloperTools/SpellItemInspectorTab.h"
#include "DeveloperTools/PlayerBuffInspectorTab.h"
#include "DeveloperTools/StatsInspectorTab.h"

namespace Plugins {

inline void DeveloperToolsPlugin::OnLoad() {
    s_instance = this;
    enabled_ = true;
    maxRange_ = 400;
    activeTabIdx_ = 0;
    ClearTrackedObjects();
    SetFocusedObject(SDK::GameObject{});
    const int now = SDK::Variables::TickCount();
    lastFocusedScanTick_ = now;
    lastSnapshotTick_ = now;

    // Create sub-tabs dynamically
    tabs_.clear();
    tabs_.push_back(std::make_unique<DevTools::ObjectDetectorTab>(this));
    tabs_.push_back(std::make_unique<DevTools::EventLoggerTab>(this));
    tabs_.push_back(std::make_unique<DevTools::SpellItemInspectorTab>(this));
    tabs_.push_back(std::make_unique<DevTools::PlayerBuffInspectorTab>(this));
    tabs_.push_back(std::make_unique<DevTools::StatsInspectorTab>(this));
    tabRuntimeStates_.assign(tabs_.size(), {});
    InitializePerformanceProfiler(tabs_.size() + 1);

    for (std::size_t index = 0; index < tabs_.size(); ++index) {
        const std::int64_t startCounter = ProfilerCounterNow();
        DevTools::TabFaultCapture fault{};
        const bool succeeded =
            DevTools::InvokeTabLoadGuarded(tabs_[index].get(), &fault);
        RecordTabDuration(index, TabCallbackKind::Load, startCounter);
        if (!succeeded) {
            RecordTabFault(index, "load", fault);
        }
    }

    SDK::Events::AddOnProcessSpell(&DeveloperToolsPlugin::OnProcessSpellCast);
    SDK::Events::AddOnDoCast(&DeveloperToolsPlugin::OnDoCastEvent);
    SDK::Events::AddOnFinishCast(&DeveloperToolsPlugin::OnFinishCastEvent);
    SDK::Events::AddOnSpellImpact(&DeveloperToolsPlugin::OnSpellImpactEvent);
    SDK::Events::AddOnProcessCastSpell(&DeveloperToolsPlugin::OnCastSpellEvent);
    SDK::Events::AddOnStopCast(&DeveloperToolsPlugin::OnStopCastEvent);
    SDK::Events::AddOnPlayAnimation(&DeveloperToolsPlugin::OnPlayAnimationEvent);
    SDK::Events::AddOnBuffAdd(&DeveloperToolsPlugin::OnBuffAddEvent);
    SDK::Events::AddOnBuffRemove(&DeveloperToolsPlugin::OnBuffRemoveEvent);
    SDK::Events::AddOnNewPath(&DeveloperToolsPlugin::OnNewPathEvent);
    SDK::Events::AddOnDeleteObject(&DeveloperToolsPlugin::OnObjectDelete);

    DestroyNativeMenu();
    menu_ = new SDK::UI::Menu(GetInternalId(), GetName(), true);
    menuEnabled_ = menu_->Add(new SDK::UI::MenuBool("Enabled", "Enable Developer Tools", enabled_));
    menuMaxRange_ = menu_->Add(new SDK::UI::MenuSlider("MaxRange", "Max Scan Range", maxRange_, 100, 1500));
    menuProvider_ = menu_->Add(new SDK::UI::MenuList("Provider", "Scan Provider", { "SDK::ObjectManager (Raw RAM)", "SDK::GameObjects Facade" }, scanProviderIndex_));

    auto* specificSub = menu_->AddSubMenu(new SDK::UI::Menu("SpecificLists", "GameObjects Specific Lists"));
    for (auto& opt : listOptions_) {
        opt.MenuControl = specificSub->Add(new SDK::UI::MenuBool(opt.Name, opt.DisplayName, opt.Enabled));
    }

    auto* filters = menu_->AddSubMenu(new SDK::UI::Menu("Filters", "Category Filters"));
    menuScanAll_ = filters->Add(new SDK::UI::MenuBool("ScanAll", "Scan All GameObjects", scanRawGameObjects_));
    menuScanHeroes_ = filters->Add(new SDK::UI::MenuBool("ScanHeroes", "Heroes (AIHeroClient)", scanHeroes_));
    menuScanMinions_ = filters->Add(new SDK::UI::MenuBool("ScanMinions", "Minions & Pets", scanMinions_));
    // REMOVED: Turret scan disabled by user request.
    // menuScanTurrets_ = filters->Add(new SDK::UI::MenuBool("ScanTurrets", "Turrets", scanTurrets_));
    menuScanMissiles_ = filters->Add(new SDK::UI::MenuBool("ScanMissiles", "Missiles", scanMissiles_));
    menuFilterClutter_ = filters->Add(new SDK::UI::MenuBool("FilterClutter", "Filter Clutter (FX, MoveTo)", filterClutter_));

    auto* logger = menu_->AddSubMenu(new SDK::UI::Menu("EventLogger", "Event Logger"));
    menuLogEnabled_ = logger->Add(new SDK::UI::MenuBool("LogEnabled", "Enable Event Logging", logEnabled_));
    menuLogSource_ = logger->Add(new SDK::UI::MenuList(
        "LogSource",
        "Log Source",
        { "Local Player Only", "Player + Allies", "Enemies Only", "Everyone" },
        logSourceIndex_));
    menuLogVerbose_ = logger->Add(new SDK::UI::MenuBool("LogVerbose", "Verbose (dump every arg field)", logVerbose_));
    menuLogRaw_ = logger->Add(new SDK::UI::MenuBool("LogRaw", "Include Raw Registers (rcx/rdx/xmm/stack)", logRaw_));
    menuLogSkipAA_ = logger->Add(new SDK::UI::MenuBool("LogSkipAA", "Skip Auto Attacks", logSkipAutoAttacks_));
    menuLogToFile_ = logger->Add(new SDK::UI::MenuBool("LogToFile", "Write to Debug Log", logToFile_));

    auto* loggerEvents = logger->AddSubMenu(new SDK::UI::Menu("LoggerEvents", "Tracked Events"));
    menuLogProcessSpell_ = loggerEvents->Add(new SDK::UI::MenuBool("EvProcessSpell", "OnProcessSpell", logProcessSpell_));
    menuLogDoCast_ = loggerEvents->Add(new SDK::UI::MenuBool("EvDoCast", "OnDoCast", logDoCast_));
    menuLogFinishCast_ = loggerEvents->Add(new SDK::UI::MenuBool("EvFinishCast", "OnFinishCast", logFinishCast_));
    menuLogSpellImpact_ = loggerEvents->Add(new SDK::UI::MenuBool("EvSpellImpact", "OnSpellImpact", logSpellImpact_));
    menuLogCastSpell_ = loggerEvents->Add(new SDK::UI::MenuBool("EvCastSpell", "OnProcessCastSpell", logCastSpell_));
    menuLogStopCast_ = loggerEvents->Add(new SDK::UI::MenuBool("EvStopCast", "OnStopCast", logStopCast_));
    menuLogAnimation_ = loggerEvents->Add(new SDK::UI::MenuBool("EvAnimation", "OnPlayAnimation", logAnimation_));
    menuLogBuffAdd_ = loggerEvents->Add(new SDK::UI::MenuBool("EvBuffAdd", "OnBuffAdd", logBuffAdd_));
    menuLogBuffRemove_ = loggerEvents->Add(new SDK::UI::MenuBool("EvBuffRemove", "OnBuffRemove", logBuffRemove_));
    menuLogNewPath_ = loggerEvents->Add(new SDK::UI::MenuBool("EvNewPath", "OnNewPath", logNewPath_));

    menuInspector_ = menu_->Add(new SDK::UI::MenuRuntime("LiveInspector", "Open Live Object Inspector", &OnMenuBridge, this, 620.0f));

    menu_->Attach();
}

inline void DeveloperToolsPlugin::OnUnload() {
    SDK::Events::RemoveOnProcessSpell(&DeveloperToolsPlugin::OnProcessSpellCast);
    SDK::Events::RemoveOnDoCast(&DeveloperToolsPlugin::OnDoCastEvent);
    SDK::Events::RemoveOnFinishCast(&DeveloperToolsPlugin::OnFinishCastEvent);
    SDK::Events::RemoveOnSpellImpact(&DeveloperToolsPlugin::OnSpellImpactEvent);
    SDK::Events::RemoveOnProcessCastSpell(&DeveloperToolsPlugin::OnCastSpellEvent);
    SDK::Events::RemoveOnStopCast(&DeveloperToolsPlugin::OnStopCastEvent);
    SDK::Events::RemoveOnPlayAnimation(&DeveloperToolsPlugin::OnPlayAnimationEvent);
    SDK::Events::RemoveOnBuffAdd(&DeveloperToolsPlugin::OnBuffAddEvent);
    SDK::Events::RemoveOnBuffRemove(&DeveloperToolsPlugin::OnBuffRemoveEvent);
    SDK::Events::RemoveOnNewPath(&DeveloperToolsPlugin::OnNewPathEvent);
    SDK::Events::RemoveOnDeleteObject(&DeveloperToolsPlugin::OnObjectDelete);

    for (std::size_t index = 0; index < tabs_.size(); ++index) {
        DevTools::TabFaultCapture fault{};
        if (!DevTools::InvokeTabUnloadGuarded(tabs_[index].get(), &fault)) {
            NightSharpDebug::Logf(
                "[DeveloperTools] isolated tab fault tab=%s callback=unload code=0x%08X address=0x%llX",
                tabs_[index]->GetTabName(),
                static_cast<unsigned>(fault.code),
                static_cast<unsigned long long>(fault.address));
        }
    }
    tabs_.clear();
    tabRuntimeStates_.clear();
    InitializePerformanceProfiler(0);

    DestroyNativeMenu();
    s_instance = nullptr;
}

inline void DeveloperToolsPlugin::OnUpdate() {
    const std::int64_t totalStartCounter = ProfilerCounterNow();
    SyncMenuSettings();
    if (!enabled_) {
        pKeyPressedLast_ = false;
        mKeyPressedLast_ = false;
        SetFocusedObject(SDK::GameObject{});
        RecordTabDuration(
            tabs_.size(), TabCallbackKind::Update, totalStartCounter);
        return;
    }

    const int now = SDK::Variables::TickCount();

    // Scan focused object every 100ms
    if (now - lastFocusedScanTick_ >= 100) {
        lastFocusedScanTick_ = now;
        SetFocusedObject(ScanFocusedObject());

        // Update live validity of snapshots only once per 1000ms (1 second)
        if (now - lastSnapshotTick_ >= 1000) {
            lastSnapshotTick_ = now;
            std::unordered_set<std::uint32_t> liveNetworkIds;
            const auto liveObjects =
                SDK::ObjectManager::Get<SDK::GameObject>();
            liveNetworkIds.reserve(liveObjects.size());
            for (const auto& obj : liveObjects) {
                if (!obj.IsValid()) continue;
                const std::uint32_t networkId =
                    ::Core::Objects::ReadNetworkId(obj.Address());
                if (networkId != 0 && networkId != 0xFFFFFFFFu) {
                    liveNetworkIds.insert(networkId);
                }
            }
            for (auto& snap : snapshots_) {
                snap.isUnderlyingValid =
                    liveNetworkIds.find(snap.networkId) != liveNetworkIds.end();
            }
        }
    }

    // Key 'M' snapshot trigger
    bool mDown = (GetAsyncKeyState('M') & 0x8000) != 0;
    if (mDown && !mKeyPressedLast_) {
        const auto obj = GetFocusedObject();
        if (obj.IsValid()) {
            TakeSnapshot(obj);
        }
    }
    mKeyPressedLast_ = mDown;

    // Object detection owns the optional 3D overlay and remains a background
    // producer.  The other inspectors read rich object-specific memory only
    // while their tab is selected; running all of them immediately after load
    // was the source of the access violation reported in the crash log.
    if (!tabs_.empty()) {
        RunTabUpdate(0);
    }
    if (activeTabIdx_ > 0 && activeTabIdx_ < static_cast<int>(tabs_.size())) {
        RunTabUpdate(static_cast<std::size_t>(activeTabIdx_));
    }

    PruneTrackedObjects(now);

    // Delegate copy hotkey to active tab
    bool isDown = (GetAsyncKeyState('P') & 0x8000) != 0;
    if (isDown && !pKeyPressedLast_) {
        if (activeTabIdx_ >= 0 && activeTabIdx_ < static_cast<int>(tabs_.size())) {
            RunTabCopy(static_cast<std::size_t>(activeTabIdx_));
        }
    }
    pKeyPressedLast_ = isDown;
    RecordTabDuration(
        tabs_.size(), TabCallbackKind::Update, totalStartCounter);
}

inline void DeveloperToolsPlugin::OnRender() {
    if (!enabled_) {
        return;
    }
    const std::int64_t totalStartCounter = ProfilerCounterNow();
    for (std::size_t index = 0; index < tabs_.size(); ++index) {
        RunTabRender(index);
    }
    RecordTabDuration(
        tabs_.size(), TabCallbackKind::Render, totalStartCounter);
    RotatePerformanceWindows();
}

inline void DeveloperToolsPlugin::OnMenu() {
    const std::int64_t totalStartCounter = ProfilerCounterNow();
    if (ImGui::Checkbox("Enable Developer Tools", &enabled_)) {
        if (menuEnabled_) menuEnabled_->SetValue(enabled_);
    }
    if (!enabled_) {
        RecordTabDuration(
            tabs_.size(), TabCallbackKind::Menu, totalStartCounter);
        return;
    }
    if (ImGui::SliderInt("Max object dist from cursor", &maxRange_, 100, 1500)) {
        if (menuMaxRange_) menuMaxRange_->SetValue(maxRange_);
    }

    ImGui::Separator();
    DrawPerformanceProfiler();
    ImGui::Separator();

    if (ImGui::BeginTabBar("DeveloperToolsTabBar")) {
        for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
            if (ImGui::BeginTabItem(tabs_[i]->GetTabName())) {
                activeTabIdx_ = i;
                const auto index = static_cast<std::size_t>(i);
                const auto state = CopyTabRuntimeState(index);
                const int now = SDK::Variables::TickCount();
                if (state.quarantined || now < state.retryAfterTick) {
                    ImGui::TextColored(
                        ImVec4(1.0f, 0.45f, 0.2f, 1.0f),
                        state.quarantined
                            ? "This tab was isolated after repeated invalid-memory reads."
                            : "This tab hit an invalid object and is cooling down before retry.");
                    ImGui::Text("Exception: 0x%08X at 0x%llX",
                                static_cast<unsigned>(state.lastFault.code),
                                static_cast<unsigned long long>(state.lastFault.address));
                    ImGui::PushID(i);
                    if (ImGui::Button("Retry tab")) {
                        ResetTabFault(index);
                    }
                    ImGui::PopID();
                } else {
                    RunTabDraw(index);
                }
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    // Render any floating snapshot inspector windows
    std::vector<std::uint32_t> closedWindowNetIds;
    for (auto netId : openInspectWindows_) {
        auto it = std::find_if(snapshots_.begin(), snapshots_.end(), [&](const DevTools::ObjectSnapshot& s) {
            return s.networkId == netId;
        });
        if (it == snapshots_.end()) continue;

        char title[128];
        std::snprintf(title, sizeof(title), "Inspect Snapshot - %s (NetId: %u)###SnapInspect%u", it->characterName.c_str(), it->networkId, it->networkId);
        bool open = true;
        if (ImGui::Begin(title, &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
            ImGui::Text("Name: %s | CharName: %s", it->name.c_str(), it->characterName.c_str());
            
            const char* teamStr = "Unknown";
            if (it->team == SDK::GameObjectTeam::Order) teamStr = "Order (Blue)";
            else if (it->team == SDK::GameObjectTeam::Chaos) teamStr = "Chaos (Red)";
            else if (it->team == SDK::GameObjectTeam::Neutral) teamStr = "Neutral (Jungle/Camp)";
            
            const char* typeStr = ObjectTypeToString(it->type);
            
            ImGui::Text("Type: %s | Team: %s", typeStr, teamStr);
            ImGui::Text("Health: %.1f / %.1f | Mana: %.1f / %.1f", it->health, it->maxHealth, it->mana, it->maxMana);
            ImGui::Text("Live Status: %s", it->isUnderlyingValid ? "Live (Valid)" : "Gone (Invalid)");
            
            ImGui::Separator();
            
            if (ImGui::Button("Copy Snapshot Info to Clipboard")) {
                std::string dump = "=== SNAPSHOT INSPECTOR ===\n";
                char buf[512];
                std::snprintf(buf, sizeof(buf), "Name: %s | CharName: %s | NetId: %u | Addr: 0x%llX\n",
                              it->name.c_str(), it->characterName.c_str(), it->networkId, static_cast<unsigned long long>(it->address));
                dump += buf;
                
                dump += "\n=== BUFFS ===\n";
                for (const auto& b : it->buffs) {
                    std::snprintf(buf, sizeof(buf), "Buff: %s | Stacks: %d | Type: %d | Live: %d\n", b.name, b.stacks, b.type, b.live ? 1 : 0);
                    dump += buf;
                }
                
                dump += "\n=== SPELLS ===\n";
                for (const auto& s : it->spells) {
                    std::snprintf(buf, sizeof(buf), "Spell: %s | Slot: %d | Level: %d | CD: %.2f | Mana: %.1f\n", s.name, static_cast<int>(s.slot), s.level, s.remainingCooldown, s.manaCost);
                    dump += buf;
                }
                ImGui::SetClipboardText(dump.c_str());
                NightSharpDebug::Logf("[Dev] Copied snapshot dump to Clipboard!");
            }
            
            if (!it->buffs.empty() && ImGui::CollapsingHeader("Buffs", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::BeginTable("SnapBuffsTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Buff Name");
                    ImGui::TableSetupColumn("Stacks");
                    ImGui::TableSetupColumn("Type");
                    ImGui::TableSetupColumn("Duration");
                    ImGui::TableSetupColumn("Live");
                    ImGui::TableHeadersRow();
                    for (const auto& b : it->buffs) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(b.name);
                        ImGui::TableNextColumn();
                        ImGui::Text("%d", b.stacks);
                        ImGui::TableNextColumn();
                        ImGui::Text("%d", b.type);
                        ImGui::TableNextColumn();
                        float dur = b.endTime > b.startTime ? b.endTime - b.startTime : 0.0f;
                        ImGui::Text("%.2fs", dur);
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(b.live ? "1" : "0");
                    }
                    ImGui::EndTable();
                }
            }
            
            if (!it->spells.empty() && ImGui::CollapsingHeader("Spellbook", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::BeginTable("SnapSpellsTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Slot");
                    ImGui::TableSetupColumn("Spell Name");
                    ImGui::TableSetupColumn("Level");
                    ImGui::TableSetupColumn("CD (Rem)");
                    ImGui::TableSetupColumn("Ammo");
                    ImGui::TableSetupColumn("Mana");
                    ImGui::TableHeadersRow();
                    for (const auto& s : it->spells) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("%d", static_cast<int>(s.slot));
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(s.name);
                        ImGui::TableNextColumn();
                        ImGui::Text("%d", s.level);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.2f", s.remainingCooldown);
                        ImGui::TableNextColumn();
                        ImGui::Text("%d / %d", s.ammo, s.maxAmmo);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.0f", s.manaCost);
                    }
                    ImGui::EndTable();
                }
            }
        }
        ImGui::End();
        if (!open) {
            closedWindowNetIds.push_back(netId);
        }
    }

    for (auto netId : closedWindowNetIds) {
        openInspectWindows_.erase(netId);
    }

    // Render any floating object event log windows
    std::vector<std::uint32_t> closedEventWindows;
    for (auto netId : CopyOpenEventLogWindows()) {
        std::string charName = "Object";
        for (const auto& obj : SDK::ObjectManager::Get<SDK::GameObject>()) {
            if (obj.IsValid() && static_cast<std::uint32_t>(obj.NetworkId()) == netId) {
                charName = obj.CharacterName();
                break;
            }
        }
        if (charName == "Object") {
            auto itSnap = std::find_if(snapshots_.begin(), snapshots_.end(), [&](const DevTools::ObjectSnapshot& s) {
                return s.networkId == netId;
            });
            if (itSnap != snapshots_.end()) {
                charName = itSnap->characterName;
            }
        }

        char title[128];
        std::snprintf(title, sizeof(title), "Event Logs - %s (NetId: %u)###EventLog%u", charName.c_str(), netId, netId);
        bool open = true;
        if (ImGui::Begin(title, &open, ImGuiWindowFlags_NoCollapse)) {
            auto logs = CopyEventLog(netId);
            static char searchFilter[64] = {};
            char searchId[128];
            std::snprintf(searchId, sizeof(searchId), "Search Logs##Search%u", netId);
            ImGui::InputText(searchId, searchFilter, sizeof(searchFilter));

            ImGui::SameLine();
            char clearId[128];
            std::snprintf(clearId, sizeof(clearId), "Clear##Clear%u", netId);
            if (ImGui::Button(clearId)) {
                ClearEventLog(netId);
                logs.clear();
            }

            ImGui::SameLine();
            char copyId[128];
            std::snprintf(copyId, sizeof(copyId), "Copy All##Copy%u", netId);
            if (ImGui::Button(copyId)) {
                std::string dump = "=== EVENT LOGS FOR " + charName + " (NetId: " + std::to_string(netId) + ") ===\n";
                for (const auto& entry : logs) {
                    char line[512];
                    std::snprintf(line, sizeof(line), "[%.2fs] [%s] %s\n", entry.time, entry.eventName.c_str(), entry.details.c_str());
                    dump += line;
                }
                ImGui::SetClipboardText(dump.c_str());
                NightSharpDebug::Logf("[Dev] Copied logs to Clipboard.");
            }

            if (ImGui::BeginChild("EventLogScroll", ImVec2(0, 300), true)) {
                for (const auto& entry : logs) {
                    if (searchFilter[0]) {
                        std::string filterLower = searchFilter;
                        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
                        
                        std::string eventLower = entry.eventName;
                        std::transform(eventLower.begin(), eventLower.end(), eventLower.begin(), ::tolower);
                        std::string detailsLower = entry.details;
                        std::transform(detailsLower.begin(), detailsLower.end(), detailsLower.begin(), ::tolower);

                        if (eventLower.find(filterLower) == std::string::npos && detailsLower.find(filterLower) == std::string::npos) {
                            continue;
                        }
                    }

                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "[%.2fs] ", entry.time);
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "[%s] ", entry.eventName.c_str());
                    ImGui::SameLine();
                    ImGui::TextUnformatted(entry.details.c_str());
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
        if (!open) {
            closedEventWindows.push_back(netId);
        }
    }
    for (auto netId : closedEventWindows) {
        SetEventLogOpen(netId, false);
    }
    RecordTabDuration(
        tabs_.size(), TabCallbackKind::Menu, totalStartCounter);
}

inline void DeveloperToolsPlugin::OnProcessSpellCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->IsEventLogOpen(netId)) {
            char details[256];
            std::snprintf(details, sizeof(details), "Spell: %s | TargetNetId: %u | Speed: %.1f | EndPos: (%.1f, %.1f, %.1f)", 
                          args.SpellName ? args.SpellName : "Unknown", args.TargetNetworkId, args.MissileSpeed, args.EndPosition.x, args.EndPosition.y, args.EndPosition.z);
            s_instance->LogEventForObject(netId, "ProcessSpellCast", details);
        }
        s_instance->DispatchTabEvent(
            &DevTools::IDeveloperTab::OnProcessSpellCast, args, "process-spell");
    }
}

inline void DeveloperToolsPlugin::OnDoCastEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->IsEventLogOpen(netId)) {
            char details[256];
            std::snprintf(details, sizeof(details), "Spell: %s | TargetNetId: %u | Speed: %.1f | EndPos: (%.1f, %.1f, %.1f)", 
                          args.SpellName ? args.SpellName : "Unknown", args.TargetNetworkId, args.MissileSpeed, args.EndPosition.x, args.EndPosition.y, args.EndPosition.z);
            s_instance->LogEventForObject(netId, "DoCast", details);
        }
        s_instance->DispatchTabEvent(
            &DevTools::IDeveloperTab::OnDoCastEvent, args, "do-cast");
    }
}

inline void DeveloperToolsPlugin::OnFinishCastEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->IsEventLogOpen(netId)) {
            char details[256];
            std::snprintf(details, sizeof(details), "Spell: %s | TargetNetId: %u", 
                          args.SpellName ? args.SpellName : "Unknown", args.TargetNetworkId);
            s_instance->LogEventForObject(netId, "FinishCast", details);
        }
        s_instance->DispatchTabEvent(
            &DevTools::IDeveloperTab::OnFinishCastEvent, args, "finish-cast");
    }
}

inline void DeveloperToolsPlugin::OnSpellImpactEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->IsEventLogOpen(netId)) {
            char details[256];
            std::snprintf(details, sizeof(details), "Spell: %s | TargetNetId: %u", 
                          args.SpellName ? args.SpellName : "Unknown", args.TargetNetworkId);
            s_instance->LogEventForObject(netId, "SpellImpact", details);
        }
        s_instance->DispatchTabEvent(
            &DevTools::IDeveloperTab::OnSpellImpactEvent, args, "spell-impact");
    }
}

inline void DeveloperToolsPlugin::OnCastSpellEvent(const SDK::Events::CastSpellEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->IsEventLogOpen(netId)) {
            char details[256];
            std::snprintf(details, sizeof(details), "Slot: %d | TargetNetId: %u", 
                          args.Slot, args.TargetNetworkId);
            s_instance->LogEventForObject(netId, "CastSpell", details);
        }
        s_instance->DispatchTabEvent(
            &DevTools::IDeveloperTab::OnCastSpellEvent, args, "cast-spell");
    }
}

inline void DeveloperToolsPlugin::OnStopCastEvent(const SDK::Events::StopCastEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->IsEventLogOpen(netId)) {
            char details[256];
            std::snprintf(details, sizeof(details), "Slot: %d | DestroyMissile: %d | KeepAnim: %d", 
                          args.Slot, args.DestroyMissile ? 1 : 0, args.KeepAnimationPlaying ? 1 : 0);
            s_instance->LogEventForObject(netId, "StopCast", details);
        }
        s_instance->DispatchTabEvent(
            &DevTools::IDeveloperTab::OnStopCastEvent, args, "stop-cast");
    }
}

inline void DeveloperToolsPlugin::OnPlayAnimationEvent(const SDK::Events::PlayAnimationEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->IsEventLogOpen(netId)) {
            char details[256];
            std::snprintf(details, sizeof(details), "Animation: %s | Id: %d", 
                          args.Animation ? args.Animation : "Unknown", args.AnimationId);
            s_instance->LogEventForObject(netId, "PlayAnimation", details);
        }
        s_instance->DispatchTabEvent(
            &DevTools::IDeveloperTab::OnPlayAnimationEvent, args, "animation");
    }
}

inline void DeveloperToolsPlugin::OnBuffAddEvent(const SDK::Events::BuffEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->IsEventLogOpen(netId)) {
            char details[256];
            std::snprintf(details, sizeof(details), "Buff: %s | Count: %d", 
                          args.BuffName ? args.BuffName : "Unknown", args.Count);
            s_instance->LogEventForObject(netId, "BuffAdd", details);
        }
        s_instance->DispatchTabEvent(
            &DevTools::IDeveloperTab::OnBuffAddEvent, args, "buff-add");
    }
}

inline void DeveloperToolsPlugin::OnBuffRemoveEvent(const SDK::Events::BuffEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->IsEventLogOpen(netId)) {
            char details[256];
            std::snprintf(details, sizeof(details), "Buff: %s | Count: %d", 
                          args.BuffName ? args.BuffName : "Unknown", args.Count);
            s_instance->LogEventForObject(netId, "BuffRemove", details);
        }
        s_instance->DispatchTabEvent(
            &DevTools::IDeveloperTab::OnBuffRemoveEvent, args, "buff-remove");
    }
}

inline void DeveloperToolsPlugin::OnNewPathEvent(const SDK::Events::NewPathEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->IsEventLogOpen(netId)) {
            char details[256];
            std::snprintf(details, sizeof(details), "Dash: %d | Speed: %.1f | Waypoints: %d", 
                          args.IsDash ? 1 : 0, args.Speed, args.PathCount);
            s_instance->LogEventForObject(netId, "NewPath", details);
        }
        s_instance->DispatchTabEvent(
            &DevTools::IDeveloperTab::OnNewPathEvent, args, "new-path");
    }
}

inline void DeveloperToolsPlugin::OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const auto focused = s_instance->GetFocusedObject();
        if (focused.IsValid() &&
            static_cast<std::uint32_t>(focused.NetworkId()) == args.Sender.NetworkId) {
            s_instance->SetFocusedObject(SDK::GameObject{});
        }
        s_instance->DispatchTabEvent(
            &DevTools::IDeveloperTab::OnDeleteObject, args, "object-delete");
    }
}

inline void DeveloperToolsPlugin::SyncMenuSettings() {
    if (menuEnabled_) enabled_ = menuEnabled_->Value;
    if (menuMaxRange_) maxRange_ = menuMaxRange_->Value;
    if (menuProvider_) scanProviderIndex_ = menuProvider_->Index;
    for (auto& opt : listOptions_) {
        if (opt.MenuControl) opt.Enabled = opt.MenuControl->Value;
    }
    if (menuScanAll_) scanRawGameObjects_ = menuScanAll_->Value;
    if (menuScanHeroes_) scanHeroes_ = menuScanHeroes_->Value;
    if (menuScanMinions_) scanMinions_ = menuScanMinions_->Value;
    // REMOVED: Turret scan disabled by user request.
    // if (menuScanTurrets_) scanTurrets_ = menuScanTurrets_->Value;
    scanTurrets_ = false;
    if (menuScanMissiles_) scanMissiles_ = menuScanMissiles_->Value;
    if (menuFilterClutter_) filterClutter_ = menuFilterClutter_->Value;

    if (menuLogEnabled_) logEnabled_ = menuLogEnabled_->Value;
    if (menuLogSource_) logSourceIndex_ = menuLogSource_->Index;
    if (menuLogVerbose_) logVerbose_ = menuLogVerbose_->Value;
    if (menuLogRaw_) logRaw_ = menuLogRaw_->Value;
    if (menuLogSkipAA_) logSkipAutoAttacks_ = menuLogSkipAA_->Value;
    if (menuLogToFile_) logToFile_ = menuLogToFile_->Value;
    if (menuLogProcessSpell_) logProcessSpell_ = menuLogProcessSpell_->Value;
    if (menuLogDoCast_) logDoCast_ = menuLogDoCast_->Value;
    if (menuLogFinishCast_) logFinishCast_ = menuLogFinishCast_->Value;
    if (menuLogSpellImpact_) logSpellImpact_ = menuLogSpellImpact_->Value;
    if (menuLogCastSpell_) logCastSpell_ = menuLogCastSpell_->Value;
    if (menuLogStopCast_) logStopCast_ = menuLogStopCast_->Value;
    if (menuLogAnimation_) logAnimation_ = menuLogAnimation_->Value;
    if (menuLogBuffAdd_) logBuffAdd_ = menuLogBuffAdd_->Value;
    if (menuLogBuffRemove_) logBuffRemove_ = menuLogBuffRemove_->Value;
    if (menuLogNewPath_) logNewPath_ = menuLogNewPath_->Value;
}

inline void DeveloperToolsPlugin::DestroyNativeMenu() {
    if (menu_) {
        SDK::UI::MenuManager::Instance().Remove(menu_);
        delete menu_;
        menu_ = nullptr;
        menuEnabled_ = nullptr;
        menuMaxRange_ = nullptr;
        menuProvider_ = nullptr;
        for (auto& opt : listOptions_) {
            opt.MenuControl = nullptr;
        }
        menuScanAll_ = nullptr;
        menuScanHeroes_ = nullptr;
        menuScanMinions_ = nullptr;
        menuScanTurrets_ = nullptr;
        menuScanMissiles_ = nullptr;
        menuFilterClutter_ = nullptr;
        menuInspector_ = nullptr;
        menuLogEnabled_ = nullptr;
        menuLogSource_ = nullptr;
        menuLogVerbose_ = nullptr;
        menuLogRaw_ = nullptr;
        menuLogSkipAA_ = nullptr;
        menuLogToFile_ = nullptr;
        menuLogProcessSpell_ = nullptr;
        menuLogDoCast_ = nullptr;
        menuLogFinishCast_ = nullptr;
        menuLogSpellImpact_ = nullptr;
        menuLogCastSpell_ = nullptr;
        menuLogStopCast_ = nullptr;
        menuLogAnimation_ = nullptr;
        menuLogBuffAdd_ = nullptr;
        menuLogBuffRemove_ = nullptr;
        menuLogNewPath_ = nullptr;
    }
}

} // namespace Plugins
