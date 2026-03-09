#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Spells/Spell.h"
#include "sdk/EzEvade/Spells/SpellDatabase.h"
#include "sdk/EzEvade/Spells/SpellRuntime.h"
#include "sdk/EzEvade/Helpers/ObjectCache.h"
#include "sdk/EzEvade/Helpers/PositionInfo.h"
#include "sdk/EzEvade/Helpers/Position.h"
#include "sdk/EzEvade/Helpers/EvadeRuntimeState.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellRegistry.h"
#include "sdk/EzEvade/Utils/DelayAction.h"
#include "sdk/EzEvade/Utils/EvadeUtils.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace EzEvade {

struct SpecialSpellEventArgs {
    bool NoProcess = false;
    std::shared_ptr<SpellData> Data = nullptr;
};

class SpellDetector {
public:
    using OnProcessDetectedSpellsHandler = std::function<void()>;
    using OnProcessSpecialSpellHandler = std::function<void(const SDK::SpellCastArgs&, std::shared_ptr<SpellData>, SpecialSpellEventArgs&)>;

    static inline std::vector<OnProcessDetectedSpellsHandler> OnProcessDetectedSpells = {};
    static inline std::vector<OnProcessSpecialSpellHandler> OnProcessSpecialSpell = {};

    static inline std::unordered_map<int, Spell> Spells = {};
    static inline std::unordered_map<int, Spell> DrawSpells = {};
    static inline std::unordered_map<int, Spell> DetectedSpells = {};

    static inline std::unordered_map<std::string, std::string> ChanneledSpells = {};
    static inline std::unordered_map<std::string, std::shared_ptr<SpellData>> OnProcessTraps = {};
    static inline std::unordered_map<std::string, std::shared_ptr<SpellData>> OnProcessSpells = {};
    static inline std::unordered_map<std::string, std::shared_ptr<SpellData>> OnMissileSpells = {};
    static inline std::unordered_set<int> ProcessedMissileIds = {};

    static inline std::unordered_map<std::string, std::shared_ptr<SpellData>> WindupSpells = {};

    static inline int SpellIDCount = 0;
    static inline float LastCheckTime = 0.0f;
    static inline float LastCheckSpellCollisionTime = 0.0f;
    static inline float LastDictionaryRefreshTime = 0.0f;
    static inline int LastDictionaryEnemyCount = 0;

    static inline std::shared_ptr<SDK::MenuUI::Menu> MenuRoot = nullptr;
    static inline std::shared_ptr<SDK::MenuUI::Menu> SpellMenu = nullptr;
    static inline std::shared_ptr<SDK::MenuUI::Menu> TrapMenu = nullptr;

public:
    explicit SpellDetector(const std::shared_ptr<SDK::MenuUI::Menu>& mainMenu) {
        MenuRoot = mainMenu;
        if (MenuRoot) {
            SpellMenu = MenuRoot->AddSubMenu("Spells", "Spells");
            TrapMenu = MenuRoot->AddSubMenu("Traps", "Traps");
        }

        LoadSpellDictionary();
        InitChannelSpells();
        RegisterEvents();

        SpellRuntime::GetActiveSpells = []() {
            std::vector<const Spell*> out;
            out.reserve(Spells.size());
            for (const auto& [id, spell] : Spells) {
                (void)id;
                out.push_back(&spell);
            }
            return out;
        };

        PositionInfo::ActiveSpellsProvider = []() {
            std::vector<ActiveSpellSnapshot> snaps;
            snaps.reserve(Spells.size());
            for (const auto& [id, spell] : Spells) {
                snaps.push_back({ id, spell.Dangerlevel });
            }
            return snaps;
        };
    }

    static void RegisterOnProcessDetectedSpells(OnProcessDetectedSpellsHandler cb) {
        OnProcessDetectedSpells.push_back(std::move(cb));
    }

    static void RegisterOnProcessSpecialSpell(OnProcessSpecialSpellHandler cb) {
        OnProcessSpecialSpell.push_back(std::move(cb));
    }

    static void RemoveNonDangerousSpells() {
        std::vector<int> toDelete;
        for (const auto& [id, spell] : Spells) {
            if (SpellExtensions::GetSpellDangerLevel(spell) < 3) {
                toDelete.push_back(id);
            }
        }
        for (int id : toDelete) {
            DelayAction::Add(1, [id]() { DeleteSpell(id); });
        }
    }

    static void UpdateSpells() {
        for (auto& [id, spell] : DetectedSpells) {
            (void)id;
            SpellExtensions::UpdateSpellInfo(spell);
        }
    }

    static void CheckSpellEndTime() {
        std::vector<int> toDelete;
        for (const auto& [id, spell] : DetectedSpells) {
            if (!spell.Info) continue;
            if (spell.Info->name.find("_trap") != std::string::npos) continue;

            if (spell.EndTime + spell.Info->extraEndTime < EvadeUtils::TickCount()) {
                toDelete.push_back(id);
            }
        }

        for (int id : toDelete) {
            DelayAction::Add(1, [id]() { DeleteSpell(id); });
        }
    }

    static void AddDetectedSpells() {
        bool spellAdded = false;
        const auto& myHero = SDK::GameObjects::Player;
        if (!myHero.IsValid()) return;

        for (auto& [id, spell] : DetectedSpells) {
            if (!spell.Info) continue;

            spell.SpellHitTime = SpellExtensions::GetSpellHitTime(spell, ObjectCache::MyHeroCache.ServerPos2D);
            spell.EvadeTime = 0.0f;

            if (DrawSpells.find(spell.SpellID) == DrawSpells.end()) {
                DrawSpells[spell.SpellID] = spell;
            }

            const int dodgeInterval = ObjectCache::Menu.GetSlider("DodgeInterval", 0);
            (void)dodgeInterval;

            const std::string dodgeKey = spell.Info->spellName + "DodgeSpell";
            const bool dodgeEnabled = ObjectCache::Menu.GetBool(dodgeKey, true);
            if (!dodgeEnabled) continue;

            if (spell.Type == SpellType::Circular && !ObjectCache::Menu.GetBool("DodgeCircularSpells", true)) {
                continue;
            }

            const std::string hpKey = spell.Info->spellName + "DodgeIgnoreHP";
            const int hpThreshold = ObjectCache::Menu.GetSlider(hpKey, spell.Info->dangerlevel == 1 ? 90 : 100);
            if (myHero.GetHealthPercent() > (float)hpThreshold) {
                continue;
            }

            if (Spells.find(spell.SpellID) == Spells.end()) {
                Spells[spell.SpellID] = spell;
                spellAdded = true;
            }
        }

        if (spellAdded) {
            for (auto& cb : OnProcessDetectedSpells) {
                try { cb(); } catch (...) {}
            }
        }
    }

    static int CreateSpell(Spell newSpell, bool processSpell = true) {
        const int spellID = SpellIDCount++;
        newSpell.SpellID = spellID;
        SpellExtensions::UpdateSpellInfo(newSpell);
        DetectedSpells[spellID] = newSpell;

        if (processSpell) {
            AddDetectedSpells();
        }
        return spellID;
    }

    static void DeleteSpell(int spellID) {
        Spells.erase(spellID);
        DrawSpells.erase(spellID);
        DetectedSpells.erase(spellID);
    }

    static int GetCurrentSpellID() {
        return SpellIDCount;
    }

    static std::vector<int> GetSpellList() {
        std::vector<int> spellList;
        spellList.reserve(Spells.size());
        for (const auto& [id, spell] : Spells) {
            (void)spell;
            spellList.push_back(id);
        }
        return spellList;
    }

    static int GetHighestDetectedSpellID() {
        int highest = 0;
        for (const auto& [id, spell] : Spells) {
            (void)spell;
            highest = std::max(highest, id);
        }
        return highest;
    }

    static float GetLowestEvadeTime(const Spell*& lowestSpell) {
        float lowest = FLT_MAX;
        lowestSpell = nullptr;

        for (auto& [id, spell] : Spells) {
            (void)id;
            if (spell.SpellHitTime != -FLT_MAX) {
                const float v = (spell.SpellHitTime - spell.EvadeTime);
                if (v < lowest) {
                    lowest = v;
                    lowestSpell = &spell;
                }
            }
        }

        return lowest;
    }

    static const Spell* GetMostDangerousSpell(bool hasProjectile = false) {
        int maxDanger = 0;
        const Spell* maxDangerSpell = nullptr;

        for (const auto& [id, spell] : Spells) {
            (void)id;
            if (!spell.Info) continue;

            if (!hasProjectile || (spell.Info->projectileSpeed > 0.0f && spell.Info->projectileSpeed < FLT_MAX)) {
                const int d = spell.Dangerlevel;
                if (d > maxDanger) {
                    maxDanger = d;
                    maxDangerSpell = &spell;
                }
            }
        }

        return maxDangerSpell;
    }

    static void InitChannelSpells() {
        ChanneledSpells["drain"] = "FiddleSticks";
        ChanneledSpells["crowstorm"] = "FiddleSticks";
        ChanneledSpells["katarinar"] = "Katarina";
        ChanneledSpells["absolutezero"] = "Nunu";
        ChanneledSpells["galioidolofdurand"] = "Galio";
        ChanneledSpells["missfortunebullettime"] = "MissFortune";
        ChanneledSpells["meditate"] = "MasterYi";
        ChanneledSpells["malzaharr"] = "Malzahar";
        ChanneledSpells["reapthewhirlwind"] = "Janna";
        ChanneledSpells["karthusfallenone"] = "Karthus";
        ChanneledSpells["karthusfallenone2"] = "Karthus";
        ChanneledSpells["velkozr"] = "Velkoz";
        ChanneledSpells["xerathlocusofpower2"] = "Xerath";
        ChanneledSpells["zace"] = "Zac";
        ChanneledSpells["pantheon_heartseeker"] = "Pantheon";
        ChanneledSpells["jhinr"] = "Jhin";
        ChanneledSpells["odinrecall"] = "AllChampions";
        ChanneledSpells["recall"] = "AllChampions";
    }

    static void LoadDummySpell(const std::shared_ptr<SpellData>& spell) {
        if (!spell || !SpellMenu) return;
        CreateSpellMenu(*SpellMenu, *spell, false);
    }

    static void CreateSpellData(const SDK::GameObject& hero,
                                const Vec3& spellStartPos,
                                const Vec3& spellEndPos,
                                const std::shared_ptr<SpellData>& spellData,
                                const SDK::GameObject& obj = SDK::GameObject(),
                                float extraEndTick = 0.0f,
                                bool processSpell = true,
                                SpellType spellType = SpellType::None,
                                bool checkEndExplosion = true,
                                float spellRadius = 0.0f) {
        if (!spellData) return;

        if (checkEndExplosion && spellData->hasEndExplosion) {
            CreateSpellData(hero, spellStartPos, spellEndPos, spellData, obj, extraEndTick, false, spellData->spellType, false);
            CreateSpellData(hero, spellStartPos, spellEndPos, spellData, obj, extraEndTick, true, SpellType::Circular, false);
            return;
        }

        const auto& myHero = SDK::GameObjects::Player;
        if (!myHero.IsValid()) return;
        if (spellStartPos.Distance2D(myHero.GetPosition()) >= spellData->range + 1000.0f) return;

        Vec2 startPosition = spellStartPos.To2D();
        Vec2 endPosition = spellEndPos.To2D();
        Vec2 direction = (endPosition - startPosition).Normalized();
        float endTick = 0.0f;

        if (spellType == SpellType::None) {
            spellType = spellData->spellType;
        }

        if (spellData->fixedRange && endPosition.Distance(startPosition) > spellData->range) {
            endPosition = startPosition + direction * spellData->range;
        }

        if (spellType == SpellType::Line) {
            endTick = spellData->spellDelay + (spellData->range / spellData->projectileSpeed) * 1000.0f;
            endPosition = startPosition + direction * spellData->range;
            if (spellData->useEndPosition) {
                const float range = endPosition.Distance(startPosition);
                endTick = spellData->spellDelay + (range / spellData->projectileSpeed) * 1000.0f;
            }
            if (obj.IsValid()) endTick -= spellData->spellDelay;
        } else if (spellType == SpellType::Circular) {
            endTick = spellData->spellDelay;
            if (endPosition.Distance(startPosition) > spellData->range) {
                endPosition = startPosition + direction * spellData->range;
            }

            if (spellData->projectileSpeed == 0.0f && hero.IsValid()) {
                endPosition = hero.GetServerPosition().To2D();
            } else if (spellData->projectileSpeed > 0.0f) {
                endTick += 1000.0f * startPosition.Distance(endPosition) / spellData->projectileSpeed;
                if (spellData->spellType == SpellType::Line && spellData->hasEndExplosion && !spellData->useEndPosition) {
                    endPosition = startPosition + direction * spellData->range;
                }
            }
        } else if (spellType == SpellType::Arc) {
            endTick = 1000.0f * startPosition.Distance(endPosition) / spellData->projectileSpeed;
            if (obj.IsValid()) endTick -= spellData->spellDelay;
        } else if (spellType == SpellType::Cone) {
            endPosition = startPosition + direction * spellData->range;
            endTick = spellData->spellDelay;
            if (spellData->projectileSpeed == 0.0f && hero.IsValid()) {
                endPosition = hero.GetServerPosition().To2D();
            } else if (spellData->projectileSpeed > 0.0f) {
                endTick += 1000.0f * startPosition.Distance(endPosition) / spellData->projectileSpeed;
            }
        } else {
            return;
        }

        if (spellData->invert) {
            const Vec2 dir = (startPosition - endPosition).Normalized();
            endPosition = startPosition + dir * startPosition.Distance(endPosition);
        }

        if (spellData->isPerpendicular) {
            startPosition = spellEndPos.To2D() - direction.Perpendicular() * spellData->secondaryRadius;
            endPosition = spellEndPos.To2D() + direction.Perpendicular() * spellData->secondaryRadius;
        }

        endTick += extraEndTick;

        Spell newSpell;
        newSpell.StartTime = EvadeUtils::TickCount();
        newSpell.EndTime = EvadeUtils::TickCount() + endTick;
        newSpell.StartPos = startPosition;
        newSpell.EndPos = endPosition;
        newSpell.Height = spellEndPos.y + spellData->extraDrawHeight;
        newSpell.Direction = direction;
        newSpell.Info = spellData;
        newSpell.Type = spellType;
        newSpell.Radius = (spellRadius > 0.0f) ? spellRadius : SpellExtensions::GetSpellRadius(newSpell);

        if (spellType == SpellType::Cone) {
            newSpell.Radius = 100.0f + (newSpell.Radius * 3.0f);
            newSpell.CnStart = startPosition + direction;
            newSpell.CnLeft = endPosition + direction.Perpendicular() * newSpell.Radius;
            newSpell.CnRight = endPosition - direction.Perpendicular() * newSpell.Radius;
        }

        if (hero.IsValid()) {
            newSpell.HeroID = hero.GetNetId();
        }

        if (obj.IsValid()) {
            newSpell.SpellObject = obj;
            newSpell.ProjectileID = obj.GetNetId();
        }

        const int spellID = CreateSpell(newSpell, processSpell);
        if (extraEndTick != 1337.0f) {
            DelayAction::Add((int)(endTick + spellData->extraEndTime), [spellID]() { DeleteSpell(spellID); });
        }
    }

private:
    static SDK::GameObject FindHeroByNetId(int netId) {
        if (netId <= 0) {
            return SDK::GameObject();
        }

        for (const auto& hero : SDK::GameObjects::AllHeroes) {
            if (!hero.IsValid()) {
                continue;
            }
            if (hero.GetNetId() == netId) {
                return hero;
            }
        }

        return SDK::GameObject();
    }

    static std::string ToLowerCopy(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        return value;
    }

    static SpellSlotId ToEvadeSlot(SDK::SpellSlotId slot) {
        switch (slot) {
        case SDK::SpellSlotId::Q: return SpellSlotId::Q;
        case SDK::SpellSlotId::W: return SpellSlotId::W;
        case SDK::SpellSlotId::E: return SpellSlotId::E;
        case SDK::SpellSlotId::R: return SpellSlotId::R;
        case SDK::SpellSlotId::Summoner1: return SpellSlotId::F;
        case SDK::SpellSlotId::Summoner2: return SpellSlotId::T;
        default: return SpellSlotId::None;
        }
    }

    static char SlotToAliasChar(SDK::SpellSlotId slot) {
        switch (slot) {
        case SDK::SpellSlotId::Q: return 'q';
        case SDK::SpellSlotId::W: return 'w';
        case SDK::SpellSlotId::E: return 'e';
        case SDK::SpellSlotId::R: return 'r';
        case SDK::SpellSlotId::Summoner1: return 'd';
        case SDK::SpellSlotId::Summoner2: return 'f';
        default: return '\0';
        }
    }

    static bool HasChampionSpecificSpellEntries() {
        for (const auto& kv : OnProcessSpells) {
            const auto& data = kv.second;
            if (!data) {
                continue;
            }
            if (!data->charName.empty() && _stricmp(data->charName.c_str(), "AllChampions") != 0) {
                return true;
            }
        }
        return false;
    }

    static void RegisterRuntimeSpellData(const std::shared_ptr<SpellData>& spellData) {
        if (!spellData || spellData->spellName.empty()) {
            return;
        }

        const std::string keySpell = ToLowerCopy(spellData->spellName);
        const std::string keyMissile = ToLowerCopy(spellData->missileName.empty() ? spellData->spellName : spellData->missileName);
        if (keySpell.empty()) {
            return;
        }

        bool inserted = false;
        if (OnProcessSpells.find(keySpell) == OnProcessSpells.end()) {
            OnProcessSpells[keySpell] = spellData;
            inserted = true;
        }
        if (OnMissileSpells.find(keyMissile) == OnMissileSpells.end()) {
            OnMissileSpells[keyMissile] = spellData;
        }

        for (const auto& n : spellData->extraSpellNames) {
            const std::string k = ToLowerCopy(n);
            if (!k.empty() && OnProcessSpells.find(k) == OnProcessSpells.end()) {
                OnProcessSpells[k] = spellData;
            }
        }
        for (const auto& n : spellData->extraMissileNames) {
            const std::string k = ToLowerCopy(n);
            if (!k.empty() && OnMissileSpells.find(k) == OnMissileSpells.end()) {
                OnMissileSpells[k] = spellData;
            }
        }

        if (inserted) {
            SpecialSpells::LoadSpecialSpell(*spellData);
            if (SpellMenu) {
                CreateSpellMenu(*SpellMenu, *spellData, false);
            }
        }
    }

    static void LoadRuntimeSpellbookFallback(bool devMode) {
        const auto& myHero = SDK::GameObjects::Player;
        if (!myHero.IsValid()) {
            return;
        }

        constexpr SDK::SpellSlotId kSlots[] = {
            SDK::SpellSlotId::Q, SDK::SpellSlotId::W, SDK::SpellSlotId::E, SDK::SpellSlotId::R
        };

        for (const auto& hero : SDK::GameObjects::AllHeroes) {
            if (!hero.IsValid() || !hero.IsAlive()) {
                continue;
            }
            if (hero.GetTeam() == myHero.GetTeam() && !devMode) {
                continue;
            }

            for (SDK::SpellSlotId slotId : kSlots) {
                const SDK::SpellSlot slot = hero.GetSpell(slotId);
                if (!slot.IsValid()) {
                    continue;
                }

                std::string spellName = slot.GetName();
                if (spellName.empty()) {
                    continue;
                }

                const std::string lowerSpell = ToLowerCopy(spellName);
                if (lowerSpell.find("basicattack") != std::string::npos ||
                    lowerSpell.find("critattack") != std::string::npos) {
                    continue;
                }

                auto data = std::make_shared<SpellData>();
                data->charName = hero.GetChampionName().empty() ? "AllChampions" : hero.GetChampionName();
                data->name = spellName;
                data->spellName = spellName;
                data->missileName = spellName;
                data->spellKey = ToEvadeSlot(slotId);
                data->spellType = SpellType::Line;
                data->spellDelay = 250.0f;
                data->dangerlevel = 2;
                data->detectionType = DetectionType::CastSpell;
                data->collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };

                const SDK::SpellData sdkData = slot.GetSpellInfo().GetSpellData();
                const int rank = (std::max)(0, slot.GetLevel() - 1);

                float castRange = sdkData.GetCastRange(rank);
                if (!std::isfinite(castRange) || castRange < 150.0f) {
                    castRange = 1200.0f;
                }
                data->range = (std::clamp)(castRange, 150.0f, 5000.0f);

                float lineWidth = sdkData.GetLineWidth();
                if (!std::isfinite(lineWidth) || lineWidth < 10.0f) {
                    lineWidth = 70.0f;
                }
                data->radius = (std::clamp)(lineWidth, 20.0f, 250.0f);

                float missileSpeed = sdkData.GetMissileSpeed();
                if (!std::isfinite(missileSpeed) || missileSpeed < 50.0f || missileSpeed > 50000.0f) {
                    missileSpeed = 1700.0f;
                }
                data->projectileSpeed = missileSpeed;

                const std::string scriptName = sdkData.GetScriptName();
                if (!scriptName.empty() && _stricmp(scriptName.c_str(), spellName.c_str()) != 0) {
                    data->extraSpellNames.push_back(scriptName);
                    data->extraMissileNames.push_back(scriptName);
                }

                const std::string champLower = ToLowerCopy(data->charName);
                const char aliasChar = SlotToAliasChar(slotId);
                if (!champLower.empty() && aliasChar != '\0') {
                    std::string alias = champLower;
                    alias.push_back(aliasChar);
                    data->extraSpellNames.push_back(alias);
                    data->extraMissileNames.push_back(alias);
                    data->extraMissileNames.push_back(alias + "missile");
                }

                RegisterRuntimeSpellData(data);
            }
        }
    }

    static std::shared_ptr<SpellData> CreateRuntimeMissileFallback(const SDK::Missile& missile,
                                                                   const std::string& eventSpellName) {
        if (!missile.IsValid() || missile.IsAutoAttack() || missile.IsTurretShot()) {
            return nullptr;
        }

        const auto& myHero = SDK::GameObjects::Player;
        if (!myHero.IsValid()) {
            return nullptr;
        }

        const SDK::GameObject caster = FindHeroByNetId(missile.GetCasterNetId());
        if (!caster.IsValid() || caster.GetTeam() == myHero.GetTeam()) {
            return nullptr;
        }

        std::string spellName = eventSpellName;
        if (spellName.empty()) {
            spellName = missile.GetSpellName();
        }
        std::string missileName = missile.GetMissileName();
        if (spellName.empty()) {
            spellName = missileName;
        }
        if (spellName.empty()) {
            spellName = SDK::GameObject(missile.address).GetName();
        }
        if (spellName.empty()) {
            return nullptr;
        }

        auto data = std::make_shared<SpellData>();
        data->charName = !caster.GetChampionName().empty() ? caster.GetChampionName() : "AllChampions";
        data->name = spellName;
        data->spellName = spellName;
        data->missileName = missileName.empty() ? spellName : missileName;
        data->spellType = SpellType::Line;
        data->spellDelay = 0.0f;
        data->detectionType = DetectionType::Missile;
        data->dangerlevel = 2;
        data->collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };

        Vec3 start = missile.GetStartPos();
        Vec3 end = missile.GetEndPos();
        if (!start.IsValid() || start.IsZero()) {
            start = missile.GetPosition();
        }
        if (!end.IsValid() || end.IsZero()) {
            end = missile.GetCastEndPos();
        }
        float range = (!start.IsZero() && !end.IsZero()) ? start.Distance2D(end) : 0.0f;
        if (!std::isfinite(range) || range < 150.0f || range > 6000.0f) {
            range = 1300.0f;
        }
        data->range = range;

        const SDK::SpellData sdkData(missile.GetSpellData());
        float width = sdkData.GetLineWidth();
        if (!std::isfinite(width) || width < 10.0f || width > 500.0f) {
            width = 70.0f;
        }
        data->radius = width;

        float speed = sdkData.GetMissileSpeed();
        if (!std::isfinite(speed) || speed < 50.0f || speed > 50000.0f) {
            speed = 1700.0f;
        }
        data->projectileSpeed = speed;

        RegisterRuntimeSpellData(data);
        return data;
    }

    static void RegisterEvents() {
        SDK::EventSystem::OnProcessSpellCast([](const SDK::SpellCastArgs& args) {
            OnProcessSpell(args);
        });
        SDK::EventSystem::OnMissileCreated([](const SDK::MissileArgs& args) {
            OnMissileCreate(args);
        });
        SDK::EventSystem::OnMissileDeleted([](const SDK::MissileArgs& args) {
            OnMissileDelete(args);
        });
        SDK::EventSystem::OnGameUpdate([](float gameTime) {
            (void)gameTime;
            OnGameUpdate();
        });
    }

    static void OnGameUpdate() {
        if (!EvadeRuntimeState::Enabled) {
            return;
        }

        EnsureSpellDictionaryLoaded();
        ProcessActiveMissilesFallback();
        UpdateSpells();
        DelayAction::Update();

        if (EvadeUtils::TickCount() - LastCheckTime > 1.0f) {
            CheckSpellEndTime();
            AddDetectedSpells();
            LastCheckTime = EvadeUtils::TickCount();
        }
    }

    static void OnMissileCreate(const SDK::MissileArgs& args) {
        if (!EvadeRuntimeState::Enabled) {
            return;
        }

        const auto& missile = args.MissileObj;
        if (!missile.IsValid()) return;
        ProcessMissileAsSpell(missile, args.CasterNetId, args.SpellName);
    }

    static void OnMissileDelete(const SDK::MissileArgs& args) {
        if (!EvadeRuntimeState::Enabled) {
            return;
        }

        const auto& missile = args.MissileObj;
        if (!missile.IsValid()) return;

        std::vector<int> toDelete;
        for (const auto& [id, spell] : Spells) {
            if (!spell.Info) continue;
            if (spell.SpellObject.IsValid() && spell.SpellObject.GetNetId() == missile.GetNetworkId()) {
                if (spell.Info->name.find("_trap") == std::string::npos) {
                    toDelete.push_back(id);
                }
            }
        }
        for (int id : toDelete) {
            DelayAction::Add(1, [id]() { DeleteSpell(id); });
        }

        const int missileId = GetStableMissileId(missile);
        if (missileId > 0) {
            ProcessedMissileIds.erase(missileId);
        }
    }

    static void OnProcessSpell(const SDK::SpellCastArgs& args) {
        if (!EvadeRuntimeState::Enabled) {
            return;
        }

        if (!args.Sender.IsValid()) return;
        if (!Situation::CheckTeam(args.Sender)) return;

        std::string name = args.SpellName;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        auto it = OnProcessSpells.find(name);
        if (it == OnProcessSpells.end()) return;
        auto spellData = it->second;
        if (!spellData || spellData->usePackets) return;

        SpecialSpellEventArgs specialArgs;
        specialArgs.Data = spellData;

        for (auto& cb : OnProcessSpecialSpell) {
            try { cb(args, spellData, specialArgs); } catch (...) {}
        }

        spellData = specialArgs.Data ? specialArgs.Data : spellData;
        if (!specialArgs.NoProcess && !spellData->noProcess) {
            CreateSpellData(args.Sender, args.StartPos, args.EndPos, spellData);
        }
    }

    static void CreateSpellMenu(SDK::MenuUI::Menu& parent, const SpellData& spell, bool trap) {
        const std::string nodeName = spell.charName + spell.spellName + (trap ? "TrapSettings" : "Settings");
        auto newSpellMenu = parent.AddSubMenu(nodeName, spell.charName + " (" + std::to_string((int)spell.spellKey) + ") Settings");

        const bool enableSpell = !spell.defaultOff;
        const std::string prefix = trap ? (spell.spellName + "_trap") : spell.spellName;

        newSpellMenu->Add<SDK::MenuUI::MenuBool>(prefix + "DrawSpell", trap ? "Draw Trap" : "Draw Spell", true);
        newSpellMenu->Add<SDK::MenuUI::MenuBool>(prefix + "DodgeSpell", trap ? "Dodge Trap [Beta]" : "Dodge Spell", enableSpell);
        newSpellMenu->Add<SDK::MenuUI::MenuSlider>(prefix + "SpellRadius", trap ? "Trap Radius" : "Spell Radius",
                                                    (int)spell.radius, (int)spell.radius - 100, (int)spell.radius + 100);
        newSpellMenu->Add<SDK::MenuUI::MenuBool>(prefix + "FastEvade", "Force Fast Evade", spell.dangerlevel == 4);
        newSpellMenu->Add<SDK::MenuUI::MenuSlider>(prefix + "DodgeIgnoreHP", "Dodge Only Below HP % <=", spell.dangerlevel == 1 ? 90 : 100, 0, 100);
        newSpellMenu->Add<SDK::MenuUI::MenuList>(prefix + "DangerLevel", "Danger Level",
            std::vector<std::string>{ "Low", "Normal", "High", "Extreme" }, std::clamp(spell.dangerlevel - 1, 0, 3));

        ObjectCache::Menu.AddMenuToCache(newSpellMenu);
    }

    static std::shared_ptr<SpellData> ResolveMissileSpellData(const SDK::Missile& missile,
                                                              const std::string& eventSpellName) {
        auto tryLookup = [](std::string key) -> std::shared_ptr<SpellData> {
            if (key.empty()) {
                return nullptr;
            }
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);

            auto it = OnMissileSpells.find(key);
            if (it != OnMissileSpells.end() && it->second) {
                return it->second;
            }

            // Fallback: partial match for patched/mangled missile names.
            for (const auto& kv : OnMissileSpells) {
                if (kv.first.empty() || !kv.second) {
                    continue;
                }
                if (key.find(kv.first) != std::string::npos || kv.first.find(key) != std::string::npos) {
                    return kv.second;
                }
            }
            return nullptr;
        };

        if (auto byEventSpell = tryLookup(eventSpellName)) {
            return byEventSpell;
        }
        if (auto bySpellName = tryLookup(missile.GetSpellName())) {
            return bySpellName;
        }
        if (auto byMissileName = tryLookup(missile.GetMissileName())) {
            return byMissileName;
        }
        if (auto byObjectName = tryLookup(SDK::GameObject(missile.address).GetName())) {
            return byObjectName;
        }

        // Database fallback (works even if OnMissileSpells was not populated yet).
        auto fromDb = [](const SpellData* src) -> std::shared_ptr<SpellData> {
            if (!src) {
                return nullptr;
            }
            return std::make_shared<SpellData>(*src);
        };

        if (auto s = fromDb(FindSpellByMissileName(missile.GetMissileName()))) {
            return s;
        }
        if (auto s = fromDb(FindSpellBySpellName(missile.GetSpellName()))) {
            return s;
        }
        if (auto s = fromDb(FindSpellBySpellName(eventSpellName))) {
            return s;
        }

        // Last fallback: fuzzy match against full EzEvade DB (case-insensitive).
        auto toLowerCopy = [](std::string value) -> std::string {
            std::transform(value.begin(), value.end(), value.begin(), ::tolower);
            return value;
        };

        std::vector<std::string> queries;
        queries.push_back(toLowerCopy(missile.GetMissileName()));
        queries.push_back(toLowerCopy(missile.GetSpellName()));
        queries.push_back(toLowerCopy(eventSpellName));
        queries.push_back(toLowerCopy(SDK::GameObject(missile.address).GetName()));

        auto fuzzyMatch = [&queries, &toLowerCopy](const std::string& candidate) -> bool {
            if (candidate.empty()) {
                return false;
            }
            const std::string lowerCandidate = toLowerCopy(candidate);
            for (const auto& q : queries) {
                if (q.empty()) {
                    continue;
                }
                if (q == lowerCandidate || q.find(lowerCandidate) != std::string::npos || lowerCandidate.find(q) != std::string::npos) {
                    return true;
                }
            }
            return false;
        };

        for (const auto& spell : GetSpellDatabase()) {
            if (fuzzyMatch(spell.missileName) || fuzzyMatch(spell.spellName)) {
                return std::make_shared<SpellData>(spell);
            }
            for (const auto& extra : spell.extraMissileNames) {
                if (fuzzyMatch(extra)) {
                    return std::make_shared<SpellData>(spell);
                }
            }
            for (const auto& extra : spell.extraSpellNames) {
                if (fuzzyMatch(extra)) {
                    return std::make_shared<SpellData>(spell);
                }
            }
        }

        return CreateRuntimeMissileFallback(missile, eventSpellName);
    }

    static SDK::GameObject ResolveMissileCaster(const SDK::Missile& missile,
                                                int eventCasterNetId,
                                                const Vec3& startPos,
                                                const std::shared_ptr<SpellData>& spellData) {
        SDK::GameObject caster = FindHeroByNetId(eventCasterNetId > 0 ? eventCasterNetId : missile.GetCasterNetId());
        if (caster.IsValid()) {
            return caster;
        }

        // Fallback when CasterNetId is unreliable: pick nearest enemy hero, prefer champion match.
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) {
            return SDK::GameObject();
        }

        const Vec3 ref = startPos.IsValid() && !startPos.IsZero() ? startPos : missile.GetPosition();
        if (!ref.IsValid() || ref.IsZero()) {
            return SDK::GameObject();
        }

        SDK::GameObject best;
        float bestDist = FLT_MAX;
        for (const auto& hero : SDK::GameObjects::AllHeroes) {
            if (!hero.IsValid() || !hero.IsAlive()) {
                continue;
            }
            if (hero.GetTeam() == me.GetTeam()) {
                continue;
            }

            if (spellData && !spellData->charName.empty() && spellData->charName != "AllChampions") {
                if (hero.GetChampionName() != spellData->charName) {
                    continue;
                }
            }

            const float d = hero.GetPosition().Distance2D(ref);
            if (d < bestDist) {
                bestDist = d;
                best = hero;
            }
        }

        // Hard cap avoids selecting random distant hero for wrong missile.
        if (best.IsValid() && bestDist <= 2500.0f) {
            return best;
        }

        return SDK::GameObject();
    }

    static void ProcessMissileAsSpell(const SDK::Missile& missile,
                                      int eventCasterNetId = 0,
                                      const std::string& eventSpellName = "") {
        if (!missile.IsValid()) {
            return;
        }

        const int missileId = GetStableMissileId(missile);
        if (missileId > 0 && ProcessedMissileIds.find(missileId) != ProcessedMissileIds.end()) {
            return;
        }

        const auto spellData = ResolveMissileSpellData(missile, eventSpellName);
        if (!spellData) {
            return;
        }

        const auto& myHero = SDK::GameObjects::Player;
        if (!myHero.IsValid()) {
            return;
        }

        Vec3 startPos = missile.GetStartPos();
        Vec3 endPos = missile.GetEndPos();
        if (!startPos.IsValid() || startPos.IsZero()) {
            startPos = missile.GetPosition();
        }
        if (!endPos.IsValid() || endPos.IsZero()) {
            endPos = missile.GetCastEndPos();
        }
        if ((!endPos.IsValid() || endPos.IsZero()) && startPos.IsValid() && !startPos.IsZero()) {
            endPos = startPos.Extend(missile.GetPosition(), std::max(200.0f, spellData->range));
        }

        if (!startPos.IsValid() || startPos.IsZero() || !endPos.IsValid() || endPos.IsZero()) {
            return;
        }

        if (spellData->range > 0.0f && spellData->range < 5000.0f) {
            if (startPos.Distance2D(myHero.GetPosition()) >= spellData->range + 1500.0f) {
                return;
            }
        }

        SDK::GameObject caster = ResolveMissileCaster(missile, eventCasterNetId, startPos, spellData);
        if (caster.IsValid()) {
            if (!Situation::CheckTeam(caster)) {
                return;
            }
            if (!caster.IsVisible() && !ObjectCache::Menu.GetBool("DodgeFOWSpells", true)) {
                return;
            }
        }

        CreateSpellData(caster, startPos, endPos, spellData, SDK::GameObject(missile.address));

        if (missileId > 0) {
            ProcessedMissileIds.insert(missileId);
        }
    }

    static void ProcessActiveMissilesFallback() {
        const auto missiles = SDK::MissileManager::GetMissiles();
        std::unordered_set<int> aliveIds;
        aliveIds.reserve(missiles.size());

        for (const auto& missile : missiles) {
            if (!missile.IsValid()) {
                continue;
            }

            const int id = GetStableMissileId(missile);
            if (id > 0) {
                aliveIds.insert(id);
            }

            ProcessMissileAsSpell(missile);
        }

        if (!ProcessedMissileIds.empty()) {
            std::vector<int> toErase;
            toErase.reserve(ProcessedMissileIds.size());
            for (int id : ProcessedMissileIds) {
                if (aliveIds.find(id) == aliveIds.end()) {
                    toErase.push_back(id);
                }
            }
            for (int id : toErase) {
                ProcessedMissileIds.erase(id);
            }
        }
    }

    static int GetStableMissileId(const SDK::Missile& missile) {
        int id = missile.GetNetworkId();
        if (id > 0) {
            return id;
        }

        if (missile.address != 0) {
            id = (int)(std::hash<uintptr_t>{}(missile.address) & 0x7FFFFFFF);
        }
        return id;
    }

    static void EnsureSpellDictionaryLoaded() {
        if (!MenuRoot) {
            return;
        }

        const auto& myHero = SDK::GameObjects::Player;
        if (!myHero.IsValid()) {
            return;
        }

        int enemyCount = 0;
        for (const auto& hero : SDK::GameObjects::AllHeroes) {
            if (hero.IsValid() && hero.GetTeam() != myHero.GetTeam()) {
                ++enemyCount;
            }
        }

        const float now = EvadeUtils::TickCount();
        const bool needInitialLoad = OnMissileSpells.empty() || OnProcessSpells.empty();
        const bool enemyCountChanged = enemyCount != LastDictionaryEnemyCount;
        const bool periodicRefresh = (now - LastDictionaryRefreshTime) > 3000.0f;

        if (!(needInitialLoad || enemyCountChanged || periodicRefresh)) {
            return;
        }

        LoadSpellDictionary();
        LastDictionaryRefreshTime = now;
        LastDictionaryEnemyCount = enemyCount;
    }

    static void LoadSpellDictionary() {
        if (!MenuRoot) return;

        const auto& myHero = SDK::GameObjects::Player;
        if (!myHero.IsValid()) return;

        const bool devMode = ObjectCache::Menu.GetBool("DevMode", false);
        SpecialSpells::LoadSpecialSpellPlugins(devMode);

        for (const auto& hero : SDK::GameObjects::AllHeroes) {
            if (!hero.IsValid()) continue;

            if (hero.GetTeam() == myHero.GetTeam() && !devMode) {
                continue;
            }

            for (const auto& spell : GetSpellDatabase()) {
                if (!(spell.charName == hero.GetChampionName() || spell.charName == "AllChampions")) {
                    continue;
                }

                if (!(spell.spellType == SpellType::Circular || spell.spellType == SpellType::Line
                    || spell.spellType == SpellType::Arc || spell.spellType == SpellType::Cone)) {
                    continue;
                }

                auto s = std::make_shared<SpellData>(spell);
                std::string keySpell = s->spellName;
                std::string keyMissile = s->missileName.empty() ? s->spellName : s->missileName;
                std::transform(keySpell.begin(), keySpell.end(), keySpell.begin(), ::tolower);
                std::transform(keyMissile.begin(), keyMissile.end(), keyMissile.begin(), ::tolower);

                if (s->hasTrap && s->projectileSpeed > 3000.0f) {
                    if (OnProcessSpells.find(keySpell + "trap") == OnProcessSpells.end()) {
                        if (s->trapBaseName.empty()) s->trapBaseName = s->spellName + "1";
                        if (s->trapTroyName.empty()) s->trapTroyName = s->spellName + "2";

                        std::string trapBase = s->trapBaseName;
                        std::string trapTroy = s->trapTroyName;
                        std::transform(trapBase.begin(), trapBase.end(), trapBase.begin(), ::tolower);
                        std::transform(trapTroy.begin(), trapTroy.end(), trapTroy.begin(), ::tolower);

                        OnProcessTraps[trapBase] = s;
                        OnProcessTraps[trapTroy] = s;
                        OnProcessSpells[keySpell + "trap"] = s;
                        SpecialSpells::LoadSpecialSpell(*s);
                        if (TrapMenu) {
                            CreateSpellMenu(*TrapMenu, *s, true);
                        }
                    }
                    continue;
                }

                if (OnProcessSpells.find(keySpell) == OnProcessSpells.end()) {
                    OnProcessSpells[keySpell] = s;
                    OnMissileSpells[keyMissile] = s;

                    for (const auto& n : s->extraSpellNames) {
                        std::string k = n;
                        std::transform(k.begin(), k.end(), k.begin(), ::tolower);
                        OnProcessSpells[k] = s;
                    }
                    for (const auto& n : s->extraMissileNames) {
                        std::string k = n;
                        std::transform(k.begin(), k.end(), k.begin(), ::tolower);
                        OnMissileSpells[k] = s;
                    }

                    SpecialSpells::LoadSpecialSpell(*s);

                    if (SpellMenu) {
                        CreateSpellMenu(*SpellMenu, *s, false);
                    }
                }
            }
        }

        // If external spell DB failed to load (common in injected runtime without sdk/Data),
        // synthesize a dictionary from enemy spellbook + runtime missile metadata.
        if (!HasChampionSpecificSpellEntries()) {
            LoadRuntimeSpellbookFallback(devMode);
        }
    }
};

} // namespace EzEvade

#include "sdk/EzEvade/SpecialSpells/SpecialSpellRegistryImpl.h"
