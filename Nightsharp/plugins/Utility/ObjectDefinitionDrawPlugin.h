#pragma once

#include "../IPlugin.h"
#include "../../SDK/SDK.h"
#include "../../DebugLog.h"
#include "../../imgui/imgui.h"

#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace Plugins {

class ObjectDefinitionDrawPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Object Definition Draw"; }
    const char* GetInternalId() const override { return "utility.object_definition_draw"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Utility; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override {
        s_instance = this;
        rows_.clear();
        missileRows_.clear();
        missileSources_.clear();
        lastRefreshTick_ = 0;
        subscribedDelete_ =
            SDK::Events::AddOnDeleteObject(&ObjectDefinitionDrawPlugin::OnDeleteObject);
        SyncMissileCreateSubscription();
        NightSharpDebug::Logf(
            "[ObjectDefinitionDraw] loaded deleteSubscribed=%d missileCreateSubscribed=%d",
            subscribedDelete_ ? 1 : 0,
            subscribedMissileCreate_ ? 1 : 0);
    }

    void OnUnload() override {
        SDK::Events::RemoveOnMissileCreate(&ObjectDefinitionDrawPlugin::OnMissileCreateObject);
        SDK::Events::RemoveOnDeleteObject(&ObjectDefinitionDrawPlugin::OnDeleteObject);
        if (s_instance == this) {
            s_instance = nullptr;
        }
        subscribedDelete_ = false;
        subscribedMissileCreate_ = false;

        AcquireSRWLockExclusive(&lock_);
        laneClearMinion_ = {};
        lastDelete_ = {};
        missileRows_.clear();
        missileSources_.clear();
        ReleaseSRWLockExclusive(&lock_);

        rows_.clear();
        NightSharpDebug::Logf("[ObjectDefinitionDraw] unloaded");
    }

    void OnUpdate() override {
        SyncMissileCreateSubscription();
        if (!enabled_) {
            return;
        }

        const DWORD now = GetTickCount();
        if (subscribedMissileCreate_) {
            RefreshMissileSourceFilter(now);
        }
        PruneMissileRows(now);
        if (lastRefreshTick_ == 0 || now - lastRefreshTick_ >= static_cast<DWORD>(refreshMs_)) {
            RefreshSnapshot();
            lastRefreshTick_ = now;
        }
    }

    void OnRender() override {
        if (!enabled_ || !SDK::Drawing::IsEnabled()) {
            return;
        }

        const auto player = SDK::ObjectManager::Player();
        const bool hasPlayer = player.IsValid();
        const Vec3 playerPosition = hasPlayer ? player.Position() : Vec3{};
        const Vec2 rendererSize = SDK::Drawing::GetRendererSize();

        int drawn = 0;
        for (const auto& row : rows_) {
            if (drawn >= maxRows_) {
                break;
            }

            Vec3 position = {};
            float radius = 0.0f;
            if (!ResolveLiveRow(row, position, radius) ||
                !InDrawDistance(position, playerPosition, hasPlayer)) {
                continue;
            }

            Vec2 screen = {};
            if (!SDK::Drawing::WorldToScreen(position, screen) ||
                !screen.IsValid() ||
                screen.x < 0.0f ||
                screen.y < 0.0f ||
                screen.x > rendererSize.x ||
                screen.y > rendererSize.y) {
                continue;
            }

            if (drawRadius_ && radius > 1.0f) {
                SDK::Drawing::DrawCircle(
                    position,
                    radius,
                    row.color,
                    1.4f,
                    32,
                    true);
            }

            if (row.hasLine && IsValidPosition(row.lineEnd)) {
                SDK::Drawing::DrawLine(
                    position,
                    row.lineEnd,
                    row.color,
                    1.2f,
                    true);
            }

            if (drawPoint_) {
                SDK::Drawing::DrawCircle(screen, 4.0f, 1.5f, row.color, 20);
            }

            if (drawLabels_) {
                char label[192] = {};
                std::snprintf(
                    label,
                    sizeof(label),
                    "%s net=%u %s",
                    row.name[0] ? row.name : "?",
                    row.networkId,
                    row.kind);
                SDK::Drawing::DrawText(
                    Vec2(screen.x + 9.0f, screen.y - 28.0f),
                    label,
                    row.color,
                    false,
                    true);
            }

            ++drawn;
        }
    }

    void OnMenu() override {
        ImGui::Checkbox("Enabled", &enabled_);
        ImGui::Checkbox("Draw labels", &drawLabels_);
        ImGui::Checkbox("Draw point", &drawPoint_);
        ImGui::Checkbox("Draw bounding radius", &drawRadius_);
        ImGui::SliderInt("Refresh ms", &refreshMs_, 30, 2000);
        ImGui::SliderFloat("Max distance", &maxDistance_, 500.0f, 8000.0f, "%.0f");
        ImGui::SliderInt("Max rows", &maxRows_, 10, 500);

        static constexpr const char* kModes[] = {
            "Enemy minions",
            "Ally minions",
            "Jungle",
            "Plants",
            "Pets",
            "Clones",
            "Wards",
            "Enemy heroes",
            "Ally heroes",
            "Turrets",
            "Inhibitors",
            "Nexus",
            "Missiles",
        };
        ImGui::Combo(
            "Draw group",
            &mode_,
            kModes,
            static_cast<int>(sizeof(kModes) / sizeof(kModes[0])));
        SyncMissileCreateSubscription();
        if (ImGui::Button("Refresh now")) {
            RefreshSnapshot();
            lastRefreshTick_ = GetTickCount();
        }
        ImGui::SameLine(0, 8);
        if (ImGui::Button("Pick lane-clear minion")) {
            PickLaneClearMinion();
        }
        ImGui::SameLine(0, 8);
        if (ImGui::Button("Clear ref")) {
            ClearLaneClearMinion();
        }

        TrackedObject laneClear = {};
        TrackedObject lastDelete = {};
        int cachedMissiles = 0;
        int cachedMissileSources = 0;
        AcquireSRWLockShared(&lock_);
        laneClear = laneClearMinion_;
        lastDelete = lastDelete_;
        cachedMissiles = static_cast<int>(missileRows_.size());
        cachedMissileSources = static_cast<int>(missileSources_.size());
        ReleaseSRWLockShared(&lock_);

        ImGui::Separator();
        ImGui::Text("Rows: %d", static_cast<int>(rows_.size()));
        ImGui::Text("OnDelete subscribed: %s", subscribedDelete_ ? "yes" : "no");
        ImGui::Text("OnDelete seen: %lld", InterlockedCompareExchange64(&deleteEvents_, 0, 0));
        ImGui::Text("OnCreateMissile subscribed: %s", subscribedMissileCreate_ ? "yes" : "no");
        ImGui::Text("OnCreateMissile seen: %lld", InterlockedCompareExchange64(&missileCreateEvents_, 0, 0));
        ImGui::Text("OnCreateMissile accepted: %lld", InterlockedCompareExchange64(&missileAcceptedEvents_, 0, 0));
        ImGui::Text("OnCreateMissile reject source/geometry: %lld / %lld",
                    InterlockedCompareExchange64(&missileRejectedSourceEvents_, 0, 0),
                    InterlockedCompareExchange64(&missileRejectedGeometryEvents_, 0, 0));
        ImGui::Text("Ally minion/turret missiles cached: %d", cachedMissiles);
        ImGui::Text("Ally missile sources cached: %d", cachedMissileSources);
        ImGui::Text("LaneClear clears: %lld", InterlockedCompareExchange64(&laneClearClears_, 0, 0));
        ImGui::Text(
            "LaneClearMinion: net=%u idx=%u ptr=0x%p",
            laneClear.networkId,
            laneClear.index,
            reinterpret_cast<void*>(laneClear.address));
        ImGui::Text(
            "Last delete: net=%u idx=%u ptr=0x%p tick=%lu",
            lastDelete.networkId,
            lastDelete.index,
            reinterpret_cast<void*>(lastDelete.address),
            static_cast<unsigned long>(lastDelete.tick));
    }

private:
    enum DrawMode {
        ModeEnemyMinions = 0,
        ModeAllyMinions,
        ModeJungle,
        ModePlants,
        ModePets,
        ModeClones,
        ModeWards,
        ModeEnemyHeroes,
        ModeAllyHeroes,
        ModeTurrets,
        ModeInhibitors,
        ModeNexus,
        ModeMissiles,
    };

    struct Row {
        uintptr_t address = 0;
        std::uint32_t networkId = 0;
        ::Core::Objects::ObjectType type = ::Core::Objects::ObjectType::Unknown;
        std::uint32_t color = 0xFFFFFFFFu;
        Vec3 snapshotPosition = {};
        Vec3 lineEnd = {};
        float snapshotRadius = 0.0f;
        DWORD expireTick = 0;
        bool useSnapshot = false;
        bool hasLine = false;
        char kind[32] = {};
        char name[64] = {};
    };

    struct TrackedObject {
        uintptr_t address = 0;
        std::uint32_t networkId = 0;
        std::uint32_t index = 0;
        DWORD tick = 0;
    };

    struct MissileSource {
        std::uint32_t networkId = 0;
        bool turret = false;
        char name[64] = {};
    };

    static void OnDeleteObject(const SDK::Events::ObjectEventArgs& args) {
        if (s_instance) {
            s_instance->HandleDelete(args);
        }
    }

    static void OnMissileCreateObject(const SDK::Events::ObjectEventArgs& args) {
        if (s_instance) {
            s_instance->HandleMissileCreate(args);
        }
    }

    void HandleDelete(const SDK::Events::ObjectEventArgs& args) {
        InterlockedIncrement64(&deleteEvents_);

        AcquireSRWLockExclusive(&lock_);
        lastDelete_ = {
            args.Sender.Ptr,
            args.Sender.NetworkId,
            args.Sender.Index,
            GetTickCount()
        };

        // EnsoulSharp-style cleanup:
        // GameObject.OnDelete += (sender, args) =>
        // {
        //     if (LaneClearMinion != null &&
        //         sender.NetworkId == LaneClearMinion.NetworkId)
        //     {
        //         LaneClearMinion = null;
        //     }
        // };
        if (laneClearMinion_.networkId != 0 &&
            args.Sender.NetworkId == laneClearMinion_.networkId) {
            laneClearMinion_ = {};
            InterlockedIncrement64(&laneClearClears_);
        }
        ReleaseSRWLockExclusive(&lock_);
    }

    void HandleMissileCreate(const SDK::Events::ObjectEventArgs& args) {
        InterlockedIncrement64(&missileCreateEvents_);
        if (!enabled_) {
            return;
        }

        const DWORD now = GetTickCount();
        const std::uint32_t sourceNetworkId = ResolveMissileSourceNetworkId(args);
        bool sourceIsTurret = false;
        char sourceName[64] = {};
        if (!FindCachedMissileSource(sourceNetworkId, sourceIsTurret, sourceName, sizeof(sourceName))) {
            InterlockedIncrement64(&missileRejectedSourceEvents_);
            return;
        }

        Vec3 start = args.StartPosition;
        if (!IsValidPosition(start)) {
            start = args.Source.Position;
        }
        if (!IsValidPosition(start)) {
            InterlockedIncrement64(&missileRejectedGeometryEvents_);
            return;
        }

        Vec3 end = args.EndPosition;
        if (!IsValidPosition(end)) {
            end = args.CastEndPosition;
        }

        Row row = {};
        row.address = args.Raw.Rcx ? args.Raw.Rcx : args.Sender.Ptr;
        row.networkId = args.MissileNetworkId != 0
            ? args.MissileNetworkId
            : args.Sender.NetworkId;
        row.type = ::Core::Objects::ObjectType::MissileClient;
        row.color = sourceIsTurret ? 0xFFFFD24Au : 0xFF00E5FFu;
        row.snapshotPosition = start;
        row.lineEnd = end;
        row.snapshotRadius = 18.0f;
        row.expireTick = now + kMissileSnapshotTtlMs;
        row.useSnapshot = true;
        row.hasLine = IsValidPosition(end);
        std::snprintf(
            row.kind,
            sizeof(row.kind),
            "%s",
            sourceIsTurret ? "AllyTurretMissile" : "AllyMinionMissile");
        std::snprintf(
            row.name,
            sizeof(row.name),
            "%s%s%s",
            args.MissileName[0] ? args.MissileName :
            (args.SpellName[0] ? args.SpellName : "Missile"),
            sourceName[0] ? " <- " : "",
            sourceName);

        AcquireSRWLockExclusive(&lock_);
        PruneMissileRowsLocked(now);
        UpsertMissileRowLocked(row);
        ReleaseSRWLockExclusive(&lock_);
        InterlockedIncrement64(&missileAcceptedEvents_);
    }

    static bool IsValidPosition(const Vec3& position) {
        return position.IsValid() &&
               position.x >= -30000.0f && position.x <= 30000.0f &&
               position.y >= -5000.0f && position.y <= 5000.0f &&
               position.z >= -30000.0f && position.z <= 30000.0f;
    }

    static void CopyObjectName(const SDK::GameObject& object, char* out, int outCount) {
        if (!out || outCount <= 0) {
            return;
        }
        out[0] = '\0';

        const std::string& cached = object.CharacterName();
        if (!cached.empty()) {
            std::snprintf(out, static_cast<std::size_t>(outCount), "%s", cached.c_str());
            return;
        }

        char raw[96] = {};
        const uintptr_t address = object.Address();
        if (Globals::IsValidPtr(address) &&
            (::Core::Objects::ReadCharacterName(address, raw, static_cast<int>(sizeof(raw))) ||
             ::Core::Objects::ReadName(address, raw, static_cast<int>(sizeof(raw))))) {
            std::snprintf(out, static_cast<std::size_t>(outCount), "%s", raw);
        }
    }

    bool InDrawDistance(const Vec3& position, const Vec3& playerPosition, bool hasPlayer) const {
        if (!hasPlayer || maxDistance_ <= 0.0f) {
            return true;
        }
        return playerPosition.DistanceSqr2D(position) <= maxDistance_ * maxDistance_;
    }

    bool ResolveLiveRow(const Row& row, Vec3& position, float& radius) const {
        if (row.useSnapshot) {
            position = row.snapshotPosition;
            radius = row.snapshotRadius;
            return IsValidPosition(position);
        }

        SDK::GameObject object(row.address, row.type);
        if (!object.IsValid()) {
            return false;
        }

        const std::uint32_t currentNetworkId = static_cast<std::uint32_t>(object.NetworkId());
        if (row.networkId != 0 &&
            currentNetworkId != 0 &&
            currentNetworkId != row.networkId) {
            return false;
        }

        position = object.Position();
        radius = object.BoundingRadius();
        return IsValidPosition(position);
    }

    template <typename TObject>
    void AddObjectRow(const TObject& object,
                      const char* kind,
                      std::uint32_t color) {
        if (!object.IsValid()) {
            return;
        }

        Row row = {};
        row.address = object.Address();
        row.networkId = static_cast<std::uint32_t>(object.NetworkId());
        row.type = object.Type();
        row.color = color;
        std::snprintf(row.kind, sizeof(row.kind), "%s", kind ? kind : "");
        CopyObjectName(object, row.name, static_cast<int>(sizeof(row.name)));
        rows_.push_back(row);
    }

    static bool IsExpired(DWORD now, DWORD expireTick) {
        return expireTick != 0 && static_cast<LONG>(now - expireTick) >= 0;
    }

    static std::uint32_t ResolveMissileSourceNetworkId(const SDK::Events::ObjectEventArgs& args) {
        if (args.SourceNetworkId != 0 && args.SourceNetworkId != 0xFFFFFFFFu) {
            return args.SourceNetworkId;
        }
        if (args.Source.NetworkId != 0 && args.Source.NetworkId != 0xFFFFFFFFu) {
            return args.Source.NetworkId;
        }
        return 0;
    }

    bool FindCachedMissileSource(std::uint32_t networkId,
                                 bool& isTurret,
                                 char* sourceName,
                                 int sourceNameCount) {
        if (networkId == 0 || networkId == 0xFFFFFFFFu) {
            return false;
        }

        bool found = false;
        AcquireSRWLockShared(&lock_);
        for (const auto& source : missileSources_) {
            if (source.networkId != networkId) {
                continue;
            }

            isTurret = source.turret;
            if (sourceName && sourceNameCount > 0) {
                std::snprintf(
                    sourceName,
                    static_cast<std::size_t>(sourceNameCount),
                    "%s",
                    source.name);
            }
            found = true;
            break;
        }
        ReleaseSRWLockShared(&lock_);
        return found;
    }

    void SyncMissileCreateSubscription() {
        const bool shouldSubscribe = enabled_ && mode_ == ModeMissiles;
        if (shouldSubscribe == subscribedMissileCreate_) {
            return;
        }

        if (shouldSubscribe) {
            subscribedMissileCreate_ =
                SDK::Events::AddOnMissileCreate(&ObjectDefinitionDrawPlugin::OnMissileCreateObject);
            return;
        }

        if (subscribedMissileCreate_) {
            SDK::Events::RemoveOnMissileCreate(&ObjectDefinitionDrawPlugin::OnMissileCreateObject);
            subscribedMissileCreate_ = false;
        }
    }

    template <typename TObject>
    static void AddMissileSource(std::vector<MissileSource>& sources,
                                 const TObject& object,
                                 bool turret) {
        if (!object.IsValid() || !object.IsAlly()) {
            return;
        }

        const auto networkId = static_cast<std::uint32_t>(object.NetworkId());
        if (networkId == 0 || networkId == 0xFFFFFFFFu) {
            return;
        }

        for (const auto& existing : sources) {
            if (existing.networkId == networkId) {
                return;
            }
        }

        MissileSource source = {};
        source.networkId = networkId;
        source.turret = turret;
        CopyObjectName(object, source.name, static_cast<int>(sizeof(source.name)));
        sources.push_back(source);
    }

    void RefreshMissileSourceFilter(DWORD now) {
        if (lastMissileSourceRefreshTick_ != 0 &&
            now - lastMissileSourceRefreshTick_ < kMissileSourceRefreshMs) {
            return;
        }
        lastMissileSourceRefreshTick_ = now;

        std::vector<MissileSource> sources;
        sources.reserve(128);
        for (const auto& minion : SDK::GameObjects::AllyMinions()) {
            AddMissileSource(sources, minion, false);
        }
        for (const auto& turret : SDK::GameObjects::Turrets()) {
            AddMissileSource(sources, turret, true);
        }

        AcquireSRWLockExclusive(&lock_);
        missileSources_ = std::move(sources);
        ReleaseSRWLockExclusive(&lock_);
    }

    void PruneMissileRows(DWORD now) {
        AcquireSRWLockExclusive(&lock_);
        PruneMissileRowsLocked(now);
        ReleaseSRWLockExclusive(&lock_);
    }

    void PruneMissileRowsLocked(DWORD now) {
        missileRows_.erase(
            std::remove_if(
                missileRows_.begin(),
                missileRows_.end(),
                [now](const Row& row) {
                    return IsExpired(now, row.expireTick);
                }),
            missileRows_.end());
    }

    void UpsertMissileRowLocked(const Row& row) {
        if (row.networkId != 0 && row.networkId != 0xFFFFFFFFu) {
            for (auto& existing : missileRows_) {
                if (existing.networkId == row.networkId) {
                    existing = row;
                    return;
                }
            }
        }

        if (missileRows_.size() >= kMaxMissileSnapshots) {
            missileRows_.erase(missileRows_.begin());
        }
        missileRows_.push_back(row);
    }

    void TrackLaneClearMinion(const SDK::AIMinionClient& minion) {
        AcquireSRWLockShared(&lock_);
        const bool alreadyTracked = laneClearMinion_.networkId != 0;
        ReleaseSRWLockShared(&lock_);
        if (alreadyTracked) {
            return;
        }

        if (!minion.IsValid() || minion.IsDead()) {
            return;
        }

        const std::uint32_t networkId = static_cast<std::uint32_t>(minion.NetworkId());
        if (networkId == 0 || networkId == 0xFFFFFFFFu) {
            return;
        }

        AcquireSRWLockExclusive(&lock_);
        if (laneClearMinion_.networkId == 0) {
            laneClearMinion_ = {
                minion.Address(),
                networkId,
                static_cast<std::uint32_t>(minion.Index()),
                GetTickCount()
            };
        }
        ReleaseSRWLockExclusive(&lock_);
    }

    void PickLaneClearMinion() {
        for (const auto& minion : SDK::GameObjects::EnemyMinions()) {
            if (minion.IsValid() && !minion.IsDead()) {
                TrackLaneClearMinion(minion);
                return;
            }
        }
    }

    void ClearLaneClearMinion() {
        AcquireSRWLockExclusive(&lock_);
        laneClearMinion_ = {};
        ReleaseSRWLockExclusive(&lock_);
    }

    void RefreshSnapshot() {
        rows_.clear();
        rows_.reserve(256);

        switch (mode_) {
        case ModeEnemyMinions:
            for (const auto& object : SDK::GameObjects::EnemyMinions()) {
                AddObjectRow(object, "EnemyMinion", 0xFFFF6464u);
                TrackLaneClearMinion(object);
            }
            break;
        case ModeAllyMinions:
            for (const auto& object : SDK::GameObjects::AllyMinions()) {
                AddObjectRow(object, "AllyMinion", 0xFF60D9FFu);
            }
            break;
        case ModeJungle:
            for (const auto& object : SDK::GameObjects::Jungle()) {
                AddObjectRow(object, "Jungle", 0xFFFFB84Du);
            }
            break;
        case ModePlants:
            for (const auto& object : SDK::GameObjects::Plants()) {
                AddObjectRow(object, "Plant", 0xFF5CFF7Cu);
            }
            break;
        case ModePets:
            for (const auto& object : SDK::GameObjects::Pets()) {
                AddObjectRow(object, "Pet", 0xFFFF7AF2u);
            }
            break;
        case ModeClones:
            for (const auto& object : SDK::GameObjects::Clones()) {
                AddObjectRow(object, "Clone", 0xFFE078FFu);
            }
            break;
        case ModeWards:
            for (const auto& object : SDK::GameObjects::Wards()) {
                AddObjectRow(object, "Ward", 0xFF8CFFB0u);
            }
            break;
        case ModeEnemyHeroes:
            for (const auto& object : SDK::GameObjects::EnemyHeroes()) {
                AddObjectRow(object, "EnemyHero", 0xFFFFD24Au);
            }
            break;
        case ModeAllyHeroes:
            for (const auto& object : SDK::GameObjects::AllyHeroes()) {
                AddObjectRow(object, "AllyHero", 0xFF57D9FFu);
            }
            break;
        case ModeTurrets:
            for (const auto& object : SDK::GameObjects::Turrets()) {
                AddObjectRow(object, "Turret", 0xFFFF625Eu);
            }
            break;
        case ModeInhibitors:
            for (const auto& object : SDK::GameObjects::Inhibitors()) {
                AddObjectRow(object, "Inhibitor", 0xFFFF8A3Du);
            }
            break;
        case ModeNexus:
            for (const auto& object : SDK::GameObjects::Nexuses()) {
                AddObjectRow(object, "Nexus", 0xFFFF3D9Au);
            }
            break;
        case ModeMissiles:
            PruneMissileRows(GetTickCount());
            AcquireSRWLockShared(&lock_);
            rows_ = missileRows_;
            ReleaseSRWLockShared(&lock_);
            break;
        default:
            break;
        }
    }

    static inline ObjectDefinitionDrawPlugin* s_instance = nullptr;
    static constexpr DWORD kMissileSnapshotTtlMs = 3500;
    static constexpr DWORD kMissileSourceRefreshMs = 250;
    static constexpr std::size_t kMaxMissileSnapshots = 192;

    SRWLOCK lock_ = SRWLOCK_INIT;
    TrackedObject laneClearMinion_ = {};
    TrackedObject lastDelete_ = {};
    volatile LONG64 deleteEvents_ = 0;
    volatile LONG64 laneClearClears_ = 0;
    volatile LONG64 missileCreateEvents_ = 0;
    volatile LONG64 missileAcceptedEvents_ = 0;
    volatile LONG64 missileRejectedSourceEvents_ = 0;
    volatile LONG64 missileRejectedGeometryEvents_ = 0;

    std::vector<Row> rows_;
    std::vector<Row> missileRows_;
    std::vector<MissileSource> missileSources_;
    bool enabled_ = true;
    bool drawLabels_ = true;
    bool drawPoint_ = true;
    bool drawRadius_ = true;
    bool subscribedDelete_ = false;
    bool subscribedMissileCreate_ = false;
    int mode_ = ModeEnemyMinions;
    int refreshMs_ = 250;
    int maxRows_ = 160;
    float maxDistance_ = 2800.0f;
    DWORD lastRefreshTick_ = 0;
    DWORD lastMissileSourceRefreshTick_ = 0;
};

} // namespace Plugins
