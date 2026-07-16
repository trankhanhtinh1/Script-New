#pragma once

// ============================================================================
// SharpShooter AIO — Senna
// Port từ CSharpFiles/Senna/Senna.cs (ImpulseAIO) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Corki.h.
//
// Kỹ năng:
//   Q Piercing Darkness — targeted 600(+scale theo souls), delay 0.325.
//     ExtraDmgQ/ExtraHealQ: line 1300 (Q xuyên qua 1 unit tới enemy/ally).
//   W Last Embrace       — skillshot line 1200, delay 0.25, width 70, speed 1200, collision.
//   E Curse of the Black Mist — self stealth (chỉ để nhận diện, không auto).
//   R Dawning Shadow     — global line; DamageR (nuke) + HealthR (shield ally).
//
// Ghi chú port (giữ 1-1 tới mức API cho phép):
//   * ResetQRange: tầm Q tăng theo souls (buff "SennaPassiveStacks").
//   * HealAllyLogic: Q heal ally máu thấp (FastHeal keybind / auto theo %).
//   * AutoRLogic: R cứu ally (UsePredHealth) hoặc R killable enemy.
//   * Soul/Mist tracking ("Barrel") qua OnCreate; AutoPick nhặt bằng IssueAttack.
//   * MISSING API: Player.IssueOrder(AttackUnit) → CoreControl::IssueAttack.
//   * Damage tính tay theo wiki (patch V26.13) — KHÔNG dùng Spell::GetDamage.
// ============================================================================

#include "../../../SDK/SDK.h"
#include "../../../core/CoreControl.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Senna {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* HealMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 600.0f };
inline Spell ExtraDmgQ{ SpellSlot::Q, 1300.0f };
inline Spell ExtraHealQ{ SpellSlot::Q, 1300.0f };
inline Spell W{ SpellSlot::W, 1200.0f };
inline Spell E{ SpellSlot::E, 400.0f };
inline Spell HealthR{ SpellSlot::R, FLT_MAX };
inline Spell DamageR{ SpellSlot::R, FLT_MAX };

inline bool Loaded = false;

// Soul/Mist wraith tracking ("Barrel").
struct SoulInfo {
    AIBaseClient Pointer;
    int ValidTime = 0;
};
inline std::vector<SoulInfo> Souls;

static AIHeroClient Player() {
    return ObjectManager::Player();
}

static std::string RuntimeObjectName(const GameObject& object) {
    char name[128] = {};
    if (object.IsValid() &&
        ::Core::Objects::ReadName(object.Address(), name, static_cast<int>(sizeof(name))) && name[0]) {
        return name;
    }
    return object.CharacterName();
}

static std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
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

static float HealthPrediction(const AIBaseClient& unit, int ms) {
    return unit.IsValid() ? Prediction::Health::GetPrediction(unit, ms) : 0.0f;
}

// ── Damage tính tay theo wiki (leagueoflegends.com/Senna, patch V26.13) ──
// KHÔNG dùng Spell::GetDamage. Chốt số ngày 2026-07-08.
//
// Q Piercing Darkness (PHYSICAL): 30/55/80/105/130 + 60% bonus AD.
// W Last Embrace       (PHYSICAL): 70/110/150/190/230 + 90% bonus AD.
// R Dawning Shadow     (PHYSICAL): 250/400/550 + 115% bonus AD + 70% AP.
static float GetRDmg(const AIHeroClient& unit) {
    const auto player = Player();
    if (!player.IsValid() || !unit.IsValid()) {
        return 0.0f;
    }
    const int level = DamageR.Instance().Level();
    if (level < 1) {
        return 0.0f;
    }
    const int idx = (level - 1 < 3) ? level - 1 : 2;
    static const float base[3] = { 250.0f, 400.0f, 550.0f };
    const float raw = base[idx] + 1.15f * player.BonusAttackDamage() + 0.70f * player.AP();
    return Damage::CalculateDamage(player, AIBaseClient(unit.Handle()), DamageType::Physical, raw);
}

// Soul còn hợp lệ (chưa hết hạn / chưa nhặt).
static bool IsValidSoul(const SoulInfo& soul) {
    if (!soul.Pointer.IsValid() || soul.Pointer.IsDead() ||
        SDK::Variables::TickCount() > soul.ValidTime) {
        return false;
    }
    return true;
}

// Danh sách unit Q có thể bắn xuyên (minion/jungle/hero/soul trong tầm Q).
// Memo theo tick: cùng 1 frame dữ liệu đã đóng băng nên tái dùng = kết quả y hệt,
// tránh copy lại 3 vector (minion/jungle/hero) mỗi lần gọi trong loop.
static std::vector<AIBaseClient> GetHittableTargets() {
    static std::vector<AIBaseClient> cache;
    static DWORD cacheTick = 0;
    static float cacheRange = -1.0f;
    const DWORD now = SDK::Variables::TickCount();
    if (now == cacheTick && cacheRange == Q.Range) {
        return cache;
    }

    std::vector<AIBaseClient> list;
    for (const auto& m : GameObjects::EnemyMinions()) {
        if (ValidTarget(m, Q.Range)) {
            list.push_back(AIBaseClient(m.Handle()));
        }
    }
    for (const auto& j : GameObjects::Jungle()) {
        if (ValidTarget(j, Q.Range)) {
            list.push_back(AIBaseClient(j.Handle()));
        }
    }
    for (const auto& h : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(h, Q.Range)) {
            list.push_back(AIBaseClient(h.Handle()));
        }
    }
    for (const auto& soul : Souls) {
        if (IsValidSoul(soul) && ValidTarget(soul.Pointer, Q.Range)) {
            list.push_back(soul.Pointer);
        }
    }

    cacheTick = now;
    cacheRange = Q.Range;
    cache = list;
    return list;
}

// R black list theo ally name.
static bool IsBlockR(const AIHeroClient& unit) {
    if (!RMenu || !unit.IsValid()) {
        return false;
    }
    const std::string key = "not." + unit.CharacterName();
    const auto* item = RMenu->Get<MenuBool>(key.c_str());
    return item ? item->Value : false;
}

// Forward declarations — đúng thứ tự file C#.
static void OnDraw();
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnObjectCreate(const GameObject& obj);
static void OnObjectDelete(const GameObject& obj);
static void ResetQRange();
static void HealAllyLogic();
static void AutoRLogic();
static void CastW();
static bool CastNormalQ(const AIBaseClient& unit = AIBaseClient());
static bool CastExtraQ(bool damage = true, const AIBaseClient& healAlly = AIBaseClient());
static void AutoPick();
static void JungleClear();
static void LaneClear();
static void Harass();
static void Combo();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Senna", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("CQ", "Use Q"));
    ComboMenu->Add(new MenuBool("CQExtra", "Use Extend Q"));
    ComboMenu->Add(new MenuBool("CW", "Use W"));
    ComboMenu->Add(new MenuList("Target", "Orb Mode",
        std::vector<std::string>{ "Passive", "Default" }, 0));

    RMenu = MenuRoot->AddSubMenu(new Menu("R Settings", "R"));
    RMenu->Add(new MenuList("UseR", "Use R",
        std::vector<std::string>{ "Always", "Only Combo", "Disable" }, 0));
    RMenu->Add(new MenuBool("RHealAlly", "Use R Help Low HP Ally"));
    RMenu->Add(new MenuSlider("RHealPerfe", "if ally hp <= X%", 15, 0, 100));
    RMenu->Add(new MenuSlider("RhealEnemyCount", "if ally count enemy >= X", 1, 0, 5));
    RMenu->Add(new MenuBool("UsePredHealth", "use Health Prediction"));
    for (const auto& obj : GameObjects::AllyHeroes()) {
        const std::string name = obj.CharacterName();
        if (name.empty()) {
            continue;
        }
        const std::string key = "not." + name;
        RMenu->Add(new MenuBool(key.c_str(), name.c_str(), false));
    }
    RMenu->Add(new MenuBool("CRKillable", "Use R Killable"));
    RMenu->Add(new MenuSlider("RCountEnemyForMe", "when count enemy <= X", 1, 0, 5));

    HealMenu = MenuRoot->AddSubMenu(new Menu("Heal Settings", "Heal set"));
    HealMenu->Add(new MenuList("UseQHeal", "Use Q Heal",
        std::vector<std::string>{ "Always", "Only Combo", "Disable" }, 0));
    HealMenu->Add(new MenuSlider("QHealPerfe", "When Ally HP <= X%", 15, 0, 100));
    HealMenu->Add(new MenuKeyBind("QHealKey", "Fast Heal", 'A', KeyBindType::Press));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("HQ", "Use Q"));
    HarassMenu->Add(new MenuBool("HQExtra", "Use Extend Q"));
    HarassMenu->Add(new MenuSlider("HMana", "Don't Harass if Mana <= X%", 40, 0, 100));
    HarassMenu->Add(new MenuKeyBind("autoHarass", "Auto Harass", 'T', KeyBindType::Toggle));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "LaneClear"));
    LaneClearMenu->Add(new MenuBool("LQ", "Use Q"));
    LaneClearMenu->Add(new MenuSlider("LQCount", "Use Q Min HitCount Minion", 2, 1, 6));
    LaneClearMenu->Add(new MenuSlider("LMana", "Don't LaneClear if Mana <= X%", 40, 0, 100));
    LaneClearMenu->Add(new MenuBool("AutoPick", "Auto Pick Soul"));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("JungleClear Settings", "JungleClear"));
    JungleClearMenu->Add(new MenuBool("JQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("JW", "Use W"));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw Settings", "Draw"));
    DrawMenu->Add(new MenuBool("DrawQ", "Draw Q"));
    DrawMenu->Add(new MenuBool("DrawQExtra", "Draw Extend Q"));
    DrawMenu->Add(new MenuBool("DrawW", "Draw W"));
    DrawMenu->Add(new MenuBool("DrawPick", "Draw Soul"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 600.0f + player.BoundingRadius());
    Q.SetTargetted(0.325f, FLT_MAX);
    Q.DamageType = DamageType::Physical;

    ExtraDmgQ = Spell(SpellSlot::Q, 1300.0f);
    ExtraDmgQ.SetSkillshot(0.325f, 50.0f, FLT_MAX, false, SpellType::Line);
    ExtraDmgQ.DamageType = DamageType::Physical;

    ExtraHealQ = Spell(SpellSlot::Q, 1300.0f);
    ExtraHealQ.SetSkillshot(0.325f, 140.0f, FLT_MAX, false, SpellType::Line);

    W = Spell(SpellSlot::W, 1200.0f);
    W.SetSkillshot(0.25f, 70.0f, 1200.0f, true, SpellType::Line);
    W.SetCollisionObjects(
        SDK::CollisionableObjects::Heroes |
        SDK::CollisionableObjects::YasuoWall);
    W.DamageType = DamageType::Physical;

    E = Spell(SpellSlot::E, 400.0f);

    HealthR = Spell(SpellSlot::R, FLT_MAX);
    HealthR.SetSkillshot(1.0f, 1200.0f, 20000.0f, false, SpellType::Line);

    DamageR = Spell(SpellSlot::R, FLT_MAX);
    DamageR.SetSkillshot(1.0f, 160.0f, 20000.0f, false, SpellType::Line);
    DamageR.DamageType = DamageType::Physical;

    BuildMenu();

    // Nạp soul đã tồn tại sẵn.
    for (const auto& obj : GameObjects::AllGameObjects()) {
        OnObjectCreate(obj);
    }

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Drawing::OnDraw += &OnDraw;
    GameObjects::AddOnCreate(&OnObjectCreate);
    GameObjects::AddOnDelete(&OnObjectDelete);

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Senna loaded</font>");
}

static void OnObjectCreate(const GameObject& obj) {
    const auto base = AIBaseClient(obj.Handle());
    if (!base.IsValid()) {
        return;
    }
    const std::string name = ToLower(RuntimeObjectName(obj));
    const bool soulName = name.find("barrel") != std::string::npos ||
                          name.find("senna") != std::string::npos ||
                          name.find("soul") != std::string::npos;
    if (soulName && base.MaxHealth() <= 1.0f && !base.IsAlly()) {
        const int id = base.NetworkId();
        if (std::any_of(Souls.begin(), Souls.end(),
            [id](const SoulInfo& soul) {
                return soul.Pointer.IsValid() && soul.Pointer.NetworkId() == id;
            })) {
            return;
        }
        SoulInfo info;
        info.Pointer = base;
        info.ValidTime = SDK::Variables::TickCount() + 9000;
        Souls.push_back(info);
    }
}

static void OnObjectDelete(const GameObject& obj) {
    const int id = obj.NetworkId();
    Souls.erase(std::remove_if(Souls.begin(), Souls.end(),
        [id](const SoulInfo& soul) {
            return !soul.Pointer.IsValid() || soul.Pointer.NetworkId() == id;
        }), Souls.end());
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "DrawQ", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFFF0000u);
    }
    if (Bool(DrawMenu, "DrawQExtra", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), ExtraDmgQ.Range, 0xFFFFA500u);
    }
    if (Bool(DrawMenu, "DrawW", false) && W.IsReady()) {
        Drawing::DrawCircle(player.Position(), W.Range, 0xFF000000u);
    }
    if (Bool(DrawMenu, "DrawPick", false)) {
        for (const auto& soul : Souls) {
            if (IsValidSoul(soul)) {
                Drawing::DrawCircle(soul.Pointer.Position(), 60.0f, 0xFF7CFC00u);
            }
        }
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

    // C#: Q delay giảm theo attack speed.
    const float delay = 0.4f - std::max(0.0f, std::min(0.2f, 0.02f * ((player.AttackSpeedMod() - 1.0f) / 0.25f)));
    Q.Delay = delay;
    ExtraDmgQ.Delay = delay;
    ExtraHealQ.Delay = delay;

    HealAllyLogic();
    AutoRLogic();
    ResetQRange();

    if (KeyActive(HarassMenu, "autoHarass") && Orbwalker::ActiveMode() != OrbwalkingMode::Combo) {
        Harass();
    }

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        if (!KeyActive(HarassMenu, "autoHarass")) {
            Harass();
        }
        break;
    case OrbwalkingMode::LaneClear:
        LaneClear();
        JungleClear();
        if (Bool(LaneClearMenu, "AutoPick")) {
            AutoPick();
        }
        break;
    default:
        break;
    }

    Souls.erase(
        std::remove_if(Souls.begin(), Souls.end(),
            [](const SoulInfo& s) { return !IsValidSoul(s); }),
        Souls.end());
}

// Tầm Q tăng theo souls (buff "SennaPassiveStacks"), tối đa 1100.
static void ResetQRange() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const int passiveCount = player.GetBuffCount("SennaPassiveStacks");
    if (passiveCount >= 400) {
        Q.Range = 1100.0f;
        return;
    }
    const int dist = 600 + (passiveCount / 20) * 25;
    Q.Range = static_cast<float>(dist) + player.BoundingRadius();
}

static void HealAllyLogic() {
    const auto player = Player();
    if (!player.IsValid() || !Q.IsReady() || ListIndex(HealMenu, "UseQHeal", 0) == 2) {
        return;
    }

    if (KeyActive(HealMenu, "QHealKey")) {
        std::vector<AIHeroClient> allyList;
        for (const auto& x : GameObjects::AllyHeroes()) {
            if (x.HealthPercent() != 100.0f && !x.IsMe() && ValidHeroTarget(x, ExtraHealQ.Range)) {
                allyList.push_back(x);
            }
        }
        std::sort(allyList.begin(), allyList.end(),
            [](const AIHeroClient& a, const AIHeroClient& b) {
                return a.Health() < b.Health();
            });
        if (allyList.empty()) {
            if (player.HealthPercent() != 100.0f) {
                for (const auto& soul : Souls) {
                    if (IsValidSoul(soul) && ValidTarget(soul.Pointer, Q.Range)) {
                        Q.CastOnUnit(soul.Pointer);
                        break;
                    }
                }
            }
        }
        for (const auto& allyobj : allyList) {
            if (player.Distance(allyobj) <= Q.Range) {
                Q.CastOnUnit(AIBaseClient(allyobj.Handle()));
            } else {
                CastExtraQ(false, AIBaseClient(allyobj.Handle()));
            }
        }
    }

    if (ListIndex(HealMenu, "UseQHeal", 0) == 0) {
        for (const auto& allyobj : GameObjects::AllyHeroes()) {
            if (!ValidHeroTarget(allyobj, ExtraHealQ.Range)) {
                continue;
            }
            if (allyobj.HealthPercent() <= static_cast<float>(Slider(HealMenu, "QHealPerfe", 15))) {
                if (player.Distance(allyobj) <= Q.Range) {
                    Q.CastOnUnit(AIBaseClient(allyobj.Handle()));
                    break;
                } else {
                    CastExtraQ(false, AIBaseClient(allyobj.Handle()));
                }
            }
        }
    }
}

static bool CanCastSpell(const Spell& spl, const AIHeroClient& obj) {
    if (HealthPrediction(AIBaseClient(obj.Handle()), static_cast<int>(spl.Delay * 1000.0f)) <= 0.0f ||
        (obj.HealthPercent() <= 10.0f && obj.CountAllyHeroesInRange(400.0f) - 1 >= 1)) {
        return false;
    }
    return true;
}

static void AutoRLogic() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const int useRMode = ListIndex(RMenu, "UseR", 0);
    if (useRMode == 2) {
        return;
    }
    if (!(useRMode == 0 || (useRMode == 1 && Orbwalker::ActiveMode() == OrbwalkingMode::Combo))) {
        return;
    }

    // R cứu ally máu thấp.
    if (Bool(RMenu, "RHealAlly")) {
        for (const auto& allyobj : GameObjects::AllyHeroes()) {
            if (!ValidHeroTarget(allyobj, HealthR.Range) || IsBlockR(allyobj)) {
                continue;
            }
            if (allyobj.HealthPercent() <= static_cast<float>(Slider(RMenu, "RHealPerfe", 15)) &&
                allyobj.CountEnemyHeroesInRange(400.0f) >= Slider(RMenu, "RhealEnemyCount", 1)) {
                const float spellToHero =
                    (allyobj.DistanceToPlayer() / DamageR.Speed) * 1000.0f + DamageR.Delay * 1000.0f;
                const bool pred = !Bool(RMenu, "UsePredHealth") ||
                    (HealthPrediction(AIBaseClient(allyobj.Handle()), static_cast<int>(spellToHero)) > 0.0f &&
                     HealthPrediction(AIBaseClient(allyobj.Handle()), static_cast<int>(spellToHero) + 500) <= 0.0f);
                if (pred) {
                    HealthR.Cast(allyobj.PreviousPosition());
                }
            }
        }
    }

    // R killable enemy.
    if (Bool(RMenu, "CRKillable")) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemy, DamageR.Range)) {
                continue;
            }
            const float spellToHero =
                (enemy.DistanceToPlayer() / DamageR.Speed) * 1000.0f + DamageR.Delay * 1000.0f;
            const float healthPreds = HealthPrediction(AIBaseClient(enemy.Handle()), static_cast<int>(spellToHero));
            if (CanCastSpell(DamageR, enemy) && healthPreds < GetRDmg(enemy) &&
                player.CountEnemyHeroesInRange(800.0f) == 0) {
                if (player.CountEnemyHeroesInRange(600.0f) <= Slider(RMenu, "RCountEnemyForMe", 1) &&
                    !player.Spellbook().IsWindingUp()) {
                    const auto pred = DamageR.GetPrediction(enemy);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh)) {
                        DamageR.Cast(pred.GetCastPosition());
                    }
                }
            }
        }
    }
}

static void CastW() {
    const auto target = GetTarget(W.Range, DamageType::Physical);
    if (!ValidHeroTarget(target, W.Range)) {
        return;
    }
    const auto pred = W.GetPrediction(target);
    if (HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh) && pred.CollisionObjects.empty()) {
        W.Cast(pred.GetCastPosition());
    }
}

static bool CastNormalQ(const AIBaseClient& unit) {
    AIBaseClient qtarget = unit;
    if (!qtarget.IsValid()) {
        const auto t = GetTarget(Q.Range, DamageType::Physical);
        if (t.IsValid()) {
            qtarget = AIBaseClient(t.Handle());
        }
    }
    if (!ValidTarget(qtarget, Q.Range)) {
        return false;
    }
    return Q.CastOnUnit(qtarget);
}

// Extended Q: bắn xuyên qua 1 unit (trong tầm Q) tới enemy (damage) / ally (heal).
static bool CastExtraQ(bool damage, const AIBaseClient& healAlly) {
    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }
    if (damage) {
        const auto t1 = GetTarget(ExtraDmgQ.Range, DamageType::Physical);
        if (!ValidHeroTarget(t1, ExtraDmgQ.Range)) {
            return false;
        }
        const auto pred = ExtraDmgQ.GetPrediction(t1);
        if (!HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
            return false;
        }
        const Vector3 castPos = pred.GetCastPosition();
        for (const auto& unit : GetHittableTargets()) {
            // unit nằm gần đường player→castPos và trong tầm Q.
            const float toLine = castPos.Distance(unit.PreviousPosition());
            if (toLine <= ExtraDmgQ.Width + unit.BoundingRadius() &&
                player.Distance(unit) <= Q.Range) {
                return ExtraDmgQ.CastOnUnit(unit);
            }
        }
    } else {
        if (healAlly.IsValid() && ValidTarget(healAlly, ExtraHealQ.Range) &&
            player.Distance(healAlly) > Q.Range) {
            for (const auto& unit : GetHittableTargets()) {
                const float toLine = healAlly.PreviousPosition().Distance(
                    player.PreviousPosition().Extend(unit.PreviousPosition(), ExtraHealQ.Range));
                if (toLine <= ExtraHealQ.Width + healAlly.BoundingRadius() &&
                    player.Distance(unit) <= Q.Range) {
                    return ExtraHealQ.CastOnUnit(unit);
                }
            }
        }
    }
    return false;
}

// Nhặt soul trong tầm AA bằng lệnh attack (thay Player.IssueOrder(AttackUnit)).
static void AutoPick() {
    for (const auto& soul : Souls) {
        if (!IsValidSoul(soul)) {
            continue;
        }
        if (AutoAttack::InAutoAttackRange(soul.Pointer) && Orbwalker::CanAttack()) {
            CoreControl::IssueAttack(soul.Pointer.Address(), soul.Pointer.Position());
        }
    }
}

static void JungleClear() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() <= static_cast<float>(Slider(LaneClearMenu, "LMana", 40))) {
        return;
    }
    if (W.IsReady() && Bool(JungleClearMenu, "JW") && !player.Spellbook().IsWindingUp()) {
        for (const auto& mob : GameObjects::Jungle()) {
            if (ValidTarget(mob, W.Range)) {
                W.Cast(mob.PreviousPosition());
                break;
            }
        }
    }
    if (Q.IsReady() && Bool(JungleClearMenu, "JQ") && !player.Spellbook().IsWindingUp()) {
        AIMinionClient best;
        float bestHp = FLT_MAX;
        for (const auto& mob : GameObjects::Jungle()) {
            if (ValidTarget(mob, Q.Range) && mob.Health() < bestHp) {
                bestHp = mob.Health();
                best = mob;
            }
        }
        if (best.IsValid()) {
            Q.CastOnUnit(AIBaseClient(best.Handle()));
        }
    }
}

static void LaneClear() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() <= static_cast<float>(Slider(LaneClearMenu, "LMana", 40))) {
        return;
    }

    if (Bool(LaneClearMenu, "LQ") && Q.IsReady() && !player.Spellbook().IsWindingUp()) {
        const int minCount = Slider(LaneClearMenu, "LQCount", 2);
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!ValidTarget(minion, ExtraDmgQ.Range)) {
                continue;
            }
            const auto pred = ExtraDmgQ.GetPrediction(minion);
            const auto& col = pred.CollisionObjects;
            if (static_cast<int>(col.size()) >= minCount) {
                for (const auto& c : col) {
                    if (c.DistanceToPlayer() <= Q.Range && ValidTarget(c, Q.Range)) {
                        Q.CastOnUnit(c);
                        return;
                    }
                }
            }
        }
    }
}

static void Harass() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() <= static_cast<float>(Slider(HarassMenu, "HMana", 40))) {
        return;
    }
    if (Bool(HarassMenu, "HQ") && Q.IsReady()) {
        if (!CastNormalQ() && Bool(HarassMenu, "HQExtra")) {
            CastExtraQ(true);
        }
    }
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Orb Mode Passive: ưu tiên đánh hero dính "sennapassivemarker".
    if (ListIndex(ComboMenu, "Target", 0) == 0) {
        AIHeroClient orbTarget;
        int bestPriority = -1;
        for (const auto& x : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(x, player.AttackRange()) || !x.HasBuff("sennapassivemarker")) {
                continue;
            }
            auto* prio_ = SDK::Modes::Priority::Instance();
        const int prio = prio_ ? prio_->GetHeroPriority(x) : 0;
            if (prio > bestPriority) {
                bestPriority = prio;
                orbTarget = x;
            }
        }
        if (orbTarget.IsValid()) {
            Orbwalker::ForceTarget(orbTarget);
        }
    }

    if (Bool(ComboMenu, "CW") && W.IsReady() && !player.Spellbook().IsWindingUp()) {
        CastW();
    }
    if (Bool(ComboMenu, "CQ") && Q.IsReady() && !player.Spellbook().IsWindingUp()) {
        if (!CastNormalQ() && Bool(ComboMenu, "CQExtra")) {
            CastExtraQ(true);
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Drawing::OnDraw -= &OnDraw;
    GameObjects::RemoveOnCreate(&OnObjectCreate);
    GameObjects::RemoveOnDelete(&OnObjectDelete);

    Souls.clear();
    Loaded = false;
}

} // namespace Plugins::SharpAIO::Senna
