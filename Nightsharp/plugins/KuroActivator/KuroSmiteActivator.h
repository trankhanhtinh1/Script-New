#pragma once

#include "KuroActivatorComponent.h"
#include "../../SDK/GameObjects/GameObjects.h"
#include "../../SDK/Wrappers/Orbwalking/Orbwalker.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace Plugins::KuroActivator {

enum class SmiteStage : int {
    Auto = 0,
    Base600 = 1,
    Unleashed1000 = 2,
    Primal1400 = 3,
};

// ============================================================================
// KuroSmiteActivator — nhóm "Trừng phạt": tất cả biến thể Smite (SummonerSmite
// + S5_SummonerSmiteDuel/PlayerGanker/Quick) gom chung một group, tự tạo menu
// và tự cast theo snapshot jungle list (không quét mỗi frame, chỉ lặp danh
// sách quái rừng hiện tại trong tầm).
// ============================================================================
class KuroSmiteActivator final : public KuroActivatorComponent {
public:
    static constexpr int kCastCacheMs = 500;
    static constexpr int kReuseMs = 150;

    KuroSmiteActivator() = default;
    KuroSmiteActivator(const KuroSmiteActivator&) = delete;
    KuroSmiteActivator& operator=(const KuroSmiteActivator&) = delete;

    const char* GetName() const override { return "Smite"; }

    void OnLoad(SDK::Menu* root) override {
        if (IsLoaded() || !root) return;
        root_ = root;

        SDK::Menu* smite = root_->AddSubMenu(new SDK::Menu("SmiteGroup", "Smite"));
        smite->Add(new SDK::MenuSeparator(
            "SmiteHint", "All Smite variants share this group"));
        enabled_ = smite->Add(new SDK::MenuBool("Enabled", "Enabled", true));
        requireOrbwalk_ = smite->Add(new SDK::MenuBool(
            "RequireOrbwalk", "Only while orbwalking", true));
        reserveScuttle_ = smite->Add(new SDK::MenuBool(
            "ReserveScuttle", "Reserve charge from Scuttle", false));
        stage_ = smite->Add(new SDK::MenuList(
            "DamageStage", "Damage stage",
            { "Auto", "Base 600", "Unleashed 1000", "Primal 1400" }, 0));
        range_ = smite->Add(new SDK::MenuSliderF(
            "CastRange", "Cast range", 525.0f, 250.0f, 800.0f));

        ResetState();
        SetLoaded(true);
        NightSharpDebug::Logf("[KuroActivator][Smite] loaded");
    }

    void OnUnload() override {
        if (!IsLoaded()) return;
        root_ = nullptr;
        enabled_ = nullptr;
        requireOrbwalk_ = nullptr;
        reserveScuttle_ = nullptr;
        stage_ = nullptr;
        range_ = nullptr;
        ResetState();
        SetLoaded(false);
        NightSharpDebug::Logf("[KuroActivator][Smite] unloaded");
    }

    void OnUpdate() override {
        if (!IsLoaded()) return;
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) return;
        const int now = SDK::Variables::TickCount();

        UpdateSmite(player, now);
    }

private:
    void ResetState() noexcept {
        lastCastTick_ = 0;
        smiteSlot_ = -1;
        smiteRefreshedAt_ = 0;
        std::memset(smiteName_, 0, sizeof(smiteName_));
    }

    void UpdateSmite(const SDK::AIHeroClient& player, int now) {
        if (!enabled_ || !enabled_->Value) return;
        if (requireOrbwalk_ && requireOrbwalk_->Value &&
            SDK::Orbwalker::ActiveMode() == SDK::OrbwalkingMode::None) {
            return;
        }

        if (now - smiteRefreshedAt_ >= kCastCacheMs) {
            smiteRefreshedAt_ = now;
            smiteName_[0] = '\0';
            smiteSlot_ = FindSummonerSlot(player, "smite");
            if (smiteSlot_ != -1) {
                const auto spell = player.Spellbook().GetSpell(
                    static_cast<SDK::SpellSlot>(smiteSlot_));
                const std::string name =
                    spell.IsValid() ? spell.Name() : std::string();
                std::snprintf(smiteName_, sizeof(smiteName_), "%s", name.c_str());
                NightSharpDebug::Logf(
                    "[<b-cyan>KuroActivator</b-cyan>][<b-yellow>Smite</b-yellow>] "
                    "slot=%d name=%s", smiteSlot_, smiteName_);
            } else {
                NightSharpDebug::Logf(
                    "[<b-cyan>KuroActivator</b-cyan>][<b-yellow>Smite</b-yellow>] "
                    "summoner 'smite' not found");
            }
        }
        if (smiteSlot_ == -1) return;

        const auto spell = player.Spellbook().GetSpell(
            static_cast<SDK::SpellSlot>(smiteSlot_));
        if (!spell.IsValid() ||
            spell.State(SDK::Game::Time()) != SDK::CoreSpellBook::State_Ready) {
            return;
        }

        const float damage = SmiteDamage();
        if (damage <= 0.0f) return;
        if (now - lastCastTick_ < kReuseMs) return;

        const float range = range_ ? range_->Value : 525.0f;
        const bool reserveScuttle = reserveScuttle_ && reserveScuttle_->Value;
        const int charges = spell.Ammo();

        SDK::AIMinionClient best{};
        float bestDist = 1e9f;
        for (const auto& monster : SDK::GameObjects::Jungle()) {
            if (!monster.IsValid() || monster.IsDead() || !monster.IsTargetable()) {
                continue;
            }
            if (reserveScuttle && IsScuttle(monster) && charges <= 1) continue;
            const float dist = player.Position().Distance2D(monster.Position());
            if (dist > range + monster.BoundingRadius()) continue;
            if (monster.Health() > damage) continue; // không hạ được => bỏ qua
            if (dist < bestDist) {
                best = monster;
                bestDist = dist;
            }
        }
        if (!best.IsValid()) {
            NightSharpDebug::Logf(
                "[<b-cyan>KuroActivator</b-cyan>][<b-yellow>Smite</b-yellow>] "
                "no target in range (dmg=%.0f, jungle=%zu)",
                damage,
                SDK::GameObjects::Jungle().size());
            return;
        }

        if (player.Spellbook().CastSpell(
                static_cast<SDK::SpellSlot>(smiteSlot_), best)) {
            lastCastTick_ = now;
            NightSharpDebug::Logf(
                "[<b-cyan>KuroActivator</b-cyan>][<b-yellow>Smite</b-yellow>] "
                "cast on %s dmg=%.0f hp=%.0f",
                best.CharacterName().c_str(), damage, best.Health());
        } else {
            NightSharpDebug::Logf(
                "[<b-cyan>KuroActivator</b-cyan>][<b-yellow>Smite</b-yellow>] "
                "cast FAILED on %s",
                best.CharacterName().c_str());
        }
    }

    // Damage theo Patch 26.1: Smite = 600 / Unleashed = 1000 / Primal = 1400.
    // Auto: tên "SummonerSmite" => 600; mọi tên "S5_..." (đã evolve) => 1000
    // (bảo thủ — tên spell không phân biệt được Unleashed vs Primal).
    float SmiteDamage() const noexcept {
        const int stage = stage_ ? stage_->Index : 0;
        if (stage == 1) return 600.0f;
        if (stage == 2) return 1000.0f;
        if (stage == 3) return 1400.0f;
        if (smiteName_[0] == '\0') return 0.0f;
        if (ContainsIgnoreCase(smiteName_, "s5_")) return 1000.0f;
        if (ContainsIgnoreCase(smiteName_, "smite")) return 600.0f;
        return 0.0f;
    }

    static bool IsScuttle(const SDK::AIMinionClient& monster) noexcept {
        return ContainsIgnoreCase(monster.CharacterName().c_str(), "scuttle");
    }

    SDK::Menu* root_ = nullptr;
    SDK::MenuBool* enabled_ = nullptr;
    SDK::MenuBool* requireOrbwalk_ = nullptr;
    SDK::MenuBool* reserveScuttle_ = nullptr;
    SDK::MenuList* stage_ = nullptr;
    SDK::MenuSliderF* range_ = nullptr;

    int lastCastTick_ = 0;
    int smiteSlot_ = -1;
    int smiteRefreshedAt_ = 0;
    char smiteName_[64] = {};
};

} // namespace Plugins::KuroActivator