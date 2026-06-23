#pragma once

#include "../../Core/CoreHud.h"
#include "../Enumerations/SpellSlot.h"
#include "Objects.h"

#include <cstdint>

namespace SDK::Hud {

using PingResourceType = ::CoreHud::PingResourceType;
using PingStatType = ::CoreHud::PingStatType;
using ClickType = ::CoreHud::ClickType;
using Snapshot = ::CoreHud::HudSnapshot;

struct SelectedSpellInfo {
    uintptr_t Address = 0;

    bool IsValid() const {
        return Globals::IsValidPtr(Address);
    }
};

struct DragonSRXInfo {
    uintptr_t Address = 0;

    bool IsValid() const {
        return Globals::IsValidPtr(Address);
    }
};

inline uintptr_t Address() {
    return ::CoreHud::HudInstance();
}

inline uintptr_t HudRoot() {
    return ::CoreHud::HudRoot();
}

inline uintptr_t InputAddress() {
    return ::CoreHud::Input();
}

inline uintptr_t SelectLogicAddress() {
    return ::CoreHud::SelectLogic();
}

inline uintptr_t SpellLogicAddress() {
    return ::CoreHud::SpellLogic();
}

inline uintptr_t CameraAddress() {
    return ::CoreHud::Camera();
}

inline bool TargetChampionsOnly() {
    return ::CoreHud::TargetChampionsOnly();
}

inline bool SetTargetChampionsOnly(bool enabled) {
    return ::CoreHud::SetTargetChampionsOnly(enabled);
}

inline bool TargetChampionsOnly(bool enabled) {
    return SetTargetChampionsOnly(enabled);
}

inline SelectedSpellInfo SelectedSpell() {
    return { ::CoreHud::SelectedSpellData() };
}

inline DragonSRXInfo DragonSRX() {
    return { ::CoreHud::DragonTracker() };
}

inline Vector3 MouseWorldPosition() {
    return ::CoreHud::MouseWorldPosition();
}

inline std::uint32_t SelectedNetworkId() {
    return ::CoreHud::SelectedNetworkId();
}

inline std::uint32_t SelectedObjectIndex() {
    return ::CoreHud::SelectedObjectIndex();
}

inline std::uint32_t TargetingState() {
    return ::CoreHud::TargetingState();
}

inline bool IsCameraLocked() {
    return ::CoreHud::IsCameraLocked();
}

inline bool SetCameraLockState(bool locked) {
    return ::CoreHud::SetCameraLockState(locked);
}

inline float CurrentZoom() {
    return ::CoreHud::CurrentZoom();
}

inline bool SetCurrentZoom(float value) {
    return ::CoreHud::SetCurrentZoom(value);
}

inline float MinZoom() {
    return ::CoreHud::MinZoom();
}

inline bool SetMinZoom(float value) {
    return ::CoreHud::SetMinZoom(value);
}

inline float MaxZoom() {
    return ::CoreHud::MaxZoom();
}

inline bool SetMaxZoom(float value) {
    return ::CoreHud::SetMaxZoom(value);
}

inline bool PingBar(PingResourceType type) {
    return ::CoreHud::PingBar(type);
}

inline bool PingSpell(const AIHeroClient& target, SpellSlot slot) {
    return target.IsValid() &&
           ::CoreHud::PingSpell(target.Address(), static_cast<int>(slot));
}

inline bool PingStat(const AIHeroClient& target, PingStatType type) {
    return target.IsValid() && ::CoreHud::PingStat(target.Address(), type);
}

inline bool ShowClick(ClickType type, const Vector3& position) {
    return ::CoreHud::ShowClick(type, position);
}

inline Snapshot GetSnapshot() {
    return ::CoreHud::Snapshot();
}

} // namespace SDK::Hud

namespace SDK::Core {
namespace Hud = ::SDK::Hud;
}
