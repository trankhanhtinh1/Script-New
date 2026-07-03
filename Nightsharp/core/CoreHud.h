#pragma once

#include "CoreRuntime.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"
#include "../DebugLog.h"

#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace CoreHud {

enum class PingResourceType : int {
    HP = 1,
    PAR = 2,
    XP = 3,
};

enum class PingStatType : int {
    AbilityPower = 0,
    Armor = 1,
    AttackDamage = 2,
    AttackSpeed = 3,
    MagicResist = 4,
    MoveSpeed = 5,
    CriticalStrike = 6,
    AbilityHaste = 7,
    LifeSteal = 8,
    SpellVamp = 9,
    MagicFlatPenetration = 10,
    ArmorFlatPenetration = 11,
    Tenacity = 12,
    AttackRange = 13,
    HealthRegen = 14,
    ParRegen = 15,
    CriticalStrikeDamageMultiplier = 16,
    Dodge = 17,
};

enum class ClickType : int {
    Move = 0,
    Attack = 1,
};

struct HudSnapshot {
    uintptr_t hud = 0;
    uintptr_t hudRoot = 0;
    uintptr_t input = 0;
    uintptr_t selectLogic = 0;
    uintptr_t spellLogic = 0;
    uintptr_t camera = 0;
    uintptr_t zoomConfig = 0;
    Vec3 mouseWorldPos = {};
    std::uint32_t selectedNetworkId = 0;
    std::uint32_t selectedObjectIndex = 0;
    bool targetChampionsOnly = false;
    bool targetChampionsOnlyValid = false;
    bool cameraLocked = false;
    float currentZoom = 0.0f;
    float minZoom = 0.0f;
    float maxZoom = 0.0f;

    bool IsValid() const {
        return Globals::IsValidPtr(hud);
    }
};

namespace detail {
    inline bool LogOnce(const char* key, const char* message) {
        struct Entry {
            const char* key = nullptr;
            bool logged = false;
        };
        static Entry entries[16] = {};

        for (auto& entry : entries) {
            if (entry.key == key || (entry.key && key && std::strcmp(entry.key, key) == 0)) {
                if (!entry.logged) {
                    NightSharpDebug::Logf("%s", message);
                    entry.logged = true;
                    return true;
                }
                return false;
            }
        }

        for (auto& entry : entries) {
            if (!entry.key) {
                entry.key = key;
                entry.logged = true;
                NightSharpDebug::Logf("%s", message);
                return true;
            }
        }
        return false;
    }

    inline bool IsSaneZoom(float value) {
        return value >= 100.0f && value <= 10000.0f;
    }

    inline bool IsSaneBoolByte(std::uint8_t value) {
        return value <= 1;
    }
} // namespace detail

inline uintptr_t HudRoot() {
    const auto& ctx = CoreRuntime::GetContext();
    if (!ctx.moduleBase) {
        (void)CoreRuntime::RefreshReadState();
    }
    const uintptr_t base = CoreRuntime::GetContext().moduleBase;
    if (!base) {
        return 0;
    }
    return Globals::Read<uintptr_t>(base + Offset::DrawingRuntime::HudRoot);
}

inline uintptr_t HudInstance() {
    auto& ctx = CoreRuntime::g_ctx;
    if (!Globals::IsValidPtr(ctx.hudInstance)) {
        (void)CoreRuntime::RefreshReadState();
    }
    return CoreRuntime::GetContext().hudInstance;
}

inline uintptr_t ScoreboardViewController() {
    auto& ctx = CoreRuntime::g_ctx;
    if (!Globals::IsValidPtr(ctx.viewPort)) {
        (void)CoreRuntime::RefreshReadState();
    }

    const uintptr_t root = CoreRuntime::GetContext().viewPort;
    if (!Globals::IsValidPtr(root)) {
        return 0;
    }

    const uintptr_t controller = Globals::Read<uintptr_t>(
        root + Offset::DrawingRuntime::ScoreboardViewController);
    return Globals::IsValidPtr(controller) ? controller : 0;
}

inline uintptr_t Input() {
    const uintptr_t hud = HudInstance();
    return Globals::IsValidPtr(hud)
        ? Globals::Read<uintptr_t>(hud + Offset::HudRuntime::Input)
        : 0;
}

inline uintptr_t CursorTargetLogic() {
    const uintptr_t hud = HudInstance();
    return Globals::IsValidPtr(hud)
        ? Globals::Read<uintptr_t>(hud + Offset::HudRuntime::CursorTargetLogic)
        : 0;
}

inline uintptr_t SelectLogic() {
    const uintptr_t hud = HudInstance();
    if (!Globals::IsValidPtr(hud)) {
        return 0;
    }

    // This is the object used by both selected-target diagnostics and the
    // evtChampionOnly toggle, despite older code naming the field SpellTargeting.
    return Globals::Read<uintptr_t>(hud + Offset::HudRuntime::SpellTargeting);
}

inline uintptr_t SpellLogic() {
    const uintptr_t hud = HudInstance();
    return Globals::IsValidPtr(hud)
        ? Globals::Read<uintptr_t>(hud + Offset::HudRuntime::SpellInfo)
        : 0;
}

inline uintptr_t Camera() {
    const uintptr_t hud = HudInstance();
    if (Globals::IsValidPtr(hud)) {
        const uintptr_t candidate = Globals::Read<uintptr_t>(hud + Offset::HudRuntime::Camera);
        if (Globals::IsValidPtr(candidate)) {
            return candidate;
        }
    }

    const auto& ctx = CoreRuntime::GetContext();
    const uintptr_t base = ctx.moduleBase ? ctx.moduleBase : Globals::base;
    if (base) {
        const uintptr_t candidate =
            Globals::Read<uintptr_t>(base + Offset::ZoomRuntime::CameraInstance);
        if (Globals::IsValidPtr(candidate)) {
            return candidate;
        }
    }
    return 0;
}

inline uintptr_t ZoomConfig() {
    const uintptr_t camera = Camera();
    if (!Globals::IsValidPtr(camera)) {
        return 0;
    }

    uintptr_t config = Globals::Read<uintptr_t>(camera + Offset::ZoomRuntime::ZoomConfigPtr);
    if (Globals::IsValidPtr(config)) {
        return config;
    }

    config = Globals::Read<uintptr_t>(camera + Offset::ZoomRuntime::ZoomFallbackPtr);
    return Globals::IsValidPtr(config) ? config : 0;
}

inline Vec3 MouseWorldPosition() {
    const uintptr_t input = Input();
    return Globals::IsValidPtr(input)
        ? Globals::Read<Vec3>(input + Offset::HudRuntime::MouseWorldPos)
        : Vec3{};
}

inline std::uint32_t SelectedNetworkId() {
    const uintptr_t input = Input();
    return Globals::IsValidPtr(input)
        ? Globals::Read<std::uint32_t>(input + Offset::HudInputLayout::SelectedObjNetId)
        : 0;
}

inline std::uint32_t SelectedObjectIndex() {
    const uintptr_t select = SelectLogic();
    return Globals::IsValidPtr(select)
        ? Globals::Read<std::uint32_t>(select + Offset::HudSpellTargetingLayout::ObjectIndex)
        : 0;
}

inline std::uint32_t TargetingState() {
    const uintptr_t select = SelectLogic();
    return Globals::IsValidPtr(select)
        ? Globals::Read<std::uint32_t>(select + Offset::HudSpellTargetingLayout::State)
        : 0;
}

inline bool TargetChampionsOnly() {
    const uintptr_t select = SelectLogic();
    if (!Globals::IsValidPtr(select)) {
        return false;
    }

    const auto value = Globals::Read<std::uint8_t>(
        select + Offset::HudSpellTargetingLayout::TargetChampionsOnly);
    return detail::IsSaneBoolByte(value) && value != 0;
}

inline bool SetTargetChampionsOnly(bool enabled) {
    const uintptr_t select = SelectLogic();
    if (!Globals::IsValidPtr(select)) {
        return false;
    }

    return Globals::Write<std::uint8_t>(
        select + Offset::HudSpellTargetingLayout::TargetChampionsOnly,
        static_cast<std::uint8_t>(enabled ? 1 : 0));
}

inline bool IsCameraLocked() {
    const uintptr_t camera = Camera();
    if (!Globals::IsValidPtr(camera)) {
        return false;
    }

    const auto flag1 = Globals::Read<std::uint8_t>(camera + Offset::HudRuntime::ZoomLockFlag1);
    const auto flag2 = Globals::Read<std::uint8_t>(camera + Offset::HudRuntime::ZoomLockFlag2);
    return (flag1 != 0) || (flag2 != 0);
}

inline bool SetCameraLockState(bool locked) {
    const uintptr_t camera = Camera();
    if (!Globals::IsValidPtr(camera)) {
        detail::LogOnce(
            "camera-lock-native-missing",
            "[CoreHud] ClientCameraPositionClient.SetLockedInternal instance is not resolved.");
        return false;
    }

    const std::uint8_t value = static_cast<std::uint8_t>(locked ? 1 : 0);
    bool ok = Globals::Write<std::uint8_t>(camera + Offset::HudRuntime::ZoomLockFlag1, value);
    ok = Globals::Write<std::uint8_t>(camera + Offset::HudRuntime::ZoomLockFlag2, value) && ok;
    return ok;
}

inline float CurrentZoom() {
    const uintptr_t camera = Camera();
    if (!Globals::IsValidPtr(camera)) {
        return 0.0f;
    }

    const float value = Globals::Read<float>(camera + Offset::ZoomRuntime::CurrentZoom);
    return detail::IsSaneZoom(value) ? value : 0.0f;
}

inline bool SetCurrentZoom(float value) {
    if (!detail::IsSaneZoom(value)) {
        return false;
    }

    const uintptr_t camera = Camera();
    return Globals::IsValidPtr(camera) &&
           Globals::Write<float>(camera + Offset::ZoomRuntime::CurrentZoom, value);
}

inline float MinZoom() {
    const uintptr_t config = ZoomConfig();
    if (!Globals::IsValidPtr(config)) {
        return 0.0f;
    }

    const float value = Globals::Read<float>(config + Offset::ZoomRuntime::ZC_MinZoom);
    return detail::IsSaneZoom(value) ? value : 0.0f;
}

inline bool SetMinZoom(float value) {
    if (!detail::IsSaneZoom(value)) {
        return false;
    }

    const uintptr_t config = ZoomConfig();
    return Globals::IsValidPtr(config) &&
           Globals::Write<float>(config + Offset::ZoomRuntime::ZC_MinZoom, value);
}

inline float MaxZoom() {
    const uintptr_t config = ZoomConfig();
    if (!Globals::IsValidPtr(config)) {
        return 0.0f;
    }

    const float value = Globals::Read<float>(config + Offset::ZoomRuntime::ZC_MaxZoom);
    return detail::IsSaneZoom(value) ? value : 0.0f;
}

inline bool SetMaxZoom(float value) {
    if (!detail::IsSaneZoom(value)) {
        return false;
    }

    const uintptr_t config = ZoomConfig();
    return Globals::IsValidPtr(config) &&
           Globals::Write<float>(config + Offset::ZoomRuntime::ZC_MaxZoom, value);
}

inline uintptr_t SelectedSpellData() {
    detail::LogOnce(
        "selected-spell-native-missing",
        "[CoreHud] HudSpellLogic.GetSpell offset is not verified; Hud.SelectedSpell returns null.");
    return 0;
}

struct DragonKillStats {
    int allyDragonKills = 0;
    int allyElderDragonKills = 0;
    int allyRiftHeraldKills = 0;
    int enemyDragonKills = 0;
    int enemyElderDragonKills = 0;
    int enemyRiftHeraldKills = 0;
};

namespace detail {
    inline uintptr_t FindStatBlock(uintptr_t treeBase, std::uint32_t teamId) {
        const uintptr_t sentinel = Globals::Read<uintptr_t>(treeBase);
        if (!Globals::IsValidPtr(sentinel)) return 0;

        const uintptr_t root = Globals::Read<uintptr_t>(
            sentinel + Offset::StatsRuntime::SentinelParent);
        if (!Globals::IsValidPtr(root)) return 0;

        uintptr_t node = root;
        for (int i = 0; i < 64 && Globals::IsValidPtr(node); ++i) {
            const auto isNil = Globals::Read<std::uint8_t>(
                node + Offset::StatsRuntime::NodeIsNil);
            if (isNil) break;

            const auto key = Globals::Read<std::uint32_t>(
                node + Offset::StatsRuntime::NodeKey);
            if (key == teamId) {
                const uintptr_t begin = Globals::Read<uintptr_t>(
                    node + Offset::StatsRuntime::NodeValueBegin);
                if (!Globals::IsValidPtr(begin)) return 0;
                return Globals::Read<uintptr_t>(begin);
            }
            const uintptr_t next = Globals::Read<uintptr_t>(
                node + (key < teamId ? 0x08 : 0x00));
            node = next;
        }
        return 0;
    }

    inline void ReadDragonKills(uintptr_t statBlock, DragonKillStats& out, bool ally) {
        if (!Globals::IsValidPtr(statBlock)) return;
        const int dragon = Globals::Read<int>(statBlock + Offset::StatsRuntime::DragonKills);
        const int elder = Globals::Read<int>(statBlock + Offset::StatsRuntime::ElderDragonKills);
        const int herald = Globals::Read<int>(statBlock + Offset::StatsRuntime::RiftHeraldKills);
        if (ally) {
            out.allyDragonKills = dragon;
            out.allyElderDragonKills = elder;
            out.allyRiftHeraldKills = herald;
        } else {
            out.enemyDragonKills = dragon;
            out.enemyElderDragonKills = elder;
            out.enemyRiftHeraldKills = herald;
        }
    }
} // namespace detail

inline DragonKillStats DragonTracker() {
    DragonKillStats result{};

    const auto& ctx = CoreRuntime::GetContext();
    const uintptr_t base = ctx.moduleBase ? ctx.moduleBase : Globals::base;
    if (!base) return result;

    const uintptr_t statsManager = Globals::Read<uintptr_t>(
        base + Offset::DrawingRuntime::StatsManager);
    if (!Globals::IsValidPtr(statsManager)) return result;

    const uintptr_t treeBase = statsManager + Offset::StatsRuntime::TreeOffset;

    const uintptr_t player = ctx.localPlayer;
    if (!Globals::IsValidPtr(player)) return result;

    const std::uint8_t teamSlot = Globals::Read<std::uint8_t>(
        player + Offset::All::Team);
    const uintptr_t teamTable = Globals::Read<uintptr_t>(
        base + Offset::GameObjectsRuntime::TeamTable);
    std::uint32_t allyTeam = teamSlot;
    if (Globals::IsValidPtr(teamTable)) {
        const auto val = Globals::Read<std::uint32_t>(
            teamTable + 0x20 + static_cast<uintptr_t>(teamSlot) * 4);
        if (val != 0) allyTeam = val;
    }
    const std::uint32_t enemyTeam = (allyTeam == 100) ? 200 : 100;

    const uintptr_t allyBlock = detail::FindStatBlock(treeBase, allyTeam);
    const uintptr_t enemyBlock = detail::FindStatBlock(treeBase, enemyTeam);

    detail::ReadDragonKills(allyBlock, result, true);
    detail::ReadDragonKills(enemyBlock, result, false);
    return result;
}

inline bool PingBar(PingResourceType /*type*/) {
    detail::LogOnce(
        "pingbar-native-missing",
        "[CoreHud] HudPlayerResourceBars.OnPing offset is not verified; Hud.PingBar is disabled.");
    return false;
}

inline bool PingSpell(uintptr_t /*target*/, int /*slot*/) {
    detail::LogOnce(
        "pingspell-native-missing",
        "[CoreHud] HudSpellLogic.HandleSpellPingClick offset is not verified; Hud.PingSpell is disabled.");
    return false;
}

inline bool PingStat(uintptr_t /*target*/, PingStatType /*type*/) {
    detail::LogOnce(
        "pingstat-native-missing",
        "[CoreHud] HudUnitStats.OnPing offset is not verified; Hud.PingStat is disabled.");
    return false;
}

inline bool ShowClick(ClickType type, const Vec3& position) {
    static_assert(std::is_trivially_copyable_v<Vec3>);

    const int index = static_cast<int>(type);
    if (index < 0 || index >= 5 || !position.IsValid()) {
        return false;
    }

    const uintptr_t cursor = CursorTargetLogic();
    if (!Globals::IsValidPtr(cursor) ||
        !Globals::Write<Vec3>(cursor + Offset::HudCursorTargetLogicLayout::ClickPosition, position)) {
        return false;
    }

    const auto& ctx = CoreRuntime::GetContext();
    const uintptr_t fn = ctx.moduleBase
        ? (ctx.moduleBase + Offset::HudCursorTargetLogicRuntime::ShowClickEffect)
        : 0;
    if (!Globals::IsExecutablePtr(fn)) {
        detail::LogOnce(
            "showclick-native-missing",
            "[CoreHud] HudCursorTargetLogic click effect function is not executable; Hud.ShowClick is disabled.");
        return false;
    }

    using ShowClickEffectFn = void(__fastcall*)(uintptr_t, std::uint8_t);
    __try {
        reinterpret_cast<ShowClickEffectFn>(fn)(cursor, static_cast<std::uint8_t>(index));
        return true;
    }
    __except (1) {
        return false;
    }
}

inline HudSnapshot Snapshot() {
    HudSnapshot out{};
    out.hud = HudInstance();
    out.hudRoot = HudRoot();
    out.input = Input();
    out.selectLogic = SelectLogic();
    out.spellLogic = SpellLogic();
    out.camera = Camera();
    out.zoomConfig = ZoomConfig();
    out.mouseWorldPos = MouseWorldPosition();
    out.selectedNetworkId = SelectedNetworkId();
    out.selectedObjectIndex = SelectedObjectIndex();
    out.currentZoom = CurrentZoom();
    out.minZoom = MinZoom();
    out.maxZoom = MaxZoom();
    out.cameraLocked = IsCameraLocked();

    if (Globals::IsValidPtr(out.selectLogic)) {
        const auto value = Globals::Read<std::uint8_t>(
            out.selectLogic + Offset::HudSpellTargetingLayout::TargetChampionsOnly);
        out.targetChampionsOnlyValid = detail::IsSaneBoolByte(value);
        out.targetChampionsOnly = out.targetChampionsOnlyValid && value != 0;
    }
    return out;
}

} // namespace CoreHud
