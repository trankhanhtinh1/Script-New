#pragma once

#include "KuroActivatorComponent.h"
#include "../../SDK/Events/Events.h"
#include "../../SDK/Extensions/AIBaseClientExtensions.h"
#include "../../SDK/Utils/Items.h"
#include "../../SDK/Wrappers/Orbwalking/Orbwalker.h"

#include <array>
#include <cstdint>
#include <cstdio>

namespace Plugins::KuroActivator {

// Các loại CC mà group QSS sẽ loại bỏ.
struct CcTrackedType {
    SDK::BuffType type = SDK::BuffType::Internal;
    const char* name = nullptr;
};

inline constexpr std::array<CcTrackedType, 9> kCcTrackedTypes = {{
    { SDK::BuffType::Stun, "Stunned" },
    { SDK::BuffType::Blind, "Blinded" },
    { SDK::BuffType::Snare, "Snared" },
    { SDK::BuffType::Charm, "Charmed" },
    { SDK::BuffType::Fear, "Feared" },
    { SDK::BuffType::Suppression, "Suppressed" },
    { SDK::BuffType::Taunt, "Taunted" },
    { SDK::BuffType::Asleep, "Asleep" },
    { SDK::BuffType::AttackSpeedSlow, "Attack speed slowed" },
}};

// ============================================================================
// KuroQssActivator — nhóm "loại bỏ hiệu ứng xấu": Quicksilver Sash, Silvermere
// Dawn, Mercurial Scimitar, Cleanse summoner. Tự tạo menu, tự subscribe buff
// events và giữ snapshot cache tránh quét buff mỗi frame.
// ============================================================================
class KuroQssActivator final : public KuroActivatorComponent {
public:
    static constexpr int kMaxBuffType = 64;
    static constexpr int kSnapshotSyncMs = 1000;
    static constexpr int kCastCacheMs = 500;
    static constexpr int kReuseMs = 250;

    static constexpr SDK::ItemId kItems[] = {
        SDK::ItemId::Quicksilver_Sash,
        SDK::ItemId::Silvermere_Dawn,
        SDK::ItemId::Mercurial_Scimitar,
    };

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
            "QssHint", "Item order: QSS -> Silvermere -> Mercurial -> Cleanse"));
        enabled_ = qss->Add(new SDK::MenuBool("Enabled", "Enabled", true));
        requireOrbwalk_ = qss->Add(new SDK::MenuBool(
            "RequireOrbwalk", "Only while orbwalking", false));
        reactionDelay_ = qss->Add(new SDK::MenuSliderF(
            "ReactionDelay", "Reaction delay (ms)", 0.0f, 0.0f, 250.0f));
        useCleanse_ = qss->Add(new SDK::MenuBool(
            "UseCleanse", "Use Cleanse summoner spell", true));
        qss->Add(new SDK::MenuSeparator("CcHint", "Remove on:"));

        ccEnabled_.fill(nullptr);
        for (std::size_t i = 0; i < kCcTrackedTypes.size(); ++i) {
            ccEnabled_[i] = qss->Add(new SDK::MenuBool(
                kCcTrackedTypes[i].name, kCcTrackedTypes[i].name, true));
        }

        ResetState();
        activeInstance_ = this;
        SDK::Events::AddOnBuffAdd(&KuroQssActivator::OnBuffAddEvent);
        SDK::Events::AddOnBuffRemove(&KuroQssActivator::OnBuffRemoveEvent);
        SDK::Events::AddOnBuffUpdate(&KuroQssActivator::OnBuffUpdateEvent);

        SetLoaded(true);
        NightSharpDebug::Logf("[KuroActivator][QSS] loaded");
    }

    void OnUnload() override {
        if (!IsLoaded()) return;
        activeInstance_ = nullptr;
        SDK::Events::RemoveOnBuffAdd(&KuroQssActivator::OnBuffAddEvent);
        SDK::Events::RemoveOnBuffRemove(&KuroQssActivator::OnBuffRemoveEvent);
        SDK::Events::RemoveOnBuffUpdate(&KuroQssActivator::OnBuffUpdateEvent);
        root_ = nullptr;
        enabled_ = nullptr;
        requireOrbwalk_ = nullptr;
        reactionDelay_ = nullptr;
        useCleanse_ = nullptr;
        ccEnabled_.fill(nullptr);
        ResetState();
        SetLoaded(false);
        NightSharpDebug::Logf("[KuroActivator][QSS] unloaded");
    }

    void OnUpdate() override {
        if (!IsLoaded()) return;
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) return;
        const int now = SDK::Variables::TickCount();

        ReconcileSnapshot(player, now);
        UpdateQss(player, now);
    }

private:
    // ── Buff snapshot cache ────────────────────────────────────────────────
    void ResetState() noexcept {
        buffPresent_.fill(false);
        ccSinceTick_ = 0;
        lastSnapshotSyncAt_ = 0;
        lastCastTick_ = 0;
        cleanseSlot_ = -1;
        cleanseRefreshedAt_ = 0;
    }

    static bool IsPlayerSender(const ::Core::Events::ObjectInfo& sender) noexcept {
        const auto player = SDK::ObjectManager::Player();
        return player.IsValid() && sender.IsValid() &&
               sender.NetworkId == static_cast<std::uint32_t>(player.NetworkId());
    }

    static bool IsTrackedCc(int type) noexcept {
        for (const auto& cc : kCcTrackedTypes) {
            if (static_cast<int>(cc.type) == type) return true;
        }
        return false;
    }

    bool AnyActiveCc() const noexcept {
        for (const auto& cc : kCcTrackedTypes) {
            const int idx = static_cast<int>(cc.type);
            if (idx >= 0 && idx < kMaxBuffType && buffPresent_[idx]) return true;
        }
        return false;
    }

    void SetBuffType(int type, bool present) noexcept {
        if (type < 0 || type >= kMaxBuffType) return;
        const bool wasAny = AnyActiveCc();
        buffPresent_[type] = present;
        if (!IsTrackedCc(type)) return;
        const bool nowAny = AnyActiveCc();
        if (present && !wasAny && ccSinceTick_ == 0) {
            ccSinceTick_ = SDK::Variables::TickCount();
        } else if (!nowAny) {
            ccSinceTick_ = 0;
        }
    }

    // Event handlers static (SDK cần function pointer), funnel về instance
    // đang active — an toàn ngay cả khi component đã unload.
    static void HandleBuff(const SDK::Events::BuffEventArgs& args,
                           bool present) noexcept {
        KuroQssActivator* self = activeInstance_;
        if (!self || !self->IsLoaded() || !IsPlayerSender(args.Sender)) return;
        self->SetBuffType(args.Type, present);
    }

    static void OnBuffAddEvent(const SDK::Events::BuffEventArgs& args) {
        HandleBuff(args, true);
    }
    static void OnBuffRemoveEvent(const SDK::Events::BuffEventArgs& args) {
        HandleBuff(args, false);
    }
    static void OnBuffUpdateEvent(const SDK::Events::BuffEventArgs& args) {
        KuroQssActivator* self = activeInstance_;
        if (!self || !self->IsLoaded() || !IsPlayerSender(args.Sender)) return;
        self->SetBuffType(args.Type, args.Count > 0);
    }

    // Reconcile 1s/lần để tự phục hồi snapshot nếu miss event (~10 calls/sec).
    void ReconcileSnapshot(const SDK::AIHeroClient& player, int now) noexcept {
        if (now - lastSnapshotSyncAt_ < kSnapshotSyncMs) return;
        lastSnapshotSyncAt_ = now;
        for (const auto& cc : kCcTrackedTypes) {
            const int idx = static_cast<int>(cc.type);
            if (idx < 0 || idx >= kMaxBuffType) continue;
            buffPresent_[idx] = SDK::HasBuffOfType(player, cc.type);
        }
        buffPresent_[static_cast<int>(SDK::BuffType::SpellShield)] =
            SDK::HasBuffOfType(player, SDK::BuffType::SpellShield);
        if (!AnyActiveCc()) ccSinceTick_ = 0;
        else if (ccSinceTick_ == 0) ccSinceTick_ = now;
    }

    void UpdateQss(const SDK::AIHeroClient& player, int now) {
        if (!enabled_ || !enabled_->Value) return;
        if (requireOrbwalk_ && requireOrbwalk_->Value &&
            SDK::Orbwalker::ActiveMode() == SDK::OrbwalkingMode::None) {
            return;
        }

        bool ccActive = false;
        for (std::size_t i = 0; i < kCcTrackedTypes.size(); ++i) {
            const int idx = static_cast<int>(kCcTrackedTypes[i].type);
            if (ccEnabled_[i] && ccEnabled_[i]->Value && idx >= 0 &&
                idx < kMaxBuffType && buffPresent_[idx]) {
                ccActive = true;
                break;
            }
        }
        if (!ccActive) {
            if (ccSinceTick_ != 0) {
                NightSharpDebug::Logf(
                    "[<b-cyan>KuroActivator</b-cyan>][<b-yellow>QSS</b-yellow>] "
                    "cc present but all toggles off");
            }
            return;
        }
        // Không phí QSS khi có SpellShield (tương đương C# HaveSpellShield).
        if (buffPresent_[static_cast<int>(SDK::BuffType::SpellShield)]) {
            NightSharpDebug::Logf(
                "[<b-cyan>KuroActivator</b-cyan>][<b-yellow>QSS</b-yellow>] "
                "skip: SpellShield active");
            return;
        }
        if (now - lastCastTick_ < kReuseMs) return;

        const int elapsed = ccSinceTick_ > 0 ? now - ccSinceTick_ : 0;
        const float delayMs = reactionDelay_ ? reactionDelay_->Value : 0.0f;
        if (elapsed < delayMs) return;

        // Dùng item theo thứ tự; UseItem chỉ cast khi item đang ready.
        for (SDK::ItemId item : kItems) {
            const bool ready = SDK::CanUseItem(player, item);
            const bool cast = SDK::Items::UseItem(player, item);
            NightSharpDebug::Logf(
                "[<b-cyan>KuroActivator</b-cyan>][<b-yellow>QSS</b-yellow>] "
                "item id=%d slot=%d ready=%d cast=%d",
                SDK::ItemIdValue(item),
                static_cast<int>(SDK::GetItemSlot(player, SDK::ItemIdValue(item))),
                ready ? 1 : 0,
                cast ? 1 : 0);
            if (cast) {
                lastCastTick_ = now;
                return;
            }
        }

        if (useCleanse_ && useCleanse_->Value) {
            if (now - cleanseRefreshedAt_ >= kCastCacheMs) {
                cleanseRefreshedAt_ = now;
                cleanseSlot_ = FindSummonerSlot(player, "boost");
            }
            if (cleanseSlot_ == -1) {
                NightSharpDebug::Logf(
                    "[<b-cyan>KuroActivator</b-cyan>][<b-yellow>QSS</b-yellow>] "
                    "cleanse: summoner 'boost' not found");
                return;
            }
            const auto spell = player.Spellbook().GetSpell(
                static_cast<SDK::SpellSlot>(cleanseSlot_));
            const bool ready = spell.IsValid() &&
                spell.State(SDK::Game::Time()) ==
                    SDK::CoreSpellBook::State_Ready;
            if (!ready) {
                NightSharpDebug::Logf(
                    "[<b-cyan>KuroActivator</b-cyan>][<b-yellow>QSS</b-yellow>] "
                    "cleanse slot=%d not ready", cleanseSlot_);
                return;
            }
            if (player.Spellbook().CastSpell(
                    static_cast<SDK::SpellSlot>(cleanseSlot_))) {
                lastCastTick_ = now;
                NightSharpDebug::Logf(
                    "[<b-cyan>KuroActivator</b-cyan>][<b-yellow>QSS</b-yellow>] "
                    "Cleanse cast slot=%d", cleanseSlot_);
            } else {
                NightSharpDebug::Logf(
                    "[<b-cyan>KuroActivator</b-cyan>][<b-yellow>QSS</b-yellow>] "
                    "Cleanse cast FAILED slot=%d", cleanseSlot_);
            }
        }
    }

    SDK::Menu* root_ = nullptr;
    SDK::MenuBool* enabled_ = nullptr;
    SDK::MenuBool* requireOrbwalk_ = nullptr;
    SDK::MenuSliderF* reactionDelay_ = nullptr;
    SDK::MenuBool* useCleanse_ = nullptr;
    std::array<SDK::MenuBool*, 9> ccEnabled_{};

    std::array<bool, kMaxBuffType> buffPresent_{};
    int ccSinceTick_ = 0;
    int lastSnapshotSyncAt_ = 0;
    int lastCastTick_ = 0;
    int cleanseSlot_ = -1;
    int cleanseRefreshedAt_ = 0;

    inline static KuroQssActivator* activeInstance_ = nullptr;
};

} // namespace Plugins::KuroActivator