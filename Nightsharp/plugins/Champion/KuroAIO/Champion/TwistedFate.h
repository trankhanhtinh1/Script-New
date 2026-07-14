#pragma once

#include "../Helper/KuroAIOCommon.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::TwistedFate {

enum class Card {
    Blue = 0,
    Red = 1,
    Gold = 2,
    None = 3,
};

enum class SelectStatus {
    Ready,
    Selecting,
    Selected,
    Cooldown,
};

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* ClearMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 1450.0f };
inline Spell W{ SpellSlot::W, 850.0f };
inline Spell E{ SpellSlot::E };
inline Spell R{ SpellSlot::R, 5500.0f };

inline Card CardSelected = Card::None;
inline Card CardSelecting = Card::None;
inline SelectStatus Status = SelectStatus::Cooldown;
inline int LastWTime = 0;
inline int ForceGoldUntil = 0;
inline bool Loaded = false;

static std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

static bool ContainsIgnoreCase(const std::string& value, const char* needle) {
    if (!needle || !needle[0]) {
        return false;
    }
    return Lower(value).find(Lower(needle)) != std::string::npos;
}

static Card CardFromName(const std::string& spellName) {
    if (ContainsIgnoreCase(spellName, "blue")) {
        return Card::Blue;
    }
    if (ContainsIgnoreCase(spellName, "red")) {
        return Card::Red;
    }
    if (ContainsIgnoreCase(spellName, "gold") ||
        ContainsIgnoreCase(spellName, "yellow")) {
        return Card::Gold;
    }
    return Card::None;
}

static Card SelectedCardFromBuffs() {
    const auto player = Player();
    if (!player.IsValid()) {
        return Card::None;
    }
    if (player.HasBuff("BlueCardPreAttack")) {
        return Card::Blue;
    }
    if (player.HasBuff("RedCardPreAttack")) {
        return Card::Red;
    }
    if (player.HasBuff("GoldCardPreAttack")) {
        return Card::Gold;
    }
    return Card::None;
}

static bool SelectedAnyCard() {
    return SelectedCardFromBuffs() != Card::None;
}

static bool IsPickACard(const std::string& spellName) {
    return EqualsIgnoreCase(spellName.c_str(), "PickACard") ||
           ContainsIgnoreCase(spellName, "pickacard");
}

static Card CardFromMenu(Menu* menu, const char* key, Card fallback) {
    switch (List(menu, key, static_cast<int>(fallback))) {
    case 0:
        return Card::Blue;
    case 1:
        return Card::Red;
    default:
        return Card::Gold;
    }
}

static Card ComboCard() {
    return CardFromMenu(ComboMenu, "ComboCard", Card::Gold);
}

static Card FarmCard() {
    return CardFromMenu(ClearMenu, "FarmCard", Card::Red);
}

static void UpdateCardState() {
    const auto player = Player();
    if (!player.IsValid()) {
        Status = SelectStatus::Cooldown;
        CardSelected = Card::None;
        CardSelecting = Card::None;
        return;
    }

    const std::string wName = W.Instance().Name();
    const bool hasTracker = player.HasBuff("pickacard_tracker");
    const Card selectedFromBuff = SelectedCardFromBuffs();

    CardSelected = selectedFromBuff;
    CardSelecting = hasTracker ? CardFromName(wName) : Card::None;

    if (selectedFromBuff != Card::None) {
        Status = SelectStatus::Selected;
    } else if (hasTracker) {
        Status = SelectStatus::Selecting;
    } else if (W.IsReady() && IsPickACard(wName)) {
        Status = SelectStatus::Ready;
    } else {
        Status = SelectStatus::Cooldown;
    }
}

static bool LetSelect(Card card) {
    if (card == Card::None ||
        Status == SelectStatus::Cooldown ||
        Status == SelectStatus::Selected) {
        return false;
    }

    const int now = SDK::Variables::TickCount();
    if (now - LastWTime <= 150) {
        return false;
    }

    if (Status == SelectStatus::Selecting) {
        if (Player().HasBuff("pickacard_tracker") && CardSelecting == card) {
            LastWTime = now;
            if (W.Cast()) {
                Status = SelectStatus::Selected;
                return true;
            }
        }
        return false;
    }

    if (Status == SelectStatus::Ready &&
        IsPickACard(W.Instance().Name()) &&
        !Player().HasBuff("pickacard_tracker")) {
        LastWTime = now;
        if (W.Cast()) {
            Status = SelectStatus::Selecting;
            return true;
        }
    }
    return false;
}

static AIBaseClient OrbwalkerHeroTarget(float range) {
    const AttackableUnit target = Orbwalker::GetTarget();
    if (!target.IsValid() ||
        target.Type() != ::Core::Objects::ObjectType::AIHeroClient) {
        return {};
    }

    const AIBaseClient hero(target.Handle());
    return ValidTarget(hero, range) ? hero : AIBaseClient();
}

static bool CastQ(const AIBaseClient& target) {
    return Q.IsReady() &&
           ValidTarget(target, Q.Range) &&
           Q.CastIfHitchanceMinimum(target, HitChance::High) ==
               CastStates::SuccessfullyCasted;
}

static void Combo() {
    if (!IsComboMode() || Orbwalker::IsWindingUp()) {
        return;
    }

    const bool useW = Bool(ComboMenu, "WCombo", true);
    if (useW && W.IsReady()) {
        (void)LetSelect(ComboCard());
    }

    if (!Bool(ComboMenu, "QCombo", true) || !Q.IsReady()) {
        return;
    }

    const AIBaseClient orbTarget = OrbwalkerHeroTarget(Q.Range);
    if (!orbTarget.IsValid()) {
        // As in the reference: without an orbwalker hero target, use the
        // KuroAIO target helper and allow Q while W is being prepared.
        (void)CastQ(GetMagicalTarget(Q.Range - 100.0f));
        return;
    }

    // Preserve the reference W -> empowered AA -> Q cadence. If W is disabled,
    // Q remains independently usable instead of being gated by card state.
    if (!useW || (!W.IsReady() && !SelectedAnyCard())) {
        (void)CastQ(orbTarget);
    }
}

static std::vector<AIBaseClient> ClearUnitsInRange(float range) {
    std::vector<AIBaseClient> result;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion, range)) {
            result.emplace_back(minion.Handle());
        }
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (ValidTarget(monster, range)) {
            result.emplace_back(monster.Handle());
        }
    }
    return result;
}

static void Clear() {
    if (!IsClearMode() || !Bool(ClearMenu, "WClear", true)) {
        return;
    }

    const auto units = ClearUnitsInRange(W.Range);
    if (units.size() == 1) {
        const auto player = Player();
        const auto& unit = units.front();
        if (unit.Health() <= 1.5f * Damage::GetAutoAttackDamage(player, unit)) {
            return;
        }
    }

    if (W.IsReady() && Orbwalker::GetTarget().IsValid()) {
        (void)LetSelect(FarmCard());
    }
}

static void OnBeforeAttack(OrbwalkingActionArgs&) {
    // Orbwalker callbacks may run before this module's update callback in the
    // same frame, so refresh the live W/buff state before trying to lock.
    UpdateCardState();
    if (!IsComboMode() ||
        !Bool(ComboMenu, "WCombo", true) ||
        !W.IsReady() ||
        Status != SelectStatus::Selecting) {
        return;
    }
    (void)LetSelect(ComboCard());
}

static bool IsGateCast(const Events::ProcessSpellEventArgs& args) {
    return EqualsIgnoreCase(args.SpellName, "Gate") ||
           EqualsIgnoreCase(args.ScriptName, "Gate") ||
           EqualsIgnoreCase(args.SpellSlotName, "Gate") ||
           EqualsIgnoreCase(args.PayloadSpellName, "Gate");
}

static void OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }

    const int now = SDK::Variables::TickCount();
    if (args.Slot == static_cast<int>(SpellSlot::W)) {
        LastWTime = now - SDK::Game::Ping() / 2;
    }

    if (args.Slot == static_cast<int>(SpellSlot::R) && IsGateCast(args)) {
        ForceGoldUntil = now + 10000;
        UpdateCardState();
        if (W.IsReady()) {
            (void)LetSelect(Card::Gold);
        }
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    UpdateCardState();

    const int now = SDK::Variables::TickCount();
    if (ForceGoldUntil > now) {
        if (SelectedAnyCard() || !W.IsReady()) {
            ForceGoldUntil = 0;
        } else {
            (void)LetSelect(Card::Gold);
        }
    }

    Combo();
    Clear();
}

static void CycleCard(MenuItem* sender, void* userData) {
    auto* key = static_cast<MenuKeyBind*>(sender);
    auto* list = static_cast<MenuList*>(userData);
    if (!key || !list || !key->Active || list->Options.empty()) {
        return;
    }
    list->Set((list->Index + 1) % static_cast<int>(list->Options.size()));
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.kuroaio.twistedfate", "Kuro - Twisted Fate", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo Settings"));
    ComboMenu->Add(new MenuBool("QCombo", "Use Q in Combo", true));
    ComboMenu->Add(new MenuBool("WCombo", "Use W in Combo", true));
    auto* comboCard = ComboMenu->Add(new MenuList(
        "ComboCard", "Combo Card", { "Blue", "Red", "Gold" }, 2));
    comboCard->Permashow();
    auto* comboCycle = ComboMenu->Add(new MenuKeyBind(
        "ChangeComboCard", "Change Combo Card", SDK::Keys::A, KeyBindType::Press));
    comboCycle->ValueChanged = &CycleCard;
    comboCycle->ValueChangedUd = comboCard;
    comboCycle->Permashow();

    ClearMenu = MenuRoot->AddSubMenu(new Menu("Clear", "Clear Settings"));
    ClearMenu->Add(new MenuBool("WClear", "Use W in Clear", true));
    auto* farmCard = ClearMenu->Add(new MenuList(
        "FarmCard", "Farm Card", { "Blue", "Red", "Gold" }, 1));
    farmCard->Permashow();
    auto* farmCycle = ClearMenu->Add(new MenuKeyBind(
        "ChangeFarmCard", "Change Farm Card", SDK::Keys::T, KeyBindType::Press));
    farmCycle->ValueChanged = &CycleCard;
    farmCycle->ValueChangedUd = farmCard;
    farmCycle->Permashow();

    MenuRoot->Attach();
}

static void RemoveMenu() {
    if (!MenuRoot) {
        return;
    }

    if (auto* item = ComboMenu ? ComboMenu->Get<MenuList>("ComboCard") : nullptr) {
        item->RemovePermashow();
    }
    if (auto* item = ComboMenu ? ComboMenu->Get<MenuKeyBind>("ChangeComboCard") : nullptr) {
        item->RemovePermashow();
    }
    if (auto* item = ClearMenu ? ClearMenu->Get<MenuList>("FarmCard") : nullptr) {
        item->RemovePermashow();
    }
    if (auto* item = ClearMenu ? ClearMenu->Get<MenuKeyBind>("ChangeFarmCard") : nullptr) {
        item->RemovePermashow();
    }

    MenuManager::Instance().Remove(MenuRoot);
    MenuRoot = nullptr;
    ComboMenu = nullptr;
    ClearMenu = nullptr;
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 1450.0f);
    Q.SetSkillshot(0.25f, 20.0f, 1000.0f, false, SkillshotType::SkillshotLine);
    W = Spell(SpellSlot::W, 850.0f);
    E = Spell(SpellSlot::E);
    R = Spell(SpellSlot::R, 5500.0f);

    CardSelected = Card::None;
    CardSelecting = Card::None;
    Status = SelectStatus::Cooldown;
    LastWTime = 0;
    ForceGoldUntil = 0;

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>Kuro - Twisted Fate loaded</font>");
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;

    RemoveMenu();
    ForceGoldUntil = 0;
    Loaded = false;
}

} // namespace Plugins::KuroAIO::TwistedFate
