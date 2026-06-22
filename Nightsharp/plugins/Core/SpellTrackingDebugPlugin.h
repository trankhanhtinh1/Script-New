#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreRuntime.h"
#include "../../Core/Globals.h"
#include "../../Core/offset.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../imgui/imgui.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace Plugins {

class SpellTrackingDebugPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Spell Tracking Debug"; }
    const char* GetInternalId() const override { return "core.spell_tracking_debug"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return true; }

    bool CanLoad() const override {
        return CoreRuntime::EnsureInitialized() &&
               CoreRuntime::RefreshReadState();
    }

    void OnLoad() override {
        s_instance = this;
        ResetRecords();
        ResetCache();
        SDK::Events::hook.OnProcessSpell += &SpellTrackingDebugPlugin::OnProcessSpell;
        SDK::Events::hook.OnDoCast += &SpellTrackingDebugPlugin::OnDoCast;
        NightSharpDebug::Logf("[SpellTrackingDebug] loaded");
    }

    void OnUnload() override {
        SDK::Events::hook.OnDoCast -= &SpellTrackingDebugPlugin::OnDoCast;
        SDK::Events::hook.OnProcessSpell -= &SpellTrackingDebugPlugin::OnProcessSpell;
        ResetRecords();
        ResetCache();
        if (s_instance == this) {
            s_instance = nullptr;
        }
        NightSharpDebug::Logf("[SpellTrackingDebug] unloaded");
    }

    void OnUpdate() override {
        RefreshCacheIfDue();
    }

    void OnRender() override {
        if (!ImGui::GetCurrentContext()) {
            return;
        }

        RefreshCacheIfDue();

        UnitSnapshot units[kMaxUnits] = {};
        int count = 0;
        CopyUnits(units, count);

        const float gameTime = SDK::Game::Time();
        const int now = SDK::Game::TickCount();
        if (m_drawRightTracker) {
            DrawRightTracker(units, count, gameTime, now);
        }
        if (m_drawWorldCompact) {
            DrawWorldCompact(units, count, gameTime, now);
        }
        if (m_drawDebugWindow) {
            DrawDebugWindow(units, count, gameTime, now);
        }
    }

    void OnMenu() override {
        ImGui::Checkbox("Right spell tracker", &m_drawRightTracker);
        ImGui::Checkbox("World compact text", &m_drawWorldCompact);
        ImGui::Checkbox("Debug table", &m_drawDebugWindow);
        ImGui::Checkbox("Show player row", &m_showPlayer);
        ImGui::Checkbox("Show allies", &m_showAllies);
        ImGui::SliderInt("Refresh ms", &m_refreshMs, 100, 1000);
        if (ImGui::Button("Clear spell event cache")) {
            ResetRecords();
        }
    }

private:
    static inline SpellTrackingDebugPlugin* s_instance = nullptr;
    static constexpr int kMaxRecords = 64;
    static constexpr int kMaxUnits = 12;
    static constexpr int kSlotCount = 6;
    static constexpr int kRecentCastMs = 1800;
    static inline constexpr SDK::SpellSlot kTrackedSlots[kSlotCount] = {
        SDK::SpellSlot::Q,
        SDK::SpellSlot::W,
        SDK::SpellSlot::E,
        SDK::SpellSlot::R,
        SDK::SpellSlot::Summoner1,
        SDK::SpellSlot::Summoner2
    };

    struct CastRecord {
        bool active = false;
        std::uint32_t networkId = 0;
        int slot = -1;
        int level = 0;
        int tick = 0;
        float castTime = 0.0f;
        float cooldown = 0.0f;
        float expireTime = 0.0f;
        char source[12] = {};
    };

    struct SpellSnapshot {
        bool valid = false;
        int level = 0;
        float cooldown = 0.0f;
        float remainingAtSample = 0.0f;
    };

    struct UnitSnapshot {
        bool active = false;
        bool player = false;
        bool enemy = false;
        bool ally = false;
        uintptr_t address = 0;
        std::uint32_t networkId = 0;
        int level = 0;
        Vec3 position = {};
        float sampleTime = 0.0f;
        char name[32] = {};
        SpellSnapshot spells[kSlotCount] = {};
    };

    mutable SRWLOCK m_recordLock = SRWLOCK_INIT;
    mutable SRWLOCK m_cacheLock = SRWLOCK_INIT;
    CastRecord m_records[kMaxRecords] = {};
    UnitSnapshot m_units[kMaxUnits] = {};
    int m_recordCursor = 0;
    int m_unitCount = 0;
    float m_nextRefreshTime = 0.0f;
    int m_refreshMs = 200;
    bool m_drawRightTracker = true;
    bool m_drawWorldCompact = true;
    bool m_drawDebugWindow = false;
    bool m_showPlayer = true;
    bool m_showAllies = false;

    static bool IsTrackedSlot(int slot) {
        return slot >= 0 && slot < kSlotCount;
    }

    static const char* SlotLabel(int slot) {
        switch (slot) {
        case 0: return "Q";
        case 1: return "W";
        case 2: return "E";
        case 3: return "R";
        case 4: return "D";
        case 5: return "F";
        default: return "?";
        }
    }

    static bool IsSaneCooldown(float value) {
        return std::isfinite(value) && value >= 0.0f && value < 1000.0f;
    }

    static float ReadAbilityHaste(uintptr_t hero) {
        if (!Globals::IsValidPtr(hero)) {
            return 0.0f;
        }

        const float haste =
            Globals::Read<float>(hero + Offset::AIHeroClient::AbilityHaste);
        return std::isfinite(haste) && haste > 0.0f && haste < 500.0f
            ? haste
            : 0.0f;
    }

    static float ApplyCooldownHaste(float cooldown, int slot, uintptr_t hero) {
        if (!IsSaneCooldown(cooldown) || cooldown <= 0.0f) {
            return 0.0f;
        }

        if (slot >= 0 && slot <= 3) {
            const float haste = ReadAbilityHaste(hero);
            if (haste > 0.0f) {
                cooldown *= 100.0f / (100.0f + haste);
            }
        }
        return IsSaneCooldown(cooldown) ? cooldown : 0.0f;
    }

    static float ReadResourceCooldown(uintptr_t resource, int level) {
        if (!Globals::IsValidPtr(resource)) {
            return 0.0f;
        }

        const int levelIndex = std::max(0, std::min(5, level - 1));
        const float cooldown = Globals::Read<float>(
            resource +
            Offset::SpellDataResourceLayout::ResCooldownTime +
            static_cast<uintptr_t>(levelIndex) * sizeof(float));
        return IsSaneCooldown(cooldown) ? cooldown : 0.0f;
    }

    static float ResolveCastCooldown(const SDK::Events::ProcessSpellEventArgs& args,
                                     int& outLevel) {
        outLevel = 0;
        if (!IsTrackedSlot(args.Slot)) {
            return 0.0f;
        }

        SDK::AIHeroClient caster =
            SDK::ObjectManager::GetUnitByNetworkId<SDK::AIHeroClient>(
                static_cast<int>(args.Sender.NetworkId));
        if (!caster.IsValid() && Globals::IsValidPtr(args.Sender.Ptr)) {
            caster = SDK::AIHeroClient(args.Sender.Ptr);
        }

        float cooldown = 0.0f;
        uintptr_t resource = args.SpellDataResource;
        if (caster.IsValid()) {
            const auto spell =
                caster.GetSpell(static_cast<SDK::SpellSlot>(args.Slot));
            if (spell.IsValid()) {
                outLevel = spell.Level();
                if (!Globals::IsValidPtr(resource)) {
                    resource = spell.DataResourcePointer();
                }
                cooldown = ReadResourceCooldown(resource, outLevel);
                if (cooldown <= 0.0f) {
                    cooldown = spell.Cooldown();
                }
                cooldown = ApplyCooldownHaste(
                    cooldown,
                    args.Slot,
                    caster.Address());
            }
        }

        if (cooldown <= 0.0f && Globals::IsValidPtr(resource)) {
            if (outLevel <= 0) {
                outLevel = 1;
            }
            cooldown = ReadResourceCooldown(resource, outLevel);
        }
        return IsSaneCooldown(cooldown) ? cooldown : 0.0f;
    }

    static float DisplayRemaining(const UnitSnapshot& unit,
                                  const SpellSnapshot& spell,
                                  float gameTime) {
        if (!spell.valid || spell.remainingAtSample <= 0.0f) {
            return 0.0f;
        }

        const float elapsed = std::max(0.0f, gameTime - unit.sampleTime);
        return std::max(0.0f, spell.remainingAtSample - elapsed);
    }

    static ImU32 SpellFillColor(const SpellSnapshot& spell, float remaining) {
        if (!spell.valid || spell.level <= 0) {
            return IM_COL32(36, 38, 42, 225);
        }
        if (remaining > 0.05f) {
            return IM_COL32(78, 60, 35, 235);
        }
        return IM_COL32(24, 84, 47, 235);
    }

    static ImU32 SpellTextColor(const SpellSnapshot& spell, float remaining) {
        if (!spell.valid || spell.level <= 0) {
            return IM_COL32(150, 150, 150, 255);
        }
        if (remaining > 0.05f) {
            return IM_COL32(255, 218, 96, 255);
        }
        return IM_COL32(125, 255, 155, 255);
    }

    static void FormatRemaining(float remaining, char* out, int outCount) {
        if (!out || outCount <= 0) {
            return;
        }
        if (remaining <= 0.05f) {
            out[0] = 0;
        } else if (remaining < 10.0f) {
            std::snprintf(out, outCount, "%.1f", remaining);
        } else {
            std::snprintf(out, outCount, "%.0f", std::ceil(remaining));
        }
    }

    static const char* UnitKind(const UnitSnapshot& unit) {
        if (unit.player) {
            return "P";
        }
        return unit.enemy ? "E" : "A";
    }

    bool ShouldShowUnit(const UnitSnapshot& unit) const {
        if (!unit.active) {
            return false;
        }
        if (unit.player) {
            return m_showPlayer;
        }
        return unit.enemy || m_showAllies;
    }

    void RefreshCacheIfDue() {
        const float gameTime = SDK::Game::Time();
        if (gameTime <= 0.0f || gameTime < m_nextRefreshTime) {
            return;
        }

        m_nextRefreshTime =
            gameTime + static_cast<float>(std::clamp(m_refreshMs, 100, 1000)) / 1000.0f;
        RefreshCache(gameTime);
    }

    void RefreshCache(float gameTime) {
        UnitSnapshot next[kMaxUnits] = {};
        int count = 0;

        const auto player = SDK::ObjectManager::Player();
        const auto playerSnapshot = player.Snapshot();
        const std::uint32_t playerTeam = playerSnapshot.team;
        const std::uint32_t playerNetworkId = playerSnapshot.handle.networkId;

        if (playerSnapshot.IsValid() && count < kMaxUnits) {
            FillUnit(player, playerSnapshot, gameTime, true, false, false, next[count++]);
        }

        for (const auto& hero : SDK::ObjectManager::Get<SDK::AIHeroClient>()) {
            if (count >= kMaxUnits || !hero.IsValid()) {
                continue;
            }

            const auto snapshot = hero.Snapshot();
            if (!snapshot.IsValid() ||
                snapshot.handle.networkId == playerNetworkId) {
                continue;
            }

            const bool enemy =
                playerTeam != 0 &&
                snapshot.team != 0 &&
                snapshot.team != playerTeam;
            const bool ally =
                playerTeam != 0 &&
                snapshot.team != 0 &&
                snapshot.team == playerTeam;
            FillUnit(hero, snapshot, gameTime, false, enemy, ally, next[count++]);
        }

        AcquireSRWLockExclusive(&m_cacheLock);
        for (int i = 0; i < kMaxUnits; ++i) {
            m_units[i] = i < count ? next[i] : UnitSnapshot{};
        }
        m_unitCount = count;
        ReleaseSRWLockExclusive(&m_cacheLock);
    }

    static void FillUnit(const SDK::AIHeroClient& hero,
                         const ::Core::Objects::ObjectSnapshot& snapshot,
                         float gameTime,
                         bool player,
                         bool enemy,
                         bool ally,
                         UnitSnapshot& out) {
        out = {};
        out.active = true;
        out.player = player;
        out.enemy = enemy;
        out.ally = ally;
        out.address = snapshot.handle.address;
        out.networkId = snapshot.handle.networkId;
        out.level = snapshot.level;
        out.position = snapshot.position;
        out.sampleTime = gameTime;
        const char* name = snapshot.characterName[0]
            ? snapshot.characterName
            : snapshot.name;
        strncpy_s(out.name, name ? name : "", _TRUNCATE);

        for (int i = 0; i < kSlotCount; ++i) {
            const auto spell = hero.GetSpell(kTrackedSlots[i]);
            auto& slot = out.spells[i];
            if (!spell.IsValid()) {
                continue;
            }

            slot.valid = true;
            slot.level = spell.Level();
            slot.cooldown = spell.Cooldown();
            slot.remainingAtSample = spell.RemainingCooldown(gameTime);
            if (!IsSaneCooldown(slot.cooldown)) {
                slot.cooldown = 0.0f;
            }
            if (!IsSaneCooldown(slot.remainingAtSample)) {
                slot.remainingAtSample = 0.0f;
            }
        }
    }

    void CopyUnits(UnitSnapshot* out, int& count) const {
        count = 0;
        if (!out) {
            return;
        }

        AcquireSRWLockShared(&m_cacheLock);
        count = std::clamp(m_unitCount, 0, kMaxUnits);
        for (int i = 0; i < count; ++i) {
            out[i] = m_units[i];
        }
        ReleaseSRWLockShared(&m_cacheLock);
    }

    void ResetCache() {
        AcquireSRWLockExclusive(&m_cacheLock);
        for (auto& unit : m_units) {
            unit = {};
        }
        m_unitCount = 0;
        m_nextRefreshTime = 0.0f;
        ReleaseSRWLockExclusive(&m_cacheLock);
    }

    bool TryGetCastRecord(std::uint32_t networkId,
                          int slot,
                          CastRecord& out) const {
        out = {};
        if (networkId == 0 || networkId == 0xFFFFFFFFu) {
            return false;
        }

        bool found = false;
        int newestTick = -1;
        AcquireSRWLockShared(&m_recordLock);
        for (const auto& record : m_records) {
            if (!record.active ||
                record.networkId != networkId ||
                (slot >= 0 && record.slot != slot)) {
                continue;
            }

            if (record.tick <= newestTick) {
                continue;
            }
            newestTick = record.tick;
            out = record;
            found = true;
        }
        ReleaseSRWLockShared(&m_recordLock);
        return found;
    }

    void RecordCast(const SDK::Events::ProcessSpellEventArgs& args,
                    const char* source) {
        if (!args.Sender.IsValid() || !IsTrackedSlot(args.Slot)) {
            return;
        }

        CastRecord record{};
        record.active = true;
        record.networkId = args.Sender.NetworkId;
        record.slot = args.Slot;
        record.tick = SDK::Game::TickCount();
        record.castTime = SDK::Game::Time();
        record.cooldown = ResolveCastCooldown(args, record.level);
        record.expireTime = record.cooldown > 0.0f
            ? record.castTime + record.cooldown
            : 0.0f;
        strncpy_s(record.source, source ? source : "", _TRUNCATE);

        AcquireSRWLockExclusive(&m_recordLock);
        int index = -1;
        for (int i = 0; i < kMaxRecords; ++i) {
            if (m_records[i].active &&
                m_records[i].networkId == record.networkId &&
                m_records[i].slot == record.slot) {
                index = i;
                break;
            }
        }
        if (index < 0) {
            index = m_recordCursor++ % kMaxRecords;
        }
        m_records[index] = record;
        ReleaseSRWLockExclusive(&m_recordLock);
    }

    static void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
        if (s_instance) {
            s_instance->RecordCast(args, "P");
        }
    }

    static void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
        if (s_instance) {
            s_instance->RecordCast(args, "D");
        }
    }

    void ResetRecords() {
        AcquireSRWLockExclusive(&m_recordLock);
        for (auto& record : m_records) {
            record = {};
        }
        m_recordCursor = 0;
        ReleaseSRWLockExclusive(&m_recordLock);
    }

    static void DrawCenteredText(ImDrawList* draw,
                                 const ImVec2& min,
                                 const ImVec2& max,
                                 ImU32 color,
                                 const char* text) {
        if (!text || !text[0]) {
            return;
        }

        const ImVec2 size = ImGui::CalcTextSize(text);
        const ImVec2 pos(
            min.x + ((max.x - min.x) - size.x) * 0.5f,
            min.y + ((max.y - min.y) - size.y) * 0.5f);
        draw->AddText(pos, color, text);
    }

    void DrawSpellBox(ImDrawList* draw,
                      const ImVec2& pos,
                      const UnitSnapshot& unit,
                      int slot,
                      float gameTime,
                      int now) const {
        constexpr float boxW = 20.0f;
        constexpr float boxH = 20.0f;
        const ImVec2 min(pos.x, pos.y);
        const ImVec2 max(pos.x + boxW, pos.y + boxH);

        const SpellSnapshot& spell = unit.spells[slot];
        CastRecord recent{};
        const bool hasRecord =
            TryGetCastRecord(unit.networkId, slot, recent);
        const bool castRecent =
            hasRecord &&
            now >= recent.tick &&
            now - recent.tick <= kRecentCastMs;
        const float eventRemaining =
            hasRecord && recent.expireTime > gameTime
                ? recent.expireTime - gameTime
                : 0.0f;
        const float remaining =
            std::max(DisplayRemaining(unit, spell, gameTime), eventRemaining);
        SpellSnapshot display = spell;
        if (hasRecord) {
            display.valid = display.valid || recent.level > 0 || recent.cooldown > 0.0f;
            display.level = std::max(display.level, recent.level);
            display.cooldown = display.cooldown > 0.0f ? display.cooldown : recent.cooldown;
        }
        const ImU32 fill = SpellFillColor(display, remaining);
        const ImU32 textColor = SpellTextColor(display, remaining);

        draw->AddRectFilled(min, max, fill, 2.0f);
        draw->AddRect(
            min,
            max,
            castRecent ? IM_COL32(255, 255, 255, 255) : IM_COL32(10, 10, 10, 240),
            2.0f,
            0,
            castRecent ? 2.0f : 1.0f);

        char center[8] = {};
        if (!display.valid || display.level <= 0) {
            center[0] = '-';
            center[1] = 0;
        } else if (remaining > 0.05f) {
            FormatRemaining(remaining, center, sizeof(center));
        } else {
            strncpy_s(center, SlotLabel(slot), _TRUNCATE);
        }
        DrawCenteredText(draw, min, max, textColor, center);

        if (display.valid && display.level > 0) {
            char level[4] = {};
            std::snprintf(level, sizeof(level), "%d", display.level);
            draw->AddText(
                ImVec2(min.x + 1.5f, min.y - 1.0f),
                IM_COL32(255, 225, 120, 255),
                level);
        }
    }

    void DrawRightTracker(const UnitSnapshot* units,
                          int count,
                          float gameTime,
                          int now) const {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        if (display.x <= 0.0f || display.y <= 0.0f) {
            return;
        }

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        constexpr float rowW = 214.0f;
        constexpr float rowH = 38.0f;
        const float x = std::max(10.0f, display.x - rowW - 14.0f);
        float y = 88.0f;

        draw->AddRectFilled(
            ImVec2(x, y - 22.0f),
            ImVec2(x + rowW, y - 4.0f),
            IM_COL32(10, 12, 14, 175),
            3.0f);
        draw->AddText(
            ImVec2(x + 6.0f, y - 20.0f),
            IM_COL32(210, 225, 255, 255),
            "SPELL TRACKER");

        for (int i = 0; i < count; ++i) {
            const UnitSnapshot& unit = units[i];
            if (!ShouldShowUnit(unit)) {
                continue;
            }

            const ImU32 stripe = unit.player
                ? IM_COL32(80, 255, 125, 255)
                : (unit.enemy
                    ? IM_COL32(255, 75, 75, 255)
                    : IM_COL32(85, 170, 255, 255));
            draw->AddRectFilled(
                ImVec2(x, y),
                ImVec2(x + rowW, y + rowH),
                IM_COL32(8, 10, 12, 178),
                3.0f);
            draw->AddRectFilled(
                ImVec2(x, y),
                ImVec2(x + 3.0f, y + rowH),
                stripe,
                2.0f);

            char label[48] = {};
            std::snprintf(
                label,
                sizeof(label),
                "%s%d %.12s",
                UnitKind(unit),
                unit.level,
                unit.name);
            draw->AddText(
                ImVec2(x + 8.0f, y + 4.0f),
                IM_COL32(235, 235, 235, 255),
                label);

            const float boxesX = x + 78.0f;
            const float boxesY = y + 10.0f;
            for (int slot = 0; slot < kSlotCount; ++slot) {
                DrawSpellBox(
                    draw,
                    ImVec2(boxesX + static_cast<float>(slot) * 21.5f, boxesY),
                    unit,
                    slot,
                    gameTime,
                    now);
            }

            y += rowH + 5.0f;
        }
    }

    void DrawWorldCompact(const UnitSnapshot* units,
                          int count,
                          float gameTime,
                          int now) const {
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        for (int i = 0; i < count; ++i) {
            const UnitSnapshot& unit = units[i];
            if (!ShouldShowUnit(unit)) {
                continue;
            }

            Vec2 screen{};
            if (!SDK::Drawing::WorldToScreen(unit.position, screen) ||
                !SDK::Drawing::OnScreen(screen)) {
                continue;
            }

            char line[128] = {};
            int used = std::snprintf(
                line,
                sizeof(line),
                "%s ",
                UnitKind(unit));
            for (int slot = 0; slot < kSlotCount && used > 0 && used < static_cast<int>(sizeof(line)); ++slot) {
                const SpellSnapshot& spell = unit.spells[slot];
                CastRecord recent{};
                const bool hasRecord =
                    TryGetCastRecord(unit.networkId, slot, recent);
                const bool castRecent =
                    hasRecord &&
                    now >= recent.tick &&
                    now - recent.tick <= kRecentCastMs;
                const float eventRemaining =
                    hasRecord && recent.expireTime > gameTime
                        ? recent.expireTime - gameTime
                        : 0.0f;
                const float remaining =
                    std::max(DisplayRemaining(unit, spell, gameTime), eventRemaining);
                const bool valid =
                    spell.valid || (hasRecord && (recent.level > 0 || recent.cooldown > 0.0f));
                const int level = std::max(spell.level, hasRecord ? recent.level : 0);

                char value[12] = {};
                if (!valid || level <= 0) {
                    std::snprintf(value, sizeof(value), "%s-", SlotLabel(slot));
                } else if (remaining > 0.05f) {
                    char cd[8] = {};
                    FormatRemaining(remaining, cd, sizeof(cd));
                    std::snprintf(value, sizeof(value), "%s%s", SlotLabel(slot), cd);
                } else {
                    std::snprintf(value, sizeof(value), "%sR", SlotLabel(slot));
                }
                if (castRecent) {
                    const int len = static_cast<int>(std::strlen(value));
                    if (len + 1 < static_cast<int>(sizeof(value))) {
                        value[len] = '*';
                        value[len + 1] = 0;
                    }
                }

                used += std::snprintf(
                    line + used,
                    sizeof(line) - static_cast<std::size_t>(used),
                    "%s ",
                    value);
            }

            const ImU32 color = unit.player
                ? IM_COL32(120, 255, 145, 245)
                : (unit.enemy
                    ? IM_COL32(255, 120, 120, 245)
                    : IM_COL32(120, 180, 255, 245));
            draw->AddText(
                ImVec2(screen.x + 18.0f, screen.y + 18.0f),
                color,
                line);
        }
    }

    void DrawDebugWindow(const UnitSnapshot* units,
                         int count,
                         float gameTime,
                         int now) const {
        ImGui::SetNextWindowBgAlpha(0.82f);
        ImGui::SetNextWindowSize(ImVec2(720.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("NightSharp Spell Tracking Debug")) {
            ImGui::End();
            return;
        }

        ImGui::Text("Game %.2f  Tick %d  Refresh %dms", gameTime, now, m_refreshMs);
        if (ImGui::BeginTable(
                "SpellTrackingCacheTable",
                8,
                ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Unit");
            for (int slot = 0; slot < kSlotCount; ++slot) {
                ImGui::TableSetupColumn(SlotLabel(slot));
            }
            ImGui::TableSetupColumn("Ptr");
            ImGui::TableHeadersRow();

            for (int i = 0; i < count; ++i) {
                const UnitSnapshot& unit = units[i];
                if (!ShouldShowUnit(unit)) {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s%d %s", UnitKind(unit), unit.level, unit.name);
                for (int slot = 0; slot < kSlotCount; ++slot) {
                    ImGui::TableSetColumnIndex(slot + 1);
                    const SpellSnapshot& spell = unit.spells[slot];
                    CastRecord recent{};
                    const bool hasRecord =
                        TryGetCastRecord(unit.networkId, slot, recent);
                    const float eventRemaining =
                        hasRecord && recent.expireTime > gameTime
                            ? recent.expireTime - gameTime
                            : 0.0f;
                    const float remaining =
                        std::max(DisplayRemaining(unit, spell, gameTime), eventRemaining);
                    const bool valid =
                        spell.valid || (hasRecord && (recent.level > 0 || recent.cooldown > 0.0f));
                    const int level = std::max(spell.level, hasRecord ? recent.level : 0);
                    const float cooldown = spell.cooldown > 0.0f
                        ? spell.cooldown
                        : (hasRecord ? recent.cooldown : 0.0f);
                    if (!valid) {
                        ImGui::TextUnformatted("--");
                    } else {
                        ImGui::Text("L%d %.1f/%.1f", level, remaining, cooldown);
                    }
                }
                ImGui::TableSetColumnIndex(7);
                ImGui::Text("0x%llX", static_cast<unsigned long long>(unit.address));
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }
};

} // namespace Plugins
