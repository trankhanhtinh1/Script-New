#pragma once

#include "AwarenessEngine.h"
#include "AwarenessDiagnostics.h"
#include "../../SDK/SDK.h"
#include "../../Core/CoreAIHeroClient.h"
#include "../../Core/CoreItem.h"

#include "../../SectionProfiler.h"
#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NightSharp::Companion {

class SdkObservationBridge final {
public:
    SdkObservationBridge() {
        heroesSnapshot_.reserve(16);
        renderHeroesSnapshot_.reserve(64);
        allyLaneMinionsSnapshot_.reserve(64);
        enemyLaneMinionsSnapshot_.reserve(64);
        wardsSnapshot_.reserve(64);
        jungleLegendarySnapshot_.reserve(32);
        jungleLargeSnapshot_.reserve(32);
        inventorySnapshot_.reserve(8);
        knownWards_.reserve(64);
        knownMissiles_.reserve(128);
        knownObjectiveKinds_.reserve(16);
        knownJungleKeys_.reserve(64);
        recentCasts_.reserve(128);
    }
    ~SdkObservationBridge() { Detach(); }

    bool Attach(AwarenessEngine& awareness) {
        if (attached_) return true;
        awareness_ = &awareness;
        active_ = this;
        bool ok = true;
        ok = (SDK::Events::hook.OnCreateObject += &OnCreateObject) && ok;
        ok = (SDK::Events::hook.OnDeleteObject += &OnDeleteObject) && ok;
        ok = (SDK::Events::hook.OnMissileCreate += &OnMissileCreate) && ok;
        ok = (SDK::Events::hook.OnMissileDelete += &OnMissileDelete) && ok;
        ok = (SDK::Events::hook.OnBuffAdd += &OnBuffAdd) && ok;
        ok = (SDK::Events::hook.OnBuffRemove += &OnBuffRemove) && ok;
        ok = (SDK::Events::hook.OnNewPath += &OnNewPath) && ok;
        ok = (SDK::Events::hook.OnTeleportRaw += &OnTeleportRaw) && ok;
        ok = (SDK::Events::hook.OnDoCast += &OnDoCast) && ok;
        ok = (SDK::Events::hook.OnFinishCast += &OnFinishCast) && ok;
        ok = (SDK::Events::hook.OnSpellImpact += &OnSpellImpact) && ok;
        ok = (SDK::Events::hook.OnStopCast += &OnStopCast) && ok;
        ok = (SDK::Events::hook.OnIntegerPropertyChange += &OnIntegerPropertyChange) && ok;
        if (!ok) {
            Detach();
            return false;
        }
        attached_ = true;
        return true;
    }

    void Detach() {
        if (!attached_ && active_ != this) return;
        SDK::Events::hook.OnCreateObject -= &OnCreateObject;
        SDK::Events::hook.OnDeleteObject -= &OnDeleteObject;
        SDK::Events::hook.OnMissileCreate -= &OnMissileCreate;
        SDK::Events::hook.OnMissileDelete -= &OnMissileDelete;
        SDK::Events::hook.OnBuffAdd -= &OnBuffAdd;
        SDK::Events::hook.OnBuffRemove -= &OnBuffRemove;
        SDK::Events::hook.OnNewPath -= &OnNewPath;
        SDK::Events::hook.OnTeleportRaw -= &OnTeleportRaw;
        SDK::Events::hook.OnDoCast -= &OnDoCast;
        SDK::Events::hook.OnFinishCast -= &OnFinishCast;
        SDK::Events::hook.OnSpellImpact -= &OnSpellImpact;
        SDK::Events::hook.OnStopCast -= &OnStopCast;
        SDK::Events::hook.OnIntegerPropertyChange -= &OnIntegerPropertyChange;
        if (active_ == this) active_ = nullptr;
        attached_ = false;
        awareness_ = nullptr;
    }

    void Reset() {
        inGame_ = false;
        frame_ = 0;
        lastPingMs_ = 0;
        lastPingAt_ = -1000.0f;
        lastContextAt_ = -1000.0f;
        lastLocalHeroAt_ = -1000.0f;
        lastHeroesAt_ = -1000.0f;
        lastWardsAt_ = -1000.0f;
        lastWaveAt_ = -1000.0f;
        lastObjectivesAt_ = -1000.0f;
        lastRegistryAt_ = -1000.0f;
        lastInsightsAt_ = -1000.0f;
        mapId_ = 0;
        ruleset_ = RoleQuestRuleset::Rotating;
        swiftplay_ = false;
        lastMode_ = RuntimeMode::Companion;
        localTeam_ = 0;
        knownWards_.clear();
        knownMissiles_.clear();
        knownObjectiveKinds_.clear();
        knownJungleKeys_.clear();
        heroesSnapshot_.clear();
        renderReadIndex_.store(0, std::memory_order_release);
        renderSnapshots_[0] = {};
        renderSnapshots_[1] = {};
        allyLaneMinionsSnapshot_.clear();
        enemyLaneMinionsSnapshot_.clear();
        wardsSnapshot_.clear();
        jungleLegendarySnapshot_.clear();
        jungleLargeSnapshot_.clear();
        inventorySnapshot_.clear();
        recentCasts_.clear();
        objectiveSeen_.fill(false);
    }

    void Update() {
        NS_PROFILE("Awareness.SdkBridge.Update");
        if (!awareness_) return;
        if (!SDK::Game::IsReady()) {
            if (inGame_) {
                awareness_->Reset();
                Reset();
            }
            return;
        }

        if (!inGame_) {
            inGame_ = true;
            awareness_->Reset();
        }
        ++frame_;
        const bool firstFrame = frame_ == 1u;

        const float rawTime = SDK::Game::Time();
        if (IsDue(rawTime, lastContextAt_, 1.0f)) {
            auto scope = BeginDiagnostic(
                AwarenessDiagnostics::Stage::BridgeContext);
            const int mapId = static_cast<int>(SDK::Map::Id());
            char gameMode[64] = {};
            SDK::MissionInfo::ReadGameMode(
                gameMode, static_cast<int>(sizeof(gameMode)));
            const RuntimeMode mode = ResolveMode(gameMode);
            const RoleQuestRuleset ruleset =
                ResolveRuleset(mapId, gameMode);
            if (firstFrame || mode != lastMode_) {
                awareness_->SetMode(mode);
                lastMode_ = mode;
            }
            if (firstFrame || mapId != mapId_) {
                mapId_ = mapId;
                awareness_->SetMapId(mapId_);
            }
            if (firstFrame || ruleset != ruleset_) {
                ruleset_ = ruleset;
                swiftplay_ =
                    ruleset_ == RoleQuestRuleset::Swiftplay;
                awareness_->SetRuleset(ruleset_);
            }
            scope.SetCounts(1, 1, 0, 1);
        }

        if (IsDue(rawTime, lastPingAt_, 0.25f)) {
            lastPingMs_ = SDK::Game::Ping();
        }
        awareness_->UpdateClock(rawTime, lastPingMs_);
        const float now = awareness_->Now();
        awareness_->BeginFrame(now);
        if (firstFrame) SeedObjectiveTimers();

        const auto player = SDK::GameObjects::Player();
        if (!player.IsValid()) return;
        localTeam_ = static_cast<std::uint32_t>(player.Team());

        // These cadences are time based instead of frame based. The old
        // modulo scheduling multiplied SDK scans when FPS was uncapped.
        const bool fullHeroScan = IsDue(now, lastHeroesAt_, 0.10f);
        if (fullHeroScan) {
            auto scope = BeginDiagnostic(
                AwarenessDiagnostics::Stage::BridgeHeroes);
            const std::size_t observed = ObserveHeroes(player);
            lastLocalHeroAt_ = now;
            const std::size_t scanned = heroesSnapshot_.size();
            scope.SetCounts(
                scanned, observed,
                scanned >= observed ? scanned - observed : 0,
                scanned);
        } else if (IsDue(now, lastLocalHeroAt_, 1.0f / 30.0f)) {
            auto scope = BeginDiagnostic(
                AwarenessDiagnostics::Stage::BridgeLocalHero);
            ObserveLocalHero(player);
            scope.SetCounts(1, 1, 0, 1);
        }

        if (IsDue(now, lastWardsAt_, 0.20f)) {
            auto scope = BeginDiagnostic(
                AwarenessDiagnostics::Stage::BridgeWards);
            ObserveWards();
            scope.SetCounts(
                wardsSnapshot_.size(), 0, 0,
                wardsSnapshot_.size());
        }
        if (IsDue(now, lastWaveAt_, 0.25f)) {
            auto scope = BeginDiagnostic(
                AwarenessDiagnostics::Stage::BridgeWave);
            ObserveWave(player);
            const std::size_t scanned =
                allyLaneMinionsSnapshot_.size() +
                enemyLaneMinionsSnapshot_.size();
            scope.SetCounts(scanned, 0, 0, scanned);
        }
        if (IsDue(now, lastObjectivesAt_, 0.50f)) {
            {
                auto scope = BeginDiagnostic(
                    AwarenessDiagnostics::Stage::BridgeObjectives);
                ObserveObjectives();
                const std::size_t scanned =
                    jungleLegendarySnapshot_.size() +
                    jungleLargeSnapshot_.size();
                scope.SetCounts(scanned, 0, 0, scanned);
            }
            {
                auto scope = BeginDiagnostic(
                    AwarenessDiagnostics::Stage::BridgeJungle);
                ObserveJungleCamps(jungleLargeSnapshot_);
                scope.SetCounts(
                    jungleLargeSnapshot_.size(), 0, 0,
                    jungleLargeSnapshot_.size());
            }
        }

        if (IsDue(now, lastRegistryAt_, 5.0f)) {
            auto scope = BeginDiagnostic(
                AwarenessDiagnostics::Stage::BridgeRegistry);
            RefreshSdkRegistry(player);
            scope.SetCounts(
                inventorySnapshot_.size(), 0, 0,
                inventorySnapshot_.size());
        }
        if (IsDue(now, lastInsightsAt_, 0.25f)) {
            auto scope = BeginDiagnostic(
                AwarenessDiagnostics::Stage::BridgeInsights);
            awareness_->RefreshInsights();
            const std::size_t objects =
                awareness_->Store().ChampionCount() +
                awareness_->Store().Wards().Size() +
                awareness_->Store().Objectives().Size() +
                awareness_->Store().Jungles().Size();
            scope.SetCounts(objects, 0, 0, objects);
        }
    }


    struct RenderPosition final {
        std::uint32_t networkId = 0;
        Point3 position = {};
        bool visible = false;
        bool dead = false;
    };

    void SetDiagnostics(AwarenessDiagnostics* diagnostics) noexcept {
        diagnostics_ = diagnostics;
    }

    std::size_t RefreshRenderPositions() noexcept {
        if (!awareness_ || !SDK::Game::IsReady()) return 0;

        const std::uint32_t readIndex =
            renderReadIndex_.load(std::memory_order_acquire);
        const std::uint32_t writeIndex = readIndex == 0 ? 1u : 0u;
        RenderPositionSnapshot& snapshot =
            renderSnapshots_[writeIndex];
        snapshot.count = 0;
        renderHeroesSnapshot_.resize(renderHeroesSnapshot_.capacity());
        renderHeroesSnapshot_.resize(
            SDK::GameObjects::HeroesInto(renderHeroesSnapshot_));

        for (const auto& hero : renderHeroesSnapshot_) {
            if (!hero.IsValid() ||
                snapshot.count >= snapshot.positions.size()) {
                continue;
            }
            const std::uint32_t networkId =
                static_cast<std::uint32_t>(hero.NetworkId());
            if (networkId == 0) continue;

            RenderPosition& position =
                snapshot.positions[snapshot.count++];
            position.networkId = networkId;
            position.visible = hero.IsVisible() || hero.IsMe();
            position.dead = hero.IsDead();
            position.position = ToPoint(hero.Position());
        }
        renderReadIndex_.store(
            writeIndex, std::memory_order_release);
        return snapshot.count;
    }

    const RenderPosition* FindRenderPosition(
        std::uint32_t networkId) const noexcept {
        if (networkId == 0) return nullptr;
        const std::uint32_t readIndex =
            renderReadIndex_.load(std::memory_order_acquire);
        const RenderPositionSnapshot& snapshot =
            renderSnapshots_[readIndex];
        for (std::size_t i = 0; i < snapshot.count; ++i) {
            if (snapshot.positions[i].networkId == networkId) {
                return &snapshot.positions[i];
            }
        }
        return nullptr;
    }
    bool ReadRenderPosition(std::uint32_t networkId,
                            RenderPosition& out) const noexcept {
        const RenderPosition* position = FindRenderPosition(networkId);
        if (!position) return false;
        out = *position;
        return true;
    }

    bool IsAttached() const noexcept { return attached_; }
    RuntimeMode Mode() const noexcept { return lastMode_; }

private:
    static inline SdkObservationBridge* active_ = nullptr;
    struct RenderPositionSnapshot final {
        std::array<RenderPosition, 64> positions{};
        std::size_t count = 0;
    };

    AwarenessDiagnostics::Scope BeginDiagnostic(
        AwarenessDiagnostics::Stage stage) noexcept {
        return diagnostics_
            ? diagnostics_->Begin(stage)
            : AwarenessDiagnostics::Scope{};
    }

    static Point3 ToPoint(const Vec3& value) noexcept { return { value.x, value.y, value.z }; }
    static Vec3 ToVec3(const Point3& value) noexcept { return { value.x, value.y, value.z }; }


    static bool IsDue(float now, float& lastAt, float interval) noexcept {
        if (!std::isfinite(now)) return false;
        if (lastAt < 0.0f || now < lastAt || now - lastAt >= interval) {
            lastAt = now;
            return true;
        }
        return false;
    }

    static bool Contains(std::string_view value, std::string_view needle) noexcept {
        if (needle.empty() || value.size() < needle.size()) return false;
        for (std::size_t i = 0; i + needle.size() <= value.size(); ++i) {
            bool match = true;
            for (std::size_t j = 0; j < needle.size(); ++j) {
                if (std::tolower(static_cast<unsigned char>(value[i + j])) !=
                    std::tolower(static_cast<unsigned char>(needle[j]))) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
        return false;
    }

    static std::string_view ObjectName(const SDK::GameObject& object) {
        const std::string& characterName = object.CharacterName();
        if (!characterName.empty()) return characterName;
        return object.Name();
    }

    static bool IsObservable(const ::Core::Events::ObjectInfo& info,
                             std::uint32_t localTeam) noexcept {
        return info.IsVisible || (localTeam != 0 && info.Team == localTeam);
    }

    static bool IsObservable(const ::Core::Events::BuffEventArgs& args,
                             std::uint32_t localTeam) noexcept {
        return IsObservable(args.Sender, localTeam);
    }

    static RuntimeMode ResolveMode(std::string_view mode) {
        if (Contains(mode, "practice")) return RuntimeMode::Practice;
        if (Contains(mode, "replay")) return RuntimeMode::Replay;
        if (Contains(mode, "spectator")) return RuntimeMode::Spectator;
        return RuntimeMode::Companion;
    }
    static RoleQuestRuleset ResolveRuleset(
        int mapId, std::string_view mode) {
        if (mapId != 11) return RoleQuestRuleset::Rotating;
        return Contains(mode, "swift")
            ? RoleQuestRuleset::Swiftplay
            : RoleQuestRuleset::Standard;
    }

    static bool IsControlSpell(std::string_view name, CrowdControl& out) noexcept {
        if (Contains(name, "stun") || Contains(name, "knockdown")) { out = CrowdControl::Stun; return true; }
        if (Contains(name, "root") || Contains(name, "snare") || Contains(name, "bind")) { out = CrowdControl::Root; return true; }
        if (Contains(name, "charm")) { out = CrowdControl::Charm; return true; }
        if (Contains(name, "fear")) { out = CrowdControl::Fear; return true; }
        if (Contains(name, "taunt")) { out = CrowdControl::Taunt; return true; }
        if (Contains(name, "silence")) { out = CrowdControl::Silence; return true; }
        if (Contains(name, "polymorph")) { out = CrowdControl::Polymorph; return true; }
        if (Contains(name, "sleep")) { out = CrowdControl::Sleep; return true; }
        if (Contains(name, "suppress")) { out = CrowdControl::Suppression; return true; }
        if (Contains(name, "knockup") || Contains(name, "knockback") || Contains(name, "airborne")) { out = CrowdControl::Airborne; return true; }
        if (Contains(name, "slow")) { out = CrowdControl::Slow; return true; }
        if (Contains(name, "ground")) { out = CrowdControl::Grounded; return true; }
        out = CrowdControl::None;
        return false;
    }

    static bool IsEnemyObject(const ::Core::Events::ObjectInfo& info,
                              std::uint32_t localTeam) noexcept {
        return localTeam != 0 && info.Team != 0 && info.Team != localTeam && info.Team != 300;
    }

    std::size_t ObserveHeroes(const SDK::AIHeroClient& player) {
        heroesSnapshot_.resize(heroesSnapshot_.capacity());
        heroesSnapshot_.resize(
            SDK::GameObjects::HeroesInto(heroesSnapshot_));
        const auto& heroes = heroesSnapshot_;
        std::array<std::uint32_t, 64> observedIds{};
        std::size_t observedCount = 0;
        bool sawPlayer = false;
        for (const auto& hero : heroes) {
            if (!hero.IsValid()) continue;
            ChampionObservation observation =
                BuildChampionObservation(hero);
            sawPlayer = sawPlayer || observation.local;
            if (observedCount < observedIds.size()) {
                observedIds[observedCount++] = observation.networkId;
            }
            awareness_->ObserveChampion(observation);
        }
        if (!sawPlayer && player.IsValid()) {
            ChampionObservation observation =
                BuildChampionObservation(player);
            if (observedCount < observedIds.size()) {
                observedIds[observedCount++] = observation.networkId;
            }
            awareness_->ObserveChampion(observation);
        }
        awareness_->CompleteChampionSnapshot(
            observedIds.data(), observedCount);
        return observedCount;
    }

    void ObserveLocalHero(const SDK::AIHeroClient& player) {
        if (!player.IsValid()) return;
        awareness_->ObserveChampion(
            BuildChampionObservation(player));
    }

    ChampionObservation BuildChampionObservation(const SDK::AIHeroClient& hero) const {
        ChampionObservation observation{};
        observation.networkId = static_cast<std::uint32_t>(hero.NetworkId());
        observation.team = static_cast<std::uint32_t>(hero.Team());
        observation.local = hero.IsMe();
        observation.ally = observation.local || (localTeam_ != 0 && observation.team == localTeam_);
        observation.enemy = !observation.ally && observation.team != 0 && observation.team != 300;
        observation.visible = observation.local || hero.IsVisible();
        const ChampionState* prior = awareness_->Store().FindChampion(observation.networkId);
        observation.dead = observation.visible || observation.ally ? hero.IsDead() : (prior ? prior->dead : false);
        observation.targetable =
            observation.visible || observation.ally
                ? hero.IsTargetable()
                : (prior ? prior->targetable : false);
        observation.invulnerable =
            observation.visible || observation.ally
                ? hero.IsInvulnerable()
                : (prior ? prior->invulnerable : false);
        observation.clone = hero.IsClone();
        if (observation.visible || observation.ally) {
            const auto book = hero.Spellbook();
            const Vec3 position = hero.Position();
            const Vec3 direction = hero.Direction();
            const Vec3 pathEnd = hero.PathEnd();
            observation.position = ToPoint(position);
            observation.direction = ToPoint(direction);
            observation.pathEnd = ToPoint(pathEnd);
            observation.pathBranches = std::clamp(hero.WaypointCount(), 0, 32);
            observation.moveSpeed = std::max(0.0f, hero.MoveSpeed());
            observation.abilityHaste = std::max(0.0f, ::CoreAIHeroClient::AbilityHaste(hero.Address()));
            observation.neutralMinionsKilled = std::max(
                0.0f,
                ::CoreAIHeroClient::NeutralMinionsKilled(hero.Address()));
            observation.currentGold = std::max(
                0.0f, ::CoreAIHeroClient::Gold(hero.Address()));
            observation.totalGold = std::max(
                0.0f, ::CoreAIHeroClient::GoldTotal(hero.Address()));
            observation.health = std::max(0.0f, hero.Health());
            observation.maxHealth = std::max(0.0f, hero.MaxHealth());
            observation.mana = std::max(0.0f, hero.Mana());
            observation.maxMana = std::max(0.0f, hero.MaxMana());
            observation.level = std::clamp(hero.Level(), 0, 18);
            observation.allShield =
                std::max(0.0f, hero.AllShield());
            observation.healthRegen =
                std::max(0.0f, hero.HealthRegenRate());
            observation.recalling = hero.IsRecalling();
            observation.channeling = book.IsChanneling();
            BuildItems(hero, observation, book);
            BuildSpells(book, observation);
            BuildBuffs(hero, observation);
            observation.possession =
                IsViegoPossession(observation);
            BuildRoleQuest(observation, prior);
        } else if (prior) {
            observation.roleQuest = prior->roleQuest;
        }
        CopyText(observation.name, hero.Name());
        CopyText(observation.championId, hero.CharacterName());
        return observation;
    }

    void BuildItems(const SDK::AIHeroClient& hero,
                    ChampionObservation& observation,
                    const SDK::SpellBookClient& book) const {
        std::array<::CoreItem::ItemSlot, 8> raw{};
        const bool includeBonusSlot = observation.local || observation.ally;
        const int count = includeBonusSlot
            ? ::CoreItem::SnapshotAllSlots(
                  hero.Address(), raw.data(),
                  static_cast<int>(raw.size()))
            : ::CoreItem::SnapshotItems(
                  hero.Address(), raw.data(),
                  ::CoreItem::kVisibleSlotCount);
        const float now = awareness_->Now();
        for (int i = 0; i < count; ++i) {
            const SDK::InventorySlot slot(raw[static_cast<std::size_t>(i)]);
            if (!slot.IsValid() ||
                observation.itemCount >= observation.items.size()) {
                continue;
            }
            const int normalizedId =
                ::CoreItem::NormalizeItemId(slot.Id());
            ObservedItem& item =
                observation.items[observation.itemCount++];
            item.itemId = SDK::ItemIdFromValue(normalizedId);
            item.slot = slot.SlotIndex();
            const SDK::SpellSlot itemSpellSlot = slot.GetSpellSlot();
            const auto spell = itemSpellSlot != SDK::SpellSlot::Unknown
                ? book.GetSpell(itemSpellSlot)
                : SDK::SpellDataInstClient{};
            const bool spellValid = spell.IsValid();
            const auto* entry = slot.DatabaseEntry();
            if (entry) {
                CopyText(item.name, entry->Name);
                awareness_->Registry().ApplySdkItemData(
                    item.itemId, entry->Name,
                    entry->CooldownMin, entry->CooldownMax,
                    entry->DurationMin, entry->DurationMax,
                    entry->RangeMin, entry->RangeMax,
                    entry->Active, entry->InStore,
                    entry->PriceTotal > 0);
            } else {
                CopyText(item.name, slot.IdText());
            }
            const ItemDefinition* definition =
                awareness_->Registry().FindAvailableItem(item.itemId);
            item.active = (entry && entry->Active) ||
                          (spellValid && definition != nullptr);
            if (spellValid) {
                item.cooldownRemaining =
                    std::max(0.0f, spell.RemainingCooldown(now));
                item.charges = std::max(0, spell.Ammo());
                item.maxCharges = std::max(0, spell.MaxAmmo());
                item.usable =
                    item.cooldownRemaining <= 0.01f &&
                    spell.State(now) == CoreSpellBook::State_Ready &&
                    (item.maxCharges <= 1 || item.charges > 0);
            } else {
                item.usable = false;
            }
            if (definition) item.capability = definition->capability;
            item.evidence = {
                Provenance::VisibleNow, Confidence::Confirmed,
                now, 0.0f, HashId("sdk.item")
            };
        }
    }

    static void AddObservedSpell(
        const SDK::SpellDataInstClient& spell,
        float now,
        const PatchRegistry& registry,
        ChampionObservation& observation) {
        if (!spell.IsValid() ||
            observation.spellCount >= observation.spells.size()) {
            return;
        }
        ObservedSpell& output = observation.spells[observation.spellCount++];
        output.slot = static_cast<int>(spell.Slot());
        char scriptName[128] = {};
        char displayName[128] = {};
        const bool hasScriptName =
            spell.ReadScriptName(scriptName, static_cast<int>(sizeof(scriptName)));
        const bool hasDisplayName =
            spell.ReadName(displayName, static_cast<int>(sizeof(displayName)));
        const char* name = hasScriptName
            ? scriptName
            : (hasDisplayName ? displayName : "");
        const std::string_view nameView(name);
        CopyText(output.name, nameView);
        output.idHash = HashId(nameView);
        if (output.slot >= 4) {
            if (const auto* definition =
                    registry.ResolveAvailableSummoner(
                        output.idHash, nameView)) {
                output.capability = definition->capability;
            }
        }
        output.cooldownRemaining =
            std::max(0.0f, spell.RemainingCooldown(now));
        output.cooldownMin = output.cooldownRemaining;
        output.cooldownMax = output.cooldownRemaining;
        output.rechargeDuration =
            std::max(output.cooldownRemaining,
                     std::max(0.0f, spell.Cooldown()));
        output.manaCost = std::max(0.0f, spell.ManaCost());
        output.cooldownKind = CooldownKind::ExactObserved;
        output.charges = std::max(0, spell.Ammo());
        output.maxCharges = std::max(1, spell.MaxAmmo());
        output.ready = output.cooldownRemaining <= 0.01f &&
                       spell.State(now) == CoreSpellBook::State_Ready &&
                       (output.charges > 0 ||
                        output.maxCharges <= 1);
        output.charging = output.maxCharges > 1 &&
                          output.charges > 0 && !output.ready;
        output.evidence = {
            Provenance::VisibleNow, Confidence::Confirmed,
            now, 0.0f, HashId("sdk.spell")
        };
    }
    void BuildSpells(const SDK::SpellBookClient& book,
                     ChampionObservation& observation) const {
        static constexpr std::array<SDK::SpellSlot, 6> kSlots = {
            SDK::SpellSlot::Q, SDK::SpellSlot::W, SDK::SpellSlot::E,
            SDK::SpellSlot::R, SDK::SpellSlot::Summoner1,
            SDK::SpellSlot::Summoner2
        };
        const float now = awareness_->Now();
        const PatchRegistry& registry = awareness_->Registry();
        for (const auto slot : kSlots) {
            AddObservedSpell(
                book.GetSpell(slot), now, registry, observation);
        }
    }
    void BuildRoleQuest(ChampionObservation& observation,
                        const ChampionState* prior) const {
        const RoleQuestRuleset ruleset = ruleset_;
        observation.roleQuest = RoleQuestTracker::Resolve(
            awareness_->Registry(), observation.items,
            observation.itemCount, ruleset,
            prior ? &prior->roleQuest : nullptr);
    }


    void BuildBuffs(const SDK::AIHeroClient& hero, ChampionObservation& observation) const {
        if (!hero.IsValid() || observation.buffCount >= observation.buffs.size()) return;
        const float now = awareness_->Now();
        const auto* snapshot = CoreBuffs::GetOrBuildFrameBuffSnapshot(hero.Address(), now);
        if (!snapshot) return;
        for (int i = 0; i < snapshot->count && observation.buffCount < observation.buffs.size(); ++i) {
            const auto& source = snapshot->entries[i];
            if (!source.isActive) continue;
            ObservedBuff& target = observation.buffs[observation.buffCount++];
            target.idHash = source.hash;
            target.stacks = source.stacks;
            target.type = static_cast<int>(source.type);
            target.startTime = source.startTime;
            target.endTime = source.endTime;
            CopyText(target.name, source.name);
            target.evidence = {
                Provenance::VisibleNow, Confidence::Confirmed,
                now, source.endTime, HashId("sdk.buff")
            };
        }
    }

    static bool IsViegoPossession(
        const ChampionObservation& observation) noexcept {
        if (!TextEqualsInsensitive(observation.championId, "Viego")) {
            return false;
        }
        for (std::size_t i = 0; i < observation.buffCount; ++i) {
            const std::string_view name = observation.buffs[i].name;
            if (TextContainsInsensitive(name, "viego") &&
                (TextContainsInsensitive(name, "possess") ||
                 TextContainsInsensitive(name, "transform"))) {
                return true;
            }
        }
        return false;
    }

    void ObserveWave(const SDK::AIHeroClient& player) {
        WaveObservation observation{};
        observation.at = awareness_->Now();
        const Vec3 playerPosition = player.Position();
        Point3 centerSum{};
        int centerCount = 0;
        observation.allyFront = 100000.0f;
        observation.enemyFront = 100000.0f;
        allyLaneMinionsSnapshot_.resize(allyLaneMinionsSnapshot_.capacity());
        enemyLaneMinionsSnapshot_.resize(enemyLaneMinionsSnapshot_.capacity());
        allyLaneMinionsSnapshot_.resize(
            SDK::GameObjects::AllyLaneMinionsInto(allyLaneMinionsSnapshot_));
        enemyLaneMinionsSnapshot_.resize(
            SDK::GameObjects::EnemyLaneMinionsInto(enemyLaneMinionsSnapshot_));
        const auto observe = [&](const auto& units, bool ally) {
            for (const auto& unit : units) {
                if (!unit.IsValid() || unit.IsDead() ||
                    !unit.IsVisible()) {
                    continue;
                }
                const float distance =
                    playerPosition.Distance2D(unit.Position());
                if (distance > 2600.0f) continue;
                if (ally) {
                    ++observation.allyMinions;
                    observation.allyHealth += std::max(0.0f, unit.Health());
                    observation.allyFront =
                        std::min(observation.allyFront, distance);
                } else {
                    ++observation.enemyMinions;
                    observation.enemyHealth += std::max(0.0f, unit.Health());
                    observation.enemyFront =
                        std::min(observation.enemyFront, distance);
                }
                centerSum = centerSum + ToPoint(unit.Position());
                ++centerCount;
            }
        };
        observe(allyLaneMinionsSnapshot_, true);
        observe(enemyLaneMinionsSnapshot_, false);
        if (observation.allyFront >= 100000.0f) observation.allyFront = 0.0f;
        if (observation.enemyFront >= 100000.0f) observation.enemyFront = 0.0f;
        if (centerCount > 0) {
            observation.center =
                centerSum * (1.0f / static_cast<float>(centerCount));
        }
        awareness_->ObserveWave(observation);
    }
    void ObserveWards() {
        wardsSnapshot_.resize(wardsSnapshot_.capacity());
        wardsSnapshot_.resize(SDK::GameObjects::WardsInto(wardsSnapshot_));
        const auto& wards = wardsSnapshot_;
        const float now = awareness_->Now();
        for (const auto& ward : wards) {
            if (!ward.IsValid()) continue;
            const std::uint32_t id =
                static_cast<std::uint32_t>(ward.NetworkId());
            if (id == 0) continue;

            const std::uint32_t team =
                static_cast<std::uint32_t>(ward.Team());
            const bool ally = localTeam_ != 0 && team == localTeam_;
            const bool enemy = localTeam_ != 0 && team != 0 &&
                               team != localTeam_ && team != 300;
            const bool visible = ward.IsVisible();
            const WardState* previous = FindWard(id);
            if (enemy && !visible && !previous) {
                continue;
            }

            const std::string_view name = ObjectName(ward);
            WardState state = previous ? *previous : WardState{};
            state.networkId = id;
            state.team = team;
            state.ally = ally;
            state.enemy = enemy;
            state.visible = visible || ally;
            state.destroyed = false;
            state.destroyedAt = 0.0f;
            if (!previous) {
                state.placedAt = now;
                state.kind = ClassifyWard(name);
                const float lifetime = WardLifetime(state.kind);
                state.expiresAt = lifetime > 0.0f
                    ? state.placedAt + lifetime : 0.0f;
                state.radius =
                    state.kind == WardKind::Control ? 900.0f : 600.0f;
            }

            const bool observedNow = visible || ally;
            if (observedNow) {
                state.position = ToPoint(ward.Position());
                state.kind = ClassifyWard(name);
                state.radius =
                    state.kind == WardKind::Control ? 900.0f : 600.0f;
                ApplyFaelightState(ward, name, state, now);
                state.evidence = {
                    visible ? Provenance::VisibleNow
                            : Provenance::ObservedEvent,
                    Confidence::Confirmed, now, state.expiresAt,
                    HashId("sdk.ward")
                };
            } else {
                state.visible = false;
                const float age = std::max(
                    0.0f, now - state.evidence.observedAt);
                state.evidence.provenance = Provenance::LastSeen;
                state.evidence.confidence = age <= 15.0f
                    ? Confidence::High
                    : (age <= 45.0f ? Confidence::Medium
                                    : Confidence::Low);
            }

            const bool inserted = previous == nullptr;
            knownWards_[id] = state.placedAt;
            awareness_->ObserveWard(state);
            if (inserted && observedNow) {
                GameEvent event{};
                event.type = EventType::WardPlaced;
                event.at = now;
                event.objectId = id;
                event.position = state.position;
                event.value = static_cast<int>(state.kind);
                event.visibleAtEvent = true;
                Publish(event);
            }
        }
    }

    template <typename Unit>
    void ApplyFaelightState(const Unit& ward, std::string_view objectName,
                            WardState& state, float now) const {
        bool faelight = Contains(objectName, "faelight");
        const auto* snapshot = CoreBuffs::GetOrBuildFrameBuffSnapshot(
            ward.Address(), now);
        if (snapshot) {
            for (int i = 0; i < snapshot->count; ++i) {
                if (snapshot->entries[i].isActive &&
                    Contains(snapshot->entries[i].name, "faelight")) {
                    faelight = true;
                    break;
                }
            }
        }
        state.faelight = faelight;
        const float bonusEnd = state.placedAt + 45.0f;
        const bool bonusActive = faelight &&
            (bonusEnd <= 0.0f || now < bonusEnd);
        state.bonusVisionObserved = bonusActive;
        state.bonusVisionUntil = bonusActive ? bonusEnd : 0.0f;
        if (bonusActive) {
            // Faelight adds a fixed +25% radius while the observed
            // superward bonus window is active.  The extra region is not
            // retained after the observation window expires.
            state.radius *= 1.25f;
        }
    }

    static WardKind ClassifyWard(std::string_view name) noexcept {
        if (Contains(name, "control")) return WardKind::Control;
        if (Contains(name, "farsight") || Contains(name, "blue trinket")) return WardKind::Farsight;
        if (Contains(name, "support") || Contains(name, "quest")) return WardKind::Support;
        if (Contains(name, "zombie")) return WardKind::Zombie;
        if (Contains(name, "trap")) return WardKind::Trap;
        if (Contains(name, "faelight")) return WardKind::Faelight;
        return WardKind::Stealth;
    }

    static float WardLifetime(WardKind kind) noexcept {
        switch (kind) {
        case WardKind::Control:
        case WardKind::Farsight:
            return 0.0f;
        case WardKind::Support: return 90.0f;
        case WardKind::Zombie: return 60.0f;
        case WardKind::Trap: return 120.0f;
        default: return 90.0f;
        }
    }

    const WardState* FindWard(std::uint32_t id) const {
        const auto& wards = awareness_->Store().Wards();
        for (std::size_t i = 0; i < wards.Size(); ++i) {
            if (wards.At(i).networkId == id) return &wards.At(i);
        }
        return nullptr;
    }

    bool IsSwiftplay() const noexcept {
        return swiftplay_;
    }

    bool ObjectiveEnabled(ObjectiveKind kind) const {
        const ObjectiveDefinition* definition = awareness_->Registry().FindObjective(kind);
        if (!definition || definition->disabled || !definition->map11) return false;
        if (!IsSwiftplay()) return true;
        if (!definition->swiftplay) return false;
        return kind != ObjectiveKind::VoidGrubs && kind != ObjectiveKind::RiftHerald;
    }

    float ObjectiveFirstSpawn(const ObjectiveDefinition& definition) const {
        if (!IsSwiftplay()) return definition.firstSpawn;
        if (definition.kind == ObjectiveKind::Baron) return 720.0f;
        if (definition.kind == ObjectiveKind::ElderDragon) return 900.0f;
        return definition.firstSpawn;
    }

    void SeedObjectiveTimers() {
        if (mapId_ != 11) return;
        const float now = awareness_->Now();
        awareness_->Registry().ForEachObjective([&](const ObjectiveDefinition& definition) {
            if (!ObjectiveEnabled(definition.kind)) return;
            const float spawnAt = ObjectiveFirstSpawn(definition);
            if (spawnAt <= 0.0f) return;
            ObjectiveState state{};
            state.kind = definition.kind;
            state.spawnAt = spawnAt;
            state.status = now < spawnAt
                ? ObjectiveStatus::SpawningSoon
                : ObjectiveStatus::AliveUnknown;
            state.evidence = { Provenance::Estimated,
                               now < spawnAt ? Confidence::Low : Confidence::Medium,
                               now, spawnAt, HashId("timer.objective-seed") };
            awareness_->ObserveObjective(state);
        });
    }


    template <typename Unit>
    void ObserveObjectiveUnit(const Unit& unit, std::array<bool, 8>& seen) {
        if (!unit.IsValid()) return;
        const std::string_view name = ObjectName(unit);
        const ObjectiveKind kind = ClassifyObjective(name);
        if (kind == ObjectiveKind::Unknown || !ObjectiveEnabled(kind)) return;
        const std::size_t kindIndex = static_cast<std::size_t>(kind);
        if (kindIndex < seen.size()) seen[kindIndex] = true;

        const float now = awareness_->Now();
        const bool visible = unit.IsVisible();
        const ObjectiveState* previous = FindObjective(kind);
        if (!visible && !previous) return;
        if (!visible) {
            ObjectiveState state = *previous;
            state.visible = false;
            state.inCombat = previous && previous->combatObservedUntil > now;
            if (state.status == ObjectiveStatus::AliveVisible ||
                state.status == ObjectiveStatus::InCombatVisible) {
                state.status = ObjectiveStatus::AliveUnknown;
            }
            const float age = std::max(
                0.0f, now - state.evidence.observedAt);
            state.evidence.provenance = Provenance::LastSeen;
            state.evidence.confidence = age <= 15.0f
                ? Confidence::High : Confidence::Medium;
            awareness_->ObserveObjective(state);
            return;
        }

        ObjectiveState state = previous ? *previous : ObjectiveState{};
        state.kind = kind;
        state.networkId = static_cast<std::uint32_t>(unit.NetworkId());
        state.position = ToPoint(unit.Position());
        state.visible = true;
        state.health = std::max(0.0f, unit.Health());
        state.maxHealth = std::max(0.0f, unit.MaxHealth());
        if (unit.IsDead()) {
            state.status = ObjectiveStatus::Dead;
            state.inCombat = false;
            state.combatObservedAt = 0.0f;
            state.combatObservedUntil = 0.0f;
        } else if (state.maxHealth > 0.0f &&
                   state.health + 0.5f < state.maxHealth) {
            state.status = ObjectiveStatus::InCombatVisible;
            state.inCombat = true;
            state.combatObservedAt = now;
            state.combatObservedUntil = now + 8.0f;
        } else {
            state.status = ObjectiveStatus::AliveVisible;
            state.inCombat = previous && previous->combatObservedUntil > now;
            if (!state.inCombat) {
                state.combatObservedAt = 0.0f;
                state.combatObservedUntil = 0.0f;
            }
        }
        if (const auto* definition =
                awareness_->Registry().FindObjective(kind)) {
            state.spawnAt = ObjectiveFirstSpawn(*definition);
            if (state.status == ObjectiveStatus::Dead &&
                definition->respawn > 0.0f) {
                state.respawnAt = now + definition->respawn;
            }
        }
        state.evidence = {
            Provenance::VisibleNow, Confidence::Confirmed, now,
            state.respawnAt, HashId("sdk.objective")
        };
        const bool wasAlive = previous &&
            (previous->status == ObjectiveStatus::AliveVisible ||
             previous->status == ObjectiveStatus::AliveUnknown ||
             previous->status == ObjectiveStatus::InCombatVisible);
        if (state.networkId != 0) {
            knownObjectiveKinds_[state.networkId] = kind;
        }
        awareness_->ObserveObjective(state);
        if (!wasAlive && state.status != ObjectiveStatus::Dead) {
            GameEvent event{};
            event.type = EventType::ObjectiveSpawned;
            event.at = now;
            event.objectId = state.networkId;
            event.value = static_cast<int>(kind);
            event.position = state.position;
            event.visibleAtEvent = true;
            Publish(event);
        }
    }

    void ObserveObjectives() {
        objectiveSeen_.fill(false);
        jungleLegendarySnapshot_.resize(jungleLegendarySnapshot_.capacity());
        jungleLargeSnapshot_.resize(jungleLargeSnapshot_.capacity());
        jungleLegendarySnapshot_.resize(
            SDK::GameObjects::JungleLegendaryInto(jungleLegendarySnapshot_));
        for (const auto& unit : jungleLegendarySnapshot_) {
            ObserveObjectiveUnit(unit, objectiveSeen_);
        }
        jungleLargeSnapshot_.resize(
            SDK::GameObjects::JungleLargeInto(jungleLargeSnapshot_));
        for (const auto& unit : jungleLargeSnapshot_) {
            ObserveObjectiveUnit(unit, objectiveSeen_);
        }
    }

    static ObjectiveKind ClassifyObjective(std::string_view name) noexcept {
        if (Contains(name, "baron") || Contains(name, "nashor")) return ObjectiveKind::Baron;
        if (Contains(name, "elderdragon") || Contains(name, "elder_dragon")) return ObjectiveKind::ElderDragon;
        if (Contains(name, "dragon")) return ObjectiveKind::ElementalDragon;
        if (Contains(name, "voidgrub") || Contains(name, "void_grub")) return ObjectiveKind::VoidGrubs;
        if (Contains(name, "herald")) return ObjectiveKind::RiftHerald;
        if (Contains(name, "scuttle") || Contains(name, "crab")) return ObjectiveKind::Scuttle;
        return ObjectiveKind::Unknown;
    }

    const ObjectiveState* FindObjective(ObjectiveKind kind) const {
        for (std::size_t i = 0; i < awareness_->Store().Objectives().Size(); ++i) {
            const auto& objective = awareness_->Store().Objectives().At(i);
            if (objective.kind == kind) return &objective;
        }
        return nullptr;
    }

    void ObserveJungleCamps(
        const std::vector<SDK::AIMinionClient>& camps) {
        const float now = awareness_->Now();
        std::array<std::uint32_t, 64> observedIds{};
        std::size_t observedCount = 0;

        for (const auto& unit : camps) {
            if (!unit.IsValid()) continue;
            const std::string_view name = ObjectName(unit);
            if (ClassifyObjective(name) != ObjectiveKind::Unknown ||
                !IsPrimaryJungleMonster(name)) {
                continue;
            }

            const std::uint32_t networkId =
                static_cast<std::uint32_t>(unit.NetworkId());
            if (networkId == 0 || networkId == 0xFFFFFFFFu) continue;
            if (observedCount < observedIds.size()) {
                observedIds[observedCount++] = networkId;
            }

            const Point3 observedPosition = ToPoint(unit.Position());
            if (!observedPosition.IsValid() || observedPosition.IsZero()) {
                continue;
            }
            const std::uint32_t campKey =
                ResolveCampKey(networkId, name, observedPosition);
            const JungleCampState* previous = FindJungleCamp(campKey);
            const bool visible = unit.IsVisible();

            JungleCampState state =
                previous ? *previous : JungleCampState{};
            state.networkId = networkId;
            state.campKey = campKey;
            CopyText(state.campId, name);
            state.position = observedPosition;
            state.visible = visible;

            // GameObjects::JungleLarge only contains live primary monsters.
            // Therefore object presence itself is enough to restore an
            // out-of-vision camp after respawn; do not preserve alive=false
            // from the previous network id.
            state.alive = true;
            state.lastSeenAliveAt = now;
            state.observedDeath = false;
            state.confirmedDeathAt = 0.0f;
            state.estimatedDeathAt = 0.0f;
            state.respawnAt = 0.0f;
            state.sourceJunglerId = 0;
            state.confidence = Confidence::Confirmed;
            state.evidence = {
                visible ? Provenance::VisibleNow : Provenance::ObservedEvent,
                Confidence::Confirmed, now, 0.0f,
                visible ? HashId("sdk.jungle-visible")
                        : HashId("sdk.jungle-object")
            };

            knownJungleKeys_[networkId] = campKey;
            awareness_->ObserveJungleCamp(state);
        }

        // Reconcile missed delete callbacks against the authoritative
        // GameObjects live-id cache. This keeps timers reliable even when the
        // native delete event is dropped.
        for (auto it = knownJungleKeys_.begin();
             it != knownJungleKeys_.end();) {
            const std::uint32_t networkId = it->first;
            bool observed = false;
            for (std::size_t i = 0; i < observedCount; ++i) {
                if (observedIds[i] == networkId) {
                    observed = true;
                    break;
                }
            }
            if (observed ||
                SDK::GameObjects::GetUnitByNetworkId<SDK::AIMinionClient>(
                    static_cast<int>(networkId)).IsValid()) {
                ++it;
                continue;
            }

            if (const JungleCampState* previous =
                    FindJungleCamp(it->second)) {
                JungleCampState state = *previous;
                if (state.alive) {
                    state.visible = false;
                    state.alive = false;
                    state.observedDeath = true;
                    state.confirmedDeathAt = now;
                    state.respawnAt = now + CampRespawn(state.campId);
                    state.confidence = Confidence::Confirmed;
                    state.evidence = {
                        Provenance::ObservedEvent,
                        Confidence::Confirmed,
                        now, state.respawnAt,
                        HashId("sdk.camp-liveid-missing")
                    };
                    awareness_->ObserveJungleCamp(state);
                }
            }
            it = knownJungleKeys_.erase(it);
        }
    }

    std::uint32_t ResolveCampKey(std::uint32_t networkId, std::string_view name,
                                 const Point3& position) const {
        const auto known = knownJungleKeys_.find(networkId);
        if (known != knownJungleKeys_.end()) return known->second;
        for (std::size_t i = 0; i < awareness_->Store().Jungles().Size(); ++i) {
            const auto& camp = awareness_->Store().Jungles().At(i);
            if (TextEqualsInsensitive(std::string_view(camp.campId), name) &&
                camp.position.Distance(position) <= 1200.0f) {
                return camp.campKey;
            }
        }
        std::uint32_t key = HashId(name);
        const auto mix = [&key](std::uint32_t value) {
            key ^= value;
            key *= 16777619u;
        };
        mix(static_cast<std::uint32_t>(std::max(0.0f, position.x) / 800.0f));
        mix(static_cast<std::uint32_t>(std::max(0.0f, position.z) / 800.0f));
        return key;
    }

    const JungleCampState* FindJungleCamp(std::uint32_t campKey) const {
        for (std::size_t i = 0; i < awareness_->Store().Jungles().Size(); ++i) {
            const auto& camp = awareness_->Store().Jungles().At(i);
            if (camp.campKey == campKey) return &camp;
        }
        return nullptr;
    }

    static bool IsKnownCamp(std::string_view name) noexcept {
        return Contains(name, "blue") || Contains(name, "red") ||
               Contains(name, "gromp") || Contains(name, "krug") ||
               Contains(name, "raptor") || Contains(name, "wolf") ||
               Contains(name, "murkwolf") ||
               Contains(name, "razorbeak") ||
               Contains(name, "bramble") || Contains(name, "sentinel");
    }

    static bool IsPrimaryJungleMonster(
        std::string_view name) noexcept {
        if (!IsKnownCamp(name)) return false;
        // Child monsters use names such as MurkwolfMini,
        // RazorbeakMini and KrugMini. Never allow them to create a second
        // marker or restart the camp timer.
        return !Contains(name, "mini") &&
               !Contains(name, "small") &&
               !Contains(name, "lesser");
    }

    float CampRespawn(std::string_view name) const noexcept {
        const bool buffCamp = Contains(name, "blue") || Contains(name, "red") ||
                              Contains(name, "bramble") || Contains(name, "sentinel");
        if (IsSwiftplay()) return buffCamp ? 270.0f : 120.0f;
        return buffCamp ? 300.0f : 135.0f;
    }




    void RefreshSdkRegistry(const SDK::AIHeroClient& player) {
        player.InventoryItems(inventorySnapshot_);
        for (const auto& slot : inventorySnapshot_) {
            const int id = ::CoreItem::NormalizeItemId(slot.Id());
            const SDK::ItemId typedId = SDK::ItemIdFromValue(id);
            if (const auto* entry = slot.DatabaseEntry()) {
                awareness_->Registry().ApplySdkItemData(
                    typedId, entry->Name, entry->CooldownMin, entry->CooldownMax,
                    entry->DurationMin, entry->DurationMax, entry->RangeMin, entry->RangeMax,
                    entry->Active, entry->InStore, entry->PriceTotal > 0);
            }
        }
    }

    static GameEvent SpellEvent(const ::Core::Events::ProcessSpellEventArgs& args,
                                EventType type, float now) {
        GameEvent event{};
        event.type = type;
        event.at = now;
        event.objectId = args.Sender.NetworkId;
        event.sourceId = args.Sender.NetworkId;
        event.targetId = args.TargetNetworkId != 0 ? args.TargetNetworkId : args.Target.NetworkId;
        event.slot = args.Slot;
        event.valueFloat = args.CastDelay;
        event.duration = args.CastTime;
        event.speed = args.MissileSpeed;
        event.startPosition = ToPoint(args.StartPosition);
        event.endPosition = ToPoint(args.EndPosition);
        event.position = ToPoint(args.CastPosition);
        if (!event.startPosition.IsValid() ||
            event.startPosition.IsZero()) {
            event.startPosition = ToPoint(args.Sender.Position);
        }
        const char* name = args.SpellName[0] ? args.SpellName : (args.ScriptName[0] ? args.ScriptName : args.PayloadSpellName);
        CopyText(event.name, name);
        CopyText(event.secondaryName, args.MissileName[0] ? args.MissileName : args.PayloadMissileName);
        event.nameHash = HashId(name);
        event.visibleAtEvent = IsObservable(args.Sender, active_ ? active_->localTeam_ : 0);
        event.enemy = IsEnemyObject(args.Sender, active_ ? active_->localTeam_ : 0);
        event.ally = !event.enemy;
        return event;
    }

    void HandleSpell(const ::Core::Events::ProcessSpellEventArgs& args, EventType type) {
        if (!awareness_ || args.Sender.NetworkId == 0) return;
        GameEvent event = SpellEvent(args, type, awareness_->Now());
        if (type == EventType::SpellCastStarted) {
            recentCasts_[event.sourceId] = event;
        }
        Publish(event);
    }


    void HandleMissileCreate(const ::Core::Events::ObjectEventArgs& args) {
        if (!awareness_ || args.Sender.NetworkId == 0) return;
        const bool sourceObservable = args.Source.IsValid()
            ? IsObservable(args.Source, localTeam_)
            : IsObservable(args.Sender, localTeam_);
        // A projectile can be legitimately visible even when its caster is
        // still hidden in brush/fog. Treat either observation as an event;
        // the renderer uses the source hero's memory position for its window.
        const bool missileObservable =
            IsObservable(args.Sender, localTeam_);
        const bool eventObservable =
            sourceObservable || missileObservable;
        const bool enemySource = args.Source.IsValid()
            ? IsEnemyObject(args.Source, localTeam_)
            : IsEnemyObject(args.Sender, localTeam_);
        ThreatState threat{};
        threat.id = args.MissileNetworkId != 0 ? args.MissileNetworkId : args.Sender.NetworkId;
        threat.sourceId = args.SourceNetworkId != 0 ? args.SourceNetworkId : args.Source.NetworkId;
        threat.targetId = args.TargetNetworkId != 0 ? args.TargetNetworkId : args.Target.NetworkId;
        threat.start = ToPoint(args.StartPosition);
        threat.end = ToPoint(args.EndPosition);
        threat.startAt = awareness_->Now();
        threat.radius = 60.0f;
        threat.visible = true;
        const char* spellName = args.SpellName[0] ? args.SpellName : args.MissileName;
        threat.spellHash = HashId(spellName);
        threat.geometry = ThreatGeometry::Line;
        float missileSpeed = 0.0f;
        int spellSlot = -1;
        const auto cast = recentCasts_.find(threat.sourceId);
        if (cast != recentCasts_.end() && awareness_->Now() - cast->second.at <= 2.0f) {
            missileSpeed = cast->second.speed;
            spellSlot = cast->second.slot;
            if (threat.start.IsZero()) threat.start = cast->second.startPosition;
            if (threat.end.IsZero()) threat.end = cast->second.endPosition;
        }
        if (args.Sender.Ptr) {
            SDK::MissileClient missile(args.Sender.Ptr);
            if (missile.IsValid()) {
                const Point3 missileStart = ToPoint(missile.StartPosition());
                const Point3 missileEnd = ToPoint(missile.EndPosition());
                if (threat.start.IsZero()) threat.start = missileStart;
                if (threat.end.IsZero()) threat.end = missileEnd;
            }
        }
        const bool speedObserved = missileSpeed > 0.0f && std::isfinite(missileSpeed);
        if (!speedObserved) missileSpeed = 1200.0f;
        threat.evidence = { Provenance::ObservedEvent,
                            speedObserved ? Confidence::High : Confidence::Medium,
                            awareness_->Now(), 0.0f, HashId("sdk.missile") };
        threat.radius = ResolveSpellRadius(threat.sourceId, spellSlot, threat.radius);
        const float distance = threat.start.Distance(threat.end);
        threat.impactAt = awareness_->Now() + std::clamp(distance / missileSpeed, 0.05f, 8.0f);
        threat.endAt = threat.impactAt + 0.15f;
        IsControlSpell(spellName, threat.control);
        threat.cleanseable = threat.control != CrowdControl::None && threat.control != CrowdControl::Airborne;
        threat.blockable = true;
        threat.dodgeable = true;
        threat.damage = EstimateSpellDamage(threat.sourceId, threat.targetId, spellSlot);
        awareness_->ObserveThreat(threat);
        knownMissiles_[threat.id] = awareness_->Now();
        GameEvent event{};
        event.type = EventType::ThreatCreated;
        event.at = awareness_->Now();
        event.objectId = threat.id;
        event.sourceId = threat.sourceId;
        event.targetId = threat.targetId;
        event.nameHash = threat.spellHash;
        event.startPosition = threat.start;
        event.endPosition = threat.end;
        event.valueFloat = threat.damage;
        event.visibleAtEvent = eventObservable;
        event.enemy = enemySource;
        event.ally = !enemySource;
        CopyText(event.name, spellName);
        Publish(event);
    }
    static SDK::AIHeroClient FindHeroByNetworkId(std::uint32_t networkId) {
        if (networkId == 0) return {};
        const auto heroes = SDK::GameObjects::Heroes();
        for (const auto& hero : heroes) {
            if (hero.CachedNetworkId() == networkId) return hero;
        }
        return {};
    }

    static SDK::AIBaseClient FindUnitByNetworkId(std::uint32_t networkId) {
        if (networkId == 0) return {};
        const auto objects = SDK::GameObjects::AllGameObjects();
        for (const auto& object : objects) {
            if (object.CachedNetworkId() == networkId) {
                return SDK::AIBaseClient(object.Handle());
            }
        }
        return {};
    }

    float ResolveSpellRadius(std::uint32_t sourceId, int slot, float fallback) const {
        if (sourceId == 0 || slot < 0 || slot > 3) return fallback;
        const auto source = FindHeroByNetworkId(sourceId);
        if (!source.IsValid()) return fallback;
        const auto spell = source.Spellbook().GetSpell(static_cast<SDK::SpellSlot>(slot));
        if (!spell.IsValid()) return fallback;
        const float width = spell.LineWidth();
        return std::isfinite(width) && width > 0.0f ? std::max(25.0f, width * 0.5f) : fallback;
    }

    float EstimateSpellDamage(std::uint32_t sourceId, std::uint32_t targetId,
                              int slot) const {
        if (sourceId == 0 || slot < 0 || slot > 3) return 0.0f;
        if (targetId == 0) {
            awareness_->Store().ForEachChampion([&](const ChampionState& state) {
                if (state.local) targetId = state.networkId;
            });
        }
        if (targetId == 0) return 0.0f;
        const auto source = FindHeroByNetworkId(sourceId);
        const auto target = FindUnitByNetworkId(targetId);
        if (!source.IsValid() || !target.IsValid()) return 0.0f;
        return std::max(0.0f, source.GetSpellDamage(
            target, static_cast<SDK::SpellSlot>(slot)));
    }

    void HandleMissileDelete(const ::Core::Events::ObjectEventArgs& args) {
        const std::uint32_t id = args.MissileNetworkId != 0 ? args.MissileNetworkId : args.Sender.NetworkId;
        if (id == 0) return;
        awareness_->RemoveThreat(id);
        knownMissiles_.erase(id);
    }

    void HandleBuff(const ::Core::Events::BuffEventArgs& args, EventType type) {
        if (!awareness_ || args.Sender.NetworkId == 0 || !IsObservable(args, localTeam_)) return;
        GameEvent event{};
        event.type = type;
        event.at = awareness_->Now();
        event.objectId = args.Sender.NetworkId;
        event.value = args.Count;
        event.duration = std::max(0.0f, args.EndTime - args.StartTime);
        event.nameHash = CoreBuffs::HashName(args.BuffName);
        CopyText(event.name, args.BuffName);
        event.visibleAtEvent = true;
        event.enemy = IsEnemyObject(args.Sender, localTeam_);
        event.ally = !event.enemy;
        Publish(event);
    }

    void HandlePath(const ::Core::Events::NewPathEventArgs& args) {
        if (!awareness_ || args.Sender.NetworkId == 0 || !IsObservable(args.Sender, localTeam_)) return;
        GameEvent event{};
        event.type = EventType::PathChanged;
        event.at = awareness_->Now();
        event.objectId = args.Sender.NetworkId;
        event.value = args.PathCount;
        event.speed = args.Speed;
        event.visibleAtEvent = true;
        event.startPosition = ToPoint(args.Sender.Position);
        if (args.PathCount > 0) {
            event.endPosition = ToPoint(
                args.Path[std::min(args.PathCount, 32) - 1]);
        }
        Publish(event);
    }

    void HandleTeleport(const ::Core::Events::TeleportEventArgs& args) {
        if (!awareness_ || args.Sender.NetworkId == 0 || !IsObservable(args.Sender, localTeam_)) return;
        bool recall = Contains(args.RecallType, "recall") ||
                      Contains(args.RecallName, "recall");
        const bool ended = args.RecallType[0] == '\0' && args.RecallName[0] == '\0';
        if (ended) {
            if (const ChampionState* state =
                    awareness_->Store().FindChampion(args.Sender.NetworkId)) {
                recall = state->recalling;
            }
        }
        GameEvent event{};
        event.type = ended
            ? (recall ? EventType::RecallEnded : EventType::TeleportEnded)
            : (recall ? EventType::RecallStarted : EventType::TeleportStarted);
        event.at = awareness_->Now();
        event.objectId = args.Sender.NetworkId;
        event.duration = recall ? 8.0f : 4.0f;
        event.visibleAtEvent = true;
        CopyText(event.name, args.RecallType);
        CopyText(event.secondaryName, args.RecallName);
        Publish(event);
    }

    void HandleStopCast(const ::Core::Events::StopCastEventArgs& args) {
        if (!awareness_ || args.CasterNetworkId == 0) return;
        const ChampionState* caster = awareness_->Store().FindChampion(args.CasterNetworkId);
        if (caster && caster->enemy && !caster->visible) return;
        GameEvent event{};
        event.type = EventType::SpellCastCancelled;
        event.at = awareness_->Now();
        event.objectId = args.CasterNetworkId;
        event.slot = args.Slot;
        event.value = args.HasBeenCast ? 1 : 0;
        event.visibleAtEvent = !caster || caster->visible || caster->ally || caster->local;
        Publish(event);
    }

    void HandleIntegerChange(const ::Core::Events::IntegerPropertyChangeEventArgs& args) {
        if (!awareness_ || args.Sender.NetworkId == 0 || !IsObservable(args.Sender, localTeam_)) return;
        const std::string_view property(args.Property);
        GameEvent event{};
        event.at = awareness_->Now();
        event.objectId = args.Sender.NetworkId;
        event.value = args.NewValue;
        event.visibleAtEvent = true;
        if (Contains(property, "item") || Contains(property, "inventory")) event.type = EventType::InventoryChanged;
        else if (Contains(property, "spell") || Contains(property, "summoner")) event.type = EventType::SummonerSpellChanged;
        else return;
        CopyText(event.name, args.Property);
        Publish(event);
    }

    void HandleObjectCreate(const ::Core::Events::ObjectEventArgs& args) {
        if (!awareness_ || args.Sender.NetworkId == 0 || !IsObservable(args.Sender, localTeam_)) return;
        GameEvent event{};
        event.type = EventType::ObjectCreated;
        event.at = awareness_->Now();
        event.objectId = args.Sender.NetworkId;
        event.position = ToPoint(args.Sender.Position);
        event.visibleAtEvent = true;
        event.enemy = IsEnemyObject(args.Sender, localTeam_);
        event.ally = !event.enemy;
        CopyText(event.name, args.Sender.CharacterName[0] ? args.Sender.CharacterName : args.Sender.Name);
        Publish(event);
    }

    void HandleObjectDelete(const ::Core::Events::ObjectEventArgs& args) {
        if (!awareness_ || args.Sender.NetworkId == 0) return;
        const std::uint32_t id = args.Sender.NetworkId;
        const bool observable = IsObservable(args.Sender, localTeam_);

        if (observable && knownWards_.contains(id)) {
            for (std::size_t i = 0; i < awareness_->Store().Wards().Size(); ++i) {
                const WardState& previous = awareness_->Store().Wards().At(i);
                if (previous.networkId != id) continue;
                WardState ward = previous;
                ward.destroyed = true;
                ward.visible = false;
                ward.destroyedAt = awareness_->Now();
                ward.bonusVisionUntil = 0.0f;
                ward.evidence = { Provenance::ObservedEvent, Confidence::Confirmed,
                                  awareness_->Now(), 0.0f, HashId("sdk.ward-delete") };
                awareness_->ObserveWard(ward);
                GameEvent destroyed{};
                destroyed.type = EventType::WardDestroyed;
                destroyed.at = awareness_->Now();
                destroyed.objectId = id;
                destroyed.position = ward.position;
                destroyed.visibleAtEvent = true;
                destroyed.ally = ward.ally;
                destroyed.enemy = ward.enemy;
                Publish(destroyed);
                break;
            }
        }
        knownWards_.erase(id);

        const auto objectiveKind = knownObjectiveKinds_.find(id);
        if (observable && objectiveKind != knownObjectiveKinds_.end()) {
            if (const ObjectiveState* previous = FindObjective(objectiveKind->second)) {
                ObjectiveState objective = *previous;
                objective.status = ObjectiveStatus::Dead;
                objective.visible = false;
                objective.health = 0.0f;
                objective.deathAt = awareness_->Now();
                if (const ObjectiveDefinition* definition =
                        awareness_->Registry().FindObjective(objective.kind);
                    definition && definition->respawn > 0.0f) {
                    objective.respawnAt = awareness_->Now() + definition->respawn;
                }
                objective.evidence = { Provenance::ObservedEvent, Confidence::Confirmed,
                                       awareness_->Now(), objective.respawnAt,
                                       HashId("sdk.objective-delete") };
                awareness_->ObserveObjective(objective);
            }
        }
        if (objectiveKind != knownObjectiveKinds_.end()) knownObjectiveKinds_.erase(objectiveKind);

        const auto jungle = knownJungleKeys_.find(id);
        // JungleLarge is an authoritative global object snapshot. Once a
        // tracked primary monster is deleted, start its camp timer even when
        // the object was outside the local camera/vision at deletion time.
        if (jungle != knownJungleKeys_.end()) {
            if (const JungleCampState* previous = FindJungleCamp(jungle->second)) {
                JungleCampState camp = *previous;
                camp.visible = false;
                camp.alive = false;
                camp.observedDeath = true;
                camp.confirmedDeathAt = awareness_->Now();
                camp.respawnAt = awareness_->Now() + CampRespawn(camp.campId);
                camp.confidence = Confidence::Confirmed;
                camp.evidence = { Provenance::ObservedEvent, Confidence::Confirmed,
                                  awareness_->Now(), camp.respawnAt, HashId("sdk.camp-delete") };
                awareness_->ObserveJungleCamp(camp);
            }
        }
        if (jungle != knownJungleKeys_.end()) knownJungleKeys_.erase(jungle);

        recentCasts_.erase(id);

        if (!observable && IsEnemyObject(args.Sender, localTeam_)) return;
        GameEvent event{};
        event.type = EventType::ObjectDeleted;
        event.at = awareness_->Now();
        event.objectId = id;
        event.visibleAtEvent = observable;
        event.enemy = IsEnemyObject(args.Sender, localTeam_);
        event.ally = !event.enemy;
        CopyText(event.name, args.Sender.CharacterName[0] ? args.Sender.CharacterName : args.Sender.Name);
        Publish(event);
    }

    void Publish(const GameEvent& event) {
        if (!awareness_) return;
        awareness_->ApplyEvent(event);
        awareness_->Bus().Publish(event);
    }

    static void OnCreateObject(const SDK::Events::ObjectEventArgs& args) { if (active_) active_->HandleObjectCreate(args); }
    static void OnDeleteObject(const SDK::Events::ObjectEventArgs& args) { if (active_) active_->HandleObjectDelete(args); }
    static void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) { if (active_) active_->HandleMissileCreate(args); }
    static void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) { if (active_) active_->HandleMissileDelete(args); }
    static void OnBuffAdd(const SDK::Events::BuffEventArgs& args) { if (active_) active_->HandleBuff(args, EventType::BuffAdded); }
    static void OnBuffRemove(const SDK::Events::BuffEventArgs& args) { if (active_) active_->HandleBuff(args, EventType::BuffRemoved); }
    static void OnNewPath(const SDK::Events::NewPathEventArgs& args) { if (active_) active_->HandlePath(args); }
    static void OnTeleportRaw(const SDK::Events::TeleportRawEventArgs& args) { if (active_) active_->HandleTeleport(args); }
    static void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) { if (active_) active_->HandleSpell(args, EventType::SpellCastStarted); }
    static void OnFinishCast(const SDK::Events::ProcessSpellEventArgs& args) { if (active_) active_->HandleSpell(args, EventType::SpellCastCompleted); }
    static void OnSpellImpact(const SDK::Events::ProcessSpellEventArgs& args) { if (active_) active_->HandleSpell(args, EventType::DamageObserved); }
    static void OnStopCast(const SDK::Events::StopCastEventArgs& args) { if (active_) active_->HandleStopCast(args); }
    static void OnIntegerPropertyChange(const SDK::Events::IntegerPropertyChangeEventArgs& args) { if (active_) active_->HandleIntegerChange(args); }

    AwarenessEngine* awareness_ = nullptr;
    AwarenessDiagnostics* diagnostics_ = nullptr;
    bool attached_ = false;
    bool inGame_ = false;
    std::uint64_t frame_ = 0;
    int lastPingMs_ = 0;
    float lastPingAt_ = -1000.0f;
    float lastContextAt_ = -1000.0f;
    float lastLocalHeroAt_ = -1000.0f;
    float lastHeroesAt_ = -1000.0f;
    float lastWardsAt_ = -1000.0f;
    float lastWaveAt_ = -1000.0f;
    float lastObjectivesAt_ = -1000.0f;
    float lastRegistryAt_ = -1000.0f;
    float lastInsightsAt_ = -1000.0f;
    int mapId_ = 0;
    RoleQuestRuleset ruleset_ = RoleQuestRuleset::Rotating;
    bool swiftplay_ = false;
    std::uint32_t localTeam_ = 0;
    std::vector<SDK::AIHeroClient> heroesSnapshot_{};
    std::vector<SDK::AIHeroClient> renderHeroesSnapshot_{};
    std::array<RenderPositionSnapshot, 2> renderSnapshots_{};
    std::atomic<std::uint32_t> renderReadIndex_{0};
    std::vector<SDK::AIMinionClient> allyLaneMinionsSnapshot_{};
    std::vector<SDK::AIMinionClient> enemyLaneMinionsSnapshot_{};
    std::vector<SDK::AIMinionClient> wardsSnapshot_{};
    std::vector<SDK::AIMinionClient> jungleLegendarySnapshot_{};
    std::vector<SDK::AIMinionClient> jungleLargeSnapshot_{};
    std::vector<SDK::InventorySlot> inventorySnapshot_{};
    RuntimeMode lastMode_ = RuntimeMode::Companion;
    std::unordered_map<std::uint32_t, float> knownWards_{};
    std::unordered_map<std::uint32_t, float> knownMissiles_{};
    std::unordered_map<std::uint32_t, ObjectiveKind> knownObjectiveKinds_{};
    std::unordered_map<std::uint32_t, std::uint32_t> knownJungleKeys_{};
    std::unordered_map<std::uint32_t, GameEvent> recentCasts_{};
    std::array<bool, 8> objectiveSeen_{};
};

} // namespace NightSharp::Companion
