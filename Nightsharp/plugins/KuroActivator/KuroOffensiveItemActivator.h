#pragma once

#include "KuroActivatorComponent.h"
#include "KuroActivatorPolicy.h"
#include "../../SDK/Extensions/AIBaseClientExtensions.h"
#include "../../SDK/GameObjects/GameObjects.h"
#include "../../SDK/Utils/Items.h"
#include "../../SDK/Wrappers/Orbwalking/Orbwalker.h"
#include "../../SDK/Wrappers/TargetSelector/TargetSelector.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Plugins::KuroActivator {


// Active item chiến đấu chạy trong Combo/Harass. Gunblade và Deathfire Grasp
// có nhánh killsteal riêng, quét trực tiếp mọi enemy và hoạt động ở mọi mode.
class KuroOffensiveItemActivator final : public KuroActivatorComponent {
public:
    static constexpr int kReuseMs = 150;

    KuroOffensiveItemActivator() = default;
    KuroOffensiveItemActivator(const KuroOffensiveItemActivator&) = delete;
    KuroOffensiveItemActivator& operator=(const KuroOffensiveItemActivator&) = delete;

    const char* GetName() const override { return "Combat Items"; }

    void OnLoad(SDK::Menu* root) override {
        if (IsLoaded() || !root) return;
        root_ = root;

        SDK::Menu* combat = root_->AddSubMenu(
            new SDK::Menu("CombatItemGroup", "Combat Items"));
        combat->Add(new SDK::MenuSeparator(
            "CombatItemHint",
            "Auto-cast in Combo; dash items are disabled by default"));
        enabled_ = combat->Add(new SDK::MenuBool("Enabled", "Enabled", true));
        useInHarass_ = combat->Add(new SDK::MenuBool(
            "UseInHarass", "Also use in Harass", false));

        itemEnabled_.fill(nullptr);
        killstealEnabled_.fill(nullptr);
        for (std::size_t i = 0; i < kOffensiveItems.size(); ++i) {
            itemEnabled_[i] = combat->Add(new SDK::MenuBool(
                kOffensiveItems[i].menuKey,
                kOffensiveItems[i].name,
                kOffensiveItems[i].enabledByDefault));
        }

        SDK::Menu* killsteal = combat->AddSubMenu(
            new SDK::Menu("KillstealGroup", "Automatic Killsteal"));
        killsteal_ = killsteal->Add(new SDK::MenuBool(
            "Enabled", "Enabled in every orbwalker mode", true));
        for (std::size_t i = 0; i < kOffensiveItems.size(); ++i) {
            const auto& item = kOffensiveItems[i];
            if (!SupportsAutomaticKillsteal(item)) continue;
            killstealEnabled_[i] = killsteal->Add(new SDK::MenuBool(
                item.menuKey, item.name, true));
        }

        lastUseTick_ = 0;
        SetLoaded(true);
        if (IsDebugLogEnabled()) {
            NightSharpDebug::Logf("[KuroActivator][CombatItems] loaded");
        }
    }

    void OnUnload() override {
        if (!IsLoaded()) return;
        root_ = nullptr;
        enabled_ = nullptr;
        useInHarass_ = nullptr;
        killsteal_ = nullptr;
        itemEnabled_.fill(nullptr);
        killstealEnabled_.fill(nullptr);
        lastUseTick_ = 0;
        SetLoaded(false);
        if (IsDebugLogEnabled()) {
            NightSharpDebug::Logf("[KuroActivator][CombatItems] unloaded");
        }
    }

    void OnUpdate() override {
        if (!IsLoaded() || !enabled_ || !enabled_->Value) return;

        const auto player = SDK::GameObjects::Player();
        if (!player.IsValid() || player.IsDead()) return;

        const int now = SDK::Variables::TickCount();
        if (killsteal_ && killsteal_->Value && TryKillsteal(player, now)) {
            return;
        }

        const SDK::OrbwalkingMode mode = SDK::Orbwalker::ActiveMode();
        const bool allowHarass = useInHarass_ && useInHarass_->Value;
        const bool modeAllowed = IsOffensiveItemMode(mode, allowHarass);
        if (!modeAllowed) return;

        if (now - lastUseTick_ < kReuseMs) return;

        auto* selector = SDK::TargetSelector::Instance();
        if (!selector) return;

        for (std::size_t i = 0; i < kOffensiveItems.size(); ++i) {
            if (!itemEnabled_[i] || !itemEnabled_[i]->Value) continue;

            const auto& item = kOffensiveItems[i];
            const int itemId = FindReadyItemId(player, item);
            if (itemId == 0) continue;

            const auto target = selector->GetTarget(item.range, item.damageType);
            if (!SDK::TargetSelector::IsValidTarget(
                    target, item.range, item.damageType)) {
                continue;
            }

            bool cast = false;
            switch (item.cast) {
            case OffensiveItemCast::Target:
                cast = SDK::Items::UseItem(player, itemId, target);
                break;
            case OffensiveItemCast::Position:
                cast = SDK::Items::UseItem(player, itemId, target.Position());
                break;
            case OffensiveItemCast::Self:
                cast = SDK::Items::UseItem(player, itemId);
                break;
            }

            lastUseTick_ = now;
            if (IsDebugLogEnabled()) {
                NightSharpDebug::Logf(
                    "[<b-cyan>KuroActivator</b-cyan>]"
                    "[<b-yellow>CombatItems</b-yellow>] item=%s id=%d target=%s cast=%d",
                    item.name,
                    itemId,
                    target.CharacterName().c_str(),
                    cast ? 1 : 0);
            }
            return;
        }
    }

private:
    static int FindReadyItemId(const SDK::AIHeroClient& player,
                               const OffensiveItemDefinition& item) noexcept {
        for (const int itemId : item.ids) {
            if (itemId != 0 && SDK::Items::CanUseItem(player, itemId)) {
                return itemId;
            }
        }
        return 0;
    }

    static float EstimateKillstealDamage(
        const SDK::AIHeroClient& player,
        const SDK::AIHeroClient& target,
        const OffensiveItemDefinition& item) noexcept {
        float rawDamage = 0.0f;
        switch (item.execute) {
        case OffensiveItemExecute::FlatMagic:
            rawDamage = item.executeValue;
            break;
        case OffensiveItemExecute::TargetMaxHealthMagic:
            rawDamage = target.MaxHealth() * item.executeValue;
            break;
        case OffensiveItemExecute::None:
            return 0.0f;
        }
        return player.CalculateMagicDamage(target, rawDamage);
    }

    static float EffectiveHealthFor(
        const SDK::AIHeroClient& target,
        SDK::DamageType damageType) noexcept {
        float health = target.Health() + target.AllShield();
        if (damageType == SDK::DamageType::Magical) {
            health += target.MagicalShield();
        } else if (damageType == SDK::DamageType::Physical) {
            health += target.PhysicalShield();
        }
        return health;
    }

    bool TryKillsteal(const SDK::AIHeroClient& player, int now) {
        for (std::size_t i = 0; i < kOffensiveItems.size(); ++i) {
            const auto& item = kOffensiveItems[i];
            if (!SupportsAutomaticKillsteal(item) ||
                !itemEnabled_[i] || !itemEnabled_[i]->Value ||
                !killstealEnabled_[i] || !killstealEnabled_[i]->Value) {
                continue;
            }

            const int itemId = FindReadyItemId(player, item);
            if (itemId == 0) continue;

            SDK::AIHeroClient best{};
            float bestEffectiveHealth = 1e9f;
            float bestDamage = 0.0f;
            for (const auto& enemy : SDK::GameObjects::EnemyHeroesFrame()) {
                if (!SDK::TargetSelector::IsValidTarget(
                        enemy, item.range, item.damageType) ||
                    enemy.IsInvulnerable() ||
                    SDK::HasBuffOfType(enemy, SDK::BuffType::SpellShield) ||
                    SDK::HasBuffOfType(enemy, SDK::BuffType::SpellImmunity)) {
                    continue;
                }

                const float damage =
                    EstimateKillstealDamage(player, enemy, item);
                const float effectiveHealth =
                    EffectiveHealthFor(enemy, item.damageType);
                if (damage < effectiveHealth ||
                    effectiveHealth >= bestEffectiveHealth) {
                    continue;
                }

                best = enemy;
                bestDamage = damage;
                bestEffectiveHealth = effectiveHealth;
            }
            if (!best.IsValid()) continue;

            const bool cast = SDK::Items::UseItem(player, itemId, best);
            lastUseTick_ = now;
            if (IsDebugLogEnabled()) {
                NightSharpDebug::Logf(
                    "[<b-cyan>KuroActivator</b-cyan>]"
                    "[<b-yellow>Killsteal</b-yellow>] item=%s id=%d target=%s "
                    "damage=%.1f effectiveHp=%.1f cast=%d",
                    item.name,
                    itemId,
                    best.CharacterName().c_str(),
                    bestDamage,
                    bestEffectiveHealth,
                    cast ? 1 : 0);
            }
            return true;
        }
        return false;
    }

    SDK::Menu* root_ = nullptr;
    SDK::MenuBool* enabled_ = nullptr;
    SDK::MenuBool* useInHarass_ = nullptr;
    SDK::MenuBool* killsteal_ = nullptr;
    std::array<SDK::MenuBool*, kOffensiveItems.size()> itemEnabled_{};
    std::array<SDK::MenuBool*, kOffensiveItems.size()> killstealEnabled_{};
    int lastUseTick_ = 0;
};

} // namespace Plugins::KuroActivator
