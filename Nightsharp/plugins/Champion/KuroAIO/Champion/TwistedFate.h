#pragma once

#include "../Helper/KuroAIOCommon.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::TwistedFate {

enum class Card {
    Blue,
    Red,
    Gold,
    None,
};

enum class SelectStatus {
    Ready,
    Selecting,
    Selected,
    Cooldown,
};

inline Menu* MenuRoot = nullptr;
inline Menu* CardMenu = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* AlwaysGoldMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* ClearMenu = nullptr;
inline Menu* KillstealMenu = nullptr;
inline Menu* DrawMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 1450.0f };
inline Spell W{ SpellSlot::W };
inline Spell E{ SpellSlot::E };
inline Spell R{ SpellSlot::R, 5500.0f };

inline Card DesiredCard = Card::None;
inline Card CyclingCard = Card::None;
inline Card LockedCard = Card::None;
inline SelectStatus Status = SelectStatus::Cooldown;
inline int LastWAction = 0;
inline int ForceGoldUntil = 0;
inline bool Loaded = false;

static std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

static bool ContainsIgnoreCase(const std::string& value, const char* needle) {
    return needle && needle[0] && Lower(value).find(Lower(needle)) != std::string::npos;
}

static Card CardFromName(const std::string& name) {
    if (ContainsIgnoreCase(name, "blue")) {
        return Card::Blue;
    }
    if (ContainsIgnoreCase(name, "red")) {
        return Card::Red;
    }
    if (ContainsIgnoreCase(name, "gold") || ContainsIgnoreCase(name, "yellow")) {
        return Card::Gold;
    }
    return Card::None;
}

static bool IsPickACard(const std::string& name) {
    return EqualsIgnoreCase(name.c_str(), "PickACard") || ContainsIgnoreCase(name, "pickacard");
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

static bool HasEmpoweredE() {
    const auto player = Player();
    return player.IsValid() &&
           (player.HasBuff("cardmasterstackparticle") || player.HasBuff("CardMasterStackParticle"));
}

static void UpdateCardState() {
    const auto player = Player();
    if (!player.IsValid()) {
        DesiredCard = Card::None;
        CyclingCard = Card::None;
        LockedCard = Card::None;
        Status = SelectStatus::Cooldown;
        return;
    }

    LockedCard = SelectedCardFromBuffs();
    const bool selecting = player.HasBuff("pickacard_tracker");
    CyclingCard = selecting ? CardFromName(W.Instance().Name()) : Card::None;

    if (LockedCard != Card::None) {
        Status = SelectStatus::Selected;
        DesiredCard = Card::None;
    } else if (selecting) {
        Status = SelectStatus::Selecting;
    } else if (W.IsReady() && IsPickACard(W.Instance().Name())) {
        Status = SelectStatus::Ready;
    } else {
        Status = SelectStatus::Cooldown;
        DesiredCard = Card::None;
    }
}

static bool TryCardAction() {
    if (DesiredCard == Card::None || Status == SelectStatus::Cooldown ||
        Status == SelectStatus::Selected) {
        return false;
    }

    const int now = SDK::Variables::TickCount();
    const int delay = 125 + SDK::Game::Ping() / 2;
    if (now - LastWAction < delay) {
        return false;
    }

    if (Status == SelectStatus::Ready && IsPickACard(W.Instance().Name())) {
        LastWAction = now;
        if (W.Cast()) {
            Status = SelectStatus::Selecting;
            return true;
        }
        return false;
    }

    if (Status == SelectStatus::Selecting && CyclingCard == DesiredCard) {
        LastWAction = now;
        if (W.Cast()) {
            Status = SelectStatus::Selected;
            return true;
        }
    }
    return false;
}

static bool SelectCard(Card card) {
    if (card == Card::None || LockedCard != Card::None) {
        return false;
    }
    DesiredCard = card;
    return TryCardAction();
}

static std::string EnemyMenuKey(const AIHeroClient& enemy) {
    return "AlwaysGold." + std::to_string(enemy.NetworkId());
}

static bool AlwaysGold(const AIHeroClient& enemy) {
    if (!AlwaysGoldMenu || !enemy.IsValid()) {
        return false;
    }
    const std::string key = EnemyMenuKey(enemy);
    const auto* item = AlwaysGoldMenu->Get<MenuBool>(key.c_str());
    return item && item->Value;
}

static bool HasHardCrowdControl(const AIBaseClient& target) {
    return SDK::HasBuffOfType(target, SDK::BuffType::Stun) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Snare) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Taunt) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Fear) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Charm) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Suppression) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Knockup) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Knockback) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Asleep);
}

static float EffectiveMagicalHealth(const AIBaseClient& target) {
    return target.Health() + target.AllShield() + target.MagicalShield();
}

static SDK::DamageStage CardStage(Card card) {
    switch (card) {
    case Card::Red:
        return SDK::DamageStage::Detonation;
    case Card::Gold:
        return SDK::DamageStage::Empowered;
    default:
        return SDK::DamageStage::Default;
    }
}

static float CardDamage(const AIBaseClient& target, Card card) {
    if (!target.IsValid() || card == Card::None) {
        return 0.0f;
    }
    float damage = W.GetDamage(target, CardStage(card));
    if (HasEmpoweredE()) {
        damage += E.GetDamage(target);
    }
    return damage;
}

static int CountEnemiesNear(const Vector3& position, float range) {
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy) && enemy.Position().Distance2D(position) <= range) {
            ++count;
        }
    }
    return count;
}

static AIHeroClient HighestPriorityTarget(float extraRange = 300.0f) {
    const auto player = Player();
    if (!player.IsValid()) {
        return {};
    }

    AIHeroClient result;
    int bestPriority = -1;
    float bestDistance = FLT_MAX;
    auto* selector = SDK::TargetSelector::Instance();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const float range = AutoAttack::GetRealAutoAttackRange(player, enemy) + extraRange;
        if (!ValidHeroTarget(enemy, range)) {
            continue;
        }
        const int priority = selector ? selector->GetPriority(enemy) : 1;
        const float distance = enemy.DistanceToPlayer();
        if (priority > bestPriority || (priority == bestPriority && distance < bestDistance)) {
            result = enemy;
            bestPriority = priority;
            bestDistance = distance;
        }
    }
    return result;
}

static bool CastQ(const AIBaseClient& target, HitChance chance = HitChance::High) {
    if (!Q.IsReady() || !ValidTarget(target, Q.Range)) {
        return false;
    }
    const auto prediction = Q.GetPrediction(target);
    if (prediction.Hitchance < chance ||
        Collisions::HasYasuoWindWallCollision(Player().ServerPosition(), prediction.GetCastPosition())) {
        return false;
    }
    return Q.Cast(prediction.GetCastPosition());
}

static Card FixedCard(Menu* menu, const char* key) {
    switch (List(menu, key, 0)) {
    case 1:
        return Card::Blue;
    case 2:
        return Card::Red;
    case 3:
        return Card::Gold;
    default:
        return Card::None;
    }
}

static Card SmartComboCard(const AIHeroClient& target) {
    if (!target.IsValid()) {
        return Card::None;
    }
    const bool blueOnlyKill = Bool(ComboMenu, "BlueOnlyKill", true);
    if (CardDamage(target, Card::Blue) >= EffectiveMagicalHealth(target)) {
        return Card::Blue;
    }
    if (AlwaysGold(target)) {
        return Card::Gold;
    }
    if (!blueOnlyKill && Player().ManaPercent() <= Slider(ComboMenu, "BlueMana", 25)) {
        return Card::Blue;
    }
    if (Player().CountAllyHeroesInRange(1200.0f) >= Player().CountEnemyHeroesInRange(1200.0f)) {
        return Card::Gold;
    }
    if (CountEnemiesNear(target.Position(), 175.0f) >= Slider(ComboMenu, "RedEnemies", 3)) {
        return Card::Red;
    }
    return Card::Gold;
}

static Card SmartHarassCard(const AIHeroClient& target) {
    if (!target.IsValid()) {
        return Card::None;
    }
    if (Player().ManaPercent() <= Slider(HarassMenu, "BlueMana", 25) || HasEmpoweredE()) {
        return Card::Blue;
    }
    if (Q.IsReady()) {
        return Card::Gold;
    }

    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion, AutoAttack::GetRealAutoAttackRange(Player(), minion)) &&
            minion.Position().Distance2D(target.Position()) <= 175.0f) {
            return Card::Red;
        }
    }
    return Card::Gold;
}

static Card ComboCard(const AIHeroClient& target) {
    const Card fixed = FixedCard(ComboMenu, "CardMode");
    return fixed == Card::None ? SmartComboCard(target) : fixed;
}

static Card HarassCard(const AIHeroClient& target) {
    const Card fixed = FixedCard(HarassMenu, "CardMode");
    return fixed == Card::None ? SmartHarassCard(target) : fixed;
}

static void AttackSelectedTarget(const AIHeroClient& target) {
    if (LockedCard != Card::None && ValidHeroTarget(target) &&
        AutoAttack::InAutoAttackRange(target) && Orbwalker::CanAttack()) {
        (void)Orbwalker::Attack(target);
    }
}

static void Combo() {
    if (!IsComboMode() || Orbwalker::IsWindingUp()) {
        return;
    }

    const auto target = HighestPriorityTarget();
    if (Bool(ComboMenu, "UseW", true) && target.IsValid()) {
        (void)SelectCard(ComboCard(target));
        AttackSelectedTarget(target);
    }

    if (!Bool(ComboMenu, "UseQ", true) || !Q.IsReady()) {
        return;
    }
    const auto qTarget = GetMagicalTarget(Q.Range);
    if (!qTarget.IsValid()) {
        return;
    }
    if (!Bool(ComboMenu, "QOnlyCC", true) || HasHardCrowdControl(qTarget)) {
        (void)CastQ(qTarget);
    }
}

static void Harass() {
    if (!IsHarassMode() || Player().ManaPercent() < Slider(HarassMenu, "Mana", 45) ||
        Orbwalker::IsWindingUp()) {
        return;
    }

    const auto target = HighestPriorityTarget();
    if (Bool(HarassMenu, "UseW", true) && target.IsValid()) {
        (void)SelectCard(HarassCard(target));
        AttackSelectedTarget(target);
    }

    if (Bool(HarassMenu, "UseQ", true) && Q.IsReady()) {
        const auto qTarget = GetMagicalTarget(Q.Range);
        if (qTarget.IsValid() &&
            (!Bool(HarassMenu, "QOnlyCC", false) || HasHardCrowdControl(qTarget))) {
            (void)CastQ(qTarget);
        }
    }
}

static std::vector<AIBaseClient> LaneMinions(float range) {
    std::vector<AIBaseClient> result;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion, range)) {
            result.emplace_back(minion.Handle());
        }
    }
    return result;
}

static int CountRedFarmTargets(const AIBaseClient& center) {
    int count = 0;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion) && minion.Position().Distance2D(center.Position()) <= 175.0f &&
            CardDamage(minion, Card::Red) >= minion.Health()) {
            ++count;
        }
    }
    return count;
}

static Card LaneCard(const AIBaseClient& target) {
    switch (List(ClearMenu, "CardMode", 0)) {
    case 1:
        return Card::Blue;
    case 2:
        return Card::Red;
    default:
        if (Player().ManaPercent() <= Slider(ClearMenu, "BlueMana", 45)) {
            return Card::Blue;
        }
        if (target.IsValid() && CountRedFarmTargets(target) >= 3) {
            return Card::Red;
        }
        return Card::None;
    }
}

static void LaneClear() {
    if (!IsClearMode()) {
        return;
    }

    if (Bool(ClearMenu, "UseQ", true) && Q.IsReady() &&
        Player().ManaPercent() >= Slider(ClearMenu, "QMana", 25)) {
        const auto minions = LaneMinions(Q.Range);
        const auto farm = Q.GetLineFarmLocation(minions);
        if (farm.MinionsHit >= Slider(ClearMenu, "QHits", 3)) {
            (void)Q.Cast(farm.Position);
        }
    }

    if (!Bool(ClearMenu, "UseW", true) || !W.IsReady()) {
        return;
    }
    const AttackableUnit orbTarget = Orbwalker::GetTarget();
    if (!orbTarget.IsValid() || orbTarget.Type() != ::Core::Objects::ObjectType::AIMinionClient) {
        return;
    }
    const AIBaseClient minion(orbTarget.Handle());
    const Card card = LaneCard(minion);
    if (card != Card::None) {
        (void)SelectCard(card);
    }
}

static void Killsteal() {
    for (const auto& enemy : EnemyHeroesByHealth(Q.Range)) {
        if (Bool(KillstealMenu, "UseQ", true) && Q.IsReady() &&
            Q.GetDamage(enemy) >= EffectiveMagicalHealth(enemy) && CastQ(enemy, HitChance::High)) {
            return;
        }

        if (Bool(KillstealMenu, "UseW", true) && W.IsReady() &&
            ValidHeroTarget(enemy, AutoAttack::GetRealAutoAttackRange(Player(), enemy)) &&
            CardDamage(enemy, Card::Blue) >= EffectiveMagicalHealth(enemy)) {
            (void)SelectCard(Card::Blue);
            if (LockedCard == Card::Blue && Orbwalker::CanAttack()) {
                (void)Orbwalker::Attack(enemy);
            }
            return;
        }
    }
}

static void AutoQ() {
    if (!Bool(MiscMenu, "AutoQCC", true) || !Q.IsReady()) {
        return;
    }
    for (const auto& enemy : EnemyHeroes(Q.Range)) {
        const bool slowed = SDK::HasBuffOfType(enemy, SDK::BuffType::Slow);
        if (HasHardCrowdControl(enemy)) {
            (void)CastQ(enemy, HitChance::Immobile);
            return;
        }
        if (slowed && CastQ(enemy, HitChance::VeryHigh)) {
            return;
        }
    }
}

static void HandleCardHotkeys() {
    if (Key(CardMenu, "GoldKey")) {
        (void)SelectCard(Card::Gold);
    } else if (Key(CardMenu, "BlueKey")) {
        (void)SelectCard(Card::Blue);
    } else if (Key(CardMenu, "RedKey")) {
        (void)SelectCard(Card::Red);
    }
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

    if (args.Slot == static_cast<int>(SpellSlot::W)) {
        LastWAction = SDK::Variables::TickCount() - SDK::Game::Ping() / 2;
    }
    if (IsGateCast(args) && Bool(MiscMenu, "GoldAfterGate", true)) {
        ForceGoldUntil = SDK::Variables::TickCount() + 10000;
        DesiredCard = Card::Gold;
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    UpdateCardState();
    if (ForceGoldUntil > SDK::Variables::TickCount()) {
        if (LockedCard != Card::None || !W.IsReady()) {
            ForceGoldUntil = 0;
        } else {
            (void)SelectCard(Card::Gold);
        }
    } else {
        ForceGoldUntil = 0;
    }

    HandleCardHotkeys();
    (void)TryCardAction();
    Killsteal();
    AutoQ();
    Combo();
    Harass();
    LaneClear();
}

static void Drawing_OnDraw() {
    if (!Player().IsValid() || Player().IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "DrawQ", false)) {
        Drawing::DrawCircle(Player().Position(), Q.Range, 0xFF7856BEu, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawR", false)) {
        Drawing::DrawCircle(Player().Position(), R.Range, 0xFFFFD73Cu, 1.5f, 64);
    }
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.kuroaio.twistedfate", "Kuro - Twisted Fate", true);

    CardMenu = MenuRoot->AddSubMenu(new Menu("Cards", "Card Selector"));
    auto* goldKey = CardMenu->Add(new MenuKeyBind("GoldKey", "Select Gold Card", SDK::Keys::W, KeyBindType::Press));
    auto* blueKey = CardMenu->Add(new MenuKeyBind("BlueKey", "Select Blue Card", SDK::Keys::E, KeyBindType::Press));
    auto* redKey = CardMenu->Add(new MenuKeyBind("RedKey", "Select Red Card", SDK::Keys::T, KeyBindType::Press));
    goldKey->Permashow();
    blueKey->Permashow();
    redKey->Permashow();

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo Settings"));
    ComboMenu->Add(new MenuBool("UseQ", "Use Q", true));
    ComboMenu->Add(new MenuBool("QOnlyCC", "Use Q only on crowd control", true));
    ComboMenu->Add(new MenuBool("UseW", "Use W", true));
    ComboMenu->Add(new MenuList("CardMode", "W card", { "Smart", "Blue", "Red", "Gold" }, 0));
    ComboMenu->Add(new MenuSlider("RedEnemies", "Red card minimum enemies", 3, 1, 5));
    ComboMenu->Add(new MenuSlider("BlueMana", "Blue card below mana percent", 25, 0, 100));
    ComboMenu->Add(new MenuBool("BlueOnlyKill", "Use Blue only when it can kill", true));

    AlwaysGoldMenu = ComboMenu->AddSubMenu(new Menu("AlwaysGold", "Always Gold Card"));
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const std::string key = EnemyMenuKey(enemy);
        std::string label = enemy.CharacterName();
        if (label.empty()) {
            label = "Enemy " + std::to_string(enemy.NetworkId());
        }
        AlwaysGoldMenu->Add(new MenuBool(key.c_str(), label.c_str(), false));
    }

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass", "Harass Settings"));
    HarassMenu->Add(new MenuBool("UseQ", "Use Q", true));
    HarassMenu->Add(new MenuBool("QOnlyCC", "Use Q only on crowd control", false));
    HarassMenu->Add(new MenuBool("UseW", "Use W", true));
    HarassMenu->Add(new MenuList("CardMode", "W card", { "Smart", "Blue", "Red", "Gold" }, 0));
    HarassMenu->Add(new MenuSlider("BlueMana", "Blue card below mana percent", 25, 0, 100));
    HarassMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 45, 0, 100));

    ClearMenu = MenuRoot->AddSubMenu(new Menu("Clear", "Lane Clear Settings"));
    ClearMenu->Add(new MenuBool("UseQ", "Use Q", true));
    ClearMenu->Add(new MenuSlider("QHits", "Q minimum minions", 3, 1, 8));
    ClearMenu->Add(new MenuSlider("QMana", "Q minimum mana percent", 25, 0, 100));
    ClearMenu->Add(new MenuBool("UseW", "Use W", true));
    ClearMenu->Add(new MenuList("CardMode", "W card", { "Smart", "Blue", "Red" }, 0));
    ClearMenu->Add(new MenuSlider("BlueMana", "Blue card below mana percent", 45, 0, 100));

    KillstealMenu = MenuRoot->AddSubMenu(new Menu("Killsteal", "Killsteal Settings"));
    KillstealMenu->Add(new MenuBool("UseQ", "Use Q", true));
    KillstealMenu->Add(new MenuBool("UseW", "Use Blue Card", true));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Draw Settings"));
    DrawMenu->Add(new MenuBool("DrawQ", "Draw Q range", false));
    DrawMenu->Add(new MenuBool("DrawR", "Draw R range", false));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc", "Misc Settings"));
    MiscMenu->Add(new MenuBool("AutoQCC", "Auto Q crowd-controlled targets", true));
    MiscMenu->Add(new MenuBool("GoldAfterGate", "Auto Gold Card after Gate", true));

    MenuRoot->Attach();
}

static void RemoveMenu() {
    if (!MenuRoot) {
        return;
    }
    if (CardMenu) {
        for (const char* key : { "GoldKey", "BlueKey", "RedKey" }) {
            if (auto* item = CardMenu->Get<MenuKeyBind>(key)) {
                item->RemovePermashow();
            }
        }
    }
    MenuManager::Instance().Remove(MenuRoot);
    MenuRoot = nullptr;
    CardMenu = nullptr;
    ComboMenu = nullptr;
    AlwaysGoldMenu = nullptr;
    HarassMenu = nullptr;
    ClearMenu = nullptr;
    KillstealMenu = nullptr;
    DrawMenu = nullptr;
    MiscMenu = nullptr;
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 1450.0f);
    Q.SetSkillshot(0.25f, 40.0f, 1000.0f, false, SkillshotType::SkillshotLine);
    W = Spell(SpellSlot::W);
    E = Spell(SpellSlot::E);
    R = Spell(SpellSlot::R, 5500.0f);

    DesiredCard = Card::None;
    CyclingCard = Card::None;
    LockedCard = Card::None;
    Status = SelectStatus::Cooldown;
    LastWAction = 0;
    ForceGoldUntil = 0;

    BuildMenu();
    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Drawing::OnDraw += &Drawing_OnDraw;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>Kuro - Twisted Fate loaded</font>");
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }
    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Drawing::OnDraw -= &Drawing_OnDraw;
    RemoveMenu();
    DesiredCard = Card::None;
    ForceGoldUntil = 0;
    Loaded = false;
}

} // namespace Plugins::KuroAIO::TwistedFate
