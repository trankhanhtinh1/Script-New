#pragma once
#include "../../../SDK/SDK.h"
#include "../../../imgui/imgui.h"
#include <Windows.h>
#include <cstdint>

namespace Plugins {
    class DeveloperToolsPlugin;
}

namespace Plugins::DevTools {

struct SnapshotBuff {
    char name[96] = {};
    int count = 0;
    int stacks = 0;
    int type = -1;
    float startTime = 0.0f;
    float endTime = 0.0f;
    uintptr_t address = 0;
    bool live = false;
};

struct SnapshotSpell {
    SDK::SpellSlot slot;
    char name[64] = {};
    int level = 0;
    int ammo = 0;
    int maxAmmo = 0;
    float cooldown = 0.0f;
    float remainingCooldown = 0.0f;
    float manaCost = 0.0f;
    std::uint32_t state = 0;
};

struct EventLogEntry {
    int tick = 0;
    float time = 0.0f;
    std::string eventName;
    std::string details;
};

struct ObjectSnapshot {
    std::uint32_t networkId = 0;
    uintptr_t address = 0;
    std::string name;
    std::string characterName;
    ::Core::Objects::ObjectType type = ::Core::Objects::ObjectType::Unknown;
    SDK::GameObjectTeam team = SDK::GameObjectTeam::Unknown;
    Vec3 position;
    float health = 0.0f;
    float maxHealth = 0.0f;
    float mana = 0.0f;
    float maxMana = 0.0f;
    int snapshotTick = 0;
    bool isUnderlyingValid = true;
    std::string note;

    // Object stats snapshot
    float armor = 0.0f;
    float spellBlock = 0.0f;
    float attackDamage = 0.0f;
    float baseAD = 0.0f;
    float bonusAD = 0.0f;
    float abilityPower = 0.0f;
    float attackRange = 0.0f;
    float moveSpeed = 0.0f;
    float attackSpeedMod = 0.0f;
    float crit = 0.0f;
    float bonusArmor = 0.0f;
    float bonusSpellBlock = 0.0f;
    float lethality = 0.0f;
    float flatArmorPen = 0.0f;
    float percentArmorPen = 0.0f;
    float flatMagicPen = 0.0f;
    float percentMagicPen = 0.0f;
    float allShield = 0.0f;
    float physShield = 0.0f;
    float magShield = 0.0f;
    float healthRegen = 0.0f;
    int level = 0;

    std::vector<SnapshotBuff> buffs;
    std::vector<SnapshotSpell> spells;
};

class IDeveloperTab {
protected:
    DeveloperToolsPlugin* plugin_ = nullptr;

public:
    explicit IDeveloperTab(DeveloperToolsPlugin* plugin) : plugin_(plugin) {}
    virtual ~IDeveloperTab() = default;

    virtual const char* GetTabName() const = 0;
    virtual void OnLoad() {}
    virtual void OnUnload() {}
    virtual void OnUpdate() {}
    virtual void OnRender() {}
    virtual void OnDrawTab() = 0;
    virtual void OnCopyHotkey() {}

    // Optional event delegates:
    virtual void OnProcessSpellCast(const SDK::Events::ProcessSpellEventArgs&) {}
    virtual void OnDoCastEvent(const SDK::Events::ProcessSpellEventArgs&) {}
    virtual void OnFinishCastEvent(const SDK::Events::ProcessSpellEventArgs&) {}
    virtual void OnSpellImpactEvent(const SDK::Events::ProcessSpellEventArgs&) {}
    virtual void OnCastSpellEvent(const SDK::Events::CastSpellEventArgs&) {}
    virtual void OnStopCastEvent(const SDK::Events::StopCastEventArgs&) {}
    virtual void OnPlayAnimationEvent(const SDK::Events::PlayAnimationEventArgs&) {}
    virtual void OnBuffAddEvent(const SDK::Events::BuffEventArgs&) {}
    virtual void OnBuffRemoveEvent(const SDK::Events::BuffEventArgs&) {}
    virtual void OnBuffUpdateEvent(const SDK::Events::BuffEventArgs&) {}
    virtual void OnNewPathEvent(const SDK::Events::NewPathEventArgs&) {}
    virtual void OnDeleteObject(const SDK::Events::ObjectEventArgs&) {}
};

// Keep SEH in tiny leaf functions.  The tab implementations use STL/RAII and
// therefore cannot safely place __try in their own bodies on MSVC (C2712).
// A bad/stale game object is allowed to fault one tab, but must never escape to
// PluginManager and disable the whole Developer Tools plugin.
struct TabFaultCapture {
    std::uint32_t code = 0;
    std::uintptr_t address = 0;
};

inline int CaptureTabException(EXCEPTION_POINTERS* exception,
                               TabFaultCapture* capture) noexcept {
    if (capture) {
        capture->code = exception && exception->ExceptionRecord
            ? static_cast<std::uint32_t>(exception->ExceptionRecord->ExceptionCode)
            : 0;
        capture->address = exception && exception->ExceptionRecord
            ? reinterpret_cast<std::uintptr_t>(exception->ExceptionRecord->ExceptionAddress)
            : 0;
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

inline bool InvokeTabLoadGuarded(IDeveloperTab* tab,
                                 TabFaultCapture* capture) noexcept {
    if (!tab) return false;
    __try {
        tab->OnLoad();
        return true;
    } __except (CaptureTabException(GetExceptionInformation(), capture)) {
        return false;
    }
}

inline bool InvokeTabUnloadGuarded(IDeveloperTab* tab,
                                   TabFaultCapture* capture) noexcept {
    if (!tab) return false;
    __try {
        tab->OnUnload();
        return true;
    } __except (CaptureTabException(GetExceptionInformation(), capture)) {
        return false;
    }
}

inline bool InvokeTabUpdateGuarded(IDeveloperTab* tab,
                                   TabFaultCapture* capture) noexcept {
    if (!tab) return false;
    __try {
        tab->OnUpdate();
        return true;
    } __except (CaptureTabException(GetExceptionInformation(), capture)) {
        return false;
    }
}

inline bool InvokeTabRenderGuarded(IDeveloperTab* tab,
                                   TabFaultCapture* capture) noexcept {
    if (!tab) return false;
    __try {
        tab->OnRender();
        return true;
    } __except (CaptureTabException(GetExceptionInformation(), capture)) {
        return false;
    }
}

inline bool InvokeTabDrawGuarded(IDeveloperTab* tab,
                                 TabFaultCapture* capture) noexcept {
    if (!tab) return false;
    __try {
        tab->OnDrawTab();
        return true;
    } __except (CaptureTabException(GetExceptionInformation(), capture)) {
        return false;
    }
}

inline bool InvokeTabCopyGuarded(IDeveloperTab* tab,
                                 TabFaultCapture* capture) noexcept {
    if (!tab) return false;
    __try {
        tab->OnCopyHotkey();
        return true;
    } __except (CaptureTabException(GetExceptionInformation(), capture)) {
        return false;
    }
}

template <typename TArgs>
inline bool InvokeTabEventGuarded(
    IDeveloperTab* tab,
    void (IDeveloperTab::*callback)(const TArgs&),
    const TArgs& args,
    TabFaultCapture* capture) noexcept {
    if (!tab || !callback) return false;
    __try {
        (tab->*callback)(args);
        return true;
    } __except (CaptureTabException(GetExceptionInformation(), capture)) {
        return false;
    }
}

} // namespace Plugins::DevTools
