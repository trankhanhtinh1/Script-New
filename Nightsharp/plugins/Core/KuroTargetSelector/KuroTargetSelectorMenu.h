#pragma once

#include "KuroTargetSelectorContracts.h"
#include "KuroTargetSelectorPolicy.h"

#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/UI/UI.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Plugins::KuroTargetSelector {

// UI state is intentionally kept separate from the selector implementation.
// This lets the service remain useful to consumers that never attach a menu,
// while keeping all input and persistence concerns in one small owner.
class Menu final {
public:
    explicit Menu(::SDK::Menu* parent)
        : parent_(parent) {
        if (parent_) {
            native_ = parent_->AddSubMenu(
                new ::SDK::Menu("KuroTargetSelector", "Kuro Target Selector"));
        }
        BuildMenu();
        RefreshHeroes();
        Resume();
    }

    ~Menu() {
        Suspend();
    }

    void Suspend() {
        if (suspended_) return;
        ::SDK::Game::OnWndProc -= &OnWndProcHandler;
        if (Ptr() == this) Ptr() = nullptr;
        if (native_) native_->Visible = false;
        suspended_ = true;
    }

    void Resume() {
        if (!suspended_) return;
        Ptr() = this;
        ::SDK::Game::OnWndProc += &OnWndProcHandler;
        if (native_) native_->Visible = true;
        suspended_ = false;
    }

    bool IsSuspended() const { return suspended_; }

    bool PreferSelectedTarget() const {
        return BoolValue("PreferSelectedTarget", true);
    }

    bool OnlySelectedTarget() const {
        return BoolValue("OnlySelectedTarget", false);
    }

    float Stickiness() const {
        return static_cast<float>(SliderValue("Stickiness", 80));
    }

    TargetProfile Profile() const {
        const auto* item = native_
            ? native_->Get<::SDK::MenuList>("Profile")
            : nullptr;
        const int index = item ? item->Index : 0;
        const int max = static_cast<int>(TargetProfile::FleeThreat);
        return static_cast<TargetProfile>(std::clamp(index, 0, max));
    }

    void SetSelected(const ::SDK::AIHeroClient& target) {
        selected_ = target;
    }

    ::SDK::AIHeroClient Selected() const {
        return selected_;
    }

    bool Blacklist(int networkId) const {
        return IsBlacklisted(networkId);
    }

    bool IsBlacklisted(int networkId) const {
        if (networkId <= 0) return false;
        if (blacklisted_.find(networkId) != blacklisted_.end()) return true;
        const auto item = blacklistItems_.find(networkId);
        return item != blacklistItems_.end() && item->second && item->second->Value;
    }

    int Priority(int networkId) const {
        if (priorityMenu_ && networkId > 0) {
            const std::string key = "Target_" + std::to_string(networkId);
            if (const auto* item = priorityMenu_->Get<::SDK::MenuSlider>(key.c_str())) {
                return std::clamp(item->Value, 1, 5);
            }
        }
        const auto it = priorities_.find(networkId);
        return it == priorities_.end() ? 1 : std::clamp(it->second, 1, 5);
    }

    bool ToggleBlacklist(int networkId) {
        if (networkId <= 0) return false;

        const auto it = blacklisted_.find(networkId);
        const bool enabled = it == blacklisted_.end();
        if (enabled) {
            blacklisted_.insert(networkId);
        } else {
            blacklisted_.erase(it);
        }

        const auto item = blacklistItems_.find(networkId);
        if (item != blacklistItems_.end() && item->second) {
            item->second->Set(enabled);
        }
        return enabled;
    }

    bool CycleHotkeyActive() const {
        const auto* item = native_
            ? native_->Get<::SDK::MenuKeyBind>("CycleHotkey")
            : nullptr;
        return item && item->Active;
    }

    bool ManualOverrideActive() const {
        const auto* item = native_
            ? native_->Get<::SDK::MenuKeyBind>("ManualOverride")
            : nullptr;
        return item && item->Active;
    }

    // The diagnostics are retained so the drawing layer and a future menu
    // panel can consume one coherent snapshot.  Copies are deliberate: the
    // selector may rebuild its working vectors immediately after this call.
    void DrawDiagnostics(const TargetSnapshot& snapshot,
                         const std::vector<TargetDecision>& decisions) {
        diagnosticSnapshot_ = snapshot;
        diagnosticDecisions_ = decisions;
    }

    ::SDK::Menu* NativeMenu() const { return native_; }

    void Refresh() { RefreshHeroes(); }

private:
    static Menu*& Ptr() {
        static Menu* ptr = nullptr;
        return ptr;
    }

    static void OnWndProcHandler(::SDK::Game::WndEventArgs& args) {
        auto* self = Ptr();
        if (!self || self->suspended_) return;

        if (args.Msg == WM_LBUTTONDOWN && self->ManualOverrideActive()) {
            self->SelectClosestToCursor();
            return;
        }

        if (args.Msg == WM_KEYDOWN && self->CycleHotkeyActive()) {
            self->CycleSelection();
        }
    }

    void BuildMenu() {
        if (!native_) return;

        native_->Add(new ::SDK::MenuBool(
            "PreferSelectedTarget", "Prefer Selected Target", true));
        native_->Add(new ::SDK::MenuBool(
            "OnlySelectedTarget", "Only Selected Target", false));
        native_->Add(new ::SDK::MenuSlider(
            "Stickiness", "Stickiness", 80, 0, 200));
        native_->Add(new ::SDK::MenuList(
            "Profile", "Profile",
            {"General", "AutoAttack", "Burst", "DPS", "Poke", "Execute",
             "Peel", "Interrupt", "AntiGapcloser", "FleeThreat"},
            0));
        native_->Add(new ::SDK::MenuKeyBind(
            "CycleHotkey", "Cycle Selected Target", ::SDK::Keys::Tab,
            ::SDK::KeyBindType::Press, false));
        native_->Add(new ::SDK::MenuKeyBind(
            "ManualOverride", "Manual Target Override", ::SDK::Keys::Shift,
            ::SDK::KeyBindType::Press, false));

        blacklistMenu_ = native_->AddSubMenu(
            new ::SDK::Menu("Blacklist", "Blacklist"));
        priorityMenu_ = native_->AddSubMenu(
            new ::SDK::Menu("Priority", "Priority"));
    }

    void RefreshHeroes() {
        if (!priorityMenu_ || !blacklistMenu_) return;

        for (const auto& hero : ::SDK::GameObjects::EnemyHeroes()) {
            if (!hero.IsValid() || hero.NetworkId() <= 0) continue;
            EnsureHeroEntries(hero);
        }
    }

    void EnsureHeroEntries(const ::SDK::AIHeroClient& hero) {
        const int networkId = hero.NetworkId();
        if (networkId <= 0) return;

        const std::string suffix = std::to_string(networkId);
        const std::string priorityKey = "Target_" + suffix;
        const std::string blacklistKey = "Target_" + suffix;
        const std::string label = hero.CharacterName().empty()
            ? suffix
            : hero.CharacterName() + " (" + suffix + ")";

        if (priorities_.find(networkId) == priorities_.end()) {
            priorities_.emplace(networkId, 1);
        }
        if (!priorityMenu_->Get<::SDK::MenuSlider>(priorityKey.c_str())) {
            priorityMenu_->Add(new ::SDK::MenuSlider(
                priorityKey.c_str(), label.c_str(), Priority(networkId), 1, 5));
        }
        if (!blacklistMenu_->Get<::SDK::MenuBool>(blacklistKey.c_str())) {
            auto* item = blacklistMenu_->Add(new ::SDK::MenuBool(
                blacklistKey.c_str(), label.c_str(), IsBlacklisted(networkId)));
            blacklistItems_[networkId] = item;
        }
    }

    bool BoolValue(const char* key, bool fallback) const {
        const auto* item = native_ ? native_->Get<::SDK::MenuBool>(key) : nullptr;
        return item ? item->Value : fallback;
    }

    int SliderValue(const char* key, int fallback) const {
        const auto* item = native_ ? native_->Get<::SDK::MenuSlider>(key) : nullptr;
        return item ? item->Value : fallback;
    }

    void SelectClosestToCursor() {
        const ::SDK::Vector3 cursor = ::SDK::Game::CursorPos();
        ::SDK::AIHeroClient closest;
        float closestDistance = 180.0f;
        for (const auto& hero : ::SDK::GameObjects::EnemyHeroes()) {
            if (!hero.IsValid() || hero.IsDead() || IsBlacklisted(hero.NetworkId())) {
                continue;
            }
            const float distance = hero.Distance(cursor);
            if (distance <= closestDistance) {
                closestDistance = distance;
                closest = hero;
            }
        }
        const bool blacklistModifier =
            (::GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        if (closest.IsValid()) {
            if (blacklistModifier) {
                ToggleBlacklist(closest.NetworkId());
            } else {
                SetSelected(closest);
            }
        } else if (!blacklistModifier) {
            // Clicking empty space clears the selected identity.  The
            // selector may still explain that identity elsewhere, but it can
            // no longer influence a new request as a manual preference.
            selected_ = {};
        }
    }

    void CycleSelection() {
        std::vector<::SDK::AIHeroClient> heroes;
        for (const auto& hero : ::SDK::GameObjects::EnemyHeroes()) {
            if (hero.IsValid() && !hero.IsDead() && hero.IsVisible() &&
                hero.IsTargetable() && !IsBlacklisted(hero.NetworkId())) {
                heroes.push_back(hero);
            }
        }
        if (heroes.empty()) {
            selected_ = {};
            return;
        }

        std::size_t next = 0;
        for (std::size_t i = 0; i < heroes.size(); ++i) {
            if (selected_.Compare(heroes[i])) {
                next = (i + 1) % heroes.size();
                break;
            }
        }
        selected_ = heroes[next];
    }

    ::SDK::Menu* parent_ = nullptr;
    ::SDK::Menu* native_ = nullptr;
    ::SDK::Menu* blacklistMenu_ = nullptr;
    ::SDK::Menu* priorityMenu_ = nullptr;
    ::SDK::AIHeroClient selected_ = {};
    std::unordered_set<int> blacklisted_;
    std::unordered_map<int, int> priorities_;
    std::unordered_map<int, ::SDK::MenuBool*> blacklistItems_;
    TargetSnapshot diagnosticSnapshot_ = {};
    std::vector<TargetDecision> diagnosticDecisions_;
    bool suspended_ = true;
};

} // namespace Plugins::KuroTargetSelector
