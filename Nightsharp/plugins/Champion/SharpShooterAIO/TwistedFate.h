#pragma once

// ============================================================================
// SharpShooter AIO — Twisted Fate
// Port từ CSharpFiles/TwistedFate/TwistedFate.cs (ImpulseAIO) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Sivir.h.
//
// Kỹ năng:
//   Q Wild Cards   — skillshot line 1450, delay 0.25, width 40, speed 1000.
//   W Pick A Card  — state machine chọn bài (Blue/Red/Gold) rồi khoá + đánh AA.
//   E Stacked Deck — passive on-hit (chỉ để tính damage combo/killsteal).
//   R Destiny      — global reveal + blink (chỉ auto-Yellow sau khi cast).
//
// Ghi chú port (giữ 1-1 tới mức API cho phép):
//   * CardSelector state machine: đọc W spell name (PickACard / *cardlock) +
//     CanUseSpell state để lặp lại quy trình chọn bài của C#.
//   * CardDamage: ratio AP cập nhật theo wiki (patch V26.x) — số base giữ theo
//     công thức C# (đã khớp wiki). Q dùng SDK GetDamage.
//   * MISSING API: MiniMap.DrawCircle (vẽ R lên minimap) → không có trong SDK,
//     bỏ phần vẽ minimap, giữ vẽ Q range. Xem missapi.md.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdlib>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::TwistedFate {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* CardSelectorMenu = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* ComboOnlyYMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* KillstealMenu = nullptr;
inline Menu* DrawMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 1450.0f };
inline Spell W{ SpellSlot::W, FLT_MAX };
inline Spell E{ SpellSlot::E, FLT_MAX };
inline Spell R{ SpellSlot::R, 5500.0f };

inline bool Loaded = false;

// CardSelector state (C# CardSelector).
enum class Cards { Red, Yellow, Blue, None };
enum class SelectStatus { Ready, Selecting, Selected, Cooldown };
inline Cards SelectedCard = Cards::None;
inline int LastW = 0;
inline int LastCardLockRequestTick = 0;
inline SelectStatus Status = SelectStatus::Ready;

// CardDamage state (C# CardDamage).
inline bool s_lichbane = false;
inline float s_lichbaneTimer = 0.0f;
inline bool s_ludens = false;
inline int s_lastItemUpdate = 0;

static AIHeroClient Player() {
    return ObjectManager::Player();
}

static bool Bool(Menu* menu, const char* key, bool fallback = true) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuBool>(key);
    return item ? item->Value : fallback;
}

static int Slider(Menu* menu, const char* key, int fallback = 0) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuSlider>(key);
    return item ? item->Value : fallback;
}

static int ListIndex(Menu* menu, const char* key, int fallback = 0) {
    return menu ? menu->GetListIndex(key, fallback) : fallback;
}

static bool KeyActive(Menu* menu, const char* key) {
    if (!menu) {
        return false;
    }
    const auto* item = menu->Get<MenuKeyBind>(key);
    return item && item->Active;
}

static bool ValidUnit(const AttackableUnit& unit) {
    return unit.IsValid() && !unit.IsDead() && unit.Health() > 0.0f;
}

static bool ValidTarget(const AIBaseClient& unit, float range = FLT_MAX) {
    return ValidUnit(unit) && Extensions::IsValidTarget(unit, range, true);
}

static bool ValidHeroTarget(const AIHeroClient& hero, float range = FLT_MAX) {
    return ValidUnit(hero) && Extensions::IsValidTarget(hero, range, true);
}

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
}

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static int HeroPriority(const AIHeroClient& hero) {
    auto* prio = SDK::Modes::Priority::Instance();
    return prio ? prio->GetHeroPriority(hero) : 0;
}

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static std::string WName() {
    const auto player = Player();
    return player.IsValid() ? player.Spellbook().GetSpell(SpellSlot::W).Name() : std::string();
}

static int RandomDelay() {
    return 125 + (std::rand() % 76);  // C#: Random().Next(125, 200)
}

// ── CardDamage (C#) — ratio AP cập nhật theo wiki V26.x; base giữ công thức C# ──
// Blue: 40/60/80/100/120 (+100% total AD) (+100% AP)
// Red : 30/45/60/75/90    (+100% total AD) (+70% AP)
// Gold: 15/22.5/30/.../45 (+100% total AD) (+50% AP)
// E Stacked Deck: 65/90/115/140/165 (+20% bonus AD) (+40% AP)
static void CheckItem() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (SDK::Variables::TickCount() > s_lastItemUpdate) {
        s_lichbane = player.HasItem(3100);
        s_ludens = player.HasItem(6655) && SDK::Items::CanUseItem(player, 6655);
        s_lastItemUpdate = SDK::Variables::TickCount() + 5000;
    }
}

static float CheckItemDamage(const AIBaseClient& unit) {
    const auto player = Player();
    if (!player.IsValid() || !unit.IsValid()) {
        return 0.0f;
    }
    float endDamage = 0.0f;
    if (s_lichbane) {
        if (SDK::Variables::TickCount() >= s_lichbaneTimer || player.HasBuff("lichbane")) {
            const float baseDamage = player.BaseAttackDamage() * 1.5f + player.AP() * 0.4f;
            endDamage += player.CalculateMagicDamage(unit, baseDamage);
        }
    }
    if (s_ludens) {
        const float baseDamage = 100.0f + player.AP() * 0.1f;
        endDamage += player.CalculateMagicDamage(unit, baseDamage);
    }
    return endDamage;
}

static float GetEDamage(const AIBaseClient& unit) {
    const auto player = Player();
    if (!player.IsValid() || !unit.IsValid()) {
        return 0.0f;
    }
    if (player.HasBuff("cardmasterstackparticle")) {
        const int level = E.Instance().Level();
        // wiki: 65/90/115/140/165 + 20% bonus AD + 40% AP.
        const float baseDamage = 65.0f + static_cast<float>(level - 1) * 25.0f;
        const float endDamage = baseDamage + player.BonusAttackDamage() * 0.20f + player.AP() * 0.40f;
        return player.CalculateMagicDamage(unit, endDamage);
    }
    return 0.0f;
}

static float GetBlueDamage(const AIBaseClient& unit) {
    const auto player = Player();
    if (!player.IsValid() || !unit.IsValid()) {
        return 0.0f;
    }
    const float base = 40.0f + static_cast<float>(W.Instance().Level() - 1) * 20.0f;
    const float endDamage = base + player.AD() + player.AP() * 1.0f;
    return player.CalculateMagicDamage(unit, endDamage) + GetEDamage(unit) + CheckItemDamage(unit);
}

static float GetRedDamage(const AIBaseClient& unit) {
    const auto player = Player();
    if (!player.IsValid() || !unit.IsValid()) {
        return 0.0f;
    }
    const float base = 30.0f + static_cast<float>(W.Instance().Level() - 1) * 15.0f;
    const float endDamage = base + player.AD() + player.AP() * 0.70f;
    return player.CalculateMagicDamage(unit, endDamage) + GetEDamage(unit) + CheckItemDamage(unit);
}

static float GetYellowDamage(const AIBaseClient& unit) {
    const auto player = Player();
    if (!player.IsValid() || !unit.IsValid()) {
        return 0.0f;
    }
    const float base = 15.0f + static_cast<float>(W.Instance().Level() - 1) * 7.5f;
    const float endDamage = base + player.AD() + player.AP() * 0.50f;
    return player.CalculateMagicDamage(unit, endDamage) + GetEDamage(unit) + CheckItemDamage(unit);
}

// ── CardSelector helpers ──
static int GetEStack() {
    const auto player = Player();
    return player.IsValid() ? player.GetBuffCount("cardmasterstackholder") : 0;
}

static bool HasEBuff() {
    const auto player = Player();
    return player.IsValid() && player.HasBuff("cardmasterstackparticle");
}

static void StartSelecting(Cards card) {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (ToLower(WName()) == "pickacard" &&
        (Status == SelectStatus::Ready || Status == SelectStatus::Cooldown)) {
        SelectedCard = card;
        if (SDK::Variables::TickCount() - LastW > 170 + Game::Ping() / 2) {
            if (player.Spellbook().CastSpell(SpellSlot::W, false)) {
                LastW = SDK::Variables::TickCount();
                Status = SelectStatus::Selecting;
            }
        }
    }
}

static void SelectCard(Cards card) {
    if (card == Cards::Red) {
        StartSelecting(Cards::Red);
    } else if (card == Cards::Yellow) {
        StartSelecting(Cards::Yellow);
    } else if (card == Cards::Blue) {
        StartSelecting(Cards::Blue);
    }
}

static bool IsYTarget(const AIHeroClient& t) {
    if (!ComboOnlyYMenu || !t.IsValid()) {
        return false;
    }
    const std::string key = "Cast." + t.CharacterName();
    const auto* item = ComboOnlyYMenu->Get<MenuBool>(key.c_str());
    return item ? item->Value : false;
}

static Cards HeroCardSelection(const AIHeroClient& t) {
    if (!t.IsValid() || !ValidHeroTarget(t)) {
        return Cards::None;
    }
    const auto player = Player();
    const auto tBase = AIBaseClient(t.Handle());
    const int alliesAroundTarget = t.CountEnemyHeroesInRange(175.0f);  // enemies around target (C# CountEnemyHerosInRangeFix)
    const int redCount = Slider(ComboMenu, "useWRedCount", 3);
    const int manaW = Slider(ComboMenu, "useWBlueMana", 25);

    if (Bool(ComboMenu, "useWBlueOnlyKill", true)) {
        if (t.Health() < GetBlueDamage(tBase)) {
            return Cards::Blue;
        }
    }
    if (IsYTarget(t)) {
        return Cards::Yellow;
    }
    if (!Bool(ComboMenu, "useWBlueOnlyKill", true) && player.ManaPercent() <= static_cast<float>(manaW)) {
        return Cards::Blue;
    }
    if (player.CountAllyHeroesInRange(500.0f) - 1 >= player.CountEnemyHeroesInRange(600.0f)) {
        return Cards::Yellow;
    }
    if (alliesAroundTarget >= redCount) {
        return Cards::Red;
    }
    if (Q.IsReady()) {
        return Cards::Yellow;
    }
    return Cards::Yellow;
}

static Cards HeroCardSelectionHarass(const AIHeroClient& t) {
    if (!t.IsValid() || !ValidHeroTarget(t)) {
        return Cards::None;
    }
    const auto player = Player();
    const auto tBase = AIBaseClient(t.Handle());
    const int manaW = Slider(HarassMenu, "useWBlueMana", 25);

    if (Bool(ComboMenu, "useWBlueOnlyKill", true)) {
        if (t.Health() < GetBlueDamage(tBase)) {
            return Cards::Blue;
        }
    }
    if (player.ManaPercent() <= static_cast<float>(manaW)) {
        return Cards::Blue;
    }
    if (HasEBuff()) {
        return Cards::Blue;
    }
    if (GetEStack() == 2) {
        return Cards::Yellow;
    }
    if (Q.IsReady()) {
        return Cards::Yellow;
    }
    return Cards::None;
}

// Forward declarations — đúng thứ tự file C#.
static void OnProcessSpell(const Events::ProcessSpellEventArgs& args);
static void OnDraw();
static void OnBuffRemove(const BuffEventArgs& args);
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void Card();
static void Combo();
static void Harass();
static void LaneClear();
static void Killsteal();
static void AutoQ();
static void CardStateUpdate();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - TwistedFate", true);

    CardSelectorMenu = MenuRoot->AddSubMenu(new Menu("CardSelector Settings", "CardSelector"));
    CardSelectorMenu->Add(new MenuKeyBind("useY", "Yellow", 'W', KeyBindType::Press));
    CardSelectorMenu->Add(new MenuKeyBind("useB", "Blue", 'E', KeyBindType::Press));
    CardSelectorMenu->Add(new MenuKeyBind("useR", "Red", 'T', KeyBindType::Press));

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useQStun", "Only CC", true));
    ComboMenu->Add(new MenuBool("useW", "Use W"));
    ComboMenu->Add(new MenuList("useWCard", "Card Mode",
        std::vector<std::string>{ "Smart", "Blue", "Red", "Yellow" }, 0));
    ComboMenu->Add(new MenuSlider("useWRedCount", "Use Red if AOE HitCount >= X", 3, 1, 5));
    ComboMenu->Add(new MenuSlider("useWBlueMana", "Use Blue if Mana <= X", 25, 0, 100));
    ComboMenu->Add(new MenuBool("useWBlueOnlyKill", "Only Killable Use Blue Card", true));
    ComboOnlyYMenu = ComboMenu->AddSubMenu(new Menu("OnlyY", "Always Yellow card"));
    for (const auto& x : GameObjects::EnemyHeroes()) {
        const std::string name = x.CharacterName();
        if (name.empty()) {
            continue;
        }
        const std::string key = "Cast." + name;
        ComboOnlyYMenu->Add(new MenuBool(key.c_str(), name.c_str(), false));
    }

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuBool("useQStun", "Only CC", false));
    HarassMenu->Add(new MenuBool("useW", "Use W"));
    HarassMenu->Add(new MenuList("useWCard", "Card Mode",
        std::vector<std::string>{ "Smart", "Blue", "Red", "Yellow" }, 0));
    HarassMenu->Add(new MenuSlider("useWBlueMana", "Use Blue Card if Mana <= X%", 25, 0, 100));
    HarassMenu->Add(new MenuSlider("Hmana", "Don't Use Spell Harass if Mana <= X%", 45, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "LaneClear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q"));
    LaneClearMenu->Add(new MenuSlider("useQCount", "Min hit x minion", 3, 1, 5));
    LaneClearMenu->Add(new MenuSlider("useQMana", "Don't Q if Mana <= X%", 25, 0, 100));
    LaneClearMenu->Add(new MenuBool("useW", "Use W"));
    LaneClearMenu->Add(new MenuList("useWCard", "Card Mode",
        std::vector<std::string>{ "Smart", "Blue", "Red", "Yellow" }, 0));
    LaneClearMenu->Add(new MenuSlider("Lmana", "Use Blue card When Mana <= X%", 45, 0, 100));

    KillstealMenu = MenuRoot->AddSubMenu(new Menu("Killsteal Settings", "Killsteal"));
    KillstealMenu->Add(new MenuBool("useQ", "Use Q"));
    KillstealMenu->Add(new MenuBool("useW", "Use W"));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw Settings", "Draw"));
    DrawMenu->Add(new MenuBool("useQ", "Draw Q"));
    DrawMenu->Add(new MenuBool("useR", "Draw R"));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("autoQ", "Auto Q CC"));
    MiscMenu->Add(new MenuBool("autoY", "Use R Auto Yellow card."));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 1450.0f);
    Q.SetSkillshot(0.25f, 40.0f, 1000.0f, false, SpellType::Line);
    Q.DamageType = DamageType::Magical;

    W = Spell(SpellSlot::W, FLT_MAX);

    E = Spell(SpellSlot::E, FLT_MAX);

    R = Spell(SpellSlot::R, 5500.0f);

    s_lichbaneTimer = static_cast<float>(SDK::Variables::TickCount());

    BuildMenu();

    Events::hook.OnProcessSpell += &OnProcessSpell;
    Events::hook.OnBuffRemove += &OnBuffRemove;
    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - TwistedFate loaded</font>");
}

// C#: OnProcessSpellCast — auto-Yellow sau khi cast R ("gate"); + card state machine.
static void OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    const std::string name = ToLower(args.SpellName);
    if (name == "gate" && Bool(MiscMenu, "autoY")) {
        StartSelecting(Cards::Yellow);
    }
    // C# CardSelector.Obj_AI_Base_OnProcessSpellCast.
    if (name == "pickacard") {
        Status = SelectStatus::Selecting;
    }
    if (name == "goldcardlock" || name == "bluecardlock" || name == "redcardlock") {
        Status = SelectStatus::Selected;
        SelectedCard = Cards::None;
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "useQ", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFFFFF00u);
    }
    // MISSING API: MiniMap.DrawCircle (R range trên minimap) — xem missapi.md.
}

// C#: lichbane buff remove → set timer +2700ms.
static void OnBuffRemove(const BuffEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    if (std::string(args.BuffName) == "lichbane") {
        s_lichbaneTimer = static_cast<float>(SDK::Variables::TickCount()) + 2700.0f;
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }

    CheckItem();
    CardStateUpdate();
    Killsteal();
    AutoQ();
    Card();

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        Harass();
        break;
    case OrbwalkingMode::LaneClear:
        LaneClear();
        break;
    default:
        break;
    }
}

// C# CardSelector.Game_OnGameUpdate — cập nhật Status + recast lock để chốt bài.
static void CardStateUpdate() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const std::string wLower = ToLower(WName());
    const int now = SDK::Variables::TickCount();

    if (player.IsDead()) {
        Status = SelectStatus::Ready;
        SelectedCard = Cards::None;
        return;
    }
    if (wLower == "pickacard") {
        Status = W.IsReady() ? SelectStatus::Ready : SelectStatus::Cooldown;
        if (Status == SelectStatus::Cooldown) {
            SelectedCard = Cards::None;
        }
    } else if (wLower == "bluecardlock" || wLower == "redcardlock" || wLower == "goldcardlock") {
        Status = SelectStatus::Selecting;
    } else if (!W.IsReady()) {
        SelectedCard = Cards::None;
        Status = SelectStatus::Cooldown;
    }

    const bool desired = (SelectedCard == Cards::Blue && wLower == "bluecardlock") ||
                         (SelectedCard == Cards::Yellow && wLower == "goldcardlock") ||
                         (SelectedCard == Cards::Red && wLower == "redcardlock");
    if (desired && now - LastW >= 120 && now - LastCardLockRequestTick >= 250) {
        if (player.Spellbook().CastSpell(SpellSlot::W, false)) {
            LastCardLockRequestTick = now;
            Status = SelectStatus::Selected;
            SelectedCard = Cards::None;
        }
    }
}

static void Card() {
    if (KeyActive(CardSelectorMenu, "useY")) {
        SelectCard(Cards::Yellow);
    }
    if (KeyActive(CardSelectorMenu, "useB")) {
        SelectCard(Cards::Blue);
    }
    if (KeyActive(CardSelectorMenu, "useR")) {
        SelectCard(Cards::Red);
    }
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Bool(ComboMenu, "useW")) {
        AIHeroClient wTarget;
        int bestPrio = -1;
        const float wRange = AutoAttack::GetRealAutoAttackRange(player, AttackableUnit()) + 300.0f;
        for (const auto& x : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(x, wRange)) {
                continue;
            }
            const int prio = HeroPriority(x);
            if (prio > bestPrio) {
                bestPrio = prio;
                wTarget = x;
            }
        }
        if (wTarget.IsValid()) {
            switch (ListIndex(ComboMenu, "useWCard", 0)) {
            case 0: {
                const Cards selectedCard = HeroCardSelection(wTarget);
                if (selectedCard != Cards::None) {
                    SelectCard(selectedCard);
                }
                break;
            }
            case 1: SelectCard(Cards::Blue); break;
            case 2: SelectCard(Cards::Red); break;
            case 3: SelectCard(Cards::Yellow); break;
            }
            if (Orbwalker::CanAttack() && AutoAttack::InAutoAttackRange(wTarget)) {
                Orbwalker::Attack(wTarget);
            }
        }
    }

    if (Bool(ComboMenu, "useQ")) {
        const auto qTarget = GetTarget(Q.Range, DamageType::Magical);
        if (!qTarget.IsValid()) {
            return;
        }
        if (Bool(ComboMenu, "useQStun", true)) {
            return;
        }
        if (!ValidHeroTarget(qTarget, Q.Range) || !Q.IsReady()) {
            return;
        }
        const auto pred = Q.GetPrediction(qTarget);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
            Q.Cast(pred.GetCastPosition());
        }
    }
}

static void Harass() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() <= static_cast<float>(Slider(HarassMenu, "Hmana", 45))) {
        return;
    }

    if (Bool(HarassMenu, "useW")) {
        AIHeroClient wTarget;
        int bestPrio = -1;
        const float wRange = AutoAttack::GetRealAutoAttackRange(player, AttackableUnit()) + 300.0f;
        for (const auto& x : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(x, wRange)) {
                continue;
            }
            const int prio = HeroPriority(x);
            if (prio > bestPrio) {
                bestPrio = prio;
                wTarget = x;
            }
        }
        if (wTarget.IsValid()) {
            switch (ListIndex(HarassMenu, "useWCard", 0)) {
            case 0: {
                const Cards selectedCard = HeroCardSelectionHarass(wTarget);
                if (selectedCard != Cards::None) {
                    SelectCard(selectedCard);
                    if (Orbwalker::CanAttack() && AutoAttack::InAutoAttackRange(wTarget)) {
                        Orbwalker::Attack(wTarget);
                    }
                } else {
                    AIMinionClient best;
                    float bestDist = FLT_MAX;
                    for (const auto& m : GameObjects::EnemyMinions()) {
                        if (!ValidTarget(m) || !AutoAttack::InAutoAttackRange(m)) {
                            continue;
                        }
                        const float d = m.Distance(wTarget);
                        if (d <= 175.0f && d < bestDist) {
                            bestDist = d;
                            best = m;
                        }
                    }
                    if (best.IsValid()) {
                        SelectCard(Cards::Red);
                        if (Orbwalker::CanAttack() && AutoAttack::InAutoAttackRange(best)) {
                            Orbwalker::Attack(best);
                        }
                    } else {
                        SelectCard(Cards::Yellow);
                        if (Orbwalker::CanAttack() && AutoAttack::InAutoAttackRange(wTarget)) {
                            Orbwalker::Attack(wTarget);
                        }
                    }
                }
                break;
            }
            case 1:
                SelectCard(Cards::Blue);
                if (Orbwalker::CanAttack() && AutoAttack::InAutoAttackRange(wTarget)) {
                    Orbwalker::Attack(wTarget);
                }
                break;
            case 2:
                SelectCard(Cards::Red);
                if (Orbwalker::CanAttack() && AutoAttack::InAutoAttackRange(wTarget)) {
                    Orbwalker::Attack(wTarget);
                }
                break;
            case 3:
                SelectCard(Cards::Yellow);
                if (Orbwalker::CanAttack() && AutoAttack::InAutoAttackRange(wTarget)) {
                    Orbwalker::Attack(wTarget);
                }
                break;
            }
        }
    }

    if (Bool(HarassMenu, "useQ")) {
        const auto qTarget = GetTarget(Q.Range, DamageType::Magical);
        if (!qTarget.IsValid()) {
            return;
        }
        if (Bool(HarassMenu, "useQStun", false)) {
            return;
        }
        if (!ValidHeroTarget(qTarget, Q.Range) || !Q.IsReady()) {
            return;
        }
        const auto pred = Q.GetPrediction(qTarget);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
            Q.Cast(pred.GetCastPosition());
        }
    }
}

static void LaneClear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Bool(LaneClearMenu, "useQ") && Q.IsReady()) {
        std::vector<AIBaseClient> qMinions;
        for (const auto& m : GameObjects::EnemyMinions()) {
            if (ValidTarget(m, Q.Range)) {
                qMinions.push_back(AIBaseClient(m.Handle()));
            }
        }
        std::sort(qMinions.begin(), qMinions.end(),
            [](const AIBaseClient& a, const AIBaseClient& b) { return a.Health() < b.Health(); });
        if (player.ManaPercent() >= static_cast<float>(Slider(LaneClearMenu, "useQMana", 25))) {
            const auto farm = Q.GetLineFarmLocation(qMinions);
            if (farm.MinionsHit >= Slider(LaneClearMenu, "useQCount", 3)) {
                Q.Cast(Vector3::From2D(farm.Position));
            }
        }
    }

    if (Bool(LaneClearMenu, "useW")) {
        std::vector<AIMinionClient> minions;
        const float aaRange = AutoAttack::GetRealAutoAttackRange(player, AttackableUnit()) + 100.0f;
        for (const auto& m : GameObjects::EnemyMinions()) {
            if (ValidTarget(m, aaRange)) {
                minions.push_back(m);
            }
        }
        if (minions.empty()) {
            return;
        }
        switch (ListIndex(LaneClearMenu, "useWCard", 0)) {
        case 0: {
            int redKillable = 0;
            for (const auto& target : minions) {
                const auto tBase = AIBaseClient(target.Handle());
                float nearest = FLT_MAX;
                for (const auto& o : minions) {
                    if (o.NetworkId() != target.NetworkId()) {
                        nearest = std::min(nearest, o.Distance(target));
                    }
                }
                const float dmg = GetRedDamage(tBase) + (Q.IsReady() ? Q.GetDamage(tBase) : 0.0f);
                if (nearest <= 200.0f && target.Health() <= dmg) {
                    ++redKillable;
                }
            }
            Cards selectedCard = Cards::None;
            if (selectedCard == Cards::None && player.ManaPercent() <= static_cast<float>(Slider(LaneClearMenu, "Lmana", 45))) {
                selectedCard = Cards::Blue;
            }
            if (selectedCard == Cards::None && redKillable >= 3) {
                selectedCard = Cards::Red;
            }
            if (selectedCard != Cards::None) {
                SelectCard(selectedCard);
            }
            break;
        }
        case 1: SelectCard(Cards::Blue); break;
        case 2: SelectCard(Cards::Red); break;
        case 3: SelectCard(Cards::Yellow); break;
        }
    }
}

static void Killsteal() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Hoist snapshot EnemyHeroes 1 lần cho cả nhánh useQ và useW (frame đóng băng = y hệt).
    const auto ksHeroes = GameObjects::EnemyHeroes();

    if (Bool(KillstealMenu, "useQ") && Q.IsReady()) {
        AIHeroClient best;
        HitChance bestHc = HitChance::None;
        Vector3 bestPos;
        for (const auto& x : ksHeroes) {
            if (!ValidHeroTarget(x, Q.Range) || x.Health() >= Q.GetDamage(AIBaseClient(x.Handle()))) {
                continue;
            }
            const auto pred = Q.GetPrediction(x);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High) &&
                static_cast<int>(pred.Hitchance) > static_cast<int>(bestHc)) {
                bestHc = pred.Hitchance;
                bestPos = pred.GetCastPosition();
                best = x;
            }
        }
        if (best.IsValid()) {
            Q.Cast(bestPos);
        }
    }

    if (Bool(KillstealMenu, "useW") && W.IsReady()) {
        AIHeroClient best;
        int bestPrio = -1;
        const float aaRange = AutoAttack::GetRealAutoAttackRange(player, AttackableUnit());
        for (const auto& x : ksHeroes) {
            if (!ValidHeroTarget(x, aaRange) || x.Health() >= GetBlueDamage(AIBaseClient(x.Handle()))) {
                continue;
            }
            const int prio = HeroPriority(x);
            if (prio > bestPrio) {
                bestPrio = prio;
                best = x;
            }
        }
        if (best.IsValid()) {
            SelectCard(Cards::Blue);
            if (Orbwalker::CanAttack() && AutoAttack::InAutoAttackRange(best)) {
                Orbwalker::Attack(best);
            }
        }
    }
}

static void AutoQ() {
    if (!Q.IsReady() || !Bool(MiscMenu, "autoQ")) {
        return;
    }
    for (const auto& h : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(h, Q.Range)) {
            continue;
        }
        const auto pred = Q.GetPrediction(h);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::Immobile) ||
            (SDK::HasBuffOfType(AIBaseClient(h.Handle()), SDK::BuffType::Slow) &&
             HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh))) {
            Q.Cast(pred.GetCastPosition());
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Events::hook.OnBuffRemove -= &OnBuffRemove;
    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Drawing::OnDraw -= &OnDraw;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::TwistedFate
