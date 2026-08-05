#pragma once

#include "KuroTargetSelectorContracts.h"
#include "KuroTargetSelectorPolicy.h"

#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/UI/UI.h"
#include <array>
#include <cstring>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
// Copied from TargetSelectorImpulse so Kuro keeps the same champion
// priority defaults without depending on the Impulse selector header.
namespace Plugins::KuroTargetSelectorPriorityData {

inline constexpr std::array<const char*, 63> PriorityFive{{
    "Ahri", "Akshan", "Anivia", "Annie", "Aphelios", "Ashe",
    "AurelionSol", "Aurora", "Azir", "Brand", "Caitlyn", "Cassiopeia",
    "Corki", "Draven", "Ezreal", "Graves", "Hwei", "Jhin", "Jinx",
    "Kaisa", "Kalista", "Karma", "Karthus", "Katarina", "Kennen",
    "Kindred", "KogMaw", "Leblanc", "Lucian", "Lux", "Malzahar",
    "MasterYi", "Mel", "MissFortune", "Neeko", "Orianna", "Qiyana",
    "Quinn", "Samira", "Sivir", "Smolder", "Soraka", "Sylas", "Syndra",
    "Taliyah", "Talon", "Teemo", "Tristana", "TwistedFate", "Twitch",
    "Varus", "Vayne", "Veigar", "Velkoz", "Vex", "Viktor", "Xayah",
    "Xerath", "Yunara", "Zed", "Zeri", "Ziggs", "Zoe"
}};

inline constexpr std::array<const char*, 30> PriorityFour{{
    "Akali", "Belveth", "Briar", "Camille", "Diana", "Ekko",
    "FiddleSticks", "Fiora", "Fizz", "Gwen", "Heimerdinger", "Jayce",
    "Kassadin", "Kayle", "Kayn", "KhaZix", "Lissandra", "Locke",
    "Mordekaiser", "Naafiri", "Nidalee", "Nilah", "Riven", "Senna",
    "Shaco", "Viego", "Vladimir", "Yasuo", "Yone", "Zilean"
}};

inline constexpr std::array<const char*, 33> PriorityThree{{
    "Aatrox", "Ambessa", "Darius", "Elise", "Evelynn", "Galio",
    "Gangplank", "Gragas", "Illaoi", "Irelia", "Jax", "Kled", "LeeSin",
    "Lillia", "Maokai", "Morgana", "Nocturne", "Pantheon", "Poppy",
    "Pyke", "RekSai", "Rengar", "Rumble", "Ryze", "Sett", "Swain",
    "Trundle", "Tryndamere", "Udyr", "Urgot", "Vi", "XinZhao", "Zaahen"
}};

inline constexpr std::array<const char*, 47> PriorityTwo{{
    "Alistar", "Amumu", "Bard", "Blitzcrank", "Braum", "Chogath",
    "DrMundo", "Garen", "Gnar", "Hecarim", "Ivern", "Janna", "JarvanIV",
    "KSante", "Leona", "Lulu", "Malphite", "Milio", "MonkeyKing", "Nami",
    "Nasus", "Nautilus", "Nunu", "Olaf", "Ornn", "Rakan", "Rammus",
    "Rell", "Renata", "Renekton", "Sejuani", "Seraphine", "Shen",
    "Shyvana", "Singed", "Sion", "Skarner", "Sona", "TahmKench", "Taric",
    "Thresh", "Volibear", "Warwick", "Yorick", "Yuumi", "Zac", "Zyra"
}};

inline int GetDefaultPriority(const std::string& alias) {
    const auto matches = [&alias](const auto& names) {
        for (const char* name : names) {
            if (_stricmp(name, alias.c_str()) == 0) return true;
        }
        return false;
    };
    if (matches(PriorityFive)) return 5;
    if (matches(PriorityFour)) return 4;
    if (matches(PriorityThree)) return 3;
    if (matches(PriorityTwo)) return 2;
    return 1;
}

} // namespace Plugins::KuroTargetSelectorPriorityData


namespace Plugins::KuroTargetSelector {

// UI state is intentionally kept separate from the selector implementation.
// This lets the service remain useful to consumers that never attach a menu,
// while keeping all input and persistence concerns in one small owner.
class Menu final {
public:
    explicit Menu(::SDK::Menu* parent, bool parentIsRoot = false)
        : parent_(parent) {
        if (parent_) {
            native_ = parentIsRoot
                ? parent_
                : parent_->AddSubMenu(
                    new ::SDK::Menu(
                        "KuroTargetSelector", "Kuro Target Selector"));
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
        if (Ptr() && Ptr() != this) Ptr()->Suspend();
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

    bool AutomaticProfile() const {
        const auto* item = native_
            ? native_->Get<::SDK::MenuList>("Profile")
            : nullptr;
        return !item || item->Index == 0;
    }

    TargetProfile Profile() const {
        const auto* item = native_
            ? native_->Get<::SDK::MenuList>("Profile")
            : nullptr;
        const int index = item ? item->Index : 0;
        const int max = static_cast<int>(TargetProfile::FleeThreat);
        if (index == max + 1) return TargetProfile::General;
        return static_cast<TargetProfile>(std::clamp(index, 0, max));
    }

    void SetSelected(const ::SDK::AIHeroClient& target) {
        if (!target.IsValid()) {
            selectedNetworkId_ = 0;
            return;
        }
        const int networkId = target.NetworkId();
        selectedNetworkId_ = networkId > 0 ? networkId : 0;
    }

    ::SDK::AIHeroClient Selected() const {
        return selectedNetworkId_ > 0
            ? ::SDK::GameObjects::GetUnitByNetworkId<
                ::SDK::AIHeroClient>(selectedNetworkId_)
            : ::SDK::AIHeroClient();
    }

    bool Blacklist(int networkId) const {
        return IsBlacklisted(networkId);
    }

    bool IsBlacklisted(int networkId) const {
        if (networkId <= 0) return false;
        const auto item = blacklistItems_.find(networkId);
        if (item != blacklistItems_.end() && item->second) {
            return item->second->Value;
        }
        return blacklisted_.find(networkId) != blacklisted_.end();
    }

    int Priority(int networkId) const {
        if (priorityMenu_ && networkId > 0) {
            const std::string key = "Target_" + std::to_string(networkId);
            if (const auto* item = priorityMenu_->Get<::SDK::MenuSlider>(key.c_str())) {
                return std::clamp(item->Value, 1, 5);
            }
        }
        const auto it = priorities_.find(networkId);
        if (it != priorities_.end()) {
            return std::clamp(it->second, 1, 5);
        }
        if (networkId > 0) {
            const auto target =
                ::SDK::GameObjects::GetUnitByNetworkId<::SDK::AIHeroClient>(
                    networkId);
            if (target.IsValid()) {
                return ::Plugins::KuroTargetSelectorPriorityData::GetDefaultPriority(
                    target.CharacterName());
            }
        }
        return 1;
    }

    bool ToggleBlacklist(int networkId) {
        if (networkId <= 0) return false;

        const bool enabled = !IsBlacklisted(networkId);
        if (enabled) {
            blacklisted_.insert(networkId);
        } else {
            blacklisted_.erase(networkId);
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

        if (args.Msg == WM_LBUTTONDBLCLK) {
            self->SelectClosestToCursor(false);
            return;
        }

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
            {"Automatic", "AutoAttack", "Burst", "DPS", "Poke", "Execute",
             "Peel", "Interrupt", "AntiGapcloser", "FleeThreat", "General"},
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

        bool selectedIsLive = selectedNetworkId_ <= 0;
        for (const auto& hero : ::SDK::GameObjects::EnemyHeroes()) {
            if (!hero.IsValid() || hero.NetworkId() <= 0) continue;
            const int networkId = hero.NetworkId();
            if (priorities_.find(networkId) == priorities_.end() ||
                blacklistItems_.find(networkId) == blacklistItems_.end()) {
                EnsureHeroEntries(hero);
            }
            selectedIsLive = selectedIsLive || networkId == selectedNetworkId_;
        }
        if (!selectedIsLive && selectedNetworkId_ > 0) {
            const auto selUnit = ::SDK::GameObjects::GetUnitByNetworkId<::SDK::AIHeroClient>(selectedNetworkId_);
            if (!selUnit.IsValid() || selUnit.IsDead()) {
                selectedNetworkId_ = 0;
            }
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
            priorities_.emplace(
                networkId,
                ::Plugins::KuroTargetSelectorPriorityData::GetDefaultPriority(
                    hero.CharacterName()));
        }
        if (!priorityMenu_->Get<::SDK::MenuSlider>(priorityKey.c_str())) {
            priorityMenu_->Add(new ::SDK::MenuSlider(
                priorityKey.c_str(), label.c_str(), Priority(networkId), 1, 5));
        }
        auto* blacklistItem =
            blacklistMenu_->Get<::SDK::MenuBool>(blacklistKey.c_str());
        if (!blacklistItem) {
            blacklistItem = blacklistMenu_->Add(new ::SDK::MenuBool(
                blacklistKey.c_str(), label.c_str(),
                blacklisted_.find(networkId) != blacklisted_.end()));
        }
        blacklistItems_[networkId] = blacklistItem;
    }

    bool BoolValue(const char* key, bool fallback) const {
        const auto* item = native_ ? native_->Get<::SDK::MenuBool>(key) : nullptr;
        return item ? item->Value : fallback;
    }

    int SliderValue(const char* key, int fallback) const {
        const auto* item = native_ ? native_->Get<::SDK::MenuSlider>(key) : nullptr;
        return item ? item->Value : fallback;
    }

    void SelectClosestToCursor(bool allowBlacklistModifier = true) {
        const ::SDK::Vector3 cursor = ::SDK::Game::CursorPos();
        const bool blacklistModifier = allowBlacklistModifier &&
            (::GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        ::SDK::AIHeroClient closest;
        float closestDistance = 180.0f;
        for (const auto& hero : ::SDK::GameObjects::EnemyHeroes()) {
            if (!hero.IsValid() || !hero.IsVisible() ||
                !hero.IsTargetable() ||
                (!blacklistModifier && IsBlacklisted(hero.NetworkId()))) {
                continue;
            }
            const float distance = hero.Distance(cursor);
            if (distance <= closestDistance) {
                closestDistance = distance;
                closest = hero;
            }
        }
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
            selectedNetworkId_ = 0;
        }
    }

    void CycleSelection() {
        std::vector<::SDK::AIHeroClient> heroes;
        for (const auto& hero : ::SDK::GameObjects::EnemyHeroes()) {
            if (hero.IsValid() && hero.IsVisible() &&
                hero.IsTargetable() && !IsBlacklisted(hero.NetworkId())) {
                heroes.push_back(hero);
            }
        }
        if (heroes.empty()) {
            selectedNetworkId_ = 0;
            return;
        }

        std::size_t next = 0;
        for (std::size_t i = 0; i < heroes.size(); ++i) {
            if (selectedNetworkId_ == heroes[i].NetworkId()) {
                next = (i + 1) % heroes.size();
                break;
            }
        }
        selectedNetworkId_ = heroes[next].NetworkId();
    }

    ::SDK::Menu* parent_ = nullptr;
    ::SDK::Menu* native_ = nullptr;
    ::SDK::Menu* blacklistMenu_ = nullptr;
    ::SDK::Menu* priorityMenu_ = nullptr;
    int selectedNetworkId_ = 0;
    std::unordered_set<int> blacklisted_;
    std::unordered_map<int, int> priorities_;
    std::unordered_map<int, ::SDK::MenuBool*> blacklistItems_;
    bool suspended_ = true;
};

} // namespace Plugins::KuroTargetSelector
