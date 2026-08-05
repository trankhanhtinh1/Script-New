#pragma once

#include "KuroActivatorComponent.h"
#include "../../SDK/GameObjects/GameObjects.h"
#include "../../SDK/Utils/Items.h"
#include "../../SDK/Wrappers/Orbwalking/Orbwalker.h"

#include <array>
#include <cstdint>
#include <cstdio>

namespace Plugins::KuroActivator {

struct AfterAttackItem {
    SDK::ItemId id = SDK::ItemId::Unknown;
    const char* name = nullptr;
};

// Item active xoay quanh người chơi, dùng ngay sau auto-attack:
//   - Bộ cũ từ ActivatorV2 (TestActivator): Ironspike Whip -> Goredrinker -> Stridebreaker
//   - Bộ mới từ AwarenessActivator PatchRegistry: Tiamat -> Ravenous/Titanic/Profane Hydra
inline constexpr std::array<AfterAttackItem, 7> kAfterAttackItems = {{
    { SDK::ItemId::Ironspike_Whip, "Ironspike Whip" },
    { SDK::ItemId::Goredrinker, "Goredrinker" },
    { SDK::ItemId::Stridebreaker, "Stridebreaker" },
    { SDK::ItemId::Tiamat, "Tiamat" },
    { SDK::ItemId::Ravenous_Hydra, "Ravenous Hydra" },
    { SDK::ItemId::Titanic_Hydra, "Titanic Hydra" },
    { SDK::ItemId::Profane_Hydra, "Profane Hydra" },
}};

// ============================================================================
// KuroAfterAttackActivator — nhóm item dùng ngay sau auto-attack (Tiamat,
// Hydra, Stridebreaker, ...). Tự tạo menu, tự subscribe Orbwalker OnAfterAttack
// và tự cast item đầu tiên còn toggled + ready khi AA trúng enemy hero/jungle.
// ============================================================================
class KuroAfterAttackActivator final : public KuroActivatorComponent {
public:
    static constexpr int kReuseMs = 200;

    KuroAfterAttackActivator() = default;
    KuroAfterAttackActivator(const KuroAfterAttackActivator&) = delete;
    KuroAfterAttackActivator& operator=(const KuroAfterAttackActivator&) = delete;

    const char* GetName() const override { return "After Attack Items"; }

    void OnLoad(SDK::Menu* root) override {
        if (IsLoaded() || !root) return;
        root_ = root;

        SDK::Menu* aa = root_->AddSubMenu(
            new SDK::Menu("AfterAttackGroup", "After Attack Items"));
        aa->Add(new SDK::MenuSeparator(
            "AaHint",
            "Order: Whip -> Goredrinker -> Stridebreaker -> Tiamat -> Hydras"));
        enabled_ = aa->Add(new SDK::MenuBool("Enabled", "Enabled", true));
        requireOrbwalk_ = aa->Add(new SDK::MenuBool(
            "RequireOrbwalk", "Only while orbwalking", true));
        includeMinions_ = aa->Add(new SDK::MenuBool(
            "IncludeMinions", "Also on lane minions (wave clear)", false));

        for (std::size_t i = 0; i < kAfterAttackItems.size(); ++i) {
            itemEnabled_[i] = aa->Add(new SDK::MenuBool(
                kAfterAttackItems[i].name, kAfterAttackItems[i].name, true));
        }

        ResetState();
        activeInstance_ = this;
        SDK::Orbwalker::OnAfterAttack += &KuroAfterAttackActivator::OnAfterAttackEvent;

        SetLoaded(true);
        NightSharpDebug::Logf("[KuroActivator][AfterAA] loaded");
    }

    void OnUnload() override {
        if (!IsLoaded()) return;
        activeInstance_ = nullptr;
        SDK::Orbwalker::OnAfterAttack -= &KuroAfterAttackActivator::OnAfterAttackEvent;
        root_ = nullptr;
        enabled_ = nullptr;
        requireOrbwalk_ = nullptr;
        includeMinions_ = nullptr;
        itemEnabled_.fill(nullptr);
        ResetState();
        SetLoaded(false);
        NightSharpDebug::Logf("[KuroActivator][AfterAA] unloaded");
    }

    void OnUpdate() override {
        // Không cần vòng update: chỉ hoạt động qua event OnAfterAttack.
    }

private:
    void ResetState() noexcept {
        lastCastTick_ = 0;
    }

    // Event handler static (SDK cần function pointer), funnel về instance đang
    // active — an toàn ngay cả khi component đã unload.
    static void OnAfterAttackEvent(SDK::OrbwalkingActionArgs& args) {
        KuroAfterAttackActivator* self = activeInstance_;
        if (!self || !self->IsLoaded()) return;
        self->HandleAfterAttack(args.Target);
    }

    // Chỉ cast khi AA trúng mục tiêu địch: enemy hero hoặc jungle monster.
    static bool IsValidTarget(const SDK::AttackableUnit& target,
                              bool includeMinions) noexcept {
        if (!target.IsValid() || target.IsDead()) return false;
        if (!target.IsEnemy()) return false;
        if (target.IsHero()) return true;

        const SDK::AIMinionClient minion(target.Handle());
        if (!minion.IsValid()) return false;
        if (minion.IsJungle()) return true;
        return includeMinions && minion.IsMinion();
    }

    void HandleAfterAttack(const SDK::AttackableUnit& target) {
        if (!enabled_ || !enabled_->Value) return;
        if (requireOrbwalk_ && requireOrbwalk_->Value &&
            SDK::Orbwalker::ActiveMode() == SDK::OrbwalkingMode::None) {
            return;
        }
        if (!IsValidTarget(target, includeMinions_ && includeMinions_->Value)) return;

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) return;
        const int now = SDK::Variables::TickCount();
        if (now - lastCastTick_ < kReuseMs) return;

        // Cast item đầu tiên còn bật + ready; mỗi item tự kiểm tra cooldown.
        for (std::size_t i = 0; i < kAfterAttackItems.size(); ++i) {
            if (!itemEnabled_[i] || !itemEnabled_[i]->Value) continue;
            if (SDK::Items::UseItem(player, kAfterAttackItems[i].id)) {
                lastCastTick_ = now;
                NightSharpDebug::Logf("[KuroActivator][AfterAA] item id=%d",
                                      SDK::ItemIdValue(kAfterAttackItems[i].id));
                return;
            }
        }
    }

    SDK::Menu* root_ = nullptr;
    SDK::MenuBool* enabled_ = nullptr;
    SDK::MenuBool* requireOrbwalk_ = nullptr;
    SDK::MenuBool* includeMinions_ = nullptr;
    std::array<SDK::MenuBool*, kAfterAttackItems.size()> itemEnabled_{};

    int lastCastTick_ = 0;

    inline static KuroAfterAttackActivator* activeInstance_ = nullptr;
};

} // namespace Plugins::KuroActivator
