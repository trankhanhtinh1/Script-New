#pragma once

#include "KuroActivatorComponent.h"
#include "KuroActivatorPolicy.h"
#include "../../SDK/Extensions/AIBaseClientExtensions.h"
#include "../../SDK/GameObjects/GameObjects.h"
#include "../../SDK/Utils/Items.h"
#include "../../SDK/Wrappers/Orbwalking/Orbwalker.h"

#include <array>
#include <cstddef>

namespace Plugins::KuroActivator {

// QSS/Silvermere/Mercurial xoá CC cho bản thân; Mikael xoá CC cho đồng đội.
// Component đọc snapshot buff trực tiếp mỗi frame và không có reaction delay.
class KuroQssActivator final : public KuroActivatorComponent {
public:
    static constexpr int kMaxBuffType = 64;
    static constexpr float kMikaelRange = 750.0f;

    KuroQssActivator() = default;
    KuroQssActivator(const KuroQssActivator&) = delete;
    KuroQssActivator& operator=(const KuroQssActivator&) = delete;

    const char* GetName() const override { return "QSS & Cleanse"; }

    void OnLoad(SDK::Menu* root) override {
        if (IsLoaded() || !root) return;
        root_ = root;

        SDK::Menu* qss = root_->AddSubMenu(
            new SDK::Menu("QssGroup", "QSS / Cleanse (remove debuffs)"));
        qss->Add(new SDK::MenuSeparator(
            "QssHint",
            "Active orbwalker mode; cleanse immediately with no reaction delay"));
        enabled_ = qss->Add(new SDK::MenuBool("Enabled", "Enabled", true));

        itemEnabled_.fill(nullptr);
        selfCcEnabled_.fill(nullptr);
        allyCcEnabled_.fill(nullptr);

        SDK::Menu* self = qss->AddSubMenu(
            new SDK::Menu("SelfCleanseGroup", "Self Cleanse"));
        self->Add(new SDK::MenuSeparator(
            "SelfItemHint", "Self items: QSS / Silvermere / Mercurial"));
        for (std::size_t i = 0; i < kCleanseItems.size(); ++i) {
            const auto& item = kCleanseItems[i];
            if (item.target != CleanseItemTarget::Self) continue;
            itemEnabled_[i] = self->Add(new SDK::MenuBool(
                item.menuKey, item.name, true));
        }
        useCleanse_ = self->Add(new SDK::MenuBool(
            "UseCleanse", "Cleanse summoner spell", true));
        self->Add(new SDK::MenuSeparator("SelfCcHint", "Cleanse self on:"));
        for (std::size_t i = 0; i < kCcTrackedTypes.size(); ++i) {
            const auto& cc = kCcTrackedTypes[i];
            selfCcEnabled_[i] = self->Add(new SDK::MenuBool(
                cc.name, cc.name, cc.enabledByDefault));
        }

        SDK::Menu* ally = qss->AddSubMenu(
            new SDK::Menu("AllyCleanseGroup", "Ally Cleanse"));
        allyEnabled_ = ally->Add(new SDK::MenuBool(
            "Enabled", "Use cleanse items on allies", true));
        ally->Add(new SDK::MenuSeparator(
            "AllyItemHint", "Mikael range: 750; Airborne/Suppression excluded"));
        for (std::size_t i = 0; i < kCleanseItems.size(); ++i) {
            const auto& item = kCleanseItems[i];
            if (item.target != CleanseItemTarget::Ally) continue;
            itemEnabled_[i] = ally->Add(new SDK::MenuBool(
                item.menuKey, item.name, true));
        }
        ally->Add(new SDK::MenuSeparator("AllyCcHint", "Cleanse allies on:"));
        for (std::size_t i = 0; i < kCcTrackedTypes.size(); ++i) {
            const auto& cc = kCcTrackedTypes[i];
            if (!cc.standardCleanseRemovable) continue;
            allyCcEnabled_[i] = ally->Add(new SDK::MenuBool(
                cc.name, cc.name, cc.enabledByDefault));
        }

        ResetState();
        SetLoaded(true);
        if (IsDebugLogEnabled()) {
            NightSharpDebug::Logf("[KuroActivator][QSS] loaded");
        }
    }

    void OnUnload() override {
        if (!IsLoaded()) return;
        root_ = nullptr;
        enabled_ = nullptr;
        allyEnabled_ = nullptr;
        useCleanse_ = nullptr;
        itemEnabled_.fill(nullptr);
        selfCcEnabled_.fill(nullptr);
        allyCcEnabled_.fill(nullptr);
        ResetState();
        SetLoaded(false);
        if (IsDebugLogEnabled()) {
            NightSharpDebug::Logf("[KuroActivator][QSS] unloaded");
        }
    }

    void OnUpdate() override {
        if (!IsLoaded() || !enabled_ || !enabled_->Value) return;
        if (!IsQssOrbwalkerModeAllowed(SDK::Orbwalker::ActiveMode())) {
            buffPresent_.fill(false);
            return;
        }

        const auto player = SDK::GameObjects::Player();
        if (!player.IsValid() || player.IsDead()) return;

        if (TrySelfCleanse(player)) return;
        if (allyEnabled_ && allyEnabled_->Value) {
            TryAllyCleanse(player);
        }
    }

private:
    void ResetState() noexcept {
        buffPresent_.fill(false);
    }

    static int FindReadyItemId(
        const SDK::AIHeroClient& player,
        const CleanseItemDefinition& item) noexcept {
        for (const int itemId : item.ids) {
            if (itemId != 0 && SDK::Items::CanUseItem(player, itemId)) {
                return itemId;
            }
        }
        return 0;
    }

    bool TrySelfCleanse(const SDK::AIHeroClient& player) {
        bool hasTrackedCc = false;
        bool standardCleanseCompatible = false;

        for (const auto& cc : kCcTrackedTypes) {
            const int idx = static_cast<int>(cc.type);
            const bool present = SDK::HasBuffOfType(player, cc.type);
            const bool wasPresent = buffPresent_[idx];
            buffPresent_[idx] = present;

            if (!present || wasPresent) continue; 

            const bool enabled =
                selfCcEnabled_[idx] && selfCcEnabled_[idx]->Value;
            if (!enabled) continue;

            hasTrackedCc = true;
            if (cc.standardCleanseRemovable) {
                standardCleanseCompatible = true;
            }
        }

        if (!hasTrackedCc) return false;

        for (std::size_t i = 0; i < kCleanseItems.size(); ++i) {
            const auto& item = kCleanseItems[i];
            if (item.target != CleanseItemTarget::Self ||
                !itemEnabled_[i] || !itemEnabled_[i]->Value) {
                continue;
            }
            const int itemId = FindReadyItemId(player, item);
            if (itemId == 0) continue;

            const bool cast = SDK::Items::UseItem(player, itemId);
            if (IsDebugLogEnabled()) {
                NightSharpDebug::Logf(
                    "[<b-cyan>KuroActivator</b-cyan>][<b-yellow>SelfCleanse</b-yellow>] "
                    "item=%s id=%d cast=%d",
                    item.name, itemId, cast ? 1 : 0);
            }
            if (cast) return true;
        }

        return standardCleanseCompatible && TryCleanseSpell(player);
    }

    bool TryCleanseSpell(const SDK::AIHeroClient& player) const {
        if (!useCleanse_ || !useCleanse_->Value) return false;

        const int cleanseSlot = FindSummonerSlot(player, "boost");
        if (cleanseSlot == -1) return false;
        const auto spell = player.Spellbook().GetSpell(
            static_cast<SDK::SpellSlot>(cleanseSlot));
        if (!spell.IsValid() ||
            spell.State(SDK::Game::Time()) !=
                SDK::CoreSpellBook::State_Ready) {
            return false;
        }

        const bool cast = player.Spellbook().CastSpell(
            static_cast<SDK::SpellSlot>(cleanseSlot));
        if (IsDebugLogEnabled()) {
            NightSharpDebug::Logf(
                "[<b-cyan>KuroActivator</b-cyan>][<b-yellow>SelfCleanse</b-yellow>] "
                "Cleanse slot=%d cast=%d",
                cleanseSlot, cast ? 1 : 0);
        }
        return cast;
    }

    int HighestEnabledAllyCcPriority(
        const SDK::AIHeroClient& ally) const noexcept {
        int priority = 0;
        for (const auto& cc : kCcTrackedTypes) {
            if (!cc.standardCleanseRemovable) continue; 
            const int idx = static_cast<int>(cc.type);
            if (idx < 0 || idx >= kMaxBuffType || !allyCcEnabled_[idx] || !allyCcEnabled_[idx]->Value) continue;
            if (SDK::HasBuffOfType(ally, cc.type)) {
                if (cc.priority > priority) priority = cc.priority;
            }
        }
        return priority;
    }

    SDK::AIHeroClient FindBestMikaelTarget(
        const SDK::AIHeroClient& player) const {
        SDK::AIHeroClient bestTarget{};
        int bestPriority = 0;
        float lowestHp = 1e9f;

        for (const auto& ally : SDK::GameObjects::AllyHeroes()) {
            if (!ally.IsValid() || ally.IsDead() || ally.IsMe()) continue;
            if (player.Position().Distance2D(ally.Position()) > kMikaelRange) {
                continue;
            }
            const int priority = HighestEnabledAllyCcPriority(ally);
            if (priority == 0) continue;

            const float hp = ally.Health();
            if (priority > bestPriority ||
                (priority == bestPriority && hp < lowestHp)) {
                bestTarget = ally;
                bestPriority = priority;
                lowestHp = hp;
            }
        }
        return bestTarget;
    }

    bool TryAllyCleanse(const SDK::AIHeroClient& player) {
        for (std::size_t i = 0; i < kCleanseItems.size(); ++i) {
            const auto& item = kCleanseItems[i];
            if (item.target != CleanseItemTarget::Ally ||
                !itemEnabled_[i] || !itemEnabled_[i]->Value) {
                continue;
            }
            const int itemId = FindReadyItemId(player, item);
            if (itemId == 0) continue;

            const auto target = FindBestMikaelTarget(player);
            if (!target.IsValid()) return false;

            const bool cast = SDK::Items::UseItem(player, itemId, target);
            if (IsDebugLogEnabled()) {
                NightSharpDebug::Logf(
                    "[<b-cyan>KuroActivator</b-cyan>][<b-yellow>AllyCleanse</b-yellow>] "
                    "item=%s id=%d target=%s cast=%d",
                    item.name,
                    itemId,
                    target.CharacterName().c_str(),
                    cast ? 1 : 0);
            }
            return cast;
        }
        return false;
    }

    SDK::Menu* root_ = nullptr;
    SDK::MenuBool* enabled_ = nullptr;
    SDK::MenuBool* allyEnabled_ = nullptr;
    SDK::MenuBool* useCleanse_ = nullptr;
    std::array<SDK::MenuBool*, kCleanseItems.size()> itemEnabled_{};
    std::array<SDK::MenuBool*, kCcTrackedTypes.size()> selfCcEnabled_{};
    std::array<SDK::MenuBool*, kCcTrackedTypes.size()> allyCcEnabled_{};
    std::array<bool, kMaxBuffType> buffPresent_{};
};

} // namespace Plugins::KuroActivator