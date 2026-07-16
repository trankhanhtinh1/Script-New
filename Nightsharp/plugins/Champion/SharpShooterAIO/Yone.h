#pragma once

// ============================================================================
// SharpShooter AIO — Yone
// Port từ CSharpFiles_2/Yone.cs (ToxicAio) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Vayne.h.
//
// Kỹ năng:
//   Q Mortal Steel  — line 475 (empowered Q3 = 950 khi có buff "yoneq3ready").
//                     delay 0.33, width 15, speed 5000 (Q3: 0.25/160/1500).
//   W Spirit Cleave — cone 600, delay 0.46.
//   E Soul Unbound  — dash 300 (manaless: cast khi Mana==0 ~ luôn cast).
//   R Fate Sealed   — line 1000, delay 0.75, width 255, speed 1500.
//
// Ghi chú port (giữ 1-1 với C#):
//   * Q.Range switch 450 <-> 950 theo buff "yoneq3ready" (YoneQ33).
//   * OnGameUpdate: theo OrbwalkerMode (Combo/LaneClear/LastHit/Harass) + YoneQ33
//     + Killsteal + skind. Cùng thứ tự C#.
//   * ListDmg/DmgOnTarget/AIBaseClient_OnBuffAdd/GetEDmg/isE2: port nguyên khối
//     (C# gốc dùng buff-name "" nên khối tracking này no-op — giữ 1-1).
//   * Damage QDamage/WDamage/RDamage recheck theo wiki (leagueoflegends.com,
//     chốt 2026-07-09) thay số patch cũ trong C#.
//   * MISSING API: SetSkin/SkinId (skind) — SDK chưa expose. Xem missapi.md.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Yone {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* ClearMenu = nullptr;
inline Menu* KillMenu = nullptr;
inline Menu* MiscMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 475.0f };
inline Spell Q3{ SpellSlot::Q, 950.0f };
inline Spell W{ SpellSlot::W, 600.0f };
inline Spell E{ SpellSlot::E, 300.0f };
inline Spell R{ SpellSlot::R, 1000.0f };

inline bool Loaded = false;
inline bool YoneQ3 = false;

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

static bool ValidUnit(const AttackableUnit& unit) {
    return unit.IsValid() && !unit.IsDead() && unit.Health() > 0.0f;
}

static bool ValidTarget(const AIBaseClient& unit, float range = FLT_MAX) {
    return ValidUnit(unit) && Extensions::IsValidTarget(unit, range, true);
}

static bool ValidHeroTarget(const AIHeroClient& hero, float range = FLT_MAX) {
    return ValidUnit(hero) && Extensions::IsValidTarget(hero, range, true);
}

// C#: OktwCommon.GetIncomingDamage — SDK chưa expose incoming-damage tracker.
// Trả 0 (bỏ term, giữ so-máu killsteal). Xem missapi.md.
static float GetIncomingDamage(const AIBaseClient&) {
    return 0.0f;
}

// ── ListDmg tracking (C# gốc: DmgOnTarget) ──
// C# tạo entry mỗi enemy hero, dùng buff-name "" trong OnBuffAdd/GetEDmg nên
// khối này thực chất không tích luỹ gì. Port 1-1 để giữ cấu trúc.
struct DmgOnTarget {
    int UID = 0;
    double dmg = 0.0;
};
inline std::vector<DmgOnTarget> ListDmg;

// Forward declarations — đúng thứ tự file C#.
static void OnGameUpdate(const GameUpdateEventArgs& args);
static void AIBaseClient_OnBuffAdd(const Events::BuffEventArgs& args);
static float GetEDmg(const AIBaseClient& target);
static bool isE2();
static void YoneQ33();
static void skind();
static float ComboDamage(const AIBaseClient& enemy);
static void OnDraw();
static void LogicQ();
static void LogicW();
static void LogicE();
static void LogicR();
static void Jungle();
static void Lanceclear();
static void LastHit();
static void Killsteal();
static float QDamage(const AIBaseClient& Qtarget);
static float WDamage(const AIBaseClient& Wtarget);
static float RDamage(const AIBaseClient& Rtarget);
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Yone", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Qsettings", "Q settings"));
    ComboMenu->Add(new MenuBool("UseQ", "Use Q in Combo"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Wsettings", "W settings"));
    HarassMenu->Add(new MenuBool("UseW", "use W in Combo"));

    auto* eMenu = MenuRoot->AddSubMenu(new Menu("Esettings", "E settings"));
    eMenu->Add(new MenuBool("UseE", "use E in Combo"));

    auto* rMenu = MenuRoot->AddSubMenu(new Menu("Rsettings", "R settings"));
    rMenu->Add(new MenuBool("UseR", "use R in Combo"));

    ClearMenu = MenuRoot->AddSubMenu(new Menu("Clear", "Clear settings"));
    ClearMenu->Add(new MenuBool("LhQ", "use Q to Last Hit"));
    ClearMenu->Add(new MenuBool("LcQ", "use Q to Laneclear"));
    ClearMenu->Add(new MenuBool("LcW", "use W to Laneclear"));
    ClearMenu->Add(new MenuBool("JcQ", "use Q to Jungleclear"));
    ClearMenu->Add(new MenuBool("JcW", "use W to Jungleclear"));

    KillMenu = MenuRoot->AddSubMenu(new Menu("Killsteal", "Killsteal settings"));
    KillMenu->Add(new MenuBool("KsQ", "use Q to Killsteal"));
    KillMenu->Add(new MenuBool("KsW", "use W to Killsteal"));
    KillMenu->Add(new MenuBool("KsR", "use R to Killsteal"));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc", "Misc settings"));
    MiscMenu->Add(new MenuSliderButton("Skin", "SkindID", 0, 0, 30, false));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Draw settings"));
    DrawMenu->Add(new MenuBool("drawQ", "Q Range  (White)", true));
    DrawMenu->Add(new MenuBool("drawW", "W Range  (White)", true));
    DrawMenu->Add(new MenuBool("drawE", "E Range (White)", true));
    DrawMenu->Add(new MenuBool("drawR", "R Range  (Red)", true));
    DrawMenu->Add(new MenuBool("drawD", "Draw Combo Damage", true));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 475.0f);
    Q.SetSkillshot(0.33f, 15.0f, 5000.0f, false, SpellType::Line);
    Q.DamageType = DamageType::Physical;

    Q3 = Spell(SpellSlot::Q, 950.0f);
    Q3.SetSkillshot(0.25f, 160.0f, 1500.0f, false, SpellType::Line);
    Q3.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, 600.0f);
    W.SetSkillshot(0.46f, 0.0f, 500.0f, false, SpellType::Cone);
    W.DamageType = DamageType::Magical;

    E = Spell(SpellSlot::E, 300.0f);
    E.DamageType = DamageType::Physical;

    R = Spell(SpellSlot::R, 1000.0f);
    R.SetSkillshot(0.75f, 255.0f, 1500.0f, false, SpellType::Line);
    R.DamageType = DamageType::Physical;

    // C#: foreach enemy hero → ListDmg.Add(new DmgOnTarget(NetworkId, 0)).
    for (const auto& item : GameObjects::EnemyHeroes()) {
        DmgOnTarget entry;
        entry.UID = item.NetworkId();
        entry.dmg = 0.0;
        ListDmg.push_back(entry);
    }

    BuildMenu();

    Events::hook.OnGameUpdate += &OnGameUpdate;
    Drawing::OnDraw += &OnDraw;
    Events::hook.OnBuffAdd += &AIBaseClient_OnBuffAdd;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Yone loaded</font>");
}

static void OnGameUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }

    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo) {
        LogicE();
        LogicR();
        LogicW();
        LogicQ();
    }

    if (Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear) {
        Jungle();
        Lanceclear();
    }

    if (Orbwalker::ActiveMode() == OrbwalkingMode::LastHit) {
        LastHit();
    }

    if (Orbwalker::ActiveMode() == OrbwalkingMode::Harass) {
    }

    YoneQ33();
    Killsteal();
    skind();
}

// C#: AIBaseClient.OnBuffAdd — reset dmg khi buff (name "") add.
static void AIBaseClient_OnBuffAdd(const Events::BuffEventArgs& args) {
    if (!Loaded) {
        return;
    }
    if (std::string(args.BuffName) != "") {
        return;
    }

    const auto sender = AIBaseClient(args.Sender.Ptr);
    if (!sender.IsValid()) {
        return;
    }
    for (auto& e : ListDmg) {
        if (e.UID == sender.NetworkId()) {
            e.dmg = 0.0;
            break;
        }
    }
}

// C#: GetEDmg — dùng buff "" nên luôn trả 0 (giữ 1-1).
static float GetEDmg(const AIBaseClient& target) {
    if (!target.HasBuff("")) {
        return 0.0f;
    }
    const DmgOnTarget* found = nullptr;
    for (const auto& e : ListDmg) {
        if (e.UID == target.NetworkId()) {
            found = &e;
            break;
        }
    }
    if (!found) {
        return 0.0f;
    }
    double dmg = 0.0;
    static const double list[5] = { 0.25, 0.275, 0.3, 0.325, 0.35 };
    const int elvl = E.Level();
    if (elvl >= 1 && elvl <= 5) {
        dmg += list[elvl - 1] * found->dmg;
    }
    return static_cast<float>(dmg);
}

// C#: Yone manaless — Mana>0 hầu như không xảy ra → !isE2() thường true.
static bool isE2() {
    return Player().Mana() > 0.0f;
}

static void YoneQ33() {
    const auto player = Player();
    if (!YoneQ3 && player.HasBuff("yoneq3ready")) {
        Q.Range = 950.0f;
        YoneQ3 = true;
    } else if (YoneQ3 && !player.HasBuff("yoneq3ready")) {
        Q.Range = 450.0f;
        YoneQ3 = false;
    }
}

// C#: skind — SetSkin/SkinId. MISSING API (xem missapi.md).
static void skind() {
    // MISSING API: Me.SkinId / Me.SetSkin(int) — SDK chưa expose skin changer.
    // Giữ menu item "Skin" để tương thích, bỏ hành vi đổi skin.
}

static float ComboDamage(const AIBaseClient& enemy) {
    const auto player = Player();
    if (!player.IsValid() || !enemy.IsValid()) {
        return 0.0f;
    }
    float damage = 0.0f;
    // C#: igniteSlot term — SDK chưa expose GetSummonerSpellDamage → bỏ (xem missapi.md).
    if (Q.IsReady()) {
        damage += player.GetSpellDamage(enemy, SpellSlot::Q);
    }
    if (W.IsReady()) {
        damage += player.GetSpellDamage(enemy, SpellSlot::W);
    }
    if (E.IsReady()) {
        damage += player.GetSpellDamage(enemy, SpellSlot::E);
    }
    if (R.IsReady()) {
        damage += player.GetSpellDamage(enemy, SpellSlot::R);
    }
    return damage;
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    // C#: các vòng DrawCircle bị comment; chỉ giữ draw combo-damage text.
    if (Bool(DrawMenu, "drawD", true)) {
        for (const auto& enemyVisible : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemyVisible)) {
                continue;
            }
            const auto eBase = AIBaseClient(enemyVisible.Handle());
            const float combo = ComboDamage(eBase);
            Vector2 screen;
            if (!Drawing::WorldToScreen(enemyVisible.Position(), screen)) {
                continue;
            }
            const float tx = screen.x + 50.0f;
            const float ty = screen.y - 40.0f;
            if (combo > enemyVisible.Health()) {
                Drawing::DrawText(tx, ty, 0xFFFF0000u, "Combo=Kill");
            } else if (combo + Damage::GetAutoAttackDamage(player, eBase) * 2.0f > enemyVisible.Health()) {
                Drawing::DrawText(tx, ty, 0xFFFFA500u, "Combo + 2 AA = Kill");
            } else {
                Drawing::DrawText(tx, ty, 0xFF00FF00u, "Unkillable with combo + 2 AA");
            }
        }
    }
}

static void LogicQ() {
    auto* ts = SDK::TargetSelector::Instance();
    const auto target = ts ? ts->GetTarget(Q.Range, DamageType::Physical) : AIHeroClient();
    if (!target.IsValid()) {
        return;
    }
    const auto tBase = AIBaseClient(target.Handle());
    if (Q.IsReady() && Bool(ComboMenu, "UseQ") && ValidHeroTarget(target, Q.Range) && Q.IsInRange(tBase)) {
        Q.Cast(tBase);
    }
}

static void LogicW() {
    auto* ts = SDK::TargetSelector::Instance();
    const auto target = ts ? ts->GetTarget(W.Range, DamageType::Physical) : AIHeroClient();
    if (!target.IsValid()) {
        return;
    }
    const auto tBase = AIBaseClient(target.Handle());
    if (W.IsReady() && W.IsInRange(tBase) && Bool(HarassMenu, "UseW") && ValidHeroTarget(target, W.Range)) {
        W.Cast(tBase);
    }
}

static void LogicE() {
    auto* ts = SDK::TargetSelector::Instance();
    const auto target = ts ? ts->GetTarget(E.Range, DamageType::Physical) : AIHeroClient();
    auto* eMenu = MenuRoot ? MenuRoot->GetSubMenu("Esettings") : nullptr;
    if (!target.IsValid()) {
        return;
    }
    if (E.IsReady() && Bool(eMenu, "UseE") && !isE2()) {
        E.Cast(target.Position());
    }
}

static void LogicR() {
    auto* ts = SDK::TargetSelector::Instance();
    const auto target = ts ? ts->GetTarget(R.Range, DamageType::Physical) : AIHeroClient();
    auto* rMenu = MenuRoot ? MenuRoot->GetSubMenu("Rsettings") : nullptr;
    if (!target.IsValid()) {
        return;
    }
    const auto tBase = AIBaseClient(target.Handle());
    if (R.IsReady() && Bool(rMenu, "UseR") &&
        R.GetDamage(tBase) + Q.GetDamage(tBase) * 2.0f + W.GetDamage(tBase) >= target.Health() &&
        ValidHeroTarget(target)) {
        R.Cast(tBase);
    }
}

static void Jungle() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    auto mobs = GameObjects::Jungle();
    mobs.erase(
        std::remove_if(mobs.begin(), mobs.end(),
            [](const AIMinionClient& mob) { return !ValidTarget(mob, Q.Range); }),
        mobs.end());
    std::sort(mobs.begin(), mobs.end(),
        [](const AIMinionClient& a, const AIMinionClient& b) { return a.MaxHealth() < b.MaxHealth(); });
    if (!mobs.empty()) {
        const auto& mob = mobs[0];
        if (Bool(ClearMenu, "JcQ") && Q.IsReady() && player.Distance(mob.Position()) < Q.Range) {
            Q.Cast(mob.Position());
        }
        if (Bool(ClearMenu, "JcW") && W.IsReady() && player.Distance(mob.Position()) < W.Range) {
            W.Cast(mob.Position());
        }
    }
}

static void Lanceclear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Bool(ClearMenu, "LcW") && W.IsReady()) {
        std::vector<AIBaseClient> minions;
        for (const auto& x : GameObjects::EnemyMinions()) {
            if (ValidTarget(x, W.Range) && x.IsMinion()) {
                minions.push_back(AIBaseClient(x.Handle()));
            }
        }
        if (!minions.empty()) {
            const auto wFarm = W.GetLineFarmLocation(minions);
            if (wFarm.Position.IsValid()) {
                W.Cast(Vector3::From2D(wFarm.Position));
                return;
            }
        }
    }

    if (Bool(ClearMenu, "LcQ") && Q.IsReady()) {
        std::vector<AIBaseClient> minions;
        for (const auto& x : GameObjects::EnemyMinions()) {
            if (ValidTarget(x, Q.Range) && x.IsMinion()) {
                minions.push_back(AIBaseClient(x.Handle()));
            }
        }
        if (!minions.empty()) {
            const auto qFarm = Q.GetLineFarmLocation(minions);
            if (qFarm.Position.IsValid()) {
                Q.Cast(Vector3::From2D(qFarm.Position));
                return;
            }
        }
    }
}

static void LastHit() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (!Bool(ClearMenu, "LhQ")) {
        return;
    }

    auto minions = GameObjects::EnemyMinions();
    minions.erase(
        std::remove_if(minions.begin(), minions.end(),
            [](const AIMinionClient& x) { return !(x.IsMinion() && !x.IsDead()); }),
        minions.end());
    std::sort(minions.begin(), minions.end(),
        [&player](const AIMinionClient& a, const AIMinionClient& b) {
            return a.Distance(player.Position()) < b.Distance(player.Position());
        });

    for (const auto& min : minions) {
        const auto mBase = AIBaseClient(min.Handle());
        if (ValidTarget(mBase, Q.Range) && min.Health() < Q.GetDamage(mBase)) {
            Orbwalker::ForceTarget(min);
            Q.Cast(mBase);
        }
    }
}

static void Killsteal() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const bool ksQ = Bool(KillMenu, "KsQ");
    const bool ksW = Bool(KillMenu, "KsW");
    const bool ksR = Bool(KillMenu, "KsR");

    auto* ts = SDK::TargetSelector::Instance();
    const auto Qtarget = ts ? ts->GetTarget(Q.Range, DamageType::Physical) : AIHeroClient();
    const auto Wtarget = ts ? ts->GetTarget(W.Range, DamageType::Physical) : AIHeroClient();
    const auto Rtarget = ts ? ts->GetTarget(R.Range, DamageType::Physical) : AIHeroClient();

    if (!Wtarget.IsValid()) return;
    if (Wtarget.IsInvulnerable()) return;
    if (!Rtarget.IsValid()) return;
    if (Rtarget.IsInvulnerable()) return;

    const auto qBase = AIBaseClient(Qtarget.Handle());
    const auto wBase = AIBaseClient(Wtarget.Handle());
    const auto rBase = AIBaseClient(Rtarget.Handle());

    if (!(Qtarget.IsValid() && player.Distance(Qtarget.Position()) <= Q.Range) ||
        !(QDamage(qBase) >= Qtarget.Health() + GetIncomingDamage(qBase))) return;
    if (Q.IsReady() && ksQ) Q.Cast(qBase);

    if (!(player.Distance(Wtarget.Position()) <= W.Range) ||
        !(WDamage(wBase) >= Wtarget.Health() + GetIncomingDamage(wBase))) return;
    if (W.IsReady() && ksW) W.Cast(wBase);

    if (!(player.Distance(Rtarget.Position()) <= R.Range) ||
        !(RDamage(rBase) >= Rtarget.Health() + GetIncomingDamage(rBase))) return;
    if (R.IsReady() && ksR) R.Cast(rBase);
}

// ── Damage tính tay theo wiki (leagueoflegends.com/Yone, chốt 2026-07-09) ──
// Q Mortal Steel (PHYSICAL): 25/50/75/100/125 + 110% total AD
// W Spirit Cleave (MIXED)  : 10/20/30/40/50 + 8/9/10/11/12% max health (½ phys ½ magic)
// R Fate Sealed (MIXED)    : 200/400/600 + 80% bonus AD (½ phys ½ magic)
static float QDamage(const AIBaseClient& Qtarget) {
    const auto player = Player();
    if (!player.IsValid() || !Qtarget.IsValid()) {
        return 0.0f;
    }
    const int qlevel = Q.Level();
    if (qlevel < 1) {
        return 0.0f;
    }
    static const float base[5] = { 25.0f, 50.0f, 75.0f, 100.0f, 125.0f };
    const int idx = (qlevel - 1 < 5) ? qlevel - 1 : 4;
    const float raw = base[idx] + 1.10f * player.AD();
    return player.CalculatePhysicalDamage(Qtarget, raw);
}

static float WDamage(const AIBaseClient& Wtarget) {
    const auto player = Player();
    if (!player.IsValid() || !Wtarget.IsValid()) {
        return 0.0f;
    }
    const int wlevel = W.Level();
    if (wlevel < 1) {
        return 0.0f;
    }
    static const float base[5] = { 10.0f, 20.0f, 30.0f, 40.0f, 50.0f };
    static const float hpPct[5] = { 0.08f, 0.09f, 0.10f, 0.11f, 0.12f };
    const int idx = (wlevel - 1 < 5) ? wlevel - 1 : 4;
    const float total = base[idx] + Wtarget.MaxHealth() * hpPct[idx];
    return player.CalculatePhysicalDamage(Wtarget, total * 0.5f) +
           player.CalculateMagicDamage(Wtarget, total * 0.5f);
}

static float RDamage(const AIBaseClient& Rtarget) {
    const auto player = Player();
    if (!player.IsValid() || !Rtarget.IsValid()) {
        return 0.0f;
    }
    const int rlevel = R.Level();
    if (rlevel < 1) {
        return 0.0f;
    }
    static const float base[3] = { 200.0f, 400.0f, 600.0f };
    const int idx = (rlevel - 1 < 3) ? rlevel - 1 : 2;
    const float total = base[idx] + 0.80f * player.BonusAttackDamage();
    return player.CalculatePhysicalDamage(Rtarget, total * 0.5f) +
           player.CalculateMagicDamage(Rtarget, total * 0.5f);
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &OnGameUpdate;
    Drawing::OnDraw -= &OnDraw;
    Events::hook.OnBuffAdd -= &AIBaseClient_OnBuffAdd;

    ListDmg.clear();
    Loaded = false;
}

} // namespace Plugins::SharpAIO::Yone
