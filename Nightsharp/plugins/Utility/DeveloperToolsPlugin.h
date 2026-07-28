#pragma once

#include "../IPlugin.h"
#include "../../SDK/SDK.h"

namespace SDK::GameObjects::detail {
    inline std::vector<AITurretClient> TurretsList;
    inline std::vector<AITurretClient> AllyTurretsList;
    inline std::vector<AITurretClient> EnemyTurretsList;
    inline std::vector<BarracksDampenerClient> InhibitorsList;
    inline std::vector<BarracksDampenerClient> AllyInhibitorsList;
    inline std::vector<BarracksDampenerClient> EnemyInhibitorsList;
    inline std::vector<HQClient> NexusList;
    inline HQClient AllyNexusObject;
    inline HQClient EnemyNexusObject;
}

#include "../../DebugLog.h"
#include "../../imgui/imgui.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
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
    bool scanTurrets_ = true;
    bool scanMissiles_ = true;
    bool filterClutter_ = true;
    std::unordered_map<std::uint32_t, int> trackedObjectTicks_;
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
    bool logBuffUpdate_ = false;
    bool logNewPath_ = false;
    char logNameFilter_[64] = {};
    int activeTabIdx_ = 0;
    std::vector<DevTools::ObjectSnapshot> snapshots_;
    std::set<std::uint32_t> openInspectWindows_;
    std::set<std::uint32_t> openEventLogWindows_;
    std::unordered_map<std::uint32_t, std::vector<DevTools::EventLogEntry>> objectEventLogs_;
    bool mKeyPressedLast_ = false;

    void LogEventForObject(std::uint32_t netId, const std::string& eventName, const std::string& details) {
        auto it = objectEventLogs_.find(netId);
        if (it != objectEventLogs_.end()) {
            DevTools::EventLogEntry entry;
            entry.tick = SDK::Game::TickCount();
            entry.time = SDK::Game::Time();
            entry.eventName = eventName;
            entry.details = details;
            
            it->second.push_back(entry);
            if (it->second.size() > 500) {
                it->second.erase(it->second.begin());
            }
        }
    }

    DevTools::ObjectSnapshot CreateSnapshot(const SDK::GameObject& obj) const {
        DevTools::ObjectSnapshot snap;
        snap.networkId = static_cast<std::uint32_t>(obj.NetworkId());
        snap.address = obj.Address();
        snap.name = obj.Name();
        snap.characterName = obj.CharacterName();
        snap.type = obj.Type();
        snap.team = obj.Team();
        snap.position = obj.Position();
        snap.snapshotTick = SDK::Game::TickCount();
        snap.isUnderlyingValid = true;

        const bool isAttackable = obj.IsHero() || obj.IsMinion() || obj.IsTurret() || 
                                 obj.Type() == ::Core::Objects::ObjectType::BarracksDampenerClient ||
                                 obj.Type() == ::Core::Objects::ObjectType::HQClient;
        if (isAttackable) {
            const auto att = SDK::AttackableUnit(obj.Handle());
            snap.health = att.Health();
            snap.maxHealth = att.MaxHealth();
            snap.allShield = att.AllShield();
            snap.physShield = att.PhysicalShield();
            snap.magShield = att.MagicalShield();
            snap.healthRegen = att.HealthRegenRate();
        }

        const bool hasBuffs = obj.IsHero() || obj.IsMinion() || obj.IsTurret();
        if (hasBuffs) {
            const auto aiBase = SDK::AIBaseClient(obj.Handle());
            snap.mana = aiBase.Mana();
            snap.maxMana = aiBase.MaxMana();
            snap.armor = aiBase.Armor();
            snap.spellBlock = aiBase.SpellBlock();
            snap.attackDamage = aiBase.TotalAttackDamage();
            snap.baseAD = aiBase.BaseAttackDamage();
            snap.bonusAD = aiBase.BonusAttackDamage();
            snap.abilityPower = aiBase.TotalMagicalDamage();
            snap.attackRange = aiBase.AttackRange();
            snap.moveSpeed = aiBase.MoveSpeed();
            snap.attackSpeedMod = aiBase.AttackSpeedMod();
            snap.crit = aiBase.Crit();
            snap.bonusArmor = aiBase.BonusArmor();
            snap.bonusSpellBlock = aiBase.BonusSpellBlock();
            snap.lethality = aiBase.Lethality();
            snap.flatArmorPen = aiBase.FlatArmorPenetrationMod();
            snap.percentArmorPen = aiBase.PercentArmorPenetrationMod();
            snap.flatMagicPen = aiBase.FlatMagicPenetrationMod();
            snap.percentMagicPen = aiBase.PercentMagicPenetrationMod();
            snap.level = aiBase.Level();

            uintptr_t buffs[256] = {};
            const int buffCount = CoreBuffs::Enumerate(obj.Address(), buffs, 256);
            const float gameTime = SDK::Game::Time();
            char nameBuf[96] = {};
            for (int i = 0; i < buffCount; ++i) {
                CoreBuffs::BuffRef buff{ buffs[i] };
                if (!buff.ReadName(nameBuf, sizeof(nameBuf))) continue;

                DevTools::SnapshotBuff b;
                strncpy_s(b.name, nameBuf, _TRUNCATE);
                b.count = aiBase.GetBuffCount(nameBuf);
                b.stacks = buff.GetStacks();
                b.type = buff.GetType();
                b.startTime = buff.GetStartTime();
                b.endTime = buff.GetEndTime();
                b.address = buff.address;
                b.live = buff.IsActive(gameTime);
                snap.buffs.push_back(b);
            }
        }

        if (hasBuffs) {
            const auto aiBase = SDK::AIBaseClient(obj.Handle());
            const float gameTime = SDK::Game::Time();

            static const SDK::SpellSlot slots[] = {
                SDK::SpellSlot::Q, SDK::SpellSlot::W, SDK::SpellSlot::E, SDK::SpellSlot::R,
                SDK::SpellSlot::Summoner1, SDK::SpellSlot::Summoner2,
                SDK::SpellSlot::Item1, SDK::SpellSlot::Item2, SDK::SpellSlot::Item3,
                SDK::SpellSlot::Item4, SDK::SpellSlot::Item5, SDK::SpellSlot::Item6,
                SDK::SpellSlot::Trinket
            };

            for (const auto& slot : slots) {
                auto spell = aiBase.GetSpell(slot);
                if (!spell.IsValid()) continue;

                DevTools::SnapshotSpell s;
                s.slot = slot;
                strncpy_s(s.name, spell.Name().c_str(), _TRUNCATE);
                s.level = spell.Level();
                s.ammo = spell.Ammo();
                s.maxAmmo = spell.MaxAmmo();
                s.cooldown = spell.Cooldown();
                s.remainingCooldown = spell.RemainingCooldown(gameTime);
                s.manaCost = spell.ManaCost();
                s.state = static_cast<std::uint32_t>(spell.State(gameTime));
                snap.spells.push_back(s);
            }
        }

        return snap;
    }

    void TakeSnapshot(const SDK::GameObject& obj) {
        auto it = std::find_if(snapshots_.begin(), snapshots_.end(), [&](const DevTools::ObjectSnapshot& s) {
            return s.networkId == static_cast<std::uint32_t>(obj.NetworkId());
        });

        DevTools::ObjectSnapshot snap = CreateSnapshot(obj);
        if (it != snapshots_.end()) {
            *it = snap;
            NightSharpDebug::Logf("[Dev] Updated snapshot for %s (NetId: %u)", snap.characterName.c_str(), snap.networkId);
        } else {
            snapshots_.push_back(snap);
            NightSharpDebug::Logf("[Dev] Added new snapshot for %s (NetId: %u)", snap.characterName.c_str(), snap.networkId);
        }
    }

    SDK::GameObject GetFocusedObject() const {
        const auto player = SDK::ObjectManager::Player();
        const Vec3 cursorPos = SDK::Game::CursorPos();
        const float rangeSqr = static_cast<float>(maxRange_ * maxRange_);

        SDK::GameObject closestObj{};
        float closestDistSqr = rangeSqr;

        for (const auto& obj : SDK::ObjectManager::Get<SDK::GameObject>()) {
            if (!obj.IsValid()) continue;
            if (player.IsValid() && obj.Address() == player.Address()) continue;

            const std::string& name = GetObjectName(obj);
            const std::string& charName = GetObjectCharacterName(obj);
            if (IsClutter(obj, name, charName)) continue;

            const float distSqr = obj.Position().DistanceSqr(cursorPos);
            if (distSqr < closestDistSqr) {
                closestDistSqr = distSqr;
                closestObj = obj;
            }
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
    SDK::UI::MenuBool* menuLogBuffUpdate_ = nullptr;
    SDK::UI::MenuBool* menuLogNewPath_ = nullptr;

private:
    std::vector<std::unique_ptr<DevTools::IDeveloperTab>> tabs_;
    static inline DeveloperToolsPlugin* s_instance = nullptr;

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
        if (obj.IsAlly()) return "Ally";
        if (obj.IsEnemy()) return "Enemy";
        const auto team = obj.Team();
        if (team == SDK::GameObjectTeam::Neutral) return "Neutral";
        if (team == SDK::GameObjectTeam::Order) return "Blue (100)";
        if (team == SDK::GameObjectTeam::Chaos) return "Red (200)";
        return "Unknown";
    }

    static void GetStatusString(const SDK::GameObject& obj, char* buf, size_t maxLen) {
        if (!obj.IsValid()) {
            std::snprintf(buf, maxLen, "Unknown");
            return;
        }
        int len = 0;
        if (obj.IsDead()) len += std::snprintf(buf + len, maxLen - len, "Dead");
        else len += std::snprintf(buf + len, maxLen - len, "Alive");

        if (!obj.IsVisible()) len += std::snprintf(buf + len, maxLen - len, ", Fog");
        if (!obj.IsTargetable()) len += std::snprintf(buf + len, maxLen - len, ", Untargetable");
        if (obj.IsInvulnerable()) len += std::snprintf(buf + len, maxLen - len, ", Invulnerable");

        if (obj.IsHero() || obj.IsMinion() || obj.IsTurret()) {
            SDK::AIBaseClient ai(obj.Handle());
            if (ai.IsValid()) {
                len += std::snprintf(buf + len, maxLen - len, ", HP:%.0f/%.0f(%.0f%%)",
                                     ai.Health(), ai.MaxHealth(), ai.HealthPercent());
            }
        }
    }

    static const char* ObjectTypeToString(::Core::Objects::ObjectType type) {
        switch (type) {
        case ::Core::Objects::ObjectType::GameObject: return "GameObject";
        case ::Core::Objects::ObjectType::AIHeroClient: return "AIHeroClient";
        case ::Core::Objects::ObjectType::AIMinionClient: return "AIMinionClient";
        case ::Core::Objects::ObjectType::AITurretClient: return "AITurretClient";
        case ::Core::Objects::ObjectType::MissileClient: return "MissileClient";
        case ::Core::Objects::ObjectType::BarracksDampenerClient: return "BarracksDampenerClient";
        case ::Core::Objects::ObjectType::HQClient: return "HQClient";
        case ::Core::Objects::ObjectType::ShopClient: return "ShopClient";
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
            auto it = std::find_if(dest.begin(), dest.end(), [&obj](const SDK::GameObject& item) {
                return item.IsValid() && item.Address() == obj.Address();
            });
            if (it == dest.end()) {
                dest.push_back(SDK::GameObject(obj.Handle()));
            }
        }
    }

    void AddUniqueObject(std::vector<SDK::GameObject>& dest, const SDK::GameObject& obj) const {
        if (!obj.IsValid()) return;
        auto it = std::find_if(dest.begin(), dest.end(), [&obj](const SDK::GameObject& item) {
            return item.IsValid() && item.Address() == obj.Address();
        });
        if (it == dest.end()) {
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
    static void OnBuffUpdateEvent(const SDK::Events::BuffEventArgs& args);
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

inline DeveloperToolsPlugin* s_instance = nullptr;

} // namespace Plugins

// Include Tab Implementations
#include "DeveloperTools/ObjectDetectorTab.h"
#include "DeveloperTools/EventLoggerTab.h"
#include "DeveloperTools/SpellItemInspectorTab.h"
#include "DeveloperTools/PlayerBuffInspectorTab.h"
#include "DeveloperTools/StatsInspectorTab.h"
#include "DeveloperTools/NavigationTab.h"

namespace Plugins {

inline void DeveloperToolsPlugin::OnLoad() {
    s_instance = this;
    enabled_ = true;
    maxRange_ = 400;
    trackedObjectTicks_.clear();

    // Create sub-tabs dynamically
    tabs_.clear();
    tabs_.push_back(std::make_unique<DevTools::ObjectDetectorTab>(this));
    tabs_.push_back(std::make_unique<DevTools::EventLoggerTab>(this));
    tabs_.push_back(std::make_unique<DevTools::SpellItemInspectorTab>(this));
    tabs_.push_back(std::make_unique<DevTools::PlayerBuffInspectorTab>(this));
    tabs_.push_back(std::make_unique<DevTools::StatsInspectorTab>(this));
    tabs_.push_back(std::make_unique<DevTools::NavigationTab>(this));

    for (auto& tab : tabs_) {
        tab->OnLoad();
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
    SDK::Events::AddOnBuffUpdate(&DeveloperToolsPlugin::OnBuffUpdateEvent);
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
    menuScanTurrets_ = filters->Add(new SDK::UI::MenuBool("ScanTurrets", "Turrets", scanTurrets_));
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
    menuLogBuffUpdate_ = loggerEvents->Add(new SDK::UI::MenuBool("EvBuffUpdate", "OnBuffUpdate", logBuffUpdate_));
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
    SDK::Events::RemoveOnBuffUpdate(&DeveloperToolsPlugin::OnBuffUpdateEvent);
    SDK::Events::RemoveOnNewPath(&DeveloperToolsPlugin::OnNewPathEvent);
    SDK::Events::RemoveOnDeleteObject(&DeveloperToolsPlugin::OnObjectDelete);

    for (auto& tab : tabs_) {
        tab->OnUnload();
    }
    tabs_.clear();

    DestroyNativeMenu();
    s_instance = nullptr;
}

inline void DeveloperToolsPlugin::OnUpdate() {
    SyncMenuSettings();
    if (!enabled_) {
        pKeyPressedLast_ = false;
        mKeyPressedLast_ = false;
        return;
    }

    // Update live validity of snapshots
    for (auto& snap : snapshots_) {
        bool found = false;
        for (const auto& obj : SDK::ObjectManager::Get<SDK::GameObject>()) {
            if (obj.IsValid() && static_cast<std::uint32_t>(obj.NetworkId()) == snap.networkId) {
                found = true;
                snap.isUnderlyingValid = true;
                break;
            }
        }
        if (!found) {
            snap.isUnderlyingValid = false;
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

    // Call sub-tabs update
    for (auto& tab : tabs_) {
        tab->OnUpdate();
    }

    if (trackedObjectTicks_.size() > 128) {
        const int now = SDK::Variables::TickCount();
        for (auto it = trackedObjectTicks_.begin(); it != trackedObjectTicks_.end(); ) {
            if (now - it->second > 15000) {
                it = trackedObjectTicks_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Delegate copy hotkey to active tab
    bool isDown = (GetAsyncKeyState('P') & 0x8000) != 0;
    if (isDown && !pKeyPressedLast_) {
        if (activeTabIdx_ >= 0 && activeTabIdx_ < static_cast<int>(tabs_.size())) {
            tabs_[activeTabIdx_]->OnCopyHotkey();
        }
    }
    pKeyPressedLast_ = isDown;
}

inline void DeveloperToolsPlugin::OnRender() {
    if (!enabled_) {
        return;
    }
    for (auto& tab : tabs_) {
        tab->OnRender();
    }
}

inline void DeveloperToolsPlugin::OnMenu() {
    if (ImGui::Checkbox("Enable Developer Tools", &enabled_)) {
        if (menuEnabled_) menuEnabled_->SetValue(enabled_);
    }
    if (!enabled_) {
        return;
    }
    if (ImGui::SliderInt("Max object dist from cursor", &maxRange_, 100, 1500)) {
        if (menuMaxRange_) menuMaxRange_->SetValue(maxRange_);
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("DeveloperToolsTabBar")) {
        for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
            if (ImGui::BeginTabItem(tabs_[i]->GetTabName())) {
                activeTabIdx_ = i;
                tabs_[i]->OnDrawTab();
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
    for (auto netId : openEventLogWindows_) {
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
            static char searchFilter[64] = {};
            char searchId[128];
            std::snprintf(searchId, sizeof(searchId), "Search Logs##Search%u", netId);
            ImGui::InputText(searchId, searchFilter, sizeof(searchFilter));

            ImGui::SameLine();
            char clearId[128];
            std::snprintf(clearId, sizeof(clearId), "Clear##Clear%u", netId);
            if (ImGui::Button(clearId)) {
                objectEventLogs_[netId].clear();
            }

            ImGui::SameLine();
            char copyId[128];
            std::snprintf(copyId, sizeof(copyId), "Copy All##Copy%u", netId);
            if (ImGui::Button(copyId)) {
                std::string dump = "=== EVENT LOGS FOR " + charName + " (NetId: " + std::to_string(netId) + ") ===\n";
                for (const auto& entry : objectEventLogs_[netId]) {
                    char line[512];
                    std::snprintf(line, sizeof(line), "[%.2fs] [%s] %s\n", entry.time, entry.eventName.c_str(), entry.details.c_str());
                    dump += line;
                }
                ImGui::SetClipboardText(dump.c_str());
                NightSharpDebug::Logf("[Dev] Copied logs to Clipboard.");
            }

            if (ImGui::BeginChild("EventLogScroll", ImVec2(0, 300), true)) {
                auto& logs = objectEventLogs_[netId];
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
        openEventLogWindows_.erase(netId);
        objectEventLogs_.erase(netId);
    }
}

inline void DeveloperToolsPlugin::OnProcessSpellCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->openEventLogWindows_.find(netId) != s_instance->openEventLogWindows_.end()) {
            char details[256];
            std::snprintf(details, sizeof(details), "Spell: %s | TargetNetId: %u | Speed: %.1f | EndPos: (%.1f, %.1f, %.1f)", 
                          args.SpellName ? args.SpellName : "Unknown", args.TargetNetworkId, args.MissileSpeed, args.EndPosition.x, args.EndPosition.y, args.EndPosition.z);
            s_instance->LogEventForObject(netId, "ProcessSpellCast", details);
        }
        for (auto& tab : s_instance->tabs_) {
            tab->OnProcessSpellCast(args);
        }
    }
}

inline void DeveloperToolsPlugin::OnDoCastEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->openEventLogWindows_.find(netId) != s_instance->openEventLogWindows_.end()) {
            char details[256];
            std::snprintf(details, sizeof(details), "Spell: %s | TargetNetId: %u | Speed: %.1f | EndPos: (%.1f, %.1f, %.1f)", 
                          args.SpellName ? args.SpellName : "Unknown", args.TargetNetworkId, args.MissileSpeed, args.EndPosition.x, args.EndPosition.y, args.EndPosition.z);
            s_instance->LogEventForObject(netId, "DoCast", details);
        }
        for (auto& tab : s_instance->tabs_) {
            tab->OnDoCastEvent(args);
        }
    }
}

inline void DeveloperToolsPlugin::OnFinishCastEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->openEventLogWindows_.find(netId) != s_instance->openEventLogWindows_.end()) {
            char details[256];
            std::snprintf(details, sizeof(details), "Spell: %s | TargetNetId: %u", 
                          args.SpellName ? args.SpellName : "Unknown", args.TargetNetworkId);
            s_instance->LogEventForObject(netId, "FinishCast", details);
        }
        for (auto& tab : s_instance->tabs_) {
            tab->OnFinishCastEvent(args);
        }
    }
}

inline void DeveloperToolsPlugin::OnSpellImpactEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->openEventLogWindows_.find(netId) != s_instance->openEventLogWindows_.end()) {
            char details[256];
            std::snprintf(details, sizeof(details), "Spell: %s | TargetNetId: %u", 
                          args.SpellName ? args.SpellName : "Unknown", args.TargetNetworkId);
            s_instance->LogEventForObject(netId, "SpellImpact", details);
        }
        for (auto& tab : s_instance->tabs_) {
            tab->OnSpellImpactEvent(args);
        }
    }
}

inline void DeveloperToolsPlugin::OnCastSpellEvent(const SDK::Events::CastSpellEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->openEventLogWindows_.find(netId) != s_instance->openEventLogWindows_.end()) {
            char details[256];
            std::snprintf(details, sizeof(details), "Slot: %d | TargetNetId: %u", 
                          args.Slot, args.TargetNetworkId);
            s_instance->LogEventForObject(netId, "CastSpell", details);
        }
        for (auto& tab : s_instance->tabs_) {
            tab->OnCastSpellEvent(args);
        }
    }
}

inline void DeveloperToolsPlugin::OnStopCastEvent(const SDK::Events::StopCastEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->openEventLogWindows_.find(netId) != s_instance->openEventLogWindows_.end()) {
            char details[256];
            std::snprintf(details, sizeof(details), "Slot: %d | DestroyMissile: %d | KeepAnim: %d", 
                          args.Slot, args.DestroyMissile ? 1 : 0, args.KeepAnimationPlaying ? 1 : 0);
            s_instance->LogEventForObject(netId, "StopCast", details);
        }
        for (auto& tab : s_instance->tabs_) {
            tab->OnStopCastEvent(args);
        }
    }
}

inline void DeveloperToolsPlugin::OnPlayAnimationEvent(const SDK::Events::PlayAnimationEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->openEventLogWindows_.find(netId) != s_instance->openEventLogWindows_.end()) {
            char details[256];
            std::snprintf(details, sizeof(details), "Animation: %s | Id: %d", 
                          args.Animation ? args.Animation : "Unknown", args.AnimationId);
            s_instance->LogEventForObject(netId, "PlayAnimation", details);
        }
        for (auto& tab : s_instance->tabs_) {
            tab->OnPlayAnimationEvent(args);
        }
    }
}

inline void DeveloperToolsPlugin::OnBuffAddEvent(const SDK::Events::BuffEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->openEventLogWindows_.find(netId) != s_instance->openEventLogWindows_.end()) {
            char details[256];
            std::snprintf(details, sizeof(details), "Buff: %s | Count: %d", 
                          args.BuffName ? args.BuffName : "Unknown", args.Count);
            s_instance->LogEventForObject(netId, "BuffAdd", details);
        }
        for (auto& tab : s_instance->tabs_) {
            tab->OnBuffAddEvent(args);
        }
    }
}

inline void DeveloperToolsPlugin::OnBuffRemoveEvent(const SDK::Events::BuffEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->openEventLogWindows_.find(netId) != s_instance->openEventLogWindows_.end()) {
            char details[256];
            std::snprintf(details, sizeof(details), "Buff: %s | Count: %d", 
                          args.BuffName ? args.BuffName : "Unknown", args.Count);
            s_instance->LogEventForObject(netId, "BuffRemove", details);
        }
        for (auto& tab : s_instance->tabs_) {
            tab->OnBuffRemoveEvent(args);
        }
    }
}

inline void DeveloperToolsPlugin::OnBuffUpdateEvent(const SDK::Events::BuffEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->openEventLogWindows_.find(netId) != s_instance->openEventLogWindows_.end()) {
            char details[256];
            std::snprintf(details, sizeof(details), "Buff: %s | Count: %d", 
                          args.BuffName ? args.BuffName : "Unknown", args.Count);
            s_instance->LogEventForObject(netId, "BuffUpdate", details);
        }
        for (auto& tab : s_instance->tabs_) {
            tab->OnBuffUpdateEvent(args);
        }
    }
}

inline void DeveloperToolsPlugin::OnNewPathEvent(const SDK::Events::NewPathEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        const std::uint32_t netId = static_cast<std::uint32_t>(args.Sender.NetworkId);
        if (s_instance->openEventLogWindows_.find(netId) != s_instance->openEventLogWindows_.end()) {
            char details[256];
            std::snprintf(details, sizeof(details), "Dash: %d | Speed: %.1f | Waypoints: %d", 
                          args.IsDash ? 1 : 0, args.Speed, args.PathCount);
            s_instance->LogEventForObject(netId, "NewPath", details);
        }
        for (auto& tab : s_instance->tabs_) {
            tab->OnNewPathEvent(args);
        }
    }
}

inline void DeveloperToolsPlugin::OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (s_instance && s_instance->enabled_) {
        for (auto& tab : s_instance->tabs_) {
            tab->OnDeleteObject(args);
        }
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
    if (menuScanTurrets_) scanTurrets_ = menuScanTurrets_->Value;
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
    if (menuLogBuffUpdate_) logBuffUpdate_ = menuLogBuffUpdate_->Value;
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
        menuLogBuffUpdate_ = nullptr;
        menuLogNewPath_ = nullptr;
    }
}

} // namespace Plugins
