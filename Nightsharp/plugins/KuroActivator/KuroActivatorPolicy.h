#pragma once

#include "../../SDK/Enums/BuffType.h"
#include "../../SDK/Enumerations/DamageType.h"
#include "../../SDK/Enumerations/ItemId.h"
#include "../../SDK/Enumerations/OrbwalkingMode.h"

#include <array>
#include <cstdint>

namespace Plugins::KuroActivator {

struct CcTrackedType {
    SDK::BuffType type = SDK::BuffType::Internal;
    const char* name = nullptr;
    bool standardCleanseRemovable = true;
    bool enabledByDefault = true;
    int priority = 0;
};

// QSS xoá mọi loại dưới đây. Cleanse/Mikael không thể xoá Suppression; Slow
// và AttackSpeedSlow vẫn có menu nhưng tắt mặc định để tránh phí active.
inline constexpr std::array<CcTrackedType, 18> kCcTrackedTypes = {{
    { SDK::BuffType::Stun, "Stunned", true, true, 10 },
    { SDK::BuffType::Silence, "Silenced", true, true, 7 },
    { SDK::BuffType::Taunt, "Taunted", true, true, 10 },
    { SDK::BuffType::Berserk, "Berserk", true, true, 10 },
    { SDK::BuffType::Polymorph, "Polymorphed", true, true, 10 },
    { SDK::BuffType::Slow, "Slowed", true, false, 2 },
    { SDK::BuffType::Snare, "Snared", true, true, 8 },
    { SDK::BuffType::AttackSpeedSlow, "Attack speed slowed", true, false, 2 },
    { SDK::BuffType::NearSight, "Nearsighted", true, true, 5 },
    { SDK::BuffType::Fear, "Feared", true, true, 10 },
    { SDK::BuffType::Charm, "Charmed", true, true, 10 },
    { SDK::BuffType::Suppression, "Suppressed", false, true, 10 },
    { SDK::BuffType::Blind, "Blinded", true, true, 5 },
    { SDK::BuffType::Flee, "Fleeing", true, true, 10 },
    { SDK::BuffType::Disarm, "Disarmed", true, true, 7 },
    { SDK::BuffType::Grounded, "Grounded", true, true, 5 },
    { SDK::BuffType::Drowsy, "Drowsy", true, true, 6 },
    { SDK::BuffType::Asleep, "Asleep", true, true, 10 },
}};

inline constexpr bool IsQssTrackedCc(int type) noexcept {
    for (const auto& cc : kCcTrackedTypes) {
        if (static_cast<int>(cc.type) == type) return true;
    }
    return false;
}

inline constexpr bool IsStandardCleanseCompatibleCc(int type) noexcept {
    for (const auto& cc : kCcTrackedTypes) {
        if (static_cast<int>(cc.type) == type) {
            return cc.standardCleanseRemovable;
        }
    }
    return false;
}

inline constexpr bool IsQssOrbwalkerModeAllowed(
    SDK::OrbwalkingMode mode) noexcept {
    return mode != SDK::OrbwalkingMode::None;
}

enum class CleanseItemTarget : std::uint8_t {
    Self,
    Ally,
};

struct CleanseItemDefinition {
    const char* menuKey = nullptr;
    const char* name = nullptr;
    std::array<int, 3> ids{};
    CleanseItemTarget target = CleanseItemTarget::Self;
};

inline constexpr std::array<CleanseItemDefinition, 4> kCleanseItems = {{
    { "QuicksilverSash", "Quicksilver Sash",
      { SDK::ItemIdValue(SDK::ItemId::Quicksilver_Sash), 0, 0 },
      CleanseItemTarget::Self },
    { "SilvermereDawn", "Silvermere Dawn",
      { SDK::ItemIdValue(SDK::ItemId::Silvermere_Dawn),
        SDK::ItemIdValue(SDK::ItemId::Silvermere_Dawn_Id226035), 0 },
      CleanseItemTarget::Self },
    { "MercurialScimitar", "Mercurial Scimitar",
      { SDK::ItemIdValue(SDK::ItemId::Mercurial_Scimitar),
        SDK::ItemIdValue(SDK::ItemId::Mercurial_Scimitar_Id223139), 0 },
      CleanseItemTarget::Self },
    { "MikaelsBlessing", "Mikael's Blessing",
      { SDK::ItemIdValue(SDK::ItemId::Mikael_s_Blessing),
        SDK::ItemIdValue(SDK::ItemId::Mikael_s_Blessing_Id223222),
        SDK::ItemIdValue(SDK::ItemId::Mikael_s_Blessing_Id323222) },
      CleanseItemTarget::Ally },
}};

inline constexpr const CleanseItemDefinition* FindCleanseItemDefinition(
    int itemId) noexcept {
    for (const auto& item : kCleanseItems) {
        for (const int candidate : item.ids) {
            if (candidate != 0 && candidate == itemId) return &item;
        }
    }
    return nullptr;
}

enum class OffensiveItemCast : std::uint8_t {
    Self,
    Target,
    Position,
};

enum class OffensiveItemExecute : std::uint8_t {
    None,
    FlatMagic,
    TargetMaxHealthMagic,
};

struct OffensiveItemDefinition {
    const char* menuKey = nullptr;
    const char* name = nullptr;
    std::array<int, 3> ids{};
    OffensiveItemCast cast = OffensiveItemCast::Self;
    float range = 0.0f;
    SDK::DamageType damageType = SDK::DamageType::True;
    bool enabledByDefault = true;
    OffensiveItemExecute execute = OffensiveItemExecute::None;
    float executeValue = 0.0f;
};

// Các ID trong cùng một hàng là biến thể của cùng item ở SR/Arena/Rotating.
// Item dash được tắt mặc định để activator không tự đổi vị trí ngoài ý muốn.
inline constexpr std::array<OffensiveItemDefinition, 9> kOffensiveItems = {{
    { "HextechGunblade", "Hextech Gunblade",
      { SDK::ItemIdValue(SDK::ItemId::Hextech_Gunblade),
        SDK::ItemIdValue(SDK::ItemId::Hextech_Gunblade_Id223146),
        SDK::ItemIdValue(SDK::ItemId::Hextech_Gunblade_Id663146) },
      OffensiveItemCast::Target, 700.0f, SDK::DamageType::Magical, true,
      OffensiveItemExecute::FlatMagic, 175.0f },
    { "DeathfireGrasp", "Deathfire Grasp",
      { SDK::ItemIdValue(SDK::ItemId::Deathfire_Grasp), 0, 0 },
      OffensiveItemCast::Target, 750.0f, SDK::DamageType::Magical, true,
      OffensiveItemExecute::TargetMaxHealthMagic, 0.15f },
    { "Everfrost", "Everfrost",
      { SDK::ItemIdValue(SDK::ItemId::Everfrost),
        SDK::ItemIdValue(SDK::ItemId::Everfrost_Id226656),
        SDK::ItemIdValue(SDK::ItemId::Everfrost_Id446656) },
      OffensiveItemCast::Position, 850.0f, SDK::DamageType::Magical, true },
    { "HextechRocketbelt", "Hextech Rocketbelt",
      { SDK::ItemIdValue(SDK::ItemId::Hextech_Rocketbelt),
        SDK::ItemIdValue(SDK::ItemId::Hextech_Rocketbelt_Id223152), 0 },
      OffensiveItemCast::Position, 725.0f, SDK::DamageType::Magical, false },
    { "Galeforce", "Galeforce",
      { SDK::ItemIdValue(SDK::ItemId::Galeforce),
        SDK::ItemIdValue(SDK::ItemId::Galeforce_Id226671),
        SDK::ItemIdValue(SDK::ItemId::Galeforce_Id446671) },
      OffensiveItemCast::Position, 450.0f, SDK::DamageType::Physical, false },
    { "YoumuusGhostblade", "Youmuu's Ghostblade",
      { SDK::ItemIdValue(SDK::ItemId::Youmuu_s_Ghostblade),
        SDK::ItemIdValue(SDK::ItemId::Youmuu_s_Ghostblade_Id223142), 0 },
      OffensiveItemCast::Self, 1000.0f, SDK::DamageType::Physical, true },
    { "RanduinsOmen", "Randuin's Omen",
      { SDK::ItemIdValue(SDK::ItemId::Randuin_s_Omen),
        SDK::ItemIdValue(SDK::ItemId::Randuin_s_Omen_Id223143), 0 },
      OffensiveItemCast::Self, 450.0f, SDK::DamageType::Physical, true },
    { "ShurelyasBattlesong", "Shurelya's Battlesong",
      { SDK::ItemIdValue(SDK::ItemId::Shurelya_s_Battlesong),
        SDK::ItemIdValue(SDK::ItemId::Shurelya_s_Battlesong_Id222065),
        SDK::ItemIdValue(SDK::ItemId::Shurelya_s_Battlesong_Id322065) },
      OffensiveItemCast::Self, 1000.0f, SDK::DamageType::Magical, true },
    { "SwordOfTheDivine", "Sword of the Divine",
      { SDK::ItemIdValue(SDK::ItemId::Sword_of_the_Divine),
        SDK::ItemIdValue(SDK::ItemId::Sword_of_the_Divine_Id443060),
        SDK::ItemIdValue(SDK::ItemId::Sword_of_the_Divine_Id663060) },
      OffensiveItemCast::Self, 700.0f, SDK::DamageType::Physical, true },
}};

inline constexpr const OffensiveItemDefinition* FindOffensiveItemDefinition(
    int itemId) noexcept {
    for (const auto& item : kOffensiveItems) {
        for (const int candidate : item.ids) {
            if (candidate != 0 && candidate == itemId) return &item;
        }
    }
    return nullptr;
}

inline constexpr bool SupportsAutomaticKillsteal(
    const OffensiveItemDefinition& item) noexcept {
    return item.execute != OffensiveItemExecute::None &&
           item.cast == OffensiveItemCast::Target;
}

inline constexpr bool IsOffensiveItemMode(SDK::OrbwalkingMode mode,
                                           bool useInHarass) noexcept {
    return mode == SDK::OrbwalkingMode::Combo ||
           (useInHarass && mode == SDK::OrbwalkingMode::Harass);
}

} // namespace Plugins::KuroActivator
